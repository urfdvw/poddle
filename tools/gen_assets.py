#!/usr/bin/env python3
"""Build-time asset pipeline for the poddle watch face.

Renders every string the face can show with Carthage Sans Bold, 1-bit with
no antialiasing, rotates each one 90 degrees clockwise, and packs each
category into its own sprite sheet:

  A  digits.png    0-9 : / -           (status time, date, progress labels)
  B  weekdays.png  Mo Tu We Th Fr Sa Su
  C  words.png     spoken-time vocabulary + AM/PM
  -  icons.png     quiet-time on/off speaker, disconnected, battery outline
     icons_color.png  the same icons tinted (and haloed) for the color theme

Each sheet is also written upright as NAME_portrait.png (the rotated sheet
rotated back) for the portrait orientation, which draws the same entries
without any rotation. src/c/assets.h describes where each entry lives.

Two scales are built:
  base   16px (one FontStruct brick = one pixel) for the 144x168 screens
  large  22px, written as NAME~emery.png so the SDK picks it for the
         200x228 Pebble Time 2; assets.h selects the matching tables.

Coordinates: the design canvas is 168x144 (landscape). Rotating it 90 degrees
clockwise gives the physical 144x168 screen, so canvas (x, y) lands on
physical (143 - y, x). Within a sheet, entries are stacked along physical y
(= canvas x), so a text entry of canvas size adv x band_h occupies the
physical sub-rect (0, py, band_h, adv). In the portrait sheet, whose height
is the rotated sheet's width W, the same entry sits at (py, W - px - h, w, h).

Usage: python3 tools/gen_assets.py   (run from the repo root)
"""

import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT_PATH = os.path.join(ROOT, "tools", "fonts", "Carthage-Sans-Bold.ttf")
IMG_DIR = os.path.join(ROOT, "resources", "images")
HEADER = os.path.join(ROOT, "src", "c", "assets.h")


DIGIT_GLYPHS = list("0123456789:/-")
WEEKDAYS = ["Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"]  # struct tm tm_wday order
ONES = ["One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine",
        "Ten", "Eleven", "Twelve"]
TEENS = ["Thirteen", "Fourteen", "Fifteen", "Sixteen", "Seventeen", "Eighteen",
         "Nineteen"]
TENS = ["Twenty", "Thirty", "Forty", "Fifty"]
WORDS = ONES + TEENS + TENS + ["Oh", "O'Clock", "AM", "PM"]

# Canvas-orientation pixel art, '#' = black.
BASE_ICONS = {
    "QUIET_OFF": [  # sound on: speaker + waves
        ".....#...#...",
        "....##....#..",
        "...###..#..#.",
        "######...#.#.",
        "######...#.#.",
        "######...#.#.",
        "...###..#..#.",
        "....##....#..",
        ".....#...#...",
    ],
    "QUIET_ON": [  # quiet time: speaker + X
        ".....#.......",
        "....##.......",
        "...###.#...#.",
        "######..#.#..",
        "######...#...",
        "######..#.#..",
        "...###.#...#.",
        "....##.......",
        ".....#.......",
    ],
    "DISCONNECTED": [  # phone link lost: Bluetooth rune + X, like QUIET_ON
        "...#.........",
        "...##........",
        ".#.#.#.#...#.",
        "..###...#.#..",
        "...#.....#...",
        "..###...#.#..",
        ".#.#.#.#...#.",
        "...##........",
        "...#.........",
    ],
    "BATTERY": [  # 18x9 body + 2x3 nub; fill is drawn at runtime
        ".#################..",
        "#.................#.",
        "#.................#.",
        "#.................##",
        "#.................##",
        "#.................##",
        "#.................#.",
        "#.................#.",
        ".#################..",
    ],
}


def _grid(w, h):
    return [["."] * w for _ in range(h)]


