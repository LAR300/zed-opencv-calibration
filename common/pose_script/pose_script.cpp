#include "pose_script.hpp"

#include <cmath>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

#include <opencv2/calib3d.hpp>
#include <opencv2/core/persistence.hpp>

namespace {

double rad2deg(double r) { return r * 180.0 / CV_PI; }

bool readNumber(const cv::FileNode& n, double& v) {
  if (!n.isInt() && !n.isReal()) return false;
  v = n.real();
  return true;
}

bool readPair(const cv::FileNode& n, double& a, double& b) {
  if (!n.isSeq() || n.size() != 2) return false;
  return readNumber(n[0], a) && readNumber(n[1], b);
}

}  // namespace

double angleDifferenceDeg(double a, double b) {
  double d = std::fmod(std::fabs(a - b), 360.0);
  return d > 180.0 ? 360.0 - d : d;
}

bool loadPoseScript(const std::string& path, PoseScript& script, std::string& error) {
  script = PoseScript();
  error.clear();
  std::ifstream file(path);
  if (!file.is_open()) {
    error = "Cannot open the pose script '" + path + "'";
    return false;
  }
  std::stringstream content;
  content << file.rdbuf();
  std::string yaml = content.str();
  if (yaml.rfind("%YAML", 0) != 0) yaml = "%YAML:1.0\n---\n" + yaml;
  cv::FileStorage fs;
  try {
    fs.open(yaml, cv::FileStorage::READ | cv::FileStorage::MEMORY);
  } catch (const cv::Exception& e) {
    error = "Cannot parse the pose script '" + path + "': " + e.what();
    return false;
  }
  if (!fs["name"].isString()) {
    error = "The pose script needs a 'name'";
    return false;
  }
  script.name = fs["name"].string();
  cv::FileNode tol = fs["tolerances"];
  if (!tol.empty()) {
    if (!tol["fraction"].empty()) readNumber(tol["fraction"], script.tolerances.fraction);
    if (!tol["position"].empty()) readNumber(tol["position"], script.tolerances.position);
    if (!tol["angle_deg"].empty()) readNumber(tol["angle_deg"], script.tolerances.angle_deg);
  }
  if (script.tolerances.fraction <= 0 || script.tolerances.position <= 0 ||
      script.tolerances.angle_deg <= 0) {
    error = "Tolerances must be positive";
    return false;
  }
  cv::FileNode poses = fs["poses"];
  if (!poses.isSeq() || poses.size() == 0) {
    error = "The pose script needs a non-empty 'poses' list";
    return false;
  }
  // B<block>-<distance>-<position>[-<variation>] (roteiro v1), or another prefix such as the
  // metric test's MET-<cm>-<position>-<n>; '_' is reserved for the image names
  static const std::regex id_format("[A-Za-z0-9]+(-[A-Za-z0-9]+)+");
  static const std::regex block_prefix("B([0-9]+)-.*");
  std::set<std::string> ids;
  for (const auto& p : poses) {
    ScriptPose pose;
    if (!p["id"].isString()) {
      error = "Every pose needs an 'id'";
      return false;
    }
    pose.id = p["id"].string();
    const std::string where = "pose '" + pose.id + "': ";
    if (!std::regex_match(pose.id, id_format)) {
      error = where + "the id must be letters and digits in '-' separated parts, "
                      "e.g. B1-P-TL or MET-300-MC-1";
      return false;
    }
    if (!ids.insert(pose.id).second) {
      error = where + "repeated id";
      return false;
    }
    double block = 0;
    if (!readNumber(p["block"], block) || block < 1 || block != std::floor(block)) {
      error = where + "'block' must be a positive integer";
      return false;
    }
    pose.block = static_cast<int>(block);
    std::smatch m;
    if (std::regex_match(pose.id, m, block_prefix) && std::stoi(m[1]) != pose.block) {
      error = where + "the id says block " + m[1].str() + " but 'block' is " +
              std::to_string(pose.block);
      return false;
    }
    if (!readPair(p["fraction"], pose.fraction_min, pose.fraction_max) || pose.fraction_min <= 0 ||
        pose.fraction_max > 2.0 || pose.fraction_min > pose.fraction_max) {
      error = where + "'fraction' must be [min, max] within (0, 2]";
      return false;
    }
    if (!readPair(p["center"], pose.center.x, pose.center.y) || pose.center.x < -0.5 ||
        pose.center.x > 1.5 || pose.center.y < -0.5 || pose.center.y > 1.5) {
      error = where + "'center' must be [x, y] in normalised image coordinates";
      return false;
    }
    if (!readNumber(p["tilt_h_deg"], pose.tilt_h_deg) || !readNumber(p["tilt_v_deg"], pose.tilt_v_deg) ||
        std::fabs(pose.tilt_h_deg) >= 90 || std::fabs(pose.tilt_v_deg) >= 90) {
      error = where + "'tilt_h_deg' and 'tilt_v_deg' must be numbers within (-90, 90)";
      return false;
    }
    if (!readNumber(p["rotation_deg"], pose.rotation_deg) || pose.rotation_deg < 0 ||
        pose.rotation_deg >= 360) {
      error = where + "'rotation_deg' must be within [0, 360)";
      return false;
    }
    script.poses.push_back(pose);
  }
  return true;
}

