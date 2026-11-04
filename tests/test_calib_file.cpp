#include <chrono>
#include <filesystem>

#include "opencv_calib_file.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace {

fs::path freshFile(const std::string& name) {
  static int n = 0;
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path dir = fs::temp_directory_path() /
                       ("calib_file_test_" + std::to_string(stamp) + "_" + std::to_string(n++));
  fs::create_directories(dir);
  return dir / name;
}

// Factory values of the ZED 2i S/N 35180697 in HD720 (SN35180697.conf)
RawStereoParameters zed2iHd720() {
  RawStereoParameters raw;
  raw.image_size = cv::Size(1280, 720);
  raw.left = {533.82, 533.845, 632.115, 369.065, {}};
  raw.right = {533.32, 533.435, 641.645, 370.337, {}};
  const double dl[8] = {-0.0584231, 0.0317579, 0.000313, -0.000123, -0.0124, 0.1, 0.2, 0.3};
  for (int i = 0; i < 8; i++) {
    raw.left.disto[i] = dl[i];
    raw.right.disto[i] = dl[i] * 1.01;
  }
  raw.translation = cv::Vec3d(119.982, -0.234264, 0.078238);  // SDK: positive Tx
  raw.rotation_vector = cv::Vec3d(0.00134, -0.000253943, 0.0021);
  return raw;
}

}  // namespace

TEST(calib_file_converts_translation_sign_and_rotation) {
  OpenCVStereoCalibration c;
  std::string error;
  CHECK(convertRawParameters(zed2iHd720(), false, c, error));
  CHECK(error.empty());
  CHECK_NEAR(c.T.at<double>(0), -119.982, 1e-12);  // OpenCV: negative Tx
  CHECK_NEAR(c.T.at<double>(1), -0.234264, 1e-12);
  CHECK_NEAR(c.T.at<double>(2), 0.078238, 1e-12);
  CHECK_NEAR(c.Rv.at<double>(1), -0.000253943, 1e-15);
  CHECK(c.image_size == cv::Size(1280, 720));
}

TEST(calib_file_rational_model_keeps_eight_coefficients) {
  OpenCVStereoCalibration c;
  std::string error;
  CHECK(convertRawParameters(zed2iHd720(), false, c, error));
  CHECK(!c.fisheye);
  CHECK(modelName(c) == "rational8");
  CHECK(c.D_left.cols == 8 && c.D_right.cols == 8);
  CHECK_NEAR(c.D_left.at<double>(0), -0.0584231, 1e-15);
  CHECK_NEAR(c.D_left.at<double>(7), 0.3, 1e-15);
  CHECK_NEAR(c.K_left.at<double>(0, 0), 533.82, 1e-12);
  CHECK_NEAR(c.K_left.at<double>(1, 1), 533.845, 1e-12);
  CHECK_NEAR(c.K_right.at<double>(0, 2), 641.645, 1e-12);
  CHECK_NEAR(c.K_right.at<double>(1, 2), 370.337, 1e-12);
}

TEST(calib_file_detects_fisheye_like_the_tools) {
  // p1 = p2 = 0 and k3, k4 (SDK indices 4, 5) not zero
  RawStereoParameters raw = zed2iHd720();
  for (auto* cam : {&raw.left, &raw.right}) {
    cam->disto = {};
    cam->disto[0] = 0.1;
    cam->disto[1] = -0.02;
    cam->disto[4] = 0.003;
    cam->disto[5] = -0.0004;
  }
  OpenCVStereoCalibration c;
  std::string error;
  CHECK(convertRawParameters(raw, false, c, error));
  CHECK(c.fisheye);
  CHECK(modelName(c) == "fisheye4");
  CHECK(c.D_left.cols == 4);
  CHECK_NEAR(c.D_left.at<double>(2), 0.003, 1e-15);
  CHECK_NEAR(c.D_left.at<double>(3), -0.0004, 1e-15);
}

TEST(calib_file_refuses_prism_terms) {
  // disto[8..11] are s1..s4 (thin prism), which the 8-coefficient file drops
  RawStereoParameters raw = zed2iHd720();
  raw.right.disto[9] = 1e-4;
  OpenCVStereoCalibration c;
  std::string error;
  CHECK(!convertRawParameters(raw, false, c, error));
  CHECK(error.find("prism") != std::string::npos);
  // The calibration prior may drop them: it is only an initial guess
  CHECK(convertRawParameters(raw, true, c, error));
  CHECK(c.D_right.cols == 8);
}

TEST(calib_file_refuses_mixed_models) {
  RawStereoParameters raw = zed2iHd720();
  raw.right.disto = {};
  raw.right.disto[4] = 0.003;
  raw.right.disto[5] = -0.0004;
  OpenCVStereoCalibration c;
  std::string error;
  CHECK(!convertRawParameters(raw, false, c, error));
  CHECK(error.find("model") != std::string::npos);
}

