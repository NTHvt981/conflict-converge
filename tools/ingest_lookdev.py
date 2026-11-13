"""One-shot ingest of the look-dev WIP assets into data/.

Reads (read-only) the look-dev render tree and emits transposed unit sheets,
team-color mask/base pairs, fallback flats, barrack sheet slices, and rewrites
data/configs/{textures,sprites,animations}.json. Idempotent: rerunning over its
own output reproduces identical files.

Usage: python tools/ingest_lookdev.py [--lookdev PATH]
"""

import argparse
import json
import os
import sys

from PIL import Image

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_LOOKDEV = os.path.join(
    os.path.dirname(REPO_ROOT), "conflict-converge-look-dev-2", "for-conflict-converge"
)
DATA = os.path.join(REPO_ROOT, "data")

# --- Look-dev source mapping -------------------------------------------------

# builder -> (artDir, sprite stem)
UNITS = {
    "soldier": ("rifle", "rifle_infantry"),
    "at": ("antiarmor", "antiarmor"),
    "engineer": ("engineer", "engineer"),
    "medic": ("medic", "medic"),
}

# manifest action -> engine action name ("run" skipped by decision: move = walk)
ACTION_MAP = {"idle": "idle", "walk": "move", "attack": "attack", "death": "death"}

# folder names lie for several units; manifest builder+action are authoritative
FOLDER_BY_ACTION = {
    ("soldier", "idle"): "out_idle",
    ("soldier", "walk"): "out_soldier",
    ("soldier", "attack"): "out_attack",
    ("soldier", "death"): "out_death",
}
FOLDER_PREFIX = {"soldier": None, "at": "out_at", "engineer": "out_eng", "medic": "out_med"}
FRAMES = {"idle": 8, "move": 8, "attack": 4, "death": 4}

TILE = 72
SRC_ANGLES = 8  # source row r = angle r*45, angle 0 = facing south, CCW on screen
# engine grid: row = frame index, col = direction (0=N ... 4=S ... 6=E)
# source row r (angle) lands in output direction column (r + 4) % 8
ANGLE_TO_DIR = lambda r: (r + 4) % 8  # noqa: E731

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

# red flat ramp: each shade -> channel-wise scale of d94a4a by shade/base ratios
RED_TARGET = (217, 74, 74)  # d94a4a, replaces SHADE_BASE
RED_RAMP = {
    SHADE_BASE: RED_TARGET,
    SHADE_LIGHT: tuple(min(255, round(c * SHADE_LIGHT[i] / SHADE_BASE[i])) for i, c in enumerate(RED_TARGET)),
    SHADE_DARK: tuple(min(255, round(c * SHADE_DARK[i] / SHADE_BASE[i])) for i, c in enumerate(RED_TARGET)),
    SHADE_DARKEST: tuple(min(255, round(c * SHADE_DARKEST[i] / SHADE_BASE[i])) for i, c in enumerate(RED_TARGET)),
}

# --- Fresh id blocks ---------------------------------------------------------

# stem -> {action: (base startId, mask startId = base + 100)}
ID_BLOCKS = {
    "rifle_infantry": {"idle": 2000, "move": 2200, "attack": 2400, "death": 2600},
    "antiarmor": {"idle": 2800, "move": 3000, "attack": 3200, "death": 3400},
    "engineer": {"idle": 3600, "move": 3800, "attack": 4000, "death": 4200},
    "medic": {"idle": 4400, "move": 4600, "attack": 4800, "death": 5000},
}
ANIM_ID_START = 1000
ANIM_DURATION = {"idle": 42, "move": 42, "attack": 125, "death": 125}
ANIM_LOOP = {"idle": True, "move": True, "attack": True, "death": False}

# old infantry entries removed from the configs (prototypeinfantry_*, lighttank_* stay)
REMOVE_PREFIXES = ("rifle_infantry_", "antiarmor_", "engineer_", "medic_")
# legacy rifle texture ids do not carry the rifle_infantry_ prefix but are the
# same infantry pair being replaced by this ingest
LEGACY_TEXTURE_IDS = (
    "rifle_idle_base", "rifle_idle_mask",
    "rifle_move_base", "rifle_move_mask",
    "rifle_attack_base", "rifle_attack_mask",
)


def load_png(path):
    with Image.open(path) as im:
        return im.convert("RGBA")


