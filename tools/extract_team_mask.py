#!/usr/bin/env python3
"""
extract_team_mask.py -- splits an already-rendered 2D sprite PNG (output of
convert_vox_to_sprite.py) into the project's base+mask team-tint pair
(documented in CLAUDE.md's "Team color: base+mask split" section):

  <name>_base.png -- every pixel EXCEPT the team-color material, unchanged;
                     team-color pixels become transparent.
  <name>_mask.png -- ONLY the team-color material, baked flat pure white
                     (no shading); everything else transparent.

At runtime: draw the base sprite normally, then draw the mask sprite tinted
by the team's color (white * tint = tint) -- one pair of sheets serves every
team, no per-team bake.

Why this works on an already-shaded PNG (no voxel/color-index data needed):
convert_vox_to_sprite.py's shading is a small, exactly-known function of the
base palette color and which cardinal face was hit (build_shade_table /
shade(), imported directly from that module) -- so the team-color material's
possible on-screen colors are a small, exactly-computable set (one shaded
RGB per face normal), not an open-ended range. Every pixel in the sprite is
classified by exact (within a tiny rounding tolerance) membership in that
set, not by fuzzy hue-distance guessing.

Usage:
    python extract_team_mask.py <sprite>.png [--color R G B] [--tolerance N]
                                [--out-base PATH] [--out-mask PATH]

--color defaults to (58, 130, 200), the "armor" slot shared by every
small_models/ asset (tank.vox, infantry_run.vox, etc.) -- override for a
sprite rendered from a different palette's team-color slot.
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import convert_vox_to_sprite as cv

DEFAULT_TEAM_COLOR = (58, 130, 200)  # small_models palette's shared "armor" slot
DEFAULT_TOLERANCE = 4  # per-channel; shading math is exact, this just absorbs rounding


def possible_shaded_rgbs(base_rgb):
    """Every RGB this base color can appear as once convert_vox_to_sprite.py's
    per-face shading is applied -- one value per cardinal face normal, using
    the exact same shade() function the renderer itself calls, so this is a
    precise reproduction, not an approximation."""
    return [cv.shade(base_rgb, normal) for normal in cv.FACE_NORMALS.values()]


def classify(pixels_rgb, targets, tolerance):
    """pixels_rgb: (N,3) array. targets: list of (r,g,b). Returns a boolean
    mask, True where the pixel is within `tolerance` (Chebyshev/per-channel)
    of ANY target color."""
    is_team = np.zeros(pixels_rgb.shape[0], dtype=bool)
    for t in targets:
        t_arr = np.array(t, dtype=np.int16)
        diff = np.abs(pixels_rgb.astype(np.int16) - t_arr)
        is_team |= np.all(diff <= tolerance, axis=1)
    return is_team


def split(sprite_path, team_color, tolerance, out_base, out_mask):
    img = Image.open(sprite_path).convert("RGBA")
    arr = np.array(img)
    h, w = arr.shape[:2]
    flat = arr.reshape(-1, 4)
    rgb, alpha = flat[:, :3], flat[:, 3]

    targets = possible_shaded_rgbs(team_color)
    is_team = classify(rgb, targets, tolerance) & (alpha > 0)

    base = flat.copy()
    base[is_team, 3] = 0  # team-color pixels become transparent in the base

    mask = np.zeros_like(flat)
    mask[is_team] = (255, 255, 255, 255)  # flat, unshaded white -- per the documented convention

    Image.fromarray(base.reshape(h, w, 4), "RGBA").save(out_base)
    Image.fromarray(mask.reshape(h, w, 4), "RGBA").save(out_mask)

    total_opaque = int(np.sum(alpha > 0))
    team_count = int(np.sum(is_team))
    print(f"{team_count}/{total_opaque} opaque pixels classified as team color "
          f"({100.0 * team_count / max(1, total_opaque):.1f}%)")
    print(f"saved {out_base}")
    print(f"saved {out_mask}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("sprite", help="path to the input 2D sprite PNG")
    parser.add_argument("--color", type=int, nargs=3, default=list(DEFAULT_TEAM_COLOR),
                        metavar=("R", "G", "B"),
                        help=f"team-color palette RGB before shading (default {DEFAULT_TEAM_COLOR})")
    parser.add_argument("--tolerance", type=int, default=DEFAULT_TOLERANCE,
                        help=f"per-channel match tolerance (default {DEFAULT_TOLERANCE})")
    parser.add_argument("--out-base", default=None, help="output base PNG path")
    parser.add_argument("--out-mask", default=None, help="output mask PNG path")
    args = parser.parse_args()

    if not os.path.isfile(args.sprite):
        print(f"error: file not found: {args.sprite}", file=sys.stderr)
        sys.exit(1)

    stem, _ext = os.path.splitext(args.sprite)
    out_base = args.out_base or f"{stem}_base.png"
    out_mask = args.out_mask or f"{stem}_mask.png"

    split(args.sprite, tuple(args.color), args.tolerance, out_base, out_mask)


if __name__ == "__main__":
    main()
