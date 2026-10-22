#include <filesystem>
#include <fstream>

#include "aprilgrid.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace {

std::string writeTemp(const std::string& name, const std::string& content) {
  fs::path path = fs::temp_directory_path() / ("aprilgrid_test_" + name);
  std::ofstream out(path);
  out << content;
  return path.string();
}

}  // namespace

TEST(detector_params_defaults) {
  auto params = defaultDetectorParameters();
  CHECK(params->markerBorderBits == 2);
  // Corners are refined on the X-junctions by detectAprilGrid (D11)
  CHECK(params->cornerRefinementMethod == cv::aruco::CORNER_REFINE_NONE);
  CHECK_NEAR(params->adaptiveThreshConstant, 25.0, 1e-9);
  CHECK(params->adaptiveThreshWinSizeMin == 5);
  CHECK(params->adaptiveThreshWinSizeMax == 21);
  CHECK(params->adaptiveThreshWinSizeStep == 4);
  CHECK_NEAR(params->minMarkerPerimeterRate, 0.02, 1e-9);
  CHECK_NEAR(params->polygonalApproxAccuracyRate, 0.03, 1e-9);
  CHECK_NEAR(params->minCornerDistanceRate, 0.05, 1e-9);
  CHECK(params->minDistanceToBorder == 3);
  CHECK_NEAR(params->minMarkerDistanceRate, 0.05, 1e-9);
}

TEST(detector_params_file_overrides_defaults) {
  auto path = writeTemp("params.yml",
                        "%YAML:1.0\n"
                        "cornerRefinementMethod: 3\n"
                        "adaptiveThreshWinSizeMax: 31\n"
                        "minMarkerPerimeterRate: 0.01\n");
  cv::Ptr<cv::aruco::DetectorParameters> params;
  std::string error, warning;
  CHECK(loadDetectorParameters(path, params, error, warning));
  CHECK(error.empty());
  CHECK(warning.empty());
  CHECK(params->cornerRefinementMethod == cv::aruco::CORNER_REFINE_APRILTAG);
  CHECK(params->adaptiveThreshWinSizeMax == 31);
  CHECK_NEAR(params->minMarkerPerimeterRate, 0.01, 1e-9);
  // Untouched fields keep the defaults
  CHECK(params->adaptiveThreshWinSizeMin == 5);
  CHECK(params->markerBorderBits == 2);
}

TEST(detector_params_border_bits_forced_to_kalibr) {
  auto path = writeTemp("border.yml", "%YAML:1.0\nmarkerBorderBits: 1\n");
  cv::Ptr<cv::aruco::DetectorParameters> params;
  std::string error, warning;
  CHECK(loadDetectorParameters(path, params, error, warning));
  CHECK(params->markerBorderBits == 2);
  CHECK(warning.find("markerBorderBits") != std::string::npos);
}

TEST(detector_params_unknown_field_warns) {
  auto path = writeTemp("typo.yml", "%YAML:1.0\nadaptiveThreshWinSizeMaxx: 31\n");
  cv::Ptr<cv::aruco::DetectorParameters> params;
  std::string error, warning;
  CHECK(loadDetectorParameters(path, params, error, warning));
  CHECK(warning.find("adaptiveThreshWinSizeMaxx") != std::string::npos);
}

TEST(detector_params_rejects_missing_file) {
  cv::Ptr<cv::aruco::DetectorParameters> params;
  std::string error, warning;
  CHECK(!loadDetectorParameters("/nonexistent/params.yml", params, error,
                                warning));
  CHECK(error.find("/nonexistent/params.yml") != std::string::npos);
}

TEST(detector_params_rejects_wrong_type) {
  auto path =
      writeTemp("type.yml", "%YAML:1.0\nadaptiveThreshWinSizeMax: \"big\"\n");
  cv::Ptr<cv::aruco::DetectorParameters> params;
  std::string error, warning;
  CHECK(!loadDetectorParameters(path, params, error, warning));
  CHECK(error.find("adaptiveThreshWinSizeMax") != std::string::npos);
}
