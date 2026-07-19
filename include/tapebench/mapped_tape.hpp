#pragma once

#include <cstddef>
#include <filesystem>
#include <span>

#include "tapebench/tick.hpp"

namespace tapebench {

// Zero-copy read access to a tape file written by write_tape(): memory-maps
// the file and exposes its Tick records directly as a std::span pointing
// into the mapped region -- no per-tick parsing, no heap copy of the data.
// Move-only (owns an OS-level mapping + file descriptor). Throws
// std::runtime_error on a missing, truncated, or malformed (bad magic or
// version) tape file. POSIX-only (mmap) -- Linux and macOS, no Windows.
class MappedTape {
 public:
  explicit MappedTape(const std::filesystem::path& path);
  ~MappedTape();

  MappedTape(const MappedTape&) = delete;
  MappedTape& operator=(const MappedTape&) = delete;
  MappedTape(MappedTape&& other) noexcept;
  MappedTape& operator=(MappedTape&& other) noexcept;

  // Zero-copy view over every tick in the tape, in on-disk order.
  std::span<const Tick> ticks() const noexcept { return ticks_; }

 private:
  void unmap() noexcept;

  int fd_ = -1;
  void* mapped_base_ = nullptr;
  std::size_t mapped_size_ = 0;
  std::span<const Tick> ticks_;
};

}  // namespace tapebench
