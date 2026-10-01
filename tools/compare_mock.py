#!/usr/bin/env python3
"""Compares an emulator canvas screenshot against the HTML mockup render.

Both images are 168x144 canvas views (tools/render_mock.js and
tools/screenshot.sh). The mockup uses Courier as a stand-in font while the
watch uses Carthage Sans, so glyph shapes differ by design; what is compared
is where each element's ink sits. For every row band, ink is split into
elements on horizontal gaps and their bounding boxes are reported side by
side. Also writes OUT_overlay.png: mockup-only ink in red, watch-only in
blue, shared ink in black (scaled 4x).

  python3 tools/compare_mock.py mock_canvas.png watch_canvas.png OUT
"""

import sys

from PIL import Image

BANDS = [
    ("status", 0, 32),
    ("separator", 33, 33),
    ("date", 34, 49),
    ("hour", 50, 70),
    ("minute", 71, 90),
    ("ampm", 91, 110),
    ("track", 111, 121),
    ("labels", 122, 143),
]
GAP = 7  # px of empty columns that separate two elements in a band


def ink(path):
    im = Image.open(path).convert("L")
    assert im.size == (168, 144), (path, im.size)
    # The emulator shows white as light gray; the mockup is antialiased.
    return [[im.getpixel((x, y)) < 110 for x in range(168)] for y in range(144)]


def elements(mask, y0, y1):
    cols = [any(mask[y][x] for y in range(y0, y1 + 1)) for x in range(168)]
    out, x = [], 0
    while x < 168:
        if not cols[x]:
            x += 1
            continue
        start, last = x, x
        while x < 168 and (cols[x] or x - last <= GAP):
            if cols[x]:
                last = x
            x += 1
        ys = [y for y in range(y0, y1 + 1) if any(mask[y][c] for c in range(start, last + 1))]
        out.append((start, ys[0], last, ys[-1]))
    return out


def main():
    mock, watch, out = ink(sys.argv[1]), ink(sys.argv[2]), sys.argv[3]
    worst = 0
    for name, y0, y1 in BANDS:
        a, b = elements(mock, y0, y1), elements(watch, y0, y1)
        print(f"{name:9s} mock  {a}")
        print(f"{'':9s} watch {b}")
        if len(a) != len(b):
            print(f"{'':9s} ELEMENT COUNT DIFFERS")
            worst = max(worst, 99)
            continue
        for ea, eb in zip(a, b):
            # Anchor by how the mockup lays the element out: left margin,
            # right margin, or centered on its own width.
            if ea[0] <= 20 and ea[2] < 150:
                d = abs(ea[0] - eb[0])
            elif ea[2] >= 150 and ea[0] > 20:
                d = abs(ea[2] - eb[2])
            elif ea[0] <= 20:
                d = max(abs(ea[0] - eb[0]), abs(ea[2] - eb[2]))
            else:
                d = abs((ea[0] + ea[2]) - (eb[0] + eb[2])) / 2
            top = abs((ea[1] + ea[3]) - (eb[1] + eb[3])) / 2  # vertical center
            print(f"{'':9s} offset x {d:g}  y {top:g}  ({ea} -> {eb})")
            worst = max(worst, d, top)
    print(f"max anchor/top offset: {worst}px")

    img = Image.new("RGB", (168, 144), "white")
    for y in range(144):
        for x in range(168):
            m, w = mock[y][x], watch[y][x]
            if m and w:
                img.putpixel((x, y), (0, 0, 0))
            elif m:
                img.putpixel((x, y), (230, 60, 60))
            elif w:
                img.putpixel((x, y), (40, 90, 230))
    img.resize((168 * 4, 144 * 4), Image.NEAREST).save(out + "_overlay.png")
    side = Image.new("RGB", (168 * 2 + 8, 144), (200, 200, 200))
    side.paste(Image.open(sys.argv[1]).convert("RGB"), (0, 0))
    side.paste(Image.open(sys.argv[2]).convert("RGB"), (176, 0))
    side.resize((side.width * 3, side.height * 3), Image.NEAREST).save(out + "_side.png")


if __name__ == "__main__":
    main()
