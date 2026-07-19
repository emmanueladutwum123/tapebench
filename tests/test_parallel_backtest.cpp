#include <gtest/gtest.h>

#include "tapebench/bar_aggregator.hpp"
#include "tapebench/parallel_backtest.hpp"
#include "tapebench/strategies/mean_reversion.hpp"
#include "tapebench/strategies/moving_average_crossover.hpp"
#include "tapebench/synthetic_generator.hpp"

using namespace tapebench;

namespace {

std::vector<Bar> MakeBars() {
  SyntheticTickParams params;
  SyntheticTickGenerator gen(7, params);
  auto ticks = gen.generate(20000);
  return aggregate_bars(ticks, 200'000'000);  // 200ms bars
}

std::vector<strategies::MovingAverageCrossover> MakeStrategyGrid() {
  std::vector<strategies::MovingAverageCrossover> strategies;
  for (std::size_t fast : {3u, 5u, 8u}) {
    for (std::size_t slow : {15u, 20u, 30u}) {
      strategies.emplace_back(fast, slow, 1.0);
    }
  }
  return strategies;
}

}  // namespace

TEST(ParallelBacktest, ParallelAndSequentialGridsProduceIdenticalResults) {
  auto bars = MakeBars();
  auto sequential_strategies = MakeStrategyGrid();
  auto parallel_strategies = MakeStrategyGrid();
  ASSERT_EQ(sequential_strategies.size(), parallel_strategies.size());

  ExecutionModel execution{5.0};

  auto sequential_reports = run_grid_sequential(sequential_strategies, bars, execution);

  ThreadPool pool(4);
  auto parallel_reports = run_grid_parallel(parallel_strategies, bars, execution, pool);

  ASSERT_EQ(sequential_reports.size(), parallel_reports.size());
  for (std::size_t i = 0; i < sequential_reports.size(); ++i) {
    const auto& seq = sequential_reports[i];
    const auto& par = parallel_reports[i];
    EXPECT_DOUBLE_EQ(seq.total_pnl, par.total_pnl) << "at grid index " << i;
    EXPECT_DOUBLE_EQ(seq.sharpe_ratio, par.sharpe_ratio) << "at grid index " << i;
    EXPECT_DOUBLE_EQ(seq.sortino_ratio, par.sortino_ratio) << "at grid index " << i;
    EXPECT_DOUBLE_EQ(seq.max_drawdown, par.max_drawdown) << "at grid index " << i;
    EXPECT_EQ(seq.fill_count, par.fill_count) << "at grid index " << i;
    EXPECT_DOUBLE_EQ(seq.total_traded_quantity, par.total_traded_quantity) << "at grid index " << i;
  }
}

TEST(ParallelBacktest, ResultsPreserveInputOrderNotCompletionOrder) {
  // Different strategy shapes (MeanReversion) with deliberately varied
  // window sizes, so each takes a different amount of work -- if
  // run_grid_parallel accidentally returned results in completion order
  // instead of input order, this would catch it.
  auto bars = MakeBars();
  std::vector<strategies::MeanReversion> strategies;
  strategies.emplace_back(5, 1.0, 1.0);
  strategies.emplace_back(10, 1.2, 1.0);
  strategies.emplace_back(20, 1.5, 1.0);
  strategies.emplace_back(30, 1.8, 1.0);

  std::vector<strategies::MeanReversion> strategies_copy;
  strategies_copy.emplace_back(5, 1.0, 1.0);
  strategies_copy.emplace_back(10, 1.2, 1.0);
  strategies_copy.emplace_back(20, 1.5, 1.0);
  strategies_copy.emplace_back(30, 1.8, 1.0);

  ExecutionModel execution{0.0};
  ThreadPool pool(4);
  auto parallel_reports = run_grid_parallel(strategies, bars, execution, pool);
  auto sequential_reports = run_grid_sequential(strategies_copy, bars, execution);

  ASSERT_EQ(parallel_reports.size(), 4u);
  for (std::size_t i = 0; i < 4; ++i) {
    EXPECT_DOUBLE_EQ(parallel_reports[i].total_pnl, sequential_reports[i].total_pnl) << "at grid index " << i;
  }
}

TEST(ParallelBacktest, EmptyStrategyGridProducesAnEmptyResult) {
  std::vector<strategies::MovingAverageCrossover> no_strategies;
  ExecutionModel execution{0.0};
  ThreadPool pool(2);

  auto reports = run_grid_parallel(no_strategies, std::span<const Bar>{}, execution, pool);
  EXPECT_TRUE(reports.empty());
}
