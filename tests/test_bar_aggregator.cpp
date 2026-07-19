#include <gtest/gtest.h>

#include "tapebench/bar_aggregator.hpp"

using namespace tapebench;

namespace {
Tick MakeTick(std::uint64_t ts_ns, double price, std::uint32_t size, Side side = Side::Buy) {
  return Tick{ts_ns, price, size, side};
}
}  // namespace

TEST(BarAggregator, EmptyInputProducesNoBars) {
  std::vector<Tick> ticks;
  auto bars = aggregate_bars(ticks, 1000);
  EXPECT_TRUE(bars.empty());
}

TEST(BarAggregator, ZeroBucketDurationProducesNoBars) {
  std::vector<Tick> ticks{MakeTick(0, 100.0, 10)};
  auto bars = aggregate_bars(ticks, 0);
  EXPECT_TRUE(bars.empty());
}

TEST(BarAggregator, SingleTickProducesOneBarEqualToThatTick) {
  std::vector<Tick> ticks{MakeTick(0, 100.0, 25)};
  auto bars = aggregate_bars(ticks, 1000);

  ASSERT_EQ(bars.size(), 1u);
  EXPECT_DOUBLE_EQ(bars[0].open, 100.0);
  EXPECT_DOUBLE_EQ(bars[0].high, 100.0);
  EXPECT_DOUBLE_EQ(bars[0].low, 100.0);
  EXPECT_DOUBLE_EQ(bars[0].close, 100.0);
  EXPECT_EQ(bars[0].volume, 25u);
  EXPECT_EQ(bars[0].trade_count, 1u);
}

TEST(BarAggregator, TicksWithinOneBucketMergeIntoOneBar) {
  // Bucket duration 1000ns, all four ticks land in [0, 1000).
  std::vector<Tick> ticks{
      MakeTick(0, 100.0, 10),
      MakeTick(200, 102.0, 5),
      MakeTick(500, 98.0, 20),
      MakeTick(900, 101.0, 15),
  };
  auto bars = aggregate_bars(ticks, 1000);

  ASSERT_EQ(bars.size(), 1u);
  EXPECT_DOUBLE_EQ(bars[0].open, 100.0);   // first tick
  EXPECT_DOUBLE_EQ(bars[0].high, 102.0);   // max of all four
  EXPECT_DOUBLE_EQ(bars[0].low, 98.0);     // min of all four
  EXPECT_DOUBLE_EQ(bars[0].close, 101.0);  // last tick
  EXPECT_EQ(bars[0].volume, 10u + 5u + 20u + 15u);
  EXPECT_EQ(bars[0].trade_count, 4u);
}

TEST(BarAggregator, TicksSpanningTwoBucketsProduceTwoBars) {
  std::vector<Tick> ticks{
      MakeTick(0, 100.0, 10),     // bucket 0: [0, 1000)
      MakeTick(500, 105.0, 20),   // bucket 0
      MakeTick(1000, 110.0, 30),  // bucket 1: [1000, 2000) -- boundary belongs to the new bucket
      MakeTick(1500, 108.0, 5),   // bucket 1
  };
  auto bars = aggregate_bars(ticks, 1000);

  ASSERT_EQ(bars.size(), 2u);

  EXPECT_EQ(bars[0].start_timestamp_ns, 0u);
  EXPECT_EQ(bars[0].end_timestamp_ns, 1000u);
  EXPECT_DOUBLE_EQ(bars[0].open, 100.0);
  EXPECT_DOUBLE_EQ(bars[0].close, 105.0);
  EXPECT_EQ(bars[0].trade_count, 2u);

  EXPECT_EQ(bars[1].start_timestamp_ns, 1000u);
  EXPECT_EQ(bars[1].end_timestamp_ns, 2000u);
  EXPECT_DOUBLE_EQ(bars[1].open, 110.0);
  EXPECT_DOUBLE_EQ(bars[1].high, 110.0);
  EXPECT_DOUBLE_EQ(bars[1].low, 108.0);
  EXPECT_DOUBLE_EQ(bars[1].close, 108.0);
  EXPECT_EQ(bars[1].trade_count, 2u);
}

TEST(BarAggregator, SkipsEmptyBucketsRatherThanEmittingThem) {
  // Bucket 0: one tick. Bucket 5 (5000-6000ns): one tick. Buckets 1-4 have
  // no ticks at all and must not appear as zero-volume bars in the output.
  std::vector<Tick> ticks{
      MakeTick(0, 100.0, 10),
      MakeTick(5200, 120.0, 40),
  };
  auto bars = aggregate_bars(ticks, 1000);

  ASSERT_EQ(bars.size(), 2u);
  EXPECT_EQ(bars[0].start_timestamp_ns, 0u);
  EXPECT_EQ(bars[1].start_timestamp_ns, 5000u);
}

TEST(BarAggregator, TotalTradeCountAcrossAllBarsEqualsInputSize) {
  std::vector<Tick> ticks;
  for (std::uint64_t i = 0; i < 250; ++i) {
    ticks.push_back(MakeTick(i * 37, 100.0 + static_cast<double>(i) * 0.01, 1));
  }
  auto bars = aggregate_bars(ticks, 1000);

  std::uint32_t total = 0;
  for (const auto& bar : bars) {
    total += bar.trade_count;
  }
  EXPECT_EQ(total, ticks.size());
}
