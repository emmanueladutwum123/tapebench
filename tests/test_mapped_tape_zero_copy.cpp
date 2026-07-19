// Deliberately its own test executable (see tests/CMakeLists.txt): this file
// overrides the GLOBAL operator new/delete for the whole process it links
// into, to count heap allocations. Isolating it means the other test binary
// (tapebench_tests) stays unaffected and its allocation patterns aren't
// muddied by this instrumentation.
#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <new>

#include "tapebench/mapped_tape.hpp"
#include "tapebench/synthetic_generator.hpp"
#include "tapebench/tape_writer.hpp"

namespace {
std::atomic<std::size_t> g_new_count{0};
}  // namespace

// Every overload must be covered, not just the plain throwing new/delete:
// libstdc++'s std::stable_sort (used internally by GoogleTest to order test
// cases) calls the *nothrow* operator new to allocate its temporary buffer.
// Missing that overload here caused ASan to report an alloc-dealloc-mismatch
// on Linux CI (that allocation fell through to ASan's own allocator, then
// got freed via this file's std::free()-based operator delete) -- it never
// reproduced locally on macOS, only on the gcc/Linux CI leg.
void* operator new(std::size_t size) {
  ++g_new_count;
  void* ptr = std::malloc(size);
  if (ptr == nullptr) {
    throw std::bad_alloc();
  }
  return ptr;
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
  ++g_new_count;
  return std::malloc(size);
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
  return ::operator new(size, std::nothrow);
}
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete(void* ptr, const std::nothrow_t&) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, const std::nothrow_t&) noexcept { std::free(ptr); }

namespace {

// Snapshots the allocation counter at construction; `allocations()` reports
// how many `operator new` calls happened since then. This scopes the check
// to just the code under test, not GoogleTest's own setup/tooling churn.
class AllocationScope {
 public:
  AllocationScope() : start_(g_new_count.load()) {}
  std::size_t allocations() const { return g_new_count.load() - start_; }

 private:
  std::size_t start_;
};

}  // namespace

using namespace tapebench;

TEST(MappedTapeZeroCopy, ReadingAllTicksThroughTheSpanDoesNotHeapAllocate) {
  const auto path = std::filesystem::temp_directory_path() / "tapebench_zero_copy_test.tape";

  SyntheticTickParams params;
  SyntheticTickGenerator gen(7, params);
  auto ticks = gen.generate(5000);
  write_tape(path, ticks);

  MappedTape tape(path);  // opening/mapping the file is not the claim under test

  // Touch every tick through the returned span, forcing real memory reads,
  // then verify none of that triggered a heap allocation -- the whole point
  // of memory-mapping is that reading the data is a pointer dereference into
  // the mapping, not a copy.
  AllocationScope scope;
  std::uint64_t checksum = 0;
  for (const auto& t : tape.ticks()) {
    checksum += t.timestamp_ns;
    checksum += static_cast<std::uint64_t>(t.size);
  }
  // A single volatile write (not a compound assignment, which C++20
  // deprecates for volatile) prevents the compiler from optimizing the
  // whole loop away as dead code.
  volatile std::uint64_t sink = checksum;
  static_cast<void>(sink);

  EXPECT_EQ(scope.allocations(), 0u);
  EXPECT_GT(checksum, 0u);  // sanity: the loop actually ran

  std::filesystem::remove(path);
}
