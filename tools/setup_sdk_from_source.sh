#!/usr/bin/env bash
# Builds a pebble-tool compatible app SDK from PebbleOS source, for hosts that
# cannot reach sdk.repebble.com (the normal `pebble sdk install latest` path).
#
# Produces <persist>/SDKs/source-local with aplite/basalt/diorite/emery/flint
# headers + libs, qemu_flint/qemu_emery firmware built from source, and the
# legacy SDK 4.9 emulator images for aplite/basalt/diorite.
set -euo pipefail

WORK="${WORK:-$HOME/pebble-src}"
SDK_TOOLS_VERSION="${SDK_TOOLS_VERSION:-0.1.10}"   # coredevices/PebbleOS-SDK release
PEBBLEOS_TAG="${PEBBLEOS_TAG:-v4.38.4}"
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK_NAME="source-local"
PERSIST="${XDG_DATA_HOME:-$HOME/.local/share}/pebble-sdk"
[ -d "$HOME/.pebble-sdk" ] && PERSIST="$HOME/.pebble-sdk"

# 1. Host packages needed by the PebbleOS build and QEMU.
if command -v apt-get >/dev/null; then
  apt-get install -y gettext librsvg2-bin cmake ninja-build \
    libsdl2-2.0-0 libpixman-1-0 libglib2.0-0 libpng16-16 >/dev/null
fi

# 2. pebble-tool (app build/emulator CLI).
command -v pebble >/dev/null || uv tool install pebble-tool --python 3.13

# 3. Firmware toolchain + Pebble QEMU (GitHub release bundle).
TOOLS="$HOME/pebbleos-sdk-$SDK_TOOLS_VERSION"
if [ ! -x "$TOOLS/qemu/bin/qemu-pebble" ]; then
  curl -LsSf -o /tmp/pebbleos-sdk-installer.sh \
    https://github.com/coredevices/PebbleOS-SDK/releases/latest/download/pebbleos-sdk-installer.sh
  sh /tmp/pebbleos-sdk-installer.sh --version "$SDK_TOOLS_VERSION" --defaults
fi
. "$TOOLS/env.sh"

# 4. PebbleOS source: qemu_flint firmware + SDK files.
mkdir -p "$WORK"
OS="$WORK/pebbleos"
if [ ! -d "$OS/.git" ]; then
  GIT_LFS_SKIP_SMUDGE=1 git clone --depth 1 --branch "$PEBBLEOS_TAG" https://github.com/coredevices/pebbleos "$OS"
fi
cd "$OS"
git fetch --depth 1 origin tag "$PEBBLEOS_TAG"
git checkout -f "$PEBBLEOS_TAG"
# The SDK-shell firmware asserts when an installed app is launched over the
# running TicToc face (the app task gets a PEBBLE_NULL_EVENT on its way out).
git apply "$HERE/pebbleos-qemu-null-event.patch"
for s in cmsis_core/CMSIS mbedtls/mbedtls nanopb/nanopb nimble/mynewt-nimble \
         qr_code_generator/QR-Code-generator resources/iconography speex/speex \
         tinymt/TinyMT nonfree/pebbleos-nonfree moddable/moddable; do
  git submodule update --init --depth 1 -- "third_party/$s"
done
[ -d .venv ] || uv venv -p 3.12 .venv -q
. .venv/bin/activate
uv pip install -q -r requirements.txt
# SDK-shell emulator firmware for every target that has a QEMU board.
QEMU_PLATFORMS="flint emery"
for p in $QEMU_PLATFORMS; do
  rm -rf build
  pbl configure --board "qemu_$p" -DCONFIG_SHELL_SDK=y
  pbl build qemu_image_micro qemu_image_spi
  mkdir -p "$WORK/qemu/$p"
  cp build/qemu_micro_flash.bin build/qemu_spi_flash.bin "$WORK/qemu/$p/"
done
pbl build sdk

