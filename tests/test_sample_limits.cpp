#include "sample_limits.hpp"
#include "test_utils.hpp"

TEST(sample_count_parses_plain_numbers) {
  int value = -1;
  std::string error;
  CHECK(parseSampleCount("25", value, error));
  CHECK(value == 25);
  CHECK(parseSampleCount("0", value, error));
  CHECK(value == 0);
}

TEST(sample_count_rejects_invalid_text) {
  for (const char* text : {"abc", "25x", "", "-3", " 25", "2.5"}) {
    int value = -1;
    std::string error;
    CHECK(!parseSampleCount(text, value, error));
    CHECK(!error.empty());
  }
}

TEST(sample_limits_defaults_are_valid) {
  std::string error;
  CHECK(validateSampleLimits(COLLECTION_MIN_SAMPLES, COLLECTION_MAX_SAMPLES, error));
  CHECK(COLLECTION_MIN_SAMPLES == 25 && COLLECTION_MAX_SAMPLES == 35);
}

TEST(sample_limits_minimum_follows_calibration) {
  std::string error;
  CHECK(!validateSampleLimits(MIN_CALIBRATION_SAMPLES - 1, 0, error));
  CHECK(error.find("20") != std::string::npos);
  CHECK(validateSampleLimits(MIN_CALIBRATION_SAMPLES, 0, error));
}

TEST(sample_limits_zero_max_means_unlimited) {
  std::string error;
  CHECK(validateSampleLimits(25, 0, error));
  CHECK(validateSampleLimits(100, 0, error));
}

TEST(sample_limits_max_below_min_rejected) {
  std::string error;
  CHECK(!validateSampleLimits(25, 24, error));
  CHECK(error.find("24") != std::string::npos && error.find("25") != std::string::npos);
  CHECK(validateSampleLimits(25, 25, error));
}

TEST(collection_completes_when_metrics_complete_after_minimum) {
  CHECK(collectionStep(25, 25, 35, true, false) == CollectionStep::COMPLETE);
  CHECK(collectionStep(35, 25, 35, true, false) == CollectionStep::COMPLETE);
}

TEST(collection_continues_below_minimum_even_with_metrics_complete) {
  CHECK(collectionStep(24, 25, 35, true, false) == CollectionStep::CONTINUE);
}

TEST(collection_pauses_at_maximum_with_incomplete_metrics) {
  CHECK(collectionStep(34, 25, 35, false, false) == CollectionStep::CONTINUE);
  CHECK(collectionStep(35, 25, 35, false, false) == CollectionStep::PAUSE_AT_MAX);
}

TEST(collection_never_pauses_without_maximum) {
  CHECK(collectionStep(35, 25, 0, false, false) == CollectionStep::CONTINUE);
  CHECK(collectionStep(500, 25, 0, false, false) == CollectionStep::CONTINUE);
}

TEST(collection_never_pauses_after_maximum_lifted) {
  CHECK(collectionStep(36, 25, 35, false, true) == CollectionStep::CONTINUE);
  CHECK(collectionStep(40, 25, 35, true, true) == CollectionStep::COMPLETE);
}

TEST(manual_finish_needs_calibration_minimum) {
  CHECK(!canFinishManually(19));
  CHECK(canFinishManually(20));
  CHECK(canFinishManually(100));
}

TEST(incomplete_metrics_lists_only_missing_in_order) {
  CHECK(incompleteMetrics(1.0f, 1.0f, 1.0f, 1.0f).empty());
  CHECK(incompleteMetrics(1.0f, 0.904f, 1.0f, 0.716f) == "Y 90%, Skew 72%");
  CHECK(incompleteMetrics(0.5f, 1.0f, 0.257f, 1.0f) == "X 50%, Size 26%");
}

TEST(incomplete_metrics_does_not_round_up_to_complete) {
  // 99.6% must not show as "100%"
  CHECK(incompleteMetrics(0.996f, 1.0f, 1.0f, 1.0f) == "X 99%");
}
