cmake_minimum_required(VERSION 3.5)
project(zed_calibration_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common/aprilgrid)

FILE(GLOB_RECURSE TEST_SRC_FILES "*.cpp")
FILE(GLOB_RECURSE APRILGRID_SRC_FILES "../common/aprilgrid/*.cpp")
ADD_EXECUTABLE(aprilgrid_tests ${TEST_SRC_FILES} ${APRILGRID_SRC_FILES})

TARGET_LINK_LIBRARIES(aprilgrid_tests ${OpenCV_LIBRARIES})

add_test(NAME aprilgrid_tests COMMAND aprilgrid_tests)
