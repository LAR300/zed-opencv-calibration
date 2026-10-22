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

void checkCorner(const cv::Point3f& corner, float x, float y) {
  CHECK_NEAR(corner.x, x, 1e-3);
  CHECK_NEAR(corner.y, y, 1e-3);
  CHECK_NEAR(corner.z, 0.0, 1e-6);
}

}  // namespace

TEST(geometry_board_dimensions) {
  auto target = kalibrTarget();
  CHECK(target.tagCount() == 36);
  CHECK_NEAR(target.spacingMm(), 26.4, 1e-3);
  CHECK_NEAR(target.pitchMm(), 114.4, 1e-3);
  CHECK_NEAR(target.boardSizeMm().width, 660.0, 1e-3);
  CHECK_NEAR(target.boardSizeMm().height, 660.0, 1e-3);
}

// Kalibr convention: IDs left->right then bottom->top, Y up; OpenCV returns the
// corners of a Kalibr tag as [BR, BL, TL, TR] in board coordinates
TEST(geometry_tag0_corners) {
  auto corners = tagCorners3D(kalibrTarget(), 0);
  CHECK(corners.size() == 4);
  if (corners.size() != 4) return;
  checkCorner(corners[0], 88.0f, 0.0f);
  checkCorner(corners[1], 0.0f, 0.0f);
  checkCorner(corners[2], 0.0f, 88.0f);
  checkCorner(corners[3], 88.0f, 88.0f);
}

TEST(geometry_tag5_is_last_of_first_row) {
  auto corners = tagCorners3D(kalibrTarget(), 5);
  CHECK(corners.size() == 4);
  if (corners.size() != 4) return;
  checkCorner(corners[1], 572.0f, 0.0f);
  checkCorner(corners[3], 660.0f, 88.0f);
}

TEST(geometry_tag30_is_first_of_last_row) {
  auto corners = tagCorners3D(kalibrTarget(), 30);
  CHECK(corners.size() == 4);
  if (corners.size() != 4) return;
  checkCorner(corners[1], 0.0f, 572.0f);
  checkCorner(corners[3], 88.0f, 660.0f);
}

TEST(geometry_tag35_reaches_board_corner) {
  auto corners = tagCorners3D(kalibrTarget(), 35);
  CHECK(corners.size() == 4);
  if (corners.size() != 4) return;
  checkCorner(corners[3], 660.0f, 660.0f);
}

TEST(geometry_rows_and_cols) {
  auto target = kalibrTarget();
  CHECK(tagRow(target, 0) == 0 && tagCol(target, 0) == 0);
  CHECK(tagRow(target, 5) == 0 && tagCol(target, 5) == 5);
  CHECK(tagRow(target, 6) == 1 && tagCol(target, 6) == 0);
  CHECK(tagRow(target, 35) == 5 && tagCol(target, 35) == 5);
}

TEST(geometry_rejects_ids_outside_grid) {
  auto target = kalibrTarget();
  CHECK(isValidTagId(target, 0));
  CHECK(isValidTagId(target, 35));
  CHECK(!isValidTagId(target, 36));
  CHECK(!isValidTagId(target, -1));
  CHECK(tagCorners3D(target, 36).empty());
}
