#!/usr/bin/env bash
# Installs the Stereolabs udev rule on the HOST (.ai/specs/ambiente-sensores R1, R2).
# Idempotent: an identical installed rule is left alone. Needs sudo.
# Usage: scripts/bench/install_zed_udev.sh [--dry-run]
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
src="$here/../../config/udev/99-stereolabs-zed.rules"
dst=/etc/udev/rules.d/99-stereolabs-zed.rules
dry=false
[[ ${1:-} == --dry-run ]] && dry=true
[[ -f /.dockerenv ]] && { echo "error: run this on the host, not in a container" >&2; exit 1; }

if [[ -f $dst ]] && cmp -s "$src" "$dst"; then
  echo "rule already installed: $dst (nothing to do)"
  exit 0
fi
run() { if $dry; then echo "would run: $*"; else echo "+ $*"; "$@"; fi; }
[[ -f $dst ]] && echo "an older rule is installed at $dst: it will be replaced"
run sudo install -m 0644 "$src" "$dst"
run sudo udevadm control --reload-rules
run sudo udevadm trigger --subsystem-match=usb --attr-match=idVendor=2b03
run sudo udevadm trigger --subsystem-match=hidraw
$dry || echo "done; unplug and plug the camera if the check still fails"
