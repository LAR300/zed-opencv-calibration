#include <filesystem>
#include <fstream>
#include <sstream>

#include "static_noise.hpp"
#include "test_utils.hpp"

TEST(static_noise_percentiles_match_numpy) {
  // numpy.percentile([1..10], [5, 50, 90, 95, 99]) = 1.45, 5.5, 9.1, 9.55, 9.91
  std::vector<double> v;
  for (int i = 10; i >= 1; --i) v.push_back(i);
  const Distribution d = summarize(v);
  CHECK(d.n == 10);
  CHECK_NEAR(d.p05, 1.45, 1e-12);
  CHECK_NEAR(d.p50, 5.5, 1e-12);
  CHECK_NEAR(d.p90, 9.1, 1e-12);
  CHECK_NEAR(d.p95, 9.55, 1e-12);
  CHECK_NEAR(d.p99, 9.91, 1e-12);
  CHECK_NEAR(d.max, 10.0, 1e-12);
  CHECK(summarize({}).n == 0);
}

TEST(static_noise_file_and_proposed_thresholds) {
  StaticNoise n;
  n.duration_s = 30.0;
  n.frames = 450;
  for (int i = 1; i <= 100; ++i) n.displacement_px.push_back(0.01 * i);
  for (int i = 0; i < 100; ++i) {
    n.sharpness_left.push_back(200.0 + i);
    n.sharpness_right.push_back(150.0 + i);
  }
  const ProposedThresholds t = proposeThresholds(n);
  CHECK(t.max_displacement_px && std::abs(*t.max_displacement_px - 0.9505) < 1e-9);
  CHECK(t.min_target_sharpness && std::abs(*t.min_target_sharpness - 0.5 * 154.95) < 1e-9);

  const auto path = std::filesystem::temp_directory_path() / "zedcal_noise_test.json";
  std::filesystem::remove(path);
  std::string error;
  CHECK(writeStaticNoise(path.string(), n, error));
  std::stringstream text;
  text << std::ifstream(path).rdbuf();
  CHECK(text.str().find("\"proposed_thresholds\"") != std::string::npos);
  CHECK(text.str().find("\"p95\"") != std::string::npos);

  StaticNoise empty;  // no target: every percentile null, no proposal
  std::filesystem::remove(path);
  CHECK(writeStaticNoise(path.string(), empty, error));
  std::stringstream e;
  e << std::ifstream(path).rdbuf();
  CHECK(e.str().find("\"max_displacement_px\": null") != std::string::npos);
}
