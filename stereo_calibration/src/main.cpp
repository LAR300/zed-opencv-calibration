#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <memory>
#include <regex>
#include <sstream>

#include <unistd.h>

#include "aprilgrid.hpp"
#include "build_info.hpp"
#include "calibration_checker.hpp"
#include "capture_dir.hpp"
#include "capture_support.hpp"
#include "detections_io.hpp"
#include "meta_io.hpp"
#include "opencv_calibration.hpp"
#include "pose_script.hpp"
#include "sample_limits.hpp"
#include "session.hpp"
#include "script_runner.hpp"
#include "session_calibration.hpp"
#include "static_capture.hpp"
#include "timelapse.hpp"
#include "zed_camera_settings.hpp"
#include "zed_open.hpp"

static_assert(MIN_CALIBRATION_SAMPLES == MIN_IMAGE,
              "sample_limits.hpp must match the calibration minimum");

namespace fs = std::filesystem;

// *********************************************************************************
// CHANGE THIS PARAMS USING THE COMMAND LINE OPTIONS
// Learn more:
// * https://docs.opencv.org/4.x/da/d0d/tutorial_camera_calibration_pattern.html

int h_edges = 9;           // number of horizontal inner edges
int v_edges = 6;           // number of vertical inner edges
float square_size = 25.4;  // mm

// Default parameters are good for this checkerboard:
// https://github.com/opencv/opencv/blob/4.x/doc/pattern.png/
// *********************************************************************************

std::string image_folder = "zed-images/";

// Argument parsing helper
std::map<std::string, std::string> parseArguments(int argc, char* argv[]);

// Coverage indicator fill helpers
void addNewCheckerboardPosition(cv::Mat& coverage_indicator,
                                cv::Mat& pos_indicator,
                                cv::Mat& limits_indicator, float norm_x,
                                float norm_y, float norm_size, float min_x,
                                float max_x, float min_y, float max_y,
                                bool draw_rect);
void addNewCheckerboardPoly(cv::Mat& coverage_indicator,
                            const std::vector<cv::Point2f>& pts_l);
void addNewTargetPoly(cv::Mat& coverage_indicator,
                      const std::vector<cv::Point2f>& outside_corners);
void applyCoverageIndicatorOverlay(cv::Mat& image,
                                   const cv::Mat& coverage_indicator,
                                   const cv::Mat& limits_indicator);
void applyPosIndicatorOverlay(cv::Mat& image, const cv::Mat& pos_indicator);
void drawBanner(cv::Mat& image, const std::vector<std::string>& lines,
                const cv::Scalar& color);

// Script mode (.ai/specs/roteiro-poses R5-R11, captura-estatica R1-R6): what
// the current frame shows of the target, for the panel and the save rule
struct ScriptFrame {
  bool pose_known = false;
  ViewGeometry view;
  PoseCheck check;
  StabilityState still;
  double sharp_l = 0.0, sharp_r = 0.0;
  bool sharp = false;
};
std::string describePose(const ScriptPose& pose);
void drawExpectedPose(cv::Mat& image, const ScriptPose& pose);
void drawScriptPanel(cv::Mat& image, const ScriptRunner& script, const ScriptFrame& f,
                     const StaticCaptureConfig& still_cfg, const std::string& message);

/// Rendering
constexpr int text_area_height = 380;   // Height of the area below the images
const cv::Size display_size(720, 404);  // Size of the rendered images

/// Calibration condition
const float max_repr_error = 0.5;  // in pixels
// Changed with --min_samples / --max_samples (max 0 = no maximum)
int min_samples = COLLECTION_MIN_SAMPLES;
int max_samples = COLLECTION_MAX_SAMPLES;
const float min_avg_x_coverage =
    0.65f;  // Checkerboard X position covering percentage of the image width
const float min_avg_y_coverage =
    0.65f;  // Checkerboard Y position covering percentage of the image height
const float min_area_range =
    0.38f;  // Checkerboard area range size [min_area-max_area]
const float min_skew_range =
    0.335f;  // Checkerboard skew ange size [min_skew-max_skew]
const float min_b_x_coverage = 0.8f;  // Checkerboard X position close to border
                                      // covering percentage of the image width
const float min_b_y_coverage = 0.8f;  // Checkerboard Y position close to border
                                      // covering percentage of the image height

const float min_target_area = 0.02f;  // Ignore checkerboards smaller than this
                                     // area (percentage of image area)
const double min_sharpness = 100.0;  // Laplacian variance threshold — frames below
                                     // this are considered blurry and rejected

// Debug
bool verbose = false;
int sdk_verbose = 0;

// SIDE-by-SIDE or TOP-BOTTOM image stacking for display
const bool image_stack_horizontal =
    true;  // true for horizontal, false for vertical

// Scale keypoints according to image size change
void scaleKP(std::vector<cv::Point2f>& pts, cv::Size in, cv::Size out) {
  float rx = out.width / static_cast<float>(in.width);
  float ry = out.height / static_cast<float>(in.height);

  for (auto& it : pts) {
    it.x *= rx;
    it.y *= ry;
  }
}

struct Args {
  std::string app_name;
  std::string svo_path = "";
  bool is_radtan_lens = true;
  bool is_zed_x_one_virtual_stereo = false;
  int left_camera_id = -1;
  int right_camera_id = -1;
  int left_camera_sn = -1;
  int right_camera_sn = -1;
  bool use_stored_images = false;
  std::string target_yaml;           // Kalibr AprilGrid target file
  std::string detector_params_file;  // AprilGrid detector parameters
  int min_tags = DEFAULT_MIN_TAGS;
  int min_rows = DEFAULT_MIN_TAG_ROWS;
  int min_cols = DEFAULT_MIN_TAG_COLS;
  bool sample_limits_given = false;  // --min_samples or --max_samples passed
  std::string sample_limits_error;   // set when one of them is not a number
  std::string camera_settings_file;  // capture settings to apply
  std::string session_file;          // traceable capture mode (--session)
  std::string detections_out;        // detections.json of the stored images
  std::string recalibrate_dir;       // session capture to calibrate again
  double timelapse_interval_s = 0.0;   // --timelapse: one pair every interval
  double timelapse_duration_min = 0.0; // ... for this long (0: off)
  std::string timelapse_error;         // set when its values are not numbers
  bool fisheye_given = false;        // --fisheye passed explicitly

  void parse(int argc, char* argv[]) {
    app_name = argv[0];
    for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];
      if (arg == "--svo" && i + 1 < argc) {
        svo_path = argv[++i];
      } else if (arg == "--fisheye") {
        is_radtan_lens = false;
        fisheye_given = true;
      } else if (arg == "--virtual") {
        is_zed_x_one_virtual_stereo = true;
      } else if (arg == "--left_id" && i + 1 < argc) {
        left_camera_id = std::stoi(argv[++i]);
      } else if (arg == "--right_id" && i + 1 < argc) {
        right_camera_id = std::stoi(argv[++i]);
      } else if (arg == "--left_sn" && i + 1 < argc) {
        left_camera_sn = std::stoi(argv[++i]);
      } else if (arg == "--right_sn" && i + 1 < argc) {
        right_camera_sn = std::stoi(argv[++i]);
      } else if (arg == "--h_edges" && i + 1 < argc) {
        h_edges = std::stoi(argv[++i]);
      } else if (arg == "--v_edges" && i + 1 < argc) {
        v_edges = std::stoi(argv[++i]);
      } else if (arg == "--square_size" && i + 1 < argc) {
        square_size = std::stof(argv[++i]);
      } else if (arg == "--use_stored_values") {
        use_stored_images = true;
      } else if (arg == "--target_yaml" && i + 1 < argc) {
        target_yaml = argv[++i];
      } else if (arg == "--camera_settings" && i + 1 < argc) {
        camera_settings_file = argv[++i];
      } else if (arg == "--session" && i + 1 < argc) {
        session_file = argv[++i];
      } else if (arg == "--detections_out" && i + 1 < argc) {
        detections_out = argv[++i];
      } else if (arg == "--recalibrate" && i + 1 < argc) {
        recalibrate_dir = argv[++i];
      } else if (arg == "--timelapse" && i + 2 < argc) {
        try {
          timelapse_interval_s = std::stod(argv[++i]);
          timelapse_duration_min = std::stod(argv[++i]);
        } catch (const std::exception&) {
          timelapse_error = "--timelapse expects <interval_s> <duration_min>";
        }
      } else if (arg == "--detector_params" && i + 1 < argc) {
        detector_params_file = argv[++i];
      } else if (arg == "--min_tags" && i + 1 < argc) {
        min_tags = std::stoi(argv[++i]);
      } else if (arg == "--min_rows" && i + 1 < argc) {
        min_rows = std::stoi(argv[++i]);
      } else if (arg == "--min_cols" && i + 1 < argc) {
        min_cols = std::stoi(argv[++i]);
      } else if ((arg == "--min_samples" || arg == "--max_samples") &&
                 i + 1 < argc) {
        sample_limits_given = true;
        int& target = (arg == "--min_samples") ? min_samples : max_samples;
        std::string error;
        if (!parseSampleCount(argv[++i], target, error) &&
            sample_limits_error.empty())
          sample_limits_error = arg + ": " + error;
      } else if (arg == "--help" || arg == "-h") {
        std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
        std::cout << "  --h_edges <value>      Number of horizontal inner "
                     "edges of the checkerboard"
                  << std::endl;
        std::cout << "  --v_edges <value>      Number of vertical inner edges "
                     "of the checkerboard"
                  << std::endl;
        std::cout << "  --square_size <value>  Size of a square in the "
                     "checkerboard (in mm)"
                  << std::endl;
        std::cout << "  --target_yaml <file>   Kalibr AprilGrid target file "
                     "(replaces the checkerboard)."
                  << std::endl;
        std::cout << "  --detector_params <file> OpenCV YAML file with AprilGrid "
                     "detector parameters."
                  << std::endl;
        std::cout << "  --min_tags <value>     Minimum AprilGrid tags seen in "
                     "both images (default "
                  << DEFAULT_MIN_TAGS << ")." << std::endl;
        std::cout << "  --min_rows <value>     Minimum tag rows seen in both "
                     "images (default "
                  << DEFAULT_MIN_TAG_ROWS << ")." << std::endl;
        std::cout << "  --min_cols <value>     Minimum tag columns seen in both "
                     "images (default "
                  << DEFAULT_MIN_TAG_COLS << ")." << std::endl;
        std::cout << "  --min_samples <value>  Samples needed before the "
                     "collection can end on complete metrics (default "
                  << COLLECTION_MIN_SAMPLES << ", at least "
                  << MIN_CALIBRATION_SAMPLES << ")." << std::endl;
        std::cout << "  --max_samples <value>  Samples after which the "
                     "collection pauses if metrics are incomplete (default "
                  << COLLECTION_MAX_SAMPLES << ", 0 = no maximum)." << std::endl;
        std::cout << "  --use_stored_values    Recompute the calibration from "
                     "the images stored in 'zed-images/'."
                  << std::endl;
        std::cout << "  --camera_settings <file> YAML file with the capture "
                     "settings to apply (exposure, gain, white balance...), "
                     "restored on exit."
                  << std::endl;
        std::cout << "  --session <file>       Traceable capture: images, detections "
                     "and metadata go to <output_root>/<condition>/captura_<XX>/ "
                     "(see config/session_example.yaml). Requires --target_yaml."
                  << std::endl;
        std::cout << "  --detections_out <file> With --use_stored_values: also write "
                     "the detections of the stored images to <file> (detections.json "
                     "format). An existing file is never overwritten."
                  << std::endl;
        std::cout << "  --recalibrate <dir>    Calibrate again a session capture folder "
                     "(captura_<XX>), without the camera, with the 'calibration' section "
                     "of its session.yaml. Requires --target_yaml. Existing results are "
                     "never overwritten."
                  << std::endl;
        std::cout << "  --timelapse <interval_s> <duration_min> With --session: save one "
                     "pair every interval, without 's', with pose_id 'thermal', for the "
                     "duration, then stop without calibrating (warm-up record, "
                     ".ai/specs/aquecimento-termico). The target must stay still in view."
                  << std::endl;
        std::cout << "  --svo <file>      Path to the SVO file." << std::endl;
        std::cout << "  --fisheye         Use fisheye lens model." << std::endl;
        std::cout << "  --virtual         Use ZED X One cameras as a virtual "
                     "stereo pair."
                  << std::endl;
        std::cout << "  --left_id <id>    Id of the left camera if using "
                     "virtual stereo."
                  << std::endl;
        std::cout << "  --right_id <id>   Id of the right camera if using "
                     "virtual stereo."
                  << std::endl;
        std::cout << "  --left_sn <sn>    S/N of the left camera if using "
                     "virtual stereo."
                  << std::endl;
        std::cout << "  --right_sn <sn>   S/N of the right camera if using "
                     "virtual stereo."
                  << std::endl;
        std::cout << "  --help, -h        Show this help message." << std::endl
                  << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << std::endl
                  << "* ZED Stereo Camera using an SVO file:" << std::endl;
        std::cout << "  " << argv[0] << " --svo camera.svo" << std::endl;
        std::cout << std::endl
                  << "* ZED Stereo Camera with a Kalibr AprilGrid target:"
                  << std::endl;
        std::cout << "  " << argv[0] << " --target_yaml target.yaml"
                  << std::endl;
        std::cout << std::endl
                  << "* Virtual Stereo Camera using camera IDs:" << std::endl;
        std::cout << "  " << argv[0] << " --virtual --left_id 0 --right_id 1"
                  << std::endl;
        std::cout << std::endl
                  << "* Virtual Stereo Camera using camera serial numbers and "
                     "a custom checkerboard:"
                  << std::endl;
        std::cout << "  " << argv[0]
                  << " --virtual --left_sn 301528071 --right_sn 300473441 "
                     "--h_edges 12 --v_edges 9 --square_size 30.0"
                  << std::endl;
        std::cout << std::endl
                  << "* Virtual Stereo Camera with fisheye lenses using camera "
                     "serial numbers:"
                  << std::endl;
        std::cout
            << "  " << argv[0]
            << " --fisheye --virtual --left_sn 301528071 --right_sn 300473441"
            << std::endl;
        std::cout << std::endl;
        exit(0);
      }
    }
  }
};

