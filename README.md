# poddle

The Pebble watch face that brings you back to 2004: an iPod mini-style layout
(status bar, info area, progress bar), drawn landscape on a 144×168 B/W screen.

Targets: **aplite** (Pebble / Pebble Steel), **diorite** (Pebble 2, Pebble 2 SE),
**flint** (Pebble 2 Duo). No color or round platforms, and no emery/gabbro.

## Development environment

### Normal path

```sh
uv tool install pebble-tool --python 3.13
pebble sdk install latest
pebble build
pebble install --emulator flint
```

### From-source path (when sdk.repebble.com is unreachable)

The cloud container used to build this repo blocks `sdk.repebble.com`, so the
SDK was built from PebbleOS source instead. `tools/setup_sdk_from_source.sh`
reproduces it:

1. Installs host packages (`gettext`, `librsvg2-bin`, cmake/ninja, SDL/pixman
   runtime libs for QEMU).
2. Installs `pebble-tool` with uv (Python 3.13).
3. Installs the [PebbleOS-SDK](https://github.com/coredevices/PebbleOS-SDK)
   release bundle (ARM GCC 14.2 + picolibc + `qemu-pebble`) from GitHub.
4. Clones `coredevices/pebbleos` at a pinned tag (`v4.38.4`), applies
   `tools/pebbleos-qemu-null-event.patch`, builds the `qemu_flint` SDK-shell
   firmware and the app SDK for aplite/diorite/flint.
5. Lays it out as a pebble-tool SDK named `source-local` and activates it.
6. On hosts without IPv6, patches pypkjs to bind `0.0.0.0` (it otherwise fails
   with `Address family not supported`).

```sh
WORK=~/pebble-src ./tools/setup_sdk_from_source.sh
pebble build
tools/emu.sh start && tools/emu.sh install && tools/emu.sh shot shot.png
```

Things worth knowing about this path:

- **Frozen platforms.** aplite and diorite are frozen at old SDK revisions;
  upstream ships their `libpebble.a` prebuilt from the legacy SDK. The script
  regenerates it from the frozen export list. It links and the symbol order
  matches, but it is not byte-identical to the official library. **Build
  release `.pbw`s with the official SDK.**
- **Emulator.** Current PebbleOS only has a QEMU board for flint among our
  targets. Flint has the same 144×168 B/W display as diorite and aplite, so
  it is the visual reference for all three.
- **Firmware patch.** The SDK-shell firmware asserts
  (`event_service_client.c:41`) when an installed app launches over the
  built-in TicToc face: the dying app task gets a `PEBBLE_NULL_EVENT`. The
  patch drops null events in the app event loop. It only affects the local
  emulator image.
- **`tools/emu.sh`** runs QEMU headless with pypkjs as the phone. Use it
  instead of `pebble install --emulator`, which reconnects to the
  firmware's serial link, and the firmware stops answering after a
  reconnect.

## Repository layout

- `src/c/` — watch face C source
- `tools/` — SDK setup, emulator helper
- `dist/poddle.pbw` — committed build output
- `docs/` — emulator screenshots
