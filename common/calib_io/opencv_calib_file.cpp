#include "opencv_calib_file.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <tuple>

#include <opencv2/core/persistence.hpp>

namespace {

bool isFisheye(const RawCameraParameters& cam) {
  return cam.disto[2] == 0.0 && cam.disto[3] == 0.0 && cam.disto[4] != 0.0 &&
         cam.disto[5] != 0.0;
}

bool hasPrism(const RawCameraParameters& cam) {
  for (int i = 8; i < 12; i++)
    if (cam.disto[i] != 0.0) return true;
  return false;
}

cv::Mat cameraMatrix(const RawCameraParameters& cam) {
  cv::Mat K = cv::Mat::eye(3, 3, CV_64FC1);
  K.at<double>(0, 0) = cam.fx;
  K.at<double>(1, 1) = cam.fy;
  K.at<double>(0, 2) = cam.cx;
  K.at<double>(1, 2) = cam.cy;
  return K;
}

cv::Mat distortion(const RawCameraParameters& cam, bool fisheye) {
  if (fisheye) {
    cv::Mat D = cv::Mat::zeros(1, 4, CV_64FC1);
    D.at<double>(0) = cam.disto[0];
    D.at<double>(1) = cam.disto[1];
    D.at<double>(2) = cam.disto[4];
    D.at<double>(3) = cam.disto[5];
    return D;
  }
  cv::Mat D = cv::Mat::zeros(1, 8, CV_64FC1);
  for (int i = 0; i < 8; i++) D.at<double>(i) = cam.disto[i];
  return D;
}

// Relative difference, with a small floor so that two zeros compare equal
// and small values (rotations, distortion terms) are still checked tightly
bool close(double a, double b, double rel_tol) {
  const double scale = std::max(std::abs(a), std::abs(b));
  return std::abs(a - b) <= rel_tol * std::max(scale, 1e-6);
}

}  // namespace

bool convertRawParameters(const RawStereoParameters& raw, bool drop_prism,
                          OpenCVStereoCalibration& calib, std::string& error) {
  error.clear();
  calib = OpenCVStereoCalibration();
  if (!drop_prism && (hasPrism(raw.left) || hasPrism(raw.right))) {
    error = "The calibration has thin prism distortion terms (s1..s4), which the "
            "8-coefficient calibration file cannot hold";
    return false;
  }
  const bool fisheye = isFisheye(raw.left);
  if (fisheye != isFisheye(raw.right)) {
    error = "The left and right cameras use different distortion models";
    return false;
  }
  calib.image_size = raw.image_size;
  calib.fisheye = fisheye;
  calib.K_left = cameraMatrix(raw.left);
  calib.K_right = cameraMatrix(raw.right);
  calib.D_left = distortion(raw.left, fisheye);
  calib.D_right = distortion(raw.right, fisheye);
  calib.T = cv::Mat::zeros(3, 1, CV_64FC1);
  calib.T.at<double>(0) = -raw.translation[0];  // the SDK stores a positive Tx
  calib.T.at<double>(1) = raw.translation[1];
  calib.T.at<double>(2) = raw.translation[2];
  calib.Rv = cv::Mat::zeros(3, 1, CV_64FC1);
  for (int i = 0; i < 3; i++) calib.Rv.at<double>(i) = raw.rotation_vector[i];
  return true;
}

std::string modelName(const OpenCVStereoCalibration& calib) {
  return calib.fisheye ? "fisheye4" : "rational8";
}

bool writeOpenCVCalibration(const std::string& path, const OpenCVStereoCalibration& calib,
                            bool refuse_existing, std::string& error) {
  error.clear();
  if (refuse_existing && std::filesystem::exists(path)) {
    error = "'" + path + "' already exists and is never overwritten";
    return false;
  }
  cv::FileStorage fs(path, cv::FileStorage::WRITE);
  if (!fs.isOpened()) {
    error = "Cannot write '" + path + "'";
    return false;
  }
  fs << "Size" << calib.image_size;
  fs << "K_LEFT" << calib.K_left << "K_RIGHT" << calib.K_right;
  if (!calib.fisheye)
    fs << "D_LEFT" << calib.D_left << "D_RIGHT" << calib.D_right;
  else
    fs << "D_LEFT_FE" << calib.D_left << "D_RIGHT_FE" << calib.D_right;
  fs << "R" << calib.Rv << "T" << calib.T;
  fs.release();
  return true;
}