# aplite/basalt/diorite are "frozen" platforms: upstream ships prebuilt
# libpebble.a from the legacy SDK. Regenerate the shim lib from the frozen
# export list.
PLATFORMS="aplite basalt diorite emery flint"
python - $PLATFORMS <<'PY'
import sys
sys.argv = ["build_sdk"] + sys.argv[1:]
sys.path.insert(0, "tools")
import build_sdk
orig = build_sdk.generate_shim_files
build_sdk.generate_shim_files = lambda *a, **k: orig(*a, **{**k, "build_shim_lib": True})
build_sdk.main()
PY

# 5. Lay out the SDK the way pebble-tool expects.
DEST="$PERSIST/SDKs/$SDK_NAME"
rm -rf "$DEST"
mkdir -p "$DEST/sdk-core/pebble" "$DEST/toolchain/bin"
cp -r build/sdk/common "$DEST/sdk-core/pebble/common"
cp build/sdk/waf "$DEST/sdk-core/pebble/waf"
cp build/sdk/requirements.txt build/sdk/package.json "$DEST/sdk-core/"
cp build/sdk/package.json "$DEST/package.json"
for p in $PLATFORMS; do
  mkdir -p "$DEST/sdk-core/pebble/$p"
  cp -r "build/sdk/$p/include" "build/sdk/$p/lib" "$DEST/sdk-core/pebble/$p/"
done
for p in $QEMU_PLATFORMS; do
  mkdir -p "$DEST/sdk-core/pebble/$p/qemu"
  cp "$WORK/qemu/$p/qemu_micro_flash.bin" "$DEST/sdk-core/pebble/$p/qemu/"
  bzip2 -c "$WORK/qemu/$p/qemu_spi_flash.bin" > "$DEST/sdk-core/pebble/$p/qemu/qemu_spi_flash.bin.bz2"
done
# Current PebbleOS has no QEMU boards for the legacy platforms; use the
# official SDK 4.9.77 emulator images mirrored in ericmigi/pebble-qemu-wasm.
LEGACY="$WORK/pebble-qemu-wasm"
[ -d "$LEGACY/.git" ] || GIT_LFS_SKIP_SMUDGE=1 git clone --depth 1 \
  https://github.com/ericmigi/pebble-qemu-wasm "$LEGACY"
for p in aplite basalt diorite; do
  mkdir -p "$DEST/sdk-core/pebble/$p/qemu"
  cp "$LEGACY/firmware/$p/qemu_micro_flash.bin" "$DEST/sdk-core/pebble/$p/qemu/"
  bzip2 -c "$LEGACY/firmware/$p/qemu_spi_flash.bin" > "$DEST/sdk-core/pebble/$p/qemu/qemu_spi_flash.bin.bz2"
done
cat > "$DEST/sdk-core/manifest.json" <<JSON
{"requirements": [], "version": "$SDK_NAME", "type": "sdk-core", "channel": ""}
JSON
ln -sf "$TOOLS"/arm-none-eabi/bin/* "$DEST/toolchain/bin/"
ln -sf "$TOOLS/qemu/bin/qemu-pebble" "$DEST/toolchain/bin/qemu-pebble"
mkdir -p "$DEST/toolchain/lib" && ln -sfn "$TOOLS/qemu/lib/pc-bios" "$DEST/toolchain/lib/pc-bios"
deactivate
uv venv -p 3.13 "$DEST/.venv" -q
VIRTUAL_ENV="$DEST/.venv" uv pip install -q -r "$DEST/sdk-core/requirements.txt" pillow
(cd "$DEST" && npm install --silent >/dev/null 2>&1 || true)
pebble sdk activate "$SDK_NAME"
pebble --version

# 6. Hosts without IPv6 (some containers): pypkjs binds ("", port), which
#    gevent resolves to AF_INET6 and fails. Bind IPv4 explicitly.
if ! python3 -c 'import socket; socket.socket(socket.AF_INET6)' 2>/dev/null; then
  PYPKJS_WS="$(uv tool dir)/pebble-tool/lib/python3.13/site-packages/pypkjs/runner/websocket.py"
  sed -i 's/WSGIServer(("", self.port)/WSGIServer(("0.0.0.0", self.port)/' "$PYPKJS_WS"
fi
