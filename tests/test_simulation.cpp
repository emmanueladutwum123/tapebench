#include <gtest/gtest.h>

#include <algorithm>

#include "tapebench/simulation.hpp"

using namespace tapebench;

namespace {

Bar MakeBar(std::uint64_t start_ns, std::uint64_t end_ns, double open, double close) {
  const double high = std::max(open, close);
  const double low = std::min(open, close);
  return Bar{start_ns, end_ns, open, high, low, close, 1, 1};
}

class FixedTargetStrategy : public Strategy<FixedTargetStrategy> {
 public:
  explicit FixedTargetStrategy(double target) : target_(target) {}
  Signal on_bar_impl(const Bar&) { return Signal{target_}; }
  const char* name_impl() const { return "FixedTarget"; }

 private:
  double target_;
};

// Flat for its first call, then a fixed long target from the second call
// onward -- makes the one-bar execution lag unambiguous to test.
class SwitchStrategy : public Strategy<SwitchStrategy> {
 public:
  Signal on_bar_impl(const Bar&) {
    ++calls_;
    return Signal{calls_ >= 2 ? 5.0 : 0.0};
  }
  const char* name_impl() const { return "Switch"; }

 private:
  int calls_ = 0;
};

}  // namespace

TEST(Simulation, EquityCurveHasOnePointPerBar) {
  FixedTargetStrategy strategy(1.0);
  std::vector<Bar> bars{MakeBar(0, 1, 100, 100), MakeBar(1, 2, 100, 100), MakeBar(2, 3, 100, 100)};
  ExecutionModel execution{0.0};

  auto result = simulate(strategy, bars, execution);
  EXPECT_EQ(result.equity_curve.size(), bars.size());
}

TEST(Simulation, NoLookAheadFirstBarsSignalOnlyFillsOnTheSecondBar) {
  // FixedTargetStrategy always wants target=10, from bar 0 onward. A naive
  // (buggy) backtest might fill that on bar 0 itself, using information
  // (bar 0's own close) that wasn't available at bar 0's open. The correct
  // behavior: still flat through bar 0, filled starting at bar 1's open.
  FixedTargetStrategy strategy(10.0);
  std::vector<Bar> bars{
      MakeBar(0, 1, 100.0, 100.0),
      MakeBar(1, 2, 105.0, 110.0),
      MakeBar(2, 3, 115.0, 120.0),
  };
  ExecutionModel execution{0.0};

  auto result = simulate(strategy, bars, execution);

  ASSERT_EQ(result.equity_curve.size(), 3u);

  // Bar 0: no prior signal to act on yet -> still flat.
  EXPECT_DOUBLE_EQ(result.equity_curve[0].position, 0.0);
  EXPECT_DOUBLE_EQ(result.equity_curve[0].equity, 0.0);

  // Bar 1: bar 0's signal (target=10) fills at bar 1's open (105), then
  // marked at bar 1's close (110): unrealized = 10 * (110-105) = 50.
  EXPECT_DOUBLE_EQ(result.equity_curve[1].position, 10.0);
  EXPECT_DOUBLE_EQ(result.equity_curve[1].equity, 50.0);

  // Bar 2: target unchanged (still 10) -> no new fill. Marked at bar 2's
  // close (120): unrealized = 10 * (120-105) = 150.
  EXPECT_DOUBLE_EQ(result.equity_curve[2].position, 10.0);
  EXPECT_DOUBLE_EQ(result.equity_curve[2].equity, 150.0);

  EXPECT_EQ(result.fill_count, 1);
  EXPECT_DOUBLE_EQ(result.final_position.avg_entry_price, 105.0);
  EXPECT_DOUBLE_EQ(result.final_position.realized_pnl, 0.0);
}

TEST(Simulation, SignalFromBarNIsExecutedAtBarNPlus1Open) {
  SwitchStrategy strategy;
  std::vector<Bar> bars{
      MakeBar(0, 1, 50.0, 50.0),  // signal computed here (call 1): flat
      MakeBar(1, 2, 60.0, 60.0),  // signal computed here (call 2): long 5 -- not filled this bar
      MakeBar(2, 3, 70.0, 70.0),  // bar 1's long-5 signal fills HERE, at this bar's open (70)
  };
  ExecutionModel execution{0.0};

  auto result = simulate(strategy, bars, execution);

  EXPECT_DOUBLE_EQ(result.equity_curve[0].position, 0.0);
  EXPECT_DOUBLE_EQ(result.equity_curve[1].position, 0.0);  // still flat: bar 1's signal not actioned yet
  EXPECT_DOUBLE_EQ(result.equity_curve[2].position, 5.0);  // filled at bar 2's open
  EXPECT_DOUBLE_EQ(result.final_position.avg_entry_price, 70.0);
}

TEST(Simulation, SlippageWorsensTheFillAndReducesEquityRelativeToNoSlippage) {
  FixedTargetStrategy strategy(10.0);
  std::vector<Bar> bars{MakeBar(0, 1, 100.0, 100.0), MakeBar(1, 2, 100.0, 100.0)};
  ExecutionModel no_slippage{0.0};
  ExecutionModel with_slippage{100.0};  // 100 bps = 1%

  auto clean = simulate(strategy, bars, no_slippage);
  auto slipped = simulate(strategy, bars, with_slippage);

  EXPECT_DOUBLE_EQ(clean.final_position.avg_entry_price, 100.0);
  EXPECT_DOUBLE_EQ(slipped.final_position.avg_entry_price, 101.0);  // 1% worse for a buy
  EXPECT_LT(slipped.equity_curve.back().equity, clean.equity_curve.back().equity);
}

TEST(Simulation, RunningOverAnEmptyBarSpanProducesAnEmptyResult) {
  FixedTargetStrategy strategy(1.0);
  std::vector<Bar> no_bars;
  ExecutionModel execution{0.0};

  auto result = simulate(strategy, no_bars, execution);
  EXPECT_TRUE(result.equity_curve.empty());
  EXPECT_EQ(result.fill_count, 0);
  EXPECT_DOUBLE_EQ(result.final_position.quantity, 0.0);
}
