#include "event_registry.hpp"

EventRegistry& EventRegistry::getInstance() {
    static EventRegistry instance;
    return instance;
}

void EventRegistry::registerAllHandlers() {
    _tasksVec.emplace_back(EventHandlers::handleLoginEvent());
    _tasksVec.emplace_back(EventHandlers::handleMessageEvent());
    _tasksVec.emplace_back(EventHandlers::handleSystemStatusEvent());
}
