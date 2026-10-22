#include "aprilgrid.hpp"

#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>

bool loadKalibrTarget(const std::string& path, AprilGridTarget& target,
                      std::string& error) {
  std::ifstream file(path);
  if (!file.is_open()) {
    error = "Cannot open the target file '" + path + "'";
    return false;
  }
  std::stringstream content;
  content << file.rdbuf();

  // Kalibr files have no '%YAML' directive, which cv::FileStorage requires
  std::string yaml = "%YAML:1.0\n---\n" + content.str();

  cv::FileStorage fs;
  try {
    fs.open(yaml, cv::FileStorage::READ | cv::FileStorage::MEMORY);
  } catch (const cv::Exception& e) {
    error = "Cannot parse the target file '" + path + "': " + e.what();
    return false;
  }
  if (!fs.isOpened()) {
    error = "Cannot parse the target file '" + path + "'";
    return false;
  }

  const char* required_keys[] = {"target_type", "tagCols", "tagRows",
                                 "tagSize", "tagSpacing"};
  for (const char* key : required_keys) {
    if (fs[key].empty()) {
      error = "Missing field '" + std::string(key) + "' in '" + path + "'";
      return false;
    }
  }

  cv::FileNode type_node = fs["target_type"];
  std::string target_type = type_node.isString() ? type_node.string() : "";
  if (target_type != "aprilgrid") {
    error = "Unsupported target_type '" + target_type + "' in '" + path +
            "': only 'aprilgrid' is supported";
    return false;
  }

  auto readNumber = [&](const char* key, double& value) -> bool {
    cv::FileNode node = fs[key];
    if (!node.isInt() && !node.isReal()) {
      error = "Field '" + std::string(key) + "' in '" + path +
              "' must be a number";
      return false;
    }
    value = node.real();
    return true;
  };

  double cols, rows, tag_size_m, tag_spacing;
  if (!readNumber("tagCols", cols) || !readNumber("tagRows", rows) ||
      !readNumber("tagSize", tag_size_m) ||
      !readNumber("tagSpacing", tag_spacing))
    return false;

  if (cols < 1 || cols != static_cast<int>(cols)) {
    error = "Field 'tagCols' must be a positive integer";
    return false;
  }
  if (rows < 1 || rows != static_cast<int>(rows)) {
    error = "Field 'tagRows' must be a positive integer";
    return false;
  }
  // Kalibr expresses the tag size in meters: a value like '88' is almost
  // certainly millimeters and would silently scale the whole calibration
  if (tag_size_m <= 0.0 || tag_size_m >= 1.0) {
    std::ostringstream ss;
    ss << "Field 'tagSize' (" << tag_size_m
       << ") must be the tag size in meters, between 0 and 1 (e.g. 0.088)";
    error = ss.str();
    return false;
  }
  if (tag_spacing <= 0.0) {
    error = "Field 'tagSpacing' must be a positive ratio of the tag size";
    return false;
  }

  target.cols = static_cast<int>(cols);
  target.rows = static_cast<int>(rows);
  target.tag_size = static_cast<float>(tag_size_m * 1000.0);
  target.tag_spacing = static_cast<float>(tag_spacing);
  error.clear();
  return true;
}

bool isValidTagId(const AprilGridTarget& target, int id) {
  return id >= 0 && id < target.tagCount();
}

int tagRow(const AprilGridTarget& target, int id) { return id / target.cols; }

int tagCol(const AprilGridTarget& target, int id) { return id % target.cols; }

std::vector<cv::Point3f> tagCorners3D(const AprilGridTarget& target, int id) {
  std::vector<cv::Point3f> corners;
  if (!isValidTagId(target, id)) return corners;

  const float x0 = tagCol(target, id) * target.pitchMm();
  const float y0 = tagRow(target, id) * target.pitchMm();
  const float s = target.tag_size;

  corners.push_back(cv::Point3f(x0 + s, y0, 0.0f));      // bottom-right
  corners.push_back(cv::Point3f(x0, y0, 0.0f));          // bottom-left
  corners.push_back(cv::Point3f(x0, y0 + s, 0.0f));      // top-left
  corners.push_back(cv::Point3f(x0 + s, y0 + s, 0.0f));  // top-right
  return corners;
}

constexpr int KALIBR_MARKER_BORDER_BITS = 2;

