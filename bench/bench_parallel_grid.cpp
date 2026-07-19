#include <benchmark/benchmark.h>

#include <thread>

#include "tapebench/bar_aggregator.hpp"
#include "tapebench/parallel_backtest.hpp"
#include "tapebench/strategies/moving_average_crossover.hpp"
#include "tapebench/synthetic_generator.hpp"
#include "tapebench/thread_pool.hpp"

using namespace tapebench;

namespace {

std::vector<Bar> MakeLargeBars() {
  SyntheticTickParams params;
  SyntheticTickGenerator gen(42, params);
  auto ticks = gen.generate(500'000);
  return aggregate_bars(ticks, 10'000'000);  // 10ms bars -> a large bar count
}

std::vector<strategies::MovingAverageCrossover> MakeGrid(std::size_t n) {
  std::vector<strategies::MovingAverageCrossover> grid;
  grid.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    grid.emplace_back(3 + i % 10, 20 + i % 30, 1.0);  // always fast < slow: max fast=12, min slow=20
  }
  return grid;
}

}  // namespace

void BM_Grid_Sequential(benchmark::State& state) {
  auto bars = MakeLargeBars();
  ExecutionModel execution{5.0};
  const auto grid_size = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    auto grid = MakeGrid(grid_size);
    auto reports = run_grid_sequential(grid, bars, execution);
    benchmark::DoNotOptimize(reports);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}
BENCHMARK(BM_Grid_Sequential)->Arg(50)->Arg(200);

void BM_Grid_Parallel(benchmark::State& state) {
  auto bars = MakeLargeBars();
  ExecutionModel execution{5.0};
  ThreadPool pool(std::thread::hardware_concurrency());
  const auto grid_size = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    auto grid = MakeGrid(grid_size);
    auto reports = run_grid_parallel(grid, bars, execution, pool);
    benchmark::DoNotOptimize(reports);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}
BENCHMARK(BM_Grid_Parallel)->Arg(50)->Arg(200);

BENCHMARK_MAIN();
