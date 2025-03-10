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
    
    explicit Executor(size_t thread_count = std::thread::hardware_concurrency()) 
    : stopped(false) {
        createThreadPool(thread_count);
    }

    ~Executor() {
        if (!stopped) stop();
    }

    void schedule(Task task) {
        if (stopped) return;
        _tasksQ.push(std::move(task));
        _cv.notify_one();
    }

    void start() {}

    void stop() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            stopped = true;
        }
        _cv.notify_all();

        for (auto& thread : _threads) {
            if (thread.joinable()) thread.join();
        }
        _threads.clear();
    }

 private:
    void createThreadPool(size_t thread_count) {
        for (size_t i = 0; i < thread_count; ++i) {
            _threads.emplace_back([this] () { run(); });
        }
    }
    void run () {
        while (true) {
            Task task;
            bool hasTask = false;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _cv.wait(lock, [this, &task, &hasTask] {
                    hasTask = _tasksQ.try_pop(task);
                    return stopped || hasTask;
                });

                if (stopped) return;
            }
            if (hasTask) {
                try {
                    task(); //run the task
                } catch (const std::exception& e) {
                    std::cerr << "Task exception: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "Unknown task exception occurred" << std::endl;
                }
            }
        }
    }

    std::vector<std::thread> _threads;
    MPMCQueue<Task> _tasksQ;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> stopped;
};
