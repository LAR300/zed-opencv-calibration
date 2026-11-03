#pragma once

// detections.json: the corners detected in every saved image pair, per
// camera, with all the tags each camera saw. Written with cv::FileStorage in
// JSON, so no new dependency; read back for the tests and the tools.
// Spec: .ai/specs/persistencia-captura/spec.md (R6, R7, R10; section Formatos)

#include <array>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/core.hpp>

constexpr int DETECTIONS_SCHEMA_VERSION = 1;

// Description of the target, enough to rebuild the 3D points without the code
struct DetectionTarget {
  std::string type;  // "aprilgrid" or "checkerboard"
  int rows = 0;      // aprilgrid: tag rows; checkerboard: inner corner rows
  int cols = 0;      // aprilgrid: tag columns; checkerboard: inner corner columns
  double tag_size_mm = 0.0;        // aprilgrid
  double tag_spacing_ratio = 0.0;  // aprilgrid
  double square_size_mm = 0.0;     // checkerboard
  std::string source;              // file the target was read from, if any
};

// One AprilGrid tag: corners in the order of tagCorners3D() (BR, BL, TL, TR)
struct TagObservation {
  int id = -1;
  std::array<cv::Point2d, 4> corners;            // after the X-junction refinement
  std::array<cv::Point2d, 4> corners_unrefined;  // as returned by the ArUco detector
};

struct CameraDetections {
  std::vector<TagObservation> tags;  // aprilgrid
  std::vector<cv::Point2d> corners;  // checkerboard: inner corners, row-major
};

struct FrameDetections {
  std::string pose_id;
  int n = 0;
  std::string file_left;   // relative to the capture folder
  std::string file_right;
  CameraDetections left;
  CameraDetections right;
};

struct DetectionsHeader {
  cv::Size image_size;
  DetectionTarget target;
  std::string detector_refinement;
  std::vector<std::pair<std::string, double>> detector_params;
};

class DetectionsWriter {
 public:
  explicit DetectionsWriter(DetectionsHeader header) : header_(std::move(header)) {}

  void addFrame(const FrameDetections& frame) { frames_.push_back(frame); }
  size_t frameCount() const { return frames_.size(); }

  // Write the whole file. It goes to '<path>.tmp' first and is then renamed,
  // so that the file on disk is always complete.
  bool write(const std::string& path, std::string& error) const;

 private:
  DetectionsHeader header_;
  std::vector<FrameDetections> frames_;
};

// Read a detections.json (schema version 1). Any other version is refused.
bool readDetections(const std::string& path, DetectionsHeader& header,
                    std::vector<FrameDetections>& frames, std::string& error);
