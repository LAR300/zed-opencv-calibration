#pragma once

// Session file of the traceable capture mode (--session): what is captured,
// where it is stored and under which conditions. OpenCV only (no ZED SDK) so
// it can be unit tested without a camera.
// Spec: .ai/specs/persistencia-captura/spec.md (R2, R3; section Formatos)

#include <optional>
#include <string>
#include <vector>

// Target dimensions measured with a caliper before the session (brief R7).
// The lists and board_side_mm are optional in the file (absent = empty/null);
// the metric test requires them (checkMetricTargetMeasured).
struct TargetMeasured {
  std::optional<double> tag_size_mm;
  std::optional<double> tag_spacing_mm;
  std::optional<double> planarity_mm;  // largest deviation from a plane
  std::optional<std::string> measured_on;
  std::vector<double> tag_sides_mm;       // individual tag sides (edges and centre)
  std::vector<double> spacings_mm;        // gaps between pairs of neighbouring tags
  std::optional<double> board_side_mm;    // whole grid, outer tag edge to outer tag edge
  std::vector<double> planarity_grid_mm;  // signed deviations at a 3x3 grid, row-major
};

// Metric test session (spec validacao-metrica R4): at least 6 tag sides and 6
// spacings, the board side, the 3x3 planarity grid and the summary values.
bool checkMetricTargetMeasured(const TargetMeasured& t, std::string& error);

// Flat port in front of the camera (brief R8); all null in air without a port
struct PortGeometry {
  bool present = false;
  std::optional<std::string> material;
  std::optional<double> thickness_mm;
  std::optional<double> lens_to_port_mm;
  std::optional<double> tilt_deg;
  std::optional<double> refractive_index;
};

// Medium between the port and the target (brief R8)
struct Medium {
  std::string type;  // air | fresh_water | sea_water
  std::optional<double> water_temperature_c;
  std::optional<double> salinity_psu;
  std::optional<double> refractive_index;
};

// How zed_stereo_calibration calibrates a session capture. Optional section:
// the defaults reproduce the tool without --session.
// Spec: .ai/specs/saida-calibracao/spec.md (R1, R2)
struct CalibrationConfig {
  std::string model = "rational8";  // rational8 | fisheye4
  double max_rms_px = 0.5;          // acceptance, per-corner RMS
  double outlier_factor = 2.5;      // per-view rejection: max(factor * median, floor)
  double outlier_floor_px = 0.5;
  std::string prior = "none";       // none | factory
};

// Static capture thresholds (spec captura-estatica, R3). Optional section. The
// defaults are provisional: the bench measures the static noise with
// --measure_static_noise and the session takes its values from noise.json.
struct StaticCaptureConfig {
  double window_s = 1.0;               // "~1 s" of the brief
  double max_displacement_px = 0.5;    // provisional
  double min_target_sharpness = 0.0;   // 0: not checked until measured
};

struct Session {
  std::string output_root;
  std::string condition;  // [A-Za-z0-9_-]+
  int capture = 0;        // 1..99 -> captura_01..captura_99
  std::string lens;
  std::string note;
  TargetMeasured target_measured;
  PortGeometry port;
  Medium medium;
  CalibrationConfig calibration;
  StaticCaptureConfig static_capture;
  // Script mode (.ai/specs/roteiro-poses R5, R7); optional keys, absent = free
  // capture. Relative paths are taken from the session file's folder.
  std::optional<std::string> pose_script;       // key 'roteiro'
  std::optional<std::string> pose_calibration;  // PnP guidance; null = the exported F
};

// Read and validate a session file. Every field must be present; unknown
// values are written 'null' (or '~'), never omitted. On failure 'error' names
// the field, as 'section.field'.
bool loadSession(const std::string& path, Session& session, std::string& error);

// "captura_XX" and <output_root>/<condition>/captura_XX
std::string captureDirName(const Session& session);
std::string captureDir(const Session& session);
