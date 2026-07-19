#include <benchmark/benchmark.h>

#include "tapebench/synthetic_generator.hpp"

using namespace tapebench;

void BM_GenerateTicks(benchmark::State& state) {
  SyntheticTickParams params;
  const auto count = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    SyntheticTickGenerator gen(42, params);
    auto ticks = gen.generate(count);
    benchmark::DoNotOptimize(ticks);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}
BENCHMARK(BM_GenerateTicks)->Arg(1000)->Arg(10000)->Arg(100000);

BENCHMARK_MAIN();
