#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace tapebench {

// A minimal, fixed-size thread pool: worker threads pull tasks from a
// shared queue guarded by a mutex + condition_variable. submit() returns a
// std::future so callers can collect each task's result (or a propagated
// exception) independently. shutdown() is idempotent and safe to call
// explicitly or let the destructor call it; submit() after shutdown throws.
class ThreadPool {
 public:
  explicit ThreadPool(std::size_t thread_count);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  template <typename F>
  auto submit(F&& task) -> std::future<std::invoke_result_t<F>> {
    using ReturnType = std::invoke_result_t<F>;
    auto packaged = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(task));
    std::future<ReturnType> future = packaged->get_future();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopping_) {
        throw std::runtime_error("ThreadPool: submit() called after shutdown");
      }
      tasks_.emplace([packaged]() { (*packaged)(); });
    }
    cv_.notify_one();
    return future;
  }

  // Stops accepting new tasks and joins every worker thread. Idempotent --
  // safe to call explicitly and then let the destructor call it again (a
  // no-op the second time), or to rely on the destructor alone.
  void shutdown();

  std::size_t thread_count() const { return workers_.size(); }

 private:
  void worker_loop();

  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stopping_ = false;
  bool stopped_ = false;
};

}  // namespace tapebench
