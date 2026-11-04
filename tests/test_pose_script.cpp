#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>

#include <opencv2/calib3d.hpp>

#include "pose_script.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace {

const cv::Size kImage(1280, 720);
const cv::Size2d kBoard(660.0, 660.0);

cv::Mat K() { return (cv::Mat_<double>(3, 3) << 534, 0, 640, 0, 534, 360, 0, 0, 1); }
cv::Mat D0() { return cv::Mat::zeros(1, 8, CV_64F); }

cv::Matx33d rotX(double deg) {
  const double a = deg * CV_PI / 180;
  return cv::Matx33d(1, 0, 0, 0, std::cos(a), -std::sin(a), 0, std::sin(a), std::cos(a));
}
cv::Matx33d rotY(double deg) {
  const double a = deg * CV_PI / 180;
  return cv::Matx33d(std::cos(a), 0, std::sin(a), 0, 1, 0, -std::sin(a), 0, std::cos(a));
}
cv::Matx33d rotZ(double deg) {
  const double a = deg * CV_PI / 180;
  return cv::Matx33d(std::cos(a), -std::sin(a), 0, std::sin(a), std::cos(a), 0, 0, 0, 1);
}

// Target facing the camera (board X right, Y up), rotated by 'cam' in the camera frame and by
// 'own' about its own normal, with its centre at 'centre' (mm, camera frame)
ViewGeometry view(const cv::Matx33d& cam, const cv::Matx33d& own, cv::Vec3d centre) {
  const cv::Matx33d facing(1, 0, 0, 0, -1, 0, 0, 0, -1);
  const cv::Matx33d R = cam * facing * own;
  const cv::Vec3d t = centre - R * cv::Vec3d(330, 330, 0);
  cv::Vec3d rvec;
  cv::Rodrigues(R, rvec);
  return viewGeometry(K(), D0(), kImage, kBoard, rvec, t);
}

std::string writeTemp(const std::string& content) {
  static int n = 0;
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path p = fs::temp_directory_path() / ("pose_script_" + std::to_string(stamp) + "_" +
                                            std::to_string(n++) + ".yaml");
  std::ofstream(p) << content;
  return p.string();
}

const char* kPose = "  - {id: B1-P-TL, block: 1, fraction: [0.70, 0.80], center: [0.17, 0.17], "
                    "tilt_h_deg: 0, tilt_v_deg: 0, rotation_deg: 0}\n";

}  // namespace

TEST(pose_script_v1_has_the_63_poses_of_the_brief) {
  PoseScript s;
  std::string error;
  CHECK(loadPoseScript(std::string(ZEDCAL_SOURCE_DIR) + "/config/roteiro_v1.yaml", s, error));
  if (!error.empty()) std::cerr << "  " << error << std::endl;
  CHECK(s.poses.size() == 63);
  std::map<int, int> per_block;
  for (const auto& p : s.poses) per_block[p.block]++;
  CHECK(per_block[1] == 27 && per_block[2] == 20 && per_block[3] == 12 && per_block[4] == 4);
  for (const auto& p : s.poses) {
    if (p.block == 2) CHECK(p.fraction_min == 0.40 && std::fabs(p.tilt_h_deg) + std::fabs(p.tilt_v_deg) == 35);
    if (p.block == 3 || p.block == 4) CHECK(p.fraction_min == 0.70);
  }
}

TEST(pose_script_metric_has_9_poses_per_distance) {
  PoseScript s;
  std::string error;
  CHECK(loadPoseScript(std::string(ZEDCAL_SOURCE_DIR) + "/config/roteiro_metrico_v1.yaml", s, error));
  CHECK(s.poses.size() == 36);
  // MET-<cm>-<position>-<n>: per distance 5 at the centre (MC-1..5) and one at
  // each edge (ML, MR, TC, BC), frontal
  const std::regex id("MET-(50|100|200|300)-(MC|ML|MR|TC|BC)-([1-5])");
  std::map<std::string, int> per_distance, centre;
  for (const auto& p : s.poses) {
    std::smatch m;
    CHECK(std::regex_match(p.id, m, id));
    if (m.empty()) continue;
    per_distance[m[1]]++;
    if (m[2] == "MC") centre[m[1]]++;
    else CHECK(m[3] == "1");
    CHECK(p.tilt_h_deg == 0.0 && p.tilt_v_deg == 0.0);
  }
  CHECK(per_distance.size() == 4);
  for (const auto& [d, n] : per_distance) {
    CHECK(n == 9);
    CHECK(centre[d] == 5);
  }
  CHECK_NEAR(s.tolerances.angle_deg, 2.0, 1e-12);
}

