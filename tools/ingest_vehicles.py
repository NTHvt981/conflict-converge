"""One-shot ingest of the vehicle look-dev packs into data/.

Reads (read-only) the vehicle render tree and emits transposed vehicle sheets
plus team-color mask/base pairs, then rewrites
data/configs/{textures,sprites,animations}.json. Idempotent: rerunning over its
own output reproduces identical files.

Usage: python tools/ingest_vehicles.py [--lookdev PATH]
"""

import argparse
import json
import os
import sys

from PIL import Image, ImageChops

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_LOOKDEV = os.path.join(
    os.path.dirname(REPO_ROOT), "conflict-converge-look-dev-2", "for-conflict-converge", "vehicles"
)
DATA = os.path.join(REPO_ROOT, "data")

# --- Look-dev source mapping -------------------------------------------------

# unit folder/build stem -> (artDir, tile)
UNITS = {
    "lighttank": ("lighttank", 144),
    "heavytank": ("heavytank", 160),
    "ifv": ("ifv", 128),
    "artillery": ("artillery", 160),
}
UNIT_ORDER = ("lighttank", "heavytank", "ifv", "artillery")
ACTIONS = ("idle", "move", "attack", "death")
SRC_LEAF = {
    "idle": "idle/full",
    "move": "move/full",
    "attack": "attack/full",
    "death": "death/body",
}

FRAMES = 4
SRC_ANGLES = 8
# engine grid: row = frame index, col = direction
# vehicle row 0 faces screen-left; source row r lands in output direction column (r + 2) % 8
ANGLE_TO_DIR = lambda r: (r + 2) % 8  # noqa: E731

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

# --- Fresh id blocks ---------------------------------------------------------

# stem -> {action: (base startId, mask startId = base + 100)}
ID_BLOCKS = {
    "lighttank": {"idle": 5200, "move": 5400, "attack": 5600, "death": 5800},
    "heavytank": {"idle": 6000, "move": 6200, "attack": 6400, "death": 6600},
    "ifv": {"idle": 6800, "move": 7000, "attack": 7200, "death": 7400},
    "artillery": {"idle": 7600, "move": 7800, "attack": 8000, "death": 8200},
}
ANIM_ID_START = 1300
ANIM_DURATION = {"idle": 250, "move": 250, "attack": 250, "death": 250}  # 4 fps
ANIM_LOOP = {"idle": True, "move": True, "attack": True, "death": False}

# old lighttank stack entries replaced by the sheets this ingest emits
REMOVE_PREFIXES = ("lighttank_body", "lighttank_head")
REMOVED_TEXTURE_IDS = (
    "lighttank_body_idle_base",
    "lighttank_body_idle_mask",
    "lighttank_head_base",
    "lighttank_head_mask",
)
REMOVED_FILES = ("body_idle_stack_base.png", "body_idle_stack_mask.png", "head_stack_base.png", "head_stack_mask.png")


def load_png(path):
    with Image.open(path) as im:
        return im.convert("RGBA")


def is_shade(px):
    if px[3] == 0:
        return False
    return any(
        all(abs(px[c] - s[c]) <= TOL for c in range(3))
        for s in SHADES
    )


def transpose_sheet(src, tile):
    """Source sheet (row=angle, col=frame) -> engine sheet (row=frame, col=dir)."""
    nframes = src.width // tile
    out = Image.new("RGBA", (SRC_ANGLES * tile, nframes * tile), (0, 0, 0, 0))
    for r in range(SRC_ANGLES):
        dcol = ANGLE_TO_DIR(r)
        for f in range(nframes):
            cell = src.crop((f * tile, r * tile, (f + 1) * tile, (r + 1) * tile))
            out.paste(cell, (dcol * tile, f * tile))
    return out


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
                shade = next(s for s in SHADES if all(abs((r, g, b)[c] - s[c]) <= TOL for c in range(3)))
                gray = MASK_VALUES[shade]
                mp[x, y] = (gray, gray, gray, 255)
            else:
                bp[x, y] = (r, g, b, a)
    return base, mask


