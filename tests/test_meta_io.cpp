#include <chrono>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

#include "meta_io.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace {

fs::path freshFile(const std::string& name) {
  static int n = 0;
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path dir = fs::temp_directory_path() /
                       ("meta_test_" + std::to_string(stamp) + "_" + std::to_string(n++));
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

CaptureMeta sampleMeta() {
  CaptureMeta m;
  m.camera.model = "ZED 2i";
  m.camera.serial = 35180697;
  m.camera.firmware = 1523;
  m.camera.resolution = cv::Size(1280, 720);
  m.camera.fps = 15;
  m.camera.lens = "2.1mm";
  m.sdk_version = "5.1.0";
  m.code_commit = "abc1234";
  m.code_dirty = false;
  m.hostname = "bancada";
  m.self_calibration_disabled = true;
  m.session.output_root = "/data";
  m.session.condition = "A_ar_sem_acrilico";
  m.session.capture = 1;
  m.session.lens = "2.1mm";
  m.session.target_measured.tag_size_mm = 88.0;
  m.session.medium.type = "air";
  m.camera_settings_start = {{"EXPOSURE", 50}, {"GAIN", 1}};
  m.camera_settings_end = {{"EXPOSURE", 50}, {"GAIN", 1}};
  m.started_utc = "2026-11-03T14:00:00Z";
  m.ended_utc = "2026-11-03T14:30:00Z";
  m.end_reason = "all metrics complete";
  return m;
}

FrameMeta sampleFrame() {
  FrameMeta f;
  f.pose_id = "free";
  f.n = 0;
  f.timestamp_ns = 1762178400123456789LL;
  f.host_uptime_s = 12345.67;
  f.since_camera_open_s = 42.5;
  return f;
}

}  // namespace

TEST(meta_writes_nulls_for_missing_temperatures) {
  CaptureMeta m = sampleMeta();
  m.frames.push_back(sampleFrame());  // no temperature
  const fs::path path = freshFile("meta.json");
  std::string error;
  CHECK(writeCaptureMeta(path.string(), m, error));
  const std::string text = readAll(path);
  CHECK(contains(text, "\"onboard_left\": null"));
  CHECK(contains(text, "\"onboard_right\": null"));
  CHECK(contains(text, "\"imu\": null"));
  CHECK(!contains(text, "__ZEDCAL_"));  // no marker left behind
}

TEST(meta_writes_temperatures_when_present) {
  CaptureMeta m = sampleMeta();
  FrameMeta f = sampleFrame();
  f.temperature_onboard_left = 41.25;
  f.temperature_onboard_right = 40.5;
  f.temperature_imu = 38.0;
  m.frames.push_back(f);
  const fs::path path = freshFile("meta.json");
  std::string error;
  CHECK(writeCaptureMeta(path.string(), m, error));
  const std::string text = readAll(path);
  CHECK(contains(text, "\"onboard_left\": 4.1250000000000000e+01") ||
        contains(text, "\"onboard_left\": 41.25"));
  CHECK(!contains(text, "\"onboard_left\": null"));
}

TEST(meta_writes_booleans_as_json_literals) {
  CaptureMeta m = sampleMeta();
  const fs::path path = freshFile("meta.json");
  std::string error;
  CHECK(writeCaptureMeta(path.string(), m, error));
  const std::string text = readAll(path);
  CHECK(contains(text, "\"self_calibration_disabled\": true"));
  CHECK(contains(text, "\"dirty\": false"));
  CHECK(contains(text, "\"present\": false"));  // session.port.present
}

TEST(meta_copies_the_session_with_nulls) {
  CaptureMeta m = sampleMeta();
  const fs::path path = freshFile("meta.json");
  std::string error;
  CHECK(writeCaptureMeta(path.string(), m, error));
  const std::string text = readAll(path);
  CHECK(contains(text, "\"condition\": \"A_ar_sem_acrilico\""));
  CHECK(contains(text, "\"tag_size_mm\": 88."));
  CHECK(contains(text, "\"planarity_mm\": null"));
  CHECK(contains(text, "\"thickness_mm\": null"));
  CHECK(contains(text, "\"water_temperature_c\": null"));
}

TEST(meta_keeps_the_full_timestamp) {
  // A 64-bit nanosecond timestamp does not fit a double: it is written as text
  CaptureMeta m = sampleMeta();
  m.frames.push_back(sampleFrame());
  const fs::path path = freshFile("meta.json");
  std::string error;
  CHECK(writeCaptureMeta(path.string(), m, error));
  CHECK(contains(readAll(path), "\"timestamp_ns\": \"1762178400123456789\""));
}

TEST(meta_records_schema_and_settings) {
  CaptureMeta m = sampleMeta();
  const fs::path path = freshFile("meta.json");
  std::string error;
  CHECK(writeCaptureMeta(path.string(), m, error));
  const std::string text = readAll(path);
  CHECK(contains(text, "\"schema_version\": 1"));
  CHECK(contains(text, "\"serial\": 35180697"));
  CHECK(contains(text, "\"EXPOSURE\": 50"));
  CHECK(contains(text, "\"end_reason\": \"all metrics complete\""));
}

TEST(meta_text_values_cannot_inject_markers) {
  // A note that happens to contain a marker must stay a plain text
  CaptureMeta m = sampleMeta();
  m.session.note = "__ZEDCAL_NULL__";
  const fs::path path = freshFile("meta.json");
  std::string error;
  CHECK(!writeCaptureMeta(path.string(), m, error));
  CHECK(contains(error, "reserved"));
}

TEST(meta_utc_time_format) {
  const std::string t = utcTimestamp(std::chrono::system_clock::time_point{});
  CHECK(t == "1970-01-01T00:00:00Z");
  CHECK(std::regex_match(utcNow(), std::regex("\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}Z")));
}

TEST(meta_parses_proc_uptime) {
  CHECK(parseProcUptime("12345.67 98765.43\n").has_value());
  CHECK_NEAR(*parseProcUptime("12345.67 98765.43\n"), 12345.67, 1e-9);
  CHECK(!parseProcUptime("").has_value());
  CHECK(!parseProcUptime("abc").has_value());
}

TEST(meta_leaves_a_python_check_file) {
  CaptureMeta m = sampleMeta();
  m.frames.push_back(sampleFrame());
  std::string error;
  CHECK(writeCaptureMeta((fs::temp_directory_path() / "zedcal_meta_check.json").string(),
                         m, error));
}
