#pragma once

// --measure_static_noise (.ai/specs/captura-estatica R7): with the target still,
// the distribution of the frame-to-frame corner displacement and of the target
// sharpness, written to noise.json, with the thresholds they suggest.

#include <optional>
#include <string>
#include <vector>

struct Distribution {
  int n = 0;
  double p05 = 0, p50 = 0, p90 = 0, p95 = 0, p99 = 0, max = 0;
};
// Percentiles by linear interpolation between order statistics (numpy's default)
Distribution summarize(std::vector<double> values);

struct StaticNoise {
  double duration_s = 0.0;
  int frames = 0;                    // frames grabbed
  std::vector<double> displacement_px;  // one per pair of consecutive frames with enough corners
  std::vector<double> sharpness_left, sharpness_right;  // one per frame with tags
};

// Proposed thresholds: the 95th percentile of the displacement (a motion below
// the detector noise cannot be told from stillness) and half the 5th
// percentile of the sharpness of the still target, the lower camera. Empty
// when there are no samples.
struct ProposedThresholds {
  std::optional<double> max_displacement_px;
  std::optional<double> min_target_sharpness;
};
ProposedThresholds proposeThresholds(const StaticNoise& noise);

bool writeStaticNoise(const std::string& path, const StaticNoise& noise, std::string& error);