def is_shade(px):
    return any(
        all(abs(px[c] - s[c]) <= TOL for c in range(3))
        for s in SHADES
    )


def transpose_sheet(src):
    """Source sheet (row=angle, col=frame) -> engine sheet (row=frame, col=dir)."""
    nframes = src.width // TILE
    out = Image.new("RGBA", (SRC_ANGLES * TILE, nframes * TILE), (0, 0, 0, 0))
    for r in range(SRC_ANGLES):
        dcol = ANGLE_TO_DIR(r)
        for f in range(nframes):
            cell = src.crop((f * TILE, r * TILE, (f + 1) * TILE, (r + 1) * TILE))
            out.paste(cell, (dcol * TILE, f * TILE))
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
            if is_shade((r, g, b)):
                shade = next(s for s in SHADES if all(abs((r, g, b)[c] - s[c]) <= TOL for c in range(3)))
                gray = MASK_VALUES[shade]
                mp[x, y] = (gray, gray, gray, 255)
            else:
                bp[x, y] = (r, g, b, a)
    return base, mask


def write_png(path, img):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, "PNG")


def src_folder(builder, action):
    if (builder, action) in FOLDER_BY_ACTION:
        return FOLDER_BY_ACTION[(builder, action)]
    return f"{FOLDER_PREFIX[builder]}_{action}"


def emit_unit_sheets(lookdev):
    """32 transposed base+mask sheets under data/sprites/units/<artDir>/."""
    for builder, (artdir, stem) in UNITS.items():
        for src_action, engine_action in ACTION_MAP.items():
            src = load_png(os.path.join(lookdev, src_folder(builder, src_action), "sheet.png"))
            sheet = transpose_sheet(src)
            base, mask = split_base_mask(sheet)
            write_png(os.path.join(DATA, "sprites", "units", artdir, f"{engine_action}_2d_base.png"), base)
            write_png(os.path.join(DATA, "sprites", "units", artdir, f"{engine_action}_2d_mask.png"), mask)


def remap_red(px):
    if is_shade(px):
        shade = next(s for s in SHADES if all(abs(px[c] - s[c]) <= TOL for c in range(3)))
        return RED_RAMP[shade]
    return px


def emit_flats(lookdev):
    """16 fallback flats: dir 6 (east), frame 0, resized 72->64 LANCZOS."""
    src_row = next(r for r in range(SRC_ANGLES) if ANGLE_TO_DIR(r) == 6)  # = 2
    for builder, (artdir, stem) in UNITS.items():
        for src_action, engine_action in (("idle", "idle"), ("attack", "attack")):
            src = load_png(os.path.join(lookdev, src_folder(builder, src_action), "sheet.png"))
            cell = src.crop((0, src_row * TILE, TILE, (src_row + 1) * TILE))
            flat = cell.resize((64, 64), Image.LANCZOS)
            write_png(
                os.path.join(DATA, "sprites", "units", artdir, f"{stem}_blue_{engine_action}.png"),
                flat,
            )
            red = Image.new("RGBA", flat.size)
            red.putdata([remap_red(px) for px in flat.getdata()])
            write_png(
                os.path.join(DATA, "sprites", "units", artdir, f"{stem}_red_{engine_action}.png"),
                red,
            )


def emit_barrack(lookdev):
    """barrack_base.png + barrack_mask.png = out_barrack sheet row 0."""
    src = load_png(os.path.join(lookdev, "out_barrack", "sheet.png"))
    row = src.crop((0, 0, 256, 256))
    base, mask = split_base_mask(row)
    write_png(os.path.join(DATA, "sprites", "buildings", "barrack_base.png"), base)
    write_png(os.path.join(DATA, "sprites", "buildings", "barrack_mask.png"), mask)


# --- Config mutation ---------------------------------------------------------

STEM_DIRS = {"rifle_infantry": "rifle", "antiarmor": "antiarmor", "engineer": "engineer", "medic": "medic"}


def is_removed(key, legacy=False):
    if key.startswith(REMOVE_PREFIXES):
        return True
    return legacy and key.startswith(LEGACY_TEXTURE_IDS)


