"""One-shot ingest of the building look-dev packs into data/sprites/buildings.

Reads (read-only) the 45-degree single-cell building sheets and emits 1:1
base/mask pairs per building type. Idempotent: rerunning over its own output
reproduces identical files.

Usage: python tools/ingest_buildings.py [--lookdev PATH]
"""

import argparse
import os
import sys

from PIL import Image

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_LOOKDEV = os.path.join(
    os.path.dirname(REPO_ROOT),
    "conflict-converge-look-dev-2",
    "for-conflict-converge",
    "buildings",
)
DATA = os.path.join(REPO_ROOT, "data")

# --- Look-dev source mapping -------------------------------------------------

# leaf stem -> (game stem, cell px)
BUILDINGS = {
    "barrack": ("barrack", 256),
    "factory": ("workshop", 384),
    "depot": ("depot", 128),
    "hq": ("hq", 384),
}

# --- Team-accent shades (palette-quantized, exact RGB +-1) -------------------

SHADE_LIGHT = (71, 159, 244)
SHADE_BASE = (58, 130, 200)
SHADE_DARK = (42, 94, 144)
SHADE_DARKEST = (32, 72, 110)
SHADES = (SHADE_LIGHT, SHADE_BASE, SHADE_DARK, SHADE_DARKEST)
TOL = 1

# mask gray = green channel relative to the lightest shade's green (159)
MASK_GREEN_REF = SHADE_LIGHT[1]
MASK_VALUES = {
    SHADE_LIGHT: 255,
    SHADE_BASE: round(SHADE_BASE[1] / MASK_GREEN_REF * 255),
    SHADE_DARK: round(SHADE_DARK[1] / MASK_GREEN_REF * 255),
    SHADE_DARKEST: round(SHADE_DARKEST[1] / MASK_GREEN_REF * 255),
}

# rebuild tolerance: per-channel sum over base+mask composited back
REBUILD_THRESHOLD = 9


def load_png(path):
    with Image.open(path) as im:
        return im.convert("RGBA")


def is_shade(px):
    if px[3] == 0:
        return False
    return any(all(abs(px[c] - s[c]) <= TOL for c in range(3)) for s in SHADES)


def split_base_mask(sheet):
    """4-shade team-accent pixels -> mask (gray intensity) / base (transparent);
    every other pixel goes to base unchanged."""
    w, h = sheet.size
    base = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    mask = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    bp, mp = base.load(), mask.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = sheet.getpixel((x, y))
            if is_shade((r, g, b, a)):
                shade = next(
                    s for s in SHADES if all(abs((r, g, b)[c] - s[c]) <= TOL for c in range(3))
                )
                gray = MASK_VALUES[shade]
                mp[x, y] = (gray, gray, gray, 255)
            else:
                bp[x, y] = (r, g, b, a)
    return base, mask


def write_png(path, img):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, "PNG")


def building_paths(stem):
    folder = os.path.join(DATA, "sprites", "buildings")
    return (
        os.path.join(folder, f"{stem}_base.png"),
        os.path.join(folder, f"{stem}_mask.png"),
    )


def emit(lookdev):
    for leaf, (stem, cell) in BUILDINGS.items():
        src_path = os.path.join(lookdev, f"{leaf}_45", "sheet.png")
        src = load_png(src_path)
        if src.size != (cell, cell):
            sys.exit(f"{leaf}_45/sheet.png is {src.size}, expected ({cell}, {cell})")
        base, mask = split_base_mask(src)
        base_path, mask_path = building_paths(stem)
        write_png(base_path, base)
        write_png(mask_path, mask)
        print(f"{leaf} -> {stem}: {cell}x{cell} base+mask")


def compose(base, mask):
    """Rebuild the source from a base/mask pair (mask wins where opaque)."""
    w, h = base.size
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    op = out.load()
    for y in range(h):
        for x in range(w):
            mr, mg, mb, ma = mask.getpixel((x, y))
            if ma == 0:
                op[x, y] = base.getpixel((x, y))
                continue
            shade = next(s for s, gray in MASK_VALUES.items() if gray == mg)
            br, bg, bb, ba = base.getpixel((x, y))
            a = ba if ba != 0 else ma
            op[x, y] = (shade[0], shade[1], shade[2], a)
    return out


def verify(lookdev):
    failures = []

    def check(cond, msg):
        if not cond:
            failures.append(msg)

    for leaf, (stem, cell) in BUILDINGS.items():
        src = load_png(os.path.join(lookdev, f"{leaf}_45", "sheet.png"))
        base_path, mask_path = building_paths(stem)
        base = load_png(base_path)
        mask = load_png(mask_path)
        for path, img in ((base_path, base), (mask_path, mask)):
            check(img.size == (cell, cell), f"{os.path.basename(path)} is {img.size}, expected {cell}x{cell}")

        w, h = src.size
        for y in range(h):
            for x in range(w):
                sr, sg, sb, sa = src.getpixel((x, y))
                br, bg, bb, ba = base.getpixel((x, y))
                mr, mg, mb, ma = mask.getpixel((x, y))
                check(ma == 0 or sa != 0, f"{stem} mask set on transparent source pixel ({x},{y})")
                check(not (ba != 0 and ma != 0), f"{stem} base and mask both set at ({x},{y})")

        rebuilt = compose(base, mask)
        over = 0
        for y in range(h):
            for x in range(w):
                s = src.getpixel((x, y))
                r = rebuilt.getpixel((x, y))
                if sum(abs(s[c] - r[c]) for c in range(4)) > REBUILD_THRESHOLD:
                    over += 1
        check(over == 0, f"{stem} rebuild mismatch: {over} px over threshold {REBUILD_THRESHOLD}")

    if failures:
        print("VERIFY FAILURES:")
        for f in failures:
            print("  -", f)
        sys.exit(1)
    print(f"verify ok: {len(BUILDINGS)} base, {len(BUILDINGS)} mask")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lookdev", default=DEFAULT_LOOKDEV)
    args = ap.parse_args()
    if not os.path.isdir(args.lookdev):
        sys.exit(f"look-dev source not found: {args.lookdev}")

    emit(args.lookdev)
    verify(args.lookdev)


if __name__ == "__main__":
    main()
