# tapebench — Design Decisions

`README.md` covers what each milestone built and how it's verified. This document
covers *why* — organized by decision, not by milestone. Performance numbers backing
several of these decisions live in [`BENCHMARKS.md`](BENCHMARKS.md).

## Table of contents

- [Why a synthetic tick generator, not real market data](#why-a-synthetic-tick-generator-not-real-market-data)
- [The tape format: a raw struct array, not a serialization library](#the-tape-format-a-raw-struct-array-not-a-serialization-library)
- [The strategy interface: CRTP, not virtual dispatch](#the-strategy-interface-crtp-not-virtual-dispatch)
- [No-look-ahead: enforced by the loop's structure, not a convention](#no-look-ahead-enforced-by-the-loops-structure-not-a-convention)
- [Position accounting: average-cost, and what a "reversal" fill means](#position-accounting-average-cost-and-what-a-reversal-fill-means)
- [Sharpe/Sortino on absolute PnL, not percentage returns](#sharpesortino-on-absolute-pnl-not-percentage-returns)
- [The thread pool: built from scratch, not `std::async`](#the-thread-pool-built-from-scratch-not-stdasync)
- [Bugs the process caught](#bugs-the-process-caught)

## Why a synthetic tick generator, not real market data

Real tick-level market data isn't reliably licensable or scriptable for a *public* CI
pipeline — a repo that fetches paid vendor data on every CI run either breaks for
anyone without a subscription, or has to smuggle credentials into a public workflow,
neither of which is acceptable. A GBM price path with Poisson-process arrival times
isn't a novel approximation; it's a standard, well-understood synthetic model, chosen
specifically *because* its statistical properties are known in closed form — tests can
assert real things ("price stays positive," "positive drift pushes the endpoint above
the start," "population z-score is bounded by `sqrt(window-1)`") without needing real
market data as ground truth. This mirrors the same reasoning the sibling `liquibook-x`
repo uses for its synthetic ITCH feed, and `quant-risk-engine` uses when it validates
its Monte Carlo pricer against Black-Scholes closed-form references: synthetic-but-
principled beats either skipping the test or silently mocking in a way that hides what
is actually being verified.

## The tape format: a raw struct array, not a serialization library

Considered: a schema-based serialization format (protobuf-style, or a "zero-copy"
framework like flatbuffers). Rejected: the entire point of the tape format is a
memory-mapped, zero-copy read path — any serialization framework, even a
zero-copy-flavored one, adds either an indirection layer or non-trivial format
complexity for a problem this small. `Tick` is already POD
(`static_assert(std::is_trivially_copyable_v<Tick>)` — a real compile-time check, not
a comment). A small fixed header (`TapeHeader`: magic, version, tick count) followed
by a contiguous array of that exact struct *is* the whole format, and it's provably
zero-copy because the on-disk layout literally **is** the in-memory layout — reading a
tape is `mmap()` plus a `reinterpret_cast`, nothing else. That claim isn't just
asserted: a dedicated test overrides global `operator new`/`delete` and asserts 0
allocations while reading 5,000 ticks through the returned `std::span`.

## The strategy interface: CRTP, not virtual dispatch

Considered: a conventional abstract-base-class-plus-virtual-method interface (the
obvious, more common choice for a pluggable strategy system). Chosen instead: CRTP
(`Strategy<Derived>`), which resolves `on_bar()`/`name()` to a direct, inlinable call
at compile time — no vtable, no indirect call. This isn't a stylistic preference
stated without evidence: `bench/bench_dispatch.cpp` measures the two dispatch
mechanisms doing **identical trivial work**, with the virtual variant constructed
through a factory in a separate translation unit specifically so the compiler can't
devirtualize or inline it away. Real result: CRTP dispatch is **~3.1x faster** (0.289ns
vs. 0.888ns per call — see [`BENCHMARKS.md`](BENCHMARKS.md)).

The tradeoff CRTP makes: strategies can no longer be stored polymorphically in a
single `std::vector<StrategyBase*>` at runtime — every strategy type needs to be known
at compile time (which is exactly why `run_grid_parallel`/`run_grid_sequential` are
themselves templates over `StrategyT`, not functions taking a runtime-polymorphic
base). For a backtesting engine where the set of strategies under test is a
compile-time decision anyway (you write the strategy code, then run it), that's the
right tradeoff. It would be the *wrong* one for a system that needs to load strategies
as runtime plugins.

A `StrategyImpl` C++20 concept enforces the interface contract, not just duck typing —
a strategy missing `on_bar_impl`/`name_impl` fails to compile with a specific,
readable `static_assert` message, rather than a wall of raw template-instantiation
errors from deep inside `Strategy<Derived>`'s internals.

## No-look-ahead: enforced by the loop's structure, not a convention

The single most common backtesting correctness bug: computing a signal from bar N's
close, then filling that signal *on bar N itself* — using information (that bar's own
close) that wasn't actually available at the moment a real order would have been
placed. The fix here isn't a code comment telling future contributors not to do that;
it's the literal control flow of `simulate()`'s loop. Each iteration executes the
*previous* iteration's queued signal (at the *current* bar's open) **before**
computing and queuing the current bar's own signal. There is no code path through
which a bar's own close can influence a fill on that same bar — the ordering makes it
structurally impossible, not merely discouraged.

## Position accounting: average-cost, and what a "reversal" fill means

Considered: FIFO/LIFO tax-lot accounting (tracking each individual fill as a separate
lot, closed out in a specific order). Chosen instead: average-cost accounting (one
running `avg_entry_price` per position) — this isn't a tax-accurate system, and
average-cost is both simpler to reason about and sufficient for P&L/Sharpe/drawdown
purposes. The one genuinely tricky case: a fill *larger* than the current position,
in the opposite direction — e.g., long 10, then a sell of 15. That's not just "close
the position," it's "close the existing 10, then open a new short 5 at the fill
price." `Position::apply_fill` computes `closing_quantity = min(|fill|, |position|)`
to realize P&L on exactly the overlapping portion, then — only if the fill was larger
— resets `avg_entry_price` to the fill price for the new, opposite-direction
remainder. All nine `Position` tests are hand-computed, including this exact reversal
case and its short-side mirror.

## Sharpe/Sortino on absolute PnL, not percentage returns

`analyze()` computes Sharpe and Sortino on bar-over-bar **absolute PnL changes**, not
percentage returns. This is a real, deliberate simplification, stated explicitly here
(and in the header comment and README) rather than hidden: this engine has no notional
capital or leverage model, so there is no principled "what percentage did the account
grow by" to compute — inventing an arbitrary capital base just to produce a percentage
would imply a precision the model doesn't actually have. Reporting "Sharpe of PnL" is
an established, if less common, alternative when a system genuinely doesn't model
capital, and it's the honest choice here rather than a fabricated-precision one.

## The thread pool: built from scratch, not `std::async`

Considered: `std::async(std::launch::async, ...)` per grid-search task — the simplest
possible way to get "some concurrency." Rejected: each `std::async` call typically
spawns a new OS thread (implementation-defined, but commonly true), which is real,
avoidable overhead when running dozens-to-hundreds of independent backtests. A real
thread pool (`ThreadPool`: mutex + condition-variable task queue, persistent worker
threads, `std::packaged_task`/`std::future` for per-task results including propagated
exceptions) amortizes thread creation across every task submitted to the pool's
lifetime, not just one. It also demonstrates the actual queue/synchronization
mechanics directly rather than hiding them behind a standard-library primitive —
correctness here is verified two ways, not one: `run_grid_parallel` vs.
`run_grid_sequential` produce byte-identical results (a data-race would show up as a
*correctness* failure, not just an occasional flake), and the full suite runs clean
under ThreadSanitizer in a dedicated CI job.

## Bugs the process caught

The concrete evidence behind "every correctness claim is backed by a test that would
actually fail if it were wrong" — collected here rather than left scattered across
eight milestones' worth of commit messages. This project surfaced four real issues;
unlike some larger sibling repos in this portfolio, that's the honest count — not
padded to look more dramatic than it was.

| # | Where | What broke | How it was caught | Fix |
|---|---|---|---|---|
| 1 | M2 | The zero-copy allocation-counting test's `operator new`/`delete` overrides were missing the `nothrow` variants; libstdc++'s internal `std::stable_sort` (used by GoogleTest to order test cases) allocates via `operator new(size, nothrow)`, which fell through to ASan's own default nothrow-new instead of this file's `malloc`-backed one, then got freed via the overridden sized `operator delete` | ASan's `alloc-dealloc-mismatch` detector, on the `gcc / Debug` CI leg only — never reproduced locally on macOS (libc++'s `stable_sort` doesn't use this allocation path) | Added the missing nothrow `operator new`/`operator new[]`/`operator delete`/`operator delete[]` overloads |
| 2 | M3 (test design, before first run) | `MeanReversion`'s test thresholds were first drafted at/above a value that's mathematically **unreachable**: a *population* z-score computed from a window that includes the outlier itself is capped at `sqrt(window-1)` (exactly 2.0 for `window=5`), so an `entry_z_score` at or above that ceiling could never fire, regardless of how extreme the test's outlier was | Caught by working through the math by hand before ever running the test, not by a test failure — a borderline/always-false assertion wouldn't have failed loudly, just silently tested nothing | Set `entry_z_score` comfortably below the ceiling (1.0-1.5 for `window=5`) in every threshold-crossing test |
| 3 | M6 | `main.cpp` and a test file both used `for (std::size_t x : {3, 5, 8})` — the braced-init-list's `int` literals implicitly narrow to `std::size_t`, which `-Wsign-conversion` (part of this project's warnings-as-errors set) flags | Local build failure before any push — caught by building with the exact production warning flags, not by CI | Explicit `u` suffixes on the literals (`{3u, 5u, 8u}`) |
| 4 | M6 | ThreadSanitizer's own runtime (`libclang_rt.tsan_cxx`) ships its own global `operator new`/`delete` overrides for its own allocation tracking — linking the M2 zero-copy test's overrides alongside them under `-fsanitize=thread` is a genuine link-time "multiple definition" error, not a fixable conflict | The brand-new `clang / TSan` CI job's first-ever run failed to link (the other 4 build-and-test legs all passed on the same push) | Excluded that one executable entirely under `TB_ENABLE_TSAN` in `tests/CMakeLists.txt` — it doesn't test concurrency at all, so no real coverage is lost |

Two of these four (#1, #4) were **only ever catchable by CI** — a Linux/gcc-specific
standard-library allocation path and a Linux-only sanitizer-runtime symbol collision,
both invisible on the macOS development machine this project was built on. That's the
concrete argument for why this project's CI matrix (gcc **and** clang, sanitizers
**and** a dedicated TSan job) is a real correctness practice here, not a checkbox.
