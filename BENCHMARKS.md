# Benchmarks

Every number below is copy-pasted output from actually running the `bench/`
executables in this repository (`-DCMAKE_BUILD_TYPE=Release
-DTB_BUILD_BENCHMARKS=ON`, no sanitizers — ASan/UBSan/TSan overhead would
make throughput numbers meaningless, same reasoning as the sibling
`liquibook-x` repo's `BENCHMARKS.md`). Nothing here is hand-waved.

## Measurement environment caveats (read before comparing to other machines)

- Measured on this repo's dev machine, which Google Benchmark itself detects
  as a virtualized/sandboxed environment: it reports `8 X 24 MHz CPU`, which
  is not a real clock speed — it's a known artifact of running inside a VM
  without direct access to the host's frequency-scaling info. **Absolute
  timings are specific to this machine and load at measurement time; they
  are not representative of dedicated bare-metal hardware.** The *relative*
  comparisons below (CRTP vs. virtual, sequential vs. parallel) are still
  meaningful, since both sides of each comparison were measured back-to-back
  on the identical machine under identical load.
- Google Benchmark's `CPU` column measures only the *launching* thread's CPU
  time. For the parallel-grid benchmarks, that column under-reports (it
  doesn't sum the worker threads' CPU time) — the `Time` (wall-clock)
  column is the one that actually reflects parallel speedup.

## CRTP vs. virtual dispatch

The flagship comparison: two strategies doing **exactly the same trivial
computation**, differing only in dispatch mechanism. The virtual variant is
constructed through a factory function defined in a separate translation
unit (`bench/virtual_strategy.cpp`) specifically so the compiler cannot
devirtualize or inline it at the call site — a same-TU virtual call is
something the optimizer could see through, which would measure nothing.

```
Benchmark                   Time             CPU   Iterations UserCounters...
-----------------------------------------------------------------------------
BM_CrtpDispatch         0.289 ns        0.289 ns   2425452000 items_per_second=3.46034G/s
BM_VirtualDispatch      0.888 ns        0.888 ns    791139241 items_per_second=1.12629G/s
```

CRTP dispatch is **~3.1x faster** than virtual dispatch here. This is the
concrete payoff of the interface design in `include/tapebench/strategy.hpp`:
`on_bar()` resolves to a direct, inlinable call at compile time, with none of
virtual dispatch's indirect vtable lookup.

## Tick generation (`SyntheticTickGenerator`)

```
Benchmark                        Time             CPU   Iterations UserCounters...
----------------------------------------------------------------------------------
BM_GenerateTicks/1000        42820 ns        42806 ns        16227 items_per_second=23.361M/s
BM_GenerateTicks/10000      660311 ns       660186 ns         1045 items_per_second=15.1473M/s
BM_GenerateTicks/100000    7017040 ns      7014898 ns           98 items_per_second=14.2554M/s
```

~14-23M synthetic ticks/second. Throughput drops moderately as N grows —
expected, since each generated tick allocates into a growing `std::vector`
(amortized, but not free) and the RNG/GBM math per tick is the dominant
per-item cost regardless of batch size.

## Memory-mapped tape iteration (`MappedTape`)

```
Benchmark                              Time             CPU   Iterations UserCounters...
----------------------------------------------------------------------------------------
BM_MappedTapeIteration/1000        12988 ns        12985 ns        53784 items_per_second=77.0114M/s
BM_MappedTapeIteration/100000     121767 ns       121677 ns         5813 items_per_second=821.845M/s
```

This is the practical payoff of the zero-copy design (see M2's dedicated
allocation-counting test for the structural proof that this path performs 0
heap allocations): at 100K ticks, throughput reaches **~822M ticks/second**
for a full mmap-open + read-every-tick cycle — that's memory-bandwidth-bound
iteration over already-mapped memory, not allocation-bound.

## Bar aggregation (`aggregate_bars`)

```
Benchmark                        Time             CPU   Iterations UserCounters...
----------------------------------------------------------------------------------
BM_AggregateBars/1000         1174 ns         1174 ns       587160 items_per_second=852.001M/s
BM_AggregateBars/100000     113499 ns       113493 ns         6118 items_per_second=881.111M/s
```

~850-880M ticks/second, consistent across scale — a single linear pass with
O(1) work per tick, exactly as designed.

## Full simulation (`simulate()`)

```
Benchmark                                          Time             CPU   Iterations UserCounters...
----------------------------------------------------------------------------------------------------
BM_Simulate_MovingAverageCrossover/20000        2786 ns         2785 ns       252536 items_per_second=71.8132M/s
BM_Simulate_MovingAverageCrossover/200000      30402 ns        30398 ns        23052 items_per_second=65.7603M/s
BM_Simulate_MeanReversion/20000                 6852 ns         6850 ns       101513 items_per_second=29.197M/s
BM_Simulate_MeanReversion/200000               77008 ns        76978 ns         9040 items_per_second=25.9686M/s
```

(Item count here is bars processed, not ticks.) `MeanReversion` costs
roughly 2.5x more per bar than `MovingAverageCrossover` — expected, since its
`on_bar_impl` recomputes a full mean + variance over its rolling window every
bar (`O(window)`), while `MovingAverageCrossover` computes two bounded-window
averages (`O(fast + slow)`, and both windows here are smaller than
`MeanReversion`'s). Neither is a scalability concern at these throughputs
relative to realistic bar counts.

## Parallel parameter-grid backtesting

```
Benchmark                       Time             CPU   Iterations UserCounters...
---------------------------------------------------------------------------------
BM_Grid_Sequential/50    34515458 ns     34513300 ns           20 items_per_second=1.44872k/s
BM_Grid_Sequential/200  142302200 ns    142302200 ns            5 items_per_second=1.40546k/s
BM_Grid_Parallel/50       9167756 ns        81803 ns         1000 items_per_second=611.225k/s
BM_Grid_Parallel/200     36551287 ns       256770 ns          100 items_per_second=778.907k/s
```

(Remember: ignore the `CPU` column for the parallel rows — see the
measurement-environment caveat above. Compare `Time`, i.e. wall-clock.)

| Grid size | Sequential (wall) | Parallel (wall) | Speedup |
|---|---|---|---|
| 50 strategies  | 34.5 ms  | 9.17 ms  | **3.76x** |
| 200 strategies | 142.3 ms | 36.55 ms | **3.89x** |

`ThreadPool` was sized to `std::thread::hardware_concurrency()` (8 on this
machine). A ~3.8x wall-clock speedup on 8 logical cores is well short of
ideal 8x linear scaling — expected, since per-task overhead
(`std::packaged_task`/`std::future` allocation, mutex-guarded queue
contention) and this machine's virtualized/shared-core environment both eat
into it. The result that matters most isn't the exact speedup number, though
— it's that `M6`'s correctness tests already proved parallel and sequential
runs produce **byte-identical results**, so this speedup is free: real
concurrency, not a race that happens to usually work.
