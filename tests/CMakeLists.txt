cmake_minimum_required(VERSION 3.5)
project(zed_calibration_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common/aprilgrid)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common/camera_settings)

FILE(GLOB_RECURSE TEST_SRC_FILES "*.cpp")
FILE(GLOB_RECURSE APRILGRID_SRC_FILES "../common/aprilgrid/*.cpp")
# Only the OpenCV part of camera_settings: zed_camera_settings.cpp needs the ZED SDK
set(CAMERA_SETTINGS_SRC_FILES ../common/camera_settings/camera_settings.cpp)
ADD_EXECUTABLE(aprilgrid_tests ${TEST_SRC_FILES} ${APRILGRID_SRC_FILES} ${CAMERA_SETTINGS_SRC_FILES})

TARGET_LINK_LIBRARIES(aprilgrid_tests ${OpenCV_LIBRARIES})

add_test(NAME aprilgrid_tests COMMAND aprilgrid_tests)
