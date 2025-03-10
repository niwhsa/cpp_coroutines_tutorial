#pragma once
#include "event_types.hpp"
#include "event_scheduler.hpp"
#include <chrono>

class EventHandlers {
public:
  static EventScheduler::Task handleLoginEvent();
  static EventScheduler::Task handleMessageEvent();
  static EventScheduler::Task handleSystemStatusEvent();
  static std::string_view toString(EventType type);
};
