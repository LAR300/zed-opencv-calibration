#include "session.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

#include <opencv2/opencv.hpp>

namespace {

constexpr int MAX_CAPTURE = 99;  // two digits in captura_XX

// cv::FileStorage 4.5.4 reads null, ~, true and false as plain strings
bool isNull(const cv::FileNode& node) {
  if (node.isNone()) return true;
  if (!node.isString()) return false;
  const std::string text = node.string();
  return text == "null" || text == "~";
}

// Reads the fields of one section, recording the first error. The section
// name prefixes every field name in the messages ("port.tilt_deg").
class Reader {
 public:
  Reader(const cv::FileNode& node, std::string section, std::string& error)
      : node_(node), section_(std::move(section)), error_(error) {}

  bool failed() const { return !error_.empty(); }

  std::string name(const std::string& key) const {
    return section_.empty() ? key : section_ + "." + key;
  }

  // The node of a mandatory field, or an empty node after setting the error
  cv::FileNode field(const std::string& key) {
    if (failed()) return cv::FileNode();
    cv::FileNode child = node_[key];
    if (child.empty() && child.isNone() && !hasKey(key)) {
      error_ = "Missing field '" + name(key) + "'";
      return cv::FileNode();
    }
    return child;
  }

  Reader section(const std::string& key) {
    cv::FileNode child = field(key);
    if (!failed() && !child.isMap())
      error_ = "Field '" + name(key) + "' must be a section with its own fields";
    return Reader(child, name(key), error_);
  }

  std::string text(const std::string& key, bool allow_empty) {
    cv::FileNode child = field(key);
    if (failed()) return "";
    if (isNull(child) || !child.isString()) {
      error_ = "Field '" + name(key) + "' must be a text";
      return "";
    }
    const std::string value = child.string();
    if (!allow_empty && value.empty()) error_ = "Field '" + name(key) + "' must not be empty";
    return value;
  }

  std::optional<std::string> optionalText(const std::string& key) {
    cv::FileNode child = field(key);
    if (failed() || isNull(child)) return std::nullopt;
    if (!child.isString()) {
      error_ = "Field '" + name(key) + "' must be a text or null";
      return std::nullopt;
    }
    return child.string();
  }

  // A number or null; 'min' and 'strict' bound the accepted values
  std::optional<double> optionalNumber(const std::string& key,
                                       std::optional<double> min = std::nullopt,
                                       bool strict = false) {
    cv::FileNode child = field(key);
    if (failed() || isNull(child)) return std::nullopt;
    if (!child.isInt() && !child.isReal()) {
      error_ = "Field '" + name(key) + "' must be a number or null";
      return std::nullopt;
    }
    const double value = child.real();
    if (min && (strict ? value <= *min : value < *min)) {
      std::ostringstream ss;
      ss << "Field '" << name(key) << "' (" << value << ") must be "
         << (strict ? "greater than " : "at least ") << *min;
      error_ = ss.str();
      return std::nullopt;
    }
    return value;
  }

  bool flag(const std::string& key) {
    cv::FileNode child = field(key);
    if (failed()) return false;
    if (child.isInt()) {
      const int value = static_cast<int>(child);
      if (value == 0 || value == 1) return value == 1;
    } else if (child.isString()) {
      const std::string value = child.string();
      if (value == "true" || value == "false") return value == "true";
    }
    error_ = "Field '" + name(key) + "' must be true or false";
    return false;
  }

  int integer(const std::string& key) {
    cv::FileNode child = field(key);
    if (failed()) return 0;
    if (!child.isInt()) {
      error_ = "Field '" + name(key) + "' must be an integer";
      return 0;
    }
    return static_cast<int>(child);
  }

  void fail(const std::string& message) {
    if (!failed()) error_ = message;
  }

 private:
  bool hasKey(const std::string& key) const {
    if (!node_.isMap()) return false;
    for (const auto& child : node_)
      if (child.name() == key) return true;
    return false;
  }