TEST(calib_file_writes_the_sdk_keys) {
  OpenCVStereoCalibration c;
  std::string error;
  CHECK(convertRawParameters(zed2iHd720(), false, c, error));
  const fs::path path = freshFile("calib.yml");
  CHECK(writeOpenCVCalibration(path.string(), c, false, error));
  cv::FileStorage fs(path.string(), cv::FileStorage::READ);
  CHECK(fs.isOpened());
  cv::Size size;
  fs["Size"] >> size;
  CHECK(size == cv::Size(1280, 720));
  cv::Mat K, D, R, T;
  fs["K_LEFT"] >> K;
  fs["D_RIGHT"] >> D;
  fs["R"] >> R;
  fs["T"] >> T;
  CHECK_NEAR(K.at<double>(0, 0), 533.82, 1e-12);
  CHECK(D.cols == 8);
  CHECK(R.rows == 3 && R.cols == 1);
  CHECK_NEAR(T.at<double>(0), -119.982, 1e-12);
}

TEST(calib_file_refuses_to_overwrite_when_asked) {
  OpenCVStereoCalibration c;
  std::string error;
  CHECK(convertRawParameters(zed2iHd720(), false, c, error));
  const fs::path path = freshFile("calib.yml");
  CHECK(writeOpenCVCalibration(path.string(), c, true, error));
  CHECK(!writeOpenCVCalibration(path.string(), c, true, error));
  CHECK(error.find("exists") != std::string::npos);
  CHECK(writeOpenCVCalibration(path.string(), c, false, error));  // old tools overwrite
}

TEST(calib_file_compares_raw_parameters) {
  const RawStereoParameters a = zed2iHd720();
  RawStereoParameters b = a;
  std::string diff;
  CHECK(sameRawParameters(a, b, 1e-4, diff));
  b.left.fx *= 1.00001;  // 1e-5 relative: within 1e-4
  CHECK(sameRawParameters(a, b, 1e-4, diff));
  b.left.fx = a.left.fx * 1.01;
  CHECK(!sameRawParameters(a, b, 1e-4, diff));
  CHECK(diff.find("left.fx") != std::string::npos);
  b = a;
  b.translation[0] += 0.5;
  CHECK(!sameRawParameters(a, b, 1e-4, diff));
  CHECK(diff.find("translation") != std::string::npos);
}

TEST(calib_file_reads_back_what_it_writes) {
  OpenCVStereoCalibration written, read;
  std::string error;
  CHECK(convertRawParameters(zed2iHd720(), false, written, error));
  const fs::path path = freshFile("calib.yml");
  CHECK(writeOpenCVCalibration(path.string(), written, true, error));
  CHECK(readOpenCVCalibration(path.string(), read, error));
  CHECK(read.image_size == written.image_size);
  CHECK(!read.fisheye);
  CHECK(cv::norm(read.K_left, written.K_left) < 1e-12);
  CHECK(cv::norm(read.D_right, written.D_right) < 1e-12);
  CHECK(cv::norm(read.T, written.T) < 1e-12);
  CHECK(cv::norm(read.Rv, written.Rv) < 1e-12);
}

TEST(calib_file_reads_14_coefficients_and_fisheye) {
  // The SLAM files have D 1x14 (rational + zeros)
  const fs::path path = freshFile("calib14.yml");
  {
    cv::FileStorage fs(path.string(), cv::FileStorage::WRITE);
    fs << "Size" << cv::Size(1280, 720);
    fs << "K_LEFT" << cv::Mat::eye(3, 3, CV_64F) << "K_RIGHT" << cv::Mat::eye(3, 3, CV_64F);
    cv::Mat D = cv::Mat::zeros(1, 14, CV_64F);
    D.at<double>(0) = 0.1;
    fs << "D_LEFT" << D << "D_RIGHT" << D;
    fs << "R" << cv::Mat::zeros(3, 1, CV_64F) << "T" << (cv::Mat_<double>(3, 1) << -120, 0, 0);
  }
  OpenCVStereoCalibration c;
  std::string error;
  CHECK(readOpenCVCalibration(path.string(), c, error));
  CHECK(c.D_left.cols == 8);
  CHECK_NEAR(c.D_left.at<double>(0), 0.1, 1e-15);

  // Non-zero terms beyond the 8th cannot be dropped silently
  const fs::path bad = freshFile("calib14_prism.yml");
  {
    cv::FileStorage fs(bad.string(), cv::FileStorage::WRITE);
    fs << "Size" << cv::Size(1280, 720);
    fs << "K_LEFT" << cv::Mat::eye(3, 3, CV_64F) << "K_RIGHT" << cv::Mat::eye(3, 3, CV_64F);
    cv::Mat D = cv::Mat::zeros(1, 14, CV_64F);
    D.at<double>(9) = 0.01;
    fs << "D_LEFT" << D << "D_RIGHT" << D;
    fs << "R" << cv::Mat::zeros(3, 1, CV_64F) << "T" << (cv::Mat_<double>(3, 1) << -120, 0, 0);
  }
  CHECK(!readOpenCVCalibration(bad.string(), c, error));
}

TEST(calib_file_read_rejects_missing_keys) {
  const fs::path path = freshFile("partial.yml");
  {
    cv::FileStorage fs(path.string(), cv::FileStorage::WRITE);
    fs << "Size" << cv::Size(1280, 720);
  }
  OpenCVStereoCalibration c;
  std::string error;
  CHECK(!readOpenCVCalibration(path.string(), c, error));
  CHECK(error.find("K_LEFT") != std::string::npos);
}
