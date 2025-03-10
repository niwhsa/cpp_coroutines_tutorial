#include "event_benchmarker.hpp"

EventBenchmarker::EventBenchmarker(EventScheduler& scheduler)
: _scheduler(scheduler) {}

void EventBenchmarker::runBenchmark(int iterations) {
    std::cout<< "\nStarting benchmark with "<< iterations << " iterations..." <<std::endl;
    _latencyVec.reserve(iterations * 3);

    for (int i = 0; i < iterations; ++i) {
        auto emitStart = std::chrono::high_resolution_clock::now();
        emitTestEvents();
        auto emitEnd = std::chrono::high_resolution_clock::now();
        auto emitDuration = std::chrono::duration_cast<std::chrono::microseconds>(emitEnd - emitStart);
        _latencyVec.emplace_back(emitDuration.count());

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    printStatistics();
}

void EventBenchmarker::emitTestEvents() {
    _scheduler.emit(EventHandlers::toString(EventType::UserLogin), std::string("jack_smith"));
    _scheduler.emit(EventHandlers::toString(EventType::NewMessage), std::string("Hello, cpp20 coroutines world!"));
    _scheduler.emit(EventHandlers::toString(EventType::SystemStatus), 1);
}

void EventBenchmarker::printStatistics() const {
    if (empty(_latencyVec)) {
        std::cout << "No benchmark data collected." << std::endl;
        return;
    }

    std::vector<long long> sortedLatencies = _latencyVec;
    std::sort(begin(sortedLatencies), end(sortedLatencies));

    size_t size = sortedLatencies.size();
    double avg = std::accumulate(begin(sortedLatencies), end(sortedLatencies), 0.0) / size;
    auto median = sortedLatencies[size / 2];
    auto p95 = sortedLatencies[static_cast<size_t>(size * 0.95)];
    auto p99 = sortedLatencies[static_cast<size_t>(size * 0.99)];

    std::cout << "\nEmission Time Statistics (microseconds):" << std::endl;
    std::cout << "Average: " << avg << " microseconds" << std::endl;
    std::cout << "Median: " << median << " microseconds" << std::endl;
    std::cout << "95th percentile: " << p95 << " microseconds" << std::endl;
    std::cout << "99th percentile: " << p99 << " microseconds" << std::endl;
    std::cout << "Sample size: " << size << " events" << std::endl;
}
