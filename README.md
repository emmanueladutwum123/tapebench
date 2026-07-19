# tapebench

A high-performance, multi-threaded C++20 backtesting and strategy-simulation
engine. Built as the third project in a quant/HFT portfolio alongside a
low-latency limit order book + matching engine and a derivatives
pricing/risk engine — this one focuses on execution/simulation
infrastructure: template-based zero-overhead strategy dispatch, concurrent
backtesting across parameter grids, and high-throughput tick data I/O.

**Status: M1 of 8 — repo skeleton, build, CI, and a synthetic tick generator.**
The rest of the milestone plan (data pipeline, strategy interface, simulation
core, analytics, concurrency, benchmarking, final docs) is tracked as the
project progresses; this README will be replaced with a full architecture
writeup at the final milestone.

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
| `Tick` | `include/tapebench/tick.hpp` | The flat, trivially-copyable trade-print type every later milestone builds on — pinned for zero-copy tape I/O in M2. |
| `SyntheticTickGenerator` | `include/tapebench/synthetic_generator.hpp`, `src/synthetic_generator.cpp` | Deterministic, seeded synthetic trade tape: GBM price path, Poisson-process arrival times, order-flow-biased trade side. |
| Demo | `src/main.cpp` | Generates and prints 20 ticks from a fixed seed. |

8 unit tests cover: determinism (same seed → identical tape), non-determinism
across seeds, strictly increasing timestamps, price positivity, size bounds,
side variety, and a drift sanity check — all passing clean under
AddressSanitizer + UndefinedBehaviorSanitizer.
