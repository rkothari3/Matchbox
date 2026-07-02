package com.matchbox.core;

public class OrderCommand {
    private final OrderType type;
    private final Order order;
    private final long cancelOrderId;
    private final long modifyPrice;
    private final long modifyQuantity;

    private OrderCommand(OrderType type, Order order, long cancelOrderId,
                         long modifyPrice, long modifyQuantity) {
        this.type = type;
        this.order = order;
        this.cancelOrderId = cancelOrderId;
        this.modifyPrice = modifyPrice;
        this.modifyQuantity = modifyQuantity;
    }

    public static OrderCommand newLimit(Side side, long price, long quantity) {
        return newLimit(side, price, quantity, TimeInForce.GTC, 0);
    }

    public static OrderCommand newLimit(Side side, long price, long quantity,
                                        TimeInForce timeInForce, long expiryTimestamp) {
        return new OrderCommand(OrderType.LIMIT,
                new Order(side, price, quantity, timeInForce, expiryTimestamp),
                0, 0, 0);
    }

    public static OrderCommand newMarket(Side side, long quantity) {
        return newMarket(side, quantity, TimeInForce.GTC, 0);
    }

    public static OrderCommand newMarket(Side side, long quantity,
                                         TimeInForce timeInForce, long expiryTimestamp) {
        return new OrderCommand(OrderType.MARKET,
                new Order(side, 0, quantity, timeInForce, expiryTimestamp),
                0, 0, 0);
    }

    public static OrderCommand newCancel(long orderId) {
        return new OrderCommand(OrderType.CANCEL, null, orderId, 0, 0);
    }

    public static OrderCommand newModify(long orderId, long newPrice, long newQuantity) {
        return new OrderCommand(OrderType.MODIFY, null, orderId, newPrice, newQuantity);
    }

    public OrderType getType() { return type; }
    public Order getOrder() { return order; }
    public long getCancelOrderId() { return cancelOrderId; }
    public long getModifyPrice() { return modifyPrice; }
    public long getModifyQuantity() { return modifyQuantity; }
}
