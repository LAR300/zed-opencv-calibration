#include <filesystem>
#include <fstream>

#include "aprilgrid.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace {

const char* kKalibrYaml =
    "target_type: 'aprilgrid' #gridtype\n"
    "tagCols: 6               #number of apriltags\n"
    "tagRows: 6               #number of apriltags\n"
    "tagSize: 0.088           #size of apriltag, edge to edge [m]\n"
    "tagSpacing: 0.3          #ratio of space between tags to tagSize\n"
    "                         #example: tagSize=2m, spacing=0.5m --> "
    "tagSpacing=0.25[-]\n";

std::string writeTemp(const std::string& name, const std::string& content) {
  fs::path path = fs::temp_directory_path() / ("aprilgrid_test_" + name);
  std::ofstream out(path);
  out << content;
  return path.string();
}

std::string replaceLine(const std::string& yaml, const std::string& key,
                        const std::string& new_line) {
  std::string out;
  size_t start = 0;
  while (start < yaml.size()) {
    size_t end = yaml.find('\n', start);
    std::string line = yaml.substr(start, end - start);
    if (line.rfind(key, 0) == 0) line = new_line;
    if (!line.empty()) out += line + "\n";
    start = end + 1;
  }
  return out;
}

}  // namespace

TEST(yaml_loads_kalibr_target) {
  AprilGridTarget target;
  std::string error;
  CHECK(loadKalibrTarget(writeTemp("ok.yaml", kKalibrYaml), target, error));
  CHECK(error.empty());
  CHECK(target.rows == 6);
  CHECK(target.cols == 6);
  CHECK_NEAR(target.tag_size, 88.0, 1e-4);  // stored in mm
  CHECK_NEAR(target.tag_spacing, 0.3, 1e-6);
}

TEST(yaml_rejects_missing_file) {
  AprilGridTarget target;
  std::string error;
  CHECK(!loadKalibrTarget("/nonexistent/target.yaml", target, error));
  CHECK(error.find("/nonexistent/target.yaml") != std::string::npos);
}

TEST(yaml_rejects_missing_field) {
  AprilGridTarget target;
  std::string error;
  auto yaml = replaceLine(kKalibrYaml, "tagRows", "");
  CHECK(!loadKalibrTarget(writeTemp("nofield.yaml", yaml), target, error));
  CHECK(error.find("tagRows") != std::string::npos);
}

TEST(yaml_rejects_other_target_type) {
  AprilGridTarget target;
  std::string error;
  auto yaml = replaceLine(kKalibrYaml, "target_type",
                          "target_type: 'checkerboard'");
  CHECK(!loadKalibrTarget(writeTemp("type.yaml", yaml), target, error));
  CHECK(error.find("checkerboard") != std::string::npos);
}

TEST(yaml_rejects_invalid_grid_size) {
  AprilGridTarget target;
  std::string error;
  auto yaml = replaceLine(kKalibrYaml, "tagCols", "tagCols: 0");
  CHECK(!loadKalibrTarget(writeTemp("cols.yaml", yaml), target, error));
  CHECK(error.find("tagCols") != std::string::npos);
}

TEST(yaml_rejects_tag_size_not_in_meters) {
  AprilGridTarget target;
  std::string error;
  auto yaml = replaceLine(kKalibrYaml, "tagSize", "tagSize: 88");
  CHECK(!loadKalibrTarget(writeTemp("unit.yaml", yaml), target, error));
  CHECK(error.find("meters") != std::string::npos);
}

TEST(yaml_rejects_zero_spacing) {
  AprilGridTarget target;
  std::string error;
  auto yaml = replaceLine(kKalibrYaml, "tagSpacing", "tagSpacing: 0");
  CHECK(!loadKalibrTarget(writeTemp("spacing.yaml", yaml), target, error));
  CHECK(error.find("tagSpacing") != std::string::npos);
}

TEST(yaml_rejects_malformed_file) {
  AprilGridTarget target;
  std::string error;
  CHECK(!loadKalibrTarget(writeTemp("bad.yaml", "tagCols: [6\n"), target,
                          error));
  CHECK(!error.empty());
}
