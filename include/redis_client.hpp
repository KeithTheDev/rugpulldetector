#pragma once
#include "trade.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <sw/redis++/redis++.h>
#include <vector>

class RedisClient {
public:
    // Add connection pooling
    explicit RedisClient(const std::string& url, size_t pool_size = 8)
        : pool_size_(pool_size) {
        connection_pool_.reserve(pool_size);
        for (size_t i = 0; i < pool_size; ++i) {
            connection_pool_.emplace_back(std::make_unique<sw::redis::Redis>(url));
        }
    }

    // Use connection pooling for better concurrent performance
    std::vector<Trade> getTrades(const std::string& key);

private:
    size_t pool_size_;
    std::vector<std::unique_ptr<sw::redis::Redis>> connection_pool_;
    static std::atomic<size_t> counter;
};
