#include "virtual_strategy.hpp"

namespace tapebench::bench {

namespace {

// Does exactly the same trivial computation as CrtpConstantStrategy (see
// bench_dispatch.cpp) -- the two benchmarks must differ ONLY in dispatch
// mechanism, not in the work being dispatched to.
class VirtualConstantStrategy : public VirtualStrategyBase {
 public:
  Signal on_bar(const Bar& bar) override { return Signal{bar.close > 0.0 ? 1.0 : 0.0}; }
};

}  // namespace

std::unique_ptr<VirtualStrategyBase> make_virtual_constant_strategy() {
  return std::make_unique<VirtualConstantStrategy>();
}

}  // namespace tapebench::bench
