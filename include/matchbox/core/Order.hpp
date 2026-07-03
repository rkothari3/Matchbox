#pragma once

#include <atomic>

#include "matchbox/core/Side.hpp"
#include "matchbox/core/TimeInForce.hpp"

namespace matchbox {

// A single resting or incoming order. Mutable quantity/filledQuantity track
// partial fills; every other field is fixed at construction.
//
// Orders are shared (std::shared_ptr) between the price-level FIFO queue and
// the id->order lookup map, so the same Order instance lives in both the
// per-level deque and the id hash map.
class Order {
public:
    // Auto-assign the next id from the shared atomic counter.
    Order(Side side, long price, long quantity,
          TimeInForce timeInForce = TimeInForce::GTC, long expiryTimestamp = 0);

    // Explicit id (used by modify's cancel-replace to preserve the id).
    Order(long orderId, Side side, long price, long quantity, long entryTime,
          TimeInForce timeInForce = TimeInForce::GTC, long expiryTimestamp = 0);

    long orderId() const { return orderId_; }
    Side side() const { return side_; }
    long price() const { return price_; }
    long quantity() const { return quantity_; }
    long filledQuantity() const { return filledQuantity_; }
    long entryTime() const { return entryTime_; }
    TimeInForce timeInForce() const { return timeInForce_; }
    long expiryTimestamp() const { return expiryTimestamp_; }

    // Apply a fill: shrink remaining quantity, grow cumulative filled.
    void reduceQuantity(long delta);
    // Overwrite remaining quantity (modify-in-place at the same price).
    void setQuantity(long quantity) { quantity_ = quantity; }

    bool isFilled() const { return quantity_ <= 0; }

private:
    static std::atomic<long> idGen_;

    long orderId_;
    Side side_;
    long price_;
    long quantity_;
    long filledQuantity_;
    long entryTime_;
    TimeInForce timeInForce_;
    long expiryTimestamp_;
};

}
