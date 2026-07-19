#pragma once

#include <cstdint>
#include <type_traits>

#include "tapebench/tick.hpp"

namespace tapebench {

// On-disk layout: a TapeHeader immediately followed by `tick_count`
// contiguous Tick records. Deliberately no compression or variable-length
// encoding -- the whole point is that a tape file can be memory-mapped and
// reinterpreted directly as an array of Tick, with zero parsing or
// deserialization on the read path (see MappedTape).
struct TapeHeader {
  std::uint32_t magic;       // must equal kTapeMagic
  std::uint32_t version;     // must equal kTapeVersion
  std::uint64_t tick_count;  // number of Tick records following this header
};

static_assert(std::is_trivially_copyable_v<TapeHeader>,
              "TapeHeader must stay trivially copyable to be read directly from a mapping");

inline constexpr std::uint32_t kTapeMagic = 0x45504154;  // "TAPE" read little-endian
inline constexpr std::uint32_t kTapeVersion = 1;

}  // namespace tapebench
