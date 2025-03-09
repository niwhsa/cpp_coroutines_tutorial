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
#include <iostream>
#include "executor.hpp"

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
        Task(const Task&) = delete; 
        Task& operator=(const Task&) = delete;  
        Task(Task&& other) noexcept : _handle(other._handle) {  
            other._handle = nullptr;
        }
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

    //Add executor-aware awaiter
    struct ExecutorAwaiter {
       
        ExecutorAwaiter(Executor& executor) : _executor(executor) {}

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) {
            _executor.schedule( [handle] () { handle.resume();});
        }
        
        void await_resume() {}
        
    private:
      Executor& _executor;

    };

    Executor& getExecutor() { return _executor; }

    // Add helper to switch executors
    ExecutorAwaiter switchToExecutor() {
        return ExecutorAwaiter(_executor);
    }

    static EventScheduler& getInstance() {
        static EventScheduler instance;
        return instance;
    }

    void registerHandler(std::string_view eventName, std::coroutine_handle<> handle) {
        std::cout << "Registering handler for: " << eventName << "\n" << std::flush;
        auto& handlersVec = _handlersMap[std::string(eventName)];

        // Don't register duplicate handles.
        if (std::find(begin(handlersVec), end(handlersVec), handle) != end(handlersVec)) return;
        
        handlersVec.emplace_back(handle);
    }

    template<typename T>
    void emit(std::string_view eventName, T data) {
        _eventsQ.push(std::make_unique<TypedEvent<T>>(eventName, std::move(data)));
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
        if (0 ==  _eventDataMap.count(std::string(eventName))) {
            throw std::runtime_error("Event data not found");
        }
    
        return std::any_cast<T>(_eventDataMap[std::string(eventName)]);
    }

    void processEvents() {
        while (!empty(_eventsQ)) {
            auto eventPtr = std::move(_eventsQ.front());
            _eventsQ.pop();
            
            eventPtr->storeData(_eventDataMap);
            auto eventName = eventPtr->eventName;
            
            if (_handlersMap.count(eventName) == 0) continue;
            size_t handlerCount = size(_handlersMap[eventName]);
            if (handlerCount == 0) {
                _eventDataMap.erase(eventName);
                continue;
            }
             
            std::shared_ptr<size_t> completedCount = std::make_shared<size_t>(0);
             
            auto handlersVec = std::move(_handlersMap[eventName]);
            _handlersMap.erase(eventName);

            for (auto& handle : handlersVec) {
                _executor.schedule([this, handle, eventName, handlerCount, completedCount]() {
                    handle.resume();
                    // Increment completed count and check if all handlers are done
                    if (++(*completedCount) == handlerCount) { _eventDataMap.erase(eventName);}
                });
            }
        }
    }

    Executor _executor;
    std::unordered_map<std::string, std::vector<std::coroutine_handle<>>> _handlersMap;
    std::queue<std::unique_ptr<Event>> _eventsQ;
    std::unordered_map<std::string, std::any> _eventDataMap;
};

template<typename T>
auto awaitEvent(std::string_view eventName) {
    return EventScheduler::EventAwaiter<T>(EventScheduler::getInstance(), eventName);
}
