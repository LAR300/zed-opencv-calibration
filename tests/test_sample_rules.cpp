#include "aprilgrid.hpp"
#include "test_utils.hpp"

namespace {

AprilGridTarget kalibrTarget() {
  AprilGridTarget target;
  target.rows = 6;
  target.cols = 6;
  target.tag_size = 88.0f;
  target.tag_spacing = 0.3f;
  return target;
}

// Fronto-parallel projection of the board: 1 mm = 1 px, Y flipped
cv::Point2f project(const cv::Point3f& p) {
  return cv::Point2f(100.0f + p.x, 800.0f - p.y);
}

TagDetections syntheticDetections(const AprilGridTarget& target,
                                  const std::vector<int>& ids) {
  TagDetections detections;
  for (int id : ids) {
    std::vector<cv::Point2f> corners;
    for (const auto& p : tagCorners3D(target, id)) corners.push_back(project(p));
    detections[id] = corners;
  }
  return detections;
}

std::vector<int> range(int first, int last) {
  std::vector<int> ids;
  for (int id = first; id <= last; id++) ids.push_back(id);
  return ids;
}

}  // namespace

TEST(match_keeps_only_common_tags_sorted_by_id) {
  auto target = kalibrTarget();
  auto left = syntheticDetections(target, range(0, 20));
  auto right = syntheticDetections(target, range(5, 25));

  std::vector<cv::Point3f> obj;
  std::vector<cv::Point2f> img_l, img_r;
  auto ids = matchCommonTags(target, left, right, obj, img_l, img_r);

  CHECK(ids == range(5, 20));
  CHECK(obj.size() == ids.size() * 4);
  CHECK(img_l.size() == obj.size() && img_r.size() == obj.size());
  // First point is corner 0 of tag 5, in both views
  auto c5 = tagCorners3D(target, 5);
  CHECK_NEAR(obj[0].x, c5[0].x, 1e-4);
  CHECK_NEAR(obj[0].y, c5[0].y, 1e-4);
  CHECK_NEAR(img_l[0].x, left.at(5)[0].x, 1e-4);
  CHECK_NEAR(img_r[0].x, right.at(5)[0].x, 1e-4);
}

TEST(correspondences_single_view) {
  auto target = kalibrTarget();
  auto det = syntheticDetections(target, {3, 1, 2});
  std::vector<cv::Point3f> obj;
  std::vector<cv::Point2f> img;
  auto ids = tagCorrespondences(target, det, obj, img);
  CHECK((ids == std::vector<int>{1, 2, 3}));
  CHECK(obj.size() == 12 && img.size() == 12);
}

TEST(minimum_accepts_12_tags_over_2_rows) {
  std::string reason;
  CHECK(meetsMinimum(kalibrTarget(), range(0, 11), DEFAULT_MIN_TAGS,
                     DEFAULT_MIN_TAG_ROWS, DEFAULT_MIN_TAG_COLS, reason));
  CHECK(reason.empty());
}

TEST(minimum_rejects_11_tags) {
  std::string reason;
  CHECK(!meetsMinimum(kalibrTarget(), range(0, 10), DEFAULT_MIN_TAGS,
                      DEFAULT_MIN_TAG_ROWS, DEFAULT_MIN_TAG_COLS, reason));
  CHECK(reason.find("11") != std::string::npos);
  CHECK(reason.find("12") != std::string::npos);
}

TEST(minimum_rejects_single_row_or_column) {
  auto target = kalibrTarget();
  std::string reason;
  // A 6x6 grid cannot hold 12 tags in one row, so use a smaller tag count
  CHECK(!meetsMinimum(target, range(0, 5), 6, DEFAULT_MIN_TAG_ROWS,
                      DEFAULT_MIN_TAG_COLS, reason));
  CHECK(reason.find("row") != std::string::npos);

  std::vector<int> one_column = {0, 6, 12, 18, 24, 30};
  CHECK(!meetsMinimum(target, one_column, 6, DEFAULT_MIN_TAG_ROWS,
                      DEFAULT_MIN_TAG_COLS, reason));
  CHECK(reason.find("column") != std::string::npos);
}

TEST(outside_corners_of_full_board) {
  auto target = kalibrTarget();
  auto det = syntheticDetections(target, range(0, 35));
  auto corners = outsideCorners(target, det);
  CHECK(corners.size() == 4);
  if (corners.size() != 4) return;
  // Cyclic order: board top-left, top-right, bottom-right, bottom-left
  std::vector<cv::Point2f> expected = {project({0, 660, 0}),
                                       project({660, 660, 0}),
                                       project({660, 0, 0}),
                                       project({0, 0, 0})};
  for (int k = 0; k < 4; k++) CHECK(cv::norm(corners[k] - expected[k]) < 1.0);
}

TEST(outside_corners_of_partial_view) {
  auto target = kalibrTarget();
  // Columns 0..2 of rows 0..1
  auto det = syntheticDetections(target, {0, 1, 2, 6, 7, 8});
  auto corners = outsideCorners(target, det);
  CHECK(corners.size() == 4);
  if (corners.size() != 4) return;
  const float x_max = 2 * target.pitchMm() + target.tag_size;  // 316.8
  const float y_max = target.pitchMm() + target.tag_size;      // 202.4
  std::vector<cv::Point2f> expected = {project({0, y_max, 0}),
                                       project({x_max, y_max, 0}),
                                       project({x_max, 0, 0}),
                                       project({0, 0, 0})};
  for (int k = 0; k < 4; k++) CHECK(cv::norm(corners[k] - expected[k]) < 1.0);
}

TEST(outside_corners_need_detections) {
  CHECK(outsideCorners(kalibrTarget(), TagDetections()).empty());
}
