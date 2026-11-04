#include "static_capture.hpp"

#include <cmath>

#include <opencv2/imgproc.hpp>

namespace {

// Sum of squared displacements of the corners of the tags present in both frames
void accumulate(const TagDetections& prev, const TagDetections& curr, double& sum, int& count) {
  for (const auto& [id, corners] : curr) {
    auto it = prev.find(id);
    if (it == prev.end() || it->second.size() != corners.size()) continue;
    for (size_t k = 0; k < corners.size(); k++) {
      const cv::Point2f d = corners[k] - it->second[k];
      sum += d.x * d.x + d.y * d.y;
      count++;
    }
  }
}

}  // namespace

StabilityState StabilityTracker::update(const TagDetections& left, const TagDetections& right,
                                        double time_s) {
  StabilityState s;
  if (!has_previous_) {
    has_previous_ = true;
    still_since_s_ = time_s;
    max_in_window_ = 0.0;
    s.indeterminate = true;
  } else {
    double sum = 0.0;
    int count = 0;
    accumulate(prev_left_, left, sum, count);
    accumulate(prev_right_, right, sum, count);
    if (count < min_common_corners_) {
      s.indeterminate = true;
      still_since_s_ = time_s;
      max_in_window_ = 0.0;
    } else {
      s.displacement_px = std::sqrt(sum / count);
      if (s.displacement_px > max_displacement_px_) {
        still_since_s_ = time_s;
        max_in_window_ = 0.0;
      } else {
        max_in_window_ = std::max(max_in_window_, s.displacement_px);
      }
    }
  }
  prev_left_ = left;
  prev_right_ = right;
  s.stable_for_s = time_s - still_since_s_;
  s.max_displacement_px = max_in_window_;
  s.stable = !s.indeterminate && s.stable_for_s >= window_s_;
  return s;
}

double targetSharpness(const cv::Mat& image, const TagDetections& tags) {
  if (tags.empty()) return 0.0;
  cv::Mat gray;
  if (image.channels() == 3)
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
  else
    gray = image;
  cv::Mat mask = cv::Mat::zeros(gray.size(), CV_8U);
  std::vector<std::vector<cv::Point>> polygons;
  for (const auto& [id, corners] : tags) {
    std::vector<cv::Point> poly;
    for (const auto& c : corners) poly.emplace_back(cvRound(c.x), cvRound(c.y));
    polygons.push_back(poly);
  }
  cv::fillPoly(mask, polygons, cv::Scalar(255));
  cv::Mat lap;
  cv::Laplacian(gray, lap, CV_64F);
  cv::Scalar mean, stddev;
  cv::meanStdDev(lap, mean, stddev, mask);
  return stddev[0] * stddev[0];
}