cv::Ptr<cv::aruco::DetectorParameters> defaultDetectorParameters() {
  auto params = cv::aruco::DetectorParameters::create();
  params->adaptiveThreshWinSizeMin = 5;
  params->adaptiveThreshWinSizeMax = 21;
  params->adaptiveThreshWinSizeStep = 4;
  params->minMarkerPerimeterRate = 0.02;
  params->polygonalApproxAccuracyRate = 0.03;
  params->minCornerDistanceRate = 0.05;
  params->minDistanceToBorder = 3;
  params->minMarkerDistanceRate = 0.05;
  // The blurred seam between a tag and the Kalibr corner squares must stay
  // white, or the detector merges them (decisions.md D11)
  params->adaptiveThreshConstant = 25;
  // ArUco only locates the tags: detectAprilGrid() refines the corners on the
  // X-junctions with the corner squares
  params->cornerRefinementMethod = cv::aruco::CORNER_REFINE_NONE;
  params->markerBorderBits = KALIBR_MARKER_BORDER_BITS;
  return params;
}

bool loadDetectorParameters(const std::string& path,
                            cv::Ptr<cv::aruco::DetectorParameters>& params,
                            std::string& error, std::string& warning) {
  error.clear();
  warning.clear();
  params = defaultDetectorParameters();

  cv::FileStorage fs;
  try {
    fs.open(path, cv::FileStorage::READ);
  } catch (const cv::Exception& e) {
    error = "Cannot parse the detector parameters file '" + path + "': " +
            e.what();
    return false;
  }
  if (!fs.isOpened()) {
    error = "Cannot open the detector parameters file '" + path + "'";
    return false;
  }

  // OpenCV 4.5.4 has no DetectorParameters reader: the fields are read one by
  // one, with the same names as the OpenCV structure
  cv::aruco::DetectorParameters& p = *params;
  const std::map<std::string, int*> int_fields = {
      {"adaptiveThreshWinSizeMin", &p.adaptiveThreshWinSizeMin},
      {"adaptiveThreshWinSizeMax", &p.adaptiveThreshWinSizeMax},
      {"adaptiveThreshWinSizeStep", &p.adaptiveThreshWinSizeStep},
      {"minDistanceToBorder", &p.minDistanceToBorder},
      {"cornerRefinementMethod", &p.cornerRefinementMethod},
      {"cornerRefinementWinSize", &p.cornerRefinementWinSize},
      {"cornerRefinementMaxIterations", &p.cornerRefinementMaxIterations},
      {"markerBorderBits", &p.markerBorderBits},
      {"perspectiveRemovePixelPerCell", &p.perspectiveRemovePixelPerCell},
      {"aprilTagMinClusterPixels", &p.aprilTagMinClusterPixels},
      {"aprilTagMaxNmaxima", &p.aprilTagMaxNmaxima},
      {"aprilTagMinWhiteBlackDiff", &p.aprilTagMinWhiteBlackDiff},
      {"aprilTagDeglitch", &p.aprilTagDeglitch}};
  const std::map<std::string, double*> double_fields = {
      {"adaptiveThreshConstant", &p.adaptiveThreshConstant},
      {"minMarkerPerimeterRate", &p.minMarkerPerimeterRate},
      {"maxMarkerPerimeterRate", &p.maxMarkerPerimeterRate},
      {"polygonalApproxAccuracyRate", &p.polygonalApproxAccuracyRate},
      {"minCornerDistanceRate", &p.minCornerDistanceRate},
      {"minMarkerDistanceRate", &p.minMarkerDistanceRate},
      {"cornerRefinementMinAccuracy", &p.cornerRefinementMinAccuracy},
      {"perspectiveRemoveIgnoredMarginPerCell",
       &p.perspectiveRemoveIgnoredMarginPerCell},
      {"maxErroneousBitsInBorderRate", &p.maxErroneousBitsInBorderRate},
      {"minOtsuStdDev", &p.minOtsuStdDev},
      {"errorCorrectionRate", &p.errorCorrectionRate}};
  const std::map<std::string, float*> float_fields = {
      {"aprilTagQuadDecimate", &p.aprilTagQuadDecimate},
      {"aprilTagQuadSigma", &p.aprilTagQuadSigma},
      {"aprilTagCriticalRad", &p.aprilTagCriticalRad},
      {"aprilTagMaxLineFitMse", &p.aprilTagMaxLineFitMse}};

  std::ostringstream warnings;
  for (const auto& node : fs.root()) {
    const std::string key = node.name();
    const bool is_number = node.isInt() || node.isReal();
    auto int_it = int_fields.find(key);
    auto double_it = double_fields.find(key);
    auto float_it = float_fields.find(key);

    if (int_it == int_fields.end() && double_it == double_fields.end() &&
        float_it == float_fields.end() && key != "detectInvertedMarker") {
      warnings << "Unknown detector parameter '" << key << "' ignored. ";
      continue;
    }
    if (!is_number) {
      error = "Detector parameter '" + key + "' in '" + path +
              "' must be a number";
      return false;
    }
    if (int_it != int_fields.end()) {
      *int_it->second = static_cast<int>(node);
    } else if (double_it != double_fields.end()) {
      *double_it->second = static_cast<double>(node);
    } else if (float_it != float_fields.end()) {
      *float_it->second = static_cast<float>(node);
    } else {
      p.detectInvertedMarker = static_cast<int>(node) != 0;
    }
  }

  if (p.markerBorderBits != KALIBR_MARKER_BORDER_BITS) {
    warnings << "markerBorderBits=" << p.markerBorderBits
             << " ignored: Kalibr AprilGrid tags have a "
             << KALIBR_MARKER_BORDER_BITS << "-bit border. ";
    p.markerBorderBits = KALIBR_MARKER_BORDER_BITS;
  }

  warning = warnings.str();
  return true;
}

