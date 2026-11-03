#pragma once

// ZED capture settings on the camera: validate, apply, read back, print and
// restore. Uses the ZED SDK; the file reading is in camera_settings.hpp.
// Design notes: .ai/specs/camera-settings/decisions.md

#include <string>
#include <utility>
#include <vector>

#include <sl/Camera.hpp>

#include "camera_settings.hpp"

// Values of every supported single-value setting, taken before applying
struct CameraSettingsSnapshot {
  std::vector<std::pair<std::string, int>> values;
};

CameraSettingsSnapshot snapshotCameraSettings(sl::Camera& camera);

// Check that the connected camera supports every setting and that each
// value is within the range the camera reports
bool validateCameraSettings(sl::Camera& camera,
                            const std::vector<CameraSetting>& settings,
                            std::string& error);

// Apply the settings (values first, mode switches last), let a few frames
// go by, then read each one back and compare
bool applyCameraSettings(sl::Camera& camera,
                         const std::vector<CameraSetting>& settings,
                         std::string& error);

// Print every supported setting, one per line, under 'header'
void printCameraSettings(sl::Camera& camera, const std::string& header);

// Put the camera back as it was in 'snapshot' (decisions.md D3)
void restoreCameraSettings(sl::Camera& camera,
                           const CameraSettingsSnapshot& snapshot);

// Restores the snapshot when destroyed, unless restore() already ran
class CameraSettingsGuard {
 public:
  CameraSettingsGuard(sl::Camera& camera, CameraSettingsSnapshot snapshot)
      : camera_(camera), snapshot_(std::move(snapshot)) {}
  ~CameraSettingsGuard() { restore(); }
  void restore();
  CameraSettingsGuard(const CameraSettingsGuard&) = delete;
  CameraSettingsGuard& operator=(const CameraSettingsGuard&) = delete;

 private:
  sl::Camera& camera_;
  CameraSettingsSnapshot snapshot_;
  bool restored_ = false;
};

// Ctrl+C only raises a flag; the tools treat it like 'q' (decisions.md D5)
void installInterruptHandler();
bool interruptRequested();
// Give Ctrl+C its default behaviour back (e.g. once the camera is closed)
void uninstallInterruptHandler();
