#include "tapebench/mapped_tape.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "tapebench/tape_format.hpp"

namespace tapebench {

MappedTape::MappedTape(const std::filesystem::path& path) {
  fd_ = ::open(path.c_str(), O_RDONLY);
  if (fd_ < 0) {
    throw std::runtime_error("MappedTape: failed to open " + path.string() + ": " + std::strerror(errno));
  }

  struct stat file_stat {};
  if (::fstat(fd_, &file_stat) != 0) {
    ::close(fd_);
    throw std::runtime_error("MappedTape: fstat failed for " + path.string());
  }
  const auto file_size = static_cast<std::size_t>(file_stat.st_size);

  if (file_size < sizeof(TapeHeader)) {
    ::close(fd_);
    throw std::runtime_error("MappedTape: " + path.string() + " is too small to contain a tape header");
  }

  void* base = ::mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd_, 0);
  if (base == MAP_FAILED) {
    ::close(fd_);
    throw std::runtime_error("MappedTape: mmap failed for " + path.string());
  }
  mapped_base_ = base;
  mapped_size_ = file_size;

  TapeHeader header{};
  std::memcpy(&header, mapped_base_, sizeof(header));

  if (header.magic != kTapeMagic) {
    unmap();
    ::close(fd_);
    throw std::runtime_error("MappedTape: " + path.string() + " has an invalid magic number (not a tape file)");
  }
  if (header.version != kTapeVersion) {
    unmap();
    ::close(fd_);
    throw std::runtime_error("MappedTape: " + path.string() + " has unsupported version " +
                              std::to_string(header.version));
  }

  const std::size_t expected_size = sizeof(TapeHeader) + header.tick_count * sizeof(Tick);
  if (expected_size != file_size) {
    unmap();
    ::close(fd_);
    throw std::runtime_error("MappedTape: " + path.string() + " is truncated or corrupt (size mismatch)");
  }

  const auto* tick_data =
      reinterpret_cast<const Tick*>(static_cast<const std::byte*>(mapped_base_) + sizeof(TapeHeader));
  ticks_ = std::span<const Tick>(tick_data, header.tick_count);
}

MappedTape::~MappedTape() {
  unmap();
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

MappedTape::MappedTape(MappedTape&& other) noexcept
    : fd_(std::exchange(other.fd_, -1)),
      mapped_base_(std::exchange(other.mapped_base_, nullptr)),
      mapped_size_(std::exchange(other.mapped_size_, 0)),
      ticks_(std::exchange(other.ticks_, {})) {}

MappedTape& MappedTape::operator=(MappedTape&& other) noexcept {
  if (this != &other) {
    unmap();
    if (fd_ >= 0) {
      ::close(fd_);
    }
    fd_ = std::exchange(other.fd_, -1);
    mapped_base_ = std::exchange(other.mapped_base_, nullptr);
    mapped_size_ = std::exchange(other.mapped_size_, 0);
    ticks_ = std::exchange(other.ticks_, {});
  }
  return *this;
}

void MappedTape::unmap() noexcept {
  if (mapped_base_ != nullptr && mapped_base_ != MAP_FAILED) {
    ::munmap(mapped_base_, mapped_size_);
    mapped_base_ = nullptr;
    mapped_size_ = 0;
  }
}

}  // namespace tapebench
