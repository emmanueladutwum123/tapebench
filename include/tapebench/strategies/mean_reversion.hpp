#pragma once

#include <cstddef>
#include <deque>

#include "tapebench/strategy.hpp"

namespace tapebench::strategies {

// Mean-reversion signal from a rolling z-score of closes: long when price is
// more than `entry_z_score` standard deviations BELOW its rolling mean
// (expecting reversion up), short when it's that far ABOVE (expecting
// reversion down), flat otherwise. Returns flat until the rolling window
// fills, and flat if the window's stddev is exactly zero (no z-score is
// meaningful when there's been no variation at all).
class MeanReversion : public Strategy<MeanReversion> {
 public:
  // Throws std::invalid_argument if window < 2 or entry_z_score <= 0.
  MeanReversion(std::size_t window, double entry_z_score, double position_size);

  Signal on_bar_impl(const Bar& bar);
  const char* name_impl() const { return "MeanReversion"; }

 private:
  std::size_t window_;
  double entry_z_score_;
  double position_size_;
  std::deque<double> closes_;  // bounded to the last window_ closes
};

}  // namespace tapebench::strategies
