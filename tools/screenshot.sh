#!/usr/bin/env bash
# Builds a demo-pinned .pbw, runs it in the running emulator (tools/emu.sh,
# EMU_PLATFORM) and saves the
# physical screenshot plus the canvas view (landscape: rotated back to 168x144).
#   tools/screenshot.sh OUT_PREFIX [HH:MM:SS] [M/D/WDAY] [MODE FORMAT] [BATTERY] [QUIET] [24H] [ORIENT] [THEME]
# MODE: 0 minute, 1 hour. FORMAT: 0 segment start/end, 1 elapsed/remaining.
# ORIENT: 0 portrait (default), 1 landscape. THEME: 0 B/W (default), 1 color.
set -euo pipefail
cd "$(dirname "$0")/.."
out="$1"; time="${2:-15:29:18}"; date="${3:-10/1/4}"
mode="${4:-1}"; format="${5:-1}"; battery="${6:-65}"; quiet="${7:-0}"; h24="${8:-0}"; orient="${9:-0}"; theme="${10:-0}"
IFS=: read -r hh mm ss <<<"$time"
IFS=/ read -r mo md wd <<<"$date"
export PODDLE_DEFINES="DEMO_HOUR=$((10#$hh)) DEMO_MIN=$((10#$mm)) DEMO_SEC=$((10#$ss))
  DEMO_MON=$mo DEMO_MDAY=$md DEMO_WDAY=$wd DEMO_PROGRESS_MODE=$mode
  DEMO_LABEL_FORMAT=$format DEMO_BATTERY=$battery DEMO_QUIET=$quiet DEMO_24H=$h24 DEMO_ORIENTATION=$orient DEMO_THEME=$theme"
pebble build >/dev/null 2>&1 || { pebble build; exit 1; }
cp build/poddle.pbw "${TMPDIR:-/tmp}/poddle-demo.pbw"
unset PODDLE_DEFINES
pebble build >/dev/null 2>&1  # leave build/ as the normal build
tools/emu.sh install "${TMPDIR:-/tmp}/poddle-demo.pbw" >/dev/null 2>&1
sleep 3
tools/emu.sh shot "${out}_screen.png"
"$(uv tool dir)/pebble-tool/bin/python" - "${out}_screen.png" "${out}_canvas.png" "$orient" "$theme" <<'PY'
import sys
from collections import Counter
from PIL import Image
# The emulator tints white gray and dims it further over time (backlight
# simulation). The B/W theme is pure black on white, so store it as such;
# the color theme is rescaled so the background (white) is white again.
raw = Image.open(sys.argv[1]).convert("RGB")
if sys.argv[4] == "1":
    bg = Counter(raw.getdata()).most_common(1)[0][0]
    # Rescale, then snap to the 64-color palette (channel steps of 85).
    im = raw.point(lambda p, k=255 / max(bg): min(3, round(p * k / 85)) * 85)
else:
    im = raw.convert("L").point(lambda p: 0 if p < 40 else 255).convert("1")
if im.size == (148, 172):  # legacy emulator machines draw a 2px frame
    im = im.crop((2, 2, 146, 170))
im.save(sys.argv[1])
if sys.argv[3] == "1":
    # Physical (px, py) shows canvas (py, 143 - px): rotate 90 deg counter-clockwise.
    im = im.transpose(Image.Transpose.ROTATE_90)
im.save(sys.argv[2])
PY
echo "${out}_screen.png ${out}_canvas.png"
