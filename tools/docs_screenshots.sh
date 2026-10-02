#!/usr/bin/env bash
# Regenerates every screenshot under docs/screenshots/ from the emulators
# (state pinned through tools/screenshot.sh), plus the mockup comparisons.
#   tools/docs_screenshots.sh
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=docs/screenshots
TMP="${TMPDIR:-/tmp}/poddle-docs"
PY="$(uv tool dir)/pebble-tool/bin/python"
mkdir -p "$TMP" "$OUT"/{portrait,landscape,devices,color,disconnected}

boot() {
  tools/emu.sh stop; sleep 1
  EMU_PLATFORM="$1" tools/emu.sh start
}
shot() {  # platform out_prefix screenshot.sh-args...
  local p="$1" out="$2"; shift 2
  EMU_PLATFORM="$p" tools/screenshot.sh "$out" "$@" >/dev/null
}

# name | screenshot.sh args (time date mode format battery quiet 24h)
STATES=(
  "hour_elapsed|15:29:18 10/1/4 1 1 65 0 0"
  "hour_segment|15:29:18 10/1/4 1 0 65 0 0"
  "minute_elapsed|15:29:18 10/1/4 0 1 65 0 0"
  "minute_segment|15:29:18 10/1/4 0 0 65 0 0"
  "oclock_quiet|12:00:00 12/31/4 1 1 100 1 0"
  "oh_five_24h|9:05:42 6/18/4 0 0 10 0 1"
  "longest|23:23:07 2/28/6 1 1 0 0 0"
  "wrap|12:59:59 1/1/4 0 0 40 0 0"
)
DEFAULT="15:29:18 10/1/4 1 1 65 0 0"
DISCONNECTED="10:59:18 10/1/4 1 1 65 1 0"  # quiet time on too: disconnected wins

# flint: every state in both orientations, the device row, disconnected.
boot flint
for o in 0 1; do
  dir=$([ "$o" = 0 ] && echo portrait || echo landscape)
  for state in "${STATES[@]}"; do
    IFS='|' read -r name args <<<"$state"
    # shellcheck disable=SC2086
    shot flint "$TMP/state" $args "$o" 0 0
    cp "$TMP/state_canvas.png" "$OUT/$dir/$name.png"
  done
  # shellcheck disable=SC2086
  shot flint "$TMP/flint_dev_$o" $DEFAULT "$o" 0 0
  # shellcheck disable=SC2086
  shot flint "$TMP/flint_t0_dc_$o" $DISCONNECTED "$o" 0 1
done

# Other platforms: device row (B/W), color theme, disconnected.
for p in aplite basalt emery; do
  boot "$p"
  themes=0
  case "$p" in basalt|emery) themes="0 1" ;; esac
  for o in 0 1; do
    # shellcheck disable=SC2086
    shot "$p" "$TMP/${p}_dev_$o" $DEFAULT "$o" 0 0
    for t in $themes; do
      # shellcheck disable=SC2086
      shot "$p" "$TMP/${p}_t${t}_dc_$o" $DISCONNECTED "$o" "$t" 1
    done
    if [ "$themes" != 0 ]; then
      # shellcheck disable=SC2086
      shot "$p" "$TMP/${p}_color_$o" $DEFAULT "$o" 1 0
    fi
  done
  if [ "$themes" != 0 ]; then
    shot "$p" "$TMP/${p}_color_q" 23:23:07 2/28/6 0 0 100 1 0 0 1 0
  fi
  if [ "$p" = emery ]; then
    shot emery "$TMP/emery_quiet" 10:59:18 10/1/4 1 1 65 1 0 0 1 0
    shot emery "$TMP/emery_sound" 10:59:18 10/1/4 1 1 65 0 0 0 1 0
  fi
done
tools/emu.sh stop

NODE_PATH="$(npm root -g)" node tools/render_mock.js 2026-10-01T15:29:18 "$TMP/mock"
"$PY" tools/compare_mock.py "$TMP/mock_portrait_hour.png" "$OUT/portrait/hour_elapsed.png" \
  "$TMP/cmp_p" | tail -1
"$PY" tools/compare_mock.py "$TMP/mock_canvas.png" "$OUT/landscape/hour_elapsed.png" \
  "$TMP/cmp_l" | tail -1

"$PY" - "$TMP" "$OUT" <<'PY'
import shutil
import sys

from PIL import Image, ImageDraw

