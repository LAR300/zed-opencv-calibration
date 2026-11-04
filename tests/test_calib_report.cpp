#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "calib_report.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace {

fs::path freshFile(const std::string& name) {
  static int n = 0;
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path dir = fs::temp_directory_path() /
                       ("calib_report_test_" + std::to_string(stamp) + "_" + std::to_string(n++));
  fs::create_directories(dir);
  return dir / name;
}

std::string readAll(const fs::path& path) {
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

CalibrationReport sample() {
  CalibrationReport r;
  r.model = "rational8";
  r.calibration_file = "calib_cpp_rational8.yml";
  r.mono_flags = "CALIB_RATIONAL_MODEL";
  r.stereo_flags = "CALIB_USE_INTRINSIC_GUESS | CALIB_RATIONAL_MODEL";
  r.term_criteria = "COUNT+EPS, 30, 1e-06";
  r.image_size = cv::Size(1280, 720);
  r.pairs_input = 2;
  r.pairs_used = 1;
  r.rms.stereo = 0.3;
  r.rms.stereo_fit_left = 0.31;
  r.rms.stereo_fit_right = 0.29;
  r.rms.free_pose_left = 0.28;
  r.rms.free_pose_right = 0.27;
  r.rms.mono_stage_left = 0.25;
  r.rms.mono_stage_right = 0.26;
  r.accepted = true;
  r.std_dev_names = {"fx", "fy", "cx", "cy"};
  r.std_dev_left = {0.1, 0.11, 0.2, 0.21};
  r.std_dev_right = {0.12, 0.13, 0.22, 0.23};
  r.code_commit = "abc";
  r.code_dirty = false;
  ViewReport kept;
  kept.pose_id = "free";
  kept.n = 0;
  kept.corners_left = 100;
  kept.corners_right = 96;
  kept.stereo_error_left = 0.3;
  kept.stereo_error_right = 0.28;
  ViewReport removed;
  removed.pose_id = "free";
  removed.n = 1;
  removed.corners_left = 80;
  removed.corners_right = 80;
  removed.removed_stage = "mono";
  removed.removed_error = 1.7;
  r.views = {kept, removed};
  return r;
}

}  // namespace

TEST(report_writes_rms_with_convention_and_mse) {
  const fs::path path = freshFile("report.json");
  std::string error;
  CHECK(writeCalibrationReport(path.string(), sample(), error));
  const std::string text = readAll(path);
  CHECK(contains(text, "\"schema_version\": 1"));
  CHECK(contains(text, "\"rms_convention\": \"per_corner_2d\""));
  CHECK(contains(text, "\"stereo_fit_left\""));
  CHECK(contains(text, "\"mono_stage_left\""));
  CHECK(contains(text, "\"mse_per_coordinate_px2\""));
  // 0.3^2 / 2 = 0.045
  CHECK(contains(text, "4.4999999999999998e-02") || contains(text, "0.045"));
  CHECK(contains(text, "\"accepted\": true"));
}

TEST(report_records_views_and_removals) {
  const fs::path path = freshFile("report.json");
  std::string error;
  CHECK(writeCalibrationReport(path.string(), sample(), error));
  const std::string text = readAll(path);
  CHECK(contains(text, "\"removed\": null"));
  CHECK(contains(text, "\"stage\": \"mono\""));
  CHECK(contains(text, "\"corners_left\": 100"));
  CHECK(contains(text, "\"pairs_used\": 1"));
}

TEST(report_marks_the_missing_stereo_uncertainty) {
  const fs::path path = freshFile("report.json");
  std::string error;
  CHECK(writeCalibrationReport(path.string(), sample(), error));
  const std::string text = readAll(path);
  CHECK(contains(text, "\"std_dev_stereo\": null"));
  CHECK(contains(text, "\"std_dev_mono_last\""));
}

TEST(report_missing_rms_is_null) {
  CalibrationReport r = sample();
  r.rms.free_pose_right.reset();
  const fs::path path = freshFile("report.json");
  std::string error;
  CHECK(writeCalibrationReport(path.string(), r, error));
  CHECK(contains(readAll(path), "\"free_pose_right\": null"));
}

TEST(report_leaves_a_python_check_file) {
  std::string error;
  CHECK(writeCalibrationReport((fs::temp_directory_path() / "zedcal_report_check.json").string(),
                               sample(), error));
}
