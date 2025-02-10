#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "detection_config.hpp"
#include "redis_client.hpp"
#include "rug_pull_detector.hpp"

class TradeProcessor {
public:
  explicit TradeProcessor(size_t num_threads)
      : workers_(), work_queues_(num_threads), queue_mutexes_(num_threads),
        queue_mutex_(), queue_cv_(), should_stop_(false) {

    // Create threads
    for (size_t i = 0; i < num_threads; ++i) {
      workers_.emplace_back([this, i] { processTradesWorker(i); });
    }
  }

  ~TradeProcessor() {
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      should_stop_ = true;
    }
    queue_cv_.notify_all();
    for (auto &worker : workers_) {
      worker.join();
    }
  }

  // Made public since it's called from main
  void addTask(const std::string &redis_key) {
    static std::atomic<size_t> round_robin{0};
    size_t queue_index = round_robin++ % work_queues_.size();

    {
      std::unique_lock<std::mutex> lock(queue_mutexes_[queue_index]);
      work_queues_[queue_index].push(redis_key);
    }
    queue_cv_.notify_one();
  }

private:
  void processTradesWorker(size_t worker_id) {
    while (true) {
      std::string task;
      bool found_task = false;

      // Try to get task from own queue first
      {
        std::unique_lock<std::mutex> lock(queue_mutexes_[worker_id]);
        if (!work_queues_[worker_id].empty()) {
          task = work_queues_[worker_id].front();
          work_queues_[worker_id].pop();
          found_task = true;
        }
      }

      // If no task in own queue, try work stealing
      if (!found_task) {
        for (size_t i = 0; i < work_queues_.size(); ++i) {
          if (i == worker_id)
            continue;

          {
            std::unique_lock<std::mutex> lock(queue_mutexes_[i]);
            if (!work_queues_[i].empty()) {
              task = work_queues_[i].front();
              work_queues_[i].pop();
              found_task = true;
              break;
            }
          }
        }
      }

      if (!found_task) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (should_stop_ && allQueuesEmpty()) {
          return;
        }
        queue_cv_.wait(lock);
        continue;
      }

      processRedisKey(task);
    }
  }

  bool allQueuesEmpty() const {
    for (size_t i = 0; i < work_queues_.size(); ++i) {
      std::unique_lock<std::mutex> lock(queue_mutexes_[i]);
      if (!work_queues_[i].empty())
        return false;
    }
    return true;
  }

  void processRedisKey(const std::string &key) {
    try {
      static RedisClient redis("redis://localhost");
      auto trades = redis.getTrades(key);

      if (trades.empty()) {
        spdlog::warn("No trades found for key: {}", key);
        return;
      }

      spdlog::info("Processing {} trades for key: {}", trades.size(), key);

      RugPullDetector detector;
      // Move trades instead of copying
      for (auto &&trade : trades) {
        detector.addTrade(std::move(trade));
      }

      DetectionConfig config;
      auto result = detector.processTrades(config);

      if (result.rug_pulled) {
        spdlog::warn("⚠️  RUG PULL DETECTED:");
        if (result.timestamp) {
          spdlog::warn("Time: {}", std::chrono::system_clock::to_time_t(
                                       result.timestamp.value()));
        }
        spdlog::warn("Trigger: {}", result.debug_info.trigger_type);
        spdlog::warn("Confidence: {:.3f}", result.debug_info.confidence);
        spdlog::warn("Drop: {:.2f}%", result.debug_info.drop_percentage);
        spdlog::warn("Peak MC: {:.3f} SOL", result.debug_info.peak_market_cap);
        spdlog::warn("Final MC: {:.3f} SOL",
                     result.debug_info.current_market_cap);
      } else {
        spdlog::info("No rug pull pattern detected for key: {}", key);
      }

    } catch (const std::exception &e) {
      spdlog::error("Error processing key {}: {}", key, e.what());
    }
  }

  // Member variables - now only declared once
  std::vector<std::thread> workers_;
  std::vector<std::queue<std::string>> work_queues_;
  mutable std::vector<std::mutex> queue_mutexes_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  bool should_stop_;
};

void setupLogger(bool debug_mode) {
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_level(debug_mode ? spdlog::level::debug : spdlog::level::info);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <redis_key> [--debug]" << std::endl;
    return 1;
  }

  std::string redis_key = argv[1];
  bool debug_mode = (argc > 2 && std::string(argv[2]) == "--debug");

  try {
    setupLogger(debug_mode);

    // Create a thread pool with number of threads equal to hardware concurrency
    spdlog::info("Starting rug pull detector with {} threads",
                 std::thread::hardware_concurrency());
    TradeProcessor processor(std::thread::hardware_concurrency());

    spdlog::info("Processing trades for key: {}", redis_key);
    processor.addTask(redis_key);

    // Wait for user input to exit
    std::string input;
    std::getline(std::cin, input);

  } catch (const std::exception &e) {
    spdlog::error("Fatal error: {}", e.what());
    return 1;
  }

  return 0;
}
