package com.matchbox.bench;

import com.matchbox.book.OrderBook;
import com.matchbox.core.*;
import org.openjdk.jmh.annotations.*;
import org.openjdk.jmh.infra.Blackhole;
import org.openjdk.jmh.results.format.ResultFormatType;
import org.openjdk.jmh.runner.Runner;
import org.openjdk.jmh.runner.options.Options;
import org.openjdk.jmh.runner.options.OptionsBuilder;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

@State(Scope.Thread)
@BenchmarkMode(Mode.SampleTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@Warmup(iterations = 3, time = 1, timeUnit = TimeUnit.SECONDS)
@Measurement(iterations = 5, time = 2, timeUnit = TimeUnit.SECONDS)
@Fork(1)
public class OrderBookBenchmark {

    private OrderBook book;
    private final AtomicLong priceSeed = new AtomicLong(1000);
    private final AtomicLong cancelId = new AtomicLong(1);
    private final long[] cancelIds;
    private int cancelIndex;

    public OrderBookBenchmark() {
        cancelIds = new long[100_000];
    }

    @Setup(Level.Trial)
    public void setup() {
        book = new OrderBook();
        for (int i = 0; i < 10_000; i++) {
            long price = 1000 + (i * 2);
            var cmd = OrderCommand.newLimit(Side.BUY, price, 10);
            book.processOrder(cmd);
            cancelIds[i] = cmd.getOrder().getOrderId();
        }
        for (int i = 0; i < 10_000; i++) {
            long price = 2000 + (i * 2);
            book.processOrder(OrderCommand.newLimit(Side.SELL, price, 10));
        }
        cancelIndex = 0;
    }

    @Benchmark
    public void addLimitOrder() {
        long price = priceSeed.incrementAndGet();
        book.processOrder(OrderCommand.newLimit(Side.BUY, price, 10));
    }

    @Benchmark
    public void matchMarketOrder() {
        long price = 2000 + (priceSeed.incrementAndGet() % 1000) * 2;
        book.processOrder(OrderCommand.newLimit(Side.BUY, price, 10));
    }

    @Benchmark
    public void cancelOrder() {
        long id = cancelIds[cancelIndex];
        cancelIndex = (cancelIndex + 1) % 10_000;
        book.processOrder(OrderCommand.newCancel(id));
    }

    public static void main(String[] args) throws Exception {
        Options opt = new OptionsBuilder()
                .include(OrderBookBenchmark.class.getSimpleName())
                .result("benchmark-results.json")
                .resultFormat(ResultFormatType.JSON)
                .build();
        new Runner(opt).run();
    }
}
