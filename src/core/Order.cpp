#include "matchbox/core/Order.hpp"

#include "matchbox/core/Clock.hpp"

namespace matchbox {

std::atomic<long> Order::idGen_{0};

Order::Order(Side side, long price, long quantity,
             TimeInForce timeInForce, long expiryTimestamp)
    : Order(idGen_.fetch_add(1) + 1, side, price, quantity, nowNanos(),
            timeInForce, expiryTimestamp) {}

Order::Order(long orderId, Side side, long price, long quantity, long entryTime,
             TimeInForce timeInForce, long expiryTimestamp)
    : orderId_(orderId),
      side_(side),
      price_(price),
      quantity_(quantity),
      filledQuantity_(0),
      entryTime_(entryTime),
      timeInForce_(timeInForce),
      expiryTimestamp_(expiryTimestamp) {}

void Order::reduceQuantity(long delta) {
    quantity_ -= delta;
    filledQuantity_ += delta;
}

}
