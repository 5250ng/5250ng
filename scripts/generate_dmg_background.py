#!/usr/bin/env python3
"""Render the macOS disk image background used by scripts/package_macos.sh.

The background is drawn once at a high resolution and downsampled to the 1x
(660x400, 72 DPI) and 2x (1320x800, 144 DPI) images that dmgbuild combines
into a multi-resolution TIFF. Finder draws the application and Applications
icons on top, so the image only carries the backdrop, the arrow between the
two icon slots and the install hint.

The rendered PNGs are checked in, so this script only needs to be re-run
when the artwork changes:

    python3 scripts/generate_dmg_background.py [--font /path/to/Sans.ttf]

Requires Pillow.
"""

import argparse
import os
import sys

from PIL import Image, ImageDraw, ImageFont

# Must match window and icon geometry in resources/macos/dmg/dmg_settings.py.
WIDTH, HEIGHT = 660, 400
ICON_SIZE = 160
APP_CENTER = (165, 180)
APPLICATIONS_CENTER = (495, 180)

# Supersampling factor relative to 1x, for antialiased edges.
SUPERSAMPLE = 4

BACKGROUND_TOP = (246, 247, 248)
BACKGROUND_BOTTOM = (226, 230, 234)
ARROW_COLOR = (43, 48, 54)
HINT_COLOR = (74, 80, 88)
# Echoes the status bar of the application icon.
ACCENT_RED = (232, 38, 43)

HINT_TEXT = "Drag 5250ng to Applications to install"

FONT_CANDIDATES = [
    "/System/Library/Fonts/SFNS.ttf",
    "/System/Library/Fonts/Helvetica.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
]


def find_font(explicit):
    if explicit:
        return explicit
    for candidate in FONT_CANDIDATES:
        if os.path.isfile(candidate):
            return candidate
    sys.exit("error: no usable font found; pass --font")


def draw_background(scale, font_path):
    width, height = WIDTH * scale, HEIGHT * scale
    image = Image.new("RGB", (width, height))
    draw = ImageDraw.Draw(image)

    for y in range(height):
        t = y / (height - 1)
        color = tuple(
            round(top + (bottom - top) * t)
            for top, bottom in zip(BACKGROUND_TOP, BACKGROUND_BOTTOM)
        )
        draw.line([(0, y), (width, y)], fill=color)

    # Arrow between the two icon slots, leaving a gap around each icon.
    gap = 28
    start_x = (APP_CENTER[0] + ICON_SIZE / 2 + gap) * scale
    end_x = (APPLICATIONS_CENTER[0] - ICON_SIZE / 2 - gap) * scale
    center_y = APP_CENTER[1] * scale
    shaft = 7 * scale
    head_length = 26 * scale
    head_half_height = 22 * scale

    draw.rounded_rectangle(
        [start_x, center_y - shaft, end_x - head_length + shaft, center_y + shaft],
        radius=shaft,
        fill=ARROW_COLOR,
    )
    draw.polygon(
        [
            (end_x - head_length, center_y - head_half_height),
            (end_x, center_y),
            (end_x - head_length, center_y + head_half_height),
        ],
        fill=ARROW_COLOR,
    )

    font = ImageFont.truetype(font_path, 15 * scale)
    hint_y = 340 * scale
    draw.text(
        (width / 2, hint_y), HINT_TEXT, font=font, fill=HINT_COLOR, anchor="mm"
    )

    draw.rectangle([0, height - 4 * scale, width, height], fill=ACCENT_RED)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--font", help="TrueType font for the install hint")
    parser.add_argument(
        "--output-dir",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "..",
            "resources",
            "macos",
            "dmg",
        ),
    )
    args = parser.parse_args()

    font_path = find_font(args.font)
    master = draw_background(SUPERSAMPLE * 2, font_path)
    os.makedirs(args.output_dir, exist_ok=True)

    for factor, suffix, dpi in ((1, "", 72), (2, "@2x", 144)):
        size = (WIDTH * factor, HEIGHT * factor)
        output = os.path.join(args.output_dir, f"background{suffix}.png")
        master.resize(size, Image.LANCZOS).save(
            output, dpi=(dpi, dpi), optimize=True
        )
        print(f"wrote {os.path.normpath(output)} ({size[0]}x{size[1]} @ {dpi} DPI)")


if __name__ == "__main__":
    main()