// --recalibrate: the session calibration of an existing capture folder, from
// its pairs and its session.yaml (.ai/specs/saida-calibracao R9)
int runRecalibrate(const Args& args, const AprilGridOptions& aprilgrid) {
  const std::string dir = args.recalibrate_dir;
  std::string error;
  Session session;
  if (!loadSession(dir + "/session.yaml", session, error)) {
    std::cerr << "Error: " << error << std::endl;
    return EXIT_FAILURE;
  }
  const std::string& model = session.calibration.model;
  if (args.fisheye_given && model != "fisheye4") {
    std::cerr << "Error: --fisheye contradicts 'calibration.model: " << model
              << "' in the session" << std::endl;
    return EXIT_FAILURE;
  }
  for (const std::string& f :
       {sessionCalibrationFile(dir, model), sessionReportFile(dir, model)}) {
    if (fs::exists(f)) {
      std::cerr << "Error: '" << f << "' already exists and is never overwritten" << std::endl;
      return EXIT_FAILURE;
    }
  }
  DetectionsHeader header;
  std::vector<FrameDetections> frames;
  if (!readDetections(dir + "/detections.json", header, frames, error)) {
    std::cerr << "Error: " << error << std::endl;
    return EXIT_FAILURE;
  }
  std::vector<std::pair<std::string, std::string>> pairs;
  SessionCalibration session_calibration;
  for (const auto& f : frames) {
    pairs.emplace_back(dir + "/" + f.file_left, dir + "/" + f.file_right);
    session_calibration.view_ids.emplace_back(f.pose_id, f.n);
  }
  session_calibration.config = session.calibration;
  session_calibration.output_dir = dir;
  session_calibration.code_commit = ZEDCAL_GIT_COMMIT;
  session_calibration.code_dirty = ZEDCAL_GIT_DIRTY != 0;

  StereoCalib calib;
  calib.initDefault(model == "rational8");
  bool use_prior = false;
  int serial = 0;
  {
    std::ifstream meta(dir + "/meta.json");
    std::stringstream text;
    text << meta.rdbuf();
    std::smatch m;
    const std::string content = text.str();
    if (std::regex_search(content, m, std::regex("\"serial\":\\s*(\\d+)")))
      serial = std::stoi(m[1]);
  }
  if (session.calibration.prior == "factory") {
    // Without the camera, the factory prior is the exported F, which the SDK
    // reads back identically (.ai/specs/exportar-fabrica)
    const std::string f_file = session.output_root + "/fabrica/SN" + std::to_string(serial) +
                               "/calib_factory_" + model + ".yml";
    OpenCVStereoCalibration f;
    if (serial == 0 || !readOpenCVCalibration(f_file, f, error)) {
      std::cerr << "Error: the factory prior needs " << f_file
                << " (zed_export_factory): " << error << std::endl;
      return EXIT_FAILURE;
    }
    calib.left.K = f.K_left;
    calib.left.D = f.D_left;
    calib.right.K = f.K_right;
    calib.right.D = f.D_right;
    calib.T = f.T;
    calib.Rv = f.Rv;
    cv::Rodrigues(calib.Rv, calib.R);
    use_prior = true;
    std::cout << " * Prior: factory calibration " << f_file << std::endl;
  } else {
    std::cout << " * Prior: none (calibration from scratch)" << std::endl;
  }
  std::cout << " * Recalibrating " << pairs.size() << " pairs of " << dir << " ("
            << model << ")" << std::endl;
  return calibrate(-1, "", calib, h_edges, v_edges, square_size, serial, false, false, false,
                   use_prior, session.calibration.max_rms_px, verbose, &aprilgrid, &pairs, dir,
                   &session_calibration);
}

