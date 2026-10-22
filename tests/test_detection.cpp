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

// 2.5 px/mm keeps tag (220 px), gap (66 px) and pitch (286 px) on whole pixels
constexpr float kPxPerMm = 2.5f;
constexpr int kMarginPx = 100;

// Board point (mm, Y up) -> pixel position used to draw it in the
// fronto-parallel synthetic image
cv::Point2f boardToImage(const AprilGridTarget& target, const cv::Point3f& p) {
  float h = target.boardSizeMm().height;
  return cv::Point2f(kMarginPx + p.x * kPxPerMm,
                     kMarginPx + (h - p.y) * kPxPerMm);
}

// Where a detector sees that point: a pixel block drawn from column 'u' has
// its edge at 'u - 0.5' in OpenCV's pixel-center convention
cv::Point2f expectedCorner(const AprilGridTarget& target, const cv::Point3f& p) {
  return boardToImage(target, p) - cv::Point2f(0.5f, 0.5f);
}

// Synthetic Kalibr-like AprilGrid: tags with a 2-bit border laid out with
// tagCorners3D(), plus the small black squares Kalibr prints between the tags.
// The tags are rotated by 180 deg so that OpenCV returns their corners as
// [BR, BL, TL, TR] in board coordinates (decisions.md D5).
cv::Mat renderBoard(const AprilGridTarget& target) {
  cv::Size2f board = target.boardSizeMm();
  cv::Mat img(static_cast<int>(board.height * kPxPerMm) + 2 * kMarginPx,
              static_cast<int>(board.width * kPxPerMm) + 2 * kMarginPx,
              CV_8UC1, cv::Scalar(255));
  auto dict = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_APRILTAG_36h11);
  const int tag_px = static_cast<int>(target.tag_size * kPxPerMm);
  const float gap = target.spacingMm();

  for (int id = 0; id < target.tagCount(); id++) {
    auto corners = tagCorners3D(target, id);
    cv::Point2f top_left = boardToImage(target, corners[2]);
    cv::Mat marker;
    cv::aruco::drawMarker(dict, id, tag_px, marker, 2);
    cv::rotate(marker, marker, cv::ROTATE_180);
    marker.copyTo(img(cv::Rect(cvRound(top_left.x), cvRound(top_left.y),
                               tag_px, tag_px)));
  }

  // Kalibr corner squares: one in every gap intersection, outer ring included
  for (int r = 0; r <= target.rows; r++) {
    for (int c = 0; c <= target.cols; c++) {
      cv::Point3f p0(c * target.pitchMm() - gap, r * target.pitchMm() - gap, 0);
      cv::Point2f a = boardToImage(target, cv::Point3f(p0.x, p0.y + gap, 0));
      cv::Point2f b = boardToImage(target, cv::Point3f(p0.x + gap, p0.y, 0));
      cv::rectangle(img, cv::Rect(a, b), cv::Scalar(0), cv::FILLED);
    }
  }
  return img;
}

}  // namespace

TEST(detection_finds_all_tags_at_expected_corners) {
  auto target = kalibrTarget();
  cv::Mat img = renderBoard(target);
  cv::GaussianBlur(img, img, cv::Size(3, 3), 0.8);

  auto detections = detectAprilGrid(img, target, defaultDetectorParameters());
  CHECK(static_cast<int>(detections.size()) == target.tagCount());

  double max_err = 0.0;
  for (const auto& [id, corners] : detections) {
    auto expected = tagCorners3D(target, id);
    CHECK(corners.size() == 4);
    for (size_t k = 0; k < 4 && k < corners.size(); k++) {
      cv::Point2f e = expectedCorner(target, expected[k]);
      max_err = std::max(max_err, cv::norm(corners[k] - e));
    }
  }
  CHECK(max_err < 0.25);
  std::cout << "    max corner error: " << max_err << " px" << std::endl;
}