TagDetections detectAprilGrid(
    const cv::Mat& image, const AprilGridTarget& target,
    const cv::Ptr<cv::aruco::DetectorParameters>& params) {
  cv::Mat gray;
  if (image.channels() == 3)
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
  else
    gray = image;

  static const auto dictionary =
      cv::aruco::getPredefinedDictionary(cv::aruco::DICT_APRILTAG_36h11);

  std::vector<std::vector<cv::Point2f>> corners;
  std::vector<int> ids;
  cv::aruco::detectMarkers(gray, dictionary, corners, ids, params);

  TagDetections detections;
  std::map<int, int> id_count;
  for (int id : ids) id_count[id]++;

  for (size_t i = 0; i < ids.size(); i++) {
    const int id = ids[i];
    if (!isValidTagId(target, id) || id_count[id] > 1) continue;

    float min_side = std::numeric_limits<float>::max();
    for (int k = 0; k < 4; k++) {
      float side = static_cast<float>(
          cv::norm(corners[i][k] - corners[i][(k + 1) % 4]));
      min_side = std::min(min_side, side);
    }
    if (min_side < MIN_TAG_SIDE_PX) continue;

    // Refine on the X-junction formed by the tag corner and the Kalibr corner
    // square. The window reaches the junction from the contour estimate but
    // stays clear of the data bits, which start at 0.2 x the tag side.
    const int win = std::max(2, cvRound(0.15f * min_side));
    cv::cornerSubPix(gray, corners[i], cv::Size(win, win), cv::Size(-1, -1),
                     cv::TermCriteria(cv::TermCriteria::EPS |
                                          cv::TermCriteria::MAX_ITER,
                                      40, 0.001));

    detections[id] = corners[i];
  }
  return detections;
}

std::vector<int> tagCorrespondences(const AprilGridTarget& target,
                                    const TagDetections& detections,
                                    std::vector<cv::Point3f>& object_points,
                                    std::vector<cv::Point2f>& image_points) {
  std::vector<int> ids;
  object_points.clear();
  image_points.clear();
  for (const auto& [id, corners] : detections) {  // std::map: sorted by ID
    auto corners_3d = tagCorners3D(target, id);
    if (corners_3d.empty()) continue;
    object_points.insert(object_points.end(), corners_3d.begin(),
                         corners_3d.end());
    image_points.insert(image_points.end(), corners.begin(), corners.end());
    ids.push_back(id);
  }
  return ids;
}

