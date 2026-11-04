#!/usr/bin/env bash
# Puts 'admin' in the 'zed' group of the running container, so that it can read
# the ZED SDK (/usr/local/zed is root:zed 770), and checks that libsl_zed.so
# loads (.ai/specs/ambiente-sensores R3). Run on the host after every container
# recreation. The lasting fix belongs in the isaac_ros entrypoint_additions/.
# Usage: scripts/bench/fix_zed_group.sh [--dry-run] [container]   (default isaac-ros-dev)
set -euo pipefail
dry=false
[[ ${1:-} == --dry-run ]] && { dry=true; shift; }
c=${1:-${ZED_CONTAINER:-isaac-ros-dev}}
groups=$(docker exec --user root "$c" id -nG admin)
if [[ " $groups " == *" zed "* ]]; then
  echo "admin is already in the zed group of $c"
elif $dry; then
  echo "would run: docker exec --user root $c usermod -aG zed admin"
else
  echo "+ docker exec --user root $c usermod -aG zed admin"
  docker exec --user root "$c" usermod -aG zed admin
fi
$dry && exit 0
if docker exec --user admin "$c" python3 -c "import ctypes; ctypes.CDLL('/usr/local/zed/lib/libsl_zed.so')" 2>/dev/null; then
  echo "libsl_zed.so loads as admin"
else
  echo "error: libsl_zed.so still does not load as admin" >&2
  exit 1
fi
