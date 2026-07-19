#pragma once

#include <span>
#include <vector>

#include "tapebench/analytics.hpp"
#include "tapebench/simulation.hpp"
#include "tapebench/thread_pool.hpp"

namespace tapebench {

// Runs analyze(simulate(strategies[i], bars, execution)) for every strategy
// in `strategies`, dispatched across `pool`, and returns the
// PerformanceReports in the SAME order as the input (result[i] always
// corresponds to strategies[i]). Each strategy instance is only ever
// touched by the one worker thread running its task -- `bars` and
// `execution` are read-only and shared, so there is no data race by
// construction, not just by luck.
template <typename StrategyT>
std::vector<PerformanceReport> run_grid_parallel(std::vector<StrategyT>& strategies, std::span<const Bar> bars,
                                                  const ExecutionModel& execution, ThreadPool& pool,
                                                  double bars_per_year = 0.0) {
  std::vector<std::future<PerformanceReport>> futures;
  futures.reserve(strategies.size());

  for (auto& strategy : strategies) {
    futures.push_back(pool.submit([&strategy, bars, &execution, bars_per_year]() {
      auto result = simulate(strategy, bars, execution);
      return analyze(result, bars_per_year);
    }));
  }

  std::vector<PerformanceReport> reports;
  reports.reserve(futures.size());
  for (auto& future : futures) {
    reports.push_back(future.get());
  }
  return reports;
}

// Sequential counterpart, used to validate run_grid_parallel's correctness:
// same strategies, same bars, same execution model must produce
// byte-identical results, since simulate() is fully deterministic.
template <typename StrategyT>
std::vector<PerformanceReport> run_grid_sequential(std::vector<StrategyT>& strategies, std::span<const Bar> bars,
                                                    const ExecutionModel& execution, double bars_per_year = 0.0) {
  std::vector<PerformanceReport> reports;
  reports.reserve(strategies.size());
  for (auto& strategy : strategies) {
    auto result = simulate(strategy, bars, execution);
    reports.push_back(analyze(result, bars_per_year));
  }
  return reports;
}

}  // namespace tapebench
