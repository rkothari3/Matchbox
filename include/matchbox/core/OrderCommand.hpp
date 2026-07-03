#pragma once

#include <memory>

#include "matchbox/core/Order.hpp"
#include "matchbox/core/OrderType.hpp"
#include "matchbox/core/Side.hpp"
#include "matchbox/core/TimeInForce.hpp"

namespace matchbox {

// A parsed instruction handed to OrderBook::processOrder. Decouples input
// parsing from matching. For LIMIT/MARKET it owns the Order (shared with the
// book once it rests); for CANCEL/MODIFY it carries the target id and new terms.
class OrderCommand {
public:
    static OrderCommand newLimit(Side side, long price, long quantity,
                                 TimeInForce timeInForce = TimeInForce::GTC,
                                 long expiryTimestamp = 0) {
        return OrderCommand(
            OrderType::LIMIT,
            std::make_shared<Order>(side, price, quantity, timeInForce, expiryTimestamp),
            0, 0, 0);
    }

    static OrderCommand newMarket(Side side, long quantity,
                                  TimeInForce timeInForce = TimeInForce::GTC,
                                  long expiryTimestamp = 0) {
        return OrderCommand(
            OrderType::MARKET,
            std::make_shared<Order>(side, 0, quantity, timeInForce, expiryTimestamp),
            0, 0, 0);
    }

    static OrderCommand newCancel(long orderId) {
        return OrderCommand(OrderType::CANCEL, nullptr, orderId, 0, 0);
    }

    static OrderCommand newModify(long orderId, long newPrice, long newQuantity) {
        return OrderCommand(OrderType::MODIFY, nullptr, orderId, newPrice, newQuantity);
    }

    OrderType type() const { return type_; }
    const std::shared_ptr<Order>& order() const { return order_; }
    long cancelOrderId() const { return cancelOrderId_; }
    long modifyPrice() const { return modifyPrice_; }
    long modifyQuantity() const { return modifyQuantity_; }

private:
    OrderCommand(OrderType type, std::shared_ptr<Order> order, long cancelOrderId,
                 long modifyPrice, long modifyQuantity)
        : type_(type),
          order_(std::move(order)),
          cancelOrderId_(cancelOrderId),
          modifyPrice_(modifyPrice),
          modifyQuantity_(modifyQuantity) {}

    OrderType type_;
    std::shared_ptr<Order> order_;
    long cancelOrderId_;
    long modifyPrice_;
    long modifyQuantity_;
};

}
