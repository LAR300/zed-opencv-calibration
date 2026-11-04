#pragma once

// Session file of the traceable capture mode (--session): what is captured,
// where it is stored and under which conditions. OpenCV only (no ZED SDK) so
// it can be unit tested without a camera.
// Spec: .ai/specs/persistencia-captura/spec.md (R2, R3; section Formatos)

#include <optional>
#include <string>

// Target dimensions measured with a caliper before the session (brief R7)
struct TargetMeasured {
  std::optional<double> tag_size_mm;
  std::optional<double> tag_spacing_mm;
  std::optional<double> planarity_mm;  // largest deviation from a plane
  std::optional<std::string> measured_on;
};

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
};

// Read and validate a session file. Every field must be present; unknown
// values are written 'null' (or '~'), never omitted. On failure 'error' names
// the field, as 'section.field'.
bool loadSession(const std::string& path, Session& session, std::string& error);

// "captura_XX" and <output_root>/<condition>/captura_XX
std::string captureDirName(const Session& session);
std::string captureDir(const Session& session);
