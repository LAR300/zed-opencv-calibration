#pragma once

// How the stereo calibration opens the camera. Shared with zed_export_factory,
// so that the exported factory calibration is read at the same resolution,
// frame rate and without the SDK self-calibration.
// Spec: .ai/specs/exportar-fabrica/spec.md (R1)

#include <sl/Camera.hpp>

inline void setCalibrationOpenParameters(sl::InitParameters& init_params) {
  init_params.depth_mode = sl::DEPTH_MODE::NONE;           // no depth needed
  init_params.camera_resolution = sl::RESOLUTION::AUTO;    // HD720 on the ZED 2i
  init_params.camera_fps = 15;
  init_params.enable_image_validity_check = false;
  init_params.camera_disable_self_calib = true;            // raw factory parameters
}
