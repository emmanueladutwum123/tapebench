#include "tapebench/analytics.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace tapebench {

PerformanceReport analyze(const SimulationResult& result, double bars_per_year) {
  PerformanceReport report;
  report.num_bars = static_cast<int>(result.equity_curve.size());
  report.fill_count = result.fill_count;
  report.total_traded_quantity = result.total_traded_quantity;

  if (result.equity_curve.empty()) {
    return report;
  }

  report.total_pnl = result.equity_curve.back().equity;

  std::vector<double> pnl_changes;
  pnl_changes.reserve(result.equity_curve.size());
  double prev_equity = 0.0;
  for (const auto& point : result.equity_curve) {
    pnl_changes.push_back(point.equity - prev_equity);
    prev_equity = point.equity;
  }

  const double mean =
      std::accumulate(pnl_changes.begin(), pnl_changes.end(), 0.0) / static_cast<double>(pnl_changes.size());

  double variance_sum = 0.0;
  double downside_variance_sum = 0.0;
  std::size_t downside_count = 0;
  for (double change : pnl_changes) {
    const double diff = change - mean;
    variance_sum += diff * diff;
    if (change < 0.0) {
      downside_variance_sum += change * change;  // semi-deviation from 0, not from the mean
      ++downside_count;
    }
  }

  const double stddev = std::sqrt(variance_sum / static_cast<double>(pnl_changes.size()));
  const double annualization = bars_per_year > 0.0 ? std::sqrt(bars_per_year) : 1.0;

  report.sharpe_ratio = (stddev > 0.0) ? (mean / stddev) * annualization : 0.0;

  const double downside_stddev =
      downside_count > 0 ? std::sqrt(downside_variance_sum / static_cast<double>(downside_count)) : 0.0;
  report.sortino_ratio = (downside_stddev > 0.0) ? (mean / downside_stddev) * annualization : 0.0;

  double peak = result.equity_curve.front().equity;
  double max_dd = 0.0;
  for (const auto& point : result.equity_curve) {
    peak = std::max(peak, point.equity);
    max_dd = std::max(max_dd, peak - point.equity);
  }
  report.max_drawdown = max_dd;

  return report;
}

}  // namespace tapebench
