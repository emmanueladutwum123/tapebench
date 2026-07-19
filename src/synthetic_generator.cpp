#include "tapebench/synthetic_generator.hpp"

#include <cmath>

namespace tapebench {

SyntheticTickGenerator::SyntheticTickGenerator(std::uint64_t seed, SyntheticTickParams params)
    : params_(params), rng_(seed), last_price_(params.start_price) {}

Tick SyntheticTickGenerator::next() {
  // Poisson-process inter-arrival time: exponential gaps between ticks.
  std::exponential_distribution<double> arrival_dist(1.0 / params_.mean_inter_arrival_us);
  double gap_us = arrival_dist(rng_);
  elapsed_ns_ += static_cast<std::uint64_t>(gap_us * 1000.0);

  double dt_seconds = gap_us / 1'000'000.0;

  // One GBM step over dt_seconds:
  //   S_{t+dt} = S_t * exp((mu - 0.5*sigma^2)*dt + sigma*sqrt(dt)*Z)
  std::normal_distribution<double> normal(0.0, 1.0);
  double z = normal(rng_);
  double sigma = params_.volatility_per_sqrt_second;
  double mu = params_.drift_per_second;
  double log_return = (mu - 0.5 * sigma * sigma) * dt_seconds + sigma * std::sqrt(dt_seconds) * z;
  double new_price = last_price_ * std::exp(log_return);

  std::uniform_int_distribution<std::uint32_t> size_dist(params_.min_size, params_.max_size);
  std::uint32_t size = size_dist(rng_);

  // Mildly realistic order flow: an up-tick is more likely to be a buy
  // aggressor, a down-tick more likely a sell aggressor, with noise either way.
  double buy_probability = new_price >= last_price_ ? 0.65 : 0.35;
  std::bernoulli_distribution side_dist(buy_probability);
  Side side = side_dist(rng_) ? Side::Buy : Side::Sell;

  last_price_ = new_price;

  return Tick{elapsed_ns_, new_price, size, side};
}

std::vector<Tick> SyntheticTickGenerator::generate(std::size_t count) {
  std::vector<Tick> ticks;
  ticks.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    ticks.push_back(next());
  }
  return ticks;
}

}  // namespace tapebench
