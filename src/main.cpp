#include <iomanip>
#include <iostream>

#include "tapebench/synthetic_generator.hpp"

int main() {
  using namespace tapebench;

  SyntheticTickParams params;
  params.start_price = 100.0;
  params.volatility_per_sqrt_second = 0.02;
  params.mean_inter_arrival_us = 500.0;

  SyntheticTickGenerator gen(/*seed=*/42, params);
  auto ticks = gen.generate(20);

  std::cout << "tapebench M1 demo -- synthetic tick tape (seed=42, 20 ticks)\n\n";
  std::cout << std::fixed << std::setprecision(4);
  for (const auto& t : ticks) {
    std::cout << "t=" << t.timestamp_ns << "ns"
              << "  price=" << t.price << "  size=" << t.size
              << "  side=" << (t.side == Side::Buy ? "BUY" : "SELL") << "\n";
  }
  return 0;
}
