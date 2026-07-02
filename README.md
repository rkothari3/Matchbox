Build a low-latency limit order book (LOB) matching engine in Java. This is a 
learning project — explain key design decisions as you implement them, don't 
just generate code silently.

CONTEXT: This mirrors the exchange-side matching logic that sits behind 
simulated markets like IMC Prosperity, which every competitor trades against 
as a black box. I'm building that black box.

CORE REQUIREMENTS:
1. Order types: Limit, Market, Cancel, Modify (cancel-replace)
2. Matching: strict price-time priority (FIFO within price level)
3. Data structure: TreeMap<Price, FIFO queue of orders> per side (bid/ask), 
   justify this choice vs alternatives (skip list, array-based) in comments
4. Order book output: live top-of-book (best bid/ask) + full depth snapshot 
   + trade log (price, size, timestamp, maker/taker order IDs)
5. Input: order stream from a file or generator (support both a replay mode 
   from a log file and a synthetic order generator for stress testing)

ARCHITECTURE (in stages, build incrementally, test each stage):
Stage 1: Core order book — add/cancel/match, single-threaded, correctness first
Stage 2: Order lifecycle — partial fills, modify-in-place, order expiry
Stage 3: Persistence — trade log + book snapshots to disk (simple binary or 
  CSV format, your call — justify it)
Stage 4: Performance — JMH microbenchmarks on add/match/cancel latency 
  (target: report p50/p99/p999 in nanoseconds); throughput stress test 
  simulating 100k orders/sec, report actual sustained throughput
Stage 5 (stretch): simple CLI or minimal web view of live book depth

CONSTRAINTS:
- Pure Java, no external matching libraries — the matching logic is the point
- JMH for benchmarking (set this up properly, not System.nanoTime() hacks)
- Unit tests for matching correctness BEFORE optimizing for speed 
  (price-time priority edge cases: same price different times, partial 
  fills against multiple resting orders, self-trade prevention if in scope)
- Clean package structure: separate order book logic, order types, matching 
  engine, and I/O/benchmark harness

DELIVERABLE: working repo with README explaining the design (data structure 
choice, latency numbers, throughput numbers, what you'd optimize next with 
more time — e.g. lock-free structures, ring buffers for the input queue, 
off-heap storage).

Steps to development.
- Each stage a new branch. 
- Once done with a branch (thus stage), push, then merge it to main.
- You are a ponytail engineer, keep your code clean.
- Always use subagents
- Whenever stuck use websearch if needed or ask me for help. 
- Keep your commit messages restricted to only 1 line. 

Context: Project inspired by IMC prosperity. 