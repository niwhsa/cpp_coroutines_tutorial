#pragma once
#include "event_scheduler.hpp"
#include "event_handlers.hpp"
#include <algorithm>
#include <numeric>

class EventBenchmarker {
    public:
      explicit EventBenchmarker(EventScheduler& scheduler);
      void runBenchmark(int iterations = 1000);
    
    private:
      void emitTestEvents();
      void printStatistics() const;
      
      EventScheduler& _scheduler;
      std::vector<long long> _latencyVec;
};
