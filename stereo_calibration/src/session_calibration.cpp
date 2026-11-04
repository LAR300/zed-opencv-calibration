#include "session_calibration.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>

#include "calib_report.hpp"
#include "opencv_calib_file.hpp"
#include "opencv_calibration.hpp"
#include "rms.hpp"

namespace {

const char* kStdDevNames[] = {"fx", "fy", "cx", "cy", "k1", "k2", "p1", "p2", "k3",
                              "k4", "k5", "k6", "s1", "s2", "s3", "s4", "tau_x", "tau_y"};

struct MonoResult {
  double rms = std::nan("");
  std::vector<double> std_dev;  // empty for fisheye
};

// The mono calibration of CameraCalib::mono_calibrate, through the Extended
// call so that the standard deviations are kept (same optimisation)
MonoResult monoCalibrate(CameraCalib& cam,
                         const std::vector<std::vector<cv::Point3f>>& obj,
                         const std::vector<std::vector<cv::Point2f>>& pts, cv::Size size,
                         int flags, bool verbose) {
  MonoResult result;
  if (!cam.disto_model_RadTan) {
    result.rms = cam.mono_calibrate(obj, pts, size, flags, verbose);
    return result;
  }
  if (cam.D.cols >= 8) flags += cv::CALIB_RATIONAL_MODEL;
  std::vector<cv::Mat> rvecs, tvecs;
  cv::Mat std_intrinsics, std_extrinsics, per_view;
  result.rms = cv::calibrateCamera(obj, pts, size, cam.K, cam.D, rvecs, tvecs, std_intrinsics,
                                   std_extrinsics, per_view, flags);
  for (int i = 0; i < std_intrinsics.rows * std_intrinsics.cols; i++)
    result.std_dev.push_back(std_intrinsics.at<double>(i));
  return result;
}

// Per-view RMS of both cameras together, as calibrate() computes it for the
// rejection: sqrt(sum of the squared errors of both images / (2 n))
double frameError(const std::vector<cv::Point2f>& proj_l, const std::vector<cv::Point2f>& pts_l,
                  const std::vector<cv::Point2f>& proj_r, const std::vector<cv::Point2f>& pts_r) {
  const int n = static_cast<int>(pts_l.size());
  double err_sq = 0.0;
  for (int j = 0; j < n; j++) {
    const double dx_l = proj_l[j].x - pts_l[j].x, dy_l = proj_l[j].y - pts_l[j].y;
    const double dx_r = proj_r[j].x - pts_r[j].x, dy_r = proj_r[j].y - pts_r[j].y;
    err_sq += dx_l * dx_l + dy_l * dy_l + dx_r * dx_r + dy_r * dy_r;
  }
  return std::sqrt(err_sq / (2 * n));
}

// Views above max(factor * median, floor); empty if fewer than 'frame_errors'
std::vector<int> outliers(const std::vector<std::pair<double, int>>& frame_errors,
                          double factor, double floor, double& threshold) {
  std::vector<int> out;
  if (frame_errors.empty()) return out;
  auto sorted = frame_errors;
  std::sort(sorted.begin(), sorted.end());
  threshold = std::max(factor * sorted[sorted.size() / 2].first, floor);
  for (const auto& [error, idx] : frame_errors)
    if (error > threshold) out.push_back(idx);
  return out;
}

template <typename T>
std::vector<T> without(const std::vector<T>& values, const std::vector<int>& removed) {
  std::vector<T> kept;
  for (int i = 0; i < static_cast<int>(values.size()); i++)
    if (std::find(removed.begin(), removed.end(), i) == removed.end()) kept.push_back(values[i]);
  return kept;
}

double freePoseRms(const std::vector<std::vector<cv::Point3f>>& obj,
                   const std::vector<std::vector<cv::Point2f>>& pts, const CameraCalib& cam) {
  std::vector<double> rms;
  std::vector<int> corners;
  for (size_t i = 0; i < obj.size(); i++) {
    cv::Mat rvec, tvec;
    if (!cv::solvePnP(obj[i], pts[i], cam.K, cam.D, rvec, tvec)) continue;
    std::vector<cv::Point2f> proj;
    cv::projectPoints(obj[i], rvec, tvec, cam.K, cam.D, proj);
    std::vector<cv::Point2d> residuals;
    for (size_t j = 0; j < proj.size(); j++)
      residuals.emplace_back(proj[j].x - pts[i][j].x, proj[j].y - pts[i][j].y);
    rms.push_back(rmsPerCorner(residuals));
    corners.push_back(static_cast<int>(proj.size()));
  }
  return aggregateRms(rms, corners);
}

}  // namespace

