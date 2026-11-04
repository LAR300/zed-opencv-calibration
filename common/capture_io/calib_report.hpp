#pragma once

// calib_cpp_<model>.report.json: what the calibration of a session capture
// did and how well the saved parameters fit. Every RMS is the per-corner 2D
// RMS of OpenCV (rms_convention), with the per-coordinate MSE next to it.
// Spec: .ai/specs/saida-calibracao/spec.md (R3-R7)

#include <optional>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

constexpr int CALIB_REPORT_SCHEMA_VERSION = 1;

struct ViewReport {
  std::string pose_id;
  int n = 0;
  int corners_left = 0;   // corners used in the calibration (common tags)
  int corners_right = 0;
  std::optional<double> stereo_error_left;   // per-view RMS in the final stereo fit
  std::optional<double> stereo_error_right;
  std::string removed_stage;                 // "", "mono" or "stereo"
  std::optional<double> removed_error;       // view RMS when it was removed
};

struct RmsSet {
  std::optional<double> stereo;            // returned by the final stereo optimisation
  std::optional<double> stereo_fit_left;   // final parameters, poses of the stereo fit
  std::optional<double> stereo_fit_right;
  std::optional<double> free_pose_left;    // final intrinsics, one free pose per view
  std::optional<double> free_pose_right;
  std::optional<double> mono_stage_left;   // last mono calibration, before the stereo fit
  std::optional<double> mono_stage_right;
};

struct CalibrationReport {
  std::string model;
  std::string calibration_file;
  std::string prior;
  double max_rms_px = 0.0;
  double outlier_factor = 0.0;
  double outlier_floor_px = 0.0;
  std::string mono_flags;
  std::string stereo_flags;
  std::string term_criteria;
  cv::Size image_size;
  int pairs_input = 0;
  int pairs_used = 0;
  RmsSet rms;
  bool accepted = false;
  std::vector<std::string> std_dev_names;  // intrinsics of the last mono calibration
  std::vector<double> std_dev_left;
  std::vector<double> std_dev_right;
  std::string code_commit;
  bool code_dirty = false;
  std::vector<ViewReport> views;
};

bool writeCalibrationReport(const std::string& path, const CalibrationReport& report,
                            std::string& error);
