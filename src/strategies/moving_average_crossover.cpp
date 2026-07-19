#include "tapebench/strategies/moving_average_crossover.hpp"

#include <stdexcept>

namespace tapebench::strategies {

MovingAverageCrossover::MovingAverageCrossover(std::size_t fast_period, std::size_t slow_period,
                                                double position_size)
    : fast_period_(fast_period), slow_period_(slow_period), position_size_(position_size) {
  if (fast_period == 0 || slow_period == 0 || fast_period >= slow_period) {
    throw std::invalid_argument(
        "MovingAverageCrossover: fast_period must be > 0 and strictly less than slow_period");
  }
}

double MovingAverageCrossover::average(std::size_t period) const {
  // Average of the most recent `period` closes; closes_ is newest-at-back.
  double sum = 0.0;
  auto it = closes_.rbegin();
  for (std::size_t i = 0; i < period; ++i, ++it) {
    sum += *it;
  }
  return sum / static_cast<double>(period);
}

Signal MovingAverageCrossover::on_bar_impl(const Bar& bar) {
  closes_.push_back(bar.close);
  if (closes_.size() > slow_period_) {
    closes_.pop_front();
  }

  if (closes_.size() < slow_period_) {
    return Signal{0.0};  // not enough history yet
  }

  const double fast_avg = average(fast_period_);
  const double slow_avg = average(slow_period_);
  return Signal{fast_avg > slow_avg ? position_size_ : -position_size_};
}

}  // namespace tapebench::strategies