int main(int argc, char* argv[]) {
  // Allow the ZED SDK to open the camera even if it has no valid calibration
  setenv("ZED_SDK_ALLOW_UNCALIBRATED_MODE", "1", 1);

  // Setup the calibration checker
  const DetectedBoardParams idealParams = {
      cv::Point2f(min_avg_x_coverage, min_avg_y_coverage), min_area_range,
      min_skew_range, min_b_x_coverage, min_b_y_coverage};  

  // Flags
  bool is_dual_mono_camera = false;
  bool is_4k_camera = false;

  // Parse command line arguments
  Args args;
  args.parse(argc, argv);

  // Session mode: the terminal output is kept from the first line, and goes
  // to capture.log once the capture folder exists
  std::unique_ptr<LogTee> log_tee;
  if (!args.session_file.empty()) log_tee = std::make_unique<LogTee>();

  std::cout << "*** Stereo Camera Calibration Tool ***" << std::endl;
  std::cout << std::endl;

  // Sample limits are checked before anything else, the camera included
  {
    std::string error = args.sample_limits_error;
    if (error.empty()) validateSampleLimits(min_samples, max_samples, error);
    if (!error.empty()) {
      std::cerr << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }
  }

  // Calibration target: checkerboard (default) or Kalibr AprilGrid
  const bool use_aprilgrid = !args.target_yaml.empty();
  AprilGridOptions aprilgrid;
  if (use_aprilgrid) {
    std::string error, warning;
    if (!loadKalibrTarget(args.target_yaml, aprilgrid.target, error)) {
      std::cerr << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }
    if (!args.detector_params_file.empty()) {
      if (!loadDetectorParameters(args.detector_params_file,
                                  aprilgrid.detector_params, error, warning)) {
        std::cerr << "Error: " << error << std::endl;
        return EXIT_FAILURE;
      }
      if (!warning.empty()) std::cerr << "Warning: " << warning << std::endl;
    }
    aprilgrid.min_tags = args.min_tags;
    aprilgrid.min_rows = args.min_rows;
    aprilgrid.min_cols = args.min_cols;

    std::cout << "The calibration process requires an AprilGrid of known "
                 "characteristics."
              << std::endl;
    std::cout << " * Target file: " << args.target_yaml << std::endl;
    printAprilGridTarget(aprilgrid.target);
    std::cout << " * Minimum per sample: " << aprilgrid.min_tags
              << " tags seen in both images, over " << aprilgrid.min_rows
              << " rows and " << aprilgrid.min_cols << " columns"
              << std::endl;
  } else {
    std::cout << "The calibration process requires a checkerboard of known "
                 "characteristics."
              << std::endl;
    std::cout << " * Expected checkerboard features:" << std::endl;
    std::cout << "   - Inner horizontal edges:\t" << h_edges << std::endl;
    std::cout << "   - Inner vertical edges:\t" << v_edges << std::endl;
    std::cout << "   - Square size:\t\t" << square_size << " mm" << std::endl;
    std::cout << "Change these parameters using the command line options if "
                 "needed. Use the '-h' option for help."
              << std::endl;
  }
  std::cout << std::endl;

  if (args.use_stored_images) {
    if (args.sample_limits_given)
      std::cout << " * --min_samples / --max_samples have no effect with "
                   "--use_stored_values: all the stored images are used."
                << std::endl;
  } else {
    std::cout << " * Sample limits: min. " << min_samples << ", "
              << (max_samples == 0 ? std::string("no maximum")
                                   : "max. " + std::to_string(max_samples))
              << std::endl;
  }

  // Capture settings file: read before opening the camera
  std::vector<CameraSetting> camera_settings;
  if (!args.camera_settings_file.empty()) {
    std::string error;
    if (!loadCameraSettings(args.camera_settings_file, camera_settings, error)) {
      std::cerr << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }
    if (args.use_stored_images)
      std::cout << " * --camera_settings has no effect with --use_stored_values: "
                   "the stored images were taken with the settings printed in "
                   "the log of their capture."
                << std::endl;
  }

  if (!args.recalibrate_dir.empty()) {
    if (!use_aprilgrid || !args.session_file.empty() || args.use_stored_images ||
        !args.detections_out.empty()) {
      std::cerr << "Error: --recalibrate requires --target_yaml and cannot be combined with "
                   "--session, --use_stored_values or --detections_out."
                << std::endl;
      return EXIT_FAILURE;
    }
    const int err = runRecalibrate(args, aprilgrid);
    std::cout << std::endl
              << (err == EXIT_SUCCESS ? " +++++ Calibration successful +++++"
                                      : " ----- Calibration failed -----")
              << std::endl;
    return err;
  }

  // --detections_out: only for the stored images, never over an existing file
  // (.ai/specs/persistencia-captura R10)
  if (!args.detections_out.empty()) {
    if (!args.use_stored_images) {
      std::cerr << "Error: --detections_out works with --use_stored_values; a live "
                   "capture writes its detections with --session."
                << std::endl;
      return EXIT_FAILURE;
    }
    if (fs::exists(args.detections_out)) {
      std::cerr << "Error: '" << args.detections_out
                << "' already exists and is never overwritten." << std::endl;
      return EXIT_FAILURE;
    }
  }

  // --timelapse: a warm-up record of a session (.ai/specs/aquecimento-termico R1)
  const bool use_timelapse = args.timelapse_duration_min > 0.0 || args.timelapse_interval_s > 0.0 ||
                             !args.timelapse_error.empty();
  if (use_timelapse) {
    if (!args.timelapse_error.empty() || args.timelapse_interval_s <= 0.0 ||
        args.timelapse_duration_min <= 0.0 ||
        args.timelapse_duration_min * 60.0 < args.timelapse_interval_s) {
      std::cerr << "Error: --timelapse expects <interval_s> <duration_min>, both greater than 0 "
                   "and the duration at least one interval."
                << std::endl;
      return EXIT_FAILURE;
    }
    if (args.session_file.empty()) {
      std::cerr << "Error: --timelapse records a session: it requires --session." << std::endl;
      return EXIT_FAILURE;
    }
  }

  // --svo: images from a recorded file (.ai/specs/higiene-ferramentas R2). A
  // session records what the camera reports during the capture (temperature,
  // uptime, settings), which an SVO does not reproduce.
  if (!args.svo_path.empty()) {
    if (!args.session_file.empty() || !args.camera_settings_file.empty() || use_timelapse) {
      std::cerr << "Error: --svo cannot be combined with --session, --timelapse or "
                   "--camera_settings: they describe a live camera."
                << std::endl;
      return EXIT_FAILURE;
    }
    if (args.use_stored_images) {
      std::cerr << "Error: --svo and --use_stored_values are two different inputs; pass one."
                << std::endl;
      return EXIT_FAILURE;
    }
  }

  // Session (traceable capture mode): read and its folder prepared before
  // opening the camera (.ai/specs/persistencia-captura R1-R3)
  const bool use_session = !args.session_file.empty();
  Session session;
  std::string capture_dir;
  if (use_session) {
    std::string error;
    if (args.use_stored_images) {
      std::cerr << "Error: --session records a new live capture; it cannot be "
                   "combined with --use_stored_values."
                << std::endl;
      return EXIT_FAILURE;
    }
    if (!use_aprilgrid) {
      std::cerr << "Error: --session requires the AprilGrid target "
                   "(--target_yaml)."
                << std::endl;
      return EXIT_FAILURE;
    }
    if (!loadSession(args.session_file, session, error)) {
      std::cerr << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }
    capture_dir = captureDir(session);
    if (!prepareCaptureDir(capture_dir, error)) {
      std::cerr << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }
    if (!log_tee->attachFile(capture_dir + "/capture.log", error)) {
      std::cerr << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }
    std::error_code copy_error;
    fs::copy_file(args.session_file, capture_dir + "/session.yaml", copy_error);
    if (copy_error) {
      std::cerr << "Error: cannot copy the session file to " << capture_dir
                << ": " << copy_error.message() << std::endl;
      return EXIT_FAILURE;
    }
    if (args.fisheye_given && session.calibration.model != "fisheye4") {
      std::cerr << "Error: --fisheye contradicts 'calibration.model: "
                << session.calibration.model << "' in the session" << std::endl;
      return EXIT_FAILURE;
    }
    args.is_radtan_lens = session.calibration.model == "rational8";
    std::cout << " * Session: " << args.session_file << std::endl;
    std::cout << "   - Condition: " << session.condition << ", capture "
              << session.capture << ", lens " << session.lens << std::endl;
    std::cout << "   - Output folder: " << capture_dir << std::endl;
  }

  // Initialize the calibration checker. Its maximum is unlimited: the
  // maximum is handled by the collection loop, which pauses instead of
  // ending (.ai/specs/sample-limits/decisions.md D1)
  CalibrationChecker checker(cv::Size(h_edges, v_edges), square_size,
                             min_samples, std::numeric_limits<size_t>::max(),
                             min_target_area, idealParams, verbose);

  // Initialize the stereo calibration data structure
  StereoCalib calib;
  calib.initDefault(args.is_radtan_lens);
  std::cout << " * Lens distorsion model: "
            << (args.is_radtan_lens ? "Radial-Tangential" : "Fisheye")
            << std::endl;

  // Session mode: the saved pairs and their detections
  std::vector<std::pair<std::string, std::string>> session_images;
  SessionCalibration session_calibration;
  if (use_session) {
    session_calibration.config = session.calibration;
    session_calibration.output_dir = capture_dir;
    session_calibration.code_commit = ZEDCAL_GIT_COMMIT;
    session_calibration.code_dirty = ZEDCAL_GIT_DIRTY != 0;
  }
  std::unique_ptr<DetectionsWriter> detections_writer;

  // Status variables
  int image_count = -1;
  bool can_use_calib_prior = false;
  sl::CameraInformation zed_info;

  if (!args.use_stored_images) {
    // Coverage scores
    float size_score = 0.0f, skew_score = 0.0f, pos_score_x = 0.0f,
          pos_score_y = 0.0f, min_bx = 0.0f, max_bx = 0.0f, min_by = 0.0f,
          max_by = 0.0f, min_size = 0.0f, max_size = 0.0f, min_skew = 0.0f,
          max_skew = 0.0f;

    // ZED Camera initialization
    sl::Camera zed_camera;
    sl::InitParameters init_params;
    // Resolution, fps, no depth, no self-calibration: shared with
    // zed_export_factory (common/calib_io/zed_open.hpp)
    setCalibrationOpenParameters(init_params);
    init_params.sdk_verbose = sdk_verbose;

    // Configure the Virtual Stereo Camera if '--virtual' argument is provided
    if (args.is_zed_x_one_virtual_stereo) {
      std::cout << " * Virtual Stereo Camera mode enabled." << std::endl;
      is_dual_mono_camera = true;
      int sn_left = args.left_camera_sn;
      int sn_right = args.right_camera_sn;

      if (sn_left != -1 && sn_right != -1) {
        std::cout << " * Using serial numbers for left and right cameras: "
                  << sn_left << ", " << sn_right << std::endl;

        int sn_stereo =
            sl::generateVirtualStereoSerialNumber(sn_left, sn_right);
        std::cout << " * Unique Virtual SN: " << sn_stereo
                  << " (Generated by the ZED SDK)" << std::endl;
        init_params.input.setVirtualStereoFromSerialNumbers(sn_left, sn_right,
                                                            sn_stereo);
      } else {
        if (args.left_camera_id == -1 || args.right_camera_id == -1) {
          std::cerr << "Error: Left and Right camera IDs or Left and Right "
                       "camera Serial Numbers must be both provided."
                    << std::endl;
          std::cerr << " * use the command '" << args.app_name
                    << " -h' for details." << std::endl;
          return EXIT_FAILURE;
        }

        std::cout << "Using camera IDs for left and right cameras: "
                  << args.left_camera_id << ", " << args.right_camera_id
                  << std::endl;

        auto cams = sl::CameraOne::getDeviceList();

        for (auto& cam : cams) {
          if (cam.id == args.left_camera_id) {
            sn_left = cam.serial_number;
          } else if (cam.id == args.right_camera_id) {
            sn_right = cam.serial_number;
          }
        }

        if (sn_left == -1 || sn_right == -1) {
          std::cerr << "Error: Could not find serial numbers for the provided "
                       "camera IDs."
                    << std::endl;
          std::cerr
              << " * use the command 'ZED_Explore --all' to get the camera "
                 "ID or the Serial Number of the connected cameras."
              << std::endl;
          return EXIT_FAILURE;
        }

        int sn_stereo =
            sl::generateVirtualStereoSerialNumber(sn_left, sn_right);
        std::cout << " * Unique Virtual SN: " << sn_stereo
                  << " (Generated by the ZED SDK)" << std::endl;

        init_params.input.setVirtualStereoFromCameraIDs(
            args.left_camera_id, args.right_camera_id, sn_stereo);
      }

      int left_model = sn_left / 10000000;
      int right_model = sn_right / 10000000;

      if (left_model != right_model) {
        std::cerr << "Error: Left and Right cameras must be of the same model."
                  << std::endl;
        return EXIT_FAILURE;
      }

      if (left_model == static_cast<int>(sl::MODEL::ZED_XONE_UHD) &&
          right_model == static_cast<int>(sl::MODEL::ZED_XONE_UHD)) {
        is_4k_camera = true;
        init_params.camera_resolution = sl::RESOLUTION::HD4K;
        std::cout << " * ZED X One 4K Virtual Stereo Camera detected."
                  << std::endl;
      } else {
        is_4k_camera = false;
        init_params.camera_resolution = sl::RESOLUTION::HD1200;
        std::cout << " * ZED X One GS Virtual Stereo Camera detected."
                  << std::endl;
      }
    }

    if (!args.svo_path.empty()) {
      init_params.input.setFromSVOFile(args.svo_path.c_str());
      std::cout << " * Input: SVO file " << args.svo_path
                << " (self calibration disabled, as live)" << std::endl;
    }

    auto status = zed_camera.open(init_params);

    // in case of a virtual stereo camera, the calibration file can be not
    // available
    if (status > sl::ERROR_CODE::SUCCESS &&
        status != sl::ERROR_CODE::INVALID_CALIBRATION_FILE) {
      std::cerr << "Error opening ZED camera: " << sl::toString(status)
                << std::endl;
      return 1;
    }

    zed_info = zed_camera.getCameraInformation();

    // Print camera information
    std::cout << " * Camera Model: " << sl::toString(zed_info.camera_model)
              << std::endl;
    std::cout << " * Camera Serial Number: " << zed_info.serial_number
              << std::endl;
    std::cout << " * Camera Resolution: "
              << zed_info.camera_configuration.resolution.width << " x "
              << zed_info.camera_configuration.resolution.height << std::endl;

    // Capture settings: applied now, restored on every exit from here
    // (.ai/specs/camera-settings/decisions.md D5)
    installInterruptHandler();
    CameraSettingsGuard settings_guard(zed_camera,
                                       snapshotCameraSettings(zed_camera));
    if (!camera_settings.empty()) {
      std::string error;
      if (!validateCameraSettings(zed_camera, camera_settings, error) ||
          !applyCameraSettings(zed_camera, camera_settings, error)) {
        std::cerr << "Error: " << error << std::endl;
        return EXIT_FAILURE;
      }
      printCameraSettings(zed_camera, "Camera settings (applied from " +
                                          args.camera_settings_file + ")");
    } else {
      printCameraSettings(zed_camera,
                          "Camera settings (current values, not changed)");
    }

    // change can_use_calib_prior if you dont want to use the calibration file
    // Session mode: metadata of the capture (persistencia-captura R8, R9)
    CaptureMeta capture_meta;
    const auto camera_opened_at = std::chrono::steady_clock::now();
    bool temperature_warned = false;
    if (use_session) {
      capture_meta.camera.model = sl::toString(zed_info.camera_model).c_str();
      capture_meta.camera.serial = zed_info.serial_number;
      capture_meta.camera.firmware = zed_info.camera_configuration.firmware_version;
      capture_meta.camera.resolution =
          cv::Size(zed_info.camera_configuration.resolution.width,
                   zed_info.camera_configuration.resolution.height);
      capture_meta.camera.fps =
          static_cast<int>(std::lround(zed_info.camera_configuration.fps));
      capture_meta.camera.lens = session.lens;
      capture_meta.sdk_version = sl::Camera::getSDKVersion().c_str();
      capture_meta.code_commit = ZEDCAL_GIT_COMMIT;
      capture_meta.code_dirty = ZEDCAL_GIT_DIRTY != 0;
      char hostname[256] = {0};
      if (gethostname(hostname, sizeof(hostname) - 1) == 0)
        capture_meta.hostname = hostname;
      capture_meta.self_calibration_disabled = init_params.camera_disable_self_calib;
      capture_meta.session = session;
      capture_meta.camera_settings_start = snapshotCameraSettings(zed_camera).values;
      capture_meta.started_utc = utcNow();
    }

    // Script mode (.ai/specs/roteiro-poses R5-R12): the session names a pose
    // script; the PnP pose that guides the operator uses F, or the calibration
    // the session names, and never enters the calibration (R7)
    std::unique_ptr<ScriptRunner> script;
    std::unique_ptr<StabilityTracker> stability;
    OpenCVStereoCalibration pose_calib;
    std::string script_message;  // why the last 's'/'o' did not save
    if (use_session && session.pose_script) {
      std::string error;
      PoseScript loaded;
      if (use_timelapse) {
        std::cerr << "Error: a session with 'roteiro' cannot be recorded with --timelapse."
                  << std::endl;
        return EXIT_FAILURE;
      }
      if (!loadPoseScript(*session.pose_script, loaded, error)) {
        std::cerr << "Error: " << error << std::endl;
        return EXIT_FAILURE;
      }
      const std::string calib_file =
          session.pose_calibration
              ? *session.pose_calibration
              : session.output_root + "/fabrica/SN" + std::to_string(zed_info.serial_number) +
                    "/calib_factory_rational8.yml";
      if (!readOpenCVCalibration(calib_file, pose_calib, error)) {
        std::cerr << "Error: the script mode needs a calibration for the pose guidance: " << error
                  << ". Export F with zed_export_factory, or set 'pose_calibration' in the "
                     "session."
                  << std::endl;
        return EXIT_FAILURE;
      }
      if (pose_calib.fisheye ||
          pose_calib.image_size != cv::Size(zed_info.camera_configuration.resolution.width,
                                            zed_info.camera_configuration.resolution.height)) {
        std::cerr << "Error: the pose guidance calibration " << calib_file
                  << " must be a rational model at the camera resolution." << std::endl;
        return EXIT_FAILURE;
      }
      script = std::make_unique<ScriptRunner>(loaded);
      stability = std::make_unique<StabilityTracker>(session.static_capture.max_displacement_px,
                                                     session.static_capture.window_s);
      ScriptMeta script_meta;
      script_meta.name = loaded.name;
      script_meta.file = *session.pose_script;
      capture_meta.script = script_meta;
      std::cout << " * Script: " << loaded.name << " (" << script->size() << " poses), pose "
                << "guidance with " << calib_file << std::endl
                << "   's' save, 'o' save out of tolerance, 'k' skip, 'b' back" << std::endl;
    }
    // Writes meta.json; called after every saved pair and when the collection ends
    auto write_meta = [&]() {
      if (!use_session) return;
      std::string meta_error;
      if (!writeCaptureMeta(capture_dir + "/meta.json", capture_meta, meta_error))
        std::cerr << "  ! " << meta_error << std::endl;
    };
    auto end_meta = [&](const std::string& reason) {
      if (!use_session) return;
      capture_meta.camera_settings_end = snapshotCameraSettings(zed_camera).values;
      capture_meta.ended_utc = utcNow();
      capture_meta.end_reason = reason;
      write_meta();
    };
    // Temperatures next to the image sensors and in the IMU; null when the
    // sensors are not reachable (HID access, see ambiente-sensores)
    auto read_temperatures = [&](FrameMeta& frame) {
      sl::SensorsData sensors;
      bool any = false;
      if (zed_camera.getSensorsData(sensors, sl::TIME_REFERENCE::CURRENT) ==
          sl::ERROR_CODE::SUCCESS) {
        using Location = sl::SensorsData::TemperatureData::SENSOR_LOCATION;
        const std::pair<Location, std::optional<double>*> locations[] = {
            {Location::ONBOARD_LEFT, &frame.temperature_onboard_left},
            {Location::ONBOARD_RIGHT, &frame.temperature_onboard_right},
            {Location::IMU, &frame.temperature_imu}};
        for (const auto& [location, field] : locations) {
          float celsius = 0.0f;
          if (sensors.temperature.get(location, celsius) == sl::ERROR_CODE::SUCCESS) {
            *field = celsius;
            any = true;
          }
        }
      }
      if (!any && !temperature_warned) {
        std::cerr << "  ! Temperatures not available (camera sensors not reachable): "
                     "recorded as null in meta.json"
                  << std::endl;
        temperature_warned = true;
      }
    };
    int64_t grab_timestamp_ns = 0;

    can_use_calib_prior = status != sl::ERROR_CODE::INVALID_CALIBRATION_FILE;

    if (can_use_calib_prior &&
        !calib.setFrom(zed_info.camera_configuration.calibration_parameters_raw)) {
      can_use_calib_prior = false;
      calib.initDefault(args.is_radtan_lens);
    }

    // Session mode: from scratch unless the session asks for the factory prior
    // (.ai/specs/saida-calibracao R1)
    if (use_session && session.calibration.prior != "factory" && can_use_calib_prior) {
      can_use_calib_prior = false;
      calib.initDefault(args.is_radtan_lens);
    }

    std::cout << " * Using prior calibration: "
              << (can_use_calib_prior ? "Yes" : "No") << std::endl;

    sl::Resolution camera_resolution = zed_info.camera_configuration.resolution;

    sl::Mat zed_imageL(camera_resolution, sl::MAT_TYPE::U8_C3, sl::MEM::CPU);
    auto rgb_l = cv::Mat(camera_resolution.height, camera_resolution.width,
                         CV_8UC3, zed_imageL.getPtr<sl::uchar1>());

    sl::Mat zed_imageR(camera_resolution, sl::MAT_TYPE::U8_C3, sl::MEM::CPU);
    auto rgb_r = cv::Mat(camera_resolution.height, camera_resolution.width,
                         CV_8UC3, zed_imageR.getPtr<sl::uchar1>());

    cv::Mat coverage_indicator =
        cv::Mat::zeros(display_size.height, display_size.width, CV_8UC1);

    cv::Mat pos_indicator =
        cv::Mat::zeros(display_size.height, display_size.width, CV_8UC1);

    cv::Mat limits_indicator =
        cv::Mat::zeros(display_size.height, display_size.width, CV_8UC1);

    cv::Mat rgb_d, rgb2_d, rgb_d_fill, rgb2_d_fill, display, rendering_image;

    bool acquisition_completed = false;
    std::vector<cv::Point3f> pts_obj_;
    for (int i = 0; i < v_edges; i++) {
      for (int j = 0; j < h_edges; j++) {
        pts_obj_.push_back(cv::Point3f(square_size * j, square_size * i, 0.0));
      }
    }

    if (use_session)
      detections_writer = std::make_unique<DetectionsWriter>(aprilgridDetectionsHeader(
          aprilgrid, cv::Size(camera_resolution.width, camera_resolution.height),
          args.target_yaml));

    // Check if the temp image folder exists and clear it (not in the session
    // mode, which never deletes anything)
    if (!use_session && fs::exists(image_folder)) {
      std::uintmax_t n{fs::remove_all(image_folder)};
      if (verbose) {
        std::cout
            << "[DEBUG][main] * Removed " << n
            << " temporary files or directories from previous calibration."
            << std::endl;
      }
    }
    // Create the temp image folder
    if (!use_session && !fs::create_directories(image_folder)) {
      std::cerr << "Error creating storage folder!";
      return 1;
    }

    auto computeSharpness = [](const cv::Mat& bgr) -> double {
      cv::Mat grey, lap;
      cv::cvtColor(bgr, grey, cv::COLOR_BGR2GRAY);
      cv::Laplacian(grey, lap, CV_64F);
      cv::Scalar mean, stddev;
      cv::meanStdDev(lap, mean, stddev);
      return stddev.val[0] * stddev.val[0];
    };

    char key = ' ';
    // bool coverage_mode = false;
    bool missing_target_on_last_pics = false;
    std::string missing_tags_reason;  // AprilGrid: why the last sample failed
    bool low_target_variability_on_last_pics = false;
    bool blurry_images = false;

    // Collection limits (.ai/specs/sample-limits)
    bool paused_at_max = false;   // maximum reached with incomplete metrics
    bool max_lifted = false;      // the user chose to continue past it
    std::string finish_message;   // why the last 'f' was refused
    std::string end_reason;       // how the collection ended, for the log

    const std::string window_name = "ZED Calibration";
    cv::namedWindow(window_name, cv::WINDOW_KEEPRATIO);
    cv::resizeWindow(window_name, display_size.width * 2,
                     display_size.height + text_area_height);

    std::unique_ptr<TimelapseSchedule> timelapse;
    auto steady_seconds = []() {
      return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
          .count();
    };
    if (use_timelapse) {
      timelapse = std::make_unique<TimelapseSchedule>(
          args.timelapse_interval_s, args.timelapse_duration_min, steady_seconds());
      std::cout << " * Timelapse: one pair every " << args.timelapse_interval_s << " s for "
                << args.timelapse_duration_min << " min (" << timelapse->expectedPairs()
                << " pairs), pose_id 'thermal'" << std::endl;
    }

    bool script_done = false;
    while (1) {
      if (key == 'q' || key == 'Q' || key == 27 || interruptRequested()) {
        std::cout << "Calibration aborted by user." << std::endl;
        end_meta("aborted by the user");
        settings_guard.restore();
        zed_camera.close();
        return EXIT_SUCCESS;
      }

      // End of the script (roteiro-poses R11): the summary, then the
      // calibration when there are enough pairs
      if (script && script->finished() && !script_done) {
        script_done = true;
        const ScriptRunner::Summary sum = script->summary();
        auto list = [](const std::vector<std::string>& ids) {
          std::string out;
          for (const auto& id : ids) out += (out.empty() ? "" : " ") + id;
          return out.empty() ? std::string("-") : out;
        };
        std::cout << ">>> Script complete: " << sum.saved.size() << " saved, "
                  << sum.out_of_tolerance.size() << " out of tolerance, " << sum.skipped.size()
                  << " skipped" << std::endl
                  << "    out of tolerance: " << list(sum.out_of_tolerance) << std::endl
                  << "    skipped: " << list(sum.skipped) << std::endl;
        const int count = std::max(image_count, 0);
        if (canFinishManually(count)) {
          end_reason = "script complete";
          acquisition_completed = true;
        } else {
          std::cout << "    " << count << " pairs: at least " << MIN_CALIBRATION_SAMPLES
                    << " are needed to calibrate; ending without calibration." << std::endl;
          end_meta("script complete, too few pairs to calibrate");
          settings_guard.restore();
          zed_camera.close();
          return EXIT_SUCCESS;
        }
      }

      const cv::Scalar info_color = cv::Scalar(50, 210, 50);
      const cv::Scalar warn_color = cv::Scalar(0, 50, 250);

      const sl::ERROR_CODE grabbed = zed_camera.grab();
      if (grabbed == sl::ERROR_CODE::END_OF_SVOFILE_REACHED) {
        // --svo: the file is over; calibrate with what was collected, if enough
        const int count = std::max(image_count, 0);
        if (canFinishManually(count)) {
          end_reason = "end of the SVO file";
          std::cout << ">>> End of the SVO file: " << count << " samples" << std::endl;
          std::cout << " *** Starting the calibration process ***" << std::endl;
          break;
        }
        std::cerr << "End of the SVO file with " << count << " samples: at least "
                  << MIN_CALIBRATION_SAMPLES << " are needed to calibrate." << std::endl;
        zed_camera.close();
        return EXIT_FAILURE;
      }
      if (grabbed == sl::ERROR_CODE::SUCCESS) {
        grab_timestamp_ns = static_cast<int64_t>(
            zed_camera.getTimestamp(sl::TIME_REFERENCE::IMAGE).getNanoseconds());
        zed_camera.retrieveImage(zed_imageL, sl::VIEW::LEFT_UNRECTIFIED_BGR);
        zed_camera.retrieveImage(zed_imageR, sl::VIEW::RIGHT_UNRECTIFIED_BGR);

        cv::resize(rgb_l, rgb_d, display_size);
        cv::resize(rgb_r, rgb2_d, display_size);
        cv::resize(rgb_l, rgb_d_fill, display_size);

        applyCoverageIndicatorOverlay(rgb_d_fill, coverage_indicator,
                                      limits_indicator);
        applyPosIndicatorOverlay(rgb_d_fill, pos_indicator);

        std::vector<cv::Point2f> pts_l, pts_r;
        bool found_l = false;
        bool found_r = false;

        // AprilGrid: tags detected on the full resolution images; a sample
        // needs enough tags seen by both cameras (decisions.md D7, D8)
        TagDetections tags_l, tags_r, common_tags_l;
        std::vector<cv::Point3f> tag_obj;
        std::vector<cv::Point2f> tag_pts_l, tag_pts_r;
        std::string tags_reason;
        bool enough_tags = false;
        ScriptFrame sf;  // script mode only

        if (use_aprilgrid) {
          tags_l = detectAprilGrid(rgb_l, aprilgrid.target,
                                   aprilgrid.detector_params);
          tags_r = detectAprilGrid(rgb_r, aprilgrid.target,
                                   aprilgrid.detector_params);
          auto ids = matchCommonTags(aprilgrid.target, tags_l, tags_r, tag_obj,
                                     tag_pts_l, tag_pts_r);
          for (int id : ids) common_tags_l[id] = tags_l[id];
          enough_tags = meetsMinimum(aprilgrid.target, ids, aprilgrid.min_tags,
                                     aprilgrid.min_rows, aprilgrid.min_cols,
                                     tags_reason);
          found_l = !tags_l.empty();
          found_r = !tags_r.empty();
          cv::Size full_size(camera_resolution.width, camera_resolution.height);
          drawAprilGridDetections(rgb_d_fill, tags_l, full_size);
          drawAprilGridDetections(rgb2_d, tags_r, full_size);

          if (script && !script->finished()) {
            // PnP on every left tag, with the guidance calibration (R6, R7)
            std::vector<cv::Point3f> obj;
            std::vector<cv::Point2f> img;
            for (const auto& [id, corners] : tags_l) {
              const auto o = tagCorners3D(aprilgrid.target, id);
              obj.insert(obj.end(), o.begin(), o.end());
              img.insert(img.end(), corners.begin(), corners.end());
            }
            cv::Vec3d rv, tv;
            if (obj.size() >= 8 &&
                estimateTargetPose(obj, img, pose_calib.K_left, pose_calib.D_left, rv, tv)) {
              const cv::Size2f board = aprilgrid.target.boardSizeMm();
              sf.view = viewGeometry(pose_calib.K_left, pose_calib.D_left, pose_calib.image_size,
                                     cv::Size2d(board.width, board.height), rv, tv);
              sf.check = checkPose(sf.view, script->current(), script->script().tolerances);
              sf.pose_known = true;
            }
            // Stillness and target sharpness (captura-estatica R1, R2)
            sf.still = stability->update(tags_l, tags_r, steady_seconds());
            sf.sharp_l = targetSharpness(rgb_l, tags_l);
            sf.sharp_r = targetSharpness(rgb_r, tags_r);
            const double min_sharp = session.static_capture.min_target_sharpness;
            sf.sharp = sf.sharp_l >= min_sharp && sf.sharp_r >= min_sharp;
            drawExpectedPose(rgb_d_fill, script->current());
            drawScriptPanel(rgb2_d, *script, sf, session.static_capture, script_message);
          }
        } else {
          found_l = cv::findChessboardCorners(rgb_d, cv::Size(h_edges, v_edges),
                                              pts_l);
          drawChessboardCorners(rgb_d_fill, cv::Size(h_edges, v_edges),
                                cv::Mat(pts_l), found_l);
          if (found_l) {
            found_r = cv::findChessboardCorners(
                rgb2_d, cv::Size(h_edges, v_edges), pts_r);
            drawChessboardCorners(rgb2_d, cv::Size(h_edges, v_edges),
                                  cv::Mat(pts_r), found_r);
          }
        }
        
        if (image_stack_horizontal) {
          cv::hconcat(rgb_d_fill, rgb2_d, display);
        } else {
          cv::vconcat(rgb_d_fill, rgb2_d, display);
        }

        cv::Mat text_info = cv::Mat::ones(
            cv::Size(display.size[1], text_area_height), display.type());
        cv::vconcat(display, text_info, rendering_image);

        if (acquisition_completed) {
          cv::putText(rendering_image,
                      "Acquisition completed! Wait for the calibration "
                      "computation to complete...",
                      cv::Point(20, display.size[0] + 50),
                      cv::FONT_HERSHEY_SIMPLEX, 0.7, info_color, 2);
          cv::putText(
              rendering_image,
              "Follow the console log for calibration progress details.",
              cv::Point(20, display.size[0] + 80), cv::FONT_HERSHEY_SIMPLEX,
              0.7, info_color, 2);
        } else {
          if (missing_target_on_last_pics ||
              low_target_variability_on_last_pics || blurry_images) {
            cv::putText(
                rendering_image, "Frames not saved for calibration.",
                cv::Point(display.size[1] / 2 - 20, display.size[0] + 285),
                cv::FONT_HERSHEY_SIMPLEX, 0.75, warn_color, 2);
          }

          if (missing_target_on_last_pics) {
            cv::putText(
                rendering_image,
                use_aprilgrid ? " * Not enough tags: " + missing_tags_reason
                              : " * Missing target on one of the cameras.",
                cv::Point(display.size[1] / 2 - 20, display.size[0] + 315),
                cv::FONT_HERSHEY_SIMPLEX, 0.75, warn_color, 2);
          }

          if (low_target_variability_on_last_pics) {
            cv::putText(
                rendering_image,
                " * Target too similar to a previous acquisition or too small.",
                cv::Point(display.size[1] / 2 - 20, display.size[0] + 345),
                cv::FONT_HERSHEY_SIMPLEX, 0.75, warn_color, 2);
          }

          if (blurry_images) {
            cv::putText(
                rendering_image,
                " * Images too blurry. Hold the target still when saving.",
                cv::Point(display.size[1] / 2 - 20, display.size[0] + 375),
                cv::FONT_HERSHEY_SIMPLEX, 0.75, warn_color, 2);
          }

          cv::putText(
              rendering_image,
              "Press 's'/space to save the frames when the target is visible "
              "in both images, 'f' to finish, 'q' to quit.",
              cv::Point(10, display.size[0] + 25), cv::FONT_HERSHEY_SIMPLEX,
              0.7, info_color, 1);

          cv::putText(
              rendering_image,
              "Move the target horizontally, vertically, forward "
              "and backward, and rotate it to improve "
              "coverage and variability scores. Framerate can be low if "
              "no target is detected.",
              cv::Point(10, display.size[0] + 45), cv::FONT_HERSHEY_SIMPLEX,
              0.5, warn_color, 1);

          // ----> Draw Status Info <---- //
          std::stringstream ss_status;
          int v_pos = display.size[0] + 80;
          int v_space = 33;
          int h_pos = 10;
          int h_space = 180;
          double font_scale = 0.7;

          auto draw_text_row = [rendering_image, h_pos, h_space, font_scale,
                                info_color,
                                warn_color](const std::string& label, int v_pos,
                                            int min_val, int max_val, int req_i,
                                            float req_f, float score) {
            // Label
            cv::putText(rendering_image, label, cv::Point(h_pos, v_pos),
                        cv::FONT_HERSHEY_SIMPLEX, font_scale,
                        (score >= 1.0f ? info_color : warn_color), 1);

            // Min Val
            cv::putText(rendering_image, std::to_string(min_val),
                        cv::Point(h_pos + h_space, v_pos),
                        cv::FONT_HERSHEY_SIMPLEX, font_scale,
                        (score >= 1.0f ? info_color : warn_color), 1);

            // Max Val
            cv::putText(rendering_image, std::to_string(max_val),
                        cv::Point(h_pos + 2 * h_space, v_pos),
                        cv::FONT_HERSHEY_SIMPLEX, font_scale,
                        (score >= 1.0f ? info_color : warn_color), 1);

            // Coverage
            cv::putText(rendering_image, std::to_string(max_val - min_val),
                        cv::Point(h_pos + 3 * h_space, v_pos),
                        cv::FONT_HERSHEY_SIMPLEX, font_scale,
                        (score >= 1.0f ? info_color : warn_color), 1);

            // Required
            cv::putText(rendering_image, std::to_string(req_i),
                        cv::Point(h_pos + 4 * h_space, v_pos),
                        cv::FONT_HERSHEY_SIMPLEX, font_scale,
                        (score >= 1.0f ? info_color : warn_color), 1);

            // Score
            std::stringstream ss;
            ss << std::fixed << std::setprecision(2) << score * 100.0f << "%";
            cv::putText(rendering_image, ss.str(),
                        cv::Point(h_pos + 5 * h_space, v_pos),
                        cv::FONT_HERSHEY_SIMPLEX, font_scale,
                        (score >= 1.0f ? info_color : warn_color), 1);
          };

          ss_status << "Sample Collection Status";
          cv::putText(rendering_image, ss_status.str(), cv::Point(10, v_pos),
                      cv::FONT_HERSHEY_SIMPLEX, font_scale, info_color, 2);

          ss_status.str("");
          v_pos += v_space;
          cv::putText(rendering_image, "METRIC", cv::Point(h_pos, v_pos),
                      cv::FONT_HERSHEY_SIMPLEX, font_scale, info_color, 2);
          cv::putText(rendering_image, "MIN_VAL",
                      cv::Point(h_pos + h_space, v_pos),
                      cv::FONT_HERSHEY_SIMPLEX, font_scale, info_color, 2);
          cv::putText(rendering_image, "MAX_VAL",
                      cv::Point(h_pos + 2 * h_space, v_pos),
                      cv::FONT_HERSHEY_SIMPLEX, font_scale, info_color, 2);
          cv::putText(rendering_image, "COVERAGE",
                      cv::Point(h_pos + 3 * h_space, v_pos),
                      cv::FONT_HERSHEY_SIMPLEX, font_scale, info_color, 2);
          cv::putText(rendering_image, "REQUIRED",
                      cv::Point(h_pos + 4 * h_space, v_pos),
                      cv::FONT_HERSHEY_SIMPLEX, font_scale, info_color, 2);
          cv::putText(rendering_image, "SCORE",
                      cv::Point(h_pos + 5 * h_space, v_pos),
                      cv::FONT_HERSHEY_SIMPLEX, font_scale, info_color, 2);

          // X
          v_pos += v_space;
          draw_text_row(
              "X [px]", v_pos,
              static_cast<int>(min_bx * camera_resolution.width),
              static_cast<int>(max_bx * camera_resolution.width),
              static_cast<int>(min_b_x_coverage * camera_resolution.width),
              min_b_x_coverage, pos_score_x);

          // Y
          v_pos += v_space;
          draw_text_row(
              "Y [px]", v_pos,
              static_cast<int>(min_by * camera_resolution.height),
              static_cast<int>(max_by * camera_resolution.height),
              static_cast<int>(min_b_y_coverage * camera_resolution.height),
              min_b_y_coverage, pos_score_y);

          // Size
          v_pos += v_space;
          draw_text_row(
              "Size [sq. px]", v_pos,
              static_cast<int>(min_size * camera_resolution.height *
                               camera_resolution.width),
              static_cast<int>(max_size * camera_resolution.height *
                               camera_resolution.width),
              static_cast<int>(min_area_range * camera_resolution.height *
                               camera_resolution.width),
              min_area_range, size_score);
          
          // Skew
          v_pos += v_space;
          draw_text_row("Skew [deg]", v_pos, static_cast<int>(min_skew * 90.0f),
                        static_cast<int>(max_skew * 90.0f),
                        static_cast<int>(min_skew_range * 90.0f),
                        static_cast<int>(min_skew_range * 90.0f), skew_score);

          std::stringstream ss_img_count;
          v_pos += v_space;
          ss_img_count << "* Sample saved: " << std::max(image_count, 0)
                       << " [min. " << min_samples << ", "
                       << (max_samples == 0
                               ? std::string("no max.")
                               : "max. " + std::to_string(max_samples))
                       << "]";
          cv::putText(rendering_image, ss_img_count.str(), cv::Point(10, v_pos),
                      cv::FONT_HERSHEY_SIMPLEX, 0.7,
                      (image_count > min_samples ? info_color : warn_color), 1);
        }

        if (paused_at_max) {
          drawBanner(rendering_image,
                     {"Maximum of " + std::to_string(max_samples) +
                          " samples reached. Incomplete: " +
                          incompleteMetrics(pos_score_x, pos_score_y,
                                            size_score, skew_score),
                      "'s'/space: continue collecting (no maximum)",
                      "'f': calibrate now    'q': abort"},
                     warn_color);
        } else if (!finish_message.empty() && !acquisition_completed) {
          drawBanner(rendering_image, {finish_message}, warn_color);
        }

        if (timelapse) {
          const double wait = std::max(0.0, timelapse->nextDue() - steady_seconds());
          drawBanner(rendering_image,
                     {"Timelapse: " + std::to_string(timelapse->takenPairs()) + "/" +
                          std::to_string(timelapse->expectedPairs()) + " pairs, next in " +
                          std::to_string(static_cast<int>(std::ceil(wait))) + " s",
                      "Keep the target still. 'q': abort"},
                     info_color);
        }
        cv::imshow(window_name, rendering_image);
        key = cv::waitKey(10);

        // --timelapse: the schedule replaces 's' and ends the record without
        // calibrating (.ai/specs/aquecimento-termico R1)
        bool timelapse_shot = false;
        if (timelapse) {
          const double now = steady_seconds();
          if (timelapse->finished(now)) {
            std::cout << ">>> Timelapse complete: " << std::max(image_count, 0) << " pairs saved, "
                      << timelapse->skippedSlots() << " slots skipped" << std::endl;
            end_meta("timelapse complete");
            settings_guard.restore();
            zed_camera.close();
            return EXIT_SUCCESS;
          }
          timelapse_shot = timelapse->due(now);
          if (timelapse_shot) timelapse->taken(now);
          if (key == 'f' || key == 'F' || key == 's' || key == 'S' || key == ' ') key = -1;
        }

        // Script mode keys (roteiro-poses R8, R10): 's'/space and 'o' go
        // through the save rule; 'k' skips, 'b' goes back
        bool script_shot = false;
        bool script_out = false;
        if (script && !script->finished() && !acquisition_completed) {
          const char k = static_cast<char>(std::tolower(key == ' ' ? 's' : key));
          if (k == 's' || k == 'o') {
            SaveConditions c;
            c.pose = sf.check;
            c.pose_known = sf.pose_known;
            c.enough_tags = enough_tags;
            c.still = sf.still.stable;
            c.sharp = sf.sharp;
            const SaveDecision d = decideScriptSave(k, c);
            script_shot = d.save;
            script_out = d.out_of_tolerance;
            script_message.clear();
            for (const auto& r : d.reasons)
              script_message += (script_message.empty() ? "" : "; ") + r;
            if (!d.save)
              std::cerr << "  ! " << script->current().id << " not saved: " << script_message
                        << std::endl;
          } else if (k == 'k') {
            capture_meta.script->events.push_back({script->current().id, "skipped", std::nullopt});
            std::cout << " * " << script->current().id << " skipped" << std::endl;
            script->skip();
            script_message.clear();
            write_meta();
          } else if (k == 'b') {
            if (script->back()) {
              capture_meta.script->events.push_back({script->current().id, "back", std::nullopt});
              std::cout << " * back to " << script->current().id << std::endl;
              script_message.clear();
              write_meta();
            }
          }
          if (k == 's' || k == 'o' || k == 'k' || k == 'b') key = -1;  // not the free mode keys
        }

        if (acquisition_completed) {
          const int count = std::max(image_count, 0);
          const auto pct = [](float score) {
            return std::to_string(
                       std::min(100, static_cast<int>(std::lround(score * 100.0f)))) +
                   "%";
          };
          std::cout << ">>> Collection ended: " << end_reason << " - " << count
                    << " samples - X " << pct(pos_score_x) << ", Y "
                    << pct(pos_score_y) << ", Size " << pct(size_score)
                    << ", Skew " << pct(skew_score) << std::endl;
          const std::string incomplete = incompleteMetrics(
              pos_score_x, pos_score_y, size_score, skew_score);
          if (!incomplete.empty())
            std::cout << "  ! Calibrating with incomplete metrics: " << incomplete
                      << std::endl;
          if (count < min_samples)
            std::cout << "  ! Calibrating with fewer samples than --min_samples ("
                      << count << "/" << min_samples << ")" << std::endl;
          std::cout << " *** Starting the calibration process ***" << std::endl;
          break;
        }

        // 'f': end the collection now, if the calibration can run
        if (key == 'f' || key == 'F') {
          const int count = std::max(image_count, 0);
          if (canFinishManually(count)) {
            end_reason = paused_at_max
                             ? "maximum reached, calibration chosen by the user"
                             : "ended by the user ('f')";
            paused_at_max = false;
            acquisition_completed = true;
          } else {
            finish_message =
                "Need at least " + std::to_string(MIN_CALIBRATION_SAMPLES) +
                " samples to calibrate: " +
                std::to_string(MIN_CALIBRATION_SAMPLES - count) + " more.";
            std::cout << "  ! " << finish_message << std::endl;
          }
          continue;
        }

        // While paused, 's'/space only resumes: the current frame is not
        // saved (.ai/specs/sample-limits/decisions.md D3)
        if (paused_at_max) {
          if ((key == 's' || key == 'S') || key == ' ') {
            paused_at_max = false;
            max_lifted = true;
            std::cout << " * Collection resumed, without maximum. Press 'f' "
                         "to calibrate."
                      << std::endl;
          }
          continue;
        }

        if ((key == 's' || key == 'S') || key == ' ' || timelapse_shot || script_shot) {
          finish_message.clear();
          std::cout << "*** New acquisition triggered ***" << std::endl;
          if (timelapse_shot && !enough_tags)
            std::cerr << "  ! Timelapse: target not seen by both cameras, pair "
                      << timelapse->takenPairs() << " not saved" << std::endl;

          missing_target_on_last_pics =
              use_aprilgrid ? !enough_tags : (!found_r || !found_l);
          missing_tags_reason = tags_reason;
          blurry_images = false;

          if (use_aprilgrid ? enough_tags : (found_l && found_r)) {
            const cv::Size full_size(camera_resolution.width,
                                     camera_resolution.height);
            auto scaled_pts_l = pts_l;
            std::vector<cv::Point2f> outside_pts, display_outside_pts;
            if (use_aprilgrid) {
              // Coverage metrics from the region covered by the common tags
              // (decisions.md D6)
              pts_l = tag_pts_l;
              outside_pts = outsideCorners(aprilgrid.target, common_tags_l);
              display_outside_pts = outside_pts;
              scaleKP(display_outside_pts, full_size, display_size);
            } else {
              scaleKP(pts_l, display_size, full_size);
              scaleKP(pts_r, display_size, full_size);
            }

            double sharpness_l = computeSharpness(rgb_l);
            double sharpness_r = computeSharpness(rgb_r);
            // the warm-up record keeps every pair: the analysis judges them
            // the warm-up record keeps every pair: the analysis judges them;
            // the script mode judges the target region instead (captura-estatica R2)
            blurry_images = !timelapse && !script &&
                            (sharpness_l < min_sharpness || sharpness_r < min_sharpness);
            if (blurry_images) {
              std::cerr << "  ! Images too blurry (L=" << std::fixed
                        << std::setprecision(1) << sharpness_l
                        << " R=" << sharpness_r
                        << ", min=" << min_sharpness << "). Hold still and retry."
                        << std::endl;
            } else if (timelapse || script ||  // no dissimilarity filter (roteiro-poses R9)
                       (use_aprilgrid ? checker.testSample(pts_l, outside_pts, full_size)
                                      : checker.testSample(pts_l, full_size))) {
              low_target_variability_on_last_pics = false;

              // saves the images
              if (image_count < 0) {
                image_count = 0;
              }
              if (use_session) {
                // Pair, then the detections of the saved files, read back as
                // the calibration reads them (persistencia-captura R4-R7)
                std::string rel_l, rel_r, io_error;
                const std::string pose_label =
                    script ? script->current().id : (timelapse ? "thermal" : "free");
                relativeImagePath("left", pose_label, image_count, rel_l, io_error);
                relativeImagePath("right", pose_label, image_count, rel_r, io_error);
                const std::string path_l = capture_dir + "/" + rel_l;
                const std::string path_r = capture_dir + "/" + rel_r;
                if (!cv::imwrite(path_l, rgb_l) || !cv::imwrite(path_r, rgb_r)) {
                  std::cerr << "Error: cannot write " << path_l << " or " << path_r
                            << std::endl;
                  settings_guard.restore();
                  zed_camera.close();
                  return EXIT_FAILURE;
                }
                TagDetections raw_l, raw_r;
                const TagDetections saved_l = detectAprilGrid(
                    cv::imread(path_l, cv::IMREAD_GRAYSCALE), aprilgrid.target,
                    aprilgrid.detector_params, &raw_l);
                const TagDetections saved_r = detectAprilGrid(
                    cv::imread(path_r, cv::IMREAD_GRAYSCALE), aprilgrid.target,
                    aprilgrid.detector_params, &raw_r);
                FrameDetections frame;
                frame.pose_id = pose_label;
                frame.n = image_count;
                frame.file_left = rel_l;
                frame.file_right = rel_r;
                frame.left = cameraDetectionsFromTags(saved_l, raw_l);
                frame.right = cameraDetectionsFromTags(saved_r, raw_r);
                detections_writer->addFrame(frame);
                if (!detections_writer->write(capture_dir + "/detections.json", io_error))
                  std::cerr << "  ! " << io_error << std::endl;
                session_images.emplace_back(path_l, path_r);
                session_calibration.view_ids.emplace_back(frame.pose_id, frame.n);
                FrameMeta frame_meta;
                frame_meta.pose_id = frame.pose_id;
                frame_meta.n = frame.n;
                frame_meta.timestamp_ns = grab_timestamp_ns;
                frame_meta.host_uptime_s = readHostUptime();
                frame_meta.since_camera_open_s =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                                  camera_opened_at)
                        .count();
                read_temperatures(frame_meta);
                if (script) {
                  // What the operator saw when saving (roteiro-poses R7, captura-estatica R6)
                  if (sf.pose_known) {
                    PoseMeasurement pm;
                    pm.fraction = sf.view.fraction;
                    pm.center_x = sf.view.center.x;
                    pm.center_y = sf.view.center.y;
                    pm.tilt_h_deg = sf.view.tilt_h_deg;
                    pm.tilt_v_deg = sf.view.tilt_v_deg;
                    pm.rotation_deg = sf.view.rotation_deg;
                    pm.distance_m = sf.view.distance_m;
                    pm.in_tolerance = sf.check.all();
                    frame_meta.pose = pm;
                  }
                  frame_meta.out_of_tolerance = script_out;
                  StillMeta still;
                  still.max_displacement_px = sf.still.max_displacement_px;
                  still.window_s = sf.still.stable_for_s;
                  still.sharpness_left = sf.sharp_l;
                  still.sharpness_right = sf.sharp_r;
                  still.threshold_displacement_px = session.static_capture.max_displacement_px;
                  still.threshold_window_s = session.static_capture.window_s;
                  still.threshold_sharpness = session.static_capture.min_target_sharpness;
                  frame_meta.still = still;
                  capture_meta.script->events.push_back(
                      {script->current().id, script_out ? "out_of_tolerance" : "saved", frame.n});
                  script->saved(frame.n, script_out);
                }
                capture_meta.frames.push_back(frame_meta);
                write_meta();
                std::cout << " * Images saved: '" << rel_l << "' and '" << rel_r
                          << "' (" << frame.left.tags.size() << " + "
                          << frame.right.tags.size() << " tags)" << std::endl;
              } else {
                cv::imwrite(image_folder + "image_left_" +
                                std::to_string(image_count) + ".png",
                            rgb_l);
                cv::imwrite(image_folder + "image_right_" +
                                std::to_string(image_count) + ".png",
                            rgb_r);
                std::cout << " * Images saved: '"
                          << image_folder + "image_left_" +
                                 std::to_string(image_count) + ".png' and '"
                          << image_folder + "image_right_" +
                                 std::to_string(image_count) + ".png'"
                          << std::endl;
              }
              image_count++;
              if (timelapse) continue;  // no coverage metrics nor maximum in a record
              if (script) continue;     // the script replaces them (roteiro-poses R9, R11)

              // The checker only reports complete metrics; the maximum is
              // handled here (.ai/specs/sample-limits/decisions.md D1)
              checker.evaluateSampleCollectionStatus(
                  size_score, skew_score, pos_score_x, pos_score_y, min_size,
                  max_size, min_skew, max_skew, min_bx, max_bx, min_by, max_by);
              const bool metrics_complete =
                  pos_score_x >= 1.0f && pos_score_y >= 1.0f &&
                  size_score >= 1.0f && skew_score >= 1.0f;
              switch (collectionStep(image_count, min_samples, max_samples,
                                     metrics_complete, max_lifted)) {
                case CollectionStep::COMPLETE:
                  std::cout << ">>> Sample collection status: COMPLETE <<<"
                            << std::endl
                            << std::endl;
                  end_reason = "all metrics complete";
                  acquisition_completed = true;
                  break;
                case CollectionStep::PAUSE_AT_MAX:
                  paused_at_max = true;
                  std::cout << "  ! Maximum of " << max_samples
                            << " samples reached with incomplete metrics: "
                            << incompleteMetrics(pos_score_x, pos_score_y,
                                                 size_score, skew_score)
                            << std::endl
                            << "    's'/space: continue collecting (no "
                               "maximum) | 'f': calibrate now | 'q': abort"
                            << std::endl;
                  break;
                case CollectionStep::CONTINUE:
                  break;
              }

              // Add the new checkerboard position to the coverage indicator
              float norm_x = checker.getLastDetectedBoardParams().avg_pos.x;
              float norm_y = checker.getLastDetectedBoardParams().avg_pos.y;
              float norm_size = checker.getLastDetectedBoardParams().size;
              addNewCheckerboardPosition(coverage_indicator, pos_indicator,
                                         limits_indicator, norm_x, norm_y,
                                         norm_size, min_bx, max_bx, min_by,
                                         max_by, (image_count >= 2));
              if (use_aprilgrid)
                addNewTargetPoly(coverage_indicator, display_outside_pts);
              else
                addNewCheckerboardPoly(coverage_indicator, scaled_pts_l);
            } else {
              std::cout << "  ! Checkerboard detected, but sample not valid. "
                           "Please try again "
                           "with a new position/orientation, not similar to "
                           "other acquisitions."
                        << std::endl;
              low_target_variability_on_last_pics = true;
            }
          } else if (use_aprilgrid) {
            std::cerr << "  ! Not enough AprilGrid tags: " << tags_reason
                      << std::endl;
          } else {
            if (!found_l) {
              std::cerr << "  ! Checkerboard not detected in the LEFT image."
                        << std::endl;
            } else if (!found_r) {
              std::cerr << "  ! Checkerboard not detected in the RIGHT image."
                        << std::endl;
            }
          }
        }
      }
    }

    // Settings at the end of the collection: they must match the start
    printCameraSettings(zed_camera, "Camera settings (end of collection)");
    end_meta(end_reason);

    // Stop camera acquisition. Ctrl+C gets its default behaviour back, so
    // that it can still interrupt the calibration computation
    settings_guard.restore();
    zed_camera.close();
    uninstallInterruptHandler();
  }

  // Detections of the stored images, with the detector the calibration uses
  if (!args.detections_out.empty()) {
    const auto pairs = legacyImagePairs(image_folder);
    std::unique_ptr<DetectionsWriter> writer;
    for (size_t i = 0; i < pairs.size(); i++) {
      const cv::Mat gray_l = cv::imread(pairs[i].first, cv::IMREAD_GRAYSCALE);
      const cv::Mat gray_r = cv::imread(pairs[i].second, cv::IMREAD_GRAYSCALE);
      if (gray_l.empty() || gray_r.empty()) continue;
      if (!writer)
        writer = std::make_unique<DetectionsWriter>(
            use_aprilgrid ? aprilgridDetectionsHeader(aprilgrid, gray_l.size(), args.target_yaml)
                          : checkerboardDetectionsHeader(cv::Size(h_edges, v_edges),
                                                         square_size, gray_l.size()));
      FrameDetections frame;
      frame.pose_id = "legacy";
      frame.n = static_cast<int>(i);
      frame.file_left = pairs[i].first;
      frame.file_right = pairs[i].second;
      if (use_aprilgrid) {
        TagDetections raw_l, raw_r;
        const TagDetections tags_l =
            detectAprilGrid(gray_l, aprilgrid.target, aprilgrid.detector_params, &raw_l);
        const TagDetections tags_r =
            detectAprilGrid(gray_r, aprilgrid.target, aprilgrid.detector_params, &raw_r);
        frame.left = cameraDetectionsFromTags(tags_l, raw_l);
        frame.right = cameraDetectionsFromTags(tags_r, raw_r);
      } else {
        std::vector<cv::Point2f> corners;
        if (detectCheckerboard(gray_l, cv::Size(h_edges, v_edges), corners))
          for (const auto& c : corners) frame.left.corners.emplace_back(c.x, c.y);
        if (detectCheckerboard(gray_r, cv::Size(h_edges, v_edges), corners))
          for (const auto& c : corners) frame.right.corners.emplace_back(c.x, c.y);
      }
      writer->addFrame(frame);
    }
    std::string io_error;
    if (!writer) {
      std::cerr << "Error: no stored image pairs in '" << image_folder << "'" << std::endl;
      return EXIT_FAILURE;
    }
    if (!writer->write(args.detections_out, io_error)) {
      std::cerr << "Error: " << io_error << std::endl;
      return EXIT_FAILURE;
    }
    std::cout << " * Detections of " << writer->frameCount() << " stored pairs written to "
              << args.detections_out << std::endl;
  }

  // Start the calibration process
  int err =
      calibrate(image_count, image_folder, calib, h_edges, v_edges, square_size,
                zed_info.serial_number, is_dual_mono_camera, is_4k_camera,
                false, can_use_calib_prior, max_repr_error, verbose,
                use_aprilgrid ? &aprilgrid : nullptr,
                use_session ? &session_images : nullptr, capture_dir,
                use_session ? &session_calibration : nullptr);
  if (err == EXIT_SUCCESS)
    std::cout << std::endl
              << " +++++ Calibration successful +++++" << std::endl;
  else
    std::cout << std::endl << " ----- Calibration failed -----" << std::endl;

  return EXIT_SUCCESS;
}

