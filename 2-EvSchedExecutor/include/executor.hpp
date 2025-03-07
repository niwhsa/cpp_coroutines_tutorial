#pragma once
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>


class Executor {
 public:
    using Task = std::function<void()>;

    void schedule(Task task) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _tasksQ.push(std::move(task));
        }
        _cv.notify_one();
    }

    void start() {
        _thread = std::thread([this] { run(); });
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            stopped = true;
        }
        _cv.notify_one();
        if (_thread.joinable()) _thread.join();
    }

 private:
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

    
    std::queue<Task> _tasksQ;
    std::thread _thread;
    std::mutex _mutex;
    std::condition_variable _cv;
    bool stopped = false;



};
