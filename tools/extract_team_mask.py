#!/usr/bin/env python3
"""
extract_team_mask.py -- splits an already-rendered 2D sprite PNG (output of
convert_vox_to_sprite.py) into the project's base+mask team-tint pair:

  <name>_base.png -- every pixel EXCEPT the team-color material, unchanged;
                     team-color pixels become transparent.
  <name>_mask.png -- ONLY the team-color material; everything else
                     transparent. By default baked flat pure white (no
                     shading); with --shaded, each face gets a gray level
                     that keeps the renderer's lighting (see below).

At runtime: draw the base sprite normally, then draw the mask sprite tinted
by the team's color (white * tint = tint) -- one pair of sheets serves every
team, no per-team bake.

Why this works on an already-shaded PNG (no voxel/color-index data needed):
convert_vox_to_sprite.py's shading is a small, exactly-known function of the
base palette color and which face was hit (shade() / FACE_NORMALS, imported
directly from that module) -- so the team-color material's possible
on-screen colors are a small, exactly-computable set (one shaded RGB per
face), not an open-ended range. Every pixel in the sprite is classified by
exact membership in that set, not by fuzzy hue-distance guessing. The
default tolerance of 1 only absorbs rounding; a larger value risks pulling
in other palette colors that happen to shade close to the team color.

--shaded mask: a face's gray level is its lighting intensity relative to the
brightest face (face_intensity(face) / max over faces), so the brightest
face stays pure white and dimmer faces are proportionally darker. Tinting
then gives tint * relative intensity: the brightest face shows the exact
team color and the others keep their relative shading.

Usage:
    python extract_team_mask.py <sprite>.png [--color R G B] [--tolerance N]
                                [--shaded] [--out-base PATH] [--out-mask PATH]

--color defaults to team_palette.TEAM_BLUE_RGB, the unit palette's "armor"
slot -- override for a sprite rendered from a different palette's
team-color slot.
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import convert_vox_to_sprite as cv
from team_palette import TEAM_BLUE_RGB

DEFAULT_TEAM_COLOR = TEAM_BLUE_RGB  # unit palette's "armor" slot
DEFAULT_TOLERANCE = 1  # per-channel; shading math is exact, this just absorbs rounding


def possible_shaded_rgbs(base_rgb):
    """Every (face, RGB) this base color can appear as once
    convert_vox_to_sprite.py's per-face shading is applied, using the exact
    same shade() function the renderer itself calls, so this is a precise
    reproduction, not an approximation."""
    return [(face, cv.shade(base_rgb, normal)) for face, normal in cv.FACE_NORMALS.items()]


def classify(pixels_rgb, targets, tolerance):
    """pixels_rgb: (N,3) array. targets: list of (r,g,b). Returns an int
    array holding, per pixel, the index of the first target within
    `tolerance` (Chebyshev/per-channel), or -1 where none matches."""
    match = np.full(pixels_rgb.shape[0], -1, dtype=np.int16)
    rgb = pixels_rgb.astype(np.int16)
    for i, t in enumerate(targets):
        hit = np.all(np.abs(rgb - np.array(t, dtype=np.int16)) <= tolerance, axis=1)
        match[hit & (match < 0)] = i
    return match


def split(sprite_path, team_color, tolerance, shaded, out_base, out_mask):
    img = Image.open(sprite_path).convert("RGBA")
    arr = np.array(img)
    h, w = arr.shape[:2]
    flat = arr.reshape(-1, 4)
    rgb, alpha = flat[:, :3], flat[:, 3]

    targets = possible_shaded_rgbs(team_color)
    match = classify(rgb, [t_rgb for _face, t_rgb in targets], tolerance)
    match[alpha == 0] = -1
    is_team = match >= 0

    base = flat.copy()
    base[is_team, 3] = 0  # team-color pixels become transparent in the base

    mask = np.zeros_like(flat)
    if shaded:
        intensity = {face: cv.face_intensity(n) for face, n in cv.FACE_NORMALS.items()}
        brightest = max(intensity.values())
        for i, (face, _t_rgb) in enumerate(targets):
            gray = round(255 * intensity[face] / brightest)
            mask[match == i] = (gray, gray, gray, 255)
    else:
        mask[is_team] = (255, 255, 255, 255)  # flat, unshaded white -- the default convention

    Image.fromarray(base.reshape(h, w, 4), "RGBA").save(out_base)
    Image.fromarray(mask.reshape(h, w, 4), "RGBA").save(out_mask)

    total_opaque = int(np.sum(alpha > 0))
    team_count = int(np.sum(is_team))
    print(f"{team_count}/{total_opaque} opaque pixels classified as team color "
          f"({100.0 * team_count / max(1, total_opaque):.1f}%)")
    print(f"saved {out_base}")
    print(f"saved {out_mask}")


def main():
    parser = argparse.ArgumentParser(
        description="Split a rendered sprite PNG into a base sprite and a team-color tint mask.")
    parser.add_argument("sprite", help="path to the input 2D sprite PNG")
    parser.add_argument("--color", type=int, nargs=3, default=list(DEFAULT_TEAM_COLOR),
                        metavar=("R", "G", "B"),
                        help=f"team-color palette RGB before shading (default {DEFAULT_TEAM_COLOR})")
    parser.add_argument("--tolerance", type=int, default=DEFAULT_TOLERANCE,
                        help=f"per-channel match tolerance (default {DEFAULT_TOLERANCE})")
    parser.add_argument("--shaded", action="store_true",
                        help="write per-face gray levels into the mask so tinting keeps the "
                             "renderer's lighting (default: flat white)")
    parser.add_argument("--out-base", default=None, help="output base PNG path")
    parser.add_argument("--out-mask", default=None, help="output mask PNG path")
    args = parser.parse_args()

    if not os.path.isfile(args.sprite):
        print(f"error: file not found: {args.sprite}", file=sys.stderr)
        sys.exit(1)

    stem, _ext = os.path.splitext(args.sprite)
    out_base = args.out_base or f"{stem}_base.png"
    out_mask = args.out_mask or f"{stem}_mask.png"

    split(args.sprite, tuple(args.color), args.tolerance, args.shaded, out_base, out_mask)


if __name__ == "__main__":
    main()
