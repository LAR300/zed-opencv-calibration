#pragma once

// Copies the ZED SDK calibration into the SDK-free RawStereoParameters of
// opencv_calib_file.hpp. Field by field, no conversion: the conversion to the
// OpenCV convention happens in one place, convertRawParameters().

#include <sl/Camera.hpp>

#include "opencv_calib_file.hpp"

inline RawCameraParameters rawCameraParametersFrom(const sl::CameraParameters& cam) {
  RawCameraParameters raw;
  raw.fx = static_cast<double>(cam.fx);
  raw.fy = static_cast<double>(cam.fy);
  raw.cx = static_cast<double>(cam.cx);
  raw.cy = static_cast<double>(cam.cy);
  for (int i = 0; i < 12; i++) raw.disto[i] = cam.disto[i];
  return raw;
}

inline RawStereoParameters rawStereoParametersFrom(const sl::CalibrationParameters& p) {
  RawStereoParameters raw;
  raw.image_size = cv::Size(static_cast<int>(p.left_cam.image_size.width),
                            static_cast<int>(p.left_cam.image_size.height));
  raw.left = rawCameraParametersFrom(p.left_cam);
  raw.right = rawCameraParametersFrom(p.right_cam);
  const auto t = p.stereo_transform.getTranslation();
  raw.translation = cv::Vec3d(t.x, t.y, t.z);
  const auto r = p.stereo_transform.getRotationVector();
  raw.rotation_vector = cv::Vec3d(r.x, r.y, r.z);
  return raw;
}
