#include "tapebench/bar_aggregator.hpp"

#include <algorithm>

namespace tapebench {

std::vector<Bar> aggregate_bars(std::span<const Tick> ticks, std::uint64_t bucket_duration_ns) {
  std::vector<Bar> bars;
  if (ticks.empty() || bucket_duration_ns == 0) {
    return bars;
  }

  const std::uint64_t t0 = ticks[0].timestamp_ns;
  Bar current{};
  bool have_current = false;
  std::uint64_t current_bucket = 0;

  auto flush = [&]() {
    if (have_current) {
      bars.push_back(current);
    }
  };

  for (const auto& tick : ticks) {
    const std::uint64_t bucket = (tick.timestamp_ns - t0) / bucket_duration_ns;
    if (!have_current || bucket != current_bucket) {
      flush();
      current = Bar{};
      current.start_timestamp_ns = t0 + bucket * bucket_duration_ns;
      current.end_timestamp_ns = current.start_timestamp_ns + bucket_duration_ns;
      current.open = tick.price;
      current.high = tick.price;
      current.low = tick.price;
      current.close = tick.price;
      current.volume = tick.size;
      current.trade_count = 1;
      current_bucket = bucket;
      have_current = true;
    } else {
      current.high = std::max(current.high, tick.price);
      current.low = std::min(current.low, tick.price);
      current.close = tick.price;
      current.volume += tick.size;
      current.trade_count += 1;
    }
  }
  flush();

  return bars;
}

}  // namespace tapebench
