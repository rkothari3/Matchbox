#include <algorithm>
#include <vector>

#include <benchmark/benchmark.h>

#include "matchbox/book/OrderBook.hpp"

using namespace matchbox;

namespace {

// The p-th percentile of the per-repetition samples Google Benchmark collects
// (one aggregate per --benchmark_repetitions run). ComputeStatistics takes a
// plain function pointer, so we expose fixed p50/p99/p999 entry points that
// delegate here. This surfaces p50/p99/p999 tail latency in nanoseconds.
double percentile(const std::vector<double>& v, double p) {
    if (v.empty()) return 0.0;
    std::vector<double> s(v);
    std::sort(s.begin(), s.end());
    double pos = p * static_cast<double>(s.size() - 1);
    auto lo = static_cast<size_t>(pos);
    double frac = pos - static_cast<double>(lo);
    if (lo + 1 < s.size()) return s[lo] * (1.0 - frac) + s[lo + 1] * frac;
    return s[lo];
}

double p50(const std::vector<double>& v) { return percentile(v, 0.50); }
double p99(const std::vector<double>& v) { return percentile(v, 0.99); }
double p999(const std::vector<double>& v) { return percentile(v, 0.999); }

// Add a non-crossing limit order into an already-populated book: pure insert
// cost (map lookup/insert + deque push_back), no matching.
void BM_AddLimitOrder(benchmark::State& state) {
    OrderBook book;
    for (int i = 0; i < 10'000; ++i)
        book.processOrder(OrderCommand::newLimit(Side::BUY, 1'000 + i * 2, 10));
    for (int i = 0; i < 10'000; ++i)
        book.processOrder(OrderCommand::newLimit(Side::SELL, 2'000'000 + i * 2, 10));

    long price = 100'000;  // between bids and asks: always rests, never crosses
    for (auto _ : state) {
        (void)_;
        book.processOrder(OrderCommand::newLimit(Side::BUY, price++, 10));
    }
}

// Match an incoming market order against one resting order: the matching inner
// loop plus trade emission and level bookkeeping. Re-seeds (untimed) in bulk
// when the resting liquidity is exhausted.
void BM_MatchMarketOrder(benchmark::State& state) {
    OrderBook book;
    long remaining = 0;
    constexpr int kBatch = 100'000;
    auto seed = [&] {
        for (int i = 0; i < kBatch; ++i)
            book.processOrder(OrderCommand::newLimit(Side::SELL, 1'000, 10));
    };

    for (auto _ : state) {
        (void)_;
        if (remaining == 0) {
            state.PauseTiming();
            book.clearTradeLog();  // bound trade-log growth over the run
            seed();
            remaining = kBatch;
            state.ResumeTiming();
        }
        book.processOrder(OrderCommand::newMarket(Side::BUY, 10));
        --remaining;
    }
}

// Cancel a distinct resting order each time: id lookup + level removal.
void BM_CancelOrder(benchmark::State& state) {
    OrderBook book;
    std::vector<long> ids;
    size_t idx = 0;
    constexpr int kBatch = 100'000;
    auto seed = [&] {
        ids.clear();
        idx = 0;
        for (int i = 0; i < kBatch; ++i) {
            auto cmd = OrderCommand::newLimit(Side::BUY, 1'000 + i, 10);
            book.processOrder(cmd);
            ids.push_back(cmd.order()->orderId());
        }
    };

    for (auto _ : state) {
        (void)_;
        if (idx >= ids.size()) {
            state.PauseTiming();
            seed();
            state.ResumeTiming();
        }
        book.processOrder(OrderCommand::newCancel(ids[idx++]));
    }
}

// Report p50/p99/p999 (plus mean/median) in nanoseconds. Repetitions give the
// sample distribution the percentile statistics are computed over.
void configure(benchmark::internal::Benchmark* b) {
    b->Unit(benchmark::kNanosecond)
        ->Repetitions(100)
        ->MinTime(0.05)
        ->ComputeStatistics("p50", p50)
        ->ComputeStatistics("p99", p99)
        ->ComputeStatistics("p999", p999)
        ->DisplayAggregatesOnly(true);
}

}  // namespace

BENCHMARK(BM_AddLimitOrder)->Apply(configure);
BENCHMARK(BM_MatchMarketOrder)->Apply(configure);
BENCHMARK(BM_CancelOrder)->Apply(configure);

BENCHMARK_MAIN();
