// Bench check (exportar-fabrica, plan step 4): the reprojection
// errors zed_stereo_checker shows (stereo_checker/src/main.cpp:127-146,
// 559-575), over N frames, with the calibration the SDK loads: the internal
// one, or an OpenCV file given as optional_opencv_calibration_file.
// Usage: checker_errors <target.yaml> <frames> [calib.yml|internal] [stored_images_folder/]
// With a folder, the stored pairs image_{left,right}_<n>.png are evaluated instead of live frames,
// and the per-pair errors are printed.
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <sl/Camera.hpp>
#include "aprilgrid.hpp"
#include "opencv_calib_file.hpp"
#include "zed_open.hpp"
#include "zed_raw_parameters.hpp"

static double rms(const std::vector<cv::Point3f>& obj, const std::vector<cv::Point2f>& img,
                  const cv::Mat& rvec, const cv::Mat& tvec, const cv::Mat& K, const cv::Mat& D) {
  std::vector<cv::Point2f> proj;
  cv::projectPoints(obj, rvec, tvec, K, D, proj);
  double sq = 0;
  for (size_t i = 0; i < img.size(); i++) {
    const double dx = proj[i].x - img[i].x, dy = proj[i].y - img[i].y;
    sq += dx * dx + dy * dy;
  }
  return std::sqrt(sq / img.size());
}
static double median(std::vector<double> v) {
  std::sort(v.begin(), v.end());
  return v.empty() ? -1 : v[v.size() / 2];
}
static double iqr(std::vector<double> v) {
  std::sort(v.begin(), v.end());
  return v.size() < 4 ? -1 : v[3 * v.size() / 4] - v[v.size() / 4];
}
int main(int argc, char** argv) {
  if (argc < 3) { std::cerr << "usage: checker_errors <target.yaml> <frames> [calib.yml]\n"; return 2; }
  AprilGridOptions opt; std::string error;
  if (!loadKalibrTarget(argv[1], opt.target, error)) { std::cerr << error << "\n"; return 1; }
  const int frames = std::stoi(argv[2]);
  sl::InitParameters ip; setCalibrationOpenParameters(ip); ip.sdk_verbose = 0;
  const std::string calib_arg = argc > 3 ? argv[3] : "internal";
  if (calib_arg != "internal") ip.optional_opencv_calibration_file = calib_arg.c_str();
  const std::string folder = argc > 4 ? argv[4] : "";
  sl::Camera cam;
  if (cam.open(ip) > sl::ERROR_CODE::SUCCESS) { std::cerr << "cannot open camera\n"; return 1; }
  OpenCVStereoCalibration c;
  if (!convertRawParameters(rawStereoParametersFrom(cam.getCameraInformation().camera_configuration.calibration_parameters_raw), false, c, error)) { std::cerr << error << "\n"; return 1; }
  cv::Mat R; cv::Rodrigues(c.Rv, R);
  const auto res = cam.getCameraInformation().camera_configuration.resolution;
  sl::Mat sl_l(res, sl::MAT_TYPE::U8_C3, sl::MEM::CPU), sl_r(res, sl::MAT_TYPE::U8_C3, sl::MEM::CPU);
  cv::Mat l(res.height, res.width, CV_8UC3, sl_l.getPtr<sl::uchar1>()), r(res.height, res.width, CV_8UC3, sl_r.getPtr<sl::uchar1>());
  std::vector<double> el, er, es;
  auto evaluate = [&](const cv::Mat& l, const cv::Mat& r, std::string& line) -> bool {
    auto tl = detectAprilGrid(l, opt.target, opt.detector_params);
    auto tr = detectAprilGrid(r, opt.target, opt.detector_params);
    std::vector<cv::Point3f> ol, orr; std::vector<cv::Point2f> pl, pr;
    tagCorrespondences(opt.target, tl, ol, pl);
    tagCorrespondences(opt.target, tr, orr, pr);
    if (ol.size() < 48 || orr.size() < 48) return false;
    cv::Mat rvl, tvl, rvr, tvr;
    cv::solvePnP(ol, pl, c.K_left, c.D_left, rvl, tvl);
    cv::solvePnP(orr, pr, c.K_right, c.D_right, rvr, tvr);
    const double e_l = rms(ol, pl, rvl, tvl, c.K_left, c.D_left);
    const double e_r = rms(orr, pr, rvr, tvr, c.K_right, c.D_right);
    cv::Mat Rl; cv::Rodrigues(rvl, Rl);
    cv::Mat Rr = R * Rl, tr_ = R * tvl + c.T, rv_s; cv::Rodrigues(Rr, rv_s);
    const double e_rs = rms(orr, pr, rv_s, tr_, c.K_right, c.D_right);
    const double e_s = std::sqrt((e_l * e_l + e_rs * e_rs) / 2.0);
    el.push_back(e_l); er.push_back(e_r); es.push_back(e_s);
    line = std::to_string(e_l) + " " + std::to_string(e_r) + " " + std::to_string(e_s);
    return true;
  };
  if (!folder.empty()) {
    cam.close();
    for (int i = 0;; i++) {
      cv::Mat l = cv::imread(folder + "image_left_" + std::to_string(i) + ".png");
      cv::Mat r = cv::imread(folder + "image_right_" + std::to_string(i) + ".png");
      if (l.empty() || r.empty()) break;
      std::string line;
      if (evaluate(l, r, line)) std::cout << "pair " << i << " " << line << std::endl;
    }
    std::cout << calib_arg << ": " << el.size() << " stored pairs, median (IQR) px"
              << " left " << median(el) << " (" << iqr(el) << ")"
              << " right " << median(er) << " (" << iqr(er) << ")"
              << " stereo " << median(es) << " (" << iqr(es) << ")" << std::endl;
    return 0;
  }
  for (int skip = 0; skip < 15; skip++) cam.grab();  // let exposure settle
  int grabs = 0, failed = 0;
  while ((int)el.size() < frames) {
    if (++grabs % 30 == 0)
      std::cerr << "grabs " << grabs << ", failed " << failed << ", valid " << el.size() << std::endl;
    if (grabs > 600) { std::cerr << "giving up\n"; return 1; }
    if (cam.grab() != sl::ERROR_CODE::SUCCESS) { failed++; continue; }
    cam.retrieveImage(sl_l, sl::VIEW::LEFT_UNRECTIFIED_BGR);
    cam.retrieveImage(sl_r, sl::VIEW::RIGHT_UNRECTIFIED_BGR);
    auto tl = detectAprilGrid(l, opt.target, opt.detector_params);
    auto tr = detectAprilGrid(r, opt.target, opt.detector_params);
    std::vector<cv::Point3f> ol, orr; std::vector<cv::Point2f> pl, pr;
    tagCorrespondences(opt.target, tl, ol, pl);
    tagCorrespondences(opt.target, tr, orr, pr);
    if (ol.size() < 48 || orr.size() < 48) {  // 12 tags each
      if (grabs % 30 == 1) std::cerr << "tags " << tl.size() << " + " << tr.size() << std::endl;
      if (grabs == 31) cv::imwrite("checker_errors_last_frame_left.png", l);  // in the current folder
      continue;
    }
    cv::Mat rvl, tvl, rvr, tvr;
    cv::solvePnP(ol, pl, c.K_left, c.D_left, rvl, tvl);
    cv::solvePnP(orr, pr, c.K_right, c.D_right, rvr, tvr);
    const double e_l = rms(ol, pl, rvl, tvl, c.K_left, c.D_left);
    const double e_r = rms(orr, pr, rvr, tvr, c.K_right, c.D_right);
    cv::Mat Rl; cv::Rodrigues(rvl, Rl);
    cv::Mat Rr = R * Rl, tr_ = R * tvl + c.T, rv_s; cv::Rodrigues(Rr, rv_s);
    const double e_rs = rms(orr, pr, rv_s, tr_, c.K_right, c.D_right);
    el.push_back(e_l); er.push_back(e_r); es.push_back(std::sqrt((e_l * e_l + e_rs * e_rs) / 2.0));
  }
  cam.close();
  std::cout << (argc > 3 ? argv[3] : "internal calibration") << ": " << frames << " frames, median (IQR) px"
            << " left " << median(el) << " (" << iqr(el) << ")"
            << " right " << median(er) << " (" << iqr(er) << ")"
            << " stereo " << median(es) << " (" << iqr(es) << ")" << std::endl;
  return 0;
}
