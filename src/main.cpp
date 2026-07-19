#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <span>

#include "tapebench/analytics.hpp"
#include "tapebench/bar_aggregator.hpp"
#include "tapebench/mapped_tape.hpp"
#include "tapebench/parallel_backtest.hpp"
#include "tapebench/simulation.hpp"
#include "tapebench/strategies/mean_reversion.hpp"
#include "tapebench/strategies/moving_average_crossover.hpp"
#include "tapebench/synthetic_generator.hpp"
#include "tapebench/tape_writer.hpp"
#include "tapebench/thread_pool.hpp"

namespace {

template <typename StrategyT>
void PrintSimulationSummary(const char* label, StrategyT& strategy, std::span<const tapebench::Bar> bars,
                             const tapebench::ExecutionModel& execution) {
  auto result = tapebench::simulate(strategy, bars, execution);
  auto report = tapebench::analyze(result);
  std::cout << "  " << label << " (" << strategy.name() << ")\n"
            << "    total_pnl=" << report.total_pnl << "  sharpe=" << report.sharpe_ratio
            << "  sortino=" << report.sortino_ratio << "  max_drawdown=" << report.max_drawdown << "\n"
            << "    fills=" << report.fill_count << "  traded_qty=" << report.total_traded_quantity
            << "  final_position=" << result.final_position.quantity << "\n";
}

}  // namespace

int main() {
  using namespace tapebench;

  SyntheticTickParams params;
  params.start_price = 100.0;
  params.volatility_per_sqrt_second = 0.02;
  params.mean_inter_arrival_us = 500.0;

  SyntheticTickGenerator gen(/*seed=*/42, params);
  auto ticks = gen.generate(20000);

  std::cout << "tapebench M6 demo -- generate -> write -> mmap-read -> aggregate -> simulate -> analyze -> grid search\n\n";
  std::cout << "Generated " << ticks.size() << " synthetic ticks (seed=42)\n";

  const auto tape_path = std::filesystem::temp_directory_path() / "tapebench_demo.tape";
  write_tape(tape_path, ticks);
  std::cout << "Wrote tape to " << tape_path << "\n";

  MappedTape tape(tape_path);
  std::cout << "Memory-mapped tape back: " << tape.ticks().size() << " ticks (zero-copy view)\n\n";

  constexpr std::uint64_t kBarDurationNs = 200'000'000;  // 200ms bars
  auto bars = aggregate_bars(tape.ticks(), kBarDurationNs);
  std::cout << "Aggregated into " << bars.size() << " x 200ms bars. First 5:\n\n";

  std::cout << std::fixed << std::setprecision(4);
  for (std::size_t i = 0; i < bars.size() && i < 5; ++i) {
    const auto& b = bars[i];
    std::cout << "  bar[" << i << "] O=" << b.open << " H=" << b.high << " L=" << b.low
              << " C=" << b.close << " vol=" << b.volume << " trades=" << b.trade_count << "\n";
  }

  std::cout << "\nSimulating both example strategies (5 bps slippage, no-look-ahead: signal from\n"
             << "bar N's close only fills at bar N+1's open):\n";
  ExecutionModel execution{/*slippage_bps=*/5.0};
  strategies::MovingAverageCrossover ma_crossover(/*fast=*/5, /*slow=*/20, /*position_size=*/1.0);
  strategies::MeanReversion mean_reversion(/*window=*/20, /*entry_z_score=*/1.5, /*position_size=*/1.0);
  PrintSimulationSummary("MovingAverageCrossover", ma_crossover, bars, execution);
  PrintSimulationSummary("MeanReversion", mean_reversion, bars, execution);

  std::cout << "\nParameter grid search: 9 MovingAverageCrossover variants (fast in {3,5,8} x slow in\n"
             << "{15,20,30}), sequential vs. parallel (custom thread pool, 4 workers):\n";

  auto make_grid = [] {
    std::vector<strategies::MovingAverageCrossover> grid;
    for (std::size_t fast : {3u, 5u, 8u}) {
      for (std::size_t slow : {15u, 20u, 30u}) {
        grid.emplace_back(fast, slow, 1.0);
      }
    }
    return grid;
  };

  auto sequential_grid = make_grid();
  const auto seq_start = std::chrono::steady_clock::now();
  auto sequential_reports = run_grid_sequential(sequential_grid, bars, execution);
  const auto seq_elapsed = std::chrono::steady_clock::now() - seq_start;

  auto parallel_grid = make_grid();
  ThreadPool pool(4);
  const auto par_start = std::chrono::steady_clock::now();
  auto parallel_reports = run_grid_parallel(parallel_grid, bars, execution, pool);
  const auto par_elapsed = std::chrono::steady_clock::now() - par_start;

  bool results_match = true;
  for (std::size_t i = 0; i < sequential_reports.size(); ++i) {
    if (sequential_reports[i].total_pnl != parallel_reports[i].total_pnl) {
      results_match = false;
    }
  }

  std::size_t best_idx = 0;
  for (std::size_t i = 1; i < parallel_reports.size(); ++i) {
    if (parallel_reports[i].sharpe_ratio > parallel_reports[best_idx].sharpe_ratio) {
      best_idx = i;
    }
  }

  std::cout << "  " << sequential_reports.size() << " variants; parallel results match sequential: "
            << (results_match ? "yes" : "NO -- BUG") << "\n"
            << "  sequential: " << std::chrono::duration<double, std::milli>(seq_elapsed).count() << " ms\n"
            << "  parallel:   " << std::chrono::duration<double, std::milli>(par_elapsed).count() << " ms\n"
            << "  (informal timing on a 9-variant/50-bar grid -- thread overhead dominates at this\n"
             << "  size; see BENCHMARKS.md from M7 for rigorous, larger-scale numbers)\n"
             << "  best Sharpe in grid: variant " << best_idx
             << " (sharpe=" << parallel_reports[best_idx].sharpe_ratio << ")\n";

  std::filesystem::remove(tape_path);
  return 0;
}
