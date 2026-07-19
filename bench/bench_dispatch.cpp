// CRTP (compile-time/static dispatch) vs. virtual (runtime dispatch)
// overhead, isolated: both strategies do exactly the same trivial work
// (see the class definitions), so any measured difference is purely the
// cost of the dispatch mechanism itself, not algorithmic complexity.
//
// The virtual variant is obtained through an opaque factory defined in a
// separate translation unit (virtual_strategy.cpp) specifically so the
// compiler cannot devirtualize or inline it here -- that's what makes this
// a fair measurement of what virtual dispatch actually costs in a real
// plugin-style architecture, rather than something the optimizer erases.
#include <benchmark/benchmark.h>

#include "tapebench/strategy.hpp"
#include "virtual_strategy.hpp"

using namespace tapebench;
using tapebench::bench::make_virtual_constant_strategy;
using tapebench::bench::VirtualStrategyBase;

namespace {

// Does exactly the same trivial computation as VirtualConstantStrategy (see
// virtual_strategy.cpp).
class CrtpConstantStrategy : public Strategy<CrtpConstantStrategy> {
 public:
  Signal on_bar_impl(const Bar& bar) { return Signal{bar.close > 0.0 ? 1.0 : 0.0}; }
  const char* name_impl() const { return "CrtpConstant"; }
};

Bar MakeBar() { return Bar{0, 1, 100.0, 100.0, 100.0, 100.0, 1, 1}; }

}  // namespace

void BM_CrtpDispatch(benchmark::State& state) {
  CrtpConstantStrategy strategy;
  Bar bar = MakeBar();
  for (auto _ : state) {
    Signal s = strategy.on_bar(bar);
    benchmark::DoNotOptimize(s);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CrtpDispatch);

void BM_VirtualDispatch(benchmark::State& state) {
  std::unique_ptr<VirtualStrategyBase> strategy = make_virtual_constant_strategy();
  Bar bar = MakeBar();
  for (auto _ : state) {
    Signal s = strategy->on_bar(bar);
    benchmark::DoNotOptimize(s);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_VirtualDispatch);

BENCHMARK_MAIN();
