#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "tapebench/bar.hpp"
#include "tapebench/tick.hpp"

namespace tapebench {

// Aggregates a tape of ticks into fixed-duration time bars. Buckets are
// aligned to the first tick's timestamp (bucket 0 starts exactly at
// ticks[0].timestamp_ns) rather than to any external wall-clock epoch, since
// tape timestamps are relative to tape start, not real time. A tick belongs
// to bucket floor((tick.timestamp_ns - ticks[0].timestamp_ns) / bucket_duration_ns).
// Returns an empty vector if `ticks` is empty or `bucket_duration_ns` is 0.
std::vector<Bar> aggregate_bars(std::span<const Tick> ticks, std::uint64_t bucket_duration_ns);

}  // namespace tapebench