std::string sessionCalibrationFile(const std::string& output_dir, const std::string& model) {
  return output_dir + "/calib_cpp_" + model + ".yml";
}

std::string sessionReportFile(const std::string& output_dir, const std::string& model) {
  return output_dir + "/calib_cpp_" + model + ".report.json";
}

int calibrateSession(const std::vector<std::vector<cv::Point3f>>& object_points,
                     const std::vector<std::vector<cv::Point2f>>& pts_l,
                     const std::vector<std::vector<cv::Point2f>>& pts_r,
                     const std::vector<int>& input_index, int pairs_input,
                     cv::Size image_size, StereoCalib& calib, bool use_intrinsic_prior,
                     const SessionCalibration& session, bool verbose) {
  const CalibrationConfig& cfg = session.config;
  const bool radtan = calib.left.disto_model_RadTan && calib.right.disto_model_RadTan;
  const std::string model = radtan ? "rational8" : "fisheye4";
  const std::string calib_file = sessionCalibrationFile(session.output_dir, model);
  const std::string report_file = sessionReportFile(session.output_dir, model);

  CalibrationReport report;
  report.model = model;
  report.calibration_file = calib_file.substr(calib_file.find_last_of('/') + 1);
  report.prior = use_intrinsic_prior ? "factory" : "none";
  report.max_rms_px = cfg.max_rms_px;
  report.outlier_factor = cfg.outlier_factor;
  report.outlier_floor_px = cfg.outlier_floor_px;
  report.image_size = image_size;
  report.pairs_input = pairs_input;
  report.code_commit = session.code_commit;
  report.code_dirty = session.code_dirty;
  report.term_criteria = "COUNT+EPS, 30, DBL_EPSILON (mono); COUNT+EPS, 30, 1e-6 (stereo)";

  // Every input pair gets a view entry; views without enough tags keep no corners
  std::vector<ViewReport> views(pairs_input);
  for (int i = 0; i < pairs_input; i++) {
    if (i < static_cast<int>(session.view_ids.size())) {
      views[i].pose_id = session.view_ids[i].first;
      views[i].n = session.view_ids[i].second;
    } else {
      views[i].pose_id = "unknown";
      views[i].n = i;
    }
  }
  for (size_t v = 0; v < input_index.size(); v++) {
    views[input_index[v]].corners_left = static_cast<int>(pts_l[v].size());
    views[input_index[v]].corners_right = static_cast<int>(pts_r[v].size());
  }

  // --- mono (from scratch unless the prior is requested: saida-calibracao R1)
  const int mono_flags = use_intrinsic_prior ? cv::CALIB_USE_INTRINSIC_GUESS : 0;
  report.mono_flags = std::string(use_intrinsic_prior ? "CALIB_USE_INTRINSIC_GUESS" : "none") +
                      (radtan ? " | CALIB_RATIONAL_MODEL" : " (fisheye)");
  std::cout << std::endl << "*** Monocular cameras calibration *** " << std::endl;
  MonoResult mono_l = monoCalibrate(calib.left, object_points, pts_l, image_size, mono_flags, verbose);
  MonoResult mono_r = monoCalibrate(calib.right, object_points, pts_r, image_size, mono_flags, verbose);

  auto obj = object_points;
  auto l = pts_l;
  auto r = pts_r;
  auto index = input_index;  // input index of every view still in use

  // --- first per-view rejection, with the mono calibrations
  if (radtan) {
    std::vector<std::pair<double, int>> errors;
    for (int i = 0; i < static_cast<int>(l.size()); i++) {
      cv::Mat rv_l, tv_l, rv_r, tv_r;
      if (!cv::solvePnP(obj[i], l[i], calib.left.K, calib.left.D, rv_l, tv_l) ||
          !cv::solvePnP(obj[i], r[i], calib.right.K, calib.right.D, rv_r, tv_r))
        continue;
      std::vector<cv::Point2f> proj_l, proj_r;
      cv::projectPoints(obj[i], rv_l, tv_l, calib.left.K, calib.left.D, proj_l);
      cv::projectPoints(obj[i], rv_r, tv_r, calib.right.K, calib.right.D, proj_r);
      errors.emplace_back(frameError(proj_l, l[i], proj_r, r[i]), i);
    }
    double threshold = 0.0;
    const auto removed = outliers(errors, cfg.outlier_factor, cfg.outlier_floor_px, threshold);
    if (!removed.empty() && static_cast<int>(l.size() - removed.size()) >= MIN_IMAGE) {
      for (const auto& [error, i] : errors)
        if (std::find(removed.begin(), removed.end(), i) != removed.end()) {
          views[index[i]].removed_stage = "mono";
          views[index[i]].removed_error = error;
          std::cout << "  * Removing outlier view " << views[index[i]].pose_id << "_"
                    << views[index[i]].n << " (RMS " << std::fixed << std::setprecision(2)
                    << error << " px, threshold " << threshold << " px)" << std::endl;
        }
      obj = without(obj, removed);
      l = without(l, removed);
      r = without(r, removed);
      index = without(index, removed);
      mono_l = monoCalibrate(calib.left, obj, l, image_size, cv::CALIB_USE_INTRINSIC_GUESS, verbose);
      mono_r = monoCalibrate(calib.right, obj, r, image_size, cv::CALIB_USE_INTRINSIC_GUESS, verbose);
    } else if (!removed.empty()) {
      std::cout << "  * " << removed.size() << " outlier view(s) kept: too few views would remain"
                << std::endl;
    }
  }

  // --- stereo, intrinsics optimised jointly (as calibrate())
  std::cout << std::endl << "*** Stereo calibration *** " << std::endl;
  report.stereo_flags = std::string("CALIB_USE_INTRINSIC_GUESS") +
                        (radtan ? " | CALIB_RATIONAL_MODEL" : " | fisheye::CALIB_CHECK_COND");
  cv::Mat per_view;
  auto stereo = [&]() -> double {
    calib.imageSize = image_size;
    if (!radtan)
      return calib.stereo_calibrate(obj, l, r, image_size, cv::CALIB_USE_INTRINSIC_GUESS, verbose);
    cv::Mat R, T, E, F;
    const double rms = cv::stereoCalibrate(
        obj, l, r, calib.left.K, calib.left.D, calib.right.K, calib.right.D, image_size, R, T, E,
        F, per_view, cv::CALIB_USE_INTRINSIC_GUESS + cv::CALIB_RATIONAL_MODEL);
    calib.R = R;
    calib.T = T;
    cv::Rodrigues(calib.R, calib.Rv);
    return rms;
  };
  double stereo_rms = stereo();

  // --- second per-view rejection, with the left pose and R, T
  if (radtan) {
    std::vector<std::pair<double, int>> errors;
    for (int i = 0; i < static_cast<int>(l.size()); i++) {
      cv::Mat rvec, tvec;
      if (!cv::solvePnP(obj[i], l[i], calib.left.K, calib.left.D, rvec, tvec)) continue;
      std::vector<cv::Point2f> proj_l, proj_r;
      cv::projectPoints(obj[i], rvec, tvec, calib.left.K, calib.left.D, proj_l);
      cv::Mat R_l, rvec_r;
      cv::Rodrigues(rvec, R_l);
      cv::Mat t_r = calib.R * tvec + calib.T;
      cv::Rodrigues(calib.R * R_l, rvec_r);
      cv::projectPoints(obj[i], rvec_r, t_r, calib.right.K, calib.right.D, proj_r);
      errors.emplace_back(frameError(proj_l, l[i], proj_r, r[i]), i);
    }
    double threshold = 0.0;
    const auto removed = outliers(errors, cfg.outlier_factor, cfg.outlier_floor_px, threshold);
    if (!removed.empty() && static_cast<int>(l.size() - removed.size()) >= MIN_IMAGE) {
      for (const auto& [error, i] : errors)
        if (std::find(removed.begin(), removed.end(), i) != removed.end()) {
          views[index[i]].removed_stage = "stereo";
          views[index[i]].removed_error = error;
          std::cout << "  * Removing stereo outlier view " << views[index[i]].pose_id << "_"
                    << views[index[i]].n << " (RMS " << std::fixed << std::setprecision(2)
                    << error << " px, threshold " << threshold << " px)" << std::endl;
        }
      obj = without(obj, removed);
      l = without(l, removed);
      r = without(r, removed);
      index = without(index, removed);
      mono_l = monoCalibrate(calib.left, obj, l, image_size, cv::CALIB_USE_INTRINSIC_GUESS, verbose);
      mono_r = monoCalibrate(calib.right, obj, r, image_size, cv::CALIB_USE_INTRINSIC_GUESS, verbose);
      stereo_rms = stereo();
    } else if (!removed.empty()) {
      std::cout << "  * " << removed.size()
                << " stereo outlier view(s) kept: too few views would remain" << std::endl;
    }
  }
  std::cout << std::defaultfloat;

  // --- RMS of the saved parameters (saida-calibracao R3)
  report.pairs_used = static_cast<int>(l.size());
  report.rms.stereo = stereo_rms;
  report.rms.mono_stage_left = mono_l.rms;
  report.rms.mono_stage_right = mono_r.rms;
  if (radtan && per_view.rows == static_cast<int>(l.size())) {
    std::vector<double> e_l, e_r;
    std::vector<int> c_l, c_r;
    for (int i = 0; i < per_view.rows; i++) {
      e_l.push_back(per_view.at<double>(i, 0));
      e_r.push_back(per_view.at<double>(i, 1));
      c_l.push_back(static_cast<int>(l[i].size()));
      c_r.push_back(static_cast<int>(r[i].size()));
      views[index[i]].stereo_error_left = e_l.back();
      views[index[i]].stereo_error_right = e_r.back();
    }
    report.rms.stereo_fit_left = aggregateRms(e_l, c_l);
    report.rms.stereo_fit_right = aggregateRms(e_r, c_r);
  }
  if (radtan) {
    report.rms.free_pose_left = freePoseRms(obj, l, calib.left);
    report.rms.free_pose_right = freePoseRms(obj, r, calib.right);
  }
  for (size_t k = 0; k < mono_l.std_dev.size() && k < std::size(kStdDevNames); k++) {
    report.std_dev_names.push_back(kStdDevNames[k]);
    report.std_dev_left.push_back(mono_l.std_dev[k]);
    report.std_dev_right.push_back(k < mono_r.std_dev.size() ? mono_r.std_dev[k] : std::nan(""));
  }
  report.views = views;

  // Acceptance on the saved parameters: the stereo fit of each camera and the
  // stereo RMS (fisheye: the mono stage, the only per-camera value available)
  const double left = radtan ? *report.rms.stereo_fit_left : mono_l.rms;
  const double right = radtan ? *report.rms.stereo_fit_right : mono_r.rms;
  const double max = cfg.max_rms_px;
  constexpr double MIN_BASELINE = 30.0;
  std::cout << std::endl << "*** Calibration Report (session) ***" << std::endl
            << " * Reprojection errors, per-corner RMS of the saved parameters:" << std::endl
            << "   * Left:\t" << left << " px" << (left > max ? "\t!!! TOO HIGH !!!" : "\t-> GOOD")
            << std::endl
            << "   * Right:\t" << right << " px" << (right > max ? "\t!!! TOO HIGH !!!" : "\t-> GOOD")
            << std::endl
            << "   * Stereo:\t" << stereo_rms << " px"
            << (stereo_rms > max ? "\t!!! TOO HIGH !!!" : "\t-> GOOD") << std::endl
            << " * Mono stage (before the stereo fit): left " << mono_l.rms << " px, right "
            << mono_r.rms << " px" << std::endl;
  if (radtan)
    std::cout << " * Free pose per view (final intrinsics): left " << *report.rms.free_pose_left
              << " px, right " << *report.rms.free_pose_right << " px" << std::endl;
  std::cout << " * Views used: " << report.pairs_used << " of " << pairs_input << std::endl;

  std::string problem;
  if (left > max || right > max || stereo_rms > max)
    problem = "reprojection error above " + std::to_string(max) + " px";
  else if (calib.T.at<double>(0) > 0)
    problem = "baseline with the opposite sign (swap the cameras)";
  else if (std::fabs(calib.T.at<double>(0)) < MIN_BASELINE)
    problem = "baseline below " + std::to_string(MIN_BASELINE) + " mm";
  report.accepted = problem.empty();

  std::string error;
  if (!writeCalibrationReport(report_file, report, error)) {
    std::cerr << " !!! " << error << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << " * Report: " << report_file << std::endl;
  if (!report.accepted) {
    std::cerr << std::endl << "\t!!! ERROR !!! Calibration not accepted: " << problem
              << ". The report is written, the calibration file is not." << std::endl;
    return EXIT_FAILURE;
  }

  OpenCVStereoCalibration out;
  out.image_size = image_size;
  out.fisheye = !radtan;
  out.K_left = calib.left.K;
  out.D_left = calib.left.D;
  out.K_right = calib.right.K;
  out.D_right = calib.right.D;
  out.Rv = calib.Rv;
  out.T = calib.T;
  if (!writeOpenCVCalibration(calib_file, out, true, error)) {
    std::cerr << " !!! " << error << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << " * OpenCV calibration file saved: " << calib_file << std::endl;
  return EXIT_SUCCESS;
}
