#pragma once
#include <array>
#include <atomic>
#include <mutex>

//Lock free MPMC queue for memory optimization
template <typename T>
class MPMCQueue {
private:
    struct Node {
        std::atomic<Node*> next{nullptr};
        std::optional<T> data; //use optional to allow default ctor , required for std:array

        Node() = default;
    };

    std::atomic<Node*> head{nullptr};
    std::atomic<Node*> tail{nullptr};
    std::mutex _mutex;

    static constexpr size_t POOL_SIZE = 1024;
    std::array<Node, POOL_SIZE> node_pool;
    std::atomic<size_t> pool_idx{0};

    Node* allocateNode() {
        std::lock_guard<std::mutex> lock(_mutex);
        auto idx = pool_idx.fetch_add(1) % POOL_SIZE;
        auto node = &node_pool[idx];
        node->next.store(nullptr);
        node->data.reset();
        return node;
    }

public:
    MPMCQueue() {
        auto dummy = allocateNode();
        head.store(dummy);
        tail.store(dummy);
    }

    void push(T value) {
        auto node = allocateNode();
        node->data.emplace(std::move(value));

        while(true) {
            auto old_tail = tail.load(std::memory_order_acquire);
            auto next = old_tail->next.load(std::memory_order_acquire);

            if (old_tail == tail.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    if (old_tail->next.compare_exchange_weak(next, node,
                        std::memory_order_release, std::memory_order_acquire)) {
                        tail.compare_exchange_strong(old_tail, node,
                            std::memory_order_release, std::memory_order_relaxed);
                        return;
                    }
                } else {
                    tail.compare_exchange_strong(old_tail, next,
                        std::memory_order_release, std::memory_order_acquire);
                }
            }
        }
    }

    bool try_pop(T& value) {
        while (true) {
            auto old_head = head.load(std::memory_order_acquire);
            if (!old_head) return false;
            
            auto next = old_head->next.load(std::memory_order_acquire);
            if (!next) return false;

            if (old_head == head.load(std::memory_order_acquire)) {
                if (head.compare_exchange_weak(old_head, next,
                    std::memory_order_release, std::memory_order_acquire)) {
                   
                    if (!next->data.has_value()) return false;
                    value =  std::move(next->data.value());
                    next->data.reset();
                   
                    return true;
                }
            }
        }
    }
}; 