static int top_left_count = 0;
static int top_right_count = 0;
static int bottom_left_count = 0;
static int bottom_right_count = 0;

void addNewCheckerboardPosition(cv::Mat& coverage_indicator,
                                cv::Mat& pos_indicator,
                                cv::Mat& limits_indicator, float norm_x,
                                float norm_y, float norm_size, float min_x,
                                float max_x, float min_y, float max_y,
                                bool draw_rect) {
  // Checkerbaord position
  int x = static_cast<int>(norm_x * pos_indicator.cols);
  int y = static_cast<int>(norm_y * pos_indicator.rows);
  int size = static_cast<int>(norm_size * 30.0f);
  cv::circle(pos_indicator, cv::Point(x, y), size, cv::Scalar(255, 255, 255),
             -1);

  int min_x_px = static_cast<int>(min_x * pos_indicator.cols);
  int max_x_px = static_cast<int>(max_x * pos_indicator.cols);
  int min_y_px = static_cast<int>(min_y * pos_indicator.rows);
  int max_y_px = static_cast<int>(max_y * pos_indicator.rows);

  limits_indicator.setTo(cv::Scalar(0, 0, 0));

  if (draw_rect) {
    int col_val = 50;
    cv::rectangle(limits_indicator, cv::Point(0, 0),
                  cv::Point(min_x_px, limits_indicator.rows - 1),
                  cv::Scalar(col_val, col_val, col_val), -1);
    cv::rectangle(limits_indicator, cv::Point(0, 0),
                  cv::Point(limits_indicator.cols - 1, min_y_px),
                  cv::Scalar(col_val, col_val, col_val), -1);
    cv::rectangle(
        limits_indicator, cv::Point(0, max_y_px),
        cv::Point(limits_indicator.cols - 1, limits_indicator.rows - 1),
        cv::Scalar(col_val, col_val, col_val), -1);
    cv::rectangle(
        limits_indicator, cv::Point(max_x_px, 0),
        cv::Point(limits_indicator.cols - 1, limits_indicator.rows - 1),
        cv::Scalar(col_val, col_val, col_val), -1);
  }
  cv::line(limits_indicator, cv::Point(min_x * limits_indicator.cols, 0),
           cv::Point(min_x * limits_indicator.cols, limits_indicator.rows - 1),
           cv::Scalar(255, 255, 255), 2);
  cv::line(limits_indicator, cv::Point(max_x * limits_indicator.cols, 0),
           cv::Point(max_x * limits_indicator.cols, limits_indicator.rows - 1),
           cv::Scalar(255, 255, 255), 2);
  cv::line(limits_indicator, cv::Point(0, min_y * limits_indicator.rows),
           cv::Point(limits_indicator.cols - 1, min_y * limits_indicator.rows),
           cv::Scalar(255, 255, 255), 2);
  cv::line(limits_indicator, cv::Point(0, max_y * limits_indicator.rows),
           cv::Point(limits_indicator.cols - 1, max_y * limits_indicator.rows),
           cv::Scalar(255, 255, 255), 2);

  if (norm_x < 0.5f && norm_y < 0.5f) {
    top_left_count++;
  } else if (norm_x >= 0.5f && norm_y < 0.5f) {
    top_right_count++;
  } else if (norm_x < 0.5f && norm_y >= 0.5f) {
    bottom_left_count++;
  } else {
    bottom_right_count++;
  }

  if (top_left_count >= min_samples / 4) {
    cv::rectangle(
        coverage_indicator, cv::Point(0, 0),
        cv::Point(coverage_indicator.cols / 2, coverage_indicator.rows / 2),
        cv::Scalar(255), -1);
  }
  if (top_right_count >= min_samples / 4) {
    cv::rectangle(
        coverage_indicator, cv::Point(coverage_indicator.cols / 2, 0),
        cv::Point(coverage_indicator.cols, coverage_indicator.rows / 2),
        cv::Scalar(255), -1);
  }
  if (bottom_left_count >= min_samples / 4) {
    cv::rectangle(
        coverage_indicator, cv::Point(0, coverage_indicator.rows / 2),
        cv::Point(coverage_indicator.cols / 2, coverage_indicator.rows),
        cv::Scalar(255), -1);
  }
  if (bottom_right_count >= min_samples / 4) {
    cv::rectangle(
        coverage_indicator,
        cv::Point(coverage_indicator.cols / 2, coverage_indicator.rows / 2),
        cv::Point(coverage_indicator.cols, coverage_indicator.rows),
        cv::Scalar(255), -1);
  }
}

