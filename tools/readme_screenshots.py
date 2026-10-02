#!/usr/bin/env python3
"""Makes the README screenshots from the store screenshots.

Store screenshots are the physical screen, so a landscape face lies on its
side. This turns the landscape ones upright (the canvas view, the same
rotation tools/screenshot.sh uses), copies the portrait ones as they are,
and writes them to docs/readme/.

  python3 tools/readme_screenshots.py
"""

import os

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = {
    # output name: (store screenshot, landscape?)
    "time2_landscape_bw.png": ("emery_2_landscape.png", True),
    "time2_landscape_color.png": ("emery_4_color_landscape.png", True),
    "original_portrait.png": ("aplite_1_portrait.png", False),
}


def main():
    out_dir = os.path.join(ROOT, "docs", "readme")
    os.makedirs(out_dir, exist_ok=True)
    for name, (src, landscape) in SHOTS.items():
        im = Image.open(os.path.join(ROOT, "store", "screenshots", src))
        if landscape:
            # Physical (px, py) shows canvas (py, W - 1 - px): rotate 90 deg counter-clockwise.
            im = im.transpose(Image.Transpose.ROTATE_90)
        im.save(os.path.join(out_dir, name))
        print(f"docs/readme/{name} <- store/screenshots/{src}")


if __name__ == "__main__":
    main()
