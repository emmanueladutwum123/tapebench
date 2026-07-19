#include <filesystem>
#include <iomanip>
#include <iostream>
#include <span>

#include "tapebench/bar_aggregator.hpp"
#include "tapebench/mapped_tape.hpp"
#include "tapebench/simulation.hpp"
#include "tapebench/strategies/mean_reversion.hpp"
#include "tapebench/strategies/moving_average_crossover.hpp"
#include "tapebench/synthetic_generator.hpp"
#include "tapebench/tape_writer.hpp"

namespace {

template <typename StrategyT>
void PrintSimulationSummary(const char* label, StrategyT& strategy, std::span<const tapebench::Bar> bars,
                             const tapebench::ExecutionModel& execution) {
  auto result = tapebench::simulate(strategy, bars, execution);
  const double final_equity = result.equity_curve.empty() ? 0.0 : result.equity_curve.back().equity;
  std::cout << "  " << label << " (" << strategy.name() << "): final equity=" << final_equity
            << "  fills=" << result.fill_count << "  final position=" << result.final_position.quantity
            << "  realized=" << result.final_position.realized_pnl << "\n";
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

  std::cout << "tapebench M4 demo -- generate -> write -> mmap-read -> aggregate -> simulate\n\n";
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

  std::filesystem::remove(tape_path);
  return 0;
}
