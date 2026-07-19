#include <filesystem>
#include <iomanip>
#include <iostream>

#include "tapebench/bar_aggregator.hpp"
#include "tapebench/mapped_tape.hpp"
#include "tapebench/synthetic_generator.hpp"
#include "tapebench/tape_writer.hpp"

int main() {
  using namespace tapebench;

  SyntheticTickParams params;
  params.start_price = 100.0;
  params.volatility_per_sqrt_second = 0.02;
  params.mean_inter_arrival_us = 500.0;

  SyntheticTickGenerator gen(/*seed=*/42, params);
  auto ticks = gen.generate(2000);

  std::cout << "tapebench M2 demo -- generate -> write -> mmap-read -> aggregate\n\n";
  std::cout << "Generated " << ticks.size() << " synthetic ticks (seed=42)\n";

  const auto tape_path = std::filesystem::temp_directory_path() / "tapebench_demo.tape";
  write_tape(tape_path, ticks);
  std::cout << "Wrote tape to " << tape_path << "\n";

  MappedTape tape(tape_path);
  std::cout << "Memory-mapped tape back: " << tape.ticks().size() << " ticks (zero-copy view)\n\n";

  constexpr std::uint64_t kBarDurationNs = 500'000'000;  // 500ms bars
  auto bars = aggregate_bars(tape.ticks(), kBarDurationNs);
  std::cout << "Aggregated into " << bars.size() << " x 500ms bars. First 5:\n\n";

  std::cout << std::fixed << std::setprecision(4);
  for (std::size_t i = 0; i < bars.size() && i < 5; ++i) {
    const auto& b = bars[i];
    std::cout << "  bar[" << i << "] O=" << b.open << " H=" << b.high << " L=" << b.low
              << " C=" << b.close << " vol=" << b.volume << " trades=" << b.trade_count << "\n";
  }

  std::filesystem::remove(tape_path);
  return 0;
}
