// Bench helper: print every VIDEO_SETTINGS value and range of the connected ZED. Read-only.
#include <iostream>
#include <sl/Camera.hpp>
int main() {
  sl::Camera zed;
  sl::InitParameters init;
  init.depth_mode = sl::DEPTH_MODE::NONE;
  init.camera_resolution = sl::RESOLUTION::AUTO;
  init.camera_fps = 15;
  init.sdk_verbose = 0;
  if (zed.open(init) > sl::ERROR_CODE::SUCCESS) { std::cerr << "cannot open camera\n"; return 1; }
  auto info = zed.getCameraInformation();
  std::cout << "model " << sl::toString(info.camera_model) << " S/N " << info.serial_number
            << " FW " << info.camera_configuration.firmware_version << " SDK " << sl::Camera::getSDKVersion() << "\n";
  for (int s = 0; s < static_cast<int>(sl::VIDEO_SETTINGS::LAST); s++) {
    auto vs = static_cast<sl::VIDEO_SETTINGS>(s);
    std::cout << sl::toString(vs) << " | ";
    if (vs == sl::VIDEO_SETTINGS::AEC_AGC_ROI) {
      sl::Rect r; auto e = zed.getCameraSettings(vs, r, sl::SIDE::LEFT);
      std::cout << (e == sl::ERROR_CODE::SUCCESS ? std::to_string(r.x) + "," + std::to_string(r.y) + " " + std::to_string(r.width) + "x" + std::to_string(r.height) : sl::toString(e).get()) << "\n";
      continue;
    }
    int v = -999; auto e = zed.getCameraSettings(vs, v);
    int mn = -999, mx = -999; auto er = zed.getCameraSettingsRange(vs, mn, mx);
    std::cout << (e == sl::ERROR_CODE::SUCCESS ? std::to_string(v) : std::string(sl::toString(e).get()))
              << " | range " << (er == sl::ERROR_CODE::SUCCESS ? std::to_string(mn) + ".." + std::to_string(mx) : std::string(sl::toString(er).get())) << "\n";
  }
  zed.close();
}
