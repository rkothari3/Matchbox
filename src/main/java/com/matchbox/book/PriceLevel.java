package com.matchbox.book;

import com.matchbox.core.Order;
import java.util.ArrayDeque;

public class PriceLevel {
    private final long price;
    private final ArrayDeque<Order> orders;

    public PriceLevel(long price) {
        this.price = price;
        this.orders = new ArrayDeque<>();
    }

    public long getPrice() { return price; }

    public void add(Order order) {
        orders.addLast(order);
    }

    public Order peek() {
        return orders.peekFirst();
    }

    public Order poll() {
        return orders.pollFirst();
    }

    public boolean remove(Order order) {
        return orders.remove(order);
    }

    public boolean isEmpty() {
        return orders.isEmpty();
    }

    public int size() {
        return orders.size();
    }

    public long totalQuantity() {
        return orders.stream().mapToLong(Order::getQuantity).sum();
    }

    public ArrayDeque<Order> getOrders() {
        return orders;
    }
}
