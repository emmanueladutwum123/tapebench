#pragma once

#include <cstdint>

namespace tapebench {

// An OHLCV bar aggregated from a run of consecutive ticks over a fixed
// time bucket.
struct Bar {
  std::uint64_t start_timestamp_ns;
  std::uint64_t end_timestamp_ns;
  double open;
  double high;
  double low;
  double close;
  std::uint64_t volume;       // sum of tick sizes in this bar
  std::uint32_t trade_count;  // number of ticks aggregated into this bar
};

}  // namespace tapebench
