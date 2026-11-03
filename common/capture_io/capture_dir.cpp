#include "capture_dir.hpp"

#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

bool prepareCaptureDir(const std::string& dir, std::string& error) {
  error.clear();
  const fs::path path(dir);
  std::error_code ec;

  if (fs::exists(path, ec)) {
    if (!fs::is_directory(path, ec)) {
      error = "The capture path '" + dir + "' exists and is not a folder";
      return false;
    }
    if (!fs::is_empty(path, ec)) {
      error = "The capture folder '" + dir +
              "' is not empty: an existing capture is never overwritten. Use another "
              "'capture' number in the session file";
      return false;
    }
  }

  for (const char* side : {"left", "right"}) {
    fs::create_directories(path / side, ec);
    if (ec) {
      error = "Cannot create '" + (path / side).string() + "': " + ec.message();
      return false;
    }
  }
  return true;
}

bool isValidPoseId(const std::string& pose_id) {
  static const std::regex pattern("[A-Za-z0-9-]+");
  return std::regex_match(pose_id, pattern);
}

bool relativeImagePath(const std::string& side, const std::string& pose_id, int n,
                       std::string& path, std::string& error) {
  path.clear();
  error.clear();
  if (side != "left" && side != "right") {
    error = "Image side '" + side + "' must be 'left' or 'right'";
    return false;
  }
  if (!isValidPoseId(pose_id)) {
    error = "Invalid pose id '" + pose_id + "': letters, digits and '-' only";
    return false;
  }
  if (n < 0) {
    error = "Image index " + std::to_string(n) + " must be >= 0";
    return false;
  }
  path = side + "/" + pose_id + "_" + std::to_string(n) + ".png";
  return true;
}
