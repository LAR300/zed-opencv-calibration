#include "meta_io.hpp"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <opencv2/core/persistence.hpp>

namespace {

// cv::FileStorage cannot write JSON null, true or false. These markers are
// written as texts and replaced by the literals after encoding.
const std::string kNull = "__ZEDCAL_NULL__";
const std::string kTrue = "__ZEDCAL_TRUE__";
const std::string kFalse = "__ZEDCAL_FALSE__";

void writeOptional(cv::FileStorage& fs, const char* key, const std::optional<double>& v) {
  if (v)
    fs << key << *v;
  else
    fs << key << kNull;
}

void writeOptionalText(cv::FileStorage& fs, const char* key,
                       const std::optional<std::string>& v) {
  if (v)
    fs << key << *v;
  else
    fs << key << kNull;
}

void writeBool(cv::FileStorage& fs, const char* key, bool v) {
  fs << key << (v ? kTrue : kFalse);
}

void writeSettings(cv::FileStorage& fs, const char* key,
                   const std::vector<std::pair<std::string, int>>& settings) {
  fs << key << "{";
  for (const auto& [name, value] : settings) fs << name << value;
  fs << "}";
}

void writeSession(cv::FileStorage& fs, const Session& s) {
  fs << "session" << "{";
  fs << "output_root" << s.output_root << "condition" << s.condition << "capture"
     << s.capture << "lens" << s.lens << "note" << s.note;
  fs << "target_measured" << "{";
  writeOptional(fs, "tag_size_mm", s.target_measured.tag_size_mm);
  writeOptional(fs, "tag_spacing_mm", s.target_measured.tag_spacing_mm);
  writeOptional(fs, "planarity_mm", s.target_measured.planarity_mm);
  writeOptionalText(fs, "measured_on", s.target_measured.measured_on);
  fs << "}";
  fs << "port" << "{";
  writeBool(fs, "present", s.port.present);
  writeOptionalText(fs, "material", s.port.material);
  writeOptional(fs, "thickness_mm", s.port.thickness_mm);
  writeOptional(fs, "lens_to_port_mm", s.port.lens_to_port_mm);
  writeOptional(fs, "tilt_deg", s.port.tilt_deg);
  writeOptional(fs, "refractive_index", s.port.refractive_index);
  fs << "}";
  fs << "medium" << "{" << "type" << s.medium.type;
  writeOptional(fs, "water_temperature_c", s.medium.water_temperature_c);
  writeOptional(fs, "salinity_psu", s.medium.salinity_psu);
  writeOptional(fs, "refractive_index", s.medium.refractive_index);
  fs << "}" << "}";
}

void replaceAll(std::string& text, const std::string& from, const std::string& to) {
  for (size_t pos = text.find(from); pos != std::string::npos;
       pos = text.find(from, pos + to.size()))
    text.replace(pos, from.size(), to);
}

// The texts that come from the user must not contain a marker
bool usesMarker(const std::string& text) {
  return text.find("__ZEDCAL_") != std::string::npos;
}

bool anyTextUsesMarker(const CaptureMeta& m) {
  const Session& s = m.session;
  for (const std::string* t :
       {&m.camera.model, &m.camera.lens, &m.sdk_version, &m.code_commit, &m.hostname,
        &s.output_root, &s.condition, &s.lens, &s.note, &s.medium.type, &m.end_reason})
    if (usesMarker(*t)) return true;
  if (s.target_measured.measured_on && usesMarker(*s.target_measured.measured_on)) return true;
  if (s.port.material && usesMarker(*s.port.material)) return true;
  for (const auto& f : m.frames)
    if (usesMarker(f.pose_id)) return true;
  return false;
}

}  // namespace

bool writeCaptureMeta(const std::string& path, const CaptureMeta& m, std::string& error) {
  error.clear();
  if (anyTextUsesMarker(m)) {
    error = "A text in the metadata uses the reserved prefix '__ZEDCAL_'";
    return false;
  }

  std::string text;
  try {
    cv::FileStorage fs(".json", cv::FileStorage::WRITE | cv::FileStorage::MEMORY |
                                    cv::FileStorage::FORMAT_JSON);
    fs << "schema_version" << META_SCHEMA_VERSION;
    fs << "camera" << "{" << "model" << m.camera.model
       // Serial and firmware fit an int; they are written as integers
       << "serial" << static_cast<int>(m.camera.serial) << "firmware"
       << static_cast<int>(m.camera.firmware);
    fs << "resolution" << "[" << m.camera.resolution.width << m.camera.resolution.height
       << "]";
    fs << "fps" << m.camera.fps << "lens" << m.camera.lens << "}";
    fs << "sdk_version" << m.sdk_version;
    fs << "code" << "{" << "commit" << m.code_commit;
    writeBool(fs, "dirty", m.code_dirty);
    fs << "}";
    fs << "host" << "{" << "hostname" << m.hostname << "}";
    writeBool(fs, "self_calibration_disabled", m.self_calibration_disabled);
    if (m.has_session)
      writeSession(fs, m.session);
    else
      fs << "session" << kNull;
    fs << "camera_settings" << "{";
    writeSettings(fs, "start", m.camera_settings_start);
    writeSettings(fs, "end", m.camera_settings_end);
    fs << "}";
    fs << "started_utc" << m.started_utc << "ended_utc" << m.ended_utc << "end_reason"
       << m.end_reason;

    fs << "frames" << "[";
    for (const auto& f : m.frames) {
      fs << "{" << "pose_id" << f.pose_id << "n" << f.n;
      // A nanosecond timestamp does not fit a double without losing digits
      fs << "timestamp_ns" << std::to_string(f.timestamp_ns);
      writeOptional(fs, "host_uptime_s", f.host_uptime_s);
      writeOptional(fs, "since_camera_open_s", f.since_camera_open_s);
      fs << "temperature_c" << "{";
      writeOptional(fs, "onboard_left", f.temperature_onboard_left);
      writeOptional(fs, "onboard_right", f.temperature_onboard_right);
      writeOptional(fs, "imu", f.temperature_imu);
      fs << "}" << "}";
    }
    fs << "]";
    text = fs.releaseAndGetString();
  } catch (const cv::Exception& e) {
    error = std::string("Cannot encode the metadata: ") + e.what();
    return false;
  }

  replaceAll(text, "\"" + kNull + "\"", "null");
  replaceAll(text, "\"" + kTrue + "\"", "true");
  replaceAll(text, "\"" + kFalse + "\"", "false");

  const std::string tmp = path + ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    out << text;
    if (!out.good()) {
      error = "Cannot write '" + tmp + "'";
      return false;
    }
  }
  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    error = "Cannot rename '" + tmp + "' to '" + path + "': " + ec.message();
    return false;
  }
  return true;
}

std::string utcTimestamp(std::chrono::system_clock::time_point time) {
  const std::time_t t = std::chrono::system_clock::to_time_t(time);
  std::tm utc{};
  gmtime_r(&t, &utc);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return buffer;
}

std::string utcNow() { return utcTimestamp(std::chrono::system_clock::now()); }

std::optional<double> parseProcUptime(const std::string& text) {
  std::istringstream in(text);
  double seconds = 0.0;
  if (!(in >> seconds)) return std::nullopt;
  return seconds;
}

std::optional<double> readHostUptime() {
  std::ifstream file("/proc/uptime");
  if (!file) return std::nullopt;
  std::stringstream ss;
  ss << file.rdbuf();
  return parseProcUptime(ss.str());
}
