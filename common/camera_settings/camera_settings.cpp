#include "camera_settings.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

#include <opencv2/opencv.hpp>

namespace {

// Not settable through this file (decisions.md D2, D7)
const std::map<std::string, std::string> kRejected = {
    {"SCENE_ILLUMINANCE", "is read-only"},
    {"AEC_AGC_ROI", "takes a rectangle and is not supported by this tool"},
    {"LED_STATUS", "does not affect the calibration and is not supported by this tool"},
    {"AUTO_EXPOSURE_TIME_RANGE", "takes a min/max range and is not supported by this tool"},
    {"AUTO_ANALOG_GAIN_RANGE", "takes a min/max range and is not supported by this tool"},
    {"AUTO_DIGITAL_GAIN_RANGE", "takes a min/max range and is not supported by this tool"}};

const char* kManualExposure[] = {"EXPOSURE", "GAIN", "EXPOSURE_TIME",
                                 "ANALOG_GAIN", "DIGITAL_GAIN"};

constexpr int WHITEBALANCE_STEP = 100;

}  // namespace

const std::vector<std::string>& videoSettingNames() {
  static const std::vector<std::string> names = {
      "BRIGHTNESS", "CONTRAST", "HUE", "SATURATION", "SHARPNESS", "GAMMA",
      "GAIN", "EXPOSURE", "AEC_AGC", "AEC_AGC_ROI", "WHITEBALANCE_TEMPERATURE",
      "WHITEBALANCE_AUTO", "LED_STATUS", "EXPOSURE_TIME", "ANALOG_GAIN",
      "DIGITAL_GAIN", "AUTO_EXPOSURE_TIME_RANGE", "AUTO_ANALOG_GAIN_RANGE",
      "AUTO_DIGITAL_GAIN_RANGE", "EXPOSURE_COMPENSATION", "DENOISING",
      "SCENE_ILLUMINANCE"};
  return names;
}

bool isModeSwitch(const std::string& name) {
  return name == "AEC_AGC" || name == "WHITEBALANCE_AUTO";
}

bool loadCameraSettings(const std::string& path,
                        std::vector<CameraSetting>& settings,
                        std::string& error) {
  settings.clear();
  std::ifstream file(path);
  if (!file.is_open()) {
    error = "Cannot open the camera settings file '" + path + "'";
    return false;
  }
  std::stringstream content;
  content << file.rdbuf();
  std::string yaml = content.str();
  // cv::FileStorage requires the '%YAML' directive
  if (yaml.rfind("%YAML", 0) != 0) yaml = "%YAML:1.0\n---\n" + yaml;

  cv::FileStorage fs;
  try {
    fs.open(yaml, cv::FileStorage::READ | cv::FileStorage::MEMORY);
  } catch (const cv::Exception& e) {
    error = "Cannot parse the camera settings file '" + path + "': " + e.what();
    return false;
  }
  if (!fs.isOpened()) {
    error = "Cannot parse the camera settings file '" + path + "'";
    return false;
  }

  const auto& names = videoSettingNames();
  std::vector<CameraSetting> values, modes;
  std::map<std::string, int> seen;
  for (const auto& node : fs.root()) {
    const std::string name = node.name();
    if (std::find(names.begin(), names.end(), name) == names.end()) {
      error = "Unknown camera setting '" + name + "' in '" + path + "'";
      return false;
    }
    auto rejected = kRejected.find(name);
    if (rejected != kRejected.end()) {
      error = "Camera setting '" + name + "' " + rejected->second;
      return false;
    }
    if (!node.isInt()) {
      error = "Camera setting '" + name + "' must be an integer";
      return false;
    }
    const int value = static_cast<int>(node);
    if (isModeSwitch(name) && value != 0 && value != 1) {
      error = "Camera setting '" + name + "' must be 0 (manual) or 1 (automatic)";
      return false;
    }
    if (name == "WHITEBALANCE_TEMPERATURE" && value % WHITEBALANCE_STEP != 0) {
      error = "Camera setting 'WHITEBALANCE_TEMPERATURE' (" +
              std::to_string(value) + ") must be a multiple of " +
              std::to_string(WHITEBALANCE_STEP) + " K";
      return false;
    }
    seen[name] = value;
    (isModeSwitch(name) ? modes : values).push_back({name, value});
  }

  // Automatic mode together with the manual value it would override
  if (seen.count("AEC_AGC") && seen["AEC_AGC"] == 1) {
    for (const char* manual : kManualExposure) {
      if (seen.count(manual)) {
        error = std::string("'AEC_AGC: 1' (automatic) contradicts '") + manual +
                "', which sets the exposure manually";
        return false;
      }
    }
  }
  if (seen.count("WHITEBALANCE_AUTO") && seen["WHITEBALANCE_AUTO"] == 1 &&
      seen.count("WHITEBALANCE_TEMPERATURE")) {
    error = "'WHITEBALANCE_AUTO: 1' (automatic) contradicts "
            "'WHITEBALANCE_TEMPERATURE', which sets it manually";
    return false;
  }

  settings = values;
  settings.insert(settings.end(), modes.begin(), modes.end());
  error.clear();
  return true;
}
