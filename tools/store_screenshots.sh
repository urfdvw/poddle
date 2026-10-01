#!/usr/bin/env bash
# Captures the app store screenshots: one portrait and one landscape per
# platform, as pure black/white PNGs. The 144x168 targets (aplite, basalt,
# diorite, flint) share one pair taken on flint; emery (200x228, large
# sprites) gets its own. Boots each emulator in turn.
# Upload order puts portrait first (see tools/publish.sh).
#   tools/store_screenshots.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="${TMPDIR:-/tmp}/poddle-store"
mkdir -p "$tmp" store/screenshots
capture() {  # EMU platform, file prefix, target platforms...
  local emu="$1" prefix="$2"; shift 2
  tools/emu.sh stop; sleep 1
  EMU_PLATFORM="$emu" tools/emu.sh start
  EMU_PLATFORM="$emu" tools/screenshot.sh "$tmp/${prefix}_portrait" 15:29:18 10/1/4 1 1 65 0 0 0 >/dev/null
  EMU_PLATFORM="$emu" tools/screenshot.sh "$tmp/${prefix}_landscape" 15:29:18 10/1/4 1 1 65 0 0 1 >/dev/null
  tools/emu.sh stop
  "$(uv tool dir)/pebble-tool/bin/python" - "$tmp" "$prefix" store/screenshots "$@" <<'PY'
import sys
from PIL import Image
src, prefix, dst, platforms = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
for n, name in ((1, "portrait"), (2, "landscape")):
    im = Image.open(f"{src}/{prefix}_{name}_screen.png").convert("1")
    for platform in platforms:
        im.save(f"{dst}/{platform}_{n}_{name}.png")
PY
}
capture flint small aplite basalt diorite flint
capture emery large emery
ls store/screenshots