TEST(pose_script_rejects_bad_files) {
  PoseScript s;
  std::string error;
  const std::string head = "name: t\nposes:\n";
  CHECK(!loadPoseScript(writeTemp(head + kPose + kPose), s, error));        // repeated id
  CHECK(error.find("repeated") != std::string::npos);
  std::string bad_id = kPose;
  bad_id.replace(bad_id.find("B1-P-TL"), 7, "B1_P_TL");
  CHECK(!loadPoseScript(writeTemp(head + bad_id), s, error));
  std::string wrong_block = kPose;
  wrong_block.replace(wrong_block.find("block: 1"), 8, "block: 2");
  CHECK(!loadPoseScript(writeTemp(head + wrong_block), s, error));
  CHECK(error.find("block") != std::string::npos);
  CHECK(!loadPoseScript(writeTemp("name: t\ntolerances: {angle_deg: -1}\nposes:\n" + std::string(kPose)), s, error));
  std::string bad_rotation = kPose;
  bad_rotation.replace(bad_rotation.find("rotation_deg: 0"), 15, "rotation_deg: 360");
  CHECK(!loadPoseScript(writeTemp(head + bad_rotation), s, error));
  CHECK(loadPoseScript(writeTemp(head + kPose), s, error));
}

TEST(view_of_a_fronto_parallel_centred_target) {
  const ViewGeometry v = view(cv::Matx33d::eye(), cv::Matx33d::eye(), {0, 0, 1000});
  CHECK_NEAR(v.fraction, 660.0 * 534.0 / 1000.0 / 1280.0, 1e-9);
  CHECK_NEAR(v.center.x, 0.5, 1e-9);
  CHECK_NEAR(v.center.y, 0.5, 1e-9);
  CHECK_NEAR(v.tilt_h_deg, 0.0, 1e-9);
  CHECK_NEAR(v.tilt_v_deg, 0.0, 1e-9);
  CHECK_NEAR(v.rotation_deg, 0.0, 1e-9);
  CHECK_NEAR(v.distance_m, 1.0, 1e-9);
}

TEST(view_tilts_follow_the_convention) {
  // Rotating about the camera x axis by +20 brings the top edge closer: tilt_h = -20
  ViewGeometry v = view(rotX(20), cv::Matx33d::eye(), {0, 0, 1000});
  CHECK_NEAR(v.tilt_h_deg, -20.0, 0.5);
  CHECK_NEAR(v.tilt_v_deg, 0.0, 0.5);
  // Rotating about the camera y axis by -30 takes the right edge away: tilt_v = +30
  v = view(rotY(-30), cv::Matx33d::eye(), {0, 0, 1000});
  CHECK_NEAR(v.tilt_v_deg, 30.0, 0.5);
  CHECK_NEAR(v.tilt_h_deg, 0.0, 0.5);
}

TEST(view_rotation_has_no_quadrant_confusion) {
  // Turning the target about its own normal by g puts its X axis at -g in the image
  const double expected[] = {0, 270, 180, 90};
  for (int k = 0; k < 4; k++) {
    const ViewGeometry v = view(cv::Matx33d::eye(), rotZ(90.0 * k), {0, 0, 1000});
    CHECK(angleDifferenceDeg(v.rotation_deg, expected[k]) < 0.5);
  }
}

TEST(view_of_a_target_larger_than_the_image) {
  const ViewGeometry v = view(cv::Matx33d::eye(), cv::Matx33d::eye(), {0, 0, 200});
  CHECK(v.fraction > 1.0);
  CHECK_NEAR(v.fraction, 660.0 * 534.0 / 200.0 / 1280.0, 1e-9);
}

TEST(check_pose_per_quantity) {
  ScriptPose p;
  p.fraction_min = 0.25;
  p.fraction_max = 0.30;
  p.center = {0.5, 0.5};
  Tolerances t;
  ViewGeometry v = view(cv::Matx33d::eye(), cv::Matx33d::eye(), {0, 0, 1000});
  CHECK(checkPose(v, p, t).all());
  v = view(rotX(20), cv::Matx33d::eye(), {0, 0, 1000});
  PoseCheck c = checkPose(v, p, t);
  CHECK(!c.tilt_h && c.tilt_v && c.rotation && !c.all());
  p.rotation_deg = 358;
  v = view(cv::Matx33d::eye(), cv::Matx33d::eye(), {0, 0, 1000});
  CHECK(checkPose(v, p, t).rotation);  // 0 and 358 are 2 deg apart
}

