# Matchbox — Repository Analysis (map of the existing matching engine)

Concrete map of what exists today, written to plan a networking/market-data-feed
layer on top of the existing C++20 limit order book. No design proposals here —
just precise facts: paths, signatures, and code.

---

## 1. Project structure, build system, tests & benchmarks

```
Matchbox/
  CMakeLists.txt                 # single top-level CMake project
  README.md                      # design doc, perf numbers, roadmap
  sample_orders.txt              # demo order stream for the CLI
  include/matchbox/              # public headers (the API surface)
    core/    Side, OrderType, TimeInForce, Order, OrderCommand, Trade, Clock
    book/    OrderBook, PriceLevel, TopOfBook, BookDepthEntry
    io/      CsvTradeLogger, BookSnapshotWriter
  src/                           # implementation, mirrors header tree
    core/Order.cpp
    book/PriceLevel.cpp
    book/OrderBook.cpp
    io/CsvTradeLogger.cpp
    io/BookSnapshotWriter.cpp
    cli/CliRepl.cpp              # -> matchbox_cli executable
  bench/
    OrderBookBenchmark.cpp       # -> matchbox_bench (Google Benchmark)
    ThroughputTest.cpp           # -> matchbox_throughput (standalone)
  test/
    OrderBookTest.cpp            # 21 tests
    OrderLifecycleTest.cpp       # 20 tests
    PersistenceTest.cpp          # 7 tests
  build/                         # existing out-of-tree build (Release), populated
```

### Build system — CMakeLists.txt (full file is 64 lines, key facts)

