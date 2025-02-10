#include "rug_pull_detector.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <spdlog/spdlog.h>

// Initialize thread-local storage
thread_local std::vector<double> RugPullDetector::price_changes_;
thread_local std::vector<double> RugPullDetector::volume_changes_;

void RugPullDetector::addTrade(Trade &&trade) {
  std::unique_lock lock(data_mutex_);

  if (trades_.empty()) {
    analysis_start_time_ = trade.timestamp;
  }

  // Update peak if necessary (moved outside of insertion for better branch
  // prediction)
  if (trade.market_cap_sol > peak_mc_) {
    peak_mc_ = trade.market_cap_sol;
    peak_time_ = trade.timestamp;
  }

  trades_.push_back(std::move(trade));
}

std::span<const Trade> RugPullDetector::getRecentTrades(
    const std::chrono::system_clock::time_point &current_time) const {

  // Calculate elapsed time since analysis started
  auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                             current_time - analysis_start_time_)
                             .count();

  // Window starts at 10 seconds and expands up to max_detection_time (60
  // seconds)
  int window_size = std::min(std::max(10, static_cast<int>(elapsed_seconds)),
                             DetectionConfig::max_detection_time);

  const auto window_start = current_time - std::chrono::seconds(window_size);

  // Binary search for window start (better than linear search)
  auto start_it = std::lower_bound(trades_.begin(), trades_.end(), window_start,
                                   [](const Trade &trade, const auto &time) {
                                     return trade.timestamp < time;
                                   });

  auto end_it = std::upper_bound(start_it, trades_.end(), current_time,
                                 [](const auto &time, const Trade &trade) {
                                   return time < trade.timestamp;
                                 });

  return std::span<const Trade>{start_it, end_it};
}

RugPullDetector::WindowStats RugPullDetector::computeWindowStats(
    std::span<const Trade> window_trades) const {

  WindowStats stats;
  if (window_trades.size() <= 1)
    return stats;

  // Pre-size vectors to avoid reallocation
  price_changes_.clear();
  volume_changes_.clear();
  price_changes_.reserve(window_trades.size() - 1);
  volume_changes_.reserve(window_trades.size() - 1);

  // Compute changes in a single pass
  for (size_t i = 1; i < window_trades.size(); ++i) {
    price_changes_.push_back(window_trades[i].market_cap_sol -
                             window_trades[i - 1].market_cap_sol);
    volume_changes_.push_back(std::abs(window_trades[i].sol_amount -
                                       window_trades[i - 1].sol_amount));
  }

  // Optimize consecutive drops calculation using SIMD-friendly approach
  // This matches Python's behavior of counting consecutive negative price
  // changes
  stats.consecutive_drops = std::count_if(
      price_changes_.rbegin(),
      std::min(price_changes_.rbegin() + 3, price_changes_.rend()),
      [](double change) { return change < 0; });

  // Compute trends using vectorizable operations
  if (!volume_changes_.empty()) {
    stats.volume_trend =
        std::reduce(volume_changes_.begin(), volume_changes_.end()) /
        static_cast<double>(volume_changes_.size());
  }

  if (!price_changes_.empty()) {
    stats.price_velocity =
        std::reduce(price_changes_.begin(), price_changes_.end()) /
        static_cast<double>(price_changes_.size());
  }

  // Calculate pattern strength using explicit double version of cube root
  const double strength_base =
      (static_cast<double>(stats.consecutive_drops) / window_trades.size()) *
      (1.0 + std::min(stats.volume_trend, 2.0)) *
      (1.0 + std::abs(stats.price_velocity));

  stats.pattern_strength = std::pow(strength_base, 1.0 / 3.0);

  return stats;
}

double
RugPullDetector::calculateConfidence(double drop, double time_diff,
                                     double pattern, double volume,
                                     const DetectionConfig &config) const {

  double price_conf = static_cast<double>(drop >= config.peak_drop_threshold);
  double time_conf =
      std::max(0.0, 1.0 - time_diff / config.time_from_peak_threshold);
  double pattern_conf =
      static_cast<double>(pattern >= config.pattern_strength_threshold);
  double volume_conf =
      static_cast<double>(volume >= config.volume_spike_threshold);

  return 0.4 * price_conf * time_conf + 0.3 * pattern_conf + 0.3 * volume_conf;
}

DetectionResult RugPullDetector::processTrades(const DetectionConfig &config) {
  std::shared_lock lock(data_mutex_);

  DetectionResult result;
  if (trades_.empty())
    return result;

  try {
    while (current_idx_ < trades_.size()) {
      const auto &current_trade = trades_[current_idx_];
      auto window_trades = getRecentTrades(current_trade.timestamp);

      if (window_trades.empty()) {
        ++current_idx_;
        continue;
      }

      // Calculate time_since_peak once
      const auto time_since_peak =
          std::chrono::duration_cast<std::chrono::seconds>(
              current_trade.timestamp - peak_time_)
              .count();

      // Calculate current_drop once
      const double current_drop =
          (peak_mc_ > 0) ? (peak_mc_ - current_trade.market_cap_sol) / peak_mc_
                         : 0;

      // Fast path for stop loss check
      if (current_drop >= config.stop_loss_threshold) {
        return buildResult(true, current_trade.timestamp, "stop_loss",
                           {{"drop_pct", current_drop * 100},
                            {"peak_mc", peak_mc_},
                            {"current_mc", current_trade.market_cap_sol}});
      }

      if (window_trades.size() > 1) {
        const auto stats = computeWindowStats(window_trades);

        // Only calculate confidence score if time threshold is met
        if (time_since_peak >= 5) {
          const double confidence_score = calculateConfidence(
              current_drop, time_since_peak, stats.pattern_strength,
              stats.volume_trend, config);

          if (confidence_score >= config.min_confidence_score) {
            return buildResult(true, current_trade.timestamp, "pattern",
                               {{"confidence", confidence_score},
                                {"drop_pct", current_drop * 100},
                                {"peak_mc", peak_mc_},
                                {"current_mc", current_trade.market_cap_sol}});
          }
        }
      }
      ++current_idx_;
    }
  } catch (const std::exception &e) {
    spdlog::error("Error processing trades: {}", e.what());
  }

  return result;
}

DetectionResult RugPullDetector::buildResult(
    bool detected, const std::chrono::system_clock::time_point &timestamp,
    const std::string &trigger,
    const std::map<std::string, double> &metrics) const {

  DetectionResult result;
  result.rug_pulled = detected;
  result.timestamp = timestamp;
  result.debug_info.trigger_type = trigger;

  if (metrics.count("confidence")) {
    result.debug_info.confidence = metrics.at("confidence");
  }
  result.debug_info.drop_percentage = metrics.at("drop_pct");
  result.debug_info.peak_market_cap = metrics.at("peak_mc");
  result.debug_info.current_market_cap = metrics.at("current_mc");

  return result;
}