TEST(view_geometry_matches_the_shared_fixture) {
  // The same fixture is checked by analysis/tests/test_coverage.py
  cv::FileStorage fs(std::string(ZEDCAL_SOURCE_DIR) + "/tests/fixtures/pose_geometry.json",
                     cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
  CHECK(fs.isOpened());
  int n = 0;
  for (const auto& c : fs["cases"]) {
    cv::Vec3d rvec(c["rvec"][0].real(), c["rvec"][1].real(), c["rvec"][2].real());
    cv::Vec3d tvec(c["tvec"][0].real(), c["tvec"][1].real(), c["tvec"][2].real());
    const ViewGeometry v = viewGeometry(K(), D0(), kImage, kBoard, rvec, tvec);
    CHECK_NEAR(v.tilt_h_deg, c["tilt_h_deg"].real(), 1e-6);
    CHECK_NEAR(v.tilt_v_deg, c["tilt_v_deg"].real(), 1e-6);
    CHECK(angleDifferenceDeg(v.rotation_deg, c["rotation_deg"].real()) < 1e-6);
    CHECK_NEAR(v.distance_m, c["distance_m"].real(), 1e-9);
    n++;
  }
  CHECK(n == 6);
}

TEST(target_pose_is_recovered_with_strong_distortion_at_the_border) {
  // The factory distortion of the bench ZED 2i (rational, 2.1 mm lens): the
  // case where a coarse undistortion gave a wrong pose in Python
  const cv::Mat Kf = (cv::Mat_<double>(3, 3) << 533.82, 0, 632.115, 0, 533.845, 369.065, 0, 0, 1);
  const cv::Mat Df = (cv::Mat_<double>(1, 8) << -0.432559, 2.10359, 8.81e-05, -7.08e-05, 0.166931,
                      -0.320247, 1.96174, 0.33095);
  std::vector<cv::Point3f> grid;
  for (int r = 0; r < 12; ++r)
    for (int c = 0; c < 12; ++c) grid.emplace_back(c * 57.2f, r * 57.2f, 0.0f);
  int cases = 0;
  for (double tilt : {-35.0, 0.0, 35.0})
    for (double x : {-700.0, 0.0, 700.0})
      for (double y : {-350.0, 0.0, 350.0}) {
        const double b = tilt * CV_PI / 180.0;
        const cv::Matx33d Ry(std::cos(b), 0, std::sin(b), 0, 1, 0, -std::sin(b), 0, std::cos(b));
        const cv::Matx33d R = Ry * cv::Matx33d(1, 0, 0, 0, -1, 0, 0, 0, -1);
        const cv::Vec3d centre(314.6, 314.6, 0.0);
        const cv::Vec3d t = cv::Vec3d(x, y, 900.0) - R * centre;
        cv::Vec3d rv;
        cv::Rodrigues(R, rv);
        std::vector<cv::Point2f> all;
        cv::projectPoints(grid, rv, t, Kf, Df, all);
        std::vector<cv::Point3f> obj;
        std::vector<cv::Point2f> img;
        for (size_t i = 0; i < all.size(); ++i)
          if (all[i].x >= 0 && all[i].x < 1280 && all[i].y >= 0 && all[i].y < 720) {
            obj.push_back(grid[i]);
            img.push_back(all[i]);
          }
        if (obj.size() < 16) continue;
        cv::Vec3d r_est, t_est;
        double rms = -1.0;
        CHECK(estimateTargetPose(obj, img, Kf, Df, r_est, t_est, &rms));
        CHECK(rms < 1e-3);
        CHECK(cv::norm(t_est - t) < 0.05);  // mm
        cv::Matx33d R_est;
        cv::Rodrigues(r_est, R_est);
        cv::Vec3d dr;
        cv::Rodrigues(cv::Mat(R_est * R.t()), dr);
        CHECK(cv::norm(dr) < 1e-5);
        ++cases;
      }
  CHECK(cases >= 20);
  cv::Vec3d r, t;
  CHECK(!estimateTargetPose({}, {}, Kf, Df, r, t));
}
