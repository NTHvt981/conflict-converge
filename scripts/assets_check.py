#!/usr/bin/env python3
"""Enforce the asset naming convention under data/.

Convention: lower_snake_case file stems, movement anims use `move`
(never `run`/`walk`), building stems carry no view suffix such as
`stack_down_left`. Atlas JSON cross-references must resolve.

Usage: python scripts/assets_check.py [--root DIR]
Exits 1 with a violation list, 0 when clean.
"""

import argparse
import json
import re
import sys
from pathlib import Path

STEM_RE = re.compile(r"[a-z0-9]+(_[a-z0-9]+)*")
BANNED_SUBSTRINGS = ("stack_down_left",)
BANNED_TOKENS = frozenset({"walk", "run"})
ACTION_SUFFIXES = ("_idle", "_move", "_attack", "_head")

SCAN_DIRS = ("sprites", "configs", "audio", "maps", "ui", "fonts")
SKIP_DIRS = {"logs", "replays"}
SKIP_FILES = {"quicksave.ccpb", "font_metrics.txt"}
ALLOWLIST = {"fonts/OFL.txt"}

ATLAS_FILES = {"textures.json", "sprites.json", "animations.json"}


def stem_violations(rel: str, stem: str) -> list:
    problems = []
    if STEM_RE.fullmatch(stem) is None:
        problems.append(f"{rel}: stem is not lower_snake_case")
    if any(s in stem for s in BANNED_SUBSTRINGS):
        problems.append(f"{rel}: banned substring {BANNED_SUBSTRINGS}")
    banned = BANNED_TOKENS.intersection(stem.split("_"))
    if banned:
        problems.append(f"{rel}: banned token(s) {sorted(banned)}")
    return problems


def check_filenames(data: Path) -> list:
    problems = []
    for scan in SCAN_DIRS:
        root = data / scan
        if not root.is_dir():
            problems.append(f"data/{scan}: directory missing")
            continue
        for path in sorted(root.rglob("*")):
            if not path.is_file():
                continue
            rel = path.relative_to(data).as_posix()
            if path.suffix != path.suffix.lower():
                problems.append(f"{rel}: extension is not lowercase")
            if rel in ALLOWLIST or path.name in SKIP_FILES:
                continue
            if any(part in SKIP_DIRS for part in path.relative_to(data).parts):
                continue
            problems.extend(stem_violations(rel, path.stem))
    return problems


def load_json(path: Path, problems: list):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        problems.append(f"{path.name}: unreadable JSON ({exc})")
        return None


def check_textures(data: Path, problems: list) -> set:
    path = data / "configs" / "textures.json"
    obj = load_json(path, problems)
    ids = set()
    if obj is None:
        return ids
    for entry in obj.get("textures", []):
        tex_id = entry.get("id", "")
        file = entry.get("file", "")
        ids.add(tex_id)
        problems.extend(stem_violations(f"textures.json:{tex_id}", tex_id))
        if not (data / "sprites" / file).is_file():
            problems.append(f"textures.json:{tex_id}: missing file data/sprites/{file}")
    if len(ids) != len(obj.get("textures", [])):
        problems.append("textures.json: duplicate texture id")
    return ids


def check_sprites(data: Path, problems: list, texture_ids: set) -> set:
    path = data / "configs" / "sprites.json"
    obj = load_json(path, problems)
    prefixes = set()
    if obj is None:
        return prefixes
    for grid in obj.get("grids", []):
        tex = grid.get("texture", "")
        prefix = grid.get("prefix", "")
        prefixes.add(prefix)
        problems.extend(stem_violations(f"sprites.json:{prefix}", prefix))
        if tex not in texture_ids:
            problems.append(f"sprites.json:{prefix}: unknown texture {tex!r}")
        base = prefix[:-5] if prefix.endswith("_mask") else prefix
        if not base.endswith(ACTION_SUFFIXES):
            problems.append(
                f"sprites.json:{prefix}: prefix ends in idle/move/attack/head (or that + _mask)"
            )
    return prefixes


def check_animations(data: Path, problems: list, prefixes: set) -> None:
    path = data / "configs" / "animations.json"
    obj = load_json(path, problems)
    if obj is None:
        return
    solid = {p for p in prefixes if not p.endswith("_mask")}
    for anim in obj.get("animations", []):
        name = anim.get("name", "")
        problems.extend(stem_violations(f"animations.json:{name}", name))
        base, _, direction = name.rpartition("_")
        if not direction.isdigit():
            problems.append(f"animations.json:{name}: name ends _<direction>")
            continue
        if base not in solid:
            problems.append(f"animations.json:{name}: unknown sprite prefix {base!r}")


def check_unit_configs(data: Path, problems: list) -> None:
    configs = data / "configs"
    for path in sorted(configs.glob("*.json")):
        if path.name in ATLAS_FILES:
            continue
        raw = path.read_text(encoding="utf-8")
        for banned_key in ("atlasWalkPrefix", "atlas_walk_prefix"):
            if banned_key in raw:
                problems.append(f"{path.name}: banned key {banned_key} (use atlasMovePrefix)")
        obj = load_json(path, problems)
        if obj is None:
            continue
        art = obj.get("art", {})
        idle = art.get("atlasIdlePrefix", "")
        move = art.get("atlasMovePrefix", "")
        if idle and not idle.endswith("_idle"):
            problems.append(f"{path.name}: atlasIdlePrefix ends _idle, got {idle!r}")
        if move and not move.endswith("_move"):
            problems.append(f"{path.name}: atlasMovePrefix ends _move, got {move!r}")
        for value in (idle, move):
            if value:
                problems.extend(stem_violations(f"{path.name}:{value}", value))


def main() -> int:
    parser = argparse.ArgumentParser(description="Enforce the data/ asset naming convention.")
    parser.add_argument("--root", default=str(Path(__file__).resolve().parent.parent))
    args = parser.parse_args()
    data = Path(args.root) / "data"
    problems: list = []
    problems.extend(check_filenames(data))
    texture_ids = check_textures(data, problems)
    prefixes = check_sprites(data, problems, texture_ids)
    check_animations(data, problems, prefixes)
    check_unit_configs(data, problems)
    for problem in problems:
        print(f"assets_check: {problem}")
    print(f"assets_check: {'FAIL' if problems else 'OK'} ({len(problems)} violation(s))")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
