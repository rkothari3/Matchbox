package com.matchbox.book;

import com.matchbox.core.*;
import java.util.*;

/**
 * Core limit order book with price-time priority matching.
 *
 * Data structure choice: TreeMap<Long, PriceLevel> per side.
 *
 * Why TreeMap over alternatives:
 * - TreeMap: O(log n) price level insert/delete, O(1) best-bid/ask via firstKey(),
 *   maintains sorted order intrinsically. Cache-friendly traversal of price levels.
 * - Skip list: O(log n) expected, but higher constant factors, more pointer chasing.
 * - Array-based: O(n) insert/delete for sparse books, only practical for tight
 *   fixed-price grids. Not suitable for arbitrary price levels.
 *
 * TreeMap gives the best balance of predictable log-time operations, low overhead,
 * and simple correct implementation for a single-threaded matching engine.
 */
public class OrderBook {

    private final TreeMap<Long, PriceLevel> bids;
    private final TreeMap<Long, PriceLevel> asks;
    private final Map<Long, Order> orderMap;
    private final List<Trade> tradeLog;

    public OrderBook() {
        this.bids = new TreeMap<>(Comparator.reverseOrder());
        this.asks = new TreeMap<>(Long::compareTo);
        this.orderMap = new HashMap<>();
        this.tradeLog = new ArrayList<>();
    }

    public void processOrder(OrderCommand cmd) {
        switch (cmd.getType()) {
            case LIMIT -> processLimit(cmd.getOrder());
            case MARKET -> processMarket(cmd.getOrder());
            case CANCEL -> processCancel(cmd.getCancelOrderId());
            case MODIFY -> processModify(cmd.getCancelOrderId(),
                    cmd.getModifyPrice(), cmd.getModifyQuantity());
        }
    }

    private void processLimit(Order taker) {
        if (taker.getSide() == Side.BUY) {
            matchBids(taker, true);
        } else {
            matchAsks(taker, true);
        }
        if (!taker.isFilled()) {
            addToBook(taker);
        }
    }

    private void processMarket(Order taker) {
        if (taker.getSide() == Side.BUY) {
            matchBids(taker, false);
        } else {
            matchAsks(taker, false);
        }
    }

    private void matchBids(Order taker, boolean isLimit) {
        while (!taker.isFilled() && !asks.isEmpty()) {
            Map.Entry<Long, PriceLevel> bestAskEntry = asks.firstEntry();
            if (isLimit && taker.getPrice() < bestAskEntry.getKey()) break;

            PriceLevel level = bestAskEntry.getValue();
            matchAgainstLevel(taker, level);
            if (level.isEmpty()) asks.pollFirstEntry();
        }
    }

    private void matchAsks(Order taker, boolean isLimit) {
        while (!taker.isFilled() && !bids.isEmpty()) {
            Map.Entry<Long, PriceLevel> bestBidEntry = bids.firstEntry();
            if (isLimit && taker.getPrice() > bestBidEntry.getKey()) break;

            PriceLevel level = bestBidEntry.getValue();
            matchAgainstLevel(taker, level);
            if (level.isEmpty()) bids.pollFirstEntry();
        }
    }

    private void matchAgainstLevel(Order taker, PriceLevel level) {
        while (!taker.isFilled() && !level.isEmpty()) {
            Order resting = level.peek();
            long matchQty = Math.min(taker.getQuantity(), resting.getQuantity());
            long ts = System.nanoTime();

            tradeLog.add(new Trade(level.getPrice(), matchQty, ts,
                    resting.getOrderId(), taker.getOrderId()));

            resting.reduceQuantity(matchQty);
            taker.reduceQuantity(matchQty);

            if (resting.isFilled()) {
                level.poll();
                orderMap.remove(resting.getOrderId());
            }
        }
    }

    private void addToBook(Order order) {
        TreeMap<Long, PriceLevel> book = order.getSide() == Side.BUY ? bids : asks;
        PriceLevel level = book.get(order.getPrice());
        if (level == null) {
            level = new PriceLevel(order.getPrice());
            book.put(order.getPrice(), level);
        }
        level.add(order);
        orderMap.put(order.getOrderId(), order);
    }

    private void processCancel(long orderId) {
        Order order = orderMap.remove(orderId);
        if (order == null) return;

        TreeMap<Long, PriceLevel> book = order.getSide() == Side.BUY ? bids : asks;
        PriceLevel level = book.get(order.getPrice());
        if (level != null) {
            level.remove(order);
            if (level.isEmpty()) book.remove(order.getPrice());
        }
    }

    private void processModify(long orderId, long newPrice, long newQuantity) {
        Order existing = orderMap.get(orderId);
        if (existing == null) return;

        Side side = existing.getSide();
        long oldPrice = existing.getPrice();

        processCancel(orderId);
        processLimit(new Order(orderId, side, newPrice, newQuantity, System.nanoTime()));
    }

    // --- Query methods ---

    public TopOfBook getTopOfBook() {
        long bidPrice = Long.MIN_VALUE, bidQty = 0;
        long askPrice = Long.MAX_VALUE, askQty = 0;

        if (!bids.isEmpty()) {
            Map.Entry<Long, PriceLevel> e = bids.firstEntry();
            bidPrice = e.getKey();
            bidQty = e.getValue().totalQuantity();
        }
        if (!asks.isEmpty()) {
            Map.Entry<Long, PriceLevel> e = asks.firstEntry();
            askPrice = e.getKey();
            askQty = e.getValue().totalQuantity();
        }
        return new TopOfBook(bidPrice, askPrice, bidQty, askQty);
    }

    public List<BookDepthEntry> getBidDepth() {
        List<BookDepthEntry> result = new ArrayList<>();
        for (Map.Entry<Long, PriceLevel> e : bids.entrySet()) {
            result.add(new BookDepthEntry(e.getKey(), e.getValue().totalQuantity(), e.getValue().size()));
        }
        return result;
    }

    public List<BookDepthEntry> getAskDepth() {
        List<BookDepthEntry> result = new ArrayList<>();
        for (Map.Entry<Long, PriceLevel> e : asks.entrySet()) {
            result.add(new BookDepthEntry(e.getKey(), e.getValue().totalQuantity(), e.getValue().size()));
        }
        return result;
    }

    public List<Trade> getTradeLog() {
        return Collections.unmodifiableList(tradeLog);
    }

    public void clearTradeLog() {
        tradeLog.clear();
    }

    public int orderCount() {
        return orderMap.size();
    }
}
