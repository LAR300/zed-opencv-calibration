#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "aprilgrid.hpp"
#include "detections_io.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace {

fs::path freshFile(const std::string& name) {
  static int n = 0;
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path dir = fs::temp_directory_path() /
                       ("detections_test_" + std::to_string(stamp) + "_" + std::to_string(n++));
  fs::create_directories(dir);
  return dir / name;
}

std::string readAll(const fs::path& path) {
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

DetectionsHeader aprilgridHeader() {
  DetectionsHeader h;
  h.image_size = cv::Size(1280, 720);
  h.target.type = "aprilgrid";
  h.target.rows = 6;
  h.target.cols = 6;
  h.target.tag_size_mm = 88.0;
  h.target.tag_spacing_ratio = 0.3;
  h.target.source = "target.yaml";
  h.detector_refinement = "xjunction_cornersubpix";
  h.detector_params = detectorParameterList(defaultDetectorParameters());
  return h;
}

TagObservation tag(int id, double x0, double y0) {
  TagObservation t;
  t.id = id;
  // Values with many significant digits, to check the precision kept
  for (int k = 0; k < 4; k++) {
    t.corners[k] = cv::Point2d(x0 + k * 10.123456789012, y0 + k * 0.000000001);
    t.corners_unrefined[k] = cv::Point2d(x0 + k * 10.0, y0 - 0.5);
  }
  return t;
}

FrameDetections frame() {
  FrameDetections f;
  f.pose_id = "B1-P-TL";
  f.n = 0;
  f.file_left = "left/B1-P-TL_0.png";
  f.file_right = "right/B1-P-TL_0.png";
  f.left.tags = {tag(12, 100.5, 200.25), tag(13, 300.0, 200.0)};
  f.right.tags = {tag(12, 80.5, 201.0)};
  return f;
}

}  // namespace

TEST(detections_round_trip_keeps_ids_order_and_precision) {
  DetectionsWriter writer(aprilgridHeader());
  writer.addFrame(frame());
  const fs::path path = freshFile("detections.json");
  std::string error;
  CHECK(writer.write(path.string(), error));
  CHECK(error.empty());

  DetectionsHeader header;
  std::vector<FrameDetections> frames;
  CHECK(readDetections(path.string(), header, frames, error));
  if (!error.empty()) std::cerr << "  " << error << std::endl;
  CHECK(header.image_size == cv::Size(1280, 720));
  CHECK(header.target.type == "aprilgrid");
  CHECK(header.target.rows == 6 && header.target.cols == 6);
  CHECK_NEAR(header.target.tag_size_mm, 88.0, 1e-12);
  CHECK_NEAR(header.target.tag_spacing_ratio, 0.3, 1e-12);
  CHECK(header.detector_refinement == "xjunction_cornersubpix");
  CHECK(header.detector_params.size() == aprilgridHeader().detector_params.size());

  CHECK(frames.size() == 1);
  const FrameDetections& f = frames.at(0);
  CHECK(f.pose_id == "B1-P-TL" && f.n == 0);
  CHECK(f.file_left == "left/B1-P-TL_0.png");
  CHECK(f.left.tags.size() == 2 && f.right.tags.size() == 1);
  CHECK(f.left.tags[0].id == 12 && f.left.tags[1].id == 13);
  const TagObservation expected = tag(12, 100.5, 200.25);
  for (int k = 0; k < 4; k++) {
    // At least 9 significant digits (spec: condições de implementação)
    CHECK_NEAR(f.left.tags[0].corners[k].x, expected.corners[k].x, 1e-9);
    CHECK_NEAR(f.left.tags[0].corners[k].y, expected.corners[k].y, 1e-9);
    CHECK_NEAR(f.left.tags[0].corners_unrefined[k].y, expected.corners_unrefined[k].y, 1e-9);
  }
}

TEST(detections_records_corner_order_and_schema_version) {
  DetectionsWriter writer(aprilgridHeader());
  const fs::path path = freshFile("detections.json");
  std::string error;
  CHECK(writer.write(path.string(), error));
  const std::string text = readAll(path);
  CHECK(text.find("\"schema_version\": 1") != std::string::npos);
  CHECK(text.find("\"BR\"") != std::string::npos);
  CHECK(text.find("\"kalibr\"") != std::string::npos);
  CHECK(text.find("\"tag36h11\"") != std::string::npos);
  CHECK(text.find("\"adaptiveThreshConstant\"") != std::string::npos);
  CHECK(text.find("\"frames\"") != std::string::npos);
}

