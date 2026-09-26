#!/usr/bin/env python3
"""
stack_vox_to_sprite.py -- render a MagicaVoxel .vox model into a static
multi-direction pixel-art sprite sheet by sprite stacking: the model is cut
into horizontal slices (one per voxel layer), and every direction is drawn
by stacking those slices bottom to top, each rotated to the facing and
lifted up the screen by its height.

Usage:
    python stack_vox_to_sprite.py <model>.vox [--angle ANGLE] [--scale SCALE]
                                  [--dirs N] [--out PATH] [--align-baseline]
    python stack_vox_to_sprite.py <pose1>.vox <pose2>.vox ... [same flags]
    python stack_vox_to_sprite.py <folder> [same flags]

Options and sheet layout follow convert_vox_to_sprite.py, so the two tools'
sheets line up and extract_team_mask.py works on either:
  --angle / -a   camera elevation in degrees, 5 to 90 (90 = top-down),
                 default 45. Slices are edge-on toward 0 degrees, so side
                 views are convert_vox_to_sprite.py's job.
  --scale        output pixels per voxel, default 1.0.
  --dirs         number of directions, evenly spaced from facing 0, default 8
                 (the same columns as convert_vox_to_sprite.py).
  --out / -o     output PNG. Default: <model>_stack.png next to the input, or
                 <shared prefix>_stack.png for several files.
  --align-baseline  shift each direction so its lowest pixel lines up; see
                 convert_vox_to_sprite.align_columns_to_baseline for when
                 that is (and is not) safe.

One column per direction, one row per model. A file with several models
(MagicaVoxel animation frames) is laid out in animation-frame order (each
model's _f tag), not the order the models happen to be stored in. Tiles are
shrink-wrapped to the content shared by every tile, padded, and rounded up
to a multiple of 16 per axis.

How the stacking maps to a real camera: with elevation angle t, a
horizontal slice at height h projects to the screen squashed vertically by
sin(t) and lifted by h * cos(t). Drawing slices bottom to top is then an
exact painter's algorithm, since along every view ray a higher plane is
always nearer the camera. Each voxel layer is drawn as a run of slice
copies from its bottom (h - 0.5) to its top (h + 0.5), spaced closely
enough (at most one output pixel, tighter at low angles) that the walls
between layers stay solid. The top copy is
shaded as the voxel's top face (+y) and the copies below it as its side
walls (+z), using convert_vox_to_sprite.shade(), so a sprite shows exactly
the two shades per palette color that extract_team_mask.py matches.

Unlike convert_vox_to_sprite.py, which keeps every voxel cube aligned to the
screen and only moves its center, the slices rotate as a whole: at diagonal
facings the voxels turn with the model (diamond-shaped tops, pixel-stepped
edges), the characteristic sprite-stack look.
"""
import argparse
import math
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import convert_vox_to_sprite as cv
import split_vox
import vox_to_slices

EPS = 0.5  # each voxel is a unit cube centered on its integer coordinate
DEFAULT_DIRS = 8
MIN_ANGLE = 5.0  # horizontal slices turn edge-on toward 0 degrees


# --------------------------------------------------------------------------
# Loading: voxel grids per model, in animation-frame order.
# --------------------------------------------------------------------------
def load_models(path):
    """Returns (grids, palette_rgb). Each grid is an int16 array indexed
    [z, y, x] in raw .vox coordinates (z up) holding the 0-based palette
    index, or -1 for empty. palette_rgb has 256 (r, g, b) entries."""
    models, raw_palette = cv.read_vox(path)
    if raw_palette is None:
        print(f"  warning: {path} has no RGBA chunk -- using MagicaVoxel's default palette.")
        raw_palette = vox_to_slices.default_palette()[1:] + [(0, 0, 0, 255)]
    palette_rgb = [tuple(p[:3]) for p in raw_palette]

    _version, chunks = split_vox.read_chunks(path)
    order = split_vox.model_order(split_vox.parse_scene(chunks), len(models))

    grids = []
    for i in order:
        (sx, sy, sz), voxels = models[i]
        grid = np.full((sz, sy, sx), -1, dtype=np.int16)
        for x, y, z, c in voxels:
            grid[z, y, x] = c - 1
        grids.append(grid)
    return grids, palette_rgb


def viewer_bounds(grids):
    """Bounding box of every filled voxel in viewer space (X = -x, Y = z up,
    Z = y depth), the same convention convert_vox_to_sprite.py uses."""
    lo, hi = np.array([np.inf] * 3), np.array([-np.inf] * 3)
    for g in grids:
        z, y, x = np.nonzero(g >= 0)
        if x.size:
            pts = np.stack([-x, z, y], axis=1)
            lo, hi = np.minimum(lo, pts.min(0)), np.maximum(hi, pts.max(0))
    if not np.isfinite(lo).all():
        raise ValueError("model has no voxels")
    return lo, hi


