#!/usr/bin/env python3
"""Compares an emulator canvas screenshot against the HTML mockup render.

Both images are canvas views of the same orientation, 144x168 portrait or
168x144 landscape (tools/render_mock.js and tools/screenshot.sh). The
mockup uses Courier as a stand-in font while the watch uses Carthage Sans,
so glyph shapes differ by design; what is compared is where each element's
ink sits. For every row band, ink is split into
elements on horizontal gaps and their bounding boxes are reported side by
side. Also writes OUT_overlay.png: mockup-only ink in red, watch-only in
blue, shared ink in black (scaled 4x).

  python3 tools/compare_mock.py mock_canvas.png watch_canvas.png OUT
"""

import sys

from PIL import Image

TOP_BANDS = [("status", 0, 32), ("separator", 33, 33), ("date", 34, 49)]
BANDS = {
    (168, 144): TOP_BANDS + [("hour", 50, 70), ("minute", 71, 90), ("ampm", 91, 110),
                             ("track", 111, 121), ("labels", 122, 143)],
    (144, 168): TOP_BANDS + [("hour", 50, 81), ("minute", 82, 101), ("ampm", 102, 134),
                             ("track", 135, 145), ("labels", 146, 167)],
}
GAP = 7  # px of empty columns that separate two elements in a band
# The status row and separator were deliberately moved up from the mockup
# (see README, layout values); they are reported but not scored.
UNSCORED = {"status", "separator"}


def ink(path):
    im = Image.open(path).convert("L")
    assert im.size in BANDS, (path, im.size)
    # The emulator shows white as light gray; the mockup is antialiased.
    w, h = im.size
    return [[im.getpixel((x, y)) < 110 for x in range(w)] for y in range(h)]


def elements(mask, y0, y1):
    width = len(mask[0])
    cols = [any(mask[y][x] for y in range(y0, y1 + 1)) for x in range(width)]
    out, x = [], 0
    while x < width:
        if not cols[x]:
            x += 1
            continue
        start, last = x, x
        while x < width and (cols[x] or x - last <= GAP):
            if cols[x]:
                last = x
            x += 1
        ys = [y for y in range(y0, y1 + 1) if any(mask[y][c] for c in range(start, last + 1))]
        out.append((start, ys[0], last, ys[-1]))
    return out


def main():
    mock, watch, out = ink(sys.argv[1]), ink(sys.argv[2]), sys.argv[3]
    w, h = len(mock[0]), len(mock)
    assert (len(watch[0]), len(watch)) == (w, h), "orientation mismatch"
    cx, right = w // 2, w - 18
    worst = 0
    for name, y0, y1 in BANDS[(w, h)]:
        a, b = elements(mock, y0, y1), elements(watch, y0, y1)
        print(f"{name:9s} mock  {a}")
        print(f"{'':9s} watch {b}")
        if name in UNSCORED:
            print(f"{'':9s} (not scored: moved on purpose)")
            continue
        if len(a) != len(b):
            print(f"{'':9s} ELEMENT COUNT DIFFERS")
            worst = max(worst, 99)
            continue
        for ea, eb in zip(a, b):
            # Anchor by how the mockup lays the element out: left margin,
            # right margin, or centered on its own width.
            if ea[0] <= 20 and ea[2] < right:
                d = abs(ea[0] - eb[0])
            elif ea[2] >= right and ea[0] > 20:
                d = abs(ea[2] - eb[2])
            elif ea[0] <= 20:
                d = max(abs(ea[0] - eb[0]), abs(ea[2] - eb[2]))
            else:
                d = abs((ea[0] + ea[2]) - (eb[0] + eb[2])) / 2
            top = abs((ea[1] + ea[3]) - (eb[1] + eb[3])) / 2  # vertical center
            print(f"{'':9s} offset x {d:g}  y {top:g}  ({ea} -> {eb})")
            worst = max(worst, d, top)
    print(f"max anchor/top offset (date row and below): {worst}px")

    img = Image.new("RGB", (w, h), "white")
    for y in range(h):
        for x in range(w):
            in_mock, in_watch = mock[y][x], watch[y][x]
            if in_mock and in_watch:
                img.putpixel((x, y), (0, 0, 0))
            elif in_mock:
                img.putpixel((x, y), (230, 60, 60))
            elif in_watch:
                img.putpixel((x, y), (40, 90, 230))
    img.resize((w * 4, h * 4), Image.NEAREST).save(out + "_overlay.png")
    side = Image.new("RGB", (w * 2 + 8, h), (200, 200, 200))
    side.paste(Image.open(sys.argv[1]).convert("RGB"), (0, 0))
    side.paste(Image.open(sys.argv[2]).convert("RGB"), (w + 8, 0))
    side.resize((side.width * 3, side.height * 3), Image.NEAREST).save(out + "_side.png")


if __name__ == "__main__":
    main()
