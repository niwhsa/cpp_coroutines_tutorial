#include "event_scheduler.hpp"
#include <iostream>
#include <string>
#include <chrono>

// Event type definitions
enum class EventType {
    UserLogin,
    NewMessage,
    SystemStatus
};

// Event handlers class to contain all event handling logic
class EventHandlers {
public:
    static EventScheduler::Task handleLoginEvent() {
        co_await EventScheduler::getInstance().switchToExecutor();
        auto userData = co_await awaitEvent<std::string>(toString(EventType::UserLogin));
        std::cout << "User logged in: " << userData << std::endl;
        
        while (true) {
            co_await std::suspend_always{};
        }
    }

    static EventScheduler::Task handleMessageEvent() {
        co_await EventScheduler::getInstance().switchToExecutor();
        auto message = co_await awaitEvent<std::string>(toString(EventType::NewMessage));
        std::cout << "New message received: " << message << std::endl;
        
        while (true) {
            co_await std::suspend_always{};
        }
    }

    static EventScheduler::Task handleSystemStatusEvent() {
        co_await EventScheduler::getInstance().switchToExecutor();
        auto status = co_await awaitEvent<int>(toString(EventType::SystemStatus));
        std::cout << "System status changed: " << status << std::endl;
        
        while (true) {
            co_await std::suspend_always{};
        }
    }

    static std::string_view toString(EventType type) {
        switch (type) {
            case EventType::UserLogin: return "user_login";
            case EventType::NewMessage: return "new_message";
            case EventType::SystemStatus: return "system_status";
        }
        throw std::runtime_error("Unknown event type");
    }
};

// Event registry to manage handler registration
class EventRegistry {
public:
    static EventRegistry& getInstance() {
        static EventRegistry instance;
        return instance;
    }

    void registerAllHandlers() {
        tasks.push_back(EventHandlers::handleLoginEvent());
        tasks.push_back(EventHandlers::handleMessageEvent());
        tasks.push_back(EventHandlers::handleSystemStatusEvent());
    }

private:
    std::vector<EventScheduler::Task> tasks;
};

int main() {
    auto& scheduler = EventScheduler::getInstance();
    
    std::cout << "Starting executor..." << std::endl;
    scheduler.getExecutor().start();

    std::cout << "Registering handlers..." << std::endl;
    EventRegistry::getInstance().registerAllHandlers();

    // Give time for handlers to register
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "Emitting user_login event..." << std::endl;
    scheduler.emit(EventHandlers::toString(EventType::UserLogin), std::string("john_doe"));
    
    std::cout << "Emitting new_message event..." << std::endl;
    scheduler.emit(EventHandlers::toString(EventType::NewMessage), std::string("Hello, World!"));
    
    std::cout << "Emitting system_status event..." << std::endl;
    scheduler.emit(EventHandlers::toString(EventType::SystemStatus), 1);

    // Wait for events to process
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Stopping executor..." << std::endl;
    scheduler.getExecutor().stop();
    return 0;
}
