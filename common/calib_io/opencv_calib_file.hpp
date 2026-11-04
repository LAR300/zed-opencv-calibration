#pragma once

// The calibration file read by the ZED SDK (optional_opencv_calibration_file)
// and the conversion from the SDK's raw parameters. OpenCV only, so it can be
// unit tested without a camera; zed_raw_parameters.hpp fills the raw
// parameters from the SDK. One place for the conversion, used by the
// calibration tool and by zed_export_factory, so they cannot diverge.
// Spec: .ai/specs/exportar-fabrica/spec.md

#include <array>
#include <string>

#include <opencv2/core.hpp>

// One camera as the SDK gives it: distortion [k1, k2, p1, p2, k3, k4, k5, k6,
// s1, s2, s3, s4]
struct RawCameraParameters {
  double fx = 0.0, fy = 0.0, cx = 0.0, cy = 0.0;
  std::array<double, 12> disto{};
};

// The stereo pair as the SDK gives it: translation with a positive Tx (mm),
// rotation as a Rodrigues vector
struct RawStereoParameters {
  cv::Size image_size;
  RawCameraParameters left, right;
  cv::Vec3d translation;
  cv::Vec3d rotation_vector;
};

// The same pair in the OpenCV convention of the calibration file
struct OpenCVStereoCalibration {
  cv::Size image_size;
  bool fisheye = false;
  cv::Mat K_left, D_left, K_right, D_right;
  cv::Mat Rv;  // 3x1 Rodrigues vector
  cv::Mat T;   // 3x1, mm, negative Tx
};

// Rational model: the first 8 coefficients. Fisheye (p1 = p2 = 0 with k3, k4
// set, the rule the tools already use): k1, k2, k3, k4. Prism terms (s1..s4)
// are refused unless 'drop_prism' is set, since the file cannot hold them.
bool convertRawParameters(const RawStereoParameters& raw, bool drop_prism,
                          OpenCVStereoCalibration& calib, std::string& error);

// "rational8" or "fisheye4"
std::string modelName(const OpenCVStereoCalibration& calib);

// Write Size, K_LEFT, K_RIGHT, D_LEFT/D_RIGHT (or D_LEFT_FE/D_RIGHT_FE), R, T
// in the order and format of the original tool. 'refuse_existing' stops
// instead of overwriting.
bool writeOpenCVCalibration(const std::string& path, const OpenCVStereoCalibration& calib,
                            bool refuse_existing, std::string& error);

// Read a calibration file: D with 5, 8 or 14 columns (only zeros beyond the
// 8th) or the fisheye keys D_LEFT_FE/D_RIGHT_FE. Rational D is returned 1x8.
bool readOpenCVCalibration(const std::string& path, OpenCVStereoCalibration& calib,
                           std::string& error);

// Every value within 'rel_tol' relative difference (absolute for values near
// zero). 'diff' names the first value that differs.
bool sameRawParameters(const RawStereoParameters& a, const RawStereoParameters& b,
                       double rel_tol, std::string& diff);
