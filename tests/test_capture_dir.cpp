#include <chrono>
#include <filesystem>
#include <fstream>

#include "capture_dir.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace {

// A fresh path under the temporary directory. Tests never delete anything:
// the capture code must not delete files, and neither do its tests.
fs::path freshPath(const std::string& name) {
  static int n = 0;
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return fs::temp_directory_path() /
         ("capture_dir_test_" + std::to_string(stamp) + "_" + std::to_string(n++)) / name;
}

bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

}  // namespace

TEST(capture_dir_creates_new_folder_with_sides) {
  const fs::path dir = freshPath("A/captura_01");
  std::string error;
  CHECK(prepareCaptureDir(dir.string(), error));
  CHECK(error.empty());
  CHECK(fs::is_directory(dir / "left"));
  CHECK(fs::is_directory(dir / "right"));
}

TEST(capture_dir_accepts_existing_empty_folder) {
  const fs::path dir = freshPath("captura_02");
  fs::create_directories(dir);
  std::string error;
  CHECK(prepareCaptureDir(dir.string(), error));
  CHECK(fs::is_directory(dir / "left"));
}

TEST(capture_dir_refuses_folder_with_content_and_keeps_it) {
  const fs::path dir = freshPath("captura_03");
  fs::create_directories(dir);
  const fs::path kept = dir / "meta.json";
  std::ofstream(kept) << "{}";
  std::string error;
  CHECK(!prepareCaptureDir(dir.string(), error));
  CHECK(contains(error, "not empty"));
  CHECK(fs::exists(kept));
  CHECK(!fs::exists(dir / "left"));  // nothing was created either
}

TEST(capture_dir_refuses_folder_with_empty_subfolder) {
  // An earlier capture that only created left/ and right/ is still a capture
  const fs::path dir = freshPath("captura_04");
  fs::create_directories(dir / "left");
  std::string error;
  CHECK(!prepareCaptureDir(dir.string(), error));
}

TEST(capture_dir_refuses_a_file_in_its_place) {
  const fs::path dir = freshPath("captura_05");
  fs::create_directories(dir.parent_path());
  std::ofstream(dir) << "not a folder";
  std::string error;
  CHECK(!prepareCaptureDir(dir.string(), error));
  CHECK(contains(error, "not a folder"));
  CHECK(fs::is_regular_file(dir));
}

TEST(capture_dir_pose_ids) {
  CHECK(isValidPoseId("free"));
  CHECK(isValidPoseId("legacy"));
  CHECK(isValidPoseId("B1-P-TL"));
  CHECK(isValidPoseId("B2-M-MC-Hp35"));
  CHECK(isValidPoseId("MET-300-MC-1"));
  CHECK(!isValidPoseId(""));
  CHECK(!isValidPoseId("B1 P"));
  CHECK(!isValidPoseId("../x"));
  CHECK(!isValidPoseId("a/b"));
  CHECK(!isValidPoseId("a_b"));  // '_' separates the pose id from n
}

TEST(capture_dir_image_names) {
  std::string rel, error;
  CHECK(relativeImagePath("left", "free", 0, rel, error));
  CHECK(rel == "left/free_0.png");
  CHECK(relativeImagePath("right", "B3-P-TL-R090", 12, rel, error));
  CHECK(rel == "right/B3-P-TL-R090_12.png");
}

TEST(capture_dir_image_names_reject_bad_input) {
  std::string rel, error;
  CHECK(!relativeImagePath("center", "free", 0, rel, error));
  CHECK(contains(error, "side"));
  CHECK(!relativeImagePath("left", "a/b", 0, rel, error));
  CHECK(contains(error, "pose"));
  CHECK(!relativeImagePath("left", "free", -1, rel, error));
  CHECK(contains(error, "index"));
}

TEST(empty_dir_is_created_without_sides) {
  const fs::path dir = freshPath("fabrica/SN1");
  std::string error;
  CHECK(prepareEmptyDir(dir.string(), error));
  CHECK(fs::is_directory(dir));
  CHECK(fs::is_empty(dir));
}

TEST(empty_dir_refuses_existing_content) {
  const fs::path dir = freshPath("fabrica/SN2");
  fs::create_directories(dir);
  std::ofstream(dir / "calib_factory_rational8.yml") << "x";
  std::string error;
  CHECK(!prepareEmptyDir(dir.string(), error));
  CHECK(contains(error, "not empty"));
  CHECK(fs::exists(dir / "calib_factory_rational8.yml"));
}
