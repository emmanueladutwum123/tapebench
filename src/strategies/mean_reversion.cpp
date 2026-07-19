#include "tapebench/strategies/mean_reversion.hpp"

#include <cmath>
#include <numeric>
#include <stdexcept>

namespace tapebench::strategies {

MeanReversion::MeanReversion(std::size_t window, double entry_z_score, double position_size)
    : window_(window), entry_z_score_(entry_z_score), position_size_(position_size) {
  if (window < 2) {
    throw std::invalid_argument("MeanReversion: window must be >= 2");
  }
  if (entry_z_score <= 0.0) {
    throw std::invalid_argument("MeanReversion: entry_z_score must be positive");
  }
}

Signal MeanReversion::on_bar_impl(const Bar& bar) {
  closes_.push_back(bar.close);
  if (closes_.size() > window_) {
    closes_.pop_front();
  }
  if (closes_.size() < window_) {
    return Signal{0.0};  // not enough history yet
  }

  const double sum = std::accumulate(closes_.begin(), closes_.end(), 0.0);
  const double mean = sum / static_cast<double>(window_);

  double sq_sum = 0.0;
  for (double close : closes_) {
    const double diff = close - mean;
    sq_sum += diff * diff;
  }
  const double stddev = std::sqrt(sq_sum / static_cast<double>(window_));

  if (stddev == 0.0) {
    return Signal{0.0};  // no variation at all -- a z-score would be undefined/meaningless
  }

  const double z = (bar.close - mean) / stddev;
  if (z <= -entry_z_score_) {
    return Signal{position_size_};  // far below mean -> expect reversion up -> go long
  }
  if (z >= entry_z_score_) {
    return Signal{-position_size_};  // far above mean -> expect reversion down -> go short
  }
  return Signal{0.0};
}

}  // namespace tapebench::strategies