def write_png(path, img):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, "PNG")


def emit_unit_sheets(lookdev):
    """32 transposed base+mask sheets under data/sprites/units/<artDir>/."""
    for unit in UNIT_ORDER:
        artdir, tile = UNITS[unit]
        for action in ACTIONS:
            src = load_png(os.path.join(lookdev, unit, SRC_LEAF[action], "sheet.png"))
            if src.height != SRC_ANGLES * tile or src.width % tile:
                sys.exit(f"unexpected sheet size for {unit}/{action}: {src.size} at tile {tile}")
            sheet = transpose_sheet(src, tile)
            base, mask = split_base_mask(sheet)
            write_png(os.path.join(DATA, "sprites", "units", artdir, f"{action}_2d_base.png"), base)
            write_png(os.path.join(DATA, "sprites", "units", artdir, f"{action}_2d_mask.png"), mask)


def remove_replaced_files():
    for name in REMOVED_FILES:
        path = os.path.join(DATA, "sprites", "units", "lighttank", name)
        if os.path.isfile(path):
            os.remove(path)


# --- Config mutation ---------------------------------------------------------


def build_entries():
    """New textures, grids and animations in the canonical emission order."""
    textures, grids, anims = [], [], []
    next_id = ANIM_ID_START
    for unit in UNIT_ORDER:
        artdir, tile = UNITS[unit]
        blocks = ID_BLOCKS[unit]
        for action in ACTIONS:
            base_id = blocks[action]
            for suffix in ("base", "mask"):
                textures.append({
                    "id": f"{unit}_{action}_{suffix}",
                    "file": f"units/{artdir}/{action}_2d_{suffix}.png",
                    "width": SRC_ANGLES * tile,
                    "height": FRAMES * tile,
                })
            grids.append({
                "texture": f"{unit}_{action}_base",
                "prefix": f"{unit}_{action}",
                "startId": base_id,
                "rows": FRAMES,
                "cols": SRC_ANGLES,
                "cellW": tile,
                "cellH": tile,
                "origin": {"x": tile // 2, "y": tile // 2},
                "maskGridStartId": base_id + 100,
            })
            grids.append({
                "texture": f"{unit}_{action}_mask",
                "prefix": f"{unit}_{action}_mask",
                "startId": base_id + 100,
                "rows": FRAMES,
                "cols": SRC_ANGLES,
                "cellW": tile,
                "cellH": tile,
                "origin": {"x": tile // 2, "y": tile // 2},
            })
    for unit in UNIT_ORDER:
        blocks = ID_BLOCKS[unit]
        for action in ACTIONS:
            for d in range(SRC_ANGLES):
                frames = [
                    {"sprite": blocks[action] + f * SRC_ANGLES + d, "durationMs": ANIM_DURATION[action]}
                    for f in range(FRAMES)
                ]
                anims.append({
                    "name": f"{unit}_{action}_{d}",
                    "id": next_id,
                    "loop": ANIM_LOOP[action],
                    "frames": frames,
                })
                next_id += 1
    return textures, grids, anims


def is_removed(key):
    return key.startswith(REMOVE_PREFIXES) or key in REMOVED_TEXTURE_IDS


def update_configs():
    new_textures, new_grids, new_anims = build_entries()
    new_texture_ids = {t["id"] for t in new_textures}
    new_grid_prefixes = {g["prefix"] for g in new_grids}
    new_anim_names = {a["name"] for a in new_anims}

    textures = json.load(open(os.path.join(DATA, "configs", "textures.json")))
    textures["textures"] = [
        t for t in textures["textures"]
        if not is_removed(t["id"]) and t["id"] not in new_texture_ids
    ] + new_textures

    sprites = json.load(open(os.path.join(DATA, "configs", "sprites.json")))
    sprites["grids"] = [
        g for g in sprites["grids"]
        if not is_removed(g["prefix"]) and g["prefix"] not in new_grid_prefixes
    ] + new_grids

    anims_doc = json.load(open(os.path.join(DATA, "configs", "animations.json")))
    anims_doc["animations"] = [
        a for a in anims_doc["animations"]
        if not is_removed(a["name"]) and a["name"] not in new_anim_names
    ] + new_anims

    return textures, sprites, anims_doc


# --- Bespoke serializers (2-space indent, LF) -------------------------------


def fmt_textures(doc):
    lines = ['{', '  "textures": [']
    for i, t in enumerate(doc["textures"]):
        comma = "," if i < len(doc["textures"]) - 1 else ""
        lines.append(
            '    { "id": "%s", "file": "%s", "width": %d, "height": %d }%s'
            % (t["id"], t["file"], t["width"], t["height"], comma)
        )
    lines += ['  ]', '}']
    return "\n".join(lines) + "\n"


def fmt_grids(doc):
    lines = ['{', '  "sprites": [],', '  "grids": [']
    for i, g in enumerate(doc["grids"]):
        comma = "," if i < len(doc["grids"]) - 1 else ""
        lines.append(
            '    { "texture": "%s", "prefix": "%s", "startId": %d,'
            % (g["texture"], g["prefix"], g["startId"])
        )
        lines.append(
            '      "rows": %d, "cols": %d, "cellW": %d, "cellH": %d,'
            % (g["rows"], g["cols"], g["cellW"], g["cellH"])
        )
        tail = ', "maskGridStartId": %d' % g["maskGridStartId"] if "maskGridStartId" in g else ""
        lines.append(
            '      "origin": { "x": %d, "y": %d }%s }%s'
            % (g["origin"]["x"], g["origin"]["y"], tail, comma)
        )
    lines += ['  ]', '}']
    return "\n".join(lines) + "\n"


def fmt_anims(doc):
    lines = ['{', '  "animations": [']
    for i, a in enumerate(doc["animations"]):
        comma = "," if i < len(doc["animations"]) - 1 else ""
        lines.append(
            '    { "name": "%s", "id": %d, "loop": %s,'
            % (a["name"], a["id"], "true" if a["loop"] else "false")
        )
        lines.append('      "frames": [')
        for j, fr in enumerate(a["frames"]):
            fcomma = "," if j < len(a["frames"]) - 1 else ""
            lines.append('        { "sprite": %d, "durationMs": %d }%s' % (fr["sprite"], fr["durationMs"], fcomma))
        lines.append('      ] }' + comma)
    lines += ['  ]', '}']
    return "\n".join(lines) + "\n"


def write_text(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


# --- Verification ------------------------------------------------------------


def binary_alpha(img):
    return img.getchannel("A").point(lambda a: 255 if a > 0 else 0)


def transposed_source_alpha(lookdev, unit, action, tile):
    """Source sheet alpha (row=angle, col=frame) in engine layout (row=frame, col=dir)."""
    src = load_png(os.path.join(lookdev, unit, SRC_LEAF[action], "sheet.png"))
    alpha = src.getchannel("A")
    out = Image.new("L", (SRC_ANGLES * tile, (src.width // tile) * tile), 0)
    for r in range(SRC_ANGLES):
        dcol = ANGLE_TO_DIR(r)
        for f in range(src.width // tile):
            out.paste(alpha.crop((f * tile, r * tile, (f + 1) * tile, (r + 1) * tile)), (dcol * tile, f * tile))
    return out.point(lambda a: 255 if a > 0 else 0)


def verify(textures, sprites, anims_doc, lookdev):
    failures = []

    def check(cond, msg):
        if not cond:
            failures.append(msg)

    grids = sprites["grids"]
    # no id collisions between grids
    owners = {}
    for g in grids:
        for sid in range(g["startId"], g["startId"] + g["rows"] * g["cols"]):
            check(sid not in owners, f"sprite id {sid} claimed by {g['prefix']} and {owners.get(sid)}")
            owners[sid] = g["prefix"]

    tex_by_id = {t["id"]: t for t in textures["textures"]}
    # every texture file exists with declared dimensions
    for t in textures["textures"]:
        path = os.path.join(DATA, "sprites", t["file"])
        check(os.path.isfile(path), f"missing texture file {t['file']}")
        if os.path.isfile(path):
            with Image.open(path) as im:
                check(
                    im.size == (t["width"], t["height"]),
                    f"{t['file']} is {im.size}, expected {(t['width'], t['height'])}",
                )
    # every grid's texture exists
    for g in grids:
        check(g["texture"] in tex_by_id, f"grid {g['prefix']} references unknown texture {g['texture']}")
        if "maskGridStartId" in g:
            check(
                any(m["startId"] == g["maskGridStartId"] and m["prefix"].endswith("_mask") for m in grids),
                f"grid {g['prefix']} maskGridStartId {g['maskGridStartId']} has no matching mask grid",
            )

    # anims: unique ids/names, sprites inside a grid
    seen_ids, seen_names = set(), set()
    for a in anims_doc["animations"]:
        check(a["id"] not in seen_ids, f"duplicate anim id {a['id']}")
        check(a["name"] not in seen_names, f"duplicate anim name {a['name']}")
        seen_ids.add(a["id"])
        seen_names.add(a["name"])
        for fr in a["frames"]:
            check(fr["sprite"] in owners, f"anim {a['name']} references unknown sprite {fr['sprite']}")
    check(len(seen_ids) == len(anims_doc["animations"]), "animation id count mismatch")

    # written base/mask sheets: mask alpha must sit inside the source alpha, and
    # base and mask must never both carry alpha at the same pixel
    for unit in UNIT_ORDER:
        artdir, tile = UNITS[unit]
        for action in ACTIONS:
            base = load_png(os.path.join(DATA, "sprites", "units", artdir, f"{action}_2d_base.png"))
            mask = load_png(os.path.join(DATA, "sprites", "units", artdir, f"{action}_2d_mask.png"))
            mask_alpha = binary_alpha(mask)
            src_alpha = transposed_source_alpha(lookdev, unit, action, tile)
            stray = ImageChops.subtract(mask_alpha, src_alpha)
            stray_count = stray.histogram()[255]
            check(
                stray_count == 0,
                f"{artdir}/{action}_2d_mask.png has {stray_count} opaque px where the source is transparent",
            )
            both = ImageChops.multiply(binary_alpha(base), mask_alpha)
            check(
                both.getbbox() is None,
                f"{artdir}/{action} base and mask both carry alpha at the same pixel",
            )

    # no removed prefixes survive
    for a in anims_doc["animations"]:
        check(not is_removed(a["name"]), f"stale anim {a['name']}")
    for t in textures["textures"]:
        check(not is_removed(t["id"]), f"stale texture {t['id']}")
    for g in grids:
        check(not is_removed(g["prefix"]), f"stale grid {g['prefix']}")
    for path in REMOVED_FILES:
        check(
            not os.path.isfile(os.path.join(DATA, "sprites", "units", "lighttank", path)),
            f"stale file {path}",
        )

    if failures:
        print("VERIFY FAILURES:")
        for f in failures:
            print("  -", f)
        sys.exit(1)
    print(f"verify ok: {len(grids)} grids, {len(textures['textures'])} textures, {len(anims_doc['animations'])} anims")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lookdev", default=DEFAULT_LOOKDEV)
    args = ap.parse_args()
    if not os.path.isdir(args.lookdev):
        sys.exit(f"look-dev source not found: {args.lookdev}")

    print("emitting vehicle sheets...")
    emit_unit_sheets(args.lookdev)
    print("removing replaced lighttank stacks...")
    remove_replaced_files()
    print("updating configs...")
    textures, sprites, anims_doc = update_configs()
    write_text(os.path.join(DATA, "configs", "textures.json"), fmt_textures(textures))
    write_text(os.path.join(DATA, "configs", "sprites.json"), fmt_grids(sprites))
    write_text(os.path.join(DATA, "configs", "animations.json"), fmt_anims(anims_doc))
    verify(textures, sprites, anims_doc, args.lookdev)


if __name__ == "__main__":
    main()
