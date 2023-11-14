#!/usr/bin/env python3

from PIL import Image, ImageFont, ImageDraw
import os
import os.path

BUF = Image.new("RGBA", (48, 48), (255, 255, 255))
BUF_DRW = ImageDraw.Draw(BUF)
BUF_DRW.fontmode = "1"

FONT_PATH = os.environ["FONT_PATH"]


def draw_font(name: str, *, size: int) -> None:
    characters = "".join(chr(x) for x in range(ord("!"), ord("~") + 1))

    font_fname = next(
        filter(
            os.path.exists,
            (f"/{FONT_PATH}/{name}.{ext}" for ext in ["ttf", "TTF", "otf"]),
        ),
    )
    fnt = ImageFont.truetype(font_fname, size)

    BUF.paste((0, 0, 0, 0), (0, 0, BUF.width, BUF.height))
    for char in characters:
        BUF_DRW.text((16, 16), char, (255, 255, 255, 255), font=fnt)
    bbox = BUF.getbbox()
    assert bbox is not None
    xoff = 16 - bbox[0]
    yoff = 16 - bbox[1]
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]

    preview_height = 6 * (h + 1)
    preview = Image.new(
        "RGBA", (16 * (w + 1) + 1, 2 * preview_height + h + 1), (255, 255, 255)
    )
    drw = ImageDraw.Draw(preview)

    for i, char in enumerate(characters, 1):
        px = (i % 16) * (w + 1) + 1
        py = (i // 16) * (h + 1) + 1
        drw.rectangle((px, py, px + w - 1, py + h - 1), (192, 192, 192))
        drw.rectangle(
            (px, py + preview_height + h, px + w - 1, py + h - 1 + preview_height + h),
            (192, 192, 192),
        )
        drw.fontmode = "1"
        drw.text((px + xoff, py + yoff), char, (0, 0, 0), font=fnt)
        drw.fontmode = "L"
        drw.text((px + xoff, py + yoff + preview_height + h), char, (0, 0, 0), font=fnt)
    preview.save(f"build/fontgen/{name}.png")

    FONTS_C.write(f'    {{.name = "{name}",\n')
    FONTS_C.write(f"     .width = {w},\n")
    FONTS_C.write(f"     .height = {h},\n")
    FONTS_C.write("     .flags = 0,\n")
    FONTS_C.write("     .data =\n")
    FONTS_C.write("         (unsigned char[]){\n")
    value = 0
    shift = 0
    for char in characters:
        BUF.paste((255, 255, 255), (0, 0, BUF.width, BUF.height))
        BUF_DRW.text((xoff, yoff), char, (0, 0, 0), font=fnt)

        for y in range(h):
            for x in range(w):
                rgb = BUF.getpixel((x, y))
                if rgb[0] == 0:
                    value |= 1 << shift
                shift += 1
    data = bytes((value >> (i * 8)) & 0xFF for i in range((shift + 7) // 8))
    for i in range(0, len(data), 11):
        FONTS_C.write("             ")
        FONTS_C.write(", ".join("0x%.2x" % x for x in data[i : i + 11]))
        FONTS_C.write(",\n")
    FONTS_C.write("         }},\n")


os.makedirs("build/fontgen", exist_ok=True)
FONTS_C = open(f"main/fonts.gen.c", "w")
FONTS_C.write("// This file is autogenerated by fontgen.py\n")
FONTS_C.write("// Do not edit.\n")
FONTS_C.write("\n")
FONTS_C.write('#include "fonts.h"\n')
FONTS_C.write("const struct font_t *fonts = (struct font_t[]){\n")

# fmt: off
# MONOSPACE:
draw_font("pzim3x5",         size=10)
# draw_font("5x5",             size=10)
draw_font("BMSPA",           size=9)
draw_font("BMplain",         size=7)
# draw_font("bubblesstandard", size=15)
# draw_font("7linedigital",    size=10)
# draw_font("HUNTER",          size=9)
draw_font("m38",             size=5)
# draw_font("formplex12",      size=11)
# draw_font("sloth",           size=15)
#
# # # VARIABLE-WIDTH:
# draw_font("SUPERDIG",         size=9)
# draw_font("tama_mini02",      size=10)
# draw_font("homespun",         size=9)
# draw_font("zxpix",            size=10)
# draw_font("Minimum",          size=16)
# draw_font("Minimum+1",        size=16)
# draw_font("HISKYF21",         size=9)
# draw_font("renew",            size=8)
# draw_font("acme_5_outlines",  size=8)
# draw_font("haiku",            size=14)
# draw_font("aztech",           size=16)
# draw_font("Commo-Monospaced", size=8)
# draw_font("crackers",         size=21)
# draw_font("Blokus",           size=16)
# fmt: on

FONTS_C.write("    {0},\n")
FONTS_C.write("};\n")
FONTS_C.close()