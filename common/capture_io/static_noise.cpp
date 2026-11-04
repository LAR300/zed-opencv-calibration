#include "static_noise.hpp"

#include <algorithm>
#include <cmath>

#include <opencv2/core.hpp>

#include "json_write.hpp"

namespace {

double percentile(const std::vector<double>& sorted, double p) {
  const double pos = p / 100.0 * (sorted.size() - 1);
  const size_t lo = static_cast<size_t>(std::floor(pos));
  const size_t hi = std::min(lo + 1, sorted.size() - 1);
  return sorted[lo] + (pos - lo) * (sorted[hi] - sorted[lo]);
}

void writeDistribution(cv::FileStorage& fs, const char* key, const std::vector<double>& values) {
  const Distribution d = summarize(values);
  fs << key << "{" << "n" << d.n;
  if (d.n == 0) {
    for (const char* k : {"p05", "p50", "p90", "p95", "p99", "max"}) json_write::null(fs, k);
  } else {
    fs << "p05" << d.p05 << "p50" << d.p50 << "p90" << d.p90 << "p95" << d.p95 << "p99" << d.p99
       << "max" << d.max;
  }
  fs << "}";
}

}  // namespace

Distribution summarize(std::vector<double> values) {
  Distribution d;
  d.n = static_cast<int>(values.size());
  if (values.empty()) return d;
  std::sort(values.begin(), values.end());
  d.p05 = percentile(values, 5);
  d.p50 = percentile(values, 50);
  d.p90 = percentile(values, 90);
  d.p95 = percentile(values, 95);
  d.p99 = percentile(values, 99);
  d.max = values.back();
  return d;
}

ProposedThresholds proposeThresholds(const StaticNoise& noise) {
  ProposedThresholds t;
  if (!noise.displacement_px.empty()) t.max_displacement_px = summarize(noise.displacement_px).p95;
  if (!noise.sharpness_left.empty() && !noise.sharpness_right.empty())
    t.min_target_sharpness = 0.5 * std::min(summarize(noise.sharpness_left).p05,
                                            summarize(noise.sharpness_right).p05);
  return t;
}

bool writeStaticNoise(const std::string& path, const StaticNoise& noise, std::string& error) {
  std::string text;
  try {
    cv::FileStorage fs(".json", cv::FileStorage::WRITE | cv::FileStorage::MEMORY |
                                    cv::FileStorage::FORMAT_JSON);
    fs << "schema_version" << 1 << "duration_s" << noise.duration_s << "frames" << noise.frames;
    writeDistribution(fs, "displacement_px", noise.displacement_px);
    writeDistribution(fs, "sharpness_left", noise.sharpness_left);
    writeDistribution(fs, "sharpness_right", noise.sharpness_right);
    const ProposedThresholds t = proposeThresholds(noise);
    fs << "proposed_thresholds" << "{";
    json_write::optional(fs, "max_displacement_px", t.max_displacement_px);
    json_write::optional(fs, "min_target_sharpness", t.min_target_sharpness);
    fs << "rule" << "max_displacement_px = p95 of the displacement; min_target_sharpness = "
                    "0.5 x the lower p05 of the two cameras";
    fs << "}";
    text = fs.releaseAndGetString();
  } catch (const cv::Exception& e) {
    error = std::string("Cannot encode the noise file: ") + e.what();
    return false;
  }
  return json_write::finishJson(text, path, error);
}
