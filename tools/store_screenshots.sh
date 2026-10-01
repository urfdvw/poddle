#!/usr/bin/env bash
# Captures the app store screenshots: five states per platform, taken in the
# emulator (each platform's own, except diorite, see below). File names are PLATFORM_N_STATE.png and N is
# the upload order (tools/publish.sh); 1 (portrait, default settings) leads
# the listing.
#
#   B/W screens (aplite, diorite, flint):
#     1 portrait     2 landscape     3 minute bar, start/end labels
#     4 O'Clock, quiet time, 24h     5 landscape, longest line
#     (aplite's state 4 leaves quiet time off: its firmware has no Quiet
#     Time API, so the icon never shows there. diorite reuses the flint
#     shots: same 144x168 B/W screen and identical rendering, without the
#     diorite emulator's blacked-out corner pixels.)
#   Color screens (basalt, emery):
#     1 portrait     2 landscape     3 color theme portrait
#     4 color theme landscape        5 color theme, quiet time, low battery
#
#   tools/store_screenshots.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="${TMPDIR:-/tmp}/poddle-store"
mkdir -p "$tmp" store/screenshots
rm -f store/screenshots/*.png

# name | time | M/D/WDAY | mode format | battery quiet 24h | orientation | theme
BW_STATES=(
  "1_portrait|15:29:18|10/1/4|1 1|65 0 0|0|0"
  "2_landscape|15:29:18|10/1/4|1 1|65 0 0|1|0"
  "3_minute_start_end|9:05:42|6/18/4|0 0|40 0 0|0|0"
  "4_oclock_quiet_24h|13:00:00|12/31/4|1 1|100 1 1|0|0"
  "5_landscape_longest|23:23:07|2/28/6|0 1|10 0 0|1|0"
)
COLOR_STATES=(
  "1_portrait|15:29:18|10/1/4|1 1|65 0 0|0|0"
  "2_landscape|15:29:18|10/1/4|1 1|65 0 0|1|0"
  "3_color_portrait|15:29:18|10/1/4|1 1|65 0 0|0|1"
  "4_color_landscape|15:29:18|10/1/4|1 1|65 0 0|1|1"
  "5_color_quiet_low_battery|23:23:07|2/28/6|0 0|20 1 0|0|1"
)

capture() {  # platform, states...
  local platform="$1"; shift
  tools/emu.sh stop; sleep 1
  EMU_PLATFORM="$platform" tools/emu.sh start
  for state in "$@"; do
    IFS='|' read -r name time date settings power orient theme <<<"$state"
    # shellcheck disable=SC2086
    EMU_PLATFORM="$platform" tools/screenshot.sh "$tmp/$platform" "$time" "$date" \
      $settings $power "$orient" "$theme" >/dev/null
    cp "$tmp/${platform}_screen.png" "store/screenshots/${platform}_${name}.png"
  done
  tools/emu.sh stop
}

capture flint "${BW_STATES[@]}"
for f in store/screenshots/flint_*.png; do cp "$f" "${f/flint_/diorite_}"; done
APLITE_STATES=("${BW_STATES[@]}")
APLITE_STATES[3]="4_oclock_24h|13:00:00|12/31/4|1 1|100 0 1|0|0"
capture aplite "${APLITE_STATES[@]}"
for p in basalt emery; do capture "$p" "${COLOR_STATES[@]}"; done
ls store/screenshots
