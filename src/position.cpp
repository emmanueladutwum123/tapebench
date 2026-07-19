#include "tapebench/position.hpp"

#include <algorithm>
#include <cmath>

namespace tapebench {

void Position::apply_fill(double fill_quantity, double fill_price) {
  if (fill_quantity == 0.0) {
    return;
  }

  const bool opening_or_same_direction = (quantity == 0.0) || ((quantity > 0.0) == (fill_quantity > 0.0));

  if (opening_or_same_direction) {
    const double new_quantity = quantity + fill_quantity;
    avg_entry_price = (quantity * avg_entry_price + fill_quantity * fill_price) / new_quantity;
    quantity = new_quantity;
    return;
  }

  // Opposite direction: this fill reduces, closes, or reverses the position.
  const double closing_quantity = std::min(std::abs(fill_quantity), std::abs(quantity));
  const double pnl_per_unit = (quantity > 0.0) ? (fill_price - avg_entry_price) : (avg_entry_price - fill_price);
  realized_pnl += pnl_per_unit * closing_quantity;

  const double remaining_fill = std::abs(fill_quantity) - closing_quantity;
  quantity += fill_quantity;
  if (remaining_fill > 0.0) {
    // The fill was bigger than the existing position: it fully closed the
    // old position and opened a new one, in the opposite direction, at
    // fill_price.
    avg_entry_price = fill_price;
  }
}

double Position::unrealized_pnl(double mark_price) const { return quantity * (mark_price - avg_entry_price); }

}  // namespace tapebench
