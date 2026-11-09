#!/usr/bin/env bash
# Bench check of the --session capture without touching the keyboard (persistencia-captura).
# Runs zed_stereo_calibration under a private Xvfb, presses 's' N times, then 'q'.
# Usage (in the container, as admin): bench_session_capture.sh <condition> <capture> <presses>
# Output: $ZED_TEST_ROOT/<condition>/captura_XX/ (default calib_runs/testes, outside dados/).
# Needs scripts/dev/build.sh (sendkey) and the AprilGrid in front of the camera.
set -u
COND=${1:-teste}; CAP=${2:-1}; PRESSES=${3:-3}
DEV=/workspaces/isaac_ros-dev/dev
BIN=$DEV/zed-opencv-calibration/build/stereo_calibration/zed_stereo_calibration
TGT=${ZED_TARGET:-$DEV/zed-opencv-calibration/config/aprilgrid_6x6_88mm.yaml}
KEY=$DEV/zed-opencv-calibration/build/dev/sendkey
ROOT=${ZED_TEST_ROOT:-$DEV/calib_runs/testes}
SESSION=/tmp/bench_session_${COND}_${CAP}.yaml
OUT=/tmp/bench_session_${COND}_${CAP}.out

sed -e "s#^output_root: .*#output_root: $ROOT#" -e "s#^condition: .*#condition: $COND#" \
    -e "s#^capture: .*#capture: $CAP#" $DEV/zed-opencv-calibration/config/session_example.yaml > "$SESSION"

Xvfb :98 -screen 0 1920x1080x24 > /dev/null 2>&1 &
XPID=$!
sleep 2
export DISPLAY=:98
"$BIN" --target_yaml "$TGT" --session "$SESSION" > "$OUT" 2>&1 &
TPID=$!
sleep 15                       # camera open and first frames
for i in $(seq 1 "$PRESSES"); do "$KEY" s; sleep 3; done
"$KEY" q
sleep 4
kill -INT "$TPID" 2>/dev/null  # Ctrl+C restores the camera settings, if still running
sleep 2
kill "$XPID" 2>/dev/null       # this Xvfb only, by PID (never killall: the Orin runs pid: host)
wait "$TPID" 2>/dev/null; echo "tool exit code: $?"
echo "output: $OUT"
