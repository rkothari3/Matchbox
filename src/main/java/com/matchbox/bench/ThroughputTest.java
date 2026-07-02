package com.matchbox.bench;

import com.matchbox.book.OrderBook;
import com.matchbox.core.*;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Standalone throughput stress test.
 *
 * Pre-seeds the book with 20k orders, then fires a mixed workload
 * (limits, markets, cancels) in a tight loop for a fixed duration.
 * Reports aggregate ops/sec and breaks down by operation type.
 */
public class ThroughputTest {

    private static final long WARMUP_NANOS = 1_000_000_000L;
    private static final long TEST_NANOS = 3_000_000_000L;

    private final OrderBook book = new OrderBook();
    private final AtomicLong priceSeq = new AtomicLong(100_000);
    private final long[] cancelIds = new long[10_000];
    private int cancelIdx;

    private volatile boolean running = true;

    private long limitCount;
    private long marketCount;
    private long cancelCount;
    private long totalOrders;

    private void seed() {
        for (int i = 0; i < 10_000; i++) {
            long price = 200_000 + (i * 2);
            var cmd = OrderCommand.newLimit(Side.BUY, price, 10);
            book.processOrder(cmd);
            cancelIds[i] = cmd.getOrder().getOrderId();
        }
        for (int i = 0; i < 10_000; i++) {
            long price = 300_000 + (i * 2);
            book.processOrder(OrderCommand.newLimit(Side.SELL, price, 10));
        }
    }

    private void run() {
        seed();

        // warmup
        long warmupEnd = System.nanoTime() + WARMUP_NANOS;
        while (System.nanoTime() < warmupEnd) {
            issueOrders();
        }

        // measured run
        long start = System.nanoTime();
        long end = start + TEST_NANOS;
        long opsStart = totalOrders;
        while (System.nanoTime() < end) {
            issueOrders();
        }
        long opsEnd = totalOrders;
        long elapsed = System.nanoTime() - start;

        double elapsedSec = elapsed / 1_000_000_000.0;
        long total = opsEnd - opsStart;

        System.out.println("--- ThroughputTest Results ---");
        System.out.printf("Test duration:      %.2f sec%n", elapsedSec);
        System.out.printf("Total orders:       %d%n", total);
        System.out.printf("Throughput:         %,.0f orders/sec%n", total / elapsedSec);
        System.out.printf("  Limit orders:     %d (%.1f%%)%n", limitCount,
                100.0 * limitCount / total);
        System.out.printf("  Market orders:    %d (%.1f%%)%n", marketCount,
                100.0 * marketCount / total);
        System.out.printf("  Cancels:          %d (%.1f%%)%n", cancelCount,
                100.0 * cancelCount / total);
        System.out.printf("Book depth:         %d orders, %d bid levels, %d ask levels%n",
                book.orderCount(),
                book.getBidDepth().size(),
                book.getAskDepth().size());
    }

    private void issueOrders() {
        long price = priceSeq.incrementAndGet();
        switch ((int) (price % 10)) {
            case 0, 1, 2, 3, 4, 5, 6 -> {
                book.processOrder(OrderCommand.newLimit(
                        price % 2 == 0 ? Side.BUY : Side.SELL, price, 10));
                limitCount++;
                totalOrders++;
            }
            case 7, 8 -> {
                book.processOrder(OrderCommand.newMarket(
                        price % 2 == 0 ? Side.BUY : Side.SELL, 5));
                marketCount++;
                totalOrders++;
            }
            case 9 -> {
                if (cancelIdx < cancelIds.length) {
                    book.processOrder(OrderCommand.newCancel(cancelIds[cancelIdx++]));
                    cancelCount++;
                    totalOrders++;
                }
            }
        }
    }

    public static void main(String[] args) {
        new ThroughputTest().run();
    }
}
