#pragma once

// Glue between the capture loop and the capture files: the terminal log and
// the conversion of the detector output to detections.json.
// Spec: .ai/specs/persistencia-captura/spec.md (R4, R6, R7)

#include <fstream>
#include <memory>
#include <sstream>
#include <streambuf>
#include <string>

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

// Header of detections.json for an AprilGrid capture
DetectionsHeader aprilgridDetectionsHeader(const AprilGridOptions& options,
                                           cv::Size image_size,
                                           const std::string& target_source);
