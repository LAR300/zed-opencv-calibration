#!/usr/bin/env bash
# Builds the bench helpers of scripts/dev/ into build/dev/. Run in the container, as admin.
# They are developer tools for checks on the bench (.ai/docs/bench.md), not part of the
# calibration flow.
set -euo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
S=$R/scripts/dev
O=$R/build/dev
mkdir -p "$O"
CV=$(pkg-config --cflags --libs opencv4)
ZED="-I/usr/local/zed/include -I/usr/local/cuda/include -L/usr/local/zed/lib -lsl_zed -Wl,-rpath,/usr/local/zed/lib -L/usr/local/cuda/lib64 -lcudart"
gcc -O2 -o "$O/sendkey" "$S/sendkey.c" -lX11 -lXtst
g++ -O2 -std=c++17 -o "$O/read_video_settings" "$S/read_video_settings.cpp" $ZED
g++ -O2 -std=c++17 -o "$O/check_detections" "$S/check_detections.cpp" \
    "$R/common/aprilgrid/aprilgrid.cpp" "$R/common/capture_io/detections_io.cpp" \
    -I"$R/common/aprilgrid" -I"$R/common/capture_io" $CV
g++ -O2 -std=c++17 -o "$O/checker_errors" "$S/checker_errors.cpp" \
    "$R/common/aprilgrid/aprilgrid.cpp" "$R/common/calib_io/opencv_calib_file.cpp" \
    -I"$R/common/aprilgrid" -I"$R/common/calib_io" $CV $ZED
echo "built in $O: sendkey read_video_settings check_detections checker_errors"
