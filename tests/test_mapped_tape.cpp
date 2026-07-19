#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "tapebench/mapped_tape.hpp"
#include "tapebench/synthetic_generator.hpp"
#include "tapebench/tape_format.hpp"
#include "tapebench/tape_writer.hpp"

using namespace tapebench;

namespace {

// A unique path per call so parallel ctest runs don't collide.
std::filesystem::path UniqueTempPath(const std::string& tag) {
  static std::atomic<int> counter{0};
  return std::filesystem::temp_directory_path() /
         ("tapebench_test_" + tag + "_" + std::to_string(counter++) + ".tape");
}

std::vector<Tick> MakeTicks(std::size_t count, std::uint64_t seed = 7) {
  SyntheticTickParams params;
  SyntheticTickGenerator gen(seed, params);
  return gen.generate(count);
}

// RAII wrapper so a test file always gets removed, even if an assertion fails.
class ScopedFile {
 public:
  explicit ScopedFile(std::filesystem::path path) : path_(std::move(path)) {}
  ~ScopedFile() { std::filesystem::remove(path_); }
  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

}  // namespace

TEST(MappedTape, RoundTripsTicksExactly) {
  ScopedFile file(UniqueTempPath("roundtrip"));
  auto original = MakeTicks(500);
  write_tape(file.path(), original);

  MappedTape tape(file.path());
  auto view = tape.ticks();

  ASSERT_EQ(view.size(), original.size());
  for (std::size_t i = 0; i < original.size(); ++i) {
    EXPECT_EQ(view[i].timestamp_ns, original[i].timestamp_ns) << "at index " << i;
    EXPECT_DOUBLE_EQ(view[i].price, original[i].price) << "at index " << i;
    EXPECT_EQ(view[i].size, original[i].size) << "at index " << i;
    EXPECT_EQ(view[i].side, original[i].side) << "at index " << i;
  }
}

TEST(MappedTape, RoundTripsAnEmptyTape) {
  ScopedFile file(UniqueTempPath("empty"));
  write_tape(file.path(), {});

  MappedTape tape(file.path());
  EXPECT_EQ(tape.ticks().size(), 0u);
}

TEST(MappedTape, ThrowsOnNonexistentFile) {
  EXPECT_THROW(MappedTape(std::filesystem::temp_directory_path() / "does_not_exist.tape"), std::runtime_error);
}

TEST(MappedTape, ThrowsOnFileTooSmallForHeader) {
  ScopedFile file(UniqueTempPath("tiny"));
  std::ofstream out(file.path(), std::ios::binary);
  out << "hi";
  out.close();

  EXPECT_THROW(MappedTape tape(file.path()), std::runtime_error);
}

TEST(MappedTape, ThrowsOnBadMagicNumber) {
  ScopedFile file(UniqueTempPath("badmagic"));
  TapeHeader bad_header{0xDEADBEEF, kTapeVersion, 0};
  std::ofstream out(file.path(), std::ios::binary);
  out.write(reinterpret_cast<const char*>(&bad_header), sizeof(bad_header));
  out.close();

  EXPECT_THROW(MappedTape tape(file.path()), std::runtime_error);
}

TEST(MappedTape, ThrowsOnUnsupportedVersion) {
  ScopedFile file(UniqueTempPath("badversion"));
  TapeHeader bad_header{kTapeMagic, kTapeVersion + 1, 0};
  std::ofstream out(file.path(), std::ios::binary);
  out.write(reinterpret_cast<const char*>(&bad_header), sizeof(bad_header));
  out.close();

  EXPECT_THROW(MappedTape tape(file.path()), std::runtime_error);
}

TEST(MappedTape, ThrowsOnTruncatedTickData) {
  ScopedFile file(UniqueTempPath("truncated"));
  // Header claims 100 ticks but the file only has room for 1.
  TapeHeader header{kTapeMagic, kTapeVersion, 100};
  auto ticks = MakeTicks(1);
  std::ofstream out(file.path(), std::ios::binary);
  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(reinterpret_cast<const char*>(ticks.data()), static_cast<std::streamsize>(sizeof(Tick)));
  out.close();

  EXPECT_THROW(MappedTape tape(file.path()), std::runtime_error);
}

TEST(MappedTape, MoveConstructionTransfersTheMapping) {
  ScopedFile file(UniqueTempPath("move"));
  auto original = MakeTicks(50);
  write_tape(file.path(), original);

  MappedTape tape_a(file.path());
  MappedTape tape_b(std::move(tape_a));

  EXPECT_EQ(tape_b.ticks().size(), original.size());
  EXPECT_EQ(tape_a.ticks().size(), 0u);  // moved-from is empty, not dangling
}

TEST(MappedTape, MoveAssignmentTransfersTheMapping) {
  ScopedFile file_a(UniqueTempPath("move_assign_a"));
  ScopedFile file_b(UniqueTempPath("move_assign_b"));
  write_tape(file_a.path(), MakeTicks(10));
  write_tape(file_b.path(), MakeTicks(30));

  MappedTape tape_a(file_a.path());
  MappedTape tape_b(file_b.path());
  tape_a = std::move(tape_b);

  EXPECT_EQ(tape_a.ticks().size(), 30u);
}
