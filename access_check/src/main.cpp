// zed_access_check: opens the camera as the calibration tool does and reads the
// temperatures the capture metadata records (.ai/specs/ambiente-sensores R4).
// Exit codes: 0 temperatures read; 2 the camera does not open; 3 it opens but
// the sensors (HID) give no temperature.

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include <sl/Camera.hpp>

#include "zed_open.hpp"

int main() {
  sl::Camera zed;
  sl::InitParameters init;
  setCalibrationOpenParameters(init);
  init.sdk_verbose = 0;
  const sl::ERROR_CODE open = zed.open(init);
  if (open != sl::ERROR_CODE::SUCCESS) {
    std::cout << "camera: cannot open (" << sl::toString(open) << ")" << std::endl;
    return 2;
  }
  const auto info = zed.getCameraInformation();
  std::cout << "camera: " << sl::toString(info.camera_model) << " S/N " << info.serial_number
            << ", firmware " << info.camera_configuration.firmware_version << std::endl;
  sl::SensorsData sensors;
  int found = 0;
  if (zed.getSensorsData(sensors, sl::TIME_REFERENCE::CURRENT) == sl::ERROR_CODE::SUCCESS) {
    using Location = sl::SensorsData::TemperatureData::SENSOR_LOCATION;
    const std::pair<Location, const char*> locations[] = {
        {Location::ONBOARD_LEFT, "ONBOARD_LEFT"},
        {Location::ONBOARD_RIGHT, "ONBOARD_RIGHT"},
        {Location::IMU, "IMU"}};
    for (const auto& [location, name] : locations) {
      float celsius = 0.0f;
      if (sensors.temperature.get(location, celsius) == sl::ERROR_CODE::SUCCESS) {
        std::cout << "temperature " << name << ": " << std::fixed << std::setprecision(2)
                  << celsius << " C" << std::endl;
        ++found;
      } else {
        std::cout << "temperature " << name << ": not available" << std::endl;
      }
    }
  } else {
    std::cout << "sensors: not available" << std::endl;
  }
  zed.close();
  return found == 3 ? 0 : 3;
}
