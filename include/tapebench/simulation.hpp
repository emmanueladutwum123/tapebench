#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "tapebench/bar.hpp"
#include "tapebench/execution_model.hpp"
#include "tapebench/position.hpp"
#include "tapebench/strategy.hpp"

namespace tapebench {

struct EquityPoint {
  std::uint64_t timestamp_ns;
  double equity;  // realized_pnl + unrealized_pnl at this point
  double position;
  double realized_pnl;
  double unrealized_pnl;
};

struct SimulationResult {
  std::vector<EquityPoint> equity_curve;  // one point per bar
  Position final_position;
  int fill_count = 0;
};

// Runs a strategy over a tape of bars with a no-look-ahead execution model:
// a signal computed from bar N's close is only actionable starting at bar
// N+1's open (real orders can't fill on information not yet available when
// the bar closed). Concretely, for each bar:
//   1. Execute any order queued from the PREVIOUS bar's signal, at THIS
//      bar's open (through `execution`, which applies slippage).
//   2. Mark the resulting position to market at THIS bar's close and record
//      an EquityPoint.
//   3. Compute this bar's signal and queue it for the NEXT bar's open.
// The very last bar's signal is computed but never executed (there is no
// following bar to fill it on) -- this mirrors how a live backtest ends
// with one still-pending, unfilled decision.
template <typename StrategyT>
SimulationResult simulate(Strategy<StrategyT>& strategy, std::span<const Bar> bars, const ExecutionModel& execution) {
  SimulationResult result;
  result.equity_curve.reserve(bars.size());

  double pending_target = 0.0;
  bool have_pending = false;

  for (const auto& bar : bars) {
    if (have_pending) {
      const double delta = pending_target - result.final_position.quantity;
      if (delta != 0.0) {
        const double price = execution.fill_price(bar.open, delta);
        result.final_position.apply_fill(delta, price);
        ++result.fill_count;
      }
      have_pending = false;
    }

    const double unrealized = result.final_position.unrealized_pnl(bar.close);
    result.equity_curve.push_back(EquityPoint{
        bar.end_timestamp_ns,
        result.final_position.realized_pnl + unrealized,
        result.final_position.quantity,
        result.final_position.realized_pnl,
        unrealized,
    });

    const Signal signal = strategy.on_bar(bar);
    pending_target = signal.target_position;
    have_pending = true;
  }

  return result;
}

}  // namespace tapebench
