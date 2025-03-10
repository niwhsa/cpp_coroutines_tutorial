#include "event_scheduler.hpp"
#include "event_registry.hpp"
#include "event_benchmarker.hpp"

int main() {
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
