#!/usr/bin/env python3
"""Draws the app icon: 罪 (tsumi, "sin", the title's first character) as a
black glyph with a green glow on a dark rounded square, after the title
logo's colors.

    python3 tools/make_app_icon.py DelaGothicOne-Regular.ttf

The font is Dela Gothic One (SIL OFL 1.1, github.com/syakuzen/DelaGothic,
also on Google Fonts); only the rendered image is kept. Writes
icons/app.png (256 px, the window icon) and icons/app.ico (16 - 256 px).
icons/app.res (the .exe icon) is then compiled from the .ico with llvm-rc;
see the commands at the end. Needs Pillow.
"""
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

GLYPH = "罪"
SIZE = 1024  # drawn large, then scaled down


def draw(font_path):
    font = ImageFont.truetype(font_path, 680)
    mask = Image.new("L", (SIZE, SIZE), 0)
    d = ImageDraw.Draw(mask)
    left, top, right, bottom = d.textbbox((0, 0), GLYPH, font=font)
    d.text(((SIZE - (right - left)) / 2 - left, (SIZE - (bottom - top)) / 2 - top), GLYPH, font=font, fill=255)

    # Dark rounded square with a faint green center.
    shape = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(shape).rounded_rectangle((16, 16, SIZE - 16, SIZE - 16), radius=180, fill=255)
    center = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(center).ellipse((SIZE * 0.15, SIZE * 0.15, SIZE * 0.85, SIZE * 0.85), fill=90)
    center = center.filter(ImageFilter.GaussianBlur(160))
    image = Image.composite(Image.new("RGBA", (SIZE, SIZE), (20, 80, 30, 255)),
                            Image.new("RGBA", (SIZE, SIZE), (4, 10, 5, 255)), center)

    # Glow: a wide soft green halo and a tight bright rim, then the black glyph.
    halo = mask.filter(ImageFilter.MaxFilter(25)).filter(ImageFilter.GaussianBlur(40))
    halo = halo.point(lambda v: min(255, int(v * 1.6)))
    rim = mask.filter(ImageFilter.MaxFilter(9)).filter(ImageFilter.GaussianBlur(8))
    image = Image.composite(Image.new("RGBA", (SIZE, SIZE), (40, 255, 90, 255)), image, halo)
    image = Image.composite(Image.new("RGBA", (SIZE, SIZE), (170, 255, 170, 255)), image, rim)
    image = Image.composite(Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 255)), image, mask)
    image.putalpha(ImageChops.multiply(image.getchannel("A"), shape))
    return image


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    icons = Path(__file__).resolve().parent.parent / "icons"
    icons.mkdir(exist_ok=True)
    image = draw(sys.argv[1])
    image.resize((256, 256), Image.LANCZOS).save(icons / "app.png")
    image.resize((256, 256), Image.LANCZOS).save(
        icons / "app.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print("Wrote icons/app.png and icons/app.ico. For icons/app.res:")
    print('  cd icons && printf \'1 ICON "app.ico"\\n\' > app.rc && llvm-rc /FO app.res app.rc && rm app.rc')


if __name__ == "__main__":
    main()
