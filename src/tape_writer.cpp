#include "tapebench/tape_writer.hpp"

#include <cstdio>
#include <stdexcept>

#include "tapebench/tape_format.hpp"

namespace tapebench {

void write_tape(const std::filesystem::path& path, std::span<const Tick> ticks) {
  std::FILE* file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) {
    throw std::runtime_error("write_tape: failed to open " + path.string() + " for writing");
  }

  TapeHeader header{kTapeMagic, kTapeVersion, static_cast<std::uint64_t>(ticks.size())};
  bool ok = std::fwrite(&header, sizeof(header), 1, file) == 1;
  if (ok && !ticks.empty()) {
    ok = std::fwrite(ticks.data(), sizeof(Tick), ticks.size(), file) == ticks.size();
  }

  bool closed_ok = std::fclose(file) == 0;
  if (!ok || !closed_ok) {
    throw std::runtime_error("write_tape: failed to fully write " + path.string());
  }
}

}  // namespace tapebench
