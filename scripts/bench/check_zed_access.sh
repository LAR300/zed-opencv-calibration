#!/usr/bin/env bash
# Read-only check of the camera access on the bench (.ai/specs/ambiente-sensores R4).
# Run on the HOST: it inspects the host's device nodes and runs probes in the container.
#   1. admin reads the ZED SDK in the container (libsl_zed.so loads);
#   2. the USB node of the camera's HID interface (2b03:f881 on the ZED 2i) and its hidraw
#      node are readable and writable by any user (the SDK opens them through libusb/hidraw;
#      the video interface goes through the kernel UVC driver and is only reported);
#   3. getSensorsData gives the ONBOARD_LEFT/RIGHT and IMU temperatures (zed_access_check).
# Exit 0 when everything passes. Env: ZED_CONTAINER (isaac-ros-dev), ZED_REPO_IN_CONTAINER.
# Usage: scripts/bench/check_zed_access.sh --check
set -uo pipefail
[[ ${1:-} == --check ]] || { echo "usage: $0 --check" >&2; exit 2; }
c=${ZED_CONTAINER:-isaac-ros-dev}
repo=${ZED_REPO_IN_CONTAINER:-/workspaces/isaac_ros-dev/dev/zed-opencv-calibration}
fails=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; fails=$((fails + 1)); }

state=$(docker inspect -f '{{.State.Running}}' "$c" 2>/dev/null)
if [[ $state != true ]]; then
  fail "container $c is not running"
  exit 1
fi

# 1. SDK
if docker exec --user admin "$c" python3 -c "import ctypes; ctypes.CDLL('/usr/local/zed/lib/libsl_zed.so')" 2>/dev/null; then
  pass "admin loads libsl_zed.so in $c"
else
  fail "admin cannot load libsl_zed.so in $c: run scripts/bench/fix_zed_group.sh"
fi

# 2. device nodes (the host's /dev is the container's: both run privileged)
usb=$(lsusb -d 2b03: 2>/dev/null)
if [[ -z $usb ]]; then
  fail "no Stereolabs device on USB (lsusb -d 2b03:)"
else
  hid_usb=0
  while read -r _ bus _ dev _ id _; do
    node=/dev/bus/usb/$bus/${dev%:}
    mode=$(stat -c %a "$node")
    name=$(udevadm info -q property -n "$node" 2>/dev/null | sed -n 's/^ID_MODEL=//p')
    if [[ $name != *HID* ]]; then
      # the video interface goes through the kernel UVC driver (/dev/video*), not libusb
      echo "INFO  USB $id $node mode $mode ($name: video, opened through /dev/video*)"
      continue
    fi
    hid_usb=1
    if (( (8#$mode & 8#006) == 8#006 )); then pass "USB $id $node mode $mode ($name)"
    else fail "USB $id $node mode $mode ($name): not rw for every user (install the udev rule)"; fi
  done <<< "$usb"
  (( hid_usb )) || fail "the camera's HID interface is not on USB"
fi
hid=""
for h in /sys/class/hidraw/hidraw*; do
  [[ -e $h/device/uevent ]] || continue
  ue=$(cat "$h/device/uevent")
  [[ $ue == *HID_ID=*00002B03:* ]] && hid="$hid /dev/$(basename "$h")"
done
if [[ -z $hid ]]; then
  echo "INFO  no hidraw node of the camera: the SDK detaches the kernel HID driver when it opens the camera (back after a replug)"
else
  for n in $hid; do
    mode=$(stat -c %a "$n")
    if (( (8#$mode & 8#006) == 8#006 )); then pass "HID $n mode $mode"
    else fail "HID $n mode $mode: not rw for every user (install the udev rule)"; fi
  done
fi
if [[ -f /etc/udev/rules.d/99-stereolabs-zed.rules ]]; then
  pass "udev rule installed"
else
  fail "udev rule not installed: scripts/bench/install_zed_udev.sh (the modes above do not survive a replug)"
fi

# 3. temperatures through the SDK
probe=$repo/build/access_check/zed_access_check
out=$(docker exec --user admin "$c" "$probe" 2>&1)
code=$?
case $code in
  0) pass "temperatures read: $(echo "$out" | grep -o '[A-Z_]*: [0-9.]* C' | tr '\n' ' ')" ;;
  2) fail "the camera does not open: $(echo "$out" | head -1)" ;;
  3) fail "the camera opens but the sensors give no temperature (HID access)" ;;
  *) fail "zed_access_check did not run ($code): build it with cmake --build build --target zed_access_check" ;;
esac

echo "$fails check(s) failed"
(( fails == 0 ))