- `cmake_minimum_required(VERSION 3.16)`, `project(matchbox LANGUAGES CXX)`.
- **C++20** (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_EXTENSIONS OFF`), default
  `Release` build type if unset.
- One static library target `matchbox` built from the 5 `.cpp` files above;
  `target_include_directories(matchbox PUBLIC .../include)` and private
  `-Wall -Wextra`.
- **External deps are fetched at configure time via `FetchContent`**:
  - `google/benchmark` tag `v1.9.1` → `matchbox_bench`
  - `google/googletest` tag `v1.15.2` → `matchbox_tests`
- Executable targets:
  - `matchbox_bench` links `matchbox benchmark::benchmark`
  - `matchbox_throughput` links `matchbox`
  - `matchbox_cli` links `matchbox`
  - `matchbox_tests` links `matchbox GTest::gtest_main`
- Tests wired via `enable_testing()` + `gtest_discover_tests(matchbox_tests)`
  (48 tests discovered). Run with `ctest --test-dir build --output-on-failure`.
- No library beyond the STL, GoogleTest, and Google Benchmark is linked.
  **No Boost, no Asio, no sockets, no JSON/binary serialization lib anywhere.**

---

## 2. Core data structures

All in namespace `matchbox`. Prices/quantities/ids/timestamps are all `long`.

### `Order` — `include/matchbox/core/Order.hpp`

```cpp
class Order {
public:
    Order(Side side, long price, long quantity,
          TimeInForce timeInForce = TimeInForce::GTC, long expiryTimestamp = 0);
    Order(long orderId, Side side, long price, long quantity, long entryTime,
          TimeInForce timeInForce = TimeInForce::GTC, long expiryTimestamp = 0);

    long orderId() const;          long side() const;
    long price() const;            long quantity() const;
    long filledQuantity() const;   long entryTime() const;
    TimeInForce timeInForce() const; long expiryTimestamp() const;

    void reduceQuantity(long delta);   // shrink remaining, grow filled
    void setQuantity(long quantity);   // modify-in-place
    bool isFilled() const;             // quantity_ <= 0
private:
    static std::atomic<long> idGen_;   // auto id source (the ONLY atomic in codebase)
    long orderId_, price_, quantity_, filledQuantity_, entryTime_, expiryTimestamp_;
    Side side_;  TimeInForce timeInForce_;
};
```

- Two constructors: one auto-assigns the next id from a **static
  `std::atomic<long> idGen_`** (`fetch_add(1)+1`) and stamps `entryTime_` via
  `nowNanos()`; the other takes explicit id/entryTime (used by modify's
  cancel-replace to preserve the id).
- The auto-assigning ctor is implemented in `src/core/Order.cpp:9` by delegating
  to the explicit one. Note the order id counter is global/static — shared by all
  `OrderBook` instances in the process.

### `PriceLevel` — `include/matchbox/book/PriceLevel.hpp`

```cpp
class PriceLevel {
public:
    explicit PriceLevel(long price);
    long price() const;
    void add(const std::shared_ptr<Order>& order);        // push_back (newest)
    const std::shared_ptr<Order>& peek() const;           // front (oldest)
    void poll();                                          // pop_front
    bool remove(const std::shared_ptr<Order>& order);     // O(n) linear scan
    bool isEmpty() const;  int size() const;  long totalQuantity() const;
private:
    long price_;
    std::deque<std::shared_ptr<Order>> orders_;  // FIFO = time priority
};
```

- `remove` is a linear scan (`std::find` on the deque) — `src/book/PriceLevel.cpp:7`.
  README flags this as a known hot-path cost (candidate for an intrusive list).

### `OrderBook` — `include/matchbox/book/OrderBook.hpp`

The class header explicitly documents itself: *"Single-threaded limit order book
with strict price-time priority."* (line 26).

```cpp
struct PriceComparator {
    bool descending = false;
    bool operator()(long a, long b) const { return descending ? a > b : a < b; }
};

class OrderBook {
public:
    using Book = std::map<long, PriceLevel, PriceComparator>;

    OrderBook();                                       // bids_ desc, asks_ asc

    void processOrder(const OrderCommand& cmd);        // <<< the single entry point
    int  expireOrders(long currentTimeNanos);          // GTD sweep, returns count

    // Queries (all const)
    TopOfBook getTopOfBook() const;
    std::vector<BookDepthEntry> getBidDepth() const;
    std::vector<BookDepthEntry> getAskDepth() const;
    const std::vector<Trade>& getTradeLog() const;
    void clearTradeLog();
    int  orderCount() const;                           // == orderMap_.size()

private:
    void processLimit(const std::shared_ptr<Order>& taker);
    void processMarket(const std::shared_ptr<Order>& taker);
    void processCancel(long orderId);
    void processModify(long orderId, long newPrice, long newQuantity);
    long totalAskQuantity() const;  long totalBidQuantity() const;
    long totalAskQuantityUpTo(long price) const;
    long totalBidQuantityDownTo(long price) const;
    void matchAgainstBook(Order& taker, Book& book, bool isLimit);
    void matchAgainstLevel(Order& taker, PriceLevel& level);
    void addToBook(const std::shared_ptr<Order>& order);

    Book bids_;                                        // desc comparator
    Book asks_;                                        // asc comparator
    std::unordered_map<long, std::shared_ptr<Order>> orderMap_;  // id -> order
    std::vector<Trade> tradeLog_;                      // in-memory only
};
```

- **Red-black tree per side**: `std::map<long, PriceLevel, PriceComparator>`.
  Bids use `PriceComparator{true}` (descending → best bid is `begin()`), asks
  use `PriceComparator{false}` (ascending → best ask is `begin()`).
- **FIFO per level**: `std::deque<std::shared_ptr<Order>>` in each `PriceLevel`.
- **Cancel hash map**: `orderMap_` (`std::unordered_map<long, shared_ptr<Order>>`)
  gives O(1) id lookup. Each resting order's `shared_ptr` lives in BOTH a level's
  deque and this map (the same instance — see `addToBook`).

### `Trade` — `include/matchbox/core/Trade.hpp` (plain struct, in-memory only)

```cpp
struct Trade {
    long price;
    long quantity;
    long timestamp;      // steady_clock nanos (nowNanos()), stamped at fill time
    long makerOrderId;   // resting order id
    long takerOrderId;   // incoming order id
};
// no separate trade id — position in the log is its implicit identity
```

### `TopOfBook` / `BookDepthEntry` — snapshot value types

- `TopOfBook` (`include/matchbox/book/TopOfBook.hpp`): `long bestBid, bestAsk,
  bidQuantity, askQuantity` with sentinel-encoded empties
  (`min long` bid / `max long` ask), `hasBid()/hasAsk()/hasBoth()/spread()`.
- `BookDepthEntry` (`include/matchbox/book/BookDepthEntry.hpp`):
  `long price; long quantity; int orderCount;` — one rung of depth.

### Enums (all `enum class`, defined in separate headers)

- `Side { BUY, SELL }` — `core/Side.hpp`
- `OrderType { LIMIT, MARKET, CANCEL, MODIFY }` — `core/OrderType.hpp`
- `TimeInForce { GTC, IOC, FOK, GTD }` — `core/TimeInForce.hpp`
- `Clock` — `core/Clock.hpp`: `inline long nowNanos()` = steady_clock nanos.

---

## 3. Order entry point(s) — the most important finding

**There is exactly one API boundary for feeding the engine, and it is
`OrderBook::processOrder(const OrderCommand&)`, declared at
`include/matchbox/book/OrderBook.hpp:45`, defined at `src/book/OrderBook.cpp:13`.**

```cpp
void OrderBook::processOrder(const OrderCommand& cmd) {
    switch (cmd.type()) {
        case OrderType::LIMIT:   processLimit(cmd.order()); break;
        case OrderType::MARKET:  processMarket(cmd.order()); break;
        case OrderType::CANCEL:  processCancel(cmd.cancelOrderId()); break;
        case OrderType::MODIFY:  processModify(cmd.cancelOrderId(),
                                               cmd.modifyPrice(), cmd.modifyQuantity()); break;
    }
}
```

`OrderCommand` (`include/matchbox/core/OrderCommand.hpp`) is the *parsed
instruction handed to the book* — this is exactly the boundary an external order
source would produce into. Its static factory constructors are the de-facto
input vocabulary:

```cpp
OrderCommand::newLimit(Side side, long price, long quantity,
                       TimeInForce tif = GTC, long expiryTimestamp = 0);
OrderCommand::newMarket(Side side, long quantity,
                        TimeInForce tif = GTC, long expiryTimestamp = 0);
OrderCommand::newCancel(long orderId);
OrderCommand::newModify(long orderId, long newPrice, long newQuantity);
```

Internals: `OrderCommand` is a small value type holding `OrderType type_`,
`std::shared_ptr<Order> order_` (only set for LIMIT/MARKET; the factory
constructs the Order and hands ownership to the book on rest), plus
`long cancelOrderId_, modifyPrice_, modifyQuantity_`.

### Who calls `processOrder` today (i.e., every current "feed source")

1. **`src/cli/CliRepl.cpp`** — parses text lines (`LIMIT BUY 100 10`,
   `MARKET SELL 5`, `CANCEL <id>`, `MODIFY <id> <p> <q>`) from a file or stdin,
   builds an `OrderCommand`, calls `book_.processOrder(oc)`. The CLI owns its
   `OrderBook book_` as a member.
2. **`bench/OrderBookBenchmark.cpp`** — Google Benchmark cases construct
   `OrderCommand`s and call `processOrder` inside the timed loop.
3. **`bench/ThroughputTest.cpp`** — same, in a tight wall-clock loop.
4. **All 48 tests** — `book.processOrder(...)` with freshly built `OrderCommand`s.

There is **no** separate "feed", "session", or "market-data" abstraction and no
`interface`/abstract `IOrderSource`. An external order source would plug in by
constructing `OrderCommand` values and calling `processOrder` — the same call
every internal consumer uses.

### Outputs an external layer would read

- Trades: `const std::vector<Trade>& OrderBook::getTradeLog() const`
  (in-memory, unbounded unless `clearTradeLog()` is called — benchmarks call
  `clearTradeLog()` between batches to bound growth; there is no push-based
  trade callback/observer today).
- Snapshot/depth: `getTopOfBook()`, `getBidDepth()`, `getAskDepth()`.
- The trade log is only dumped to CSV manually via `CsvTradeLogger::appendTrade`
  by the caller (see PersistenceTest.cpp:117-132 for the pattern).

---

## 4. Threading model

**The engine is strictly single-threaded today. Not safe to call from multiple
threads as-is.**

Evidence (exhaustive):

- `OrderBook` contains `std::map`, `std::unordered_map`, `std::deque`,
  `std::vector` members with **no locks, no mutexes, no condition variables,
  no thread/async usage anywhere**. A grep for `mutex|atomic|thread|lock_guard
  |async|future|condition_variable|spinlock|memory_order` across all
  `.hpp/.cpp` files returns only:
  - `Order.hpp:3` `#include <atomic>`
  - `Order.hpp:43` `static std::atomic<long> idGen_;`
  - `Order.cpp:7` `std::atomic<long> Order::idGen_{0};`
  - the doc comment "Single-threaded limit order book" in `OrderBook.hpp:26`
- The `idGen_` atomic is the only shared mutable state that would be safe to
  touch concurrently; everything else (order book state, `tradeLog_`) is plain
  mutable state with no synchronization.
- **Consequence:** two threads calling `processOrder` on the same `OrderBook`
  is a data race. The README's roadmap explicitly proposes a *"lock-free
  ingress ring buffer (SPSC/MPSC) feeding the single matcher thread, decoupling
  parsing from matching"* — i.e., the intended concurrency model is a single
  matcher thread fed from a queue, not a multi-threaded book.

The intended split for a networked feed is already hinted at by the code shape:
parsing (network bytes → `OrderCommand`) is naturally producer-side, matching
(`processOrder`) is consumer-side.

---

## 5. Order / message representation

- **Everything is an in-memory C++ object.** Orders are heap-allocated
  `Order` instances held via `std::shared_ptr` (shared between the level deque
  and `orderMap_`). `OrderCommand` is a small value struct that *owns* the
  `shared_ptr<Order>` for LIMIT/MARKET or just ids for CANCEL/MODIFY.
- **Order fields:** `orderId_`, `side_`, `price_`, `quantity_`,
  `filledQuantity_`, `entryTime_` (steady-clock nanos at construction),
  `timeInForce_`, `expiryTimestamp_`. All `long` except the two enums.
  ~48 bytes of payload + shared_ptr control block + heap allocation per order.
- **There is NO wire format, no serialization, no schema, no binary/text
  protocol for orders.** The only text-ish encoding of the domain is:
  - the CLI's human input grammar (`LIMIT BUY <p> <q>` etc., parsed ad-hoc in
    `CliRepl.cpp:70-117` by tokenizing on whitespace — not a shared parser);
  - CSV **output** for trades and snapshots via `CsvTradeLogger` /
    `BookSnapshotWriter` (each row built with a single `ostream <<`, flushed per
    row). These are the closest thing to a serialization precedent, and they are
    output-only (trades/snapshots), never used to parse incoming orders.