TEST(detections_without_frames_is_valid) {
  DetectionsWriter writer(aprilgridHeader());
  const fs::path path = freshFile("detections.json");
  std::string error;
  CHECK(writer.write(path.string(), error));
  DetectionsHeader header;
  std::vector<FrameDetections> frames;
  CHECK(readDetections(path.string(), header, frames, error));
  CHECK(frames.empty());
}

TEST(detections_camera_without_tags_is_an_empty_list) {
  DetectionsWriter writer(aprilgridHeader());
  FrameDetections f = frame();
  f.right.tags.clear();
  writer.addFrame(f);
  const fs::path path = freshFile("detections.json");
  std::string error;
  CHECK(writer.write(path.string(), error));
  DetectionsHeader header;
  std::vector<FrameDetections> frames;
  CHECK(readDetections(path.string(), header, frames, error));
  CHECK(frames.size() == 1 && frames[0].right.tags.empty());
}

TEST(detections_checkerboard_corners) {
  DetectionsHeader h;
  h.image_size = cv::Size(1280, 720);
  h.target.type = "checkerboard";
  h.target.rows = 6;
  h.target.cols = 9;
  h.target.square_size_mm = 23.8;
  h.detector_refinement = "cornersubpix_11x11";
  DetectionsWriter writer(h);
  FrameDetections f;
  f.pose_id = "legacy";
  f.n = 3;
  f.file_left = "zed-images/image_left_3.png";
  f.file_right = "zed-images/image_right_3.png";
  for (int i = 0; i < 54; i++) {
    f.left.corners.push_back(cv::Point2d(i * 1.5, i * 0.25));
    f.right.corners.push_back(cv::Point2d(i * 1.5 - 40.0, i * 0.25));
  }
  writer.addFrame(f);
  const fs::path path = freshFile("detections.json");
  std::string error;
  CHECK(writer.write(path.string(), error));

  DetectionsHeader header;
  std::vector<FrameDetections> frames;
  CHECK(readDetections(path.string(), header, frames, error));
  CHECK(header.target.type == "checkerboard");
  CHECK_NEAR(header.target.square_size_mm, 23.8, 1e-12);
  CHECK(frames.size() == 1);
  CHECK(frames[0].left.corners.size() == 54 && frames[0].left.tags.empty());
  CHECK_NEAR(frames[0].right.corners[53].x, 53 * 1.5 - 40.0, 1e-9);
}

TEST(detections_write_replaces_atomically) {
  // The file is rewritten after every saved pair; a reader never sees a
  // partial file, and no temporary file is left behind
  DetectionsWriter writer(aprilgridHeader());
  const fs::path path = freshFile("detections.json");
  std::string error;
  writer.addFrame(frame());
  CHECK(writer.write(path.string(), error));
  FrameDetections second = frame();
  second.n = 1;
  writer.addFrame(second);
  CHECK(writer.write(path.string(), error));
  DetectionsHeader header;
  std::vector<FrameDetections> frames;
  CHECK(readDetections(path.string(), header, frames, error));
  CHECK(frames.size() == 2);
  int files = 0;
  for (const auto& entry : fs::directory_iterator(path.parent_path())) {
    (void)entry;
    files++;
  }
  CHECK(files == 1);
}

TEST(detections_rejects_unknown_schema_version) {
  const fs::path path = freshFile("detections.json");
  std::ofstream(path) << "{\n  \"schema_version\": 2,\n  \"frames\": []\n}\n";
  DetectionsHeader header;
  std::vector<FrameDetections> frames;
  std::string error;
  CHECK(!readDetections(path.string(), header, frames, error));
  CHECK(error.find("schema_version") != std::string::npos);
}

TEST(detections_leaves_a_python_check_file) {
  // Written to a fixed path so that the Python json module can be run on it
  // after the tests (plan step 3): python3 -c "import json; json.load(open(P))"
  DetectionsWriter writer(aprilgridHeader());
  writer.addFrame(frame());
  std::string error;
  CHECK(writer.write((fs::temp_directory_path() / "zedcal_detections_check.json").string(),
                     error));
}
