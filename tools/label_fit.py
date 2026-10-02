#!/usr/bin/env python3
"""Measures whether the two progress labels fit side by side, from the
digit sprite metrics in src/c/assets.h (the same ink rules as
canvas_draw_left/right): left label ink starts at MARGIN, right label ink
ends at W - MARGIN, and the gap between them must stay positive.

Checks every label pair the custom period can show in Elapsed / remaining
(every second of every start/end pair with whole minutes is too many, so
it walks every span length 1..1439 minutes and every elapsed second; the
labels depend only on those), plus the existing hour/minute labels.

  python3 tools/label_fit.py
"""

import operator
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHARS = "0123456789:/-"
# (name, canvas widths [portrait, landscape], text margin)
SETS = [("BASE", (144, 168), 8), ("LARGE", (200, 228), 11)]


def metrics(name):
    src = open(os.path.join(ROOT, "src", "c", "assets.h")).read()
    block = re.search(r"ASSET_DIGIT_ENTRIES_%s\[DIGIT_COUNT\] = \{(.*?)\};" % name, src, re.S)[1]
    rows = re.findall(r"\{([^}]*)\}", block)
    out = {}
    for ch, row in zip(CHARS, rows):
        py, px, w, h, ox, adv, lsb, rsb = map(int, row.split(","))
        out[ch] = (adv, lsb, rsb)
    return out


def ink(m, text):
    adv = sum(m[c][0] for c in text)
    return adv - m[text[0]][1] - m[text[-1]][2]


def duration(sign, sec, hour_seconds):
    # Same as format_duration(): H:MM:SS in landscape, H:MM in portrait.
    if sec < 3600:
        return f"{sign}{sec // 60:02d}:{sec % 60:02d}"
    if not hour_seconds:
        return f"{sign}{sec // 3600}:{sec // 60 % 60:02d}"
    return f"{sign}{sec // 3600}:{sec // 60 % 60:02d}:{sec % 60:02d}"


def main():
    for name, widths, margin in SETS:
        m = metrics(name)
        existing = max(ink(m, a) + ink(m, b) for a, b in
                       [(f"{e // 60:02d}:{e % 60:02d}", f"-{(3600 - e) // 60:02d}:{(3600 - e) % 60:02d}")
                        for e in range(3600)])
        for orient, w in zip(("portrait", "landscape"), widths):
            # Widest pair for each span: the labels only depend on (span, elapsed).
            hs = orient == "landscape"
            n = 1439 * 60 + 1
            lw = [ink(m, duration("", s, hs)) for s in range(n)]
            rw = [ink(m, duration("-", s, hs)) for s in range(n)]
            worst = {}
            for span_min in range(1, 1440):
                span = span_min * 60
                totals = list(map(operator.add, lw[:span], rw[span:0:-1]))
                total = max(totals)
                e = totals.index(total)
                worst[span_min] = (total, duration("", e, hs), duration("-", span - e, hs))
            room = w - 2 * margin
            gap_existing = room - existing
            bad = [s for s, (t, _, _) in worst.items() if room - t <= 0]
            tight = [s for s, (t, _, _) in worst.items() if 0 < room - t < gap_existing]
            top = max(worst.values())
            print(f"{name} {orient} {w}px: room {room}px; hour-mode labels leave {gap_existing}px")
            print(f"  widest period pair {top[1]!r} + {top[2]!r}: {top[0]}px, gap {room - top[0]}px")
            if bad:
                print(f"  OVERLAP for spans {min(bad)}..{max(bad)} min ({len(bad)} span lengths)")
            else:
                print("  no overlap for any span")
            if tight:
                print(f"  tighter than today's labels for {len(tight)} span lengths "
                      f"({min(tight)}..{max(tight)} min)")


if __name__ == "__main__":
    main()
