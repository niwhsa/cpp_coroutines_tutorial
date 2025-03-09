#pragma once
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include "mpmc_queue.hpp"

class Executor {
 public:
    using Func = std::function<void()>;

    //Add priority levels for Task
    enum class Priority : uint8_t {
        High = 0,
        Normal = 1,
        Low = 2,
        kNumPriorities = 3
    };

    struct Task {
        std::function<void()> func;
        Priority priority{Priority::Normal};

        Task(Func f, Priority p = Priority::Normal)
              : func(std::move(f)), priority(p) {}
        
              void operator() () { func(); }
    };
   
    struct Config {
        size_t threadCount;
        size_t minThreads;
        size_t tasksPerThreadThreshold;
        std::chrono::seconds keepAliveTime;
        bool enableWorkStealing{true};

        Config()
        : threadCount(std::thread::hardware_concurrency()),
          minThreads(std::thread::hardware_concurrency()/ 2),
          tasksPerThreadThreshold(3),
          keepAliveTime(std::chrono::seconds(60)),
          enableWorkStealing(true) {}
    };
    
    explicit Executor(const Config& config = Config{}) 
    : stopped(false),
      _keepAliveTime(config.keepAliveTime),
      _minThreads(config.minThreads),
      _maxThreads(config.threadCount),
      _activeThreads(0),
      _pendingTasks(0),
      _config(config) {
        createThreadPool(_minThreads); //Start with min threads
    }

    ~Executor() {
        if (!stopped) stop();
    }

    void schedule(Func task, Priority priority = Priority::Normal) {
        if (stopped) return;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            //Try to add to localQ if called from worker thread
            if (_config.enableWorkStealing && currentThreadId < _localQVec.size()) {
                _localQVec[currentThreadId]->push(Task(std::move(task), priority));
            } else {
                _taskQArray[static_cast<size_t>(priority)].push(Task(std::move(task), priority));
            }
            _pendingTasks++;

            //Scale up if needed
            if (shouldScaleUp()) { addThread(); }
        }
        _cv.notify_one();
    }

    void start() {}

    void stop() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            stopped = true;
        }
        _cv.notify_all();

        for (auto& thread : _threadsVec) {
            if (thread.joinable()) thread.join();
        }
        _threadsVec.clear();
    }

 private:
    void createThreadPool(size_t thread_count) {
        std::lock_guard<std::mutex> local(_mutex);
        _localQVec.reserve(thread_count);
       
        for (size_t i = 0; i < thread_count; ++i) {// (size(_threadsVec) < thread_count) {
           
            _localQVec.emplace_back(std::make_unique<MPMCQueue<Task>>());

            _threadsVec.emplace_back([this, i] () { 
                currentThreadId = i;
                run();
             });
            _activeThreads++;
        }
    }

    void run () {
        while (true) {
            Task task([] {});
            if (!waitForTask(task)) {
                return;
            }
            executeTask(task);
        }
    }
    
    bool tryStealTask(Task& task) {
        if (!_config.enableWorkStealing) return false;
       
        // Try stealing from other threads' local queues
        size_t startIdx = (currentThreadId + 1) % _localQVec.size();
        for (size_t i = 0; i < _localQVec.size(); ++i) {
            size_t victimId = (startIdx + i) % _localQVec.size();
            if (_localQVec[victimId]->try_pop(task)) {
                _pendingTasks--;
                return true;
            }
        }
        return false;
    }
    
    bool getNextTask(Task& task) {
        //First try local Q
        if (_config.enableWorkStealing && currentThreadId < _localQVec.size()) {
            if (_localQVec[currentThreadId]->try_pop(task)) {
                _pendingTasks--;
                return true;
            }
        }

        //Get task from global q
        for (size_t p = 0; p < static_cast<size_t>(Priority::kNumPriorities); ++p) {
            if(_taskQArray[p].try_pop(task)) {
                _pendingTasks--;
                return true;
            }
        }
        return tryStealTask(task);
    }

    bool waitForTask(Task& task) {
        bool hasTask = false;
        std::unique_lock<std::mutex> lock(_mutex);
        
        //Wait with timeout for task
        if(_cv.wait_for(lock, _keepAliveTime, [this, &task, &hasTask] {
                    hasTask = getNextTask(task);
                    return stopped || hasTask;
            })) {
                if (stopped) {
                    decrementActiveThreads();
                    return false; //don't wait for task
                }
                return true;
        }

        //Handle timeout -scale down if idle
        if (_activeThreads > _minThreads) {
            decrementActiveThreads();
            return false;
        }
        return true;
    }

    void executeTask(Task& task) {
        try {
             task(); //run the task
        } catch (const std::exception& e) { handleTaskError(e); } 
        catch (...) { handleUnknownError(); }
    }

    void decrementActiveThreads() {  _activeThreads--; }
    
    void handleTaskError(const std::exception& e) {
        std::cerr << "Task exception: " << e.what() << std::endl;
    }

    void handleUnknownError() {
        std::cerr << "Unknown task exception occurred" << std::endl;
    }

    bool shouldScaleUp() const  {
        const size_t tasksPerThread = _pendingTasks / (_activeThreads + 1);
        return tasksPerThread > _tasksPerThreadThreshold && 
               _activeThreads < _maxThreads && _pendingTasks > 0;
    }

    void addThread() {
        if (_activeThreads >- _maxThreads) return;

        try {
            _threadsVec.emplace_back([this] { run();});
            _activeThreads++;
        } catch (const std::exception& e) {
            std::cerr<<" Failed to create thread " << e.what() << std::endl;
        }
    }

    std::vector<std::thread> _threadsVec;
    std::array<MPMCQueue<Task>, static_cast<size_t>(Priority::kNumPriorities)> _taskQArray;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> stopped;
    std::chrono::seconds _keepAliveTime;
    size_t _minThreads;
    size_t _maxThreads;
    size_t _tasksPerThreadThreshold;
    std::atomic<size_t> _activeThreads;
    std::atomic<size_t> _pendingTasks;
    Config _config;
    //work stealing
    std::vector<std::unique_ptr<MPMCQueue<Task>>> _localQVec;
    thread_local static inline size_t currentThreadId = std::numeric_limits<size_t>::max();
};
