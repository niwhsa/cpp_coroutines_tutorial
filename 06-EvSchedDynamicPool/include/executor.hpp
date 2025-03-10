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
    using Task = std::function<void()>;

    struct Config {
        size_t threadCount;
        size_t minThreads;
        size_t tasksPerThreadThreshold;
        std::chrono::seconds keepAliveTime;

        Config()
        : threadCount(std::thread::hardware_concurrency()),
          minThreads(std::thread::hardware_concurrency()/ 2),
          tasksPerThreadThreshold(3),
          keepAliveTime(std::chrono::seconds(60)) {}
    };
    
    explicit Executor(const Config& config = Config{}) 
    : stopped(false),
      _keepAliveTime(config.keepAliveTime),
      _minThreads(config.minThreads),
      _maxThreads(config.threadCount),
      _activeThreads(0),
      _pendingTasks(0) {
        createThreadPool(_minThreads); //Start with min threads
    }

    ~Executor() {
        if (!stopped) stop();
    }

    void schedule(Task task) {
        if (stopped) return;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _tasksQ.push(std::move(task));
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
        while (size(_threadsVec) < thread_count) {
            _threadsVec.emplace_back([this] () { run(); });
            _activeThreads++;
        }
    }

    void run () {
        while (true) {
            Task task;
            if (!waitForTask(task)) {
                return;
            }
            executeTask(task);
        }
    }

    bool waitForTask(Task& task) {
        bool hasTask = false;
        std::unique_lock<std::mutex> lock(_mutex);
        
        //Wait with timeout for task
        if(_cv.wait_for(lock, _keepAliveTime, [this, &task, &hasTask] {
                    hasTask = _tasksQ.try_pop(task);
                    if (hasTask) _pendingTasks--;
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
    MPMCQueue<Task> _tasksQ;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> stopped;
    std::chrono::seconds _keepAliveTime;
    size_t _minThreads;
    size_t _maxThreads;
    size_t _tasksPerThreadThreshold;
    std::atomic<size_t> _activeThreads;
    std::atomic<size_t> _pendingTasks;
};
