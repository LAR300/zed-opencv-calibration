#pragma once

// Helpers to write JSON with cv::FileStorage, which cannot write null, true or
// false: markers are written as texts and finishJson() replaces them by the
// JSON literals. Shared by meta.json and the calibration report.

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <opencv2/core/persistence.hpp>

namespace json_write {

inline const std::string kNull = "__ZEDCAL_NULL__";
inline const std::string kTrue = "__ZEDCAL_TRUE__";
inline const std::string kFalse = "__ZEDCAL_FALSE__";

// User texts must not contain the marker prefix
inline bool usesMarker(const std::string& text) {
  return text.find("__ZEDCAL_") != std::string::npos;
}

inline void optional(cv::FileStorage& fs, const char* key, const std::optional<double>& v) {
  if (v)
    fs << key << *v;
  else
    fs << key << kNull;
}

inline void optionalText(cv::FileStorage& fs, const char* key,
                         const std::optional<std::string>& v) {
  if (v)
    fs << key << *v;
  else
    fs << key << kNull;
}

inline void boolean(cv::FileStorage& fs, const char* key, bool v) {
  fs << key << (v ? kTrue : kFalse);
}

inline void null(cv::FileStorage& fs, const char* key) { fs << key << kNull; }

inline void replaceAll(std::string& text, const std::string& from, const std::string& to) {
  for (size_t pos = text.find(from); pos != std::string::npos;
       pos = text.find(from, pos + to.size()))
    text.replace(pos, from.size(), to);
}

// FileStorage escapes ' as \' inside strings, which is not valid JSON. Every
// backslash it writes starts a two-character escape (\\, \", \', \n, ...), so
// a scan that copies escapes in pairs drops only the backslash of \'.
inline std::string unescapeSingleQuotes(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\\' && i + 1 < text.size()) {
      if (text[i + 1] != '\'') out += '\\';
      out += text[++i];
    } else {
      out += text[i];
    }
  }
  return out;
}

// Replace the markers and write the text through '<path>.tmp' and a rename
inline bool finishJson(std::string text, const std::string& path, std::string& error) {
  text = unescapeSingleQuotes(text);
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

}  // namespace json_write