# --------------------------------------------------------------------------
# Camera fit: the look-at point is the model's bottom-center (cx, ground_y,
# cz), fixed in world space, so the model rotates in place across every
# direction. Checking the 8 corners of the bounding box gives the exact
# screen extent, since projection is affine.
# --------------------------------------------------------------------------
def camera_fit(lo, hi, tilt_rad, n_dirs):
    cx, cz = (lo[0] + hi[0]) / 2.0, (lo[2] + hi[2]) / 2.0
    ground_y = lo[1] - EPS
    st, ct = math.sin(tilt_rad), math.cos(tilt_rad)
    targets, need_x, need_v = [], 0.0, 0.0
    for d in range(n_dirs):
        f = 2 * math.pi * d / n_dirs
        fc, fs = math.cos(-f), math.sin(-f)
        tx, tz = cx * fc - cz * fs, cx * fs + cz * fc
        targets.append((tx, tz))
        for x in (lo[0] - EPS, hi[0] + EPS):
            for z in (lo[2] - EPS, hi[2] + EPS):
                rx, rz = x * fc - z * fs, x * fs + z * fc
                need_x = max(need_x, abs(rx - tx))
                for y in (lo[1] - EPS, hi[1] + EPS):
                    need_v = max(need_v, abs(ct * (ground_y - y) + st * (rz - tz)))
    return ground_y, targets, need_x * cv.MARGIN_FACTOR, need_v * cv.MARGIN_FACTOR


# --------------------------------------------------------------------------
# Render one direction by stacking slices.
# --------------------------------------------------------------------------
def render_direction(grid, top_lut, side_lut, facing, tilt_rad, target, ground_y, ppu, w, h):
    st, ct = math.sin(tilt_rad), math.cos(tilt_rad)
    fc, fs = math.cos(-facing), math.sin(-facing)
    tx, tz = target
    sz, sy, sx = grid.shape

    py, px = np.mgrid[0:h, 0:w]
    screen_x = (px + 0.5 - w / 2.0) / ppu
    screen_v = (py + 0.5 - h / 2.0) / ppu  # down the screen, in voxel units
    rx = tx + screen_x

    out = np.zeros((h, w, 4), dtype=np.uint8)
    # Copies per layer: a layer's wall spans ct * ppu pixels, and one copy
    # covers a band only st * ppu pixels tall at low angles, so space copies
    # no further apart than the smaller of that band and one pixel.
    steps = max(1, math.ceil(ct * ppu / min(1.0, st * ppu)))
    for z in range(sz):
        layer = grid[z]
        if not (layer >= 0).any():
            continue
        for j in range(steps + 1):
            lift = z - EPS + j / steps
            # Inverse projection: screen point -> ground-plane point at this
            # height (rotated frame) -> unrotated viewer X/Z -> raw voxel x/y.
            rz = tz + (screen_v - ct * (ground_y - lift)) / st
            vx = rx * fc + rz * fs
            vz = -rx * fs + rz * fc
            xi = np.floor(-vx + 0.5).astype(np.int32)
            yi = np.floor(vz + 0.5).astype(np.int32)
            inside = (xi >= 0) & (xi < sx) & (yi >= 0) & (yi < sy)
            ci = np.full((h, w), -1, dtype=np.int16)
            ci[inside] = layer[yi[inside], xi[inside]]
            hit = ci >= 0
            lut = top_lut if j == steps else side_lut
            out[hit, :3] = lut[ci[hit]]
            out[hit, 3] = 255
    return Image.fromarray(out, "RGBA")