def _speaker(g, box_rows, flare):
    """Filled cone: a box on the left, then columns widening by one row up
    and down each."""
    top, bottom = box_rows
    for c in range(flare[0]):
        for r in range(top, bottom + 1):
            g[r][c] = "#"
    for i, c in enumerate(range(flare[0], flare[1] + 1)):
        for r in range(top - 1 - i, bottom + 2 + i):
            g[r][c] = "#"


def large_icons():
    w, h = 17, 12
    on, off = _grid(w, h), _grid(w, h)
    for g in (on, off):
        _speaker(g, (4, 7), (4, 7))
    # Waves: two arcs centered on the cone's mouth.
    for radius, (r0, r1) in ((3.5, (3, 8)), (7.5, (0, 11))):
        for r in range(r0, r1 + 1):
            dy = r - 5.5
            c = round(7 + (radius ** 2 - dy ** 2) ** 0.5) if abs(dy) <= radius else None
            if c is not None:
                on[r][min(c, w - 2)] = "#"
    for i in range(6):  # quiet time: X
        off[3 + i][10 + i] = "#"
        off[3 + i][15 - i] = "#"
    battery = _grid(27, 12)  # 25x12 body + 2x4 nub; fill drawn at runtime
    for c in range(1, 24):
        battery[0][c] = battery[11][c] = "#"
    for r in range(1, 11):
        battery[r][0] = battery[r][24] = "#"
    for r in range(4, 8):
        battery[r][25] = battery[r][26] = "#"
    rune = [  # Bluetooth rune, 9 wide; the X matches QUIET_ON's
        "....#....",
        "....##...",
        "....#.#..",
        ".#..#..#.",
        "..#.#.#..",
        "...###...",
        "...###...",
        "..#.#.#..",
        ".#..#..#.",
        "....#.#..",
        "....##...",
        "....#....",
    ]
    disconnected = _grid(w, h)
    for r, row in enumerate(rune):
        for c, ch in enumerate(row):
            disconnected[r][c] = ch
    for i in range(6):
        disconnected[3 + i][10 + i] = "#"
        disconnected[3 + i][15 - i] = "#"
    rows = lambda g: ["".join(row) for row in g]  # noqa: E731
    return {"QUIET_OFF": rows(on), "QUIET_ON": rows(off), "DISCONNECTED": rows(disconnected),
            "BATTERY": rows(battery)}


class Scale:
    def __init__(self, name, font_px, tracking, icons, suffix):
        self.name, self.font_px, self.tracking, self.icons = name, font_px, tracking, icons
        self.suffix = suffix  # resource file tag, e.g. "~emery"
        self.font = ImageFont.truetype(FONT_PATH, font_px)
        probe, _ = render(self, "H")
        self.cap_top = ink_rows(probe)[1]
        self.cap_h = ink_rows(probe)[3] - self.cap_top


PAD = 4  # left padding so glyphs that overhang their origin are not clipped


