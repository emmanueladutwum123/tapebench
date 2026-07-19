#include <gtest/gtest.h>

#include "tapebench/execution_model.hpp"

using namespace tapebench;

TEST(ExecutionModel, ZeroSlippageReturnsReferencePriceExactlyForABuy) {
  ExecutionModel execution{0.0};
  EXPECT_DOUBLE_EQ(execution.fill_price(100.0, 10.0), 100.0);
}

TEST(ExecutionModel, ZeroSlippageReturnsReferencePriceExactlyForASell) {
  ExecutionModel execution{0.0};
  EXPECT_DOUBLE_EQ(execution.fill_price(100.0, -10.0), 100.0);
}

TEST(ExecutionModel, PositiveSlippageWorsensABuyPrice) {
  ExecutionModel execution{50.0};  // 50 bps = 0.5%
  EXPECT_DOUBLE_EQ(execution.fill_price(100.0, 10.0), 100.5);
}

TEST(ExecutionModel, PositiveSlippageWorsensASellPrice) {
  ExecutionModel execution{50.0};
  EXPECT_DOUBLE_EQ(execution.fill_price(100.0, -10.0), 99.5);
}

TEST(ExecutionModel, ZeroQuantityReturnsReferencePriceUnchangedEvenWithSlippageConfigured) {
  ExecutionModel execution{50.0};
  EXPECT_DOUBLE_EQ(execution.fill_price(100.0, 0.0), 100.0);
}
