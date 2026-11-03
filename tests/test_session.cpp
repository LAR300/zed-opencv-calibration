#include <filesystem>
#include <fstream>
#include <sstream>

#include "session.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

namespace {

// A complete session file, one field per line, so that a test can drop or
// replace a single field
const std::vector<std::string> kBaseLines = {
    "output_root: /tmp/dados",
    "condition: A_ar_sem_acrilico",
    "capture: 1",
    "lens: \"2.1mm\"",
    "note: \"\"",
    "target_measured:",
    "  tag_size_mm: 88.0",
    "  tag_spacing_mm: 26.4",
    "  planarity_mm: null",
    "  measured_on: \"2026-11-03\"",
    "port:",
    "  present: false",
    "  material: null",
    "  thickness_mm: null",
    "  lens_to_port_mm: null",
    "  tilt_deg: null",
    "  refractive_index: null",
    "medium:",
    "  type: air",
    "  water_temperature_c: null",
    "  salinity_psu: null",
    "  refractive_index: null",
};

std::string join(const std::vector<std::string>& lines) {
  std::ostringstream out;
  for (const auto& line : lines) out << line << "\n";
  return out.str();
}

// Base file with the line starting with 'prefix' replaced (or removed when
// 'replacement' is empty)
std::string withLine(const std::string& prefix, const std::string& replacement) {
  std::vector<std::string> lines;
  for (const auto& line : kBaseLines) {
    if (line.rfind(prefix, 0) == 0) {
      if (!replacement.empty()) lines.push_back(replacement);
    } else {
      lines.push_back(line);
    }
  }
  return join(lines);
}

bool load(const std::string& content, Session& session, std::string& error) {
  static int n = 0;
  fs::path path = fs::temp_directory_path() /
                  ("session_test_" + std::to_string(n++) + ".yaml");
  std::ofstream(path) << content;
  return loadSession(path.string(), session, error);
}

bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

}  // namespace

TEST(session_reads_complete_file) {
  Session s;
  std::string error;
  CHECK(load(join(kBaseLines), s, error));
  CHECK(error.empty());
  CHECK(s.output_root == "/tmp/dados");
  CHECK(s.condition == "A_ar_sem_acrilico");
  CHECK(s.capture == 1);
  CHECK(s.lens == "2.1mm");
  CHECK(s.note.empty());
  CHECK(s.target_measured.tag_size_mm.has_value());
  CHECK_NEAR(*s.target_measured.tag_size_mm, 88.0, 1e-12);
  CHECK_NEAR(*s.target_measured.tag_spacing_mm, 26.4, 1e-12);
  CHECK(!s.target_measured.planarity_mm.has_value());
  CHECK(s.target_measured.measured_on == "2026-11-03");
  CHECK(!s.port.present);
  CHECK(!s.port.material.has_value());
  CHECK(!s.port.thickness_mm.has_value());
  CHECK(s.medium.type == "air");
  CHECK(!s.medium.water_temperature_c.has_value());
}

TEST(session_example_file_is_accepted) {
  Session s;
  std::string error;
  CHECK(loadSession(std::string(ZEDCAL_SOURCE_DIR) + "/config/session_example.yaml",
                    s, error));
  if (!error.empty()) std::cerr << "  " << error << std::endl;
}

TEST(session_accepts_opencv_header) {
  Session s;
  std::string error;
  CHECK(load("%YAML:1.0\n---\n" + join(kBaseLines), s, error));
}

TEST(session_rejects_missing_file) {
  Session s;
  std::string error;
  CHECK(!loadSession("/nonexistent/session.yaml", s, error));
  CHECK(contains(error, "Cannot open"));
}

