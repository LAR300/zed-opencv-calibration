#pragma once

// Glue between the capture loop and the capture files: the terminal log and
// the conversion of the detector output to detections.json.
// Spec: .ai/specs/persistencia-captura/spec.md (R4, R6, R7)

#include <fstream>
#include <memory>
#include <sstream>
#include <streambuf>
#include <string>
#include <utility>
#include <vector>

#include "aprilgrid.hpp"
#include "detections_io.hpp"

// Duplicates std::cout and std::cerr into capture.log. It starts buffering in
// memory, because the first lines are printed before the capture folder
// exists; attachFile() writes them and keeps logging to the file. The
// streams are restored when the object is destroyed.
class LogTee {
 public:
  LogTee();
  ~LogTee();
  LogTee(const LogTee&) = delete;
  LogTee& operator=(const LogTee&) = delete;

  bool attachFile(const std::string& path, std::string& error);

 private:
  class Buffer;
  std::stringstream memory_;
  std::ofstream file_;
  std::unique_ptr<Buffer> out_, err_;
  std::streambuf* old_out_ = nullptr;
  std::streambuf* old_err_ = nullptr;
};

// The tags of one camera, with the corners after and before the refinement.
// A tag missing from 'unrefined' keeps its refined corners in both fields.
CameraDetections cameraDetectionsFromTags(const TagDetections& refined,
                                          const TagDetections& unrefined);

// A float as the decimal it was written as (0.3f -> 0.3, not 0.30000001):
// the target sizes are floats in the tools and doubles in detections.json
double floatToDecimal(float value);

// Header of detections.json for an AprilGrid capture
DetectionsHeader aprilgridDetectionsHeader(const AprilGridOptions& options,
                                           cv::Size image_size,
                                           const std::string& target_source);

// Image pairs of an old capture: <folder>image_{left,right}_<n>.png from
// n = 0 while both files exist, the rule calibrate() uses
std::vector<std::pair<std::string, std::string>> legacyImagePairs(const std::string& folder);

// The checkerboard detection of calibrate(): findChessboardCorners (flags 3)
// and cornerSubPix with an 11x11 window (15x15 from 3000 px wide). Shared so
// that detections.json and the calibration use the same corners.
bool detectCheckerboard(const cv::Mat& gray, cv::Size inner_corners,
                        std::vector<cv::Point2f>& corners);

// Header of detections.json for a checkerboard capture
DetectionsHeader checkerboardDetectionsHeader(cv::Size inner_corners, float square_size_mm,
                                              cv::Size image_size);
