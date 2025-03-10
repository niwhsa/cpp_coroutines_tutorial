#include "event_handlers.hpp"

EventScheduler::Task EventHandlers::handleLoginEvent() {
    co_await EventScheduler::getInstance().switchToExecutor();
    auto start = std::chrono::high_resolution_clock::now();
    auto userData = co_await awaitEvent<std::string>(toString(EventType::UserLogin));
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "User logged in: " << userData 
              << " (Latency: " << duration.count() << " microseconds)\n";
    
    while (true) {
        co_await std::suspend_always{};
    }
}

EventScheduler::Task EventHandlers::handleMessageEvent() {
    co_await EventScheduler::getInstance().switchToExecutor();
    auto start = std::chrono::high_resolution_clock::now();
    auto message = co_await awaitEvent<std::string>(toString(EventType::NewMessage));
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "New message received: " << message 
              << " (Latency: " << duration.count() << " microseconds)\n";
    
    while (true) {
        co_await std::suspend_always{};
    }
}

EventScheduler::Task EventHandlers::handleSystemStatusEvent() {
    co_await EventScheduler::getInstance().switchToExecutor();
    auto start = std::chrono::high_resolution_clock::now();
    auto status = co_await awaitEvent<int>(toString(EventType::SystemStatus));
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "System status changed: " << status 
              << " (Latency: " << duration.count() << " microseconds)\n";
    
    while (true) {
        co_await std::suspend_always{};
    }
}

std::string_view EventHandlers::toString(EventType type) {
    switch (type) {
        case EventType::UserLogin: return "user_login";
        case EventType::NewMessage: return "new_message";
        case EventType::SystemStatus: return "system_status";
    }
    throw std::runtime_error("Unknown event type");
}
