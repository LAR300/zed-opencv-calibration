#include "zed_camera_settings.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <map>

namespace {

// Explicit mapping, so that a reordered SDK enum cannot silently swap
// settings
const std::map<std::string, sl::VIDEO_SETTINGS>& settingIds() {
  using VS = sl::VIDEO_SETTINGS;
  static const std::map<std::string, VS> ids = {
      {"BRIGHTNESS", VS::BRIGHTNESS},
      {"CONTRAST", VS::CONTRAST},
      {"HUE", VS::HUE},
      {"SATURATION", VS::SATURATION},
      {"SHARPNESS", VS::SHARPNESS},
      {"GAMMA", VS::GAMMA},
      {"GAIN", VS::GAIN},
      {"EXPOSURE", VS::EXPOSURE},
      {"AEC_AGC", VS::AEC_AGC},
      {"WHITEBALANCE_TEMPERATURE", VS::WHITEBALANCE_TEMPERATURE},
      {"WHITEBALANCE_AUTO", VS::WHITEBALANCE_AUTO},
      {"EXPOSURE_TIME", VS::EXPOSURE_TIME},
      {"ANALOG_GAIN", VS::ANALOG_GAIN},
      {"DIGITAL_GAIN", VS::DIGITAL_GAIN},
      {"EXPOSURE_COMPENSATION", VS::EXPOSURE_COMPENSATION},
      {"DENOISING", VS::DENOISING}};
  return ids;
}

// Frames grabbed before reading the settings back (decisions.md D4)
constexpr int SETTLE_FRAMES = 5;

std::atomic<bool> interrupt_requested{false};

bool readSetting(sl::Camera& camera, sl::VIDEO_SETTINGS id, int& value) {
  if (!camera.isCameraSettingSupported(id)) return false;
  return camera.getCameraSettings(id, value) == sl::ERROR_CODE::SUCCESS;
}

}  // namespace

CameraSettingsSnapshot snapshotCameraSettings(sl::Camera& camera) {
  CameraSettingsSnapshot snapshot;
  for (const auto& name : videoSettingNames()) {
    auto it = settingIds().find(name);
    int value;
    if (it != settingIds().end() && readSetting(camera, it->second, value))
      snapshot.values.push_back({name, value});
  }
  return snapshot;
}

bool validateCameraSettings(sl::Camera& camera,
                            const std::vector<CameraSetting>& settings,
                            std::string& error) {
  for (const auto& setting : settings) {
    auto it = settingIds().find(setting.name);
    if (it == settingIds().end() || !camera.isCameraSettingSupported(it->second)) {
      error = "Camera setting '" + setting.name +
              "' is not supported by this camera model";
      return false;
    }
    int min_val, max_val;
    if (camera.getCameraSettingsRange(it->second, min_val, max_val) ==
            sl::ERROR_CODE::SUCCESS &&
        (setting.value < min_val || setting.value > max_val)) {
      error = "Camera setting '" + setting.name + "' (" +
              std::to_string(setting.value) + ") is out of range [" +
              std::to_string(min_val) + ", " + std::to_string(max_val) + "]";
      return false;
    }
  }
  error.clear();
  return true;
}

bool applyCameraSettings(sl::Camera& camera,
                         const std::vector<CameraSetting>& settings,
                         std::string& error) {
  // 'settings' already holds the values first and the mode switches last
  for (const auto& setting : settings) {
    auto err = camera.setCameraSettings(settingIds().at(setting.name), setting.value);
    if (err != sl::ERROR_CODE::SUCCESS) {
      error = "Cannot set camera setting '" + setting.name + "' to " +
              std::to_string(setting.value) + ": " + std::string(sl::toString(err).c_str());
      return false;
    }
  }

  for (int i = 0; i < SETTLE_FRAMES; i++) camera.grab();

  for (const auto& setting : settings) {
    int value = 0;
    if (!readSetting(camera, settingIds().at(setting.name), value)) {
      error = "Cannot read camera setting '" + setting.name + "' back";
      return false;
    }
    if (value != setting.value) {
      error = "Camera setting '" + setting.name + "' was set to " +
              std::to_string(setting.value) + " but reads back as " +
              std::to_string(value);
      return false;
    }
  }
  error.clear();
  return true;
}

void printCameraSettings(sl::Camera& camera, const std::string& header) {
  std::cout << " * " << header << ":" << std::endl;
  for (const auto& name : videoSettingNames()) {
    auto it = settingIds().find(name);
    int value;
    if (it == settingIds().end() || !readSetting(camera, it->second, value))
      continue;
    std::cout << "   - " << name << ": " << value;
    if (isModeSwitch(name)) std::cout << (value ? " (automatic)" : " (manual)");
    std::cout << std::endl;
  }
}

void restoreCameraSettings(sl::Camera& camera,
                           const CameraSettingsSnapshot& snapshot) {
  // Every value first, including those an automatic mode drives: the ZED 2i
  // keeps reporting the last value set (e.g. WHITEBALANCE_TEMPERATURE) even
  // after the automatic mode is back on
  for (const auto& [name, value] : snapshot.values)
    if (!isModeSwitch(name)) camera.setCameraSettings(settingIds().at(name), value);
  // Mode switches last: they undo the manual mode set by the values above
  for (const auto& [name, value] : snapshot.values)
    if (isModeSwitch(name)) camera.setCameraSettings(settingIds().at(name), value);
}

void CameraSettingsGuard::restore() {
  if (restored_) return;
  restoreCameraSettings(camera_, snapshot_);
  restored_ = true;
}

void installInterruptHandler() {
  std::signal(SIGINT, [](int) { interrupt_requested = true; });
}

bool interruptRequested() { return interrupt_requested; }

void uninstallInterruptHandler() { std::signal(SIGINT, SIG_DFL); }