  cv::FileNode node_;
  std::string section_;
  std::string& error_;
};

// cv::FileStorage 4.5.4 keeps a trailing '# comment' as part of an unquoted
// text value ("A_ar   # note"). Comments are removed here: a '#' at the start
// of a line or after a space, outside quotes, starts a comment.
std::string stripComments(const std::string& yaml) {
  std::istringstream in(yaml);
  std::ostringstream out;
  std::string line;
  while (std::getline(in, line)) {
    char quote = 0;
    size_t cut = std::string::npos;
    for (size_t i = 0; i < line.size(); i++) {
      const char c = line[i];
      if (quote) {
        if (c == quote) quote = 0;
      } else if (c == '"' || c == '\'') {
        quote = c;
      } else if (c == '#' && (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t')) {
        cut = i;
        break;
      }
    }
    std::string kept = line.substr(0, cut);
    while (!kept.empty() && (kept.back() == ' ' || kept.back() == '\t')) kept.pop_back();
    out << kept << "\n";
  }
  return out.str();
}

}  // namespace

bool loadSession(const std::string& path, Session& session, std::string& error) {
  session = Session();
  error.clear();

  std::ifstream file(path);
  if (!file.is_open()) {
    error = "Cannot open the session file '" + path + "'";
    return false;
  }
  std::stringstream content;
  content << file.rdbuf();
  std::string yaml = stripComments(content.str());
  // cv::FileStorage requires the '%YAML' directive
  if (yaml.rfind("%YAML", 0) != 0) yaml = "%YAML:1.0\n---\n" + yaml;

  cv::FileStorage fs;
  try {
    fs.open(yaml, cv::FileStorage::READ | cv::FileStorage::MEMORY);
  } catch (const cv::Exception& e) {
    error = "Cannot parse the session file '" + path + "': " + e.what() +
            " (texts that start with a digit, such as dates and the lens, must be "
            "quoted: measured_on: \"2026-11-03\", lens: \"2.1mm\")";
    return false;
  }
  if (!fs.isOpened()) {
    error = "Cannot parse the session file '" + path + "'";
    return false;
  }

  Reader root(fs.root(), "", error);
  session.output_root = root.text("output_root", false);

  session.condition = root.text("condition", false);
  if (!root.failed() &&
      !std::regex_match(session.condition, std::regex("[A-Za-z0-9_-]+")))
    root.fail("Field 'condition' ('" + session.condition +
              "') may only contain letters, digits, '_' and '-'");

  session.capture = root.integer("capture");
  if (!root.failed() && (session.capture < 1 || session.capture > MAX_CAPTURE))
    root.fail("Field 'capture' (" + std::to_string(session.capture) +
              ") must be between 1 and " + std::to_string(MAX_CAPTURE));

  session.lens = root.text("lens", false);
  session.note = root.text("note", true);

  Reader target = root.section("target_measured");
  auto& t = session.target_measured;
  t.tag_size_mm = target.optionalNumber("tag_size_mm", 0.0, true);
  t.tag_spacing_mm = target.optionalNumber("tag_spacing_mm", 0.0, true);
  t.planarity_mm = target.optionalNumber("planarity_mm", 0.0);
  t.measured_on = target.optionalText("measured_on");
  if (error.empty()) {
    const cv::FileNode tn = fs["target_measured"];
    auto list = [&](const char* key, std::vector<double>& out, bool positive) {
      const cv::FileNode n = tn[key];
      if (!error.empty() || n.empty() || isNull(n)) return;
      const std::string name = std::string("target_measured.") + key;
      if (!n.isSeq()) {
        error = "Field '" + name + "' must be a list of numbers or null";
        return;
      }
      for (const auto& v : n) {
        if (!v.isInt() && !v.isReal()) {
          error = "Field '" + name + "' must be a list of numbers or null";
          return;
        }
        if (positive && v.real() <= 0.0) {
          error = "Field '" + name + "' must hold values greater than 0";
          return;
        }
        out.push_back(v.real());
      }
    };
    list("tag_sides_mm", t.tag_sides_mm, true);
    list("spacings_mm", t.spacings_mm, true);
    list("planarity_grid_mm", t.planarity_grid_mm, false);
    const cv::FileNode side = tn["board_side_mm"];
    if (error.empty() && !side.empty() && !isNull(side)) {
      if ((!side.isInt() && !side.isReal()) || side.real() <= 0.0)
        error = "Field 'target_measured.board_side_mm' must be a number greater than 0 or null";
      else
        t.board_side_mm = side.real();
    }
  }

  Reader port = root.section("port");
  auto& p = session.port;
  p.present = port.flag("present");
  p.material = port.optionalText("material");
  p.thickness_mm = port.optionalNumber("thickness_mm", 0.0, true);
  p.lens_to_port_mm = port.optionalNumber("lens_to_port_mm", 0.0);
  p.tilt_deg = port.optionalNumber("tilt_deg");
  p.refractive_index = port.optionalNumber("refractive_index", 1.0);

  Reader medium = root.section("medium");
  auto& m = session.medium;
  m.type = medium.text("type", false);
  if (!medium.failed() && m.type != "air" && m.type != "fresh_water" &&
      m.type != "sea_water")
    medium.fail("Field 'medium.type' ('" + m.type +
                "') must be air, fresh_water or sea_water");
  m.water_temperature_c = medium.optionalNumber("water_temperature_c");
  m.salinity_psu = medium.optionalNumber("salinity_psu", 0.0);
  m.refractive_index = medium.optionalNumber("refractive_index", 1.0);

  // Optional section; each field keeps its default when absent
  cv::FileNode calib = fs["calibration"];
  if (error.empty() && !calib.empty() && !calib.isNone()) {
    auto& c = session.calibration;
    auto number = [&](const char* key, double& value, double min, bool strict) {
      cv::FileNode n = calib[key];
      if (!error.empty() || n.empty() || n.isNone()) return;
      if (!n.isInt() && !n.isReal()) {
        error = std::string("Field 'calibration.") + key + "' must be a number";
        return;
      }
      const double v = n.real();
      if (strict ? v <= min : v < min) {
        std::ostringstream ss;
        ss << "Field 'calibration." << key << "' (" << v << ") must be "
           << (strict ? "greater than " : "at least ") << min;
        error = ss.str();
        return;
      }
      value = v;
    };
    auto choice = [&](const char* key, std::string& value,
                      std::initializer_list<const char*> allowed) {
      cv::FileNode n = calib[key];
      if (!error.empty() || n.empty() || n.isNone()) return;
      const std::string v = n.isString() ? n.string() : "";
      for (const char* a : allowed)
        if (v == a) {
          value = v;
          return;
        }
      std::string list;
      for (const char* a : allowed) list += (list.empty() ? "" : ", ") + std::string(a);
      error = std::string("Field 'calibration.") + key + "' ('" + v + "') must be one of: " +
              list;
    };
    if (!calib.isMap()) error = "Field 'calibration' must be a section with its own fields";
    choice("model", c.model, {"rational8", "fisheye4"});
    number("max_rms_px", c.max_rms_px, 0.0, true);
    number("outlier_factor", c.outlier_factor, 1.0, true);
    number("outlier_floor_px", c.outlier_floor_px, 0.0, true);
    choice("prior", c.prior, {"none", "factory"});
  }

  cv::FileNode still = fs["static_capture"];
  if (error.empty() && !still.empty() && !still.isNone()) {
    auto& c = session.static_capture;
    auto number = [&](const char* key, double& value, double min, bool strict) {
      cv::FileNode n = still[key];
      if (!error.empty() || n.empty() || n.isNone()) return;
      const double v = (n.isInt() || n.isReal()) ? n.real() : -1e300;
      if (v == -1e300 || (strict ? v <= min : v < min)) {
        std::ostringstream ss;
        ss << "Field 'static_capture." << key << "' must be a number "
           << (strict ? "greater than " : "at least ") << min;
        error = ss.str();
        return;
      }
      value = v;
    };
    if (!still.isMap()) error = "Field 'static_capture' must be a section with its own fields";
    number("window_s", c.window_s, 0.0, true);
    number("max_displacement_px", c.max_displacement_px, 0.0, true);
    number("min_target_sharpness", c.min_target_sharpness, 0.0, false);
  }

  if (!error.empty()) {
    error += " in '" + path + "'";
    session = Session();
    return false;
  }
  return true;
}

bool checkMetricTargetMeasured(const TargetMeasured& t, std::string& error) {
  auto fail = [&](const std::string& message) {
    error = "Metric test session: " + message;
    return false;
  };
  if (!t.tag_size_mm || !t.tag_spacing_mm)
    return fail("target_measured.tag_size_mm and tag_spacing_mm are required");
  if (t.tag_sides_mm.size() < 6)
    return fail("target_measured.tag_sides_mm needs at least 6 tags (edges and centre), has " +
                std::to_string(t.tag_sides_mm.size()));
  if (t.spacings_mm.size() < 6)
    return fail("target_measured.spacings_mm needs at least 6 pairs of tags, has " +
                std::to_string(t.spacings_mm.size()));
  if (!t.board_side_mm) return fail("target_measured.board_side_mm is required");
  if (t.planarity_grid_mm.size() != 9)
    return fail("target_measured.planarity_grid_mm needs the 9 points of a 3x3 grid, has " +
                std::to_string(t.planarity_grid_mm.size()));
  if (!t.planarity_mm) return fail("target_measured.planarity_mm is required");
  if (!t.measured_on) return fail("target_measured.measured_on is required");
  return true;
}

std::string captureDirName(const Session& session) {
  char name[32];
  std::snprintf(name, sizeof(name), "captura_%02d", session.capture);
  return name;
}

std::string captureDir(const Session& session) {
  return (std::filesystem::path(session.output_root) / session.condition /
          captureDirName(session))
      .string();
}