tmp, out = sys.argv[1], sys.argv[2]
STATES = ["hour_elapsed", "hour_segment", "minute_elapsed", "minute_segment",
          "oclock_quiet", "oh_five_24h", "longest", "wrap"]


def grid(cells, cols, path, scale=2, label=True):
    """cells: list of (label, image); bottom-aligned rows of `cols`."""
    rows = [cells[i:i + cols] for i in range(0, len(cells), cols)]
    pad = 14 if label else 0
    width = max(sum(im.width + 8 for _, im in r) for r in rows)
    heights = [max(im.height for _, im in r) + pad + 8 for r in rows]
    g = Image.new("RGB", (width, sum(heights)), "white")
    d = ImageDraw.Draw(g)
    y = 0
    for r, h in zip(rows, heights):
        x = 0
        for name, im in r:
            g.paste(im.convert("RGB"), (x, y + h - pad - 8 - im.height))
            if label:
                d.text((x, y + h - pad - 6), name, fill=(0, 0, 0))
            x += im.width + 8
        y += h
    g.resize((g.width * scale, g.height * scale), Image.NEAREST).save(path)


def img(path):
    return Image.open(path).convert("RGB")


for d in ("portrait", "landscape"):
    grid([(s, img(f"{out}/{d}/{s}.png")) for s in STATES], 4, f"{out}/{d}/all_states.png")
shutil.copy(f"{tmp}/cmp_p_side.png", f"{out}/portrait/compare_side_by_side.png")
shutil.copy(f"{tmp}/cmp_p_overlay.png", f"{out}/portrait/compare_overlay.png")
shutil.copy(f"{tmp}/cmp_l_side.png", f"{out}/landscape/compare_side_by_side.png")
shutil.copy(f"{tmp}/cmp_l_overlay.png", f"{out}/landscape/compare_overlay.png")
shutil.copy(f"{tmp}/mock_portrait_hour.png", f"{out}/portrait/mock_frame2.png")
shutil.copy(f"{tmp}/mock_portrait_minute.png", f"{out}/portrait/mock_frame1.png")
shutil.copy(f"{tmp}/mock_canvas.png", f"{out}/landscape/mock_frame3_canvas.png")

# diorite renders identically to flint (same 144x168 B/W screen); the flint
# capture avoids the diorite emulator's blacked-out corners.
devices = ["aplite", "basalt", "diorite", "flint", "emery"]
src = {p: ("flint" if p == "diorite" else p) for p in devices}
cells = []
for o, orient in ((0, "portrait"), (1, "landscape")):
    for p in devices:
        im = img(f"{tmp}/{src[p]}_dev_{o}_screen.png")
        im.save(f"{out}/devices/{p}_{orient}.png")
        cells.append((p, im))
grid(cells, len(devices), f"{out}/devices/all_devices.png")

cells = []
for p in ("basalt", "emery"):
    for o, orient in ((0, "portrait"), (1, "landscape")):
        im = img(f"{tmp}/{p}_color_{o}_screen.png")
        im.save(f"{out}/color/{p}_{orient}.png")
        cells.append((f"{p} {orient}", im))
    cells.append((f"{p} quiet", img(f"{tmp}/{p}_color_q_screen.png")))
grid(cells, 6, f"{out}/color/all_color.png", label=False)

cells = []
for p, t in (("flint", 0), ("aplite", 0), ("diorite", 0), ("basalt", 0), ("basalt", 1),
             ("emery", 0), ("emery", 1)):
    for o in (0, 1):
        theme = " color" if t else ""
        cells.append((f"{p}{theme} {'portrait' if o == 0 else 'landscape'}",
                      img(f"{tmp}/{src[p]}_t{t}_dc_{o}_screen.png")))
grid(cells, 8, f"{out}/disconnected/all_disconnected.png", scale=1)

three = [img(f"{tmp}/emery_t1_dc_0_screen.png"), img(f"{tmp}/emery_quiet_screen.png"),
         img(f"{tmp}/emery_sound_screen.png")]
three = [im.crop((0, 0, 80, 40)).resize((320, 160), Image.NEAREST) for im in three]
g = Image.new("RGB", (320 * 3 + 32, 160), "white")
for i, im in enumerate(three):
    g.paste(im, (i * 336, 0))
g.save(f"{out}/disconnected/three_states_color.png")
print("docs screenshots written")
PY
