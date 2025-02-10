#pragma once
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <deque>
#include <memory>
#include <span>
#include <map>
#include "detection_config.hpp"
#include "trade.hpp"
#include "detection_result.hpp"

// Pre-allocated buffer size for trades
constexpr size_t INITIAL_TRADE_BUFFER = 1024;

class RugPullDetector {
public:
    explicit RugPullDetector() 
        : data_mutex_()
        , trades_()
        , peak_mc_(0.0)
        , peak_time_(std::chrono::system_clock::now())
        , analysis_start_time_(peak_time_)
        , current_idx_(0) {
        trades_.reserve(INITIAL_TRADE_BUFFER);
    }

    // Public interface
    void addTrade(Trade&& trade);
    DetectionResult processTrades(const DetectionConfig& config);

private:
    // Cache frequently computed values
    struct WindowStats {
        double volume_trend;        // Equivalent to volume_changes.mean()
        double price_velocity;      // Equivalent to price_changes.mean()
        int consecutive_drops;      // Equivalent to sum(price_drops[-3:])
        double pattern_strength;    // Pattern strength calculation

        WindowStats() 
            : volume_trend(0)
            , price_velocity(0)
            , consecutive_drops(0)
            , pattern_strength(0) {}
    };

    // Private member functions
    std::span<const Trade> getRecentTrades(
        const std::chrono::system_clock::time_point& current_time) const;

    WindowStats computeWindowStats(std::span<const Trade> window_trades) const;

    double calculateConfidence(
        double drop,
        double time_diff,
        double pattern,
        double volume,
        const DetectionConfig& config) const;

    DetectionResult buildResult(
        bool detected,
        const std::chrono::system_clock::time_point& timestamp,
        const std::string& trigger,
        const std::map<std::string, double>& metrics) const;

    // Member variables - order must match initialization order in constructor
    mutable std::shared_mutex data_mutex_;
    std::vector<Trade> trades_;
    double peak_mc_;
    std::chrono::system_clock::time_point peak_time_;
    std::chrono::system_clock::time_point analysis_start_time_;
    size_t current_idx_;

    // Thread-local storage for temporary calculations
    static thread_local std::vector<double> price_changes_;
    static thread_local std::vector<double> volume_changes_;
};
