#pragma once

namespace tapebench {

// Tracks a single instrument's position using average-cost accounting:
// quantity (positive = long, negative = short, zero = flat), the
// volume-weighted average entry price of the current position, and P&L
// realized so far from fills that closed or reduced a position.
struct Position {
  double quantity = 0.0;
  double avg_entry_price = 0.0;
  double realized_pnl = 0.0;

  // Applies a fill of `fill_quantity` (signed: positive = buy, negative =
  // sell) at `fill_price`. A fill in the same direction as the current
  // position (or opening from flat) updates the weighted-average entry
  // price. A fill in the opposite direction realizes P&L on however much of
  // the existing position it closes; if it's larger than the existing
  // position, the excess opens a new position in the opposite direction at
  // `fill_price`. A zero-quantity fill is a no-op.
  void apply_fill(double fill_quantity, double fill_price);

  // Mark-to-market P&L on the current position at `mark_price`, not
  // included in realized_pnl until an actual fill closes it.
  double unrealized_pnl(double mark_price) const;
};

}  // namespace tapebench
