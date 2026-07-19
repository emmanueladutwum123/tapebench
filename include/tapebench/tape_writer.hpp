#pragma once

#include <filesystem>
#include <span>

#include "tapebench/tick.hpp"

namespace tapebench {

// Writes `ticks` to `path` in the tapebench binary tape format (see
// tape_format.hpp). Throws std::runtime_error if the file can't be created
// or fully written.
void write_tape(const std::filesystem::path& path, std::span<const Tick> ticks);

}  // namespace tapebench
