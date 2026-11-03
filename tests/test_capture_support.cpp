#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "aprilgrid.hpp"
#include "capture_support.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace {

fs::path freshDir() {
  static int n = 0;
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path dir = fs::temp_directory_path() /
                       ("capture_support_test_" + std::to_string(stamp) + "_" + std::to_string(n++));
  fs::create_directories(dir);
  return dir;
}

std::string readAll(const fs::path& path) {
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::vector<cv::Point2f> square(float x, float y, float s) {
  return {{x + s, y + s}, {x, y + s}, {x, y}, {x + s, y}};
}

}  // namespace

TEST(log_tee_keeps_the_lines_before_the_file_and_after) {
  const fs::path log = freshDir() / "capture.log";
  {
    LogTee tee;  // starts buffering in memory
    std::cout << "before the folder exists" << std::endl;
    std::cerr << "an error before" << std::endl;
    std::string error;
    CHECK(tee.attachFile(log.string(), error));
    std::cout << "after" << std::endl;
  }
  std::cout << "not in the log" << std::endl;  // the tee is gone
  const std::string text = readAll(log);
  CHECK(text.find("before the folder exists") != std::string::npos);
  CHECK(text.find("an error before") != std::string::npos);
  CHECK(text.find("after") != std::string::npos);
  CHECK(text.find("not in the log") == std::string::npos);
}

TEST(log_tee_without_file_writes_nothing) {
  const fs::path dir = freshDir();
  {
    LogTee tee;
    std::cout << "discarded" << std::endl;
  }
  CHECK(fs::is_empty(dir));
}

TEST(camera_detections_keep_both_corner_sets_sorted_by_id) {
  TagDetections refined, unrefined;
  refined[7] = square(10.25f, 20.5f, 30.0f);
  refined[3] = square(100.0f, 50.0f, 30.0f);
  unrefined[7] = square(10.0f, 20.0f, 30.0f);
  unrefined[3] = square(99.5f, 49.5f, 30.0f);
  const CameraDetections cam = cameraDetectionsFromTags(refined, unrefined);
  CHECK(cam.tags.size() == 2);
  CHECK(cam.tags[0].id == 3 && cam.tags[1].id == 7);
  CHECK_NEAR(cam.tags[1].corners[2].x, 10.25, 1e-6);
  CHECK_NEAR(cam.tags[1].corners_unrefined[2].x, 10.0, 1e-6);
  CHECK(cam.corners.empty());
}

TEST(camera_detections_without_unrefined_copy_the_refined) {
  TagDetections refined, unrefined;
  refined[1] = square(1.0f, 2.0f, 3.0f);
  const CameraDetections cam = cameraDetectionsFromTags(refined, unrefined);
  CHECK(cam.tags.size() == 1);
  CHECK_NEAR(cam.tags[0].corners_unrefined[0].x, cam.tags[0].corners[0].x, 1e-12);
}

TEST(aprilgrid_header_describes_target_and_detector) {
  AprilGridOptions options;
  options.target.rows = 6;
  options.target.cols = 6;
  options.target.tag_size = 88.0f;
  options.target.tag_spacing = 0.3f;
  const DetectionsHeader h =
      aprilgridDetectionsHeader(options, cv::Size(1280, 720), "target.yaml");
  CHECK(h.image_size == cv::Size(1280, 720));
  CHECK(h.target.type == "aprilgrid");
  CHECK(h.target.rows == 6 && h.target.cols == 6);
  CHECK_NEAR(h.target.tag_size_mm, 88.0, 1e-6);
  CHECK_NEAR(h.target.tag_spacing_ratio, 0.3, 1e-6);
  CHECK(h.target.source == "target.yaml");
  CHECK(h.detector_refinement == "xjunction_cornersubpix");
  bool has_constant = false;
  for (const auto& [name, value] : h.detector_params)
    if (name == "adaptiveThreshConstant") has_constant = value == 25.0;
  CHECK(has_constant);
}

TEST(legacy_pairs_stop_at_the_first_missing_index) {
  // Same rule as calibrate(): image_{left,right}_<n>.png from 0 while both exist
  const fs::path dir = freshDir();
  for (int i : {0, 1, 2, 4}) {
    std::ofstream(dir / ("image_left_" + std::to_string(i) + ".png")) << "x";
    std::ofstream(dir / ("image_right_" + std::to_string(i) + ".png")) << "x";
  }
  std::ofstream(dir / "image_left_3.png") << "x";  // right_3 missing
  const auto pairs = legacyImagePairs(dir.string() + "/");
  CHECK(pairs.size() == 3);
  CHECK(pairs[2].first == dir.string() + "/image_left_2.png");
  CHECK(pairs[2].second == dir.string() + "/image_right_2.png");
}

TEST(checkerboard_header_describes_the_board) {
  const DetectionsHeader h = checkerboardDetectionsHeader(cv::Size(9, 6), 23.8f,
                                                          cv::Size(1280, 720));
  CHECK(h.target.type == "checkerboard");
  CHECK(h.target.cols == 9 && h.target.rows == 6);
  CHECK(h.target.square_size_mm == 23.8);  // exact decimal, not 23.7999992
  CHECK(h.detector_refinement == "cornersubpix_11x11");
  CHECK(checkerboardDetectionsHeader(cv::Size(9, 6), 23.8f, cv::Size(3840, 2160))
            .detector_refinement == "cornersubpix_15x15");
}

TEST(checkerboard_detection_finds_a_synthetic_board) {
  // A rendered 10x7-square board has 9x6 inner corners
  const int square = 40, cols = 10, rows = 7, margin = 60;
  cv::Mat img(rows * square + 2 * margin, cols * square + 2 * margin, CV_8UC1, cv::Scalar(255));
  for (int r = 0; r < rows; r++)
    for (int c = 0; c < cols; c++)
      if ((r + c) % 2 == 0)
        cv::rectangle(img, cv::Rect(margin + c * square, margin + r * square, square, square),
                      cv::Scalar(0), cv::FILLED);
  std::vector<cv::Point2f> corners;
  CHECK(detectCheckerboard(img, cv::Size(9, 6), corners));
  CHECK(corners.size() == 54);
  cv::Mat blank(200, 200, CV_8UC1, cv::Scalar(128));
  CHECK(!detectCheckerboard(blank, cv::Size(9, 6), corners));
}

TEST(float_to_decimal_keeps_the_written_value) {
  CHECK(floatToDecimal(0.3f) == 0.3);
  CHECK(floatToDecimal(23.8f) == 23.8);
  CHECK(floatToDecimal(88.0f) == 88.0);
}