void addNewCheckerboardPoly(cv::Mat& coverage_indicator,
                            const std::vector<cv::Point2f>& pts_l) {
  std::vector<cv::Point2f> outside_corners = {
      pts_l[0], pts_l[h_edges - 1], pts_l[pts_l.size() - 1],
      pts_l[pts_l.size() - h_edges]};
  addNewTargetPoly(coverage_indicator, outside_corners);
}

void addNewTargetPoly(cv::Mat& coverage_indicator,
                      const std::vector<cv::Point2f>& outside_corners) {
  std::vector<cv::Point> poly_pts;
  for (const auto& corner : outside_corners) poly_pts.push_back(corner);

  cv::Mat mask = cv::Mat::zeros(coverage_indicator.size(), CV_8UC1);
  cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{poly_pts},
               cv::Scalar(255 / 5, 255 / 5, 255 / 5));

  coverage_indicator = coverage_indicator + mask;
}

void applyCoverageIndicatorOverlay(cv::Mat& image,
                                   const cv::Mat& coverage_indicator,
                                   const cv::Mat& limits_indicator) {
  std::vector<cv::Mat> channels;
  cv::split(image, channels);
  channels[0] = channels[0] - coverage_indicator;
  channels[2] = channels[2] - coverage_indicator;
  channels[0] = channels[0] - limits_indicator;
  channels[1] = channels[1] - limits_indicator;
  cv::merge(channels, image);
}

