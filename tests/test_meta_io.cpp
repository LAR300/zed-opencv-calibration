#include <chrono>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

#include "json_write.hpp"
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

TEST(meta_copies_the_metric_target_measurements) {
  CaptureMeta m = sampleMeta();
  const fs::path empty = freshFile("meta.json");
  std::string error;
  CHECK(writeCaptureMeta(empty.string(), m, error));
  CHECK(contains(readAll(empty), "\"board_side_mm\": null"));
  m.session.target_measured.tag_sides_mm = {88.02, 87.98};
  m.session.target_measured.board_side_mm = 660.1;
  m.session.target_measured.planarity_grid_mm = {0.0, -0.1};
  const fs::path path = freshFile("meta.json");
  CHECK(writeCaptureMeta(path.string(), m, error));
  const std::string text = readAll(path);
  // FileStorage writes doubles as %.17g: 8.8019999999999996e+01
  CHECK(contains(text, "\"tag_sides_mm\": [\n"));
  CHECK(contains(text, "8.80199999"));
  CHECK(contains(text, "\"board_side_mm\": 6.6010000"));
  CHECK(contains(text, "-1.0000000"));
  CHECK(contains(text, "\"spacings_mm\": [\n"));
}

TEST(meta_writes_the_script_mode_records) {
  CaptureMeta m = sampleMeta();
  m.frames.push_back(sampleFrame());
  const fs::path free_path = freshFile("meta.json");
  std::string error;
  CHECK(writeCaptureMeta(free_path.string(), m, error));
  const std::string free_text = readAll(free_path);
  CHECK(contains(free_text, "\"script\": null"));
  CHECK(contains(free_text, "\"pose\": null"));
  CHECK(contains(free_text, "\"out_of_tolerance\": null"));
  CHECK(contains(free_text, "\"still\": null"));
  CHECK(contains(free_text, "\"roteiro\": null"));

  FrameMeta f = sampleFrame();
  PoseMeasurement p;
  p.fraction = 0.5;
  p.tilt_h_deg = 35.0;
  p.in_tolerance = true;
  f.pose = p;
  f.out_of_tolerance = false;
  StillMeta st;
  st.window_s = 1.0;
  st.threshold_window_s = 1.0;
  f.still = st;
  m.frames = {f};
  ScriptMeta script;
  script.name = "roteiro_v1";
  script.file = "/abs/roteiro_v1.yaml";
  script.events = {{"B1-P-TL", "saved", 0}, {"B1-P-TC", "skipped", std::nullopt}};
  m.script = script;
  m.session.pose_script = "/abs/roteiro_v1.yaml";
  const fs::path path = freshFile("meta.json");
  CHECK(writeCaptureMeta(path.string(), m, error));
  const std::string text = readAll(path);
  CHECK(contains(text, "\"in_tolerance\": true"));
  CHECK(contains(text, "\"out_of_tolerance\": false"));
  CHECK(contains(text, "\"action\": \"skipped\""));
  CHECK(contains(text, "\"n\": null"));
  CHECK(contains(text, "\"roteiro\": \"/abs/roteiro_v1.yaml\""));
  CHECK(contains(text, "\"min_target_sharpness\""));
  CHECK(!contains(text, "__ZEDCAL_"));
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

TEST(meta_writes_single_quotes_as_valid_json) {
  // FileStorage escapes ' as \', which JSON does not accept
  CaptureMeta m = sampleMeta();
  m.end_reason = "ended by the user ('f')";
  m.session.note = "a\\'b";  // a backslash right before a quote
  const fs::path path = freshFile("meta.json");
  std::string error;
  CHECK(writeCaptureMeta(path.string(), m, error));
  const std::string text = readAll(path);
  CHECK(contains(text, "\"end_reason\": \"ended by the user ('f')\""));
  CHECK(contains(text, "\"note\": \"a\\\\'b\""));
  CHECK(!contains(text, "(\\'"));
}

TEST(json_unescape_single_quotes_keeps_other_escapes) {
  CHECK(json_write::unescapeSingleQuotes("\"x\\'y\"") == "\"x'y\"");
  CHECK(json_write::unescapeSingleQuotes("\"x\\\\\\'y\"") == "\"x\\\\'y\"");
  CHECK(json_write::unescapeSingleQuotes("\"x\\\\y\\\"z\\n\"") == "\"x\\\\y\\\"z\\n\"");
  CHECK(json_write::unescapeSingleQuotes("\"x\\\\\"") == "\"x\\\\\"");
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

TEST(meta_without_session_writes_null) {
  CaptureMeta m = sampleMeta();
  m.has_session = false;
  const fs::path path = freshFile("meta.json");
  std::string error;
  CHECK(writeCaptureMeta(path.string(), m, error));
  const std::string text = readAll(path);
  CHECK(contains(text, "\"session\": null"));
  CHECK(!contains(text, "\"condition\""));
}

TEST(meta_copies_the_calibration_section) {
  CaptureMeta m = sampleMeta();
  m.session.calibration.prior = "factory";
  const fs::path path = freshFile("meta.json");
  std::string error;
  CHECK(writeCaptureMeta(path.string(), m, error));
  const std::string text = readAll(path);
  CHECK(contains(text, "\"calibration\""));
  CHECK(contains(text, "\"prior\": \"factory\""));
  CHECK(contains(text, "\"model\": \"rational8\""));
}