TEST(session_rejects_each_missing_field) {
  // Every field is mandatory: unknown values are 'null', never omitted
  const std::vector<std::pair<std::string, std::string>> fields = {
      {"output_root:", "output_root"},
      {"condition:", "condition"},
      {"capture:", "capture"},
      {"lens:", "lens"},
      {"note:", "note"},
      {"  tag_size_mm:", "target_measured.tag_size_mm"},
      {"  tag_spacing_mm:", "target_measured.tag_spacing_mm"},
      {"  planarity_mm:", "target_measured.planarity_mm"},
      {"  measured_on:", "target_measured.measured_on"},
      {"  present:", "port.present"},
      {"  material:", "port.material"},
      {"  thickness_mm:", "port.thickness_mm"},
      {"  lens_to_port_mm:", "port.lens_to_port_mm"},
      {"  tilt_deg:", "port.tilt_deg"},
      {"  type:", "medium.type"},
      {"  water_temperature_c:", "medium.water_temperature_c"},
      {"  salinity_psu:", "medium.salinity_psu"},
  };
  for (const auto& [prefix, name] : fields) {
    Session s;
    std::string error;
    const bool ok = load(withLine(prefix, ""), s, error);
    CHECK(!ok);
    CHECK(contains(error, "Missing field '" + name + "'"));
    if (ok || !contains(error, name))
      std::cerr << "  missing '" << name << "': " << error << std::endl;
  }
}

TEST(session_rejects_missing_sections) {
  // A section without its children: drop the 'port:' block entirely
  std::vector<std::string> lines;
  bool in_port = false;
  for (const auto& line : kBaseLines) {
    if (line == "port:") in_port = true;
    else if (line.rfind("  ", 0) != 0) in_port = false;
    if (!in_port) lines.push_back(line);
  }
  Session s;
  std::string error;
  CHECK(!load(join(lines), s, error));
  CHECK(contains(error, "port"));
}

TEST(session_rejects_invalid_condition) {
  const char* bad[] = {"condition: \"A ar\"", "condition: A/B", "condition: \"\"",
                       "condition: null"};
  for (const char* line : bad) {
    Session s;
    std::string error;
    CHECK(!load(withLine("condition:", line), s, error));
    CHECK(contains(error, "condition"));
  }
  Session s;
  std::string error;
  CHECK(load(withLine("condition:", "condition: metrico_A-2"), s, error));
}

TEST(session_rejects_invalid_capture) {
  const char* bad[] = {"capture: 0", "capture: -1", "capture: 100", "capture: 1.5",
                       "capture: abc", "capture: null"};
  for (const char* line : bad) {
    Session s;
    std::string error;
    CHECK(!load(withLine("capture:", line), s, error));
    CHECK(contains(error, "capture"));
  }
  Session s;
  std::string error;
  CHECK(load(withLine("capture:", "capture: 99"), s, error));
  CHECK(s.capture == 99);
}

TEST(session_rejects_empty_lens) {
  Session s;
  std::string error;
  CHECK(!load(withLine("lens:", "lens: \"\""), s, error));
  CHECK(contains(error, "lens"));
}

TEST(session_rejects_invalid_medium_type) {
  Session s;
  std::string error;
  CHECK(!load(withLine("  type:", "  type: vacuum"), s, error));
  CHECK(contains(error, "medium.type"));
  for (const char* type : {"air", "fresh_water", "sea_water"}) {
    Session ok;
    CHECK(load(withLine("  type:", std::string("  type: ") + type), ok, error));
    CHECK(ok.medium.type == type);
  }
}

TEST(session_reads_null_in_both_spellings) {
  Session s;
  std::string error;
  CHECK(load(withLine("  tilt_deg:", "  tilt_deg: ~"), s, error));
  CHECK(!s.port.tilt_deg.has_value());
}

