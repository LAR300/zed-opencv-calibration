#pragma once

// Static capture: is the target still, and is it sharp where it matters?
// The ZED 2i has a rolling shutter: a target that moves while the rows are
// read comes out deformed, not only blurred. Pure functions, OpenCV only.
// Spec: .ai/specs/captura-estatica/spec.md

#include <vector>

#include <opencv2/core.hpp>

#include "aprilgrid.hpp"

struct StabilityState {
  bool stable = false;            // still for at least the window
  bool indeterminate = false;     // too few corners in common with the previous frame
  double stable_for_s = 0.0;      // time since the last movement
  double displacement_px = 0.0;   // RMS displacement of the common corners to the previous frame
  double max_displacement_px = 0.0;  // largest displacement inside the current still window
};

// Follows the corners of the tags seen in two consecutive frames, in both
// cameras. A frame whose displacement exceeds the threshold, or with too few
// common corners, restarts the still window.
class StabilityTracker {
 public:
  StabilityTracker(double max_displacement_px, double window_s, int min_common_corners = 16)
      : max_displacement_px_(max_displacement_px), window_s_(window_s),
        min_common_corners_(min_common_corners) {}

  StabilityState update(const TagDetections& left, const TagDetections& right, double time_s);
  void reset() { has_previous_ = false; }

 private:
  double max_displacement_px_, window_s_;
  int min_common_corners_;
  bool has_previous_ = false;
  TagDetections prev_left_, prev_right_;
  double still_since_s_ = 0.0;
  double max_in_window_ = 0.0;
};

// Variance of the Laplacian inside the polygons of the detected tags only. The
// whole-image variance of the original tool lets a sharp background pass with
// a blurred target. Returns 0 when there is no tag.
double targetSharpness(const cv::Mat& image, const TagDetections& tags);
