#pragma once
#include <string>
#include <optional>
#include <chrono>

struct DetectionResult {
    bool rug_pulled = false;
    std::optional<std::chrono::system_clock::time_point> timestamp;
    struct DebugInfo {
        std::string trigger_type;
        double confidence = 1.0;
        double drop_percentage = 0.0;
        double peak_market_cap = 0.0;
        double current_market_cap = 0.0;
    } debug_info;
};
