#pragma once

// ZED capture settings file (--camera_settings): reading and fixed rules.
// OpenCV only (no ZED SDK) so it can be unit tested without a camera; the
// camera side is in zed_camera_settings.hpp.
// Design notes: .ai/specs/camera-settings/decisions.md

#include <string>
#include <vector>

struct CameraSetting {
  std::string name;  // sl::VIDEO_SETTINGS name, e.g. "EXPOSURE"
  int value;
};

// Every sl::VIDEO_SETTINGS name of ZED SDK 5.1, in the SDK order
const std::vector<std::string>& videoSettingNames();

// Mode switches: applied after the values they control (decisions.md D3)
bool isModeSwitch(const std::string& name);

// Read a settings file: "NAME: value" lines, '#' comments, with or without
// the OpenCV '%YAML:1.0' header. Rejects unknown, read-only and unsupported
// names, non-integer values, contradictory modes and white balance
// temperatures off the 100 K step. On success 'settings' holds the values
// first and the mode switches last.
bool loadCameraSettings(const std::string& path,
                        std::vector<CameraSetting>& settings,
                        std::string& error);
