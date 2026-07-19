#include <gtest/gtest.h>

#include "tapebench/strategies/moving_average_crossover.hpp"

using namespace tapebench;
using tapebench::strategies::MovingAverageCrossover;

namespace {
Bar MakeBar(double close) {
  return Bar{0, 1, close, close, close, close, 1, 1};
}
}  // namespace

TEST(MovingAverageCrossover, RejectsInvalidPeriods) {
  EXPECT_THROW(MovingAverageCrossover(0, 5, 1.0), std::invalid_argument);
  EXPECT_THROW(MovingAverageCrossover(5, 5, 1.0), std::invalid_argument);
  EXPECT_THROW(MovingAverageCrossover(6, 5, 1.0), std::invalid_argument);
}

TEST(MovingAverageCrossover, FlatUntilTheSlowWindowFills) {
  MovingAverageCrossover strategy(3, 5, 10.0);
  for (int i = 0; i < 4; ++i) {
    Signal signal = strategy.on_bar(MakeBar(10.0));
    EXPECT_DOUBLE_EQ(signal.target_position, 0.0) << "at bar " << i;
  }
}

TEST(MovingAverageCrossover, GoesLongWhenFastAverageRisesAboveSlow) {
  // fast=3, slow=5. 5 bars of 10 fill the window with fast==slow (short,
  // since the comparison is strict '>'), then a run of 20s pulls the fast
  // average above the slow average, which still has stale 10s in it.
  MovingAverageCrossover strategy(3, 5, 10.0);
  std::vector<double> closes{10, 10, 10, 10, 10, 20, 20, 20, 20};
  std::vector<double> signals;
  for (double c : closes) {
    signals.push_back(strategy.on_bar(MakeBar(c)).target_position);
  }

  // Bars 0-3: warmup, flat.
  for (int i = 0; i < 4; ++i) {
    EXPECT_DOUBLE_EQ(signals[static_cast<std::size_t>(i)], 0.0) << "at bar " << i;
  }
  // Bar 4: window just filled with all-equal closes -> fast == slow -> short.
  EXPECT_DOUBLE_EQ(signals[4], -10.0);
  // Bars 5-8: fast average (reacting faster to the new 20s) pulls above the
  // slow average (still dragged down by older 10s) -> long.
  for (std::size_t i = 5; i < signals.size(); ++i) {
    EXPECT_DOUBLE_EQ(signals[i], 10.0) << "at bar " << i;
  }
}

TEST(MovingAverageCrossover, GoesShortWhenFastAverageFallsBelowSlow) {
  MovingAverageCrossover strategy(3, 5, 10.0);
  std::vector<double> closes{10, 10, 10, 10, 10, 5, 5, 5, 5};
  std::vector<double> signals;
  for (double c : closes) {
    signals.push_back(strategy.on_bar(MakeBar(c)).target_position);
  }

  EXPECT_DOUBLE_EQ(signals[4], -10.0);  // equal averages -> short (strict '>')
  for (std::size_t i = 5; i < signals.size(); ++i) {
    EXPECT_DOUBLE_EQ(signals[i], -10.0) << "at bar " << i;  // fast pulled below slow
  }
}