std::vector<int> matchCommonTags(const AprilGridTarget& target,
                                 const TagDetections& left,
                                 const TagDetections& right,
                                 std::vector<cv::Point3f>& object_points,
                                 std::vector<cv::Point2f>& image_points_left,
                                 std::vector<cv::Point2f>& image_points_right) {
  std::vector<int> ids;
  object_points.clear();
  image_points_left.clear();
  image_points_right.clear();
  for (const auto& [id, corners_l] : left) {
    auto it_r = right.find(id);
    if (it_r == right.end()) continue;
    auto corners_3d = tagCorners3D(target, id);
    if (corners_3d.empty()) continue;
    object_points.insert(object_points.end(), corners_3d.begin(),
                         corners_3d.end());
    image_points_left.insert(image_points_left.end(), corners_l.begin(),
                             corners_l.end());
    image_points_right.insert(image_points_right.end(), it_r->second.begin(),
                              it_r->second.end());
    ids.push_back(id);
  }
  return ids;
}

bool meetsMinimum(const AprilGridTarget& target, const std::vector<int>& ids,
                  int min_tags, int min_rows, int min_cols,
                  std::string& reason) {
  std::set<int> rows, cols;
  for (int id : ids) {
    rows.insert(tagRow(target, id));
    cols.insert(tagCol(target, id));
  }

  std::ostringstream ss;
  if (static_cast<int>(ids.size()) < min_tags) {
    ss << ids.size() << " tags detected, at least " << min_tags
       << " required";
  } else if (static_cast<int>(rows.size()) < min_rows) {
    ss << "tags span " << rows.size() << " row(s), at least " << min_rows
       << " required";
  } else if (static_cast<int>(cols.size()) < min_cols) {
    ss << "tags span " << cols.size() << " column(s), at least " << min_cols
       << " required";
  }
  reason = ss.str();
  return reason.empty();
}

std::vector<cv::Point2f> outsideCorners(const AprilGridTarget& target,
                                        const TagDetections& detections) {
  std::vector<cv::Point2f> outside;
  std::vector<cv::Point3f> obj_3d;
  std::vector<cv::Point2f> img;
  tagCorrespondences(target, detections, obj_3d, img);
  if (obj_3d.size() < 4) return outside;

  std::vector<cv::Point2f> obj;
  float min_x = std::numeric_limits<float>::max(), max_x = -min_x;
  float min_y = min_x, max_y = -min_x;
  for (const auto& p : obj_3d) {
    obj.push_back(cv::Point2f(p.x, p.y));
    min_x = std::min(min_x, p.x);
    max_x = std::max(max_x, p.x);
    min_y = std::min(min_y, p.y);
    max_y = std::max(max_y, p.y);
  }

  cv::Mat H = cv::findHomography(obj, img);
  if (H.empty()) return outside;

  std::vector<cv::Point2f> box = {{min_x, max_y},   // top-left (Y up)
                                  {max_x, max_y},   // top-right
                                  {max_x, min_y},   // bottom-right
                                  {min_x, min_y}};  // bottom-left
  cv::perspectiveTransform(box, outside, H);
  return outside;
}

void printAprilGridTarget(const AprilGridTarget& target) {
  cv::Size2f board = target.boardSizeMm();
  std::cout << " * Expected AprilGrid features:" << std::endl;
  std::cout << "   - Tags:\t\t\t" << target.cols << " x " << target.rows << " ("
            << target.tagCount() << " tags, tag36h11)" << std::endl;
  std::cout << "   - Tag size:\t\t" << target.tag_size << " mm" << std::endl;
  std::cout << "   - Tag spacing:\t\t" << target.spacingMm() << " mm (ratio "
            << target.tag_spacing << ")" << std::endl;
  std::cout << "   - Board size:\t\t" << board.width << " x " << board.height
            << " mm (outer edges of the tags)" << std::endl;
  std::cout << "Measure the printed target: if it differs, put the measured "
               "values in the target file."
            << std::endl;
}

void drawAprilGridDetections(cv::Mat& display, const TagDetections& detections,
                             cv::Size detection_size) {
  if (detections.empty()) return;
  const float sx = display.cols / static_cast<float>(detection_size.width);
  const float sy = display.rows / static_cast<float>(detection_size.height);

  std::vector<std::vector<cv::Point2f>> corners;
  std::vector<int> ids;
  for (const auto& [id, tag_corners] : detections) {
    std::vector<cv::Point2f> scaled;
    for (const auto& c : tag_corners) scaled.push_back(cv::Point2f(c.x * sx, c.y * sy));
    corners.push_back(scaled);
    ids.push_back(id);
  }
  cv::aruco::drawDetectedMarkers(display, corners, ids, cv::Scalar(0, 255, 0));
}
