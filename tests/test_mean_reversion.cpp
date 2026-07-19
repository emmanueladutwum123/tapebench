#include <gtest/gtest.h>

#include "tapebench/strategies/mean_reversion.hpp"

using namespace tapebench;
using tapebench::strategies::MeanReversion;

namespace {
Bar MakeBar(double close) {
  return Bar{0, 1, close, close, close, close, 1, 1};
}
}  // namespace

TEST(MeanReversion, RejectsInvalidParameters) {
  EXPECT_THROW(MeanReversion(1, 2.0, 1.0), std::invalid_argument);   // window < 2
  EXPECT_THROW(MeanReversion(5, 0.0, 1.0), std::invalid_argument);   // z-score must be positive
  EXPECT_THROW(MeanReversion(5, -1.0, 1.0), std::invalid_argument);  // z-score must be positive
}

TEST(MeanReversion, FlatUntilTheWindowFills) {
  MeanReversion strategy(5, 1.0, 10.0);
  for (int i = 0; i < 4; ++i) {
    Signal signal = strategy.on_bar(MakeBar(100.0 + static_cast<double>(i)));
    EXPECT_DOUBLE_EQ(signal.target_position, 0.0) << "at bar " << i;
  }
}

TEST(MeanReversion, StaysFlatWhenAllPricesInWindowAreIdentical) {
  // Zero variation -> zero stddev -> the guard must keep this flat rather
  // than dividing by zero / producing an undefined z-score.
  MeanReversion strategy(5, 1.0, 10.0);
  for (int i = 0; i < 10; ++i) {
    Signal signal = strategy.on_bar(MakeBar(100.0));
    EXPECT_DOUBLE_EQ(signal.target_position, 0.0) << "at bar " << i;
  }
}

// NOTE on thresholds below: this is a *population* z-score computed from a
// window that includes the outlier itself, which caps the maximum
// achievable |z| at sqrt(window - 1) as the outlier's own magnitude inflates
// its own stddev denominator. For window=5 that ceiling is exactly
// sqrt(4) = 2.0 -- so entry_z_score must be set comfortably below 2.0 for
// these tests to be meaningful (and not sit right at an unreachable,
// borderline-flaky threshold).

TEST(MeanReversion, GoesLongOnALargeDownwardOutlier) {
  MeanReversion strategy(5, 1.0, 10.0);
  // Mild variation around 100 to establish a small, well-defined stddev.
  for (double c : {99.0, 101.0, 100.0, 99.0, 101.0}) {
    strategy.on_bar(MakeBar(c));
  }
  // A huge drop relative to that tiny baseline variation pushes |z| close to
  // its ~2.0 ceiling -- comfortably past the 1.0 threshold either way.
  Signal signal = strategy.on_bar(MakeBar(20.0));
  EXPECT_DOUBLE_EQ(signal.target_position, 10.0);
}

TEST(MeanReversion, GoesShortOnALargeUpwardOutlier) {
  MeanReversion strategy(5, 1.0, 10.0);
  for (double c : {99.0, 101.0, 100.0, 99.0, 101.0}) {
    strategy.on_bar(MakeBar(c));
  }
  Signal signal = strategy.on_bar(MakeBar(500.0));
  EXPECT_DOUBLE_EQ(signal.target_position, -10.0);
}

TEST(MeanReversion, StaysFlatForDeviationsWithinTheThreshold) {
  MeanReversion strategy(5, 1.5, 10.0);
  for (double c : {99.0, 101.0, 100.0, 99.0, 101.0}) {
    strategy.on_bar(MakeBar(c));
  }
  // A small nudge (z ~= 0.27 given this window's own variability) stays
  // comfortably inside the 1.5-stddev band.
  Signal signal = strategy.on_bar(MakeBar(100.5));
  EXPECT_DOUBLE_EQ(signal.target_position, 0.0);
}
