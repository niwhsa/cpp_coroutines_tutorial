#include "event_scheduler.hpp"
#include <iostream>
#include <string>


EventScheduler::Task handleLoginEvents() {
    auto userData = co_await awaitEvent<std::string>("user_login");
    std::cout << "User logged in: " << userData << std::endl;
    
    while (true) {
        co_await std::suspend_always {};
    }
}

EventScheduler::Task handleMessageEvents() {
    auto message = co_await awaitEvent<std::string>("new_message");
    std::cout << "New message received: " << message << std::endl;

    while (true) {
        co_await std::suspend_always{};
    }
}

EventScheduler::Task handleSystemEvents() {
    auto status = co_await awaitEvent<int>("system_status");
    std::cout << "System status changed: " << status << std::endl;
    
    while (true) {
        co_await std::suspend_always{};
    }
}

int main() {
    auto& scheduler = EventScheduler::getInstance();

    auto loginHandler   = handleLoginEvents();
    auto messageHandler = handleMessageEvents();
    auto systemHandler  = handleSystemEvents();

    scheduler.emit("user_login", std::string("john_doe"));
    scheduler.emit("new_message", std::string("Hello, World!"));
    scheduler.emit("system_status", 1);
 
    
    return 0;
}
