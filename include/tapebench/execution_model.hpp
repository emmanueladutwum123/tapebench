#pragma once

namespace tapebench {

// The simplest realistic fill model: a fixed adverse-price cost, in basis
// points, applied against a reference price -- buys fill worse (higher),
// sells fill worse (lower). Meant to stand in for bid-ask spread / market
// impact without modeling either explicitly.
struct ExecutionModel {
  double slippage_bps = 0.0;

  // Returns the fill price for a trade of `signed_quantity` (positive =
  // buy, negative = sell, zero = no trade) against `reference_price`.
  double fill_price(double reference_price, double signed_quantity) const {
    const double direction = (signed_quantity > 0.0) ? 1.0 : ((signed_quantity < 0.0) ? -1.0 : 0.0);
    return reference_price * (1.0 + direction * slippage_bps / 10000.0);
  }
};

}  // namespace tapebench
