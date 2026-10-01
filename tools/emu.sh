#!/usr/bin/env bash
# Headless emulator helper. EMU_PLATFORM picks the board: flint (default),
# emery, basalt, diorite or aplite.
#   tools/emu.sh start            boot the qemu_flint firmware + pypkjs
#   tools/emu.sh install [pbw]    install a .pbw (default build/poddle.pbw)
#   tools/emu.sh shot out.png     screenshot via the QEMU monitor
#   tools/emu.sh monitor CMD...   send a raw QEMU monitor command
#   tools/emu.sh stop
set -euo pipefail
PERSIST="${XDG_DATA_HOME:-$HOME/.local/share}/pebble-sdk"
[ -d "$HOME/.pebble-sdk" ] && PERSIST="$HOME/.pebble-sdk"
SDK="$PERSIST/SDKs/${PEBBLE_SDK_NAME:-source-local}"
PLATFORM="${EMU_PLATFORM:-flint}"
QEMU_DIR="$SDK/sdk-core/pebble/$PLATFORM/qemu"
STATE="${EMU_STATE:-/tmp/poddle-emu}"
SPI="$STATE/spi.bin"
AUDIO=(-audio driver=none,id=audio0)
case "$PLATFORM" in
  flint) MACHINE=(-machine pebble-flint -cpu cortex-m4 -drive if=mtd,format=raw,file="$SPI" "${AUDIO[@]}") ;;
  emery) MACHINE=(-machine pebble-emery -cpu cortex-m33 -drive if=mtd,format=raw,file="$SPI"
                  "${AUDIO[@]}") ;;
  # Legacy SDK 4.9 firmware images (see setup_sdk_from_source.sh).
  basalt) MACHINE=(-machine pebble-snowy-bb -cpu cortex-m4
                   -drive if=none,id=spi-flash,file="$SPI",format=raw) ;;
  diorite) MACHINE=(-machine pebble-silk-bb -cpu cortex-m4 -mtdblock "$SPI") ;;
  aplite) MACHINE=(-machine pebble-bb2 -cpu cortex-m3 -mtdblock "$SPI") ;;
  *) echo "unsupported EMU_PLATFORM: $PLATFORM" >&2; exit 1 ;;
esac
TOOL_PY="$(uv tool dir)/pebble-tool/bin/python"
PORT_BT=12344 PORT_MON=12346 PORT_PKJS=9000
mkdir -p "$STATE"

case "${1:-}" in
  start)
    bzip2 -dc "$QEMU_DIR/qemu_spi_flash.bin.bz2" > "$SPI"
    "$SDK/toolchain/bin/qemu-pebble" -rtc base=localtime \
      -serial null \
      -serial tcp::$PORT_BT,server=on,wait=off \
      -serial file:"$STATE/console.log" \
      -kernel "$QEMU_DIR/qemu_micro_flash.bin" \
      -monitor tcp::$PORT_MON,server=on,wait=off \
      "${MACHINE[@]}" \
      -display none \
      > "$STATE/qemu.log" 2>&1 &
    echo $! > "$STATE/qemu.pid"
    for _ in $(seq 60); do
      grep -aqE "Ready for communication|<SDK Home>|<Launcher>" "$STATE/console.log" 2>/dev/null && break
      sleep 1
    done
    # pypkjs plays the phone; it must be the first and only client of the
    # firmware's serial link (the firmware does not survive a reconnect).
    # Legacy firmware logs are hashed, so the ready line above may never
    # match; if pypkjs gives up waiting for the watch, start it again.
    for _ in 1 2 3; do
      rm -rf "$STATE/pkjs"; mkdir -p "$STATE/pkjs"
      "$TOOL_PY" -m pypkjs --qemu localhost:$PORT_BT --port $PORT_PKJS \
        --persist "$STATE/pkjs" > "$STATE/pypkjs.log" 2>&1 &
      echo $! > "$STATE/pypkjs.pid"
      for _ in $(seq 30); do
        (exec 3<>/dev/tcp/127.0.0.1/$PORT_PKJS) 2>/dev/null && break 2
        kill -0 "$(cat "$STATE/pypkjs.pid")" 2>/dev/null || break
        sleep 1
      done
      sleep 5
    done
    ;;
  install)
    pebble install --phone 127.0.0.1:$PORT_PKJS "${2:-build/poddle.pbw}"
    ;;
  shot)
    out="${2:-screenshot.png}"
    "$0" monitor "screendump $out.ppm"
    sleep 0.5
    "$TOOL_PY" -c "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[2])" "$out.ppm" "$out"
    rm -f "$out.ppm"
    ;;
  monitor)
    shift
    "$TOOL_PY" - "$PORT_MON" "$*" <<'PY'
import socket, sys, time
s = socket.create_connection(("localhost", int(sys.argv[1])))
time.sleep(0.3); s.recv(65536)
s.sendall((sys.argv[2] + "\n").encode()); time.sleep(0.5); s.recv(65536)
PY
    ;;
  stop)
    for p in pypkjs qemu; do
      [ -f "$STATE/$p.pid" ] && kill "$(cat "$STATE/$p.pid")" 2>/dev/null || true
      rm -f "$STATE/$p.pid"
    done
    ;;
  *) echo "usage: $0 start|install [pbw]|shot out.png|monitor CMD|stop" >&2; exit 1;;
esac