void applyPosIndicatorOverlay(cv::Mat& image, const cv::Mat& pos_indicator) {
  std::vector<cv::Mat> channels;
  cv::split(image, channels);
  channels[2] = channels[2] - pos_indicator;
  channels[1] = channels[1] - pos_indicator;
  cv::merge(channels, image);
}

void drawBanner(cv::Mat& image, const std::vector<std::string>& lines,
                const cv::Scalar& color) {
  const int line_height = 34;
  const int height = line_height * static_cast<int>(lines.size()) + 20;
  const cv::Rect box(20, 20, image.cols - 40, height);
  cv::Mat roi = image(box);
  cv::Mat dark(roi.size(), roi.type(), cv::Scalar(0, 0, 0));
  cv::addWeighted(roi, 0.35, dark, 0.65, 0.0, roi);
  for (size_t i = 0; i < lines.size(); i++) {
    cv::putText(image, lines[i],
                cv::Point(box.x + 15, box.y + 30 + static_cast<int>(i) * line_height),
                cv::FONT_HERSHEY_SIMPLEX, 0.8, color, 2);
  }
}

std::string describePose(const ScriptPose& pose) {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(2) << "width " << pose.fraction_min << "-"
     << pose.fraction_max << ", centre (" << pose.center.x << ", " << pose.center.y << ")"
     << std::setprecision(0);
  if (pose.tilt_h_deg != 0.0) ss << ", tilt H " << std::showpos << pose.tilt_h_deg << std::noshowpos;
  if (pose.tilt_v_deg != 0.0) ss << ", tilt V " << std::showpos << pose.tilt_v_deg << std::noshowpos;
  if (pose.rotation_deg != 0.0) ss << ", rotation " << pose.rotation_deg;
  return ss.str();
}

