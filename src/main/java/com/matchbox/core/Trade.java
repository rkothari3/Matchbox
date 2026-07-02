package com.matchbox.core;

public class Trade {
    private final long price;
    private final long quantity;
    private final long timestamp;
    private final long makerOrderId;
    private final long takerOrderId;

    public Trade(long price, long quantity, long timestamp,
                 long makerOrderId, long takerOrderId) {
        this.price = price;
        this.quantity = quantity;
        this.timestamp = timestamp;
        this.makerOrderId = makerOrderId;
        this.takerOrderId = takerOrderId;
    }

    public long getPrice() { return price; }
    public long getQuantity() { return quantity; }
    public long getTimestamp() { return timestamp; }
    public long getMakerOrderId() { return makerOrderId; }
    public long getTakerOrderId() { return takerOrderId; }

    @Override
    public String toString() {
        return "Trade{" + "price=" + price + ", qty=" + quantity +
                ", ts=" + timestamp + ", maker=" + makerOrderId +
                ", taker=" + takerOrderId + '}';
    }
}
