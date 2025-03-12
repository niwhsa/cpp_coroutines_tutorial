#include "event_scheduler.hpp"
#include "event_registry.hpp"
#include "event_benchmarker.hpp"
#include "batch_executor.hpp"
#include "async_fs_executor.hpp"

void runFSExecutorBenchmark();

void runExecutorBenchmark(Executor& executor, const std::string& name) {
    std::cout << "\nTesting " << name << "..." << std::endl;
    
    executor.start();
    constexpr int NUM_TASKS = 1000000;
    std::atomic<int> completed{0};
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_TASKS; ++i) {
        executor.schedule([&completed] () {
            // CPU-bound work
            volatile double result = 0l;
            for(int j = 0; j < 1000; ++j) {
                result += j * j * 3.14;
            }
            ++completed;
        });
    }
    
    while (completed < NUM_TASKS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << name << " completed " << completed << " out of " << NUM_TASKS
     << " tasks in " << duration.count() << "ms" << std::endl;
    
    executor.stop();
}

void runExecutorBenchmarks() {
    std::cout << "\n=== CPU-Bound Task Benchmarks ===" << std::endl;

    // Test regular executor
    Executor::Config config;
    config.threadCount = std::thread::hardware_concurrency();
    
    Executor regularExecutor(config);
    runExecutorBenchmark(regularExecutor, "Regular Executor");
    
    // Batch Executor benchmarks with different batch sizes
    std::vector<size_t> batchSizes = {8, 16, 32, 64, 128, 256};
    
    for (auto batchSize : batchSizes) {
        config.batchExecutorTaskBatchSize = batchSize;
        BatchExecutor batchExecutor(config);
        runExecutorBenchmark(batchExecutor, "Batch Executor (batch size: " + std::to_string(batchSize) + ")");
    }

    // Add FS benchmark
    std::cout << "\n=== I/O-Bound Task Benchmarks ===" << std::endl;
    runFSExecutorBenchmark();
}

void runEventSystemBenchmark() {
    std::cout << "\n=== Event System Benchmarks ===" << std::endl;
    auto& scheduler = EventScheduler::getInstance();
    
    std::cout << "Starting executor..." << std::endl;
    scheduler.getExecutor().start();

    std::cout << "Registering handlers..." << std::endl;
    EventRegistry::getInstance().registerAllHandlers();

    // Give time for handlers to register
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EventBenchmarker eventBenchmarker(scheduler);
    eventBenchmarker.runBenchmark(1000);

    std::cout << "Stopping executor..." << std::endl;
    scheduler.getExecutor().stop();
}

int main() {
    runExecutorBenchmarks();
    runEventSystemBenchmark();
    return 0;
}
