#pragma once

// Sample collection limits of zed_stereo_calibration: pure rules, no OpenCV or
// ZED SDK, so they can be unit tested without a camera.
// Design notes: .ai/specs/sample-limits/decisions.md

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <utility>

constexpr int COLLECTION_MIN_SAMPLES = 25;
constexpr int COLLECTION_MAX_SAMPLES = 35;  // 0 means no maximum

// Fewest samples 'calibrate()' accepts; must match MIN_IMAGE in
// opencv_calibration.hpp (checked there with a static_assert)
constexpr int MIN_CALIBRATION_SAMPLES = 20;

// Parse a non-negative sample count. Rejects signs, spaces, decimals and any
// trailing characters ("25x").
inline bool parseSampleCount(const std::string& text, int& value,
                             std::string& error) {
  if (text.empty() || text.size() > 6) {
    error = "'" + text + "' is not a valid number of samples";
    return false;
  }
  for (char c : text) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      error = "'" + text + "' is not a valid number of samples";
      return false;
    }
  }
  value = std::stoi(text);
  error.clear();
  return true;
}

// Check --min_samples / --max_samples (max 0 = no maximum)
inline bool validateSampleLimits(int min_samples, int max_samples,
                                 std::string& error) {
  if (min_samples < MIN_CALIBRATION_SAMPLES) {
    error = "--min_samples (" + std::to_string(min_samples) +
            ") must be at least " + std::to_string(MIN_CALIBRATION_SAMPLES) +
            ", the fewest samples the calibration accepts";
    return false;
  }
  if (max_samples < 0 || (max_samples != 0 && max_samples < min_samples)) {
    error = "--max_samples (" + std::to_string(max_samples) +
            ") must be 0 (no maximum) or at least --min_samples (" +
            std::to_string(min_samples) + ")";
    return false;
  }
  error.clear();
  return true;
}

enum class CollectionStep {
  CONTINUE,      // keep collecting
  COMPLETE,      // all metrics at 100% with at least the minimum: calibrate
  PAUSE_AT_MAX,  // maximum reached with incomplete metrics: ask the user
};

// Decide what to do after a sample is accepted. 'count' is the number of
// samples saved so far; 'max_lifted' is set once the user chose to continue
// past the maximum.
inline CollectionStep collectionStep(int count, int min_samples,
                                     int max_samples, bool metrics_complete,
                                     bool max_lifted) {
  if (metrics_complete && count >= min_samples) return CollectionStep::COMPLETE;
  if (max_samples != 0 && !max_lifted && count >= max_samples)
    return CollectionStep::PAUSE_AT_MAX;
  return CollectionStep::CONTINUE;
}

// The 'f' key ends the collection only when the calibration can run
inline bool canFinishManually(int count) {
  return count >= MIN_CALIBRATION_SAMPLES;
}

// "Y 90%, Skew 72%" for the metrics below 100%, in panel order. Percentages
// are rounded, but capped at 99% so that an incomplete metric never shows as
// 100%.
inline std::string incompleteMetrics(float x_score, float y_score,
                                     float size_score, float skew_score) {
  const std::pair<const char*, float> metrics[] = {
      {"X", x_score}, {"Y", y_score}, {"Size", size_score}, {"Skew", skew_score}};
  std::string out;
  for (const auto& [name, score] : metrics) {
    if (score >= 1.0f) continue;
    if (!out.empty()) out += ", ";
    const int percent = std::min(99, static_cast<int>(std::lround(score * 100.0f)));
    out += std::string(name) + " " + std::to_string(percent) + "%";
  }
  return out;
}
