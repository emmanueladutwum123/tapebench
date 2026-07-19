#include <benchmark/benchmark.h>

#include "tapebench/bar_aggregator.hpp"
#include "tapebench/simulation.hpp"
#include "tapebench/strategies/mean_reversion.hpp"
#include "tapebench/strategies/moving_average_crossover.hpp"
#include "tapebench/synthetic_generator.hpp"

using namespace tapebench;

namespace {
std::vector<Bar> MakeBars(std::size_t tick_count) {
  SyntheticTickParams params;
  SyntheticTickGenerator gen(42, params);
  auto ticks = gen.generate(tick_count);
  return aggregate_bars(ticks, 50'000'000);  // 50ms bars -> plenty of bars for a given tick count
}
}  // namespace

void BM_Simulate_MovingAverageCrossover(benchmark::State& state) {
  auto bars = MakeBars(static_cast<std::size_t>(state.range(0)));
  ExecutionModel execution{5.0};
  for (auto _ : state) {
    strategies::MovingAverageCrossover strategy(5, 20, 1.0);
    auto result = simulate(strategy, bars, execution);
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * static_cast<std::int64_t>(bars.size()));
}
BENCHMARK(BM_Simulate_MovingAverageCrossover)->Arg(20000)->Arg(200000);

void BM_Simulate_MeanReversion(benchmark::State& state) {
  auto bars = MakeBars(static_cast<std::size_t>(state.range(0)));
  ExecutionModel execution{5.0};
  for (auto _ : state) {
    strategies::MeanReversion strategy(20, 1.5, 1.0);
    auto result = simulate(strategy, bars, execution);
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * static_cast<std::int64_t>(bars.size()));
}
BENCHMARK(BM_Simulate_MeanReversion)->Arg(20000)->Arg(200000);

BENCHMARK_MAIN();
