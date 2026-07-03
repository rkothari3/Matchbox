#include <cstdio>
#include <vector>

#include "matchbox/book/OrderBook.hpp"
#include "matchbox/core/Clock.hpp"

using namespace matchbox;

// Standalone throughput stress test.
//
// Pre-seeds the book with 20k orders, then fires a mixed workload (limits,
// markets, cancels) in a tight loop for a fixed duration and reports aggregate
// orders/sec plus a breakdown by operation type. Unlike the latency
// microbenchmarks this measures sustained throughput, so a plain steady_clock
// wall-time loop is the right tool (no per-op framework overhead).
namespace {

constexpr long kWarmupNanos = 1'000'000'000L;   // 1s
constexpr long kTestNanos = 3'000'000'000L;     // 3s

class ThroughputTest {
public:
    void run() {
        seed();

        // Warmup: let the allocator/caches settle before measuring.
        long warmupEnd = nowNanos() + kWarmupNanos;
        while (nowNanos() < warmupEnd) issueOrders();

        // Snapshot counters so the breakdown reflects only the measured window.
        long start = nowNanos();
        long end = start + kTestNanos;
        long opsStart = totalOrders_;
        long limitStart = limitCount_, marketStart = marketCount_, cancelStart = cancelCount_;
        while (nowNanos() < end) issueOrders();
        long elapsed = nowNanos() - start;

        double elapsedSec = elapsed / 1'000'000'000.0;
        long total = totalOrders_ - opsStart;
        long limits = limitCount_ - limitStart;
        long markets = marketCount_ - marketStart;
        long cancels = cancelCount_ - cancelStart;

        std::printf("--- ThroughputTest Results ---\n");
        std::printf("Test duration:      %.2f sec\n", elapsedSec);
        std::printf("Total orders:       %ld\n", total);
        std::printf("Throughput:         %.0f orders/sec\n", total / elapsedSec);
        std::printf("  Limit orders:     %ld (%.1f%%)\n", limits,
                    100.0 * static_cast<double>(limits) / static_cast<double>(total));
        std::printf("  Market orders:    %ld (%.1f%%)\n", markets,
                    100.0 * static_cast<double>(markets) / static_cast<double>(total));
        std::printf("  Cancels:          %ld (%.1f%%)\n", cancels,
                    100.0 * static_cast<double>(cancels) / static_cast<double>(total));
        std::printf("Book depth:         %d orders, %zu bid levels, %zu ask levels\n",
                    book_.orderCount(), book_.getBidDepth().size(), book_.getAskDepth().size());
    }

private:
    void seed() {
        for (int i = 0; i < 10'000; ++i) {
            auto cmd = OrderCommand::newLimit(Side::BUY, 200'000 + i * 2, 10);
            book_.processOrder(cmd);
            cancelIds_.push_back(cmd.order()->orderId());
        }
        for (int i = 0; i < 10'000; ++i)
            book_.processOrder(OrderCommand::newLimit(Side::SELL, 300'000 + i * 2, 10));
    }

    void issueOrders() {
        long price = ++priceSeq_;
        switch (price % 10) {
            case 0: case 1: case 2: case 3: case 4: case 5: case 6:
                book_.processOrder(OrderCommand::newLimit(
                    price % 2 == 0 ? Side::BUY : Side::SELL, price, 10));
                ++limitCount_;
                ++totalOrders_;
                break;
            case 7: case 8:
                book_.processOrder(OrderCommand::newMarket(
                    price % 2 == 0 ? Side::BUY : Side::SELL, 5));
                ++marketCount_;
                ++totalOrders_;
                break;
            case 9:
                if (cancelIdx_ < cancelIds_.size()) {
                    book_.processOrder(OrderCommand::newCancel(cancelIds_[cancelIdx_++]));
                    ++cancelCount_;
                    ++totalOrders_;
                }
                break;
        }
    }

    OrderBook book_;
    std::vector<long> cancelIds_;
    size_t cancelIdx_ = 0;
    long priceSeq_ = 100'000;

    long limitCount_ = 0;
    long marketCount_ = 0;
    long cancelCount_ = 0;
    long totalOrders_ = 0;
};

}  // namespace

int main() {
    ThroughputTest().run();
    return 0;
}