- `Trade` similarly has no id and no serialization; its only representation is
  the in-memory struct and the CSV row `timestamp,price,quantity,makerOrderId,takerOrderId`.

---

## 6. How the benchmark harness drives load

### Google Benchmark latency harness — `bench/OrderBookBenchmark.cpp`

- Three cases: `BM_AddLimitOrder`, `BM_MatchMarketOrder`, `BM_CancelOrder`.
- Each constructs an `OrderBook` locally, seeds it (untimed, in a `PauseTiming`
  window), then inside the timed loop calls `book.processOrder(OrderCommand::newLimit(...))`
  / `newMarket(...)` / `newCancel(...)`. **All in-process function calls** — no
  vectors of pre-generated orders, no feed concept. Orders are synthesized
  on-the-fly from a running price counter.
- Percentile stats (p50/p99/p999) via a custom `ComputeStatistics` percentile
  function, `->Repetitions(100)`.

### Throughput harness — `bench/ThroughputTest.cpp` (the 2.1M orders/sec source)

- Single class `ThroughputTest` with `void run()` and `void issueOrders()`.
- `issueOrders()` (lines 69-92) is a **deterministic synthetic workload
  generator**, invoked in a pure wall-clock loop:

```cpp
void issueOrders() {
    long price = ++priceSeq_;
    switch (price % 10) {
        case 0..6:  // ~70%
            book_.processOrder(OrderCommand::newLimit(price % 2 ? SELL : BUY, price, 10));
            break;
        case 7..8:  // ~20%
            book_.processOrder(OrderCommand::newMarket(price % 2 ? SELL : BUY, 5));
            break;
        case 9:     // ~10%
            if (cancelIdx_ < cancelIds_.size())
                book_.processOrder(OrderCommand::newCancel(cancelIds_[cancelIdx_++]));
            break;
    }
}
```

