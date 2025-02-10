#pragma once
#include <chrono>

struct DetectionConfig {
  // Change to constexpr for compile-time optimization
  static constexpr double peak_drop_threshold = 0.100; // 10% drop
  static constexpr int time_from_peak_threshold = 105; // seconds
  static constexpr double volume_spike_threshold = 1.244;
  static constexpr double min_confidence_score = 0.672;
  static constexpr double early_warning_threshold = 0.050;
  static constexpr int consecutive_drops_threshold = 2;
  static constexpr double price_velocity_threshold = 0.012;
  static constexpr double pattern_strength_threshold = 0.457;
  static constexpr int short_window = 1;
  static constexpr double stop_loss_threshold = 0.40; // 40% drop
  static constexpr int max_detection_time = 60;
};
