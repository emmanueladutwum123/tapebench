#pragma once

#include <cstdint>
#include <type_traits>

namespace tapebench {

// Which side initiated (was the aggressor in) the trade.
enum class Side : std::uint8_t { Buy, Sell };

// A single trade print on the tape. Deliberately a flat, trivially-copyable
// POD: this is the type that gets memory-mapped and parsed in bulk in M2, so
// it needs a fixed, predictable layout with no hidden allocation or padding
// surprises. static_asserts in tick.cpp-equivalent (see tests) pin the size.
struct Tick {
  std::uint64_t timestamp_ns;  // monotonic nanoseconds since the tape start
  double price;
  std::uint32_t size;
  Side side;
};

static_assert(std::is_trivially_copyable_v<Tick>,
              "Tick must stay trivially copyable for zero-copy tape I/O in M2");

}  // namespace tapebench