- Flow: `seed()` (20k resting orders, capturing ids for later cancels) →
  1s warmup loop of `issueOrders()` → 3s measured loop → prints orders/sec and a
  limit/market/cancel breakdown. `priceSeq_` starts at 100'000; bids
  200k-240k and asks 300k-340k are pre-seeded, so generated orders live in the
  gap and mostly rest (occasionally crossing).
- **Key point for you:** the "feed of orders" today is this synthetic in-loop
  generator directly calling `processOrder`. There is **no pre-generated order
  vector and no intermediate feed/buffer**. To drive the same engine from a
  network socket you would replace the `issueOrders()` body with
  parse-from-buffer → `processOrder`, or layer a producer in front of the loop.
  Because the loop is tight and synchronous, any I/O would need to be async or
  batched to avoid stalling the matcher — but the *insertion point* (a single
  `processOrder(OrderCommand)` call) is unchanged.

---

## 7. Dependencies and build constraints

- **C++ standard:** C++20 (required). `-Wall -Wextra` on the library.
- **External libs linked today:** only
  - `benchmark::benchmark` (Google Benchmark, FetchContent, v1.9.1) → `matchbox_bench`
  - `GTest::gtest_main` (GoogleTest, FetchContent, v1.15.2) → `matchbox_tests`
  - Everything else is the C++ standard library.
