#pragma once

// Pose script (roteiro) of the traceable capture: the ordered list of target
// poses, and the geometry that tells, from a target pose, how far the current
// view is from the expected one. OpenCV only, testable without a camera.
// Spec: .ai/specs/roteiro-poses/spec.md
//
// Angle conventions, in the optical frame of the left camera (x right, y down,
// z forward), shared with analysis/zedcal (cobertura):
//  * tilt_h: positive when the top edge of the target is farther from the
//    camera than its bottom edge (rotation about the camera x axis)
//  * tilt_v: positive when the right edge is farther than the left edge
//    (rotation about the camera y axis)
//  * rotation: angle in the image of the target X axis, 0 with the target
//    upright, growing clockwise (image y points down), in [0, 360)

#include <string>
#include <vector>

#include <opencv2/core.hpp>

struct Tolerances {
  double fraction = 0.05;   // of the image width, beyond the pose's own range
  double position = 0.08;   // normalised image coordinates
  double angle_deg = 5.0;   // tilts and rotation
};

struct ScriptPose {
  std::string id;
  int block = 0;
  double fraction_min = 0.0;  // fraction of the image width taken by the target
  double fraction_max = 1.0;
  cv::Point2d center;         // normalised (0..1) position of the target centre
  double tilt_h_deg = 0.0;
  double tilt_v_deg = 0.0;
  double rotation_deg = 0.0;
};

struct PoseScript {
  std::string name;
  Tolerances tolerances;
  std::vector<ScriptPose> poses;
};

// Read and validate a script file: unique ids in the B<block>-... format,
// fractions in (0, 2], positive tolerances, angles within +-90 (tilts) and
// [0, 360) (rotation)
bool loadPoseScript(const std::string& path, PoseScript& script, std::string& error);

// What a target pose looks like from the left camera
struct ViewGeometry {
  double fraction = 0.0;      // projected width of the whole target / image width (may be > 1)
  cv::Point2d center;         // normalised position of the projected target centre
  double tilt_h_deg = 0.0;
  double tilt_v_deg = 0.0;
  double rotation_deg = 0.0;  // [0, 360)
  double distance_m = 0.0;    // camera to target centre
};

// 'board_size_mm': outer size of the target (AprilGrid: tags edge to edge).
// 'rvec', 'tvec': pose of the target frame (X right, Y up, origin at a corner)
// in the left camera.
ViewGeometry viewGeometry(const cv::Mat& K, const cv::Mat& D, cv::Size image_size,
                          cv::Size2d board_size_mm, const cv::Vec3d& rvec, const cv::Vec3d& tvec);

// Pose of the planar target (z = 0) from 4 or more corners, for the operator
// guidance only (roteiro-poses R7). Candidates: IPPE on points undistorted with
// tight criteria (the default undistortion is too coarse at the border of a
// wide-angle lens), IPPE as OpenCV does it and the iterative PnP; each refined
// with Levenberg-Marquardt, the lowest reprojection RMS wins. Same approach as
// the Python zedcal.residuals.pose. Rational model only. False with fewer than
// 4 points or no solution.
bool estimateTargetPose(const std::vector<cv::Point3f>& obj, const std::vector<cv::Point2f>& img,
                        const cv::Mat& K, const cv::Mat& D, cv::Vec3d& rvec, cv::Vec3d& tvec,
                        double* rms_px = nullptr);

// Per quantity: inside the pose's range and tolerances?
struct PoseCheck {
  bool fraction = false, position = false, tilt_h = false, tilt_v = false, rotation = false;
  bool all() const { return fraction && position && tilt_h && tilt_v && rotation; }
};
PoseCheck checkPose(const ViewGeometry& view, const ScriptPose& pose, const Tolerances& tol);

// Smallest absolute difference between two angles in degrees (wraps at 360)
double angleDifferenceDeg(double a, double b);
