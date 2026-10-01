#!/usr/bin/env python3
"""Build-time asset pipeline for the poddle watch face.

Renders every string the face can show with Carthage Sans Bold at its native
pixel size (16px: one FontStruct brick = one pixel, 1-bit, no antialiasing),
rotates each one 90 degrees clockwise, and packs each category into its own
sprite sheet:

  A  digits.png    0-9 : / -           (status time, date, progress labels)
  B  weekdays.png  Mo Tu We Th Fr Sa Su
  C  words.png     spoken-time vocabulary + AM/PM
  -  icons.png     quiet-time on/off speaker, battery outline

Each sheet is also written upright as NAME_portrait.png (the rotated sheet
rotated back) for the portrait orientation, which draws the same entries
without any rotation. src/c/assets.h describes where each entry lives.

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
FONT_PX = 16  # 1024 units/em, 64-unit bricks -> 16px puts one brick on one pixel
IMG_DIR = os.path.join(ROOT, "resources", "images")
HEADER = os.path.join(ROOT, "src", "c", "assets.h")

AMPM_TRACKING = 2  # extra px between letters, the mockup's letter-spacing: 0.15em

DIGIT_GLYPHS = list("0123456789:/-")
WEEKDAYS = ["Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"]  # struct tm tm_wday order
ONES = ["One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine",
        "Ten", "Eleven", "Twelve"]
TEENS = ["Thirteen", "Fourteen", "Fifteen", "Sixteen", "Seventeen", "Eighteen",
         "Nineteen"]
TENS = ["Twenty", "Thirty", "Forty", "Fifty"]
WORDS = ONES + TEENS + TENS + ["Oh", "O'Clock", "AM", "PM"]

# Canvas-orientation pixel art, '#' = black.
ICONS = {
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

font = ImageFont.truetype(FONT_PATH, FONT_PX)
BASELINE = font.getmetrics()[0]  # ascent in px; text drawn at y=0 has its baseline here
CAP_TOP = BASELINE - 9  # 9-brick cap height


PAD = 4  # left padding so glyphs that overhang their origin are not clipped


def render(text, tracking=0):
    """Render text 1-bit on a canvas-oriented strip with its pen origin at
    x=PAD; returns (img, advance)."""
    if tracking:
        adv = sum(int(font.getlength(c)) for c in text) + tracking * (len(text) - 1)
    else:
        adv = int(font.getlength(text))
    img = Image.new("1", (adv + 2 * PAD, FONT_PX + 4), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    if tracking:
        x = PAD
        for c in text:
            d.text((x, 0), c, font=font, fill=0)
            x += int(font.getlength(c)) + tracking
    else:
        d.text((PAD, 0), text, font=font, fill=0)
    return img, adv


def ink_rows(img):
    inv = img.point(lambda p: 255 - p if img.mode == "L" else (0 if p else 255))
    box = inv.convert("L").point(lambda p: 255 if p else 0).getbbox()
    return box


def save_sheet(sheet, name):
    sheet.save(os.path.join(IMG_DIR, name + ".png"), optimize=True)
    sheet.transpose(Image.Transpose.ROTATE_90).save(
        os.path.join(IMG_DIR, name + "_portrait.png"), optimize=True)


def build_text_sheet(name, items, tracking_for=None):
    """items: list of (c_name, text). Returns sheet metadata."""
    rendered = []
    top, bottom = None, None
    for c_name, text in items:
        trk = tracking_for(text) if tracking_for else 0
        img, adv = render(text, trk)
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
    save_sheet(sheet, name)
    return {"name": name, "band_h": band_h, "cap_offset": CAP_TOP - top,
            "entries": entries, "canvas_strips": canvas_strips}


def build_icon_sheet():
    entries, strips = [], []
    py, max_h = 0, 0
    for name, rows in ICONS.items():
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
    save_sheet(sheet, "icons")
    return {"name": "icons", "entries": entries}


def c_ident(text):
    return "".join(c if c.isalnum() else "_" for c in text.upper().replace("'", ""))


def self_check(sheets):
    """Composing entries by advance (black-only, like GCompOpAnd at runtime)
    must reproduce rendering the whole string in one go."""
    def table(key):
        return {e["text"]: (e, st) for e, st in zip(sheets[key]["entries"],
                                                    sheets[key]["canvas_strips"])}
    digits, words = table("digits"), table("words")
    space = int(font.getlength(" "))

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
        top = CAP_TOP - sheets[band_key]["cap_offset"]
        img, adv = render(text)
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
    print(f"self-check: {len(checks)} composed strings match direct rendering")
    return space


def write_header(sheets, space):
    lines = [
        "// Generated by tools/gen_assets.py -- do not edit.",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "// Sprite sheet entry. w/h are the strip's size on the 168x144 design",
        "// canvas; (px, py) is its top-left in the (pre-rotated) sheet bitmap,",
        "// where it occupies h columns by w rows. The strip starts ox columns",
        "// from the pen position and the pen then moves adv columns. Ink spans",
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
        f"#define ASSET_SPACE_ADVANCE {space}",
        f"#define ASSET_AMPM_TRACKING {AMPM_TRACKING}",
        "",
    ]
    for key, prefix in (("digits", "DIGIT"), ("weekdays", "WDAY"), ("words", "WORD")):
        s = sheets[key]
        lines.append(f"// {key}.png")
        lines.append(f"#define ASSET_{prefix}_BAND_H {s['band_h']}")
        lines.append(f"#define ASSET_{prefix}_CAP_OFFSET {s['cap_offset']}"
                     "  // rows from band top to cap top")
        lines.append("enum {")
        for i, e in enumerate(s["entries"]):
            lines.append(f"  {prefix}_{e['name']} = {i},")
        lines.append(f"  {prefix}_COUNT")
        lines.append("};")
        lines.append("")
    lines.append("// icons.png")
    lines.append("enum {")
    for i, e in enumerate(sheets["icons"]["entries"]):
        lines.append(f"  ICON_{e['name']} = {i},")
    lines.append("  ICON_COUNT")
    lines.append("};")
    lines.append("")
    for key, prefix in (("digits", "DIGIT"), ("weekdays", "WDAY"), ("words", "WORD"),
                        ("icons", "ICON")):
        lines.append(f"static const SheetEntry ASSET_{prefix}_ENTRIES[{prefix}_COUNT] = {{")
        for e in sheets[key]["entries"]:
            label = e.get("text", e["name"])
            lines.append(f"  {{ {e['py']:4d}, {e['px']}, {e['w']:3d}, {e['h']:2d}, "
                         f"{e['ox']:2d}, {e['adv']:3d}, {e['lsb']:2d}, {e['rsb']:2d} }},"
                         f"  // {label}")
        lines.append("};")
        lines.append("")
    with open(HEADER, "w") as f:
        f.write("\n".join(lines))


def main():
    os.makedirs(IMG_DIR, exist_ok=True)
    digit_names = {":": "COLON", "/": "SLASH", "-": "MINUS"}
    sheets = {
        "digits": build_text_sheet("digits", [(digit_names.get(c, c), c) for c in DIGIT_GLYPHS]),
        "weekdays": build_text_sheet("weekdays", [(w.upper(), w) for w in WEEKDAYS]),
        "words": build_text_sheet("words", [(c_ident(w), w) for w in WORDS],
                                  tracking_for=lambda t: AMPM_TRACKING if t in ("AM", "PM") else 0),
        "icons": build_icon_sheet(),
    }
    space = self_check(sheets)
    write_header(sheets, space)
    manifest = {k: {kk: vv for kk, vv in v.items() if kk != "canvas_strips"}
                for k, v in sheets.items()}
    with open(os.path.join(IMG_DIR, "sheets.json"), "w") as f:
        json.dump(manifest, f, indent=1)
    total = sum(os.path.getsize(os.path.join(IMG_DIR, n + sfx + ".png"))
                for n in ("digits", "weekdays", "words", "icons") for sfx in ("", "_portrait"))
    print(f"wrote 4 sheets x 2 orientations ({total} bytes PNG) and "
          f"{os.path.relpath(HEADER, ROOT)}")


if __name__ == "__main__":
    main()
