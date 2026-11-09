// Bench check (persistencia-captura, plan step 7): detect the saved
// PNGs again and compare with detections.json.
// Usage: check_detections <capture_dir> <target.yaml>
#include <cmath>
#include <iostream>
#include <set>
#include "aprilgrid.hpp"
#include "detections_io.hpp"
int main(int argc, char** argv) {
  if (argc < 3) { std::cerr << "usage: check_detections <capture_dir> <target.yaml>\n"; return 2; }
  const std::string dir = argv[1];
  AprilGridOptions opt; std::string error;
  if (!loadKalibrTarget(argv[2], opt.target, error)) { std::cerr << error << "\n"; return 1; }
  DetectionsHeader header; std::vector<FrameDetections> frames;
  if (!readDetections(dir + "/detections.json", header, frames, error)) { std::cerr << error << "\n"; return 1; }
  double max_diff = 0.0; int only_one = 0, tags = 0, mismatched_ids = 0;
  for (const auto& f : frames) {
    std::set<int> ids_l, ids_r;
    for (int side = 0; side < 2; side++) {
      const auto& cam = side == 0 ? f.left : f.right;
      const std::string file = dir + "/" + (side == 0 ? f.file_left : f.file_right);
      TagDetections raw;
      TagDetections fresh = detectAprilGrid(cv::imread(file, cv::IMREAD_GRAYSCALE), opt.target, opt.detector_params, &raw);
      if (fresh.size() != cam.tags.size()) mismatched_ids++;
      for (const auto& t : cam.tags) {
        (side == 0 ? ids_l : ids_r).insert(t.id); tags++;
        auto it = fresh.find(t.id); auto rit = raw.find(t.id);
        if (it == fresh.end() || rit == raw.end()) { mismatched_ids++; continue; }
        for (int k = 0; k < 4; k++) {
          max_diff = std::max({max_diff, std::abs(t.corners[k].x - it->second[k].x), std::abs(t.corners[k].y - it->second[k].y),
                               std::abs(t.corners_unrefined[k].x - rit->second[k].x), std::abs(t.corners_unrefined[k].y - rit->second[k].y)});
        }
      }
    }
    for (int id : ids_l) if (!ids_r.count(id)) only_one++;
    for (int id : ids_r) if (!ids_l.count(id)) only_one++;
    std::cout << f.pose_id << "_" << f.n << ": left " << ids_l.size() << " tags, right " << ids_r.size() << " tags\n";
  }
  std::cout << "frames " << frames.size() << ", tags " << tags << ", tags seen by one camera only " << only_one
            << ", id mismatches " << mismatched_ids << ", max |corner diff| " << max_diff << " px\n";
  return (mismatched_ids == 0 && max_diff < 1e-6) ? 0 : 1;
}
