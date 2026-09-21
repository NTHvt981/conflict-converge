"""Bake OpenSansPX TTFs into BMFont XML .fnt + PNG atlases for rmlui-probe.

The probe's bitmap font engine (tools/rmlui_probe/FontEngineBitmap.cpp) only
parses BMFont XML plus a single PNG page, so the pixel TTFs in data/fonts/
must be pre-rasterized. Glyphs are rendered 1-bit (FT_LOAD_TARGET_MONO,
smooth=0) for hard pixel edges; the render interface point-filters font
atlases so dp-upscaled clones stay chunky instead of blurring.

Usage:  python tools/bake_opensanspx_fnt.py
Output: tools/rmlui_probe/data/ui/OpenSansPX_{Regular,Bold}_22.{fnt,png}
"""
import os
import sys

import freetype
from PIL import Image

SIZE = 22
ATLAS_W = 256
GUTTER = 1

# ASCII printable + Latin-1 (minus soft hyphen, absent from the TTF) +
# ellipsis (drives FontMetrics.has_ellipsis in the probe parser).
CHARSET = [c for c in range(32, 127)]
CHARSET += [c for c in range(160, 256) if c != 0xAD]
CHARSET.append(0x2026)

FACES = [
    ("data/fonts/OpenSansPX.ttf", 0, "OpenSansPX_Regular_22"),
    ("data/fonts/OpenSansPXBold.ttf", 1, "OpenSansPX_Bold_22"),
]


def render_mono(face, codepoint):
    face.load_char(
        chr(codepoint),
        freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO,
    )
    slot = face.glyph
    bmp = slot.bitmap
    rows = []
    if bmp.width > 0 and bmp.rows > 0:
        buf = bytes(bmp.buffer)
        for y in range(bmp.rows):
            row = []
            for x in range(bmp.width):
                byte = buf[y * bmp.pitch + x // 8]
                row.append(1 if byte & (0x80 >> (x % 8)) else 0)
            rows.append(row)
    advance = (slot.advance.x + 32) >> 6
    return rows, advance, slot.bitmap_left, slot.bitmap_top


def bake(ttf_path, bold, stem):
    face = freetype.Face(ttf_path)
    face.set_pixel_sizes(0, SIZE)
    asc = face.size.ascender >> 6
    desc = (-face.size.descender) >> 6
    line_height = face.size.height >> 6

    glyphs = {}
    for cp in CHARSET:
        if face.get_char_index(cp) == 0:
            continue
        rows, advance, left, top = render_mono(face, cp)
        glyphs[cp] = {
            "rows": rows,
            "w": len(rows[0]) if rows else 0,
            "h": len(rows) if rows else 0,
            "advance": advance,
            "xoffset": left,
            # BMFont yoffset is top-origin; the probe parser shifts it back
            # to the baseline (offset.y = yoffset - ascent).
            "yoffset": asc - top,
        }

    # Shelf pack.
    placements = {}
    x = y = row_h = 0
    for cp, g in glyphs.items():
        if g["w"] == 0 or g["h"] == 0:
            placements[cp] = (0, 0)
            continue
        if x + g["w"] > ATLAS_W:
            x = 0
            y += row_h + GUTTER
            row_h = 0
        placements[cp] = (x, y)
        x += g["w"] + GUTTER
        row_h = max(row_h, g["h"])
    atlas_h = y + row_h

    img = Image.new("RGBA", (ATLAS_W, atlas_h), (255, 255, 255, 0))
    px = img.load()
    for cp, g in glyphs.items():
        if g["w"] == 0 or g["h"] == 0:
            continue
        ox, oy = placements[cp]
        for dy, row in enumerate(g["rows"]):
            for dx, bit in enumerate(row):
                if bit:
                    px[ox + dx, oy + dy] = (255, 255, 255, 255)

    # Kerning for baked pairs only.
    kernings = []
    for left in glyphs:
        for right in glyphs:
            try:
                k = face.get_kerning(
                    face.get_char_index(left),
                    face.get_char_index(right),
                    freetype.FT_KERNING_DEFAULT,
                )
            except Exception:
                continue
            amount = (k.x + 32) >> 6
            if amount:
                kernings.append((left, right, amount))

    out_dir = os.path.join("tools", "rmlui_probe", "data", "ui")
    img.save(os.path.join(out_dir, stem + ".png"))

    lines = ['<?xml version="1.0"?>', "<font>"]
    lines.append(
        '  <info face="OpenSansPX" size="%d" bold="%d" italic="0" charset="" '
        'unicode="1" stretchH="100" smooth="0" aa="1" padding="0,0,0,0" '
        'spacing="1,1" outline="0"/>' % (SIZE, bold)
    )
    lines.append(
        '  <common lineHeight="%d" base="%d" scaleW="%d" scaleH="%d" '
        'pages="1" packed="0" alphaChnl="0" redChnl="4" greenChnl="4" '
        'blueChnl="4"/>' % (line_height, asc, ATLAS_W, atlas_h)
    )
    lines.append("  <pages>")
    lines.append('    <page id="0" file="%s.png" />' % stem)
    lines.append("  </pages>")
    lines.append('  <chars count="%d">' % len(glyphs))
    for cp, g in glyphs.items():
        ox, oy = placements[cp]
        lines.append(
            '    <char id="%d" x="%d" y="%d" width="%d" height="%d" '
            'xoffset="%d" yoffset="%d" xadvance="%d" page="0" chnl="15" />'
            % (cp, ox, oy, g["w"], g["h"], g["xoffset"], g["yoffset"], g["advance"])
        )
    lines.append("  </chars>")
    lines.append('  <kernings count="%d">' % len(kernings))
    for left, right, amount in kernings:
        lines.append(
            '    <kerning first="%d" second="%d" amount="%d" />'
            % (left, right, amount)
        )
    lines.append("  </kernings>")
    lines.append("</font>")
    with open(os.path.join(out_dir, stem + ".fnt"), "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    print(
        "%s: %d glyphs, %dx%d atlas, base %d line %d, %d kernings"
        % (stem, len(glyphs), ATLAS_W, atlas_h, asc, line_height, len(kernings))
    )


if __name__ == "__main__":
    for ttf, bold, stem in FACES:
        if not os.path.exists(ttf):
            sys.exit("missing " + ttf)
        bake(ttf, bold, stem)
