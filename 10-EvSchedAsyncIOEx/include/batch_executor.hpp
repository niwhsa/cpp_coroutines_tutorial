#pragma once
#include "executor.hpp"

class BatchExecutor : public Executor
{
public:
    using Executor::Executor;

private:
    static constexpr size_t DEFAULT_BATCH_SIZE = 64;
    thread_local static inline MPMCQueue<Task> *_localQ = nullptr;

    struct alignas(64) TaskBatch { // Cache line alignment
        std::vector<Task> taskbatchQ; //Use LIFO strategy
         size_t maxSize;
     
        explicit TaskBatch(size_t size = DEFAULT_BATCH_SIZE) : maxSize(size) {
            taskbatchQ.reserve(maxSize);
        }

        void clear() { taskbatchQ.clear(); }
        bool empty() const { return  taskbatchQ.empty(); }
        bool full() const { return   taskbatchQ.size() >= maxSize; }
        size_t size() const { return taskbatchQ.size(); }

        void add(Task &&task) {
            taskbatchQ.emplace_back(std::move(task));
        }

        Task take() {
            Task task = std::move(taskbatchQ.back());
            taskbatchQ.pop_back();
            return task;
        }
    };

protected:
    virtual void run() override
    {
        // Init thread-local queue
        _localQ = new MPMCQueue<Task>(_taskPoolSize / _config.threadCount);
        Executor::run();
        delete _localQ;
    }

    virtual bool getNextTask(Task &task) override
    {
        // First try localQ
        if (_localQ && _localQ->try_pop(task))
            return true;

        // Try to get a batch of tasks
        TaskBatch batch(_config.batchExecutorTaskBatchSize);

        for (size_t p = 0; p < static_cast<size_t>(Priority::kNumPriorities); ++p) {
            while (!batch.full() && !_taskQArray[p]->empty()) {
                if (_taskQArray[p]->try_pop(task)) {
                    batch.add(std::move(task));
                    _pendingTasks--;
                }
            }
            if (!batch.empty()) break; // limit to a single prio level batch tasks
        }

        // If we got batch tasks, take one and put the rest in local Q
        if (!batch.empty())
        {
            task = batch.take();
            while (!batch.empty())
            {
                _localQ->push(batch.take());
            }
            return true;
        }

        // Last resort - try stealing
        return tryStealTask(task);
    }
};
