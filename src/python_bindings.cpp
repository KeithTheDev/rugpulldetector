#include "redis_client.hpp"
#include "rug_pull_detector.hpp"
#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace pybind11::literals;

py::dict
check_rug_pull_sync(const std::string &mint_address,
                    const std::string &redis_url = "redis://localhost") {
  try {
    RedisClient redis(redis_url);
    auto trades = redis.getTrades("recent_trades:" + mint_address);

    if (trades.empty()) {
      return py::dict("rug_pulled"_a = false, "timestamp"_a = py::none(),
                      "debug_info"_a =
                          py::dict("error"_a = "No trade data found"));
    }

    RugPullDetector detector;
    for (auto &&trade : trades) {
      detector.addTrade(std::move(trade));
    }

    DetectionConfig config;
    auto result = detector.processTrades(config);

    if (result.rug_pulled) {
      return py::dict(
          "rug_pulled"_a = true,
          "timestamp"_a = result.timestamp.has_value()
                              ? py::cast(result.timestamp.value())
                              : py::none(),
          "debug_info"_a = py::dict(
              "trigger_type"_a = result.debug_info.trigger_type,
              "confidence"_a = result.debug_info.confidence,
              "drop_percentage"_a = result.debug_info.drop_percentage,
              "peak_market_cap"_a = result.debug_info.peak_market_cap,
              "current_market_cap"_a = result.debug_info.current_market_cap));
    }

    return py::dict("rug_pulled"_a = false, "timestamp"_a = py::none(),
                    "debug_info"_a = py::dict());
  } catch (const std::exception &e) {
    return py::dict("rug_pulled"_a = false, "timestamp"_a = py::none(),
                    "debug_info"_a = py::dict("error"_a = e.what()));
  }
}

PYBIND11_MODULE(rugpull_detector, m) {
  m.doc() = "Rug Pull Detector Module";

  m.def("check_rug_pull_sync", &check_rug_pull_sync,
        "Synchronously check if a token has been rug pulled",
        py::arg("mint_address"), py::arg("redis_url") = "redis://localhost");

  py::class_<DetectionConfig>(m, "DetectionConfig")
      .def(py::init<>())
      .def_readonly_static("peak_drop_threshold",
                           &DetectionConfig::peak_drop_threshold)
      .def_readonly_static("time_from_peak_threshold",
                           &DetectionConfig::time_from_peak_threshold)
      .def_readonly_static("volume_spike_threshold",
                           &DetectionConfig::volume_spike_threshold)
      .def_readonly_static("min_confidence_score",
                           &DetectionConfig::min_confidence_score)
      .def_readonly_static("early_warning_threshold",
                           &DetectionConfig::early_warning_threshold)
      .def_readonly_static("consecutive_drops_threshold",
                           &DetectionConfig::consecutive_drops_threshold)
      .def_readonly_static("price_velocity_threshold",
                           &DetectionConfig::price_velocity_threshold)
      .def_readonly_static("pattern_strength_threshold",
                           &DetectionConfig::pattern_strength_threshold)
      .def_readonly_static("stop_loss_threshold",
                           &DetectionConfig::stop_loss_threshold)
      .def_readonly_static("max_detection_time",
                           &DetectionConfig::max_detection_time);
}
