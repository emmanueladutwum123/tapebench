# tapebench

A high-performance, multi-threaded C++20 backtesting and strategy-simulation
engine. Built as the third project in a quant/HFT portfolio alongside a
low-latency limit order book + matching engine and a derivatives
pricing/risk engine — this one focuses on execution/simulation
infrastructure: template-based zero-overhead strategy dispatch, concurrent
backtesting across parameter grids, and high-throughput tick data I/O.

**Status: M3 of 8 — repo skeleton/build/CI/synthetic generator (M1), a
zero-copy tick/bar data pipeline (M2), and a CRTP strategy interface with two
example strategies (M3).** The rest of the milestone plan (simulation core,
analytics, concurrency, benchmarking, final docs) is tracked as the project
progresses; this README will be replaced with a full architecture writeup at
the final milestone.

## Why a synthetic tick generator, not real market data?

Real tick-level market data isn't reliably licensable or scriptable for a
public CI pipeline. Every milestone in this repo is built and tested against
a seeded, deterministic synthetic tape instead — a GBM-style price path with
Poisson-process inter-arrival times — so the test suite has no external data
dependency and is fully reproducible. This mirrors the same honest approach
taken in the sibling `liquibook-x` (synthetic ITCH-style feed) and
`quant-risk-engine` (statistical validation against closed-form references)
repos in this portfolio.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DTB_ENABLE_ASAN=ON -DTB_ENABLE_UBSAN=ON
cmake --build build -j
./build/tapebench_demo
ctest --test-dir build --output-on-failure
```

## What's here so far

| Component | File(s) | What it does |
|---|---|---|
| `Tick` | `include/tapebench/tick.hpp` | The flat, trivially-copyable trade-print type every later milestone builds on. |
| `SyntheticTickGenerator` | `include/tapebench/synthetic_generator.hpp`, `src/synthetic_generator.cpp` | Deterministic, seeded synthetic trade tape: GBM price path, Poisson-process arrival times, order-flow-biased trade side. |
| Tape format | `include/tapebench/tape_format.hpp` | The on-disk layout: a small header immediately followed by contiguous `Tick` records — no compression or variable-length encoding, so it can be memory-mapped and reinterpreted directly. |
| `write_tape` | `include/tapebench/tape_writer.hpp`, `src/tape_writer.cpp` | Serializes a tick tape to disk in that format. |
| `MappedTape` | `include/tapebench/mapped_tape.hpp`, `src/mapped_tape.cpp` | Memory-maps a tape file and exposes its ticks as a `std::span<const Tick>` pointing directly into the mapping — **zero heap copy on the read path, verified (not assumed)** by a dedicated test that overrides global `operator new` and asserts 0 allocations while reading 5,000 ticks. Move-only; throws on a missing/truncated/malformed file. |
| `Bar` / `aggregate_bars` | `include/tapebench/bar.hpp`, `include/tapebench/bar_aggregator.hpp`, `src/bar_aggregator.cpp` | Aggregates a tick tape into fixed-duration OHLCV bars, single pass, skipping empty buckets. |
| `Strategy<Derived>` | `include/tapebench/strategy.hpp` | CRTP base for **compile-time (zero-overhead) strategy dispatch** — `on_bar()`/`name()` resolve to a direct, inlinable call to `Derived`'s implementation, no vtable, no indirect call. Enforced by a C++20 concept (`StrategyImpl`), not just duck-typing, so a strategy that gets the interface wrong fails to compile with a specific message. `run_over_bars()` is a generic driver that works identically for any conforming strategy. |
| `MovingAverageCrossover` | `include/tapebench/strategies/moving_average_crossover.hpp` + `.cpp` | Example trend-following strategy: long when the fast close average is above the slow one, short otherwise. |
| `MeanReversion` | `include/tapebench/strategies/mean_reversion.hpp` + `.cpp` | Example mean-reverting strategy: rolling z-score of closes, long/short on large deviations from the rolling mean. |
| Demo | `src/main.cpp` | Full pipeline: generate 20,000 ticks → write tape → memory-map it back → aggregate into 200ms bars → run both example strategies over the same bars, side by side. |

39 unit tests across 3 test executables, all passing clean under
AddressSanitizer + UndefinedBehaviorSanitizer:
- **`tapebench_tests`**: generator determinism/bounds/sanity (8), `MappedTape`
  round-trip + move semantics + every documented error case — nonexistent
  file, too-small file, bad magic, unsupported version, truncated tick data
  (9), `BarAggregator` bucket boundaries, empty-bucket skipping, OHLCV
  correctness, and a total-trade-count invariant across all bars (7), the CRTP
  dispatch mechanics proven generic across two differently-shaped strategies
  (4), and both example strategies' warm-up/threshold/edge-case behavior (7).
- **`tapebench_zero_copy_tests`**: the dedicated allocation-counting proof (1).
