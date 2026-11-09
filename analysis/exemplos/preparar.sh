#!/usr/bin/env bash
# Prepares the example of the offline analysis (.ai/docs/fluxo-analise-offline.md): for each
# recorded AprilGrid capture, a work folder in calib_runs/analises/exemplo/<name>/ with a link to
# its zed-images/ and the C++ tool run without the camera (--use_stored_values), which writes
# detections.json and the calibration zed_calibration_0.yml. The original folders are only read.
# Run in the container, as admin. Refuses to run if the example folder exists.
set -euo pipefail
DEV=/workspaces/isaac_ros-dev/dev
A=${EXEMPLO:-$DEV/calib_runs/analises/exemplo}
BIN=$DEV/zed-opencv-calibration/build/stereo_calibration/zed_stereo_calibration
TARGET=$DEV/zed-opencv-calibration/config/aprilgrid_6x6_88mm.yaml
declare -A CAPTURES=(
  [v1]=2026-10-20_ar_aprilgrid             # air, first AprilGrid capture (few tilted poses)
  [v2]=2026-10-20_ar_aprilgrid_v2          # air
  [controles]=2026-11-03_camera_settings   # air, fixed camera settings
  [acrilico]=2026-10-23_acrilico_ar_aprilgrid   # air, through the acrylic port
)
if [[ -e $A ]]; then echo "$A exists: delete it to prepare again"; exit 1; fi
for name in v1 v2 controles acrilico; do
  dir=$A/$name
  mkdir -p "$dir"
  ln -s "$DEV/calib_runs/${CAPTURES[$name]}/zed-images" "$dir/zed-images"
  cp "$TARGET" "$dir/target.yaml"
  echo "== $name (${CAPTURES[$name]})"
  (cd "$dir" && "$BIN" --use_stored_values --target_yaml target.yaml --detections_out detections.json > calib.log 2>&1)
  grep -E "Valid samples|Stereo:" "$dir/calib.log"
done
echo "prepared in $A"
