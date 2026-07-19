#pragma once

#include <cstddef>
#include <deque>

#include "tapebench/strategy.hpp"

namespace tapebench::strategies {

// Classic trend-following signal: long whenever the fast moving average of
// closes is above the slow one, short whenever it's below. Returns a flat
// (zero) signal until enough bars have accumulated to fill the slow window.
class MovingAverageCrossover : public Strategy<MovingAverageCrossover> {
 public:
  // Throws std::invalid_argument if fast_period isn't strictly less than
  // slow_period, or either is 0.
  MovingAverageCrossover(std::size_t fast_period, std::size_t slow_period, double position_size);

  Signal on_bar_impl(const Bar& bar);
  const char* name_impl() const { return "MovingAverageCrossover"; }

 private:
  double average(std::size_t period) const;

  std::size_t fast_period_;
  std::size_t slow_period_;
  double position_size_;
  std::deque<double> closes_;  // bounded to the last slow_period_ closes
};

}  // namespace tapebench::strategies
