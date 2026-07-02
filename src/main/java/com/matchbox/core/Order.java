package com.matchbox.core;

import java.util.concurrent.atomic.AtomicLong;

public class Order {
    private static final AtomicLong ID_GEN = new AtomicLong(0);

    private final long orderId;
    private final Side side;
    private final long price;
    private long quantity;
    private long filledQuantity;
    private final long entryTime;
    private final TimeInForce timeInForce;
    private final long expiryTimestamp;

    public Order(Side side, long price, long quantity) {
        this(ID_GEN.incrementAndGet(), side, price, quantity, System.nanoTime(),
                TimeInForce.GTC, 0);
    }

    public Order(Side side, long price, long quantity, TimeInForce timeInForce) {
        this(ID_GEN.incrementAndGet(), side, price, quantity, System.nanoTime(),
                timeInForce, 0);
    }

    public Order(Side side, long price, long quantity, TimeInForce timeInForce, long expiryTimestamp) {
        this(ID_GEN.incrementAndGet(), side, price, quantity, System.nanoTime(),
                timeInForce, expiryTimestamp);
    }

    public Order(long orderId, Side side, long price, long quantity, long entryTime) {
        this(orderId, side, price, quantity, entryTime, TimeInForce.GTC, 0);
    }

    public Order(long orderId, Side side, long price, long quantity, long entryTime,
                 TimeInForce timeInForce, long expiryTimestamp) {
        this.orderId = orderId;
        this.side = side;
        this.price = price;
        this.quantity = quantity;
        this.filledQuantity = 0;
        this.entryTime = entryTime;
        this.timeInForce = timeInForce;
        this.expiryTimestamp = expiryTimestamp;
    }

    public long getOrderId() { return orderId; }
    public Side getSide() { return side; }
    public long getPrice() { return price; }
    public long getQuantity() { return quantity; }
    public long getFilledQuantity() { return filledQuantity; }
    public long getEntryTime() { return entryTime; }
    public TimeInForce getTimeInForce() { return timeInForce; }
    public long getExpiryTimestamp() { return expiryTimestamp; }

    public void reduceQuantity(long delta) {
        this.quantity -= delta;
        this.filledQuantity += delta;
    }

    public void setQuantity(long quantity) {
        this.quantity = quantity;
    }

    public boolean isFilled() { return quantity <= 0; }
}
