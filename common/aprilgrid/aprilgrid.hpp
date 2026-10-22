#pragma once

// Kalibr AprilGrid calibration target support, shared by the stereo tools.
// OpenCV only (no ZED SDK) so it can be unit tested without a camera.
// Design notes: .ai/specs/aprilgrid-target/decisions.md

#include <map>
#include <string>
#include <vector>

#include <opencv2/aruco.hpp>
#include <opencv2/opencv.hpp>

struct AprilGridTarget {
  int rows = 0;             // Number of tag rows (Kalibr 'tagRows')
  int cols = 0;             // Number of tag columns (Kalibr 'tagCols')
  float tag_size = 0.0f;    // Tag size, edge to edge, in mm (Kalibr 'tagSize' is in m)
  float tag_spacing = 0.0f; // Space between tags as a ratio of the tag size (Kalibr 'tagSpacing')

  int tagCount() const { return rows * cols; }
  float spacingMm() const { return tag_size * tag_spacing; }
  float pitchMm() const { return tag_size + spacingMm(); }
  // Physical size of the tag area, from the outer edge of the first tag to the
  // outer edge of the last one
  cv::Size2f boardSizeMm() const {
    return cv::Size2f(cols * tag_size + (cols - 1) * spacingMm(),
                      rows * tag_size + (rows - 1) * spacingMm());
  }
};

// Load a Kalibr 'target.yaml' describing an AprilGrid.
// Returns false and fills 'error' if the file cannot be read or is not a valid
// AprilGrid description.
bool loadKalibrTarget(const std::string& path, AprilGridTarget& target,
                      std::string& error);

// Tag layout (Kalibr convention, see decisions.md D5):
//  * IDs increase left -> right along a row, then rows bottom -> top
//  * Board frame: X right, Y up, origin at the outer corner of tag 0, in mm
bool isValidTagId(const AprilGridTarget& target, int id);
int tagRow(const AprilGridTarget& target, int id);
int tagCol(const AprilGridTarget& target, int id);

// 3D corners of a tag, in the order OpenCV returns them for a Kalibr tag:
// [bottom-right, bottom-left, top-left, top-right] in board coordinates.
// Empty if 'id' is not part of the grid.
std::vector<cv::Point3f> tagCorners3D(const AprilGridTarget& target, int id);

// Tag detection parameters. The defaults are tuned for a Kalibr AprilGrid
// (decisions.md D9, D11); 'markerBorderBits' is always 2, as Kalibr prints its
// tags with a 2-bit border.
cv::Ptr<cv::aruco::DetectorParameters> defaultDetectorParameters();

// Load detector parameters from an OpenCV YAML file using the field names of
// cv::aruco::DetectorParameters (e.g. 'cornerRefinementMethod: 3'). Fields not
// present keep the defaults. Unknown fields and a 'markerBorderBits' other than
// 2 are reported in 'warning'.
bool loadDetectorParameters(const std::string& path,
                            cv::Ptr<cv::aruco::DetectorParameters>& params,
                            std::string& error, std::string& warning);

// Detected tags: tag ID -> its 4 image corners, in the order of tagCorners3D()
using TagDetections = std::map<int, std::vector<cv::Point2f>>;

// Tags with a side shorter than this are discarded: their corners are too
// inaccurate for a calibration
constexpr float MIN_TAG_SIDE_PX = 10.0f;

// Detect the AprilGrid tags in a grayscale or BGR image. Only complete tags
// whose ID belongs to the grid are returned; an ID detected more than once
// (e.g. a reflection) is discarded. Corners are refined on the X-junctions
// with the Kalibr corner squares (decisions.md D11).
TagDetections detectAprilGrid(
    const cv::Mat& image, const AprilGridTarget& target,
    const cv::Ptr<cv::aruco::DetectorParameters>& params);

// Minimum tags for a valid sample (spec R7), seen by both cameras. All three
// can be changed from the command line.
constexpr int DEFAULT_MIN_TAGS = 12;
constexpr int DEFAULT_MIN_TAG_ROWS = 2;
constexpr int DEFAULT_MIN_TAG_COLS = 2;

// 3D/2D correspondences of the detected tags, sorted by tag ID. Returns the IDs.
std::vector<int> tagCorrespondences(const AprilGridTarget& target,
                                    const TagDetections& detections,
                                    std::vector<cv::Point3f>& object_points,
                                    std::vector<cv::Point2f>& image_points);

// Same as tagCorrespondences() for a stereo pair, keeping only the tags seen
// in both images, as required by cv::stereoCalibrate. Returns the common IDs.
std::vector<int> matchCommonTags(const AprilGridTarget& target,
                                 const TagDetections& left,
                                 const TagDetections& right,
                                 std::vector<cv::Point3f>& object_points,
                                 std::vector<cv::Point2f>& image_points_left,
                                 std::vector<cv::Point2f>& image_points_right);

// Check that a set of tags is large and spread enough to be a sample. On
// failure 'reason' tells what is missing.
bool meetsMinimum(const AprilGridTarget& target, const std::vector<int>& ids,
                  int min_tags, int min_rows, int min_cols,
                  std::string& reason);

// Image corners of the board region covered by the detected tags: the
// bounding rectangle of the tags on the board, projected with a homography.
// Cyclic order: board top-left, top-right, bottom-right, bottom-left.
// Empty if there is no detection. Used for the coverage metrics, which were
// designed for the 4 outside corners of a full checkerboard (decisions.md D6).
std::vector<cv::Point2f> outsideCorners(const AprilGridTarget& target,
                                        const TagDetections& detections);

// Print the expected target features, in the style used for the checkerboard,
// so that the user can check them against the printed target
void printAprilGridTarget(const AprilGridTarget& target);

// Draw the detected tags, with their IDs, on a display image whose size may
// differ from the image the tags were detected in
void drawAprilGridDetections(cv::Mat& display, const TagDetections& detections,
                             cv::Size detection_size);

// Everything a tool needs to use an AprilGrid instead of the checkerboard
struct AprilGridOptions {
  AprilGridTarget target;
  cv::Ptr<cv::aruco::DetectorParameters> detector_params =
      defaultDetectorParameters();
  int min_tags = DEFAULT_MIN_TAGS;
  int min_rows = DEFAULT_MIN_TAG_ROWS;
  int min_cols = DEFAULT_MIN_TAG_COLS;
};
