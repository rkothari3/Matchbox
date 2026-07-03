#pragma once

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "matchbox/book/BookDepthEntry.hpp"
#include "matchbox/book/PriceLevel.hpp"
#include "matchbox/book/TopOfBook.hpp"
#include "matchbox/core/Order.hpp"
#include "matchbox/core/OrderCommand.hpp"
#include "matchbox/core/Trade.hpp"

namespace matchbox {

// Orders the price ladder so the best price is always begin(): bids descend
// (highest first), asks ascend (lowest first). A single comparator type (with
// a direction flag) lets both sides share one map type, differing only in
// comparator direction.
struct PriceComparator {
    bool descending = false;
    bool operator()(long a, long b) const { return descending ? a > b : a < b; }
};

// Single-threaded limit order book with strict price-time priority.
//
// Data structure choice (vs alternatives):
//   std::map (red-black tree) per side gives O(log N) insert/erase/lookup by
//   price and, crucially, ordered iteration so the best level is always
//   begin() and sweeping walks levels in price order. Alternatives:
//     - skip list: similar big-O, but no std implementation and no cache win
//       over a balanced tree at these sizes;
//     - flat sorted array: O(1) best but O(N) insert/erase in the middle,
//       which dominates once the book has many live levels;
//     - hash map on price: O(1) point access but loses the ordering we need
//       to find best price and sweep, forcing a full scan.
//   Within a level, a std::deque FIFO enforces time priority.
class OrderBook {
public:
    using Book = std::map<long, PriceLevel, PriceComparator>;

    OrderBook();

    void processOrder(const OrderCommand& cmd);

    // Cancel every resting GTD order whose expiry is at or before `currentTimeNanos`.
    // Returns how many were expired.
    int expireOrders(long currentTimeNanos);

    // --- Queries ---
    TopOfBook getTopOfBook() const;
    std::vector<BookDepthEntry> getBidDepth() const;
    std::vector<BookDepthEntry> getAskDepth() const;
    const std::vector<Trade>& getTradeLog() const { return tradeLog_; }
    void clearTradeLog() { tradeLog_.clear(); }
    int orderCount() const { return static_cast<int>(orderMap_.size()); }

private:
    void processLimit(const std::shared_ptr<Order>& taker);
    void processMarket(const std::shared_ptr<Order>& taker);
    void processCancel(long orderId);
    void processModify(long orderId, long newPrice, long newQuantity);

    // Liquidity available to a taker, used for FOK all-or-nothing pre-checks.
    long totalAskQuantity() const;
    long totalBidQuantity() const;
    long totalAskQuantityUpTo(long price) const;   // asks at or below a buy limit
    long totalBidQuantityDownTo(long price) const;  // bids at or above a sell limit

    // Cross `taker` against `book` (asks for a buy, bids for a sell), stopping
    // at the price limit when isLimit is true; false = market (sweep freely).
    void matchAgainstBook(Order& taker, Book& book, bool isLimit);
    void matchAgainstLevel(Order& taker, PriceLevel& level);
    void addToBook(const std::shared_ptr<Order>& order);

    Book bids_;
    Book asks_;
    std::unordered_map<long, std::shared_ptr<Order>> orderMap_;
    std::vector<Trade> tradeLog_;
};

}
