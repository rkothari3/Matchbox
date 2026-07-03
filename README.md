# Matchbox — a low-latency limit order book matching engine (C++)

A single-threaded limit order book (LOB) matching engine with strict
price-time priority, written in modern C++20. It models the exchange-side
matching logic that sits behind simulated markets like **IMC Prosperity** —
the black box every competitor trades against. This project *is* that black box.

> Built for tight control over memory layout and latency. The build is CMake;
> tests use GoogleTest and benchmarks use Google Benchmark.

## Features

- **Order types:** Limit, Market, Cancel, Modify (cancel-replace)
- **Matching:** strict price-time priority — FIFO within each price level
- **Time-in-force:** GTC, IOC (immediate-or-cancel), FOK (fill-or-kill),
  GTD (good-till-date, with explicit expiry sweep)
- **Outputs:** live top-of-book, full-depth snapshot, and a trade log
  (price, size, timestamp, maker/taker order ids)
- **I/O:** CSV trade log + CSV book snapshots; an interactive CLI that replays
  an order file or reads stdin
- **Performance harness:** Google Benchmark micro-benchmarks (p50/p99/p999
  latency) and a standalone throughput stress test

## Layout

```
include/matchbox/        public headers
  core/   Side, OrderType, TimeInForce, Order, OrderCommand, Trade, Clock
  book/   OrderBook, PriceLevel, TopOfBook, BookDepthEntry
  io/     CsvTradeLogger, BookSnapshotWriter
src/      implementation (.cpp) mirroring the header tree
  cli/    CliRepl.cpp        interactive REPL (matchbox_cli)
bench/    OrderBookBenchmark.cpp   latency micro-benchmarks (matchbox_bench)
          ThroughputTest.cpp       throughput stress test (matchbox_throughput)
test/     OrderBookTest, OrderLifecycleTest, PersistenceTest  (matchbox_tests)
```

## Build & run

Requires a C++20 compiler and CMake ≥ 3.16. GoogleTest and Google Benchmark are
fetched automatically at configure time (network access needed on first run).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

ctest --test-dir build --output-on-failure   # 48 correctness tests
./build/matchbox_cli sample_orders.txt        # replay an order stream
./build/matchbox_throughput                   # sustained orders/sec
./build/matchbox_bench                         # p50/p99/p999 latency
```

## Design

### Data structure — why an ordered map per side

Each side of the book is a `std::map<long, PriceLevel>` (a red-black tree)
keyed by price. Bids use a descending comparator and asks an ascending one, so
the **best price is always `begin()`** and sweeping walks levels in price order.
A single `PriceComparator` functor with a `descending` flag lets both sides
share one map type, differing only in comparator direction.

Within a price level, orders sit in a `std::deque<std::shared_ptr<Order>>` FIFO,
which enforces time priority: new orders `push_back`, the matcher takes from the
front. Orders are shared (`shared_ptr`) between the level's queue and an
`unordered_map<id, Order>` used for O(1) cancel/modify lookup, so one object
lives in both the deque and the hash map.

Alternatives considered:

| Option | Best price | Insert / erase mid-book | Verdict |
|---|---|---|---|
| **Ordered map (RB-tree)** | O(log N) via `begin()` | O(log N) | **chosen** — balanced, ordered iteration for sweeps |
| Skip list | O(log N) | O(log N) | similar big-O, no `std` type, no cache win at these sizes |
| Flat sorted array | O(1) | O(N) shift | insert/erase dominates once many levels are live |
| Hash map on price | — | O(1) | loses ordering; can't find best or sweep without a full scan |

### Matching

`matchAgainstBook` pulls the best level (`begin()`), stops at the taker's price
limit (for limit orders), and delegates to `matchAgainstLevel`, which crosses
the incoming order against resting orders front-to-back, emitting a `Trade` per
fill and popping fully-filled makers. Market orders skip the price check and
sweep freely; any unfilled remainder is dropped (they never rest). IOC/FOK never
rest either; FOK pre-checks total crossable depth and rejects atomically if it
can't fill in full. Modify at the same price resizes in place (keeps time
priority); modify to a new price is a cancel-replace (loses it).

### Persistence — CSV over binary

Both the trade log and snapshot writer emit CSV. For a learning-stage engine,
human-readability and trivial tooling (awk / pandas / a spreadsheet) outweigh
the compactness of a binary format; the serialization is a single `<<` per row
with no schema coupling.

## Numbers

Measured on an 8-core WSL2 box (2995 MHz, L3 24 MiB), Release build.
Latency percentiles are computed by Google Benchmark over 100 repetitions.

| Operation | p50 | p99 | p999 |
|---|---|---|---|
| Add limit (into a 20k-order book) | ~295 ns | ~376 ns | ~566 ns |
| Match market (1 fill + level pop) | ~88 ns | ~106 ns | ~109 ns |
| Cancel (id lookup + level removal) | ~267 ns | ~345 ns | ~348 ns |

**Throughput:** ~3.2M orders/sec sustained on a mixed workload (≈78% limits,
≈22% markets) over a 3-second measured window after a 1-second warmup.

> Methodology note: these percentiles are over per-repetition aggregates, so
> they characterize typical and near-tail latency rather than the extreme tail
> of individual operations. Absolute numbers vary with machine and load.

## What I'd optimize next

- **O(1) cancel** via an intrusive doubly-linked list per level, with the id map
  storing a node handle — today an interior cancel is a linear scan of the deque.
- **Arena / object-pool allocation** for `Order` and level nodes to kill
  per-order `shared_ptr` control-block allocations and improve cache locality;
  a slab of value-type orders referenced by index removes the `shared_ptr`
  overhead on the hot path entirely.
- **Cache-friendly price ladder** — a flat array indexed by (price − floor) for
  dense tick ranges, falling back to the tree for sparse extremes.
- **Lock-free ingress ring buffer** (SPSC/MPSC) feeding the single matcher
  thread, decoupling parsing from matching.
- **Off-heap / memory-mapped** trade and snapshot logs to remove I/O from the
  critical path.

## Development

Built in five staged branches (`cpp-stage-1..5`), each merged to `main`:
core book → order lifecycle (TIF, modify, expiry) → persistence → performance
harness → CLI. Project inspired by IMC Prosperity.
