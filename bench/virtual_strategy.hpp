#pragma once

#include <memory>

#include "tapebench/strategy.hpp"

namespace tapebench::bench {

// A virtual-dispatch counterpart to the CRTP Strategy interface, existing
// purely to benchmark dispatch overhead against it -- not used anywhere in
// the production library.
class VirtualStrategyBase {
 public:
  virtual ~VirtualStrategyBase() = default;
  virtual Signal on_bar(const Bar& bar) = 0;
};

// Defined in a separate .cpp so the concrete type is opaque at any call site
// that only sees this factory declaration -- the compiler cannot devirtualize
// or inline through a pointer obtained this way. That's what makes the
// dispatch-overhead benchmark fair: a same-translation-unit virtual call is
// something the optimizer could see through and inline anyway, which would
// measure nothing.
std::unique_ptr<VirtualStrategyBase> make_virtual_constant_strategy();

}  // namespace tapebench::bench
