#include <gtest/gtest.h>

#include "tapebench/synthetic_generator.hpp"

using namespace tapebench;

namespace {
SyntheticTickParams DefaultParams() {
  SyntheticTickParams p;
  p.start_price = 100.0;
  p.volatility_per_sqrt_second = 0.02;
  p.mean_inter_arrival_us = 500.0;
  p.min_size = 1;
  p.max_size = 500;
  return p;
}
}  // namespace

TEST(SyntheticTickGenerator, SameSeedProducesIdenticalSequence) {
  SyntheticTickGenerator gen_a(42, DefaultParams());
  SyntheticTickGenerator gen_b(42, DefaultParams());

  auto ticks_a = gen_a.generate(200);
  auto ticks_b = gen_b.generate(200);

  ASSERT_EQ(ticks_a.size(), ticks_b.size());
  for (std::size_t i = 0; i < ticks_a.size(); ++i) {
    EXPECT_EQ(ticks_a[i].timestamp_ns, ticks_b[i].timestamp_ns) << "at index " << i;
    EXPECT_DOUBLE_EQ(ticks_a[i].price, ticks_b[i].price) << "at index " << i;
    EXPECT_EQ(ticks_a[i].size, ticks_b[i].size) << "at index " << i;
    EXPECT_EQ(ticks_a[i].side, ticks_b[i].side) << "at index " << i;
  }
}

TEST(SyntheticTickGenerator, DifferentSeedsProduceDifferentSequences) {
  SyntheticTickGenerator gen_a(1, DefaultParams());
  SyntheticTickGenerator gen_b(2, DefaultParams());

  auto ticks_a = gen_a.generate(50);
  auto ticks_b = gen_b.generate(50);

  int differences = 0;
  for (std::size_t i = 0; i < ticks_a.size(); ++i) {
    if (ticks_a[i].price != ticks_b[i].price) ++differences;
  }
  EXPECT_GT(differences, 0);
}

TEST(SyntheticTickGenerator, TimestampsAreStrictlyIncreasing) {
  SyntheticTickGenerator gen(7, DefaultParams());
  auto ticks = gen.generate(1000);

  for (std::size_t i = 1; i < ticks.size(); ++i) {
    EXPECT_GT(ticks[i].timestamp_ns, ticks[i - 1].timestamp_ns) << "at index " << i;
  }
}

TEST(SyntheticTickGenerator, PricesStayPositive) {
  SyntheticTickGenerator gen(7, DefaultParams());
  auto ticks = gen.generate(10000);

  for (const auto& t : ticks) {
    EXPECT_GT(t.price, 0.0);
  }
}

TEST(SyntheticTickGenerator, SizesStayWithinConfiguredBounds) {
  auto params = DefaultParams();
  params.min_size = 10;
  params.max_size = 20;
  SyntheticTickGenerator gen(7, params);
  auto ticks = gen.generate(500);

  for (const auto& t : ticks) {
    EXPECT_GE(t.size, params.min_size);
    EXPECT_LE(t.size, params.max_size);
  }
}

TEST(SyntheticTickGenerator, BothSidesAppearOverALargeSample) {
  SyntheticTickGenerator gen(7, DefaultParams());
  auto ticks = gen.generate(2000);

  bool saw_buy = false;
  bool saw_sell = false;
  for (const auto& t : ticks) {
    if (t.side == Side::Buy) saw_buy = true;
    if (t.side == Side::Sell) saw_sell = true;
  }
  EXPECT_TRUE(saw_buy);
  EXPECT_TRUE(saw_sell);
}

TEST(SyntheticTickGenerator, PositiveDriftPushesPriceUpOnAverage) {
  // Deterministic (fixed seed), wide tolerance -- this is a sanity check on
  // the GBM drift term, not a statistical-convergence proof like the sibling
  // quant-risk-engine repo's Monte Carlo tests.
  auto params = DefaultParams();
  params.drift_per_second = 5.0;              // strong upward drift
  params.volatility_per_sqrt_second = 0.005;  // low noise so drift dominates
  SyntheticTickGenerator gen(7, params);
  auto ticks = gen.generate(5000);

  EXPECT_GT(ticks.back().price, params.start_price);
}

TEST(Tick, IsTriviallyCopyable) {
  // Pinned because M2's memory-mapped tape I/O depends on this layout
  // guarantee -- a regression here would silently break zero-copy parsing.
  EXPECT_TRUE(std::is_trivially_copyable_v<Tick>);
}
