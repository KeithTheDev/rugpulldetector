#pragma once
#include <chrono>
#include <string>

// Added memory alignment for better cache performance
struct alignas(32) Trade {
  std::chrono::system_clock::time_point timestamp;
  double market_cap_sol;
  double sol_amount;
};
