#include <benchmark/benchmark.h>

#include <cstdint>
#include <filesystem>

#include "tapebench/mapped_tape.hpp"
#include "tapebench/synthetic_generator.hpp"
#include "tapebench/tape_writer.hpp"

using namespace tapebench;

namespace {
std::filesystem::path MakeTestTape(std::size_t count) {
  SyntheticTickParams params;
  SyntheticTickGenerator gen(42, params);
  auto ticks = gen.generate(count);
  auto path = std::filesystem::temp_directory_path() / "tapebench_bench.tape";
  write_tape(path, ticks);
  return path;
}
}  // namespace

// Measures the full mmap-open + iterate-every-tick cost -- the number this
// is really validating is the "zero-copy" claim's practical payoff: reading
// millions of ticks should be dominated by memory bandwidth, not per-tick
// allocation/copy overhead (see M2's dedicated allocation-counting test for
// the structural proof; this benchmark shows the resulting throughput).
void BM_MappedTapeIteration(benchmark::State& state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  auto path = MakeTestTape(count);
  for (auto _ : state) {
    MappedTape tape(path);
    std::uint64_t sum = 0;
    for (const auto& t : tape.ticks()) {
      sum += t.timestamp_ns;
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
  std::filesystem::remove(path);
}
BENCHMARK(BM_MappedTapeIteration)->Arg(1000)->Arg(100000);

BENCHMARK_MAIN();
