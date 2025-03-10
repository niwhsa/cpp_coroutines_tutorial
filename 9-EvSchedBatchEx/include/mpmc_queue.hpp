#pragma once
#include <array>
#include <atomic>
#include <mutex>

//Lock free MPMC queue for memory optimization
template <typename T>
class MPMCQueue {
private:
    size_t _poolSize;

    struct Node {
        std::atomic<Node*> next{nullptr};
        std::optional<T> data; //use optional to allow default ctor , required for std:array

        Node() = default;
    };

    std::atomic<Node*> head{nullptr};
    std::atomic<Node*> tail{nullptr};
    std::mutex _mutex;

    //Dynamic task node pool for MPMC queue mgmt 
    std::vector<std::unique_ptr<Node>> node_pool;
    std::atomic<size_t> pool_idx{0};

    Node* allocateNode() {
        auto idx = pool_idx.fetch_add(1);

        if (idx >= size(node_pool)) {  //pool exhausted, resize
            std::lock_guard<std::mutex> lock(_mutex);
            size_t newSize = node_pool.size() << 1; //double it
            resizePool(newSize);
        }
        
        auto node = node_pool[idx % node_pool.size()].get();
        node->next.store(nullptr);
        node->data.reset();
        return node;
    }

public:
     explicit MPMCQueue(size_t initPoolSize = 1024) : _poolSize(initPoolSize) {
        node_pool.reserve(_poolSize);
        for (size_t i = 0; i < _poolSize; ++i) {
            node_pool.emplace_back(std::make_unique<Node>());
        } 

        auto dummy = allocateNode();
        head.store(dummy);
        tail.store(dummy);
    }

    bool empty() const {
        auto h = head.load(std::memory_order_acquire);
        auto n = h->next.load(std::memory_order_acquire);
        return n == nullptr;
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
    
    // Method to resize pool
    void resizePool(size_t newSize) {
        if (newSize <= _poolSize) return;
        
        size_t additionalNodes = newSize - _poolSize;
        node_pool.reserve(newSize);
        
        for (size_t i = 0; i < additionalNodes; ++i) {
            node_pool.emplace_back(std::make_unique<Node>());
        }
        
        _poolSize = newSize;
    }

    ~MPMCQueue() {
        head.store(nullptr);
        tail.store(nullptr);
    }

}; 
