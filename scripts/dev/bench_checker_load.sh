#!/usr/bin/env bash
# Bench check: does zed_stereo_checker open the camera with a given calibration file?
# Runs it under a private Xvfb for a few seconds, then 'q'. No target needed.
# Usage (in the container, as admin): bench_checker_load.sh <calib.yml>
set -u
DEV=/workspaces/isaac_ros-dev/dev
BIN=$DEV/zed-opencv-calibration/build/stereo_checker/zed_stereo_checker
TGT=${ZED_TARGET:-$DEV/zed-opencv-calibration/config/aprilgrid_6x6_88mm.yaml}
KEY=$DEV/zed-opencv-calibration/build/dev/sendkey
OUT=/tmp/bench_checker_load.out
Xvfb :98 -screen 0 1920x1080x24 > /dev/null 2>&1 &
XPID=$!
sleep 2
export DISPLAY=:98
"$BIN" --target_yaml "$TGT" --calib_opencv "$1" > "$OUT" 2>&1 &
TPID=$!
sleep 12
"$KEY" q
sleep 3
kill -INT "$TPID" 2>/dev/null
sleep 1
kill "$XPID" 2>/dev/null   # this Xvfb only, by PID
wait "$TPID" 2>/dev/null; echo "checker exit code: $?"
grep -E "Error|Calibration source|Camera S/N|Distortion model" "$OUT"