# --------------------------------------------------------------------------
# Sheet pipeline
# --------------------------------------------------------------------------
def render_sheet(grids, palette_rgb, degree, scale, n_dirs, out_path, align_baseline=False):
    tilt = math.radians(degree)
    top_lut = np.array([cv.shade(c, cv.FACE_NORMALS["+y"]) for c in palette_rgb], dtype=np.uint8)
    side_lut = np.array([cv.shade(c, cv.FACE_NORMALS["+z"]) for c in palette_rgb], dtype=np.uint8)

    ground_y, targets, need_x, need_v = camera_fit(*viewer_bounds(grids), tilt, n_dirs)
    tw, th, ppu = cv.resolve_canvas(need_x, need_v, scale)
    print(f"angle={degree:g}  directions={n_dirs}  frames={len(grids)}  scale={ppu:g}px/voxel")
    if n_dirs == cv.N_DIRS:
        print(f"directions (left to right): {', '.join(cv.DIRECTION_LABELS)}")

    sheet = Image.new("RGBA", (tw * n_dirs, th * len(grids)), (0, 0, 0, 0))
    for f, grid in enumerate(grids):
        for d in range(n_dirs):
            tile = render_direction(grid, top_lut, side_lut, 2 * math.pi * d / n_dirs, tilt,
                                    targets[d], ground_y, ppu, tw, th)
            sheet.paste(tile, (d * tw, f * th), tile)

    if align_baseline:
        sheet = cv.align_columns_to_baseline(sheet, n_dirs, len(grids), tw, th)

    bounds = cv.tight_content_bounds(sheet, n_dirs, len(grids), tw, th)
    if bounds is None:
        print("warning: rendered sheet is fully transparent -- skipping recenter/resize")
    else:
        min_x, min_y, max_x, max_y = bounds
        cw, ch = max_x - min_x + 1, max_y - min_y + 1
        fw = cv.round_up_to_multiple(cw + cv.AUTO_SIZE_PAD, cv.AUTO_SIZE_MULTIPLE)
        fh = cv.round_up_to_multiple(ch + cv.AUTO_SIZE_PAD, cv.AUTO_SIZE_MULTIPLE)
        sheet = cv.recenter_and_resize(sheet, n_dirs, len(grids), tw, th, fw, fh, bounds)
        print(f"tile size {fw}x{fh}px (sprite content {cw}x{ch}px)")

    sheet.save(out_path)
    print(f"saved {out_path}")
    return out_path


def default_out_path(vox_paths):
    """<model>_stack.png for one file; for several, the shared alphabetic
    prefix of their names with trailing digits/underscores trimmed
    (run1..run4 -> run_stack.png), else the first file's name."""
    stems = [os.path.splitext(os.path.basename(p))[0] for p in vox_paths]
    base = stems[0] if len(stems) == 1 else (os.path.commonprefix(stems).rstrip("_0123456789") or stems[0])
    return os.path.join(os.path.dirname(os.path.abspath(vox_paths[0])), f"{base}_stack.png")


def expand_inputs(args_files):
    paths = []
    for p in args_files:
        if os.path.isdir(p):
            found = sorted(f for f in os.listdir(p) if f.lower().endswith(".vox"))
            if not found:
                sys.exit(f"error: no .vox files found in folder: {p}")
            paths.extend(os.path.join(p, f) for f in found)
            continue
        if not os.path.isfile(p) and os.path.isfile(p + ".vox"):
            p += ".vox"
        if not os.path.isfile(p):
            sys.exit(f"error: file not found: {p}")
        paths.append(p)
    return paths


def main():
    parser = argparse.ArgumentParser(
        description="Render a .vox model into a multi-direction sprite sheet by sprite stacking.")
    parser.add_argument("vox_file", nargs="+",
                        help="input .vox file(s) or folder(s); several files become one sheet, "
                             "one model per row")
    parser.add_argument("--angle", "--degree", "-a", dest="degree", type=float, default=45.0,
                        help=f"camera elevation in degrees, {MIN_ANGLE:g}-90 (90=top-down). Default 45.")
    parser.add_argument("--scale", type=float, default=cv.DEFAULT_PIXELS_PER_UNIT,
                        help="output pixels per voxel. Default 1.0.")
    parser.add_argument("--dirs", type=int, default=DEFAULT_DIRS,
                        help=f"number of directions, evenly spaced. Default {DEFAULT_DIRS}.")
    parser.add_argument("--out", "-o", default=None, help="output PNG path")
    parser.add_argument("--align-baseline", action="store_true",
                        help="line up each direction's lowest pixel (see "
                             "convert_vox_to_sprite.py --align-baseline)")
    args = parser.parse_args()

    if not MIN_ANGLE <= args.degree <= 90.0:
        sys.exit(f"error: --angle must be between {MIN_ANGLE:g} and 90, got {args.degree:g} "
                 f"(slices are edge-on below that; use convert_vox_to_sprite.py for side views)")
    if args.scale <= 0:
        sys.exit(f"error: --scale must be positive, got {args.scale}")
    if args.dirs < 1:
        sys.exit(f"error: --dirs must be at least 1, got {args.dirs}")
    degree = args.degree

    vox_paths = expand_inputs(args.vox_file)
    grids, palette_rgb = [], None
    for p in vox_paths:
        g, pal = load_models(p)
        voxels = sum(int((m >= 0).sum()) for m in g)
        print(f"  {p}: {len(g)} model(s), {voxels} voxels")
        grids.extend(g)
        if palette_rgb is None:
            palette_rgb = pal
        elif pal != palette_rgb:
            print(f"  warning: {p}'s palette differs from the first file's -- "
                  f"using the first file's palette for every frame's colors.")

    render_sheet(grids, palette_rgb, degree, args.scale, args.dirs,
                 args.out or default_out_path(vox_paths), args.align_baseline)


if __name__ == "__main__":
    main()
