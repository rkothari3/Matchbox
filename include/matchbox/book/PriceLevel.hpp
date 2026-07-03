#pragma once

#include <deque>
#include <memory>

#include "matchbox/core/Order.hpp"

namespace matchbox {

// A single price rung: a FIFO queue of orders enforcing time priority.
//
// std::deque mirrors Java's ArrayDeque — O(1) push_back / pop_front / front,
// which is all the hot path (add new order, take best-priority order) needs.
// Cancel of an interior order is a linear scan (same as ArrayDeque.remove);
// see the README note on the intrusive-list O(1)-cancel optimization.
class PriceLevel {
public:
    explicit PriceLevel(long price) : price_(price) {}

    long price() const { return price_; }

    void add(const std::shared_ptr<Order>& order) { orders_.push_back(order); }
    const std::shared_ptr<Order>& peek() const { return orders_.front(); }
    void poll() { orders_.pop_front(); }
    bool remove(const std::shared_ptr<Order>& order);

    bool isEmpty() const { return orders_.empty(); }
    int size() const { return static_cast<int>(orders_.size()); }
    long totalQuantity() const;

private:
    long price_;
    std::deque<std::shared_ptr<Order>> orders_;
};

}
