#include "redis_client.hpp"
#include <chrono>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
std::atomic<size_t> RedisClient::counter{0};

std::vector<Trade> RedisClient::getTrades(const std::string& key) {
    std::vector<Trade> trades;
    try {
        size_t index = counter++ % pool_size_;
        auto& redis = connection_pool_[index];

        // First check if key exists
        bool exists = redis->exists(key);
        if (!exists) {
            spdlog::error("Key does not exist: {}", key);
            return trades;
        }

        // Get key type and TTL for debugging
        auto key_type = redis->type(key);
        auto ttl = redis->ttl(key);
        spdlog::info("Key type: {}, TTL: {}s", key_type, ttl);

        // Get all members with scores
        std::vector<std::string> members;
        redis->zrange(key, 0, -1, std::back_inserter(members));

        trades.reserve(members.size());
        
        for (const auto& member : members) {
            try {
                // Get score for this member
                auto score = redis->zscore(key, member);
                if (!score) {
                    spdlog::error("Failed to get score for member");
                    continue;
                }

                // Parse JSON data
                auto trade_json = json::parse(member);

                // Create Trade object
                Trade trade;

                // Convert score (timestamp) to time_point
                trade.timestamp = std::chrono::system_clock::from_time_t(
                    static_cast<std::time_t>(*score));

                // Get values from JSON using value() method
                trade.market_cap_sol = trade_json["marketCapSol"].get<double>();
                trade.sol_amount = trade_json["solAmount"].get<double>();

                trades.push_back(std::move(trade));
            } catch (const json::exception& e) {
                spdlog::error("Failed to parse trade data: {}\nData: {}", 
                    e.what(), member);
                continue;
            } catch (const std::exception& e) {
                spdlog::error("Error processing trade: {}", e.what());
                continue;
            }
        }

        if (trades.empty()) {
            spdlog::error("No valid trades found after parsing");
            return trades;
        }

        // Sort trades by timestamp
        std::sort(trades.begin(), trades.end(), 
            [](const Trade& a, const Trade& b) {
                return a.timestamp < b.timestamp;
            });

        // Print trade summary
        if (trades.size() >= 2) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                trades.back().timestamp - trades.front().timestamp).count();
            
            spdlog::info("Analysis summary:\n"
                        "  Total trades: {}\n"
                        "  Time span: {:.1f} seconds\n"
                        "  Initial MC: {:.3f} SOL\n"
                        "  Latest MC: {:.3f} SOL",
                        trades.size(),
                        static_cast<double>(duration),
                        trades.back().market_cap_sol,
                        trades.front().market_cap_sol);
        }

    } catch (const sw::redis::Error& e) {
        spdlog::error("Redis error: {}", e.what());
    } catch (const std::exception& e) {
        spdlog::error("Error processing trades: {}", e.what());
    }

    return trades;
}
