#!/usr/bin/env bash
# Captures the app store screenshots from the flint emulator: one portrait,
# one landscape, as pure black/white 144x168 PNGs. All three targets share
# the same 144x168 B/W screen, so each platform gets the same pair.
# Upload order puts portrait first (see tools/publish.sh).
#   tools/emu.sh start && tools/store_screenshots.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="${TMPDIR:-/tmp}/poddle-store"
mkdir -p "$tmp" store/screenshots
tools/screenshot.sh "$tmp/portrait" 15:29:18 10/1/4 1 1 65 0 0 0 >/dev/null
tools/screenshot.sh "$tmp/landscape" 15:29:18 10/1/4 1 1 65 0 0 1 >/dev/null
"$(uv tool dir)/pebble-tool/bin/python" - "$tmp" store/screenshots <<'PY'
import sys
from PIL import Image
src, dst = sys.argv[1], sys.argv[2]
for n, name in ((1, "portrait"), (2, "landscape")):
    # The emulator tints white light gray; store the real 1-bit screen.
    im = Image.open(f"{src}/{name}_screen.png").convert("L").point(lambda p: 0 if p < 110 else 255)
    assert im.size == (144, 168)
    for platform in ("aplite", "diorite", "flint"):
        im.convert("1").save(f"{dst}/{platform}_{n}_{name}.png")
PY
ls store/screenshots
