#pragma once
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>


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
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _tasksQ.push(std::move(task));
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
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _cv.wait(lock, [this] {
                    return stopped || !empty(_tasksQ);
                });

                if (stopped && empty(_tasksQ)) return;
                task  = std::move(_tasksQ.front());
                _tasksQ.pop();
            }
            task();
        }
    }

    std::vector<std::thread> _threads;
    std::queue<Task> _tasksQ;
    std::mutex _mutex;
    std::condition_variable _cv;
    bool stopped = false;
};