def render(sc, text, tracking=0):
    """Render text 1-bit on a canvas-oriented strip with its pen origin at
    x=PAD; returns (img, advance)."""
    font = sc.font
    if tracking:
        adv = sum(round(font.getlength(c)) for c in text) + tracking * (len(text) - 1)
    else:
        adv = round(font.getlength(text))
    img = Image.new("1", (adv + 2 * PAD, sc.font_px + 6), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    if tracking:
        x = PAD
        for c in text:
            d.text((x, 0), c, font=font, fill=0)
            x += round(font.getlength(c)) + tracking
    else:
        d.text((PAD, 0), text, font=font, fill=0)
    return img, adv


def ink_rows(img):
    inv = img.point(lambda p: 255 - p if img.mode == "L" else (0 if p else 255))
    box = inv.convert("L").point(lambda p: 255 if p else 0).getbbox()
    return box


def save_sheet(sc, sheet, name):
    sheet.save(os.path.join(IMG_DIR, name + sc.suffix + ".png"), optimize=True)
    sheet.transpose(Image.Transpose.ROTATE_90).save(
        os.path.join(IMG_DIR, name + "_portrait" + sc.suffix + ".png"), optimize=True)


def build_text_sheet(sc, name, items, tracking_for=None):
    """items: list of (c_name, text). Returns sheet metadata."""
    rendered = []
    top, bottom = None, None
    for c_name, text in items:
        trk = tracking_for(text) if tracking_for else 0
        img, adv = render(sc, text, trk)
        box = ink_rows(img)
        assert box is not None, text
        top = box[1] if top is None else min(top, box[1])
        bottom = box[3] if bottom is None else max(bottom, box[3])
        rendered.append((c_name, text, img, adv, box))

    band_h = bottom - top
    entries = []
    strips = []
    canvas_strips = []
    py = 0
    for c_name, text, img, adv, box in rendered:
        # Strip spans the pen box plus any ink overhanging it.
        x0 = min(PAD, box[0])
        x1 = max(PAD + adv, box[2])
        strip = img.crop((x0, top, x1, bottom))
        canvas_strips.append((strip, x0 - PAD))
        strips.append(strip.transpose(Image.Transpose.ROTATE_270))  # 90 deg clockwise
        entries.append({
            "name": c_name, "text": text, "py": py, "px": 0, "w": x1 - x0, "h": band_h,
            "ox": x0 - PAD, "adv": adv, "lsb": box[0] - PAD, "rsb": PAD + adv - box[2],
        })
        py += x1 - x0

    sheet = Image.new("1", (band_h, py), 1)
    y = 0
    for s in strips:
        sheet.paste(s, (0, y))
        y += s.height
    save_sheet(sc, sheet, name)
    return {"name": name, "band_h": band_h, "cap_offset": sc.cap_top - top,
            "entries": entries, "canvas_strips": canvas_strips}


def build_icon_sheet(sc):
    entries, strips = [], []
    py, max_h = 0, 0
    for name, rows in sc.icons.items():
        h, w = len(rows), len(rows[0])
        img = Image.new("1", (w, h), 1)
        for y, row in enumerate(rows):
            assert len(row) == w, name
            for x, ch in enumerate(row):
                if ch == "#":
                    img.putpixel((x, y), 0)
        strips.append(img.transpose(Image.Transpose.ROTATE_270))
        entries.append({"name": name, "py": py, "px": 0, "w": w, "h": h, "ox": 0, "adv": w,
                        "lsb": 0, "rsb": 0})
        py += w
        max_h = max(max_h, h)
    sheet = Image.new("1", (max_h, py), 1)
    y = 0
    for s in strips:
        sheet.paste(s, (0, y))
        y += s.height
    save_sheet(sc, sheet, "icons")
    color_entries = save_color_icons(sc, sheet, entries)
    return {"name": "icons", "entries": entries, "color_entries": color_entries}


# Color theme icon tints (exact Pebble 64-color palette values).
ICON_COLORS = {
    "QUIET_OFF": (0x00, 0x55, 0xAA),  # GColorCobaltBlue: dark enough to read on the gray
    "QUIET_ON": (0x00, 0x55, 0xAA),
    "DISCONNECTED": (0x00, 0x55, 0xAA),
    "BATTERY": (0x55, 0x55, 0x55),    # GColorDarkGray frame; fill drawn at runtime
}


# Icons that get a 1px white halo (outside only) in the color theme, so thin
# strokes stay legible on the gray status row.
HALO_ICONS = {"QUIET_OFF", "QUIET_ON", "DISCONNECTED", "BATTERY"}
# Haloed icons whose enclosed gaps are filled white as well (the Bluetooth
# rune's small holes); the battery's interior stays clear for its fill.
HALO_FILL_HOLES = {"DISCONNECTED"}


def save_color_icons(sc, sheet, entries):
    """Color theme icons: each icon's ink tinted, haloed icons ringed by one
    white pixel (8-neighbour), the rest transparent. Every icon gets a 1px
    margin for the halo, so this sheet has its own layout (returned as
    entries; drawn 1px up and left of the B/W icon position).
    Writes icons_color.png (+ _portrait), packed for color platforms only."""
    strips, color_entries = [], []
    py, max_h = 0, 0
    for e in entries:
        w, h = e["w"] + 2, e["h"] + 2
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        ink = set()
        # Entry sits h columns by w rows in the rotated sheet; read it back
        # in canvas orientation.
        for cy in range(e["h"]):
            for cx in range(e["w"]):
                if sheet.getpixel((e["px"] + e["h"] - 1 - cy, e["py"] + cx)) == 0:
                    ink.add((cx + 1, cy + 1))
        if e["name"] in HALO_ICONS:
            # Only on the outside: background reachable from the padded edge
            # (keeps the battery's interior clear for its fill).
            outside, todo = set(), [(0, 0)]
            while todo:
                p = todo.pop()
                if p in outside or p in ink or not (0 <= p[0] < w and 0 <= p[1] < h):
                    continue
                outside.add(p)
                todo += [(p[0] + 1, p[1]), (p[0] - 1, p[1]), (p[0], p[1] + 1), (p[0], p[1] - 1)]
            for (x, y) in ink:
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        if (x + dx, y + dy) in outside:
                            img.putpixel((x + dx, y + dy), (255, 255, 255, 255))
            if e["name"] in HALO_FILL_HOLES:
                for y in range(h):
                    for x in range(w):
                        if (x, y) not in ink and (x, y) not in outside:
                            img.putpixel((x, y), (255, 255, 255, 255))
        tint = ICON_COLORS[e["name"]] + (255,)
        for (x, y) in ink:
            img.putpixel((x, y), tint)
        strips.append(img.transpose(Image.Transpose.ROTATE_270))
        color_entries.append({"name": e["name"], "py": py, "px": 0, "w": w, "h": h,
                              "ox": -1, "adv": e["w"], "lsb": 0, "rsb": 0})
        py += w
        max_h = max(max_h, h)
    color = Image.new("RGBA", (max_h, py), (0, 0, 0, 0))
    y = 0
    for st in strips:
        color.paste(st, (0, y))
        y += st.height
    color.save(os.path.join(IMG_DIR, "icons_color" + sc.suffix + ".png"), optimize=True)
    color.transpose(Image.Transpose.ROTATE_90).save(
        os.path.join(IMG_DIR, "icons_color_portrait" + sc.suffix + ".png"), optimize=True)
    return color_entries


def c_ident(text):
    return "".join(c if c.isalnum() else "_" for c in text.upper().replace("'", ""))


def self_check(sc, sheets):
    """Composing entries by advance (black-only, like GCompOpAnd at runtime)
    must reproduce rendering the whole string in one go. That only holds
    where every advance is a whole number of pixels (16px, one brick per
    pixel); at other sizes the runtime's integer advances are the reference,
    so only the space advance is returned."""
    def table(key):
        return {e["text"]: (e, st) for e, st in zip(sheets[key]["entries"],
                                                    sheets[key]["canvas_strips"])}
    digits, words = table("digits"), table("words")
    space = round(sc.font.getlength(" "))
    if sc.font_px % 16:
        print(f"self-check ({sc.name}): skipped, {sc.font_px}px has fractional advances")
        return space

    def compose(tokens, band_key):
        band = sheets[band_key]["band_h"]
        width = sum(space if t == " " else (digits.get(t) or words[t])[0]["adv"] for t in tokens)
        out = Image.new("1", (width + 2 * PAD, band), 1)
        x = PAD
        for t in tokens:
            if t == " ":
                x += space
                continue
            key = "digits" if t in digits else "words"
            e, (strip, ox) = (digits if key == "digits" else words)[t]
            dy = sheets[band_key]["cap_offset"] - sheets[key]["cap_offset"]
            mask = strip.point(lambda p: 0 if p else 255).convert("L")
            out.paste(0, (x + ox, dy), mask)
            x += e["adv"]
        return out

    def reference(text, band_key):
        top = sc.cap_top - sheets[band_key]["cap_offset"]
        img, adv = render(sc, text)
        return img.crop((0, top, adv + 2 * PAD, top + sheets[band_key]["band_h"]))

    phrases = []
    for m in range(1, 60):
        if m < 10:
            phrases.append(["Oh", " ", ONES[m - 1]])
        elif m < 13:
            phrases.append([ONES[m - 1]])
        elif m < 20:
            phrases.append([TEENS[m - 13]])
        elif m % 10 == 0:
            phrases.append([TENS[m // 10 - 2]])
        else:
            phrases.append([TENS[m // 10 - 2], "-", ONES[m % 10 - 1]])
    checks = [(p, "words") for p in phrases]
    for h in range(24):
        for m in range(60):
            checks.append((list(f"{h % 12 or 12}:{m:02d}"), "digits"))
            checks.append((list(f"{h}:{m:02d}"), "digits"))
            checks.append((list(f"{m:02d}:{h:02d}"), "digits"))
            checks.append((list(f"-{m:02d}:{h:02d}"), "digits"))
    for mo in range(1, 13):
        for d in range(1, 32):
            checks.append((list(f"{mo}/{d}"), "digits"))
    bad = 0
    for tokens, band_key in checks:
        text = "".join(tokens)
        got, ref = compose(tokens, band_key), reference(text, band_key)
        if got.tobytes() != ref.tobytes():
            bad += 1
            print(f"compose mismatch: {text!r}", file=sys.stderr)
    if bad:
        raise SystemExit(f"{bad} composition mismatches")
    print(f"self-check ({sc.name}): {len(checks)} composed strings match direct rendering")
    return space


def write_header(scales):
    base = scales["base"][0]
    lines = [
        "// Generated by tools/gen_assets.py -- do not edit.",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "// Sprite sheet entry. w/h are the strip's size on the design canvas;",
        "// (px, py) is its top-left in the (pre-rotated) sheet bitmap, where it",
        "// occupies h columns by w rows. The strip starts ox columns from the",
        "// pen position and the pen then moves adv columns. Ink spans",
        "// [pen + lsb, pen + adv - rsb).",
        "typedef struct {",
        "  uint16_t py;",
        "  uint8_t px;",
        "  uint8_t w;",
        "  uint8_t h;",
        "  int8_t ox;",
        "  uint8_t adv;",
        "  int8_t lsb;",
        "  int8_t rsb;",
        "} SheetEntry;",
        "",
        "// The large set (NAME~emery.png) is what the SDK packs for the",
        "// 200x228 Pebble Time 2; every other target gets the base set.",
        "#if defined(PBL_DISPLAY_WIDTH) && PBL_DISPLAY_WIDTH >= 200",
        "#define ASSET_LARGE 1",
        "#else",
        "#define ASSET_LARGE 0",
        "#endif",
        "",
    ]
    # Entry order is the same at every scale, so the enums are shared.
    for key, prefix in (("digits", "DIGIT"), ("weekdays", "WDAY"), ("words", "WORD"),
                        ("icons", "ICON")):
        lines.append("enum {")
        for i, e in enumerate(base[key]["entries"]):
            lines.append(f"  {prefix}_{e['name']} = {i},")
        lines.append(f"  {prefix}_COUNT")
        lines.append("};")
        lines.append("")
    for scale_name, (sheets, space, sc) in scales.items():
        sfx = scale_name.upper()
        lines.append(f"// {scale_name}: Carthage Sans Bold {sc.font_px}px, cap height {sc.cap_h}")
        lines.append(f"#define ASSET_SPACE_ADVANCE_{sfx} {space}")
        for key, prefix in (("digits", "DIGIT"), ("weekdays", "WDAY"), ("words", "WORD")):
            lines.append(f"#define ASSET_{prefix}_CAP_OFFSET_{sfx} {sheets[key]['cap_offset']}"
                         "  // rows from band top to cap top")
        for key, prefix in (("digits", "DIGIT"), ("weekdays", "WDAY"), ("words", "WORD"),
                            ("icons", "ICON")):
            lines.append(f"static const SheetEntry ASSET_{prefix}_ENTRIES_{sfx}"
                         f"[{prefix}_COUNT] = {{")
            for e in sheets[key]["entries"]:
                label = e.get("text", e["name"])
                lines.append(f"  {{ {e['py']:4d}, {e['px']}, {e['w']:3d}, {e['h']:2d}, "
                             f"{e['ox']:2d}, {e['adv']:3d}, {e['lsb']:2d}, {e['rsb']:2d} }},"
                             f"  // {label}")
            lines.append("};")
        lines.append(f"// icons_color: 1px larger on every side; draw at (x - 1, y - 1).")
        lines.append(f"static const SheetEntry ASSET_ICON_COLOR_ENTRIES_{sfx}[ICON_COUNT] = {{")
        for e in sheets["icons"]["color_entries"]:
            lines.append(f"  {{ {e['py']:4d}, {e['px']}, {e['w']:3d}, {e['h']:2d}, "
                         f"{e['ox']:2d}, {e['adv']:3d}, {e['lsb']:2d}, {e['rsb']:2d} }},"
                         f"  // {e['name']}")
        lines.append("};")
        lines.append("")
    lines.append("#if ASSET_LARGE")
    names = ["SPACE_ADVANCE", "DIGIT_CAP_OFFSET", "WDAY_CAP_OFFSET", "WORD_CAP_OFFSET",
             "DIGIT_ENTRIES", "WDAY_ENTRIES", "WORD_ENTRIES", "ICON_ENTRIES",
             "ICON_COLOR_ENTRIES"]
    for n in names:
        lines.append(f"#define ASSET_{n} ASSET_{n}_LARGE")
    lines.append("#else")
    for n in names:
        lines.append(f"#define ASSET_{n} ASSET_{n}_BASE")
    lines.append("#endif")
    lines.append("")
    with open(HEADER, "w") as f:
        f.write("\n".join(lines))


def build_scale(sc):
    digit_names = {":": "COLON", "/": "SLASH", "-": "MINUS"}
    sheets = {
        "digits": build_text_sheet(sc, "digits",
                                   [(digit_names.get(c, c), c) for c in DIGIT_GLYPHS]),
        "weekdays": build_text_sheet(sc, "weekdays", [(w.upper(), w) for w in WEEKDAYS]),
        "words": build_text_sheet(sc, "words", [(c_ident(w), w) for w in WORDS],
                                  tracking_for=lambda t: sc.tracking if t in ("AM", "PM") else 0),
        "icons": build_icon_sheet(sc),
    }
    return sheets, self_check(sc, sheets), sc


def main():
    os.makedirs(IMG_DIR, exist_ok=True)
    # AM/PM tracking reproduces the mockup's letter-spacing: 0.15em.
    scales = {
        "base": build_scale(Scale("base", 16, 2, BASE_ICONS, "")),
        "large": build_scale(Scale("large", 22, 3, large_icons(), "~emery")),
    }
    write_header(scales)
    manifest = {name: {k: {kk: vv for kk, vv in v.items() if kk != "canvas_strips"}
                       for k, v in sheets.items()}
                for name, (sheets, _, _) in scales.items()}
    with open(os.path.join(IMG_DIR, "sheets.json"), "w") as f:
        json.dump(manifest, f, indent=1)
    files = [f for f in os.listdir(IMG_DIR) if f.endswith(".png")]
    total = sum(os.path.getsize(os.path.join(IMG_DIR, f)) for f in files)
    print(f"wrote {len(files)} sheets ({total} bytes PNG) and {os.path.relpath(HEADER, ROOT)}")


if __name__ == "__main__":
    main()
