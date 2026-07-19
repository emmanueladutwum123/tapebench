#pragma once

#include "tapebench/simulation.hpp"

namespace tapebench {

struct PerformanceReport {
  double total_pnl = 0.0;              // final equity (realized + unrealized)
  double sharpe_ratio = 0.0;           // mean(bar-over-bar PnL change) / stddev(...), annualized if requested
  double sortino_ratio = 0.0;          // same, but the denominator is downside-only semi-deviation
  double max_drawdown = 0.0;           // largest peak-to-trough decline in equity, as a positive number
  double total_traded_quantity = 0.0;  // sum of |fill size| across every fill (turnover)
  int fill_count = 0;
  int num_bars = 0;
};

// Computes standard performance metrics from a simulation's equity curve.
//
// Sharpe/Sortino here are computed on bar-over-bar *absolute PnL changes*,
// not percentage returns -- this engine has no notion of notional capital
// or leverage, so there is no principled "return %" to divide by. This is a
// real, deliberate simplification (sometimes called "Sharpe of PnL"), not a
// hidden one; see README for the caveat this implies when comparing across
// strategies with very different position sizes.
//
// `bars_per_year` is the annualization factor (how many bars of this
// duration occur in a year, e.g. ~1,576,800 for 20-second bars); pass 0 to
// report unannualized, per-bar ratios instead.
PerformanceReport analyze(const SimulationResult& result, double bars_per_year = 0.0);

}  // namespace tapebench