bool readOpenCVCalibration(const std::string& path, OpenCVStereoCalibration& calib,
                           std::string& error) {
  error.clear();
  calib = OpenCVStereoCalibration();
  cv::FileStorage fs;
  try {
    fs.open(path, cv::FileStorage::READ);
  } catch (const cv::Exception& e) {
    error = "Cannot parse '" + path + "': " + e.what();
    return false;
  }
  if (!fs.isOpened()) {
    error = "Cannot open '" + path + "'";
    return false;
  }
  calib.fisheye = !fs["D_LEFT_FE"].empty();
  const char* d_left = calib.fisheye ? "D_LEFT_FE" : "D_LEFT";
  const char* d_right = calib.fisheye ? "D_RIGHT_FE" : "D_RIGHT";
  for (const char* key : {"Size", "K_LEFT", "K_RIGHT", d_left, d_right, "R", "T"}) {
    if (fs[key].empty()) {
      error = "'" + path + "' has no '" + key + "'";
      return false;
    }
  }
  fs["Size"] >> calib.image_size;
  fs["K_LEFT"] >> calib.K_left;
  fs["K_RIGHT"] >> calib.K_right;
  cv::Mat dl, dr;
  fs[d_left] >> dl;
  fs[d_right] >> dr;
  fs["R"] >> calib.Rv;
  fs["T"] >> calib.T;
  auto toModel = [&](const cv::Mat& d, cv::Mat& out, const char* key) {
    cv::Mat flat = d.reshape(1, 1);
    flat.convertTo(flat, CV_64F);
    if (calib.fisheye) {
      if (flat.cols != 4) {
        error = std::string(key) + " must have 4 coefficients";
        return false;
      }
      out = flat.clone();
      return true;
    }
    if (flat.cols != 5 && flat.cols != 8 && flat.cols != 14) {
      error = std::string(key) + " must have 5, 8 or 14 coefficients";
      return false;
    }
    for (int i = 8; i < flat.cols; i++)
      if (flat.at<double>(i) != 0.0) {
        error = std::string(key) + " has non-zero terms beyond the 8th (thin prism or tilt)";
        return false;
      }
    out = cv::Mat::zeros(1, 8, CV_64F);
    for (int i = 0; i < std::min(flat.cols, 8); i++) out.at<double>(i) = flat.at<double>(i);
    return true;
  };
  if (!toModel(dl, calib.D_left, d_left) || !toModel(dr, calib.D_right, d_right)) {
    error = "'" + path + "': " + error;
    return false;
  }
  calib.K_left.convertTo(calib.K_left, CV_64F);
  calib.K_right.convertTo(calib.K_right, CV_64F);
  calib.Rv = calib.Rv.reshape(1, 3);
  calib.T = calib.T.reshape(1, 3);
  calib.Rv.convertTo(calib.Rv, CV_64F);
  calib.T.convertTo(calib.T, CV_64F);
  return true;
}

bool sameRawParameters(const RawStereoParameters& a, const RawStereoParameters& b,
                       double rel_tol, std::string& diff) {
  diff.clear();
  std::ostringstream out;
  auto check = [&](const std::string& name, double x, double y) {
    if (diff.empty() && !close(x, y, rel_tol)) {
      out << name << ": " << x << " vs " << y;
      diff = out.str();
    }
  };
  if (a.image_size != b.image_size) {
    diff = "image size differs";
    return false;
  }
  for (const auto& [name, ca, cb] :
       {std::tuple{"left", &a.left, &b.left}, std::tuple{"right", &a.right, &b.right}}) {
    const std::string n = name;
    check(n + ".fx", ca->fx, cb->fx);
    check(n + ".fy", ca->fy, cb->fy);
    check(n + ".cx", ca->cx, cb->cx);
    check(n + ".cy", ca->cy, cb->cy);
    for (int i = 0; i < 12; i++)
      check(n + ".disto[" + std::to_string(i) + "]", ca->disto[i], cb->disto[i]);
  }
  for (int i = 0; i < 3; i++) {
    check("translation[" + std::to_string(i) + "]", a.translation[i], b.translation[i]);
    check("rotation_vector[" + std::to_string(i) + "]", a.rotation_vector[i],
          b.rotation_vector[i]);
  }
  return diff.empty();
}
