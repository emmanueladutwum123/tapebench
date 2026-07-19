#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "tapebench/tick.hpp"

namespace tapebench {

// Parameters for the synthetic tape. Defaults are plausible but not
// calibrated to real data -- see README for why: real tick data isn't
// reliably licensable/scriptable for CI, so every milestone in this repo is
// built and tested against this generator, with any real-data runs
// documented separately and honestly (same approach as the sibling
// liquibook-x and quant-risk-engine repos).
struct SyntheticTickParams {
  double start_price = 100.0;
  double drift_per_second = 0.0;              // GBM mu, applied per second of sim time
  double volatility_per_sqrt_second = 0.02;   // GBM sigma, per sqrt(second)
  double mean_inter_arrival_us = 500.0;       // average microseconds between ticks
  std::uint32_t min_size = 1;
  std::uint32_t max_size = 500;
};

// Deterministic, seeded generator for a synthetic trade tape: a GBM-style
// price path with Poisson-process inter-arrival times, log-normal-ish trade
// sizes, and an aggressor side biased toward the direction of the last price
// move. Same seed + same params always produces the exact same tape -- every
// later milestone's tests depend on this determinism.
class SyntheticTickGenerator {
 public:
  SyntheticTickGenerator(std::uint64_t seed, SyntheticTickParams params);

  // Generates the next tick in the sequence.
  Tick next();

  // Generates `count` ticks at once.
  std::vector<Tick> generate(std::size_t count);

 private:
  SyntheticTickParams params_;
  std::mt19937_64 rng_;
  std::uint64_t elapsed_ns_ = 0;
  double last_price_;
};

}  // namespace tapebench
