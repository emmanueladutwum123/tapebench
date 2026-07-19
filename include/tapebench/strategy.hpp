#pragma once

#include <concepts>
#include <span>
#include <vector>

#include "tapebench/bar.hpp"

namespace tapebench {

// A strategy's decision after seeing one bar: the position it wants to be
// in (positive = long, negative = short, zero = flat), in the same units as
// tick/bar size. The simulation core (M4) is responsible for translating a
// *change* in target_position into an actual order -- a strategy just says
// where it wants to be, not how to get there.
struct Signal {
  double target_position = 0.0;
};

// Any type usable as a CRTP strategy implementation must provide these two
// methods. Enforced as a concept (not just duck-typing), so a strategy that
// gets the interface wrong fails to compile with a clear, specific message
// at the point of use, rather than a wall of template-instantiation errors.
template <typename T>
concept StrategyImpl = requires(T strategy, const Bar& bar) {
  { strategy.on_bar_impl(bar) } -> std::same_as<Signal>;
  { strategy.name_impl() } -> std::convertible_to<const char*>;
};

// CRTP base: Derived provides on_bar_impl()/name_impl(); Strategy<Derived>
// provides the public on_bar()/name() interface that dispatches to them.
// This is compile-time (static) polymorphism -- the dispatch inside on_bar()
// resolves to a direct, inlinable call to Derived::on_bar_impl at compile
// time. No vtable, no indirect call, unlike a virtual-function interface.
// See M7 for a measured comparison against virtual dispatch.
template <typename Derived>
class Strategy {
 public:
  Signal on_bar(const Bar& bar) {
    static_assert(StrategyImpl<Derived>,
                  "Derived must implement 'Signal on_bar_impl(const Bar&)' and "
                  "'const char* name_impl() const' to be used as a Strategy");
    return static_cast<Derived*>(this)->on_bar_impl(bar);
  }

  const char* name() const { return static_cast<const Derived*>(this)->name_impl(); }
};

// Generic driver: runs any CRTP strategy over a span of bars, in order,
// returning one Signal per bar. Works identically for every strategy type
// with zero virtual dispatch -- this is what "zero-overhead strategy
// dispatch" means in practice, not just a slogan (see the CRTP-vs-virtual
// benchmark in M7).
template <typename StrategyT>
  requires StrategyImpl<StrategyT>
std::vector<Signal> run_over_bars(Strategy<StrategyT>& strategy, std::span<const Bar> bars) {
  std::vector<Signal> signals;
  signals.reserve(bars.size());
  for (const auto& bar : bars) {
    signals.push_back(strategy.on_bar(bar));
  }
  return signals;
}

}  // namespace tapebench