TEST(detection_works_under_perspective) {
  auto target = kalibrTarget();
  cv::Mat img = renderBoard(target);
  std::vector<cv::Point2f> src = {{0, 0},
                                  {float(img.cols), 0},
                                  {float(img.cols), float(img.rows)},
                                  {0, float(img.rows)}};
  std::vector<cv::Point2f> dst = {{80, 40},
                                  {float(img.cols) - 160, 120},
                                  {float(img.cols) - 60, float(img.rows) - 30},
                                  {20, float(img.rows) - 140}};
  cv::Mat H = cv::getPerspectiveTransform(src, dst);
  cv::Mat warped;
  cv::warpPerspective(img, warped, H, img.size(), cv::INTER_LINEAR,
                      cv::BORDER_CONSTANT, cv::Scalar(255));

  auto detections = detectAprilGrid(warped, target, defaultDetectorParameters());
  CHECK(static_cast<int>(detections.size()) == target.tagCount());

  double max_err = 0.0;
  for (const auto& [id, corners] : detections) {
    auto expected = tagCorners3D(target, id);
    std::vector<cv::Point2f> e_img, e_warp;
    for (const auto& p : expected) e_img.push_back(expectedCorner(target, p));
    cv::perspectiveTransform(e_img, e_warp, H);
    for (size_t k = 0; k < 4 && k < corners.size(); k++)
      max_err = std::max(max_err, cv::norm(corners[k] - e_warp[k]));
  }
  CHECK(max_err < 0.5);
  std::cout << "    max corner error: " << max_err << " px" << std::endl;
}

TEST(detection_keeps_only_complete_tags_on_partial_view) {
  auto target = kalibrTarget();
  cv::Mat img = renderBoard(target);
  // Cut through the middle of the 4th tag column
  int cut = cvRound(kMarginPx + (3 * target.pitchMm() + target.tag_size / 2) *
                                    kPxPerMm);
  cv::Mat partial = img(cv::Rect(0, 0, cut, img.rows)).clone();

  auto detections =
      detectAprilGrid(partial, target, defaultDetectorParameters());
  CHECK(detections.size() == 18);  // columns 0..2 of the 6 rows
  for (const auto& [id, corners] : detections) CHECK(tagCol(target, id) < 3);
}

TEST(detection_ignores_tags_outside_the_grid) {
  auto target = kalibrTarget();
  cv::Mat img = renderBoard(target);
  cv::Mat canvas(img.rows, img.cols + 300, CV_8UC1, cv::Scalar(255));
  img.copyTo(canvas(cv::Rect(0, 0, img.cols, img.rows)));
  auto dict = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_APRILTAG_36h11);
  cv::Mat foreign;
  cv::aruco::drawMarker(dict, 40, 220, foreign, 2);
  foreign.copyTo(canvas(cv::Rect(img.cols + 40, 200, 220, 220)));

  auto detections = detectAprilGrid(canvas, target, defaultDetectorParameters());
  CHECK(static_cast<int>(detections.size()) == target.tagCount());
  CHECK(detections.count(40) == 0);
}

TEST(detection_rejects_tiny_tags) {
  auto target = kalibrTarget();
  cv::Mat img = renderBoard(target);
  cv::Mat tiny;
  // Tags become ~9 px wide: below the 10 px minimum
  cv::resize(img, tiny, cv::Size(), 0.04, 0.04, cv::INTER_AREA);
  auto detections = detectAprilGrid(tiny, target, defaultDetectorParameters());
  CHECK(detections.empty());
}

// Real Kalibr boards seen from afar: the corner squares touch the tag corners
// and the blurred seam between them turns dark, so a contour-based detector
// merges each tag with its squares (decisions.md D11). Reproduced by drawing
// the board, shrinking it to ~35 px tags and blurring it.
TEST(detection_separates_tags_from_kalibr_corner_squares) {
  auto target = kalibrTarget();
  cv::Mat img = renderBoard(target);
  const double f = 0.16;  // 220 px tags -> ~35 px
  cv::Mat small;
  cv::resize(img, small, cv::Size(), f, f, cv::INTER_AREA);
  cv::GaussianBlur(small, small, cv::Size(5, 5), 1.0);

  auto detections = detectAprilGrid(small, target, defaultDetectorParameters());
  // The OpenCV default threshold finds 2 tags here. This synthetic case is
  // harsher than real images, where the same settings find >= 12 tags in 34 of
  // 38 views: the bound guards against the regression, it is not the real rate.
  CHECK(detections.size() >= 24);

  double max_err = 0.0;
  for (const auto& [id, corners] : detections) {
    auto expected = tagCorners3D(target, id);
    for (size_t k = 0; k < 4 && k < corners.size(); k++) {
      cv::Point2f e = expectedCorner(target, expected[k]);
      cv::Point2f e_small((e.x + 0.5f) * f - 0.5f, (e.y + 0.5f) * f - 0.5f);
      max_err = std::max(max_err, cv::norm(corners[k] - e_small));
    }
  }
  CHECK(max_err < 0.3);
  std::cout << "    " << detections.size() << " tags, max corner error: "
            << max_err << " px" << std::endl;
}
