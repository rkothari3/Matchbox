package com.matchbox.core;

import java.util.concurrent.atomic.AtomicLong;

public class Order {
    private static final AtomicLong ID_GEN = new AtomicLong(0);

    private final long orderId;
    private final Side side;
    private final long price;
    private long quantity;
    private final long entryTime;

    public Order(Side side, long price, long quantity) {
        this.orderId = ID_GEN.incrementAndGet();
        this.side = side;
        this.price = price;
        this.quantity = quantity;
        this.entryTime = System.nanoTime();
    }

    public Order(long orderId, Side side, long price, long quantity, long entryTime) {
        this.orderId = orderId;
        this.side = side;
        this.price = price;
        this.quantity = quantity;
        this.entryTime = entryTime;
    }

    public long getOrderId() { return orderId; }
    public Side getSide() { return side; }
    public long getPrice() { return price; }
    public long getQuantity() { return quantity; }
    public long getEntryTime() { return entryTime; }

    public void reduceQuantity(long delta) {
        this.quantity -= delta;
    }

    public boolean isFilled() { return quantity <= 0; }
}