// The expected target, as the square it would make facing the camera, on the
// left display image (roteiro-poses R5)
void drawExpectedPose(cv::Mat& image, const ScriptPose& pose) {
  const double w = 0.5 * (pose.fraction_min + pose.fraction_max) * image.cols;
  const cv::Point2d c(pose.center.x * image.cols, pose.center.y * image.rows);
  const cv::Rect box(cvRound(c.x - w / 2), cvRound(c.y - w / 2), cvRound(w), cvRound(w));
  cv::rectangle(image, box, cv::Scalar(0, 220, 255), 2);
  cv::drawMarker(image, cv::Point(cvRound(c.x), cvRound(c.y)), cv::Scalar(0, 220, 255),
                 cv::MARKER_CROSS, 18, 2);
}

// Expected pose, measured quantities (green inside the tolerance, red outside),
// stillness and target sharpness, on the right display image
void drawScriptPanel(cv::Mat& image, const ScriptRunner& script, const ScriptFrame& f,
                     const StaticCaptureConfig& still_cfg, const std::string& message) {
  const int line = 22;
  const cv::Rect box(0, 0, image.cols, line * 6 + 12);
  cv::Mat roi = image(box);
  cv::Mat dark(roi.size(), roi.type(), cv::Scalar(0, 0, 0));
  cv::addWeighted(roi, 0.3, dark, 0.7, 0.0, roi);
  const cv::Scalar white(240, 240, 240), green(60, 220, 60), red(60, 60, 240);
  const double scale = 0.5;
  int y = line;
  auto text = [&](const std::string& t, int x, const cv::Scalar& color) {
    cv::putText(image, t, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, scale, color, 1);
    int base = 0;
    return x + cv::getTextSize(t, cv::FONT_HERSHEY_SIMPLEX, scale, 1, &base).width + 12;
  };
  const ScriptPose& p = script.current();
  text("Pose " + std::to_string(script.index() + 1) + "/" + std::to_string(script.size()) + "  " +
           p.id,
       8, white);
  y += line;
  text(describePose(p), 8, white);
  y += line;
  if (!f.pose_known) {
    text("pose: not available (too few tags)", 8, red);
  } else {
    auto num = [](double v, int prec, bool sign = false) {
      std::ostringstream o;
      o << std::fixed << std::setprecision(prec) << (sign ? std::showpos : std::noshowpos) << v;
      return o.str();
    };
    int x = 8;
    x = text("width " + num(f.view.fraction, 2), x, f.check.fraction ? green : red);
    x = text("centre (" + num(f.view.center.x, 2) + ", " + num(f.view.center.y, 2) + ")", x,
             f.check.position ? green : red);
    x = text("tilt H " + num(f.view.tilt_h_deg, 0, true), x, f.check.tilt_h ? green : red);
    x = text("V " + num(f.view.tilt_v_deg, 0, true), x, f.check.tilt_v ? green : red);
    x = text("rot " + num(f.view.rotation_deg, 0), x, f.check.rotation ? green : red);
    text(num(f.view.distance_m, 2) + " m", x, white);
  }
  y += line;
  {
    std::ostringstream st;
    st << std::fixed << std::setprecision(1);
    if (f.still.indeterminate)
      st << "stillness: indeterminate (few common corners)";
    else if (f.still.stable)
      st << "still for " << f.still.stable_for_s << " s";
    else
      st << "moving (" << std::setprecision(2) << f.still.displacement_px << " px; still for "
         << std::setprecision(1) << f.still.stable_for_s << "/" << still_cfg.window_s << " s)";
    int x = text(st.str(), 8, f.still.stable ? green : red);
    std::ostringstream sh;
    sh << std::fixed << std::setprecision(0) << "sharpness L " << f.sharp_l << " R " << f.sharp_r
       << " (min " << still_cfg.min_target_sharpness << ")";
    text(sh.str(), x, f.sharp ? green : red);
  }
  y += line;
  text("s save   o save out of tolerance   k skip   b back   f calibrate   q abort", 8, white);
  y += line;
  if (!message.empty()) text("not saved: " + message, 8, red);
}
