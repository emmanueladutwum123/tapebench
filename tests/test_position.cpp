#include <gtest/gtest.h>

#include "tapebench/position.hpp"

using namespace tapebench;

TEST(Position, OpeningFromFlatSetsAvgEntryPriceToFillPrice) {
  Position position;
  position.apply_fill(10.0, 100.0);
  EXPECT_DOUBLE_EQ(position.quantity, 10.0);
  EXPECT_DOUBLE_EQ(position.avg_entry_price, 100.0);
  EXPECT_DOUBLE_EQ(position.realized_pnl, 0.0);
}

TEST(Position, AddingToLongUpdatesWeightedAverageEntryPrice) {
  Position position;
  position.apply_fill(10.0, 100.0);
  position.apply_fill(5.0, 110.0);
  EXPECT_DOUBLE_EQ(position.quantity, 15.0);
  EXPECT_DOUBLE_EQ(position.avg_entry_price, (10.0 * 100.0 + 5.0 * 110.0) / 15.0);
  EXPECT_DOUBLE_EQ(position.realized_pnl, 0.0);
}

TEST(Position, PartiallyClosingLongRealizesProportionalPnLAndKeepsAvgEntryPrice) {
  Position position;
  position.apply_fill(10.0, 100.0);
  position.apply_fill(-4.0, 120.0);
  EXPECT_DOUBLE_EQ(position.quantity, 6.0);
  EXPECT_DOUBLE_EQ(position.avg_entry_price, 100.0);  // unchanged: remaining position keeps its cost basis
  EXPECT_DOUBLE_EQ(position.realized_pnl, 80.0);       // (120-100) * 4
}

TEST(Position, FullyClosingLongRealizesAllPnLAndZeroesQuantity) {
  Position position;
  position.apply_fill(10.0, 100.0);
  position.apply_fill(-10.0, 130.0);
  EXPECT_DOUBLE_EQ(position.quantity, 0.0);
  EXPECT_DOUBLE_EQ(position.realized_pnl, 300.0);  // (130-100) * 10
}

TEST(Position, ReversingLongToShortRealizesPnLOnClosedPortionAndOpensNewAvgPrice) {
  Position position;
  position.apply_fill(10.0, 100.0);
  position.apply_fill(-15.0, 130.0);
  EXPECT_DOUBLE_EQ(position.quantity, -5.0);
  EXPECT_DOUBLE_EQ(position.avg_entry_price, 130.0);  // new short position's own cost basis
  EXPECT_DOUBLE_EQ(position.realized_pnl, 300.0);      // (130-100) * 10, on the closed long portion only
}

TEST(Position, OpeningShortAndPartiallyCoveringAtAProfitRealizesPositivePnL) {
  Position position;
  position.apply_fill(-10.0, 100.0);  // open short
  position.apply_fill(4.0, 90.0);     // buy back 4 at a lower price -> profit
  EXPECT_DOUBLE_EQ(position.quantity, -6.0);
  EXPECT_DOUBLE_EQ(position.avg_entry_price, 100.0);
  EXPECT_DOUBLE_EQ(position.realized_pnl, 40.0);  // (100-90) * 4
}

TEST(Position, UnrealizedPnlIsPositiveForALongPositionWhenPriceRises) {
  Position position;
  position.apply_fill(10.0, 100.0);
  EXPECT_DOUBLE_EQ(position.unrealized_pnl(120.0), 200.0);
}

TEST(Position, UnrealizedPnlIsPositiveForAShortPositionWhenPriceFalls) {
  Position position;
  position.apply_fill(-10.0, 100.0);
  EXPECT_DOUBLE_EQ(position.unrealized_pnl(80.0), 200.0);
}

TEST(Position, ApplyingAZeroQuantityFillIsANoOp) {
  Position position;
  position.apply_fill(10.0, 100.0);
  position.apply_fill(0.0, 999.0);
  EXPECT_DOUBLE_EQ(position.quantity, 10.0);
  EXPECT_DOUBLE_EQ(position.avg_entry_price, 100.0);
  EXPECT_DOUBLE_EQ(position.realized_pnl, 0.0);
}