TEST(session_reads_port_values) {
  std::string content = join(kBaseLines);
  auto replace = [&](const std::string& from, const std::string& to) {
    content.replace(content.find(from), from.size(), to);
  };
  replace("  present: false", "  present: true");
  replace("  material: null", "  material: PMMA");
  replace("  thickness_mm: null", "  thickness_mm: 10.0");
  replace("  lens_to_port_mm: null", "  lens_to_port_mm: 5.5");
  replace("  tilt_deg: null", "  tilt_deg: -0.4");
  replace("port:\n  present: true\n  material: PMMA\n  thickness_mm: 10.0\n"
          "  lens_to_port_mm: 5.5\n  tilt_deg: -0.4\n  refractive_index: null",
          "port:\n  present: true\n  material: PMMA\n  thickness_mm: 10.0\n"
          "  lens_to_port_mm: 5.5\n  tilt_deg: -0.4\n  refractive_index: 1.4905");
  Session s;
  std::string error;
  CHECK(load(content, s, error));
  if (!error.empty()) std::cerr << "  " << error << std::endl;
  CHECK(s.port.present);
  CHECK(s.port.material.has_value() && *s.port.material == "PMMA");
  CHECK_NEAR(*s.port.thickness_mm, 10.0, 1e-12);
  CHECK_NEAR(*s.port.lens_to_port_mm, 5.5, 1e-12);
  CHECK_NEAR(*s.port.tilt_deg, -0.4, 1e-12);
  CHECK_NEAR(*s.port.refractive_index, 1.4905, 1e-12);
}

TEST(session_accepts_integer_numbers) {
  Session s;
  std::string error;
  CHECK(load(withLine("  tag_size_mm:", "  tag_size_mm: 88"), s, error));
  CHECK_NEAR(*s.target_measured.tag_size_mm, 88.0, 1e-12);
}

TEST(session_rejects_invalid_numbers) {
  const std::vector<std::pair<std::string, std::string>> bad = {
      {"  tag_size_mm:", "  tag_size_mm: 0"},
      {"  tag_size_mm:", "  tag_size_mm: abc"},
      {"  tag_spacing_mm:", "  tag_spacing_mm: -26.4"},
      {"  planarity_mm:", "  planarity_mm: -0.1"},
      {"  thickness_mm:", "  thickness_mm: 0"},
      {"  lens_to_port_mm:", "  lens_to_port_mm: -1"},
      {"  salinity_psu:", "  salinity_psu: -1"},
  };
  for (const auto& [prefix, line] : bad) {
    Session s;
    std::string error;
    CHECK(!load(withLine(prefix, line), s, error));
    if (error.empty()) std::cerr << "  accepted: " << line << std::endl;
  }
  // refractive_index appears twice (port and medium); both must be >= 1
  std::string content = join(kBaseLines);
  const std::string from = "  refractive_index: null\nmedium:";
  content.replace(content.find(from), from.size(),
                  "  refractive_index: 0.9\nmedium:");
  Session s;
  std::string error;
  CHECK(!load(content, s, error));
  CHECK(contains(error, "port.refractive_index"));
}

TEST(session_rejects_invalid_bool) {
  Session s;
  std::string error;
  CHECK(!load(withLine("  present:", "  present: maybe"), s, error));
  CHECK(contains(error, "port.present"));
  for (const char* v : {"true", "false", "1", "0"}) {
    Session ok;
    CHECK(load(withLine("  present:", std::string("  present: ") + v), ok, error));
  }
}

TEST(session_explains_unquoted_values) {
  // cv::FileStorage cannot parse an unquoted date such as 2026-11-03
  Session s;
  std::string error;
  CHECK(!load(withLine("  measured_on:", "  measured_on: 2026-11-03"), s, error));
  CHECK(contains(error, "quote"));
  // Same for a text that starts with a digit, such as the lens
  CHECK(!load(withLine("lens:", "lens: 2.1mm"), s, error));
  CHECK(contains(error, "quote"));
}

TEST(session_captures_dir_name_has_two_digits) {
  Session s;
  s.output_root = "/data";
  s.condition = "A";
  s.capture = 1;
  CHECK(captureDirName(s) == "captura_01");
  s.capture = 12;
  CHECK(captureDirName(s) == "captura_12");
  CHECK(captureDir(s) == "/data/A/captura_12");
}

TEST(session_ignores_trailing_comments) {
  Session s;
  std::string error;
  std::string content = withLine("condition:", "condition: A_ar   # comment");
  content = "# header comment\n" + content;
  const std::string from = "note: \"\"";
  content.replace(content.find(from), from.size(), "note: \"a # b\"  # real comment");
  CHECK(load(content, s, error));
  if (!error.empty()) std::cerr << "  " << error << std::endl;
  CHECK(s.condition == "A_ar");
  CHECK(s.note == "a # b");  // a '#' inside quotes is kept
}
