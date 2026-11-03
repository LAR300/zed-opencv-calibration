#pragma once

// meta.json: what was captured, with what, when and under which conditions.
// One file per capture, with one entry per saved image pair.
// Spec: .ai/specs/persistencia-captura/spec.md (R8, R9; section Formatos)

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/core.hpp>

#include "session.hpp"

constexpr int META_SCHEMA_VERSION = 1;

struct CameraMeta {
  std::string model;
  long long serial = 0;
  long long firmware = 0;
  cv::Size resolution;
  int fps = 0;
  std::string lens;
};

struct FrameMeta {
  std::string pose_id;
  int n = 0;
  int64_t timestamp_ns = 0;  // image timestamp from the SDK
  std::optional<double> host_uptime_s;
  std::optional<double> since_camera_open_s;
  std::optional<double> temperature_onboard_left;
  std::optional<double> temperature_onboard_right;
  std::optional<double> temperature_imu;
};

struct CaptureMeta {
  CameraMeta camera;
  std::string sdk_version;
  std::string code_commit;
  bool code_dirty = false;
  std::string hostname;
  bool self_calibration_disabled = true;
  Session session;
  std::vector<std::pair<std::string, int>> camera_settings_start;
  std::vector<std::pair<std::string, int>> camera_settings_end;
  std::string started_utc;
  std::string ended_utc;
  std::string end_reason;
  std::vector<FrameMeta> frames;
};

// Write the file through '<path>.tmp' and a rename, like detections.json.
// Missing optional values are written as JSON null.
bool writeCaptureMeta(const std::string& path, const CaptureMeta& meta,
                      std::string& error);

// "2026-11-03T14:00:00Z"
std::string utcTimestamp(std::chrono::system_clock::time_point time);
std::string utcNow();

// First number of /proc/uptime: seconds since the host booted
std::optional<double> parseProcUptime(const std::string& text);
std::optional<double> readHostUptime();
