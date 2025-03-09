#pragma once
#include "event_scheduler.hpp"
#include "event_handlers.hpp"

class EventRegistry {
  public:
    static EventRegistry& getInstance();
    void registerAllHandlers();
  
  private:
    std::vector<EventScheduler::Task> _tasksVec;
};
