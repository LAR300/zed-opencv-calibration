#include "detections_io.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

#include <opencv2/core/persistence.hpp>

namespace {

const char* kCornerOrder[] = {"BR", "BL", "TL", "TR"};

void writePoint(cv::FileStorage& fs, const cv::Point2d& p) {
  fs << "[" << p.x << p.y << "]";
}

void writeCamera(cv::FileStorage& fs, const char* key, const CameraDetections& cam,
                 bool aprilgrid) {
  fs << key << "{";
  if (aprilgrid) {
    fs << "tags" << "[";
    for (const auto& tag : cam.tags) {
      fs << "{" << "id" << tag.id;
      fs << "corners" << "[";
      for (const auto& p : tag.corners) writePoint(fs, p);
      fs << "]";
      fs << "corners_unrefined" << "[";
      for (const auto& p : tag.corners_unrefined) writePoint(fs, p);
      fs << "]" << "}";
    }
    fs << "]";
  } else {
    fs << "corners" << "[";
    for (const auto& p : cam.corners) writePoint(fs, p);
    fs << "]";
  }
  fs << "}";
}

bool readPoint(const cv::FileNode& node, cv::Point2d& p) {
  if (!node.isSeq() || node.size() != 2) return false;
  p.x = static_cast<double>(node[0]);
  p.y = static_cast<double>(node[1]);
  return true;
}

bool readCamera(const cv::FileNode& node, bool aprilgrid, CameraDetections& cam,
                std::string& error) {
  if (!node.isMap()) {
    error = "a camera entry must be a map";
    return false;
  }
  if (aprilgrid) {
    for (const auto& t : node["tags"]) {
      TagObservation tag;
      tag.id = static_cast<int>(t["id"]);
      cv::FileNode corners = t["corners"], raw = t["corners_unrefined"];
      if (corners.size() != 4 || raw.size() != 4) {
        error = "tag " + std::to_string(tag.id) + " must have 4 corners";
        return false;
      }
      for (int k = 0; k < 4; k++) {
        if (!readPoint(corners[k], tag.corners[k]) ||
            !readPoint(raw[k], tag.corners_unrefined[k])) {
          error = "bad corner in tag " + std::to_string(tag.id);
          return false;
        }
      }
      cam.tags.push_back(tag);
    }
  } else {
    for (const auto& c : node["corners"]) {
      cv::Point2d p;
      if (!readPoint(c, p)) {
        error = "bad checkerboard corner";
        return false;
      }
      cam.corners.push_back(p);
    }
  }
  return true;
}

}  // namespace

bool DetectionsWriter::write(const std::string& path, std::string& error) const {
  error.clear();
  const bool aprilgrid = header_.target.type == "aprilgrid";
  std::string text;
  try {
    cv::FileStorage fs(".json", cv::FileStorage::WRITE | cv::FileStorage::MEMORY |
                                    cv::FileStorage::FORMAT_JSON);
    fs << "schema_version" << DETECTIONS_SCHEMA_VERSION;
    fs << "image_size" << "[" << header_.image_size.width << header_.image_size.height
       << "]";

    const DetectionTarget& t = header_.target;
    fs << "target" << "{" << "type" << t.type << "rows" << t.rows << "cols" << t.cols;
    if (aprilgrid) {
      fs << "tag_size_mm" << t.tag_size_mm << "tag_spacing_ratio" << t.tag_spacing_ratio
         << "family" << "tag36h11" << "id_convention" << "kalibr";
      fs << "corner_order" << "[";
      for (const char* c : kCornerOrder) fs << c;
      fs << "]";
    } else {
      fs << "square_size_mm" << t.square_size_mm << "corner_order" << "row_major";
    }
    fs << "source" << t.source << "}";

    fs << "detector" << "{" << "refinement" << header_.detector_refinement;
    fs << "params" << "{";
    for (const auto& [name, value] : header_.detector_params) fs << name << value;
    fs << "}" << "}";

    fs << "frames" << "[";
    for (const auto& f : frames_) {
      fs << "{" << "pose_id" << f.pose_id << "n" << f.n;
      fs << "files" << "{" << "left" << f.file_left << "right" << f.file_right << "}";
      writeCamera(fs, "left", f.left, aprilgrid);
      writeCamera(fs, "right", f.right, aprilgrid);
      fs << "}";
    }
    fs << "]";
    text = fs.releaseAndGetString();
  } catch (const cv::Exception& e) {
    error = std::string("Cannot encode detections: ") + e.what();
    return false;
  }

  const std::string tmp = path + ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) {
      error = "Cannot write '" + tmp + "'";
      return false;
    }
    out << text;
    if (!out.good()) {
      error = "Cannot write '" + tmp + "'";
      return false;
    }
  }
  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    error = "Cannot rename '" + tmp + "' to '" + path + "': " + ec.message();
    return false;
  }
  return true;
}

bool readDetections(const std::string& path, DetectionsHeader& header,
                    std::vector<FrameDetections>& frames, std::string& error) {
  header = DetectionsHeader();
  frames.clear();
  error.clear();

  cv::FileStorage fs;
  try {
    fs.open(path, cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
  } catch (const cv::Exception& e) {
    error = "Cannot parse '" + path + "': " + e.what();
    return false;
  }
  if (!fs.isOpened()) {
    error = "Cannot open '" + path + "'";
    return false;
  }

  const int version = fs["schema_version"].isInt() ? static_cast<int>(fs["schema_version"]) : -1;
  if (version != DETECTIONS_SCHEMA_VERSION) {
    error = "'" + path + "' has schema_version " + std::to_string(version) +
            ", only " + std::to_string(DETECTIONS_SCHEMA_VERSION) + " is supported";
    return false;
  }

  cv::FileNode size = fs["image_size"];
  if (size.isSeq() && size.size() == 2)
    header.image_size = cv::Size(static_cast<int>(size[0]), static_cast<int>(size[1]));

  cv::FileNode t = fs["target"];
  header.target.type = static_cast<std::string>(t["type"]);
  header.target.rows = static_cast<int>(t["rows"]);
  header.target.cols = static_cast<int>(t["cols"]);
  header.target.source = static_cast<std::string>(t["source"]);
  const bool aprilgrid = header.target.type == "aprilgrid";
  if (aprilgrid) {
    header.target.tag_size_mm = static_cast<double>(t["tag_size_mm"]);
    header.target.tag_spacing_ratio = static_cast<double>(t["tag_spacing_ratio"]);
  } else if (header.target.type == "checkerboard") {
    header.target.square_size_mm = static_cast<double>(t["square_size_mm"]);
  } else {
    error = "'" + path + "' has an unknown target type '" + header.target.type + "'";
    return false;
  }

  cv::FileNode d = fs["detector"];
  header.detector_refinement = static_cast<std::string>(d["refinement"]);
  for (const auto& p : d["params"])
    header.detector_params.emplace_back(p.name(), static_cast<double>(p));

  for (const auto& f : fs["frames"]) {
    FrameDetections frame;
    frame.pose_id = static_cast<std::string>(f["pose_id"]);
    frame.n = static_cast<int>(f["n"]);
    frame.file_left = static_cast<std::string>(f["files"]["left"]);
    frame.file_right = static_cast<std::string>(f["files"]["right"]);
    std::string camera_error;
    if (!readCamera(f["left"], aprilgrid, frame.left, camera_error) ||
        !readCamera(f["right"], aprilgrid, frame.right, camera_error)) {
      error = "'" + path + "', frame " + frame.pose_id + "_" + std::to_string(frame.n) +
              ": " + camera_error;
      return false;
    }
    frames.push_back(frame);
  }
  return true;
}
