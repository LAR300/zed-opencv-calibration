#pragma once

// Calibration of a session capture (--session, --recalibrate): the same steps
// as calibrate() (mono, per-view rejection, stereo, per-view rejection), with
// the Extended OpenCV calls, the removed views recorded, the RMS of the saved
// parameters, and calib_cpp_<model>.yml plus its report in the capture folder.
// Spec: .ai/specs/saida-calibracao/spec.md

#include <string>
#include <utility>
#include <vector>

#include <opencv2/core.hpp>

#include "session.hpp"

struct StereoCalib;

struct SessionCalibration {
  CalibrationConfig config;
  // pose_id and n of every input pair, in the order calibrate() loads them
  std::vector<std::pair<std::string, int>> view_ids;
  std::string output_dir;
  std::string code_commit;
  bool code_dirty = false;
};

// 'input_index' gives, for every detected view, its index among the input
// pairs. Returns EXIT_SUCCESS when the calibration is accepted and written.
int calibrateSession(const std::vector<std::vector<cv::Point3f>>& object_points,
                     const std::vector<std::vector<cv::Point2f>>& pts_l,
                     const std::vector<std::vector<cv::Point2f>>& pts_r,
                     const std::vector<int>& input_index, int pairs_input,
                     cv::Size image_size, StereoCalib& calib, bool use_intrinsic_prior,
                     const SessionCalibration& session, bool verbose);

// The two files a session calibration writes, for a model ("rational8")
std::string sessionCalibrationFile(const std::string& output_dir, const std::string& model);
std::string sessionReportFile(const std::string& output_dir, const std::string& model);
