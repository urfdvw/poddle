#!/usr/bin/env bash
# Builds a demo-pinned .pbw, runs it in the flint emulator and saves the
# physical screenshot plus the canvas view (rotated back to 168x144).
#   tools/screenshot.sh OUT_PREFIX [HH:MM:SS] [M/D/WDAY] [MODE FORMAT] [BATTERY] [QUIET] [24H]
# MODE: 0 minute, 1 hour. FORMAT: 0 segment start/end, 1 elapsed/remaining.
set -euo pipefail
cd "$(dirname "$0")/.."
out="$1"; time="${2:-15:29:18}"; date="${3:-10/1/4}"
mode="${4:-1}"; format="${5:-1}"; battery="${6:-65}"; quiet="${7:-0}"; h24="${8:-0}"
IFS=: read -r hh mm ss <<<"$time"
IFS=/ read -r mo md wd <<<"$date"
export PODDLE_DEFINES="DEMO_HOUR=$((10#$hh)) DEMO_MIN=$((10#$mm)) DEMO_SEC=$((10#$ss))
  DEMO_MON=$mo DEMO_MDAY=$md DEMO_WDAY=$wd DEMO_PROGRESS_MODE=$mode
  DEMO_LABEL_FORMAT=$format DEMO_BATTERY=$battery DEMO_QUIET=$quiet DEMO_24H=$h24"
pebble build >/dev/null 2>&1 || { pebble build; exit 1; }
cp build/poddle.pbw "${TMPDIR:-/tmp}/poddle-demo.pbw"
unset PODDLE_DEFINES
pebble build >/dev/null 2>&1  # leave build/ as the normal build
tools/emu.sh install "${TMPDIR:-/tmp}/poddle-demo.pbw" >/dev/null 2>&1
sleep 3
tools/emu.sh shot "${out}_screen.png"
"$(uv tool dir)/pebble-tool/bin/python" - "${out}_screen.png" "${out}_canvas.png" <<'PY'
import sys
from PIL import Image
# Physical (px, py) shows canvas (py, 143 - px): rotate 90 deg counter-clockwise.
Image.open(sys.argv[1]).transpose(Image.Transpose.ROTATE_90).save(sys.argv[2])
PY
echo "${out}_screen.png ${out}_canvas.png"
