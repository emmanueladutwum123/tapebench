#include <benchmark/benchmark.h>

#include "tapebench/bar_aggregator.hpp"
#include "tapebench/synthetic_generator.hpp"

using namespace tapebench;

void BM_AggregateBars(benchmark::State& state) {
  SyntheticTickParams params;
  SyntheticTickGenerator gen(42, params);
  auto ticks = gen.generate(static_cast<std::size_t>(state.range(0)));
  for (auto _ : state) {
    auto bars = aggregate_bars(ticks, 200'000'000);
    benchmark::DoNotOptimize(bars);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}
BENCHMARK(BM_AggregateBars)->Arg(1000)->Arg(100000);

BENCHMARK_MAIN();
