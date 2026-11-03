cmake_minimum_required(VERSION 3.5)
project(zed_calibration_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common/aprilgrid)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../stereo_calibration/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common/camera_settings)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common/capture_io)

FILE(GLOB_RECURSE TEST_SRC_FILES "*.cpp")
FILE(GLOB_RECURSE APRILGRID_SRC_FILES "../common/aprilgrid/*.cpp")
# Only the OpenCV part of camera_settings: zed_camera_settings.cpp needs the ZED SDK
set(CAMERA_SETTINGS_SRC_FILES ../common/camera_settings/camera_settings.cpp)
FILE(GLOB_RECURSE CAPTURE_IO_SRC_FILES "../common/capture_io/*.cpp")
ADD_EXECUTABLE(aprilgrid_tests ${TEST_SRC_FILES} ${APRILGRID_SRC_FILES}
               ${CAMERA_SETTINGS_SRC_FILES} ${CAPTURE_IO_SRC_FILES})

TARGET_LINK_LIBRARIES(aprilgrid_tests ${OpenCV_LIBRARIES})
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/BuildInfo.cmake)
zedcal_add_build_info(aprilgrid_tests)
# Lets the tests read files of the repository, such as config/session_example.yaml
target_compile_definitions(aprilgrid_tests PRIVATE
                           ZEDCAL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/..")

add_test(NAME aprilgrid_tests COMMAND aprilgrid_tests)
