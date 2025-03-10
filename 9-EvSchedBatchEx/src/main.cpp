#include "event_scheduler.hpp"
#include "event_registry.hpp"
#include "event_benchmarker.hpp"
#include "batch_executor.hpp"

void runExcutorBenchmark(Executor& executor, const std::string& name) {
    std::cout << "\nTesting " << name << "..." << std::endl;
    
    executor.start();
    //Create some test tasks
    constexpr int NUM_TASKS = 1000000;
    std::atomic<int> completed{0};
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_TASKS; ++i) {
        executor.schedule([&completed] () {
            //Simulate some work
            //std:: cout<<"So far scheduled "<< completed <<" out of " << NUM_TASKS << " \n";
            //std::this_thread::sleep_for(std::chrono::microseconds(100)); 
            // CPU-bound work
            volatile int result = 0;
            for(int j = 0; j < 1000; ++j) {
                result += j * j;
            }
            ++completed;
        });
    }
    
    while (completed < NUM_TASKS) {
       // std:: cout<<"So far completed "<< completed <<" out of " << NUM_TASKS << " \n";
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << name << " completed " << completed << " out of " << NUM_TASKS
     << " tasks in " << duration.count() << "ms" << std::endl;
    
    executor.stop();
}

void runExecutorBenchmarks() {
    // Test regular executor
    Executor::Config config;
    config.threadCount = 4;
    config.minThreads = 2;
    config.tasksPerThreadThreshold = 100;  // Increase threshold to prevent excessive scaling
    config.keepAliveTime = std::chrono::seconds(30);
    config.enableWorkStealing = true;  // Disable work stealing for regular executor
    
    Executor regularExecutor(config);
    runExcutorBenchmark(regularExecutor, "Regular Executor");
    
    // Test batch executor
    BatchExecutor batchExecutor(config);
    runExcutorBenchmark(batchExecutor, "Batch Executor");
}

int main() {
    
    runExecutorBenchmarks();
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
    return 0;
}
