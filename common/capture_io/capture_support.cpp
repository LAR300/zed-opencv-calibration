#include "capture_support.hpp"

#include <iostream>

// Writes every character to the original stream, to the memory buffer while
// there is no file, and to the file once it is attached
class LogTee::Buffer : public std::streambuf {
 public:
  Buffer(std::streambuf* original, LogTee& owner) : original_(original), owner_(owner) {}

 protected:
  int overflow(int c) override {
    if (c == EOF) return !EOF;
    const char ch = static_cast<char>(c);
    if (original_) original_->sputc(ch);
    if (owner_.file_.is_open())
      owner_.file_.put(ch);
    else
      owner_.memory_.put(ch);
    return c;
  }

  std::streamsize xsputn(const char* s, std::streamsize n) override {
    if (original_) original_->sputn(s, n);
    if (owner_.file_.is_open())
      owner_.file_.write(s, n);
    else
      owner_.memory_.write(s, n);
    return n;
  }

  int sync() override {
    if (original_) original_->pubsync();
    if (owner_.file_.is_open()) owner_.file_.flush();
    return 0;
  }

 private:
  std::streambuf* original_;
  LogTee& owner_;
};

LogTee::LogTee() {
  old_out_ = std::cout.rdbuf();
  old_err_ = std::cerr.rdbuf();
  out_ = std::make_unique<Buffer>(old_out_, *this);
  err_ = std::make_unique<Buffer>(old_err_, *this);
  std::cout.rdbuf(out_.get());
  std::cerr.rdbuf(err_.get());
}

LogTee::~LogTee() {
  std::cout.flush();
  std::cerr.flush();
  std::cout.rdbuf(old_out_);
  std::cerr.rdbuf(old_err_);
  if (file_.is_open()) file_.close();
}

bool LogTee::attachFile(const std::string& path, std::string& error) {
  error.clear();
  file_.open(path, std::ios::out | std::ios::trunc);
  if (!file_.is_open()) {
    error = "Cannot write the capture log '" + path + "'";
    return false;
  }
  file_ << memory_.str();
  file_.flush();
  memory_.str("");
  return true;
}

CameraDetections cameraDetectionsFromTags(const TagDetections& refined,
                                          const TagDetections& unrefined) {
  CameraDetections cam;
  for (const auto& [id, corners] : refined) {  // std::map: sorted by ID
    if (corners.size() != 4) continue;
    TagObservation tag;
    tag.id = id;
    auto raw = unrefined.find(id);
    const bool has_raw = raw != unrefined.end() && raw->second.size() == 4;
    for (int k = 0; k < 4; k++) {
      tag.corners[k] = cv::Point2d(corners[k].x, corners[k].y);
      const cv::Point2f& r = has_raw ? raw->second[k] : corners[k];
      tag.corners_unrefined[k] = cv::Point2d(r.x, r.y);
    }
    cam.tags.push_back(tag);
  }
  return cam;
}

DetectionsHeader aprilgridDetectionsHeader(const AprilGridOptions& options,
                                           cv::Size image_size,
                                           const std::string& target_source) {
  DetectionsHeader h;
  h.image_size = image_size;
  h.target.type = "aprilgrid";
  h.target.rows = options.target.rows;
  h.target.cols = options.target.cols;
  h.target.tag_size_mm = options.target.tag_size;
  h.target.tag_spacing_ratio = options.target.tag_spacing;
  h.target.source = target_source;
  h.detector_refinement = "xjunction_cornersubpix";
  h.detector_params = detectorParameterList(options.detector_params);
  return h;
}
