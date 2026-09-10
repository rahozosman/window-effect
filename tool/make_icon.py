"""Turns the master artwork into the Windows application icon.

One source image, one generated output: `windows/runner/resources/app_icon.ico`,
which is also what the tray icon and the title bar use (IDI_APP_ICON). Run it
again whenever `assets/icon/app_icon.png` changes:

    python tool/make_icon.py [source.png]

The artwork arrives as a square tile on a black field. Icons are composited
against whatever the user's taskbar and title bar happen to be, so the field
has to go: the script finds the tile, crops to it, and rounds the corners with
an anti-aliased mask so the black outside the tile becomes transparent while
the black *inside* the glass stays exactly as drawn.
"""

import os
import sys

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MASTER = os.path.join(ROOT, "assets", "icon", "app_icon.png")
TARGET = os.path.join(ROOT, "windows", "runner", "resources", "app_icon.ico")

# Everything Windows asks for, from the notification area (16) to the Start
# menu and Alt-Tab (256).
SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]

# The corner radius of the tile in the artwork, as a fraction of its width.
CORNER = 0.225

# Anything above this counts as tile rather than background when looking for
# the edges. Low enough to include the glow, high enough to ignore the noise
# in a black field.
EDGE_THRESHOLD = 24


def tile_bounds(image):
    """The bounding box of the artwork, glow included."""
    grey = image.convert("L")
    mask = grey.point(lambda value: 255 if value > EDGE_THRESHOLD else 0)
    box = mask.getbbox()
    return box if box is not None else (0, 0, image.width, image.height)


def square(box, width, height):
    """Grows |box| into a square that still fits inside the image."""
    left, top, right, bottom = box
    size = max(right - left, bottom - top)
    centre_x = (left + right) // 2
    centre_y = (top + bottom) // 2
    left = max(0, min(centre_x - size // 2, width - size))
    top = max(0, min(centre_y - size // 2, height - size))
    return (left, top, left + size, top + size)


def rounded(image, radius_fraction):
    """Applies an anti-aliased rounded-square alpha mask."""
    # Drawn at 4x and downsampled: PIL's rounded_rectangle has no anti-aliasing
    # of its own, and a hard-edged mask on a glass icon looks like a mistake.
    scale = 4
    big = Image.new("L", (image.width * scale, image.height * scale), 0)
    ImageDraw.Draw(big).rounded_rectangle(
        (0, 0, big.width - 1, big.height - 1),
        radius=int(big.width * radius_fraction),
        fill=255,
    )
    mask = big.resize(image.size, Image.LANCZOS)

    out = image.convert("RGBA")
    # Multiply rather than replace, so anything already transparent stays so.
    alpha = out.getchannel("A").point(lambda value: value)
    out.putalpha(Image.composite(alpha, Image.new("L", out.size, 0), mask))
    return out


def main():
    source_path = sys.argv[1] if len(sys.argv) > 1 else MASTER
    if not os.path.exists(source_path):
        raise SystemExit("no source artwork at %s" % source_path)

    source = Image.open(source_path).convert("RGBA")
    box = square(tile_bounds(source), source.width, source.height)
    tile = source.crop(box).resize((1024, 1024), Image.LANCZOS)
    tile = rounded(tile, CORNER)

    os.makedirs(os.path.dirname(MASTER), exist_ok=True)
    tile.save(MASTER)

    os.makedirs(os.path.dirname(TARGET), exist_ok=True)
    tile.save(TARGET, format="ICO", sizes=[(s, s) for s in SIZES])

    print("cropped to %s" % (box,))
    print("master  %s" % MASTER)
    print("icon    %s (%s)" % (TARGET, ", ".join(str(s) for s in SIZES)))


if __name__ == "__main__":
    main()