ViewGeometry viewGeometry(const cv::Mat& K, const cv::Mat& D, cv::Size image_size,
                          cv::Size2d board, const cv::Vec3d& rvec, const cv::Vec3d& tvec) {
  ViewGeometry g;
  const double W = board.width, H = board.height;
  std::vector<cv::Point3d> pts = {{0, 0, 0}, {W, 0, 0}, {W, H, 0}, {0, H, 0},
                                  {W / 2, H / 2, 0}, {W / 2 + 1.0, H / 2, 0}};
  std::vector<cv::Point2d> img;
  cv::projectPoints(pts, rvec, tvec, K, D, img);
  double min_x = img[0].x, max_x = img[0].x;
  for (int i = 1; i < 4; i++) {
    min_x = std::min(min_x, img[i].x);
    max_x = std::max(max_x, img[i].x);
  }
  g.fraction = (max_x - min_x) / image_size.width;
  g.center = cv::Point2d(img[4].x / image_size.width, img[4].y / image_size.height);
  const cv::Point2d axis = img[5] - img[4];
  double rot = rad2deg(std::atan2(axis.y, axis.x));
  if (rot < 0) rot += 360.0;
  g.rotation_deg = rot >= 360.0 ? rot - 360.0 : rot;

  cv::Matx33d R;
  cv::Rodrigues(rvec, R);
  // Target normal, oriented towards the camera
  cv::Vec3d n(R(0, 2), R(1, 2), R(2, 2));
  if (n[2] > 0) n = -n;
  g.tilt_h_deg = rad2deg(std::atan2(-n[1], -n[2]));
  g.tilt_v_deg = rad2deg(std::atan2(n[0], -n[2]));
  const cv::Vec3d centre = R * cv::Vec3d(W / 2, H / 2, 0) + tvec;
  g.distance_m = cv::norm(centre) / 1000.0;
  return g;
}

PoseCheck checkPose(const ViewGeometry& v, const ScriptPose& p, const Tolerances& t) {
  PoseCheck c;
  c.fraction = v.fraction >= p.fraction_min - t.fraction && v.fraction <= p.fraction_max + t.fraction;
  c.position = std::fabs(v.center.x - p.center.x) <= t.position &&
               std::fabs(v.center.y - p.center.y) <= t.position;
  c.tilt_h = std::fabs(v.tilt_h_deg - p.tilt_h_deg) <= t.angle_deg;
  c.tilt_v = std::fabs(v.tilt_v_deg - p.tilt_v_deg) <= t.angle_deg;
  c.rotation = angleDifferenceDeg(v.rotation_deg, p.rotation_deg) <= t.angle_deg;
  return c;
}
