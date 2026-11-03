#include <filesystem>
#include <fstream>

#include "camera_settings.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace {

std::string writeTemp(const std::string& name, const std::string& content) {
  fs::path path = fs::temp_directory_path() / ("camera_settings_test_" + name);
  std::ofstream out(path);
  out << content;
  return path.string();
}

bool load(const std::string& content, std::vector<CameraSetting>& settings,
          std::string& error) {
  static int n = 0;
  return loadCameraSettings(writeTemp(std::to_string(n++) + ".yaml", content),
                            settings, error);
}

int valueOf(const std::vector<CameraSetting>& settings, const std::string& name) {
  for (const auto& s : settings)
    if (s.name == name) return s.value;
  return -999;
}

}  // namespace

TEST(camera_settings_reads_values) {
  std::vector<CameraSetting> settings;
  std::string error;
  CHECK(load("EXPOSURE: 40  # comment\nGAIN: 20\nSHARPNESS: 2\n", settings, error));
  CHECK(error.empty());
  CHECK(settings.size() == 3);
  CHECK(valueOf(settings, "EXPOSURE") == 40);
  CHECK(valueOf(settings, "GAIN") == 20);
  CHECK(valueOf(settings, "SHARPNESS") == 2);
}

TEST(camera_settings_accepts_opencv_header) {
  std::vector<CameraSetting> settings;
  std::string error;
  CHECK(load("%YAML:1.0\n---\nGAMMA: 5\n", settings, error));
  CHECK(valueOf(settings, "GAMMA") == 5);
}

TEST(camera_settings_empty_file_is_empty_list) {
  std::vector<CameraSetting> settings;
  std::string error;
  CHECK(load("# nothing set\n", settings, error));
  CHECK(settings.empty());
}

TEST(camera_settings_rejects_unknown_name) {
  std::vector<CameraSetting> settings;
  std::string error;
  CHECK(!load("EXPOSURE: 40\nEXPOSUR: 40\n", settings, error));
  CHECK(error.find("EXPOSUR") != std::string::npos);
}

TEST(camera_settings_rejects_non_integer) {
  std::vector<CameraSetting> settings;
  std::string error;
  CHECK(!load("EXPOSURE: 40.5\n", settings, error));
  CHECK(error.find("EXPOSURE") != std::string::npos);
  CHECK(!load("GAIN: high\n", settings, error));
  CHECK(error.find("GAIN") != std::string::npos);
}

TEST(camera_settings_rejects_missing_or_malformed_file) {
  std::vector<CameraSetting> settings;
  std::string error;
  CHECK(!loadCameraSettings("/nonexistent/settings.yaml", settings, error));
  CHECK(error.find("/nonexistent/settings.yaml") != std::string::npos);
  CHECK(!load("EXPOSURE: [40\n", settings, error));
  CHECK(!error.empty());
}

TEST(camera_settings_rejects_read_only_and_out_of_scope) {
  std::vector<CameraSetting> settings;
  std::string error;
  CHECK(!load("SCENE_ILLUMINANCE: 10\n", settings, error));
  CHECK(error.find("read-only") != std::string::npos);
  CHECK(!load("AEC_AGC_ROI: 1\n", settings, error));
  CHECK(error.find("AEC_AGC_ROI") != std::string::npos);
  CHECK(!load("LED_STATUS: 0\n", settings, error));
  CHECK(error.find("LED_STATUS") != std::string::npos);
  CHECK(!load("AUTO_EXPOSURE_TIME_RANGE: 1000\n", settings, error));
  CHECK(error.find("AUTO_EXPOSURE_TIME_RANGE") != std::string::npos);
}

TEST(camera_settings_rejects_auto_with_manual_values) {
  std::vector<CameraSetting> settings;
  std::string error;
  CHECK(!load("AEC_AGC: 1\nEXPOSURE: 40\n", settings, error));
  CHECK(error.find("AEC_AGC") != std::string::npos && error.find("EXPOSURE") != std::string::npos);
  CHECK(!load("AEC_AGC: 1\nGAIN: 10\n", settings, error));
  CHECK(!load("WHITEBALANCE_AUTO: 1\nWHITEBALANCE_TEMPERATURE: 4700\n", settings, error));
  CHECK(error.find("WHITEBALANCE_AUTO") != std::string::npos);
}

TEST(camera_settings_mode_switches_are_binary) {
  std::vector<CameraSetting> settings;
  std::string error;
  CHECK(!load("AEC_AGC: 2\n", settings, error));
  CHECK(!load("WHITEBALANCE_AUTO: -1\n", settings, error));
}

TEST(camera_settings_white_balance_step) {
  std::vector<CameraSetting> settings;
  std::string error;
  CHECK(!load("WHITEBALANCE_TEMPERATURE: 4750\n", settings, error));
  CHECK(error.find("100") != std::string::npos);
  CHECK(load("WHITEBALANCE_TEMPERATURE: 4700\n", settings, error));
}

TEST(camera_settings_freezing_auto_is_allowed) {
  std::vector<CameraSetting> settings;
  std::string error;
  CHECK(load("AEC_AGC: 0\n", settings, error));
  CHECK(valueOf(settings, "AEC_AGC") == 0);
}

TEST(camera_settings_mode_switches_come_last) {
  std::vector<CameraSetting> settings;
  std::string error;
  CHECK(load("AEC_AGC: 0\nWHITEBALANCE_AUTO: 0\nEXPOSURE: 40\nSHARPNESS: 2\n",
             settings, error));
  CHECK(settings.size() == 4);
  if (settings.size() != 4) return;
  CHECK(settings[0].name == "EXPOSURE");
  CHECK(settings[1].name == "SHARPNESS");
  CHECK(settings[2].name == "AEC_AGC");
  CHECK(settings[3].name == "WHITEBALANCE_AUTO");
}
