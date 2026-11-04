#include <cmath>
#include <vector>

#include "rms.hpp"
#include "test_utils.hpp"

TEST(rms_equal_weights_is_the_quadratic_mean) {
  // Two views with the same number of corners
  CHECK_NEAR(aggregateRms({0.3, 0.4}, {48, 48}), std::sqrt((0.09 + 0.16) / 2.0), 1e-12);
}

TEST(rms_is_weighted_by_the_corners_of_each_view) {
  // sqrt((100 * 0.2^2 + 300 * 0.4^2) / 400)
  CHECK_NEAR(aggregateRms({0.2, 0.4}, {100, 300}), std::sqrt((4.0 + 48.0) / 400.0), 1e-12);
}

TEST(rms_of_no_view_is_nan) {
  CHECK(std::isnan(aggregateRms({}, {})));
  CHECK(std::isnan(aggregateRms({0.3}, {0})));
  CHECK(std::isnan(aggregateRms({0.3, 0.4}, {48})));  // sizes differ
}

TEST(rms_per_corner_to_mse_per_coordinate) {
  // OpenCV RMS = sqrt(sum |e|^2 / corners); per coordinate: RMS^2 / 2
  CHECK_NEAR(msePerCoordinate(0.5), 0.125, 1e-15);
  CHECK_NEAR(msePerCoordinate(0.0), 0.0, 1e-15);
}

TEST(rms_of_residuals_matches_the_opencv_convention) {
  // Two corners with errors (3, 4) and (0, 0): sqrt((25 + 0) / 2)
  CHECK_NEAR(rmsPerCorner({{3.0, 4.0}, {0.0, 0.0}}), std::sqrt(12.5), 1e-12);
}
