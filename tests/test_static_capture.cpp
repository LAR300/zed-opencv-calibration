#include <random>

#include <opencv2/imgproc.hpp>

#include "static_capture.hpp"
#include "test_utils.hpp"

namespace {

// Three tags of 4 corners at fixed places, with Gaussian noise of 'sigma' px
TagDetections tags(double sigma, std::mt19937& rng, cv::Point2f shift = {0, 0},
                   std::vector<int> ids = {1, 2, 3}) {
  std::normal_distribution<float> n(0.0f, static_cast<float>(sigma));
  TagDetections t;
  for (int id : ids) {
    const float x = 200.0f * id, y = 300.0f;
    t[id] = {{x + 80 + n(rng), y + 80 + n(rng)}, {x + n(rng), y + 80 + n(rng)},
             {x + n(rng), y + n(rng)}, {x + 80 + n(rng), y + n(rng)}};
    for (auto& c : t[id]) c += shift;
  }
  return t;
}

}  // namespace

TEST(stability_still_target_with_noise_becomes_stable_after_the_window) {
  std::mt19937 rng(1);
  StabilityTracker tr(0.5, 1.0);
  StabilityState s;
  for (int k = 0; k <= 15; k++) s = tr.update(tags(0.05, rng), tags(0.05, rng), k / 15.0);
  CHECK(s.stable && !s.indeterminate);
  CHECK(s.max_displacement_px < 0.2);
  CHECK_NEAR(s.stable_for_s, 1.0, 1e-9);
}

TEST(stability_a_1px_jump_restarts_the_window) {
  std::mt19937 rng(2);
  StabilityTracker tr(0.5, 1.0);
  StabilityState s;
  for (int k = 0; k < 10; k++) s = tr.update(tags(0.05, rng), tags(0.05, rng), k / 15.0);
  s = tr.update(tags(0.05, rng, {1.0f, 0}), tags(0.05, rng, {1.0f, 0}), 10 / 15.0);
  CHECK(!s.stable && s.stable_for_s == 0.0);
  CHECK(s.displacement_px > 0.9);
  for (int k = 11; k <= 25; k++) s = tr.update(tags(0.05, rng, {1.0f, 0}), tags(0.05, rng, {1.0f, 0}), k / 15.0);
  CHECK(s.stable);
}

TEST(stability_survives_a_tag_that_comes_and_goes) {
  std::mt19937 rng(3);
  StabilityTracker tr(0.5, 1.0, 8);
  StabilityState s;
  for (int k = 0; k <= 15; k++) {
    std::vector<int> ids = (k % 2) ? std::vector<int>{1, 2} : std::vector<int>{1, 2, 3};
    s = tr.update(tags(0.05, rng, {0, 0}, ids), tags(0.05, rng, {0, 0}, ids), k / 15.0);
  }
  CHECK(s.stable);
}

TEST(stability_too_few_common_corners_is_indeterminate) {
  std::mt19937 rng(4);
  StabilityTracker tr(0.5, 1.0, 16);
  tr.update(tags(0.0, rng, {0, 0}, {1}), tags(0.0, rng, {0, 0}, {1}), 0.0);
  const StabilityState s = tr.update(tags(0.0, rng, {0, 0}, {1}), tags(0.0, rng, {0, 0}, {1}), 0.1);
  CHECK(s.indeterminate && !s.stable);
}

TEST(sharpness_is_measured_on_the_target_not_the_background) {
  // Textured (sharp) background; a checkered patch as the "target"
  cv::Mat img(720, 1280, CV_8U);
  cv::randu(img, 0, 255);
  cv::Mat patch(200, 200, CV_8U);
  for (int r = 0; r < 200; r++)
    for (int c = 0; c < 200; c++) patch.at<uchar>(r, c) = ((r / 20 + c / 20) % 2) ? 255 : 0;
  TagDetections t;
  t[1] = {{700, 500}, {500, 500}, {500, 300}, {700, 300}};
  cv::Mat sharp = img.clone(), blurred = img.clone();
  patch.copyTo(sharp(cv::Rect(500, 300, 200, 200)));
  cv::Mat soft;
  cv::GaussianBlur(patch, soft, cv::Size(0, 0), 4.0);
  soft.copyTo(blurred(cv::Rect(500, 300, 200, 200)));
  // The whole-image measure barely sees the difference; the target measure does
  cv::Mat lap_a, lap_b;
  cv::Laplacian(sharp, lap_a, CV_64F);
  cv::Laplacian(blurred, lap_b, CV_64F);
  cv::Scalar m, sa, sb;
  cv::meanStdDev(lap_a, m, sa);
  cv::meanStdDev(lap_b, m, sb);
  CHECK(sb[0] * sb[0] > 0.9 * sa[0] * sa[0]);
  CHECK(targetSharpness(blurred, t) < 0.2 * targetSharpness(sharp, t));
  CHECK(targetSharpness(sharp, TagDetections()) == 0.0);
}

TEST(sharpness_of_a_sharp_target_on_a_flat_background_is_high) {
  cv::Mat img(720, 1280, CV_8U, cv::Scalar(128));
  for (int r = 300; r < 500; r++)
    for (int c = 500; c < 700; c++) img.at<uchar>(r, c) = (((r - 300) / 20 + (c - 500) / 20) % 2) ? 255 : 0;
  TagDetections t;
  t[1] = {{700, 500}, {500, 500}, {500, 300}, {700, 300}};
  cv::Mat lap;
  cv::Laplacian(img, lap, CV_64F);
  cv::Scalar m, s;
  cv::meanStdDev(lap, m, s);
  CHECK(targetSharpness(img, t) > 5 * s[0] * s[0]);  // the whole image dilutes it
}
