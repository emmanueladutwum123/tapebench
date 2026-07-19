#include <gtest/gtest.h>

#include <mutex>
#include <numeric>
#include <set>
#include <stdexcept>
#include <thread>

#include "tapebench/thread_pool.hpp"

using namespace tapebench;

TEST(ThreadPool, RunsATaskAndReturnsItsResultThroughTheFuture) {
  ThreadPool pool(2);
  auto future = pool.submit([] { return 42; });
  EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPool, RunsManyTasksAndPreservesEachOnesOwnResult) {
  ThreadPool pool(4);
  std::vector<std::future<int>> futures;
  for (int i = 0; i < 100; ++i) {
    futures.push_back(pool.submit([i] { return i * i; }));
  }
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(futures[static_cast<std::size_t>(i)].get(), i * i);
  }
}

TEST(ThreadPool, PropagatesAnExceptionThrownInsideATaskThroughTheFuture) {
  ThreadPool pool(2);
  auto future = pool.submit([]() -> int { throw std::runtime_error("boom"); });
  EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(ThreadPool, ActuallyDistributesWorkAcrossMoreThanOneOsThread) {
  // Not a strict proof of concurrency, but a standard, practically-reliable
  // sanity check: with far more tasks than worker threads, on any real
  // multi-core machine the OS will schedule them onto more than one thread.
  ThreadPool pool(4);
  std::mutex ids_mutex;
  std::set<std::thread::id> ids;

  std::vector<std::future<void>> futures;
  for (int i = 0; i < 200; ++i) {
    futures.push_back(pool.submit([&] {
      std::lock_guard<std::mutex> lock(ids_mutex);
      ids.insert(std::this_thread::get_id());
    }));
  }
  for (auto& f : futures) {
    f.get();
  }

  EXPECT_GT(ids.size(), 1u);
}

TEST(ThreadPool, ShutdownIsIdempotentAndSubmitAfterShutdownThrows) {
  ThreadPool pool(2);
  pool.shutdown();
  pool.shutdown();  // must not double-join or crash

  EXPECT_THROW(pool.submit([] { return 1; }), std::runtime_error);
}

TEST(ThreadPool, ThreadCountReportsTheConfiguredWorkerCount) {
  ThreadPool pool(3);
  EXPECT_EQ(pool.thread_count(), 3u);
}
