#include <gtest/gtest.h>

#include <cmath>

#include "tapebench/analytics.hpp"

using namespace tapebench;

namespace {
SimulationResult MakeResultFromEquity(const std::vector<double>& equity_values) {
  SimulationResult result;
  std::uint64_t ts = 0;
  for (double equity : equity_values) {
    result.equity_curve.push_back(EquityPoint{ts, equity, 0.0, equity, 0.0});
    ++ts;
  }
  return result;
}
}  // namespace

TEST(Analytics, EmptyEquityCurveProducesAnAllZeroReport) {
  SimulationResult result;
  auto report = analyze(result);

  EXPECT_DOUBLE_EQ(report.total_pnl, 0.0);
  EXPECT_DOUBLE_EQ(report.sharpe_ratio, 0.0);
  EXPECT_DOUBLE_EQ(report.sortino_ratio, 0.0);
  EXPECT_DOUBLE_EQ(report.max_drawdown, 0.0);
  EXPECT_EQ(report.num_bars, 0);
}

TEST(Analytics, SingleBarEquityCurveHasZeroSharpeAndZeroDrawdown) {
  // With one sample, sample variance is 0 (there's nothing to vary against),
  // so Sharpe/Sortino are guarded to 0 rather than dividing by zero.
  auto result = MakeResultFromEquity({42.0});
  auto report = analyze(result);

  EXPECT_DOUBLE_EQ(report.total_pnl, 42.0);
  EXPECT_DOUBLE_EQ(report.sharpe_ratio, 0.0);
  EXPECT_DOUBLE_EQ(report.max_drawdown, 0.0);
  EXPECT_EQ(report.num_bars, 1);
}

TEST(Analytics, ComputesSharpeAndSortinoAgainstAnIndependentReferenceCalculation) {
  // Equity: 10, 30, 20 -> bar-over-bar PnL changes: 10, 20, -10.
  auto result = MakeResultFromEquity({10.0, 30.0, 20.0});
  auto report = analyze(result);

  // Reference calculation, written independently of analyze()'s own code
  // path, directly from the definitions of mean/population-stddev/semi-
  // deviation -- not just re-invoking the function under test.
  const double mean = (10.0 + 20.0 + (-10.0)) / 3.0;
  const double variance =
      ((10.0 - mean) * (10.0 - mean) + (20.0 - mean) * (20.0 - mean) + (-10.0 - mean) * (-10.0 - mean)) / 3.0;
  const double stddev = std::sqrt(variance);
  const double expected_sharpe = mean / stddev;
  const double downside_stddev = std::sqrt(((-10.0) * (-10.0)) / 1.0);  // only one downside sample: -10
  const double expected_sortino = mean / downside_stddev;

  EXPECT_NEAR(report.sharpe_ratio, expected_sharpe, 1e-9);
  EXPECT_NEAR(report.sortino_ratio, expected_sortino, 1e-9);
  EXPECT_DOUBLE_EQ(report.total_pnl, 20.0);
}

TEST(Analytics, AnnualizationScalesSharpeAndSortinoBySqrtBarsPerYear) {
  auto result = MakeResultFromEquity({10.0, 30.0, 20.0});
  auto unannualized = analyze(result, 0.0);
  auto annualized = analyze(result, 252.0);

  EXPECT_NEAR(annualized.sharpe_ratio, unannualized.sharpe_ratio * std::sqrt(252.0), 1e-9);
  EXPECT_NEAR(annualized.sortino_ratio, unannualized.sortino_ratio * std::sqrt(252.0), 1e-9);
}

TEST(Analytics, SortinoIsZeroWhenThereAreNoDownsidePeriods) {
  // Monotonically increasing equity -> every bar-over-bar change is
  // positive -> no downside samples to compute a semi-deviation from.
  auto result = MakeResultFromEquity({10.0, 20.0, 35.0, 50.0});
  auto report = analyze(result);

  EXPECT_DOUBLE_EQ(report.sortino_ratio, 0.0);
  EXPECT_GT(report.sharpe_ratio, 0.0);  // there IS variance in the changes, just no downside ones
}

TEST(Analytics, MaxDrawdownFindsTheLargestPeakToTroughDeclineNotJustTheLastOne) {
  // Peak 150 at index 1; the biggest decline from any peak-so-far to a
  // later trough is 150 -> 60 = 90, which happens well before the curve
  // recovers to a new peak of 200 at the end.
  auto result = MakeResultFromEquity({100.0, 150.0, 80.0, 120.0, 60.0, 200.0});
  auto report = analyze(result);

  EXPECT_DOUBLE_EQ(report.max_drawdown, 90.0);
}

TEST(Analytics, PassesThroughFillCountAndTradedQuantityFromTheSimulationResult) {
  SimulationResult result = MakeResultFromEquity({10.0, 20.0});
  result.fill_count = 3;
  result.total_traded_quantity = 17.5;

  auto report = analyze(result);
  EXPECT_EQ(report.fill_count, 3);
  EXPECT_DOUBLE_EQ(report.total_traded_quantity, 17.5);
}
