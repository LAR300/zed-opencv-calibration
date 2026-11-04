// zed_export_factory: writes the factory calibration of the connected camera
// ("F") in the OpenCV file the calibration tool writes, so that F can be
// treated as one more calibration in every metric.
// Spec: .ai/specs/exportar-fabrica/spec.md

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <unistd.h>

#include <sl/Camera.hpp>

#include "build_info.hpp"
#include "capture_dir.hpp"
#include "capture_support.hpp"
#include "meta_io.hpp"
#include "opencv_calib_file.hpp"
#include "zed_open.hpp"
#include "zed_raw_parameters.hpp"

namespace fs = std::filesystem;

namespace {

constexpr double VERIFY_REL_TOL = 1e-4;  // spec R4

struct Args {
  std::string output_root;
  std::string verify_file;
  bool verify = false;
  cv::Size expected_resolution;  // empty: not checked

  bool parse(int argc, char* argv[], std::string& error) {
    for (int i = 1; i < argc; i++) {
      const std::string arg = argv[i];
      if (arg == "--output_root" && i + 1 < argc) {
        output_root = argv[++i];
      } else if (arg == "--verify") {
        verify = true;
      } else if (arg == "--verify_file" && i + 1 < argc) {
        verify_file = argv[++i];
      } else if (arg == "--expect_resolution" && i + 1 < argc) {
        int w = 0, h = 0;
        char x = 0;
        std::istringstream in(argv[++i]);
        if (!(in >> w >> x >> h) || x != 'x' || w <= 0 || h <= 0) {
          error = "--expect_resolution must be WIDTHxHEIGHT, e.g. 1280x720";
          return false;
        }
        expected_resolution = cv::Size(w, h);
      } else if (arg == "--help" || arg == "-h") {
        std::cout
            << "Usage: " << argv[0] << " --output_root <dir> [--verify] [--expect_resolution WxH]\n"
            << "       " << argv[0] << " --verify_file <calib.yml>\n\n"
            << "Writes the factory calibration of the connected camera, opened as\n"
            << "zed_stereo_calibration opens it (same resolution and fps, SDK\n"
            << "self-calibration disabled), to <dir>/fabrica/SN<serial>/:\n"
            << "  calib_factory_<model>.yml  the OpenCV file read by the ZED SDK\n"
            << "  meta.json                  camera, SDK, code commit, time\n"
            << "  SN<serial>.conf            copy of the SDK factory file, if present\n"
            << "  export.log                 the terminal output\n"
            << "An existing folder is never overwritten.\n\n"
            << "  --verify            reopen the camera with the written file and check\n"
            << "                      that the SDK reads back the same parameters\n"
            << "  --verify_file <f>   only that check, for a given file\n"
            << "  --expect_resolution WxH  stop if the camera opens at another resolution\n";
        std::exit(EXIT_SUCCESS);
      } else {
        error = "Unknown or incomplete option '" + arg + "' (see --help)";
        return false;
      }
    }
    if (verify_file.empty() && output_root.empty()) {
      error = "--output_root is required (or --verify_file to only check a file)";
      return false;
    }
    return true;
  }
};

bool openCamera(sl::Camera& camera, const std::string& calibration_file, std::string& error) {
  sl::InitParameters init_params;
  setCalibrationOpenParameters(init_params);
  init_params.sdk_verbose = 0;
  if (!calibration_file.empty())
    init_params.optional_opencv_calibration_file = calibration_file.c_str();
  const sl::ERROR_CODE status = camera.open(init_params);
  if (status > sl::ERROR_CODE::SUCCESS) {
    error = std::string("Cannot open the camera: ") + sl::toString(status).c_str();
    return false;
  }
  return true;
}

void printParameters(const OpenCVStereoCalibration& c) {
  auto camera = [](const char* name, const cv::Mat& K, const cv::Mat& D) {
    std::cout << "   - " << name << ": fx " << K.at<double>(0, 0) << ", fy "
              << K.at<double>(1, 1) << ", cx " << K.at<double>(0, 2) << ", cy "
              << K.at<double>(1, 2) << ", D " << D << std::endl;
  };
  std::cout << std::setprecision(9);
  camera("left ", c.K_left, c.D_left);
  camera("right", c.K_right, c.D_right);
  std::cout << "   - T [mm]: " << c.T.t() << " (baseline " << -c.T.at<double>(0) << " mm)"
            << std::endl;
  std::cout << "   - R (Rodrigues): " << c.Rv.t() << std::endl;
}

// Reopen the camera with 'file' as its calibration and compare what the SDK
// reads back with 'expected'
bool verifyFile(const std::string& file, const RawStereoParameters& expected) {
  sl::Camera camera;
  std::string error;
  if (!openCamera(camera, file, error)) {
    std::cerr << "Verify: " << error << std::endl;
    return false;
  }
  const RawStereoParameters read = rawStereoParametersFrom(
      camera.getCameraInformation().camera_configuration.calibration_parameters_raw);
  camera.close();
  std::string diff;
  if (!sameRawParameters(expected, read, VERIFY_REL_TOL, diff)) {
    std::cerr << " * Verify FAILED for " << file << ": the SDK reads back " << diff
              << " (tolerance " << VERIFY_REL_TOL << " relative)" << std::endl;
    return false;
  }
  std::cout << " * Verify passed: the SDK reads " << file
            << " back as the factory calibration (within " << VERIFY_REL_TOL << " relative)"
            << std::endl;
  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  // A camera without a valid factory file still opens; it is refused below
  setenv("ZED_SDK_ALLOW_UNCALIBRATED_MODE", "1", 1);

  Args args;
  std::string error;
  if (!args.parse(argc, argv, error)) {
    std::cerr << "Error: " << error << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << "*** ZED factory calibration export ***" << std::endl;

  // --verify_file: only compare a file with the camera's factory calibration
  if (!args.verify_file.empty()) {
    if (!fs::exists(args.verify_file)) {
      std::cerr << "Error: '" << args.verify_file << "' does not exist" << std::endl;
      return EXIT_FAILURE;
    }
    sl::Camera camera;
    if (!openCamera(camera, "", error)) {
      std::cerr << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }
    const RawStereoParameters factory = rawStereoParametersFrom(
        camera.getCameraInformation().camera_configuration.calibration_parameters_raw);
    camera.close();
    return verifyFile(args.verify_file, factory) ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  LogTee log_tee;  // export.log, once the output folder exists
  const std::string started = utcNow();

  sl::Camera camera;
  if (!openCamera(camera, "", error)) {
    std::cerr << "Error: " << error << std::endl;
    return EXIT_FAILURE;
  }
  const sl::CameraInformation info = camera.getCameraInformation();
  const cv::Size resolution(info.camera_configuration.resolution.width,
                            info.camera_configuration.resolution.height);
  std::cout << " * Camera: " << sl::toString(info.camera_model) << ", S/N "
            << info.serial_number << ", firmware " << info.camera_configuration.firmware_version
            << ", " << resolution.width << " x " << resolution.height << " @ "
            << info.camera_configuration.fps << " fps" << std::endl;

  if (!args.expected_resolution.empty() && resolution != args.expected_resolution) {
    std::cerr << "Error: the camera opened at " << resolution.width << "x" << resolution.height
              << ", not at the expected " << args.expected_resolution.width << "x"
              << args.expected_resolution.height << std::endl;
    camera.close();
    return EXIT_FAILURE;
  }

  const RawStereoParameters raw =
      rawStereoParametersFrom(info.camera_configuration.calibration_parameters_raw);
  camera.close();
  OpenCVStereoCalibration calib;
  if (!convertRawParameters(raw, false, calib, error)) {
    std::cerr << "Error: " << error << std::endl;
    return EXIT_FAILURE;
  }

  const fs::path dir = fs::path(args.output_root) / "fabrica" /
                       ("SN" + std::to_string(info.serial_number));
  if (!prepareEmptyDir(dir.string(), error) ||
      !log_tee.attachFile((dir / "export.log").string(), error)) {
    std::cerr << "Error: " << error << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << " * Factory calibration (raw, " << modelName(calib) << "):" << std::endl;
  printParameters(calib);

  const fs::path calib_file = dir / ("calib_factory_" + modelName(calib) + ".yml");
  if (!writeOpenCVCalibration(calib_file.string(), calib, true, error)) {
    std::cerr << "Error: " << error << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << " * Written: " << calib_file.string() << std::endl;

  const fs::path conf = fs::path("/usr/local/zed/settings") /
                        ("SN" + std::to_string(info.serial_number) + ".conf");
  std::error_code copy_error;
  if (fs::exists(conf) && fs::copy_file(conf, dir / conf.filename(), copy_error))
    std::cout << " * Copied: " << conf.string() << std::endl;
  else
    std::cout << " ! No copy of the SDK factory file " << conf.string()
              << (copy_error ? ": " + copy_error.message() : std::string()) << std::endl;

  CaptureMeta meta;
  meta.camera.model = sl::toString(info.camera_model).c_str();
  meta.camera.serial = info.serial_number;
  meta.camera.firmware = info.camera_configuration.firmware_version;
  meta.camera.resolution = resolution;
  meta.camera.fps = static_cast<int>(std::lround(info.camera_configuration.fps));
  meta.sdk_version = sl::Camera::getSDKVersion().c_str();
  meta.code_commit = ZEDCAL_GIT_COMMIT;
  meta.code_dirty = ZEDCAL_GIT_DIRTY != 0;
  char hostname[256] = {0};
  if (gethostname(hostname, sizeof(hostname) - 1) == 0) meta.hostname = hostname;
  meta.self_calibration_disabled = true;
  meta.has_session = false;
  meta.started_utc = started;
  meta.ended_utc = utcNow();
  meta.end_reason = "factory calibration export";
  if (!writeCaptureMeta((dir / "meta.json").string(), meta, error)) {
    std::cerr << "Error: " << error << std::endl;
    return EXIT_FAILURE;
  }

  if (args.verify && !verifyFile(calib_file.string(), raw)) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
