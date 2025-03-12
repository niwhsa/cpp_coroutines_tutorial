#pragma once
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <iostream>
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
        size_t initialTaskPoolSize = 256; 
        size_t batchExecutorTaskBatchSize = 512;

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
      _taskPoolSize(config.initialTaskPoolSize),
      _config(config) {}

    ~Executor() {
        if (!stopped) stop();
    }

    void schedule(Func task, Priority priority = Priority::Normal) {
        if (stopped) return;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            
            //Resize task queue if necessary
            checkTaskQueueResize();

            //Try to add to localQ if called from worker thread
            if (_config.enableWorkStealing && currentThreadId < _localQVec.size()) {
                _localQVec[currentThreadId]->push(Task(std::move(task), priority));
            } else {
                _taskQArray[static_cast<size_t>(priority)]->push(Task(std::move(task), priority));
            }
            _pendingTasks++;

            //Scale up if needed
            if (shouldScaleUp()) { addThread(); }
        }
        _cv.notify_one();
    }

    void start() {
         createThreadPool(_minThreads); //Start with min threads
    }

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
        
        // Calculate optimal queue sizes based on config
        size_t queueSize = std::max(
            _config.threadCount * _config.tasksPerThreadThreshold,
            static_cast<size_t>(1024)  // Minimum size
        );

        // Initialize task queues with configured pool size
        for (auto& queue : _taskQArray) {
            queue = std::make_unique<MPMCQueue<Task>>(queueSize);
        }

        // Initialize local queues for work stealing
        if (_config.enableWorkStealing) {
            _localQVec.reserve(thread_count);
            for (size_t i = 0; i< thread_count; ++i) { 
                _localQVec.emplace_back(std::make_unique<MPMCQueue<Task>>(queueSize / thread_count));
            }
        }

        //Create worker threads
        for (size_t i = 0; i < thread_count; ++i) {
            _threadsVec.emplace_back([this, i] () { 
                currentThreadId = i;
                run();
            });
            _activeThreads++;
        }
    }
    
    virtual bool getNextTask(Task& task) {
        //First try local Q
        if (_config.enableWorkStealing && currentThreadId < _localQVec.size()) {
            if (_localQVec[currentThreadId]->try_pop(task)) {
                _pendingTasks--;
                return true;
            }
        }

        //Get task from global q
        for (size_t p = 0; p < static_cast<size_t>(Priority::kNumPriorities); ++p) {
            if(_taskQArray[p]->try_pop(task)) {
                _pendingTasks--;
                return true;
            }
        }
        return tryStealTask(task);
    }

    bool waitForTask(Task& task) {
        bool hasTask = false;
        std::unique_lock<std::mutex> lock(_mutex);
        
        //Wait for timeout or stopped || hasTask to be true
        if(_cv.wait_for(lock, _keepAliveTime, [this, &task, &hasTask] {
                    hasTask = getNextTask(task);
                    return stopped || hasTask; 
            })) {
                //either stopped == true or hasTask == true or both
                if (stopped) {
                    decrementActiveThreads();
                    return false; //don't wait for task
                }
               if (hasTask) return true; //hasTask is true
        }

        //Handle timeout -scale down if idle
        if (stopped || _activeThreads > _minThreads) { decrementActiveThreads(); }
        return false;
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
        if (_activeThreads >= _maxThreads) {
            return;
        }

        size_t threadId = _localQVec.size();
        try {
            _localQVec.emplace_back(std::make_unique<MPMCQueue<Task>>());
            _threadsVec.emplace_back([this, threadId] {
                currentThreadId = threadId;
                run();
            });
            _activeThreads++;
        } catch (const std::exception& e) {
            std::cerr<<" Failed to create thread " << e.what() << std::endl;
        }
    }

    void checkTaskQueueResize() {
        if (_pendingTasks <= _taskPoolSize * 0.8)  return;  //80 % threshold
        
        size_t newSize = _taskPoolSize << 1;
        for (size_t p = 0; p < static_cast<size_t>(Priority::kNumPriorities); ++p) {
            _taskQArray[p]->resizePool(newSize);
        }

        if (_config.enableWorkStealing) {
            for (auto& localQ : _localQVec) {
                localQ->resizePool(newSize / _localQVec.size());
            }
        }

        _taskPoolSize = newSize;
    }

    std::vector<std::thread> _threadsVec;
    std::condition_variable _cv;
    std::atomic<bool> stopped;
    std::chrono::seconds _keepAliveTime;
    size_t _minThreads;
    size_t _maxThreads;
    size_t _tasksPerThreadThreshold;
    
    std::atomic<size_t> _activeThreads;
    
    //work stealing
    std::vector<std::unique_ptr<MPMCQueue<Task>>> _localQVec;
    thread_local static inline size_t currentThreadId = std::numeric_limits<size_t>::max();

  protected:
    std::atomic<size_t> _pendingTasks;
    std::array<std::unique_ptr<MPMCQueue<Task>>, static_cast<size_t>(Priority::kNumPriorities)> _taskQArray;
    std::mutex _mutex;
    size_t _taskPoolSize;
    Config _config;

    virtual void run () {
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
    
};