- **Not present:** no Boost, no Asio, no standalone standalone networking or
  serialization library. Adding networking would either use raw POSIX sockets
  (no new deps) or require adding a new FetchContent dependency (Asio/Boost.ASIO,
  etc.). No pre-existing async framework.
- **Build constraints to respect:**
  - New sources must be added to `add_library(matchbox ...)` in CMakeLists.txt
    (currently 5 .cpp files) to be visible to all consumers; or a new
    library/target added for the networking layer.
  - All code lives in `namespace matchbox`, headers under `include/matchbox/`,
    impls under `src/`.
  - The engine is header-light: logic is compiled into `libmatchbox.a`; only
    `Clock.hpp` and `Trade.hpp` operator<< are header-inline.
  - Network access is required at *configure* time for FetchContent (already the
    case), so adding another FetchContent dep is consistent with the workflow.

---

## 8. Gaps relevant to adding networking (explicit confirmations)

| Gap | Status |
|---|---|
| Network layer | **Does not exist.** No socket code, no listeners, no I/O abstraction. Grep for `socket/recv/send/bind/listen/accept` → zero hits. |
| Message/wire format | **Does not exist.** No serialization for inbound orders. The only format precedent is CSV *output* (`CsvTradeLogger`, `BookSnapshotWriter`) and the ad-hoc CLI text grammar in `CliRepl.cpp` (not a reusable parser). |
| Client/session concept | **Does not exist.** No session, connection, or client-id notion anywhere. The engine has no notion of "who sent this order" — an `Order` has no client/account field. Trades reference only `makerOrderId`/`takerOrderId`. |
| Feed / order-source abstraction | **Does not exist.** All callers construct `OrderCommand` and call `OrderBook::processOrder` directly. The single clean insertion point is `processOrder`. |
| Push-based trade/state notifications | **Does not exist.** Trades accumulate in an unbounded in-memory `tradeLog_`; consumers poll `getTradeLog()`/`getTopOfBook()`. No callback/observer/subscription mechanism to push executions or market data outward. |
| Thread safety for external producers | **Not thread-safe.** Single-threaded engine, no locks. Safe to call `processOrder` from exactly one thread; concurrent calls race. README roadmap already plans an SPSC/MPSC ingress ring feeding one matcher thread. |
| Concurrency primitives already present | Only `std::atomic<long> Order::idGen_` (order-id allocation). Nothing else. |

### Other details a network/market-data layer will hit

- **Order identity:** ids are process-global from a static atomic counter
  (`Order::idGen_`), starting at 1. A networked system that wants client-supplied
  ids must use the explicit-id `Order` ctor — but note **nothing validates id
  uniqueness**; `orderMap_[id]` would silently overwrite on collision.
- **Timestamps:** `entryTime_` and `Trade::timestamp` come from
  `nowNanos()` (steady_clock, monotonic, not wall-clock). If a feed carries
  exchange timestamps, that would be a new input — nothing consumes external
  timestamps today.
- **Rejected/remainder visibility:** the engine silently drops what cannot fill
  (market remainder, IOC/FOK leftovers, cancels of unknown ids, FOK rejects).
  There is **no ack/reject path out of the engine** — a market-data/order
  management layer would need to synthesize confirms/rejects from its own
  knowledge of the submit + resulting trade log, since the book returns nothing.
- **Trade log growth:** unbounded in-memory `std::vector<Trade>`; callers must
  `clearTradeLog()` periodically (benchmarks do this between batches). A
  long-running networked server would need its own draining/streaming strategy.
- **File layout convention for new code:** headers `include/matchbox/<area>/*.hpp`,
  impls `src/<area>/*.cpp`, all in `namespace matchbox`, registered as a CMake
  target in CMakeLists.txt.
