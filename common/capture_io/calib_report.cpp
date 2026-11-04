#include "calib_report.hpp"

#include "json_write.hpp"

namespace {

void writeRms(cv::FileStorage& fs, const RmsSet& r, bool squared) {
  const std::pair<const char*, const std::optional<double>*> fields[] = {
      {"stereo", &r.stereo},
      {"stereo_fit_left", &r.stereo_fit_left},
      {"stereo_fit_right", &r.stereo_fit_right},
      {"free_pose_left", &r.free_pose_left},
      {"free_pose_right", &r.free_pose_right},
      {"mono_stage_left", &r.mono_stage_left},
      {"mono_stage_right", &r.mono_stage_right}};
  for (const auto& [name, value] : fields) {
    std::optional<double> v = *value;
    if (v && squared) v = (*v) * (*v) / 2.0;
    json_write::optional(fs, name, v);
  }
}

void writeList(cv::FileStorage& fs, const char* key, const std::vector<double>& values) {
  fs << key << "[";
  for (double v : values) fs << v;
  fs << "]";
}

}  // namespace

bool writeCalibrationReport(const std::string& path, const CalibrationReport& r,
                            std::string& error) {
  error.clear();
  for (const auto& v : r.views)
    if (json_write::usesMarker(v.pose_id)) {
      error = "A pose id uses the reserved prefix '__ZEDCAL_'";
      return false;
    }
  std::string text;
  try {
    cv::FileStorage fs(".json", cv::FileStorage::WRITE | cv::FileStorage::MEMORY |
                                    cv::FileStorage::FORMAT_JSON);
    fs << "schema_version" << CALIB_REPORT_SCHEMA_VERSION;
    fs << "calibration_file" << r.calibration_file << "model" << r.model;
    fs << "prior" << r.prior;
    fs << "criteria" << "{" << "max_rms_px" << r.max_rms_px << "outlier_factor"
       << r.outlier_factor << "outlier_floor_px" << r.outlier_floor_px << "}";
    fs << "flags" << "{" << "mono" << r.mono_flags << "stereo" << r.stereo_flags
       << "term_criteria" << r.term_criteria << "}";
    fs << "image_size" << "[" << r.image_size.width << r.image_size.height << "]";
    fs << "pairs_input" << r.pairs_input << "pairs_used" << r.pairs_used;

    fs << "rms_convention" << "per_corner_2d";
    fs << "rms_px" << "{";
    writeRms(fs, r.rms, false);
    fs << "}";
    fs << "mse_per_coordinate_px2" << "{";
    writeRms(fs, r.rms, true);
    fs << "}";
    json_write::boolean(fs, "accepted", r.accepted);

    fs << "std_dev_mono_last" << "{" << "names" << "[";
    for (const auto& n : r.std_dev_names) fs << n;
    fs << "]";
    writeList(fs, "left", r.std_dev_left);
    writeList(fs, "right", r.std_dev_right);
    fs << "}";
    // OpenCV 4.5.4 gives no standard deviations for the stereo fit; the offline
    // analysis estimates them by bootstrap (spec nucleo-offline)
    json_write::null(fs, "std_dev_stereo");

    fs << "code" << "{" << "commit" << r.code_commit;
    json_write::boolean(fs, "dirty", r.code_dirty);
    fs << "}";

    fs << "views" << "[";
    for (const auto& v : r.views) {
      fs << "{" << "pose_id" << v.pose_id << "n" << v.n << "corners_left" << v.corners_left
         << "corners_right" << v.corners_right;
      json_write::optional(fs, "stereo_error_left", v.stereo_error_left);
      json_write::optional(fs, "stereo_error_right", v.stereo_error_right);
      if (v.removed_stage.empty()) {
        json_write::null(fs, "removed");
      } else {
        fs << "removed" << "{" << "stage" << v.removed_stage;
        json_write::optional(fs, "error", v.removed_error);
        fs << "}";
      }
      fs << "}";
    }
    fs << "]";
    text = fs.releaseAndGetString();
  } catch (const cv::Exception& e) {
    error = std::string("Cannot encode the calibration report: ") + e.what();
    return false;
  }
  return json_write::finishJson(text, path, error);
}
