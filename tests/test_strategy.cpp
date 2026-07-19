#include <gtest/gtest.h>

#include "tapebench/strategy.hpp"

using namespace tapebench;

namespace {

// Minimal CRTP strategy for testing the interface mechanics in isolation
// from any real trading logic: always returns a fixed target position.
class ConstantStrategy : public Strategy<ConstantStrategy> {
 public:
  explicit ConstantStrategy(double position) : position_(position) {}

  Signal on_bar_impl(const Bar&) { return Signal{position_}; }
  const char* name_impl() const { return "Constant"; }

 private:
  double position_;
};

// A second, differently-shaped strategy: counts how many bars it has seen
// and returns that count as the position. Exists to prove run_over_bars is
// genuinely generic over any StrategyImpl, not just tailored to one shape.
class BarCountingStrategy : public Strategy<BarCountingStrategy> {
 public:
  Signal on_bar_impl(const Bar&) {
    ++count_;
    return Signal{static_cast<double>(count_)};
  }
  const char* name_impl() const { return "BarCounting"; }

 private:
  int count_ = 0;
};

Bar MakeBar(double close) {
  return Bar{0, 1, close, close, close, close, 1, 1};
}

}  // namespace

TEST(Strategy, OnBarDispatchesToTheDerivedImplementation) {
  ConstantStrategy strategy(42.0);
  Signal signal = strategy.on_bar(MakeBar(100.0));
  EXPECT_DOUBLE_EQ(signal.target_position, 42.0);
}

TEST(Strategy, NameDispatchesToTheDerivedImplementation) {
  ConstantStrategy strategy(1.0);
  EXPECT_STREQ(strategy.name(), "Constant");
}

TEST(Strategy, RunOverBarsIsGenericAcrossDifferentStrategyShapes) {
  std::vector<Bar> bars{MakeBar(1), MakeBar(2), MakeBar(3)};

  ConstantStrategy constant(7.0);
  auto constant_signals = run_over_bars(constant, bars);
  ASSERT_EQ(constant_signals.size(), 3u);
  for (const auto& s : constant_signals) {
    EXPECT_DOUBLE_EQ(s.target_position, 7.0);
  }

  BarCountingStrategy counting;
  auto counting_signals = run_over_bars(counting, bars);
  ASSERT_EQ(counting_signals.size(), 3u);
  EXPECT_DOUBLE_EQ(counting_signals[0].target_position, 1.0);
  EXPECT_DOUBLE_EQ(counting_signals[1].target_position, 2.0);
  EXPECT_DOUBLE_EQ(counting_signals[2].target_position, 3.0);
}

TEST(Strategy, RunOverBarsOnEmptyInputReturnsEmptyOutput) {
  ConstantStrategy strategy(5.0);
  std::vector<Bar> no_bars;
  auto signals = run_over_bars(strategy, no_bars);
  EXPECT_TRUE(signals.empty());
}