def update_configs():
    """Returns (textures, sprites, anims_doc, new_keys) where new_keys lists the
    ids/prefixes/names appended by this run (excluded from stale checks)."""
    new_keys = set()
    # textures.json
    textures = json.load(open(os.path.join(DATA, "configs", "textures.json")))
    kept = [t for t in textures["textures"] if not is_removed(t["id"], legacy=True)]
    for stem, actions in ID_BLOCKS.items():
        for action in ("idle", "move", "attack", "death"):
            h = FRAMES[action] * TILE
            for suffix in ("base", "mask"):
                tid = f"{stem}_{action}_{suffix}"
                new_keys.add(("texture", tid))
                kept.append({
                    "id": tid,
                    "file": f"units/{STEM_DIRS[stem]}/{action}_2d_{suffix}.png",
                    "width": 576,
                    "height": h,
                })
    textures["textures"] = kept

    # sprites.json
    sprites = json.load(open(os.path.join(DATA, "configs", "sprites.json")))
    grids = [g for g in sprites["grids"] if not is_removed(g["prefix"])]
    for stem, actions in ID_BLOCKS.items():
        for action in ("idle", "move", "attack", "death"):
            rows = FRAMES[action]
            base_id = actions[action]
            new_keys.add(("grid", f"{stem}_{action}"))
            new_keys.add(("grid", f"{stem}_{action}_mask"))
            grids.append({
                "texture": f"{stem}_{action}_base",
                "prefix": f"{stem}_{action}",
                "startId": base_id,
                "rows": rows,
                "cols": 8,
                "cellW": TILE,
                "cellH": TILE,
                "origin": {"x": 36, "y": 36},
                "maskGridStartId": base_id + 100,
            })
            grids.append({
                "texture": f"{stem}_{action}_mask",
                "prefix": f"{stem}_{action}_mask",
                "startId": base_id + 100,
                "rows": rows,
                "cols": 8,
                "cellW": TILE,
                "cellH": TILE,
                "origin": {"x": 36, "y": 36},
            })
    sprites["grids"] = grids

    # animations.json
    anims_doc = json.load(open(os.path.join(DATA, "configs", "animations.json")))
    anims = [a for a in anims_doc["animations"] if not is_removed(a["name"])]
    next_id = ANIM_ID_START
    for stem, actions in ID_BLOCKS.items():
        for action in ("idle", "move", "attack", "death"):
            for d in range(8):
                frames = [
                    {"sprite": actions[action] + f * 8 + d, "durationMs": ANIM_DURATION[action]}
                    for f in range(FRAMES[action])
                ]
                name = f"{stem}_{action}_{d}"
                new_keys.add(("anim", name))
                anims.append({
                    "name": name,
                    "id": next_id,
                    "loop": ANIM_LOOP[action],
                    "frames": frames,
                })
                next_id += 1
    anims_doc["animations"] = anims

    return textures, sprites, anims_doc, new_keys


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


def verify(textures, sprites, anims_doc, new_keys):
    failures = []

    def stale(kind, key):
        return is_removed(key, legacy=(kind == "texture")) and (kind, key) not in new_keys

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

    # anims: unique ids/names, sprites inside a grid, no removed-id references
    seen_ids, seen_names = set(), set()
    for a in anims_doc["animations"]:
        check(a["id"] not in seen_ids, f"duplicate anim id {a['id']}")
        check(a["name"] not in seen_names, f"duplicate anim name {a['name']}")
        seen_ids.add(a["id"])
        seen_names.add(a["name"])
        for fr in a["frames"]:
            check(fr["sprite"] in owners, f"anim {a['name']} references unknown sprite {fr['sprite']}")

    # no removed prefixes survive
    for a in anims_doc["animations"]:
        check(not stale("anim", a["name"]), f"stale anim {a['name']}")
    for t in textures["textures"]:
        check(not stale("texture", t["id"]), f"stale texture {t['id']}")
    for g in grids:
        check(not stale("grid", g["prefix"]), f"stale grid {g['prefix']}")

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

    print("emitting unit sheets...")
    emit_unit_sheets(args.lookdev)
    print("emitting flats...")
    emit_flats(args.lookdev)
    print("emitting barrack...")
    emit_barrack(args.lookdev)
    print("updating configs...")
    textures, sprites, anims_doc, new_keys = update_configs()
    write_text(os.path.join(DATA, "configs", "textures.json"), fmt_textures(textures))
    write_text(os.path.join(DATA, "configs", "sprites.json"), fmt_grids(sprites))
    write_text(os.path.join(DATA, "configs", "animations.json"), fmt_anims(anims_doc))
    verify(textures, sprites, anims_doc, new_keys)


if __name__ == "__main__":
    main()
