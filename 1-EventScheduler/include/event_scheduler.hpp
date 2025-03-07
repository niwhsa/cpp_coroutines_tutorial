#pragma once
#include <unordered_map>
#include <queue>
#include <vector>
#include <memory>
#include <any>
#include <string>
#include <string_view>
#include <coroutine>
#include <stdexcept>

class EventScheduler {
public:

    struct Task {
        struct promise_type {
            Task get_return_object() {
                return Task(std::coroutine_handle<promise_type>::from_promise(*this));
            }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() { std::terminate(); }
        };

        Task(std::coroutine_handle<promise_type> handle) : _handle(handle) {}
        ~Task() {
            if (_handle) _handle.destroy();
        }

    private:
        std::coroutine_handle<promise_type> _handle;
    };

    template<typename T>
    struct EventAwaiter {
        EventAwaiter(EventScheduler& scheduler, std::string_view eventName)
            : _scheduler(scheduler), _eventName(eventName) {}

        bool await_ready() const noexcept { return false; }
        
        void await_suspend(std::coroutine_handle<> handle) {
            _scheduler.registerHandler(_eventName, handle);
        }

        T await_resume() {
            return _scheduler.getEventData<T>(_eventName);
        }

    private:
        EventScheduler& _scheduler;
        std::string_view _eventName;
    };

    static EventScheduler& getInstance() {
        static EventScheduler instance;
        return instance;
    }

    void registerHandler(std::string_view eventName, std::coroutine_handle<> handle) {
        _handlers[std::string(eventName)].push_back(handle);
    }

    template<typename T>
    void emit(std::string_view eventName, T data) {
        _events.push(std::make_unique<TypedEvent<T>>(eventName, std::move(data)));
        processEvents();
    }

private:

    struct Event {
        Event(std::string_view name) : eventName(name) {}
        virtual ~Event() = default;
        virtual void storeData(std::unordered_map<std::string, std::any>& eventData) = 0;
        std::string eventName;
    };

    template<typename T>
    struct TypedEvent : Event {
        TypedEvent(std::string_view name, T value) 
            : Event(name), data(std::move(value)) {}
        void storeData(std::unordered_map<std::string, std::any>& eventData) override {
            eventData[eventName] = data;
        }
        T data;
     };

    template<typename T>
    T getEventData(std::string_view eventName) {
        if (0 ==  _eventData.count(std::string(eventName))) {
            throw std::runtime_error("Event data not found");
        }
    
        return std::any_cast<T>(_eventData[std::string(eventName)]);
    }

    void processEvents() {
        while (!empty(_events)) {
            auto event = std::move(_events.front());
            _events.pop();
            
            event->storeData(_eventData);

            // Resume all coroutines waiting for this event
            for (auto& handle : _handlers[event->eventName]) {
                    handle.resume();
            }

            _eventData.erase(event->eventName);
        }
    }

    std::unordered_map<std::string, std::vector<std::coroutine_handle<>>> _handlers;
    std::queue<std::unique_ptr<Event>> _events;
    std::unordered_map<std::string, std::any> _eventData;
};

template<typename T>
auto awaitEvent(std::string_view eventName) {
    return EventScheduler::EventAwaiter<T>(EventScheduler::getInstance(), eventName);
}
