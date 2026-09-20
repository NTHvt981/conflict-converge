#!/usr/bin/env python3
"""
convert_vox_to_sprite.py -- turn any MagicaVoxel .vox model into a static,
8-direction orthographic pixel-art sprite sheet. No rig, no animation: one
flat raycast render per direction of the model exactly as it sits in the
file.

Usage:
    python convert_vox_to_sprite.py <model>.vox [--angle ANGLE] [--scale SCALE]
                                     [--out PATH]

    python convert_vox_to_sprite.py <pose1>.vox <pose2>.vox ... [same flags]
    python convert_vox_to_sprite.py <folder> [same flags]
        Multiple files -- or a folder, expanded to every .vox file directly
        inside it (alphabetically, non-recursive) -- combine into ONE
        multi-frame sheet, one input file per row, sharing a single camera
        fit -- for this project's hand-authored-separate-pose-files
        convention (e.g. infantry_generic's run1.vox..run4.vox), the same
        treatment a single multi-model .vox file already gets (see below).
        All files must share the same palette; the first file's is used for
        every frame's colors, with a warning if a later file's differs.
        Output defaults to a name derived from the files' shared prefix
        (e.g. run1..run4 -> run_2d.png), overridable with --out.

--angle / -a is the camera's elevation angle above the ground plane, in
degrees: 0 = horizontal (pure side view), 90 = straight top-down. Default
45 (a generic 3/4-ish overhead angle). (--degree still works as an alias.)

--scale sets the output pixels per voxel unit (uniform, never
stretched). The tile always auto-sizes to the model's projected footprint
at this scale (see "recenter + resize" below), padded a little, then
rounded up to a multiple of 16 per axis (independently for width and
height -- a tall, narrow subject gets a tall, narrow tile instead of being
forced square). Default 1.0.

Whatever scale is used, the model is always scaled uniformly (never
stretched) and placed dead center in the frame -- across all 8 directions
it rotates in place around a fixed point; it never shifts around within
the tile from one direction to the next.

Recenter + resize: the camera fit (see compute_camera_fit) has to reserve
enough canvas for the largest silhouette across all 8 directions and both
axes, which usually leaves a lot of unused, off-center padding for any one
direction (e.g. a tall thin character needs a tall tile, but that same
tall tile is far wider than any single direction's silhouette actually
needs). After the initial render, this tool measures the tight
non-transparent bounding box shared across every direction/frame tile,
then re-crops every tile so that box sits dead-center and shrinks the
tile to that tight box (padded a little, then rounded up to a multiple of
16) instead of the old world-space auto-fit size, so the output is always
a tight tile that isn't mostly empty margin.

Output: "<model>_2d.png" written next to the input file. 8 columns -- one
tile per direction, 45 degrees apart around the vertical axis, starting at
facing = 0. If the .vox file contains multiple models (MagicaVoxel scenes
with several objects, or -- the common case -- separate poses of the same
character bundled in one file, e.g. hand-authored animation frames), each
model becomes its own ROW, top to bottom in file order, all sharing one
camera fit (so the subject stays the same scale and pivots around the same
point in every row -- flipping through rows in order plays like an
animation). A single-model file (the common case) is just one row, same
as before.

Rendering approach (matches the raycasting technique already used by this
project's other preview/bake scripts, e.g. lighttank/preview_tank.py and
lighttank/render_tank_sprites.py): per output pixel, cast a ray through the
voxel cloud (bucketed by rounded rotated-X for speed) and shade the nearest
hit by cardinal face (+y/-y/+z/-z only -- side +-x faces aren't
distinguished, the same simplification the existing renderers use).

Limitations (kept simple on purpose -- this is a flat, no-rig conversion
tool, not a scene importer):
  - Reads plain SIZE/XYZI/RGBA chunks. Scene-graph transform chunks
    (nTRN/nGRP/nSHP) are ignored, so multiple models are NOT positioned
    relative to each other via any scene-graph placement -- each is read
    in its own local coordinates and treated as an independent frame/row.
    This matches how this project's own multi-pose .vox files are built
    (each model is a complete standalone pose, not a scene-graph part),
    and only breaks down for a true multi-object *scene* export where
    models are meant to be composited via their transform nodes.
  - Requires an RGBA chunk (a custom palette) in the file. Most exported
    .vox files include one; if it's missing, a synthetic fallback palette
    is used and a warning is printed, since MagicaVoxel's built-in default
    palette isn't reproduced here.
"""
import argparse
import itertools
import math
import os
import struct
import sys
import time

import numpy as np
from PIL import Image

EPS = 0.5  # each voxel is a unit cube centered on its integer coordinate
N_DIRS = 8
# Compass label for each direction column d (facing = d * 45deg), in order.
DIRECTION_LABELS = ["top", "top left", "left", "bottom left",
                    "bottom", "bottom right", "right", "top right"]
MARGIN_FACTOR = 1.03  # small safety pad so edge voxels never clip
DEFAULT_PIXELS_PER_UNIT = 1.0  # default --scale (output pixels per voxel unit)
AUTO_SIZE_MULTIPLE = 16  # auto-sized tile rounds up to this (--scale always auto-sizes tight tiles)
AUTO_SIZE_PAD = 4  # total breathing-room pixels (both sides combined) added per axis before rounding


# --------------------------------------------------------------------------
# .vox reader: generic RIFF-style chunk walk, tolerant of unknown chunk
# types (skips them via their own declared size), collects every top-level
# SIZE/XYZI pair (handles multi-model files) and the last RGBA palette seen.
# --------------------------------------------------------------------------
def read_vox(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[0:4] != b"VOX ":
        raise ValueError(f"{path}: not a .vox file (missing 'VOX ' magic bytes)")

    def read_chunk_header(pos):
        cid = data[pos:pos + 4]
        n, m = struct.unpack("<ii", data[pos + 4:pos + 12])
        content_start = pos + 12
        return cid, content_start, content_start + n + m

    cid, cstart, cend = read_chunk_header(8)
    if cid != b"MAIN":
        raise ValueError(f"{path}: expected a MAIN chunk, found {cid!r}")

    pos = cstart
    end_main = cend
    models = []  # list of (size_xyz, [(x,y,z,color_byte), ...])
    pending_size = None
    palette = None
    while pos < end_main:
        cid, cstart, nend = read_chunk_header(pos)
        if cid == b"SIZE":
            pending_size = struct.unpack("<iii", data[cstart:cstart + 12])
        elif cid == b"XYZI":
            if pending_size is None:
                raise ValueError(f"{path}: XYZI chunk with no preceding SIZE chunk")
            (num_voxels,) = struct.unpack("<i", data[cstart:cstart + 4])
            off = cstart + 4
            voxels = []
            for _ in range(num_voxels):
                x, y, z, c = data[off:off + 4]
                voxels.append((x, y, z, c))
                off += 4
            models.append((pending_size, voxels))
            pending_size = None
        elif cid == b"RGBA":
            palette = []
            off = cstart
            for _ in range(256):
                r, g, b, a = data[off:off + 4]
                palette.append((r, g, b, a))
                off += 4
        # every other chunk type (PACK, nTRN, nGRP, nSHP, MATL, LAYR, ...)
        # is intentionally skipped -- see module docstring's limitations.
        pos = nend

    if not models:
        raise ValueError(f"{path}: no voxel data found (no SIZE/XYZI chunks)")
    return models, palette


def fallback_palette():
    """Synthetic palette used when the .vox has no RGBA chunk. Distinct
    hues so voxels stay visually distinguishable; not MagicaVoxel's real
    default palette."""
    import colorsys
    pal = []
    for i in range(256):
        h = (i * 0.6180339887) % 1.0  # golden-ratio hue spread
        r, g, b = colorsys.hsv_to_rgb(h, 0.55, 0.95)
        pal.append((int(r * 255), int(g * 255), int(b * 255), 255))
    return pal


def load_viewer_frames(path):
    """Returns (frames, palette_rgb) where frames is a list -- one entry
    per model in the .vox file, in file order -- of voxel lists, each a
    list of (x, y_up, z_depth, color_index0based) tuples in this project's
    usual viewer space (x, y=up, z=depth). palette_rgb is a list of
    (r,g,b) tuples, 0-indexed (already resolved from the .vox file's
    1-indexed palette byte -> palette[byte-1] convention). A normal
    single-model file just returns a single-entry list."""
    models, raw_palette = read_vox(path)

    if raw_palette is None:
        print("warning: no RGBA chunk in this file -- using a synthetic "
              "fallback palette (colors will not match MagicaVoxel's "
              "built-in default palette).")
        raw_palette = fallback_palette()

    frames = []
    for _size, raw_voxels in models:
        frame = []
        for (x, y, z, c) in raw_voxels:
            # raw vox space (x=width, y=depth, z=height) -> viewer space
            # (x, y=up, z=depth), matching this project's convention. The x
            # negation preserves handedness: a plain (x, z, y) swap is a
            # reflection that mirrors the rendered model (e.g. text reads
            # backwards), so negate x to make it a proper rotation.
            frame.append((-x, z, y, c - 1))
        frames.append(frame)
    palette_rgb = [tuple(p[:3]) for p in raw_palette]
    return frames, palette_rgb


# --------------------------------------------------------------------------
# Shading: same ambient + 3-light setup used by the project's other voxel
# renderers, applied per cardinal face.
# --------------------------------------------------------------------------
AMBIENT = 0.42
_LIGHTS_RAW = [((0.4, 0.9, 0.5), 0.9), ((-0.6, 0.3, 0.3), 0.22), ((0.0, 0.4, -0.8), 0.20)]


def _normalize(v):
    length = math.sqrt(sum(c * c for c in v))
    return tuple(c / length for c in v)


LIGHTS = [(_normalize(d), s) for d, s in _LIGHTS_RAW]
FACE_NORMALS = {"+y": (0, 1, 0), "-y": (0, -1, 0), "+z": (0, 0, 1), "-z": (0, 0, -1)}


def shade(rgb, normal):
    d = AMBIENT
    for light_dir, strength in LIGHTS:
        d += max(0.0, sum(a * b for a, b in zip(normal, light_dir))) * strength
    d = min(d, 1.45)
    return tuple(min(255, int(c * d)) for c in rgb)


def build_shade_table(palette_rgb):
    return {ci: {face: shade(rgb, n) for face, n in FACE_NORMALS.items()}
            for ci, rgb in enumerate(palette_rgb)}


# --------------------------------------------------------------------------
# Camera fit: figures out, for a given tilt, the world-space point the
# camera looks at -- (cx, ground_y, cz), the bottom-center of the model
# (ground level, halfway across its footprint), not its vertical middle --
# and, per direction, how far the projected content extends from that
# point (needed_x[d] horizontally, needed_yz[d] along the combined
# height/depth screen axis). These are affine in x/y/z, so checking the 8
# bounding-box corners is sufficient to get the exact (not approximate)
# extents; no sampling every voxel needed.
#
# Looking at the bottom-center means the canvas ends up with the model's
# feet near the middle of the frame and empty space below (the camera
# looks slightly down past the model's base, same as aiming at a
# character's feet rather than its chest) -- that's intentional, matching
# this tool's reference framing, not a bug.
#
# The look-at point (cx, cz) is the SAME point in world space for every
# direction, and every tile is exactly `half_w`/`half_h` pixels wide/tall
# centered on it -- so the model never shifts within the frame between
# directions, it only rotates in place around a fixed screen-center point.
# --------------------------------------------------------------------------
def compute_camera_fit(voxels, tilt_rad):
    xs = [v[0] for v in voxels]
    ys = [v[1] for v in voxels]
    zs = [v[2] for v in voxels]
    cx, cz = (min(xs) + max(xs)) / 2.0, (min(zs) + max(zs)) / 2.0
    ground_y = min(ys) - EPS  # bottom face of the lowest voxel: true ground contact
    hx = (max(xs) - min(xs)) / 2.0 + EPS
    hz = (max(zs) - min(zs)) / 2.0 + EPS

    st, ct = math.sin(tilt_rad), math.cos(tilt_rad)
    corners_xz = list(itertools.product([cx - hx, cx + hx], [cz - hz, cz + hz]))
    corners_y = [min(ys) - EPS, max(ys) + EPS]

    target_x_per_dir = []
    needed_x = 0.0   # world half-extent needed along the screen-horizontal axis
    needed_yz = 0.0  # world half-extent needed along the screen-vertical axis
    for d in range(N_DIRS):
        facing = d * math.pi / 4
        fc, fs = math.cos(-facing), math.sin(-facing)
        target_x = cx * fc - cz * fs
        target_x_per_dir.append(target_x)
        for (x, z) in corners_xz:
            rx = x * fc - z * fs
            rz = x * fs + z * fc
            needed_x = max(needed_x, abs(rx - target_x))
            for y in corners_y:
                needed_yz = max(needed_yz, abs(ct * (ground_y - y) + st * rz))

    return ground_y, target_x_per_dir, needed_x * MARGIN_FACTOR, needed_yz * MARGIN_FACTOR


def resolve_canvas(needed_x, needed_yz, scale):
    """Turns the world-space fit requirements into a final (width, height,
    pixels_per_unit). The scale is the output pixels per voxel unit; the
    tile auto-sizes to the model's projected footprint at this scale."""
    ppu = scale
    side = max(16, int(math.ceil(2 * max(needed_x, needed_yz) * ppu)))
    if side % 2:
        side += 1
    return side, side, ppu


# --------------------------------------------------------------------------
# Render one direction: bucket-by-column raycast, nearest hit per pixel,
# cardinal-face shading. Same core technique as preview_tank.py.
# --------------------------------------------------------------------------
def render_direction(voxels, shade_table, facing_rad, tilt_rad, target_x, target_y,
                     ppu, width, height):
    st, ct = math.sin(tilt_rad), math.cos(tilt_rad)
    fc, fs = math.cos(-facing_rad), math.sin(-facing_rad)

    buckets = {}
    for (x, y, z, ci) in voxels:
        rx = x * fc - z * fs
        rz = x * fs + z * fc
        buckets.setdefault(round(rx), []).append((rx, y, rz, ci))

    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    px = img.load()
    half_w, half_h = width / 2.0, height / 2.0

    for py in range(height):
        dy_world = (py - half_h + 0.5) / ppu
        oy = target_y - dy_world * ct
        oz = dy_world * st
        for pxi in range(width):
            dx_world = (pxi - half_w + 0.5) / ppu
            ox = target_x + dx_world
            xk = round(ox)

            best_t, best_ci, best_is_y_face, best_vy, best_vz = 1e18, -1, True, 0.0, 0.0
            for bk in (xk - 1, xk, xk + 1):
                bucket = buckets.get(bk)
                if not bucket:
                    continue
                for (vx, vy, vz, ci) in bucket:
                    if not (vx - EPS <= ox <= vx + EPS):
                        continue
                    ty0, ty1 = (oy - (vy + EPS)) / st, (oy - (vy - EPS)) / st
                    if ty0 > ty1:
                        ty0, ty1 = ty1, ty0
                    tz0, tz1 = (oz - (vz + EPS)) / ct, (oz - (vz - EPS)) / ct
                    if tz0 > tz1:
                        tz0, tz1 = tz1, tz0
                    t_enter, t_exit = max(ty0, tz0), min(ty1, tz1)
                    if t_enter > t_exit or t_enter >= best_t:
                        continue
                    best_t, best_ci = t_enter, ci
                    best_is_y_face, best_vy, best_vz = (ty0 >= tz0), vy, vz

            if best_ci < 0:
                continue
            if best_is_y_face:
                hy = oy - best_t * st
                face = "+y" if hy > best_vy else "-y"
            else:
                hz = oz - best_t * ct
                face = "+z" if hz > best_vz else "-z"
            px[pxi, py] = shade_table[best_ci][face] + (255,)

    return img


# --------------------------------------------------------------------------
# Recenter + resize: shrink-wrap every tile to the sprite's own tight pixel
# footprint instead of the much larger canvas compute_camera_fit had to
# reserve to avoid clipping the largest silhouette across all 8 directions.
# --------------------------------------------------------------------------
def tight_content_bounds(sheet, n_dirs, n_frames, tile_w, tile_h):
    """Scans every direction/frame tile in `sheet` and returns the tight
    non-transparent pixel bounding box (min_x, min_y, max_x, max_y), in
    coordinates LOCAL to a single tile (relative to that tile's own
    top-left corner), unioned across every tile. Every tile shares one
    fixed camera fit, so this union is guaranteed to contain every
    direction/frame's actual content -- cropping to it can't clip
    anything. Returns None if the whole sheet is empty."""
    alpha = np.asarray(sheet)[:, :, 3]
    min_x, min_y = tile_w, tile_h
    max_x, max_y = -1, -1
    for f in range(n_frames):
        for d in range(n_dirs):
            tile_alpha = alpha[f * tile_h:(f + 1) * tile_h, d * tile_w:(d + 1) * tile_w]
            ys, xs = np.nonzero(tile_alpha)
            if xs.size == 0:
                continue
            min_x, max_x = min(min_x, int(xs.min())), max(max_x, int(xs.max()))
            min_y, max_y = min(min_y, int(ys.min())), max(max_y, int(ys.max()))
    if max_x < 0:
        return None
    return min_x, min_y, max_x, max_y


def round_up_to_multiple(value, multiple):
    return max(multiple, -(-value // multiple) * multiple)


def align_columns_to_baseline(sheet, n_dirs, n_frames, tile_w, tile_h):
    """Aligns every direction's own ground contact to a shared row.

    The look-at pivot (cx, ground_y, cz) is a single fixed 3D point that
    gets correctly reprojected for every facing -- it never drifts. But
    this renderer's tilt couples height and depth onto one screen axis
    (see render_direction's oy/oz), so a pose with real depth asymmetry
    (e.g. a running stride, whose forward/backward leg extent sits along
    the character's own local x-axis) partially projects that
    forward/backward extent onto the vertical screen axis at oblique
    facings -- 0/180 degrees see none of it, 45/135/etc. see the most.
    The pivot stays put; the silhouette's apparent height around it does
    not, producing a direction-dependent vertical wobble across the 8
    columns even though every frame within a single direction stays
    consistent with the others.

    Fix: shift each column (all its frames together, so within-direction
    motion is untouched) down so every column's own lowest content pixel
    lands on the same row -- the lowest-reaching column's row, so every
    shift is >= 0 and nothing needs to move off either edge of the tile.

    CAUTION -- opt-in only (see the `--align-baseline` flag), not a
    default: this assumes the lowest visible pixel in every direction is
    the same kind of "grounded" part (e.g. a standing character's feet).
    That's not true for every model -- a tank's gun barrel, tilted toward
    the camera at some facings, can dip lower on screen than the tracks
    at THAT facing while the tracks are lowest at others. Applying this to
    a model like that drags the hull around to chase the barrel instead of
    fixing anything. Only enable it for subjects where the bottom-most
    point is reliably the actual ground contact in every direction.
    """
    alpha = np.asarray(sheet)[:, :, 3]
    col_max_y = []
    for d in range(n_dirs):
        max_y = -1
        for f in range(n_frames):
            tile_alpha = alpha[f * tile_h:(f + 1) * tile_h, d * tile_w:(d + 1) * tile_w]
            ys, xs = np.nonzero(tile_alpha)
            if xs.size:
                max_y = max(max_y, int(ys.max()))
        col_max_y.append(max_y)

    valid = [m for m in col_max_y if m >= 0]
    if not valid or max(valid) == min(valid):
        return sheet  # empty sheet, or already aligned -- nothing to do

    target_bottom = max(valid)
    new_sheet = Image.new("RGBA", sheet.size, (0, 0, 0, 0))
    for d in range(n_dirs):
        if col_max_y[d] < 0:
            continue
        shift = target_bottom - col_max_y[d]  # always >= 0, see docstring
        for f in range(n_frames):
            box = (d * tile_w, f * tile_h, (d + 1) * tile_w, (f + 1) * tile_h)
            tile_img = sheet.crop(box)
            new_sheet.paste(tile_img, (d * tile_w, f * tile_h + shift), tile_img)
    return new_sheet


def recenter_and_resize(sheet, n_dirs, n_frames, tile_w, tile_h, final_w, final_h, bounds):
    """Rebuilds the sheet at (final_w, final_h) per tile: every tile is
    cropped to the shared tight `bounds` and pasted dead-center in the new
    tile. One crop+paste per tile both recenters the sprite (it no longer
    sits wherever the ground-anchored camera happened to leave it within
    the old, larger tile) and shrinks the tile to fit it."""
    min_x, min_y, max_x, max_y = bounds
    content_w, content_h = max_x - min_x + 1, max_y - min_y + 1
    paste_x = (final_w - content_w) // 2
    paste_y = (final_h - content_h) // 2

    new_sheet = Image.new("RGBA", (final_w * n_dirs, final_h * n_frames), (0, 0, 0, 0))
    for f in range(n_frames):
        for d in range(n_dirs):
            box = (d * tile_w + min_x, f * tile_h + min_y,
                   d * tile_w + max_x + 1, f * tile_h + max_y + 1)
            crop = sheet.crop(box)
            new_sheet.paste(crop, (d * final_w + paste_x, f * final_h + paste_y), crop)
    return new_sheet


def render_sheet(frames, palette_rgb, degree, scale, out_path, align_baseline=False):
    """Core render pipeline, shared by both single-file (`convert`) and
    multi-file (`convert_many`) entry points: every entry in `frames` is one
    row (one animation frame), all sharing one camera fit so the subject
    stays the same scale and pivots around the same point in every
    row/direction (required for the rows to read as a coherent animation
    instead of jittering/rescaling frame to frame -- and, for `convert_many`,
    for separate pose files to line up as if they were one animation)."""
    t0 = time.time()
    tilt_rad = math.radians(degree)
    shade_table = build_shade_table(palette_rgb)
    # Fit the camera to every frame's combined bounding box, not each
    # frame's own -- see this function's docstring.
    all_voxels = [v for frame in frames for v in frame]
    target_y, target_x_per_dir, needed_x, needed_yz = compute_camera_fit(all_voxels, tilt_rad)

    out_width, out_height, ppu = resolve_canvas(needed_x, needed_yz, scale)
    print(f"angle={degree}  tile size={out_width}x{out_height}px  scale={ppu:.2f}px/unit")
    dir_list = ", ".join(DIRECTION_LABELS)
    print(f"directions (left to right): {dir_list}")

    sheet = Image.new("RGBA", (out_width * N_DIRS, out_height * len(frames)), (0, 0, 0, 0))
    for f_idx, frame_voxels in enumerate(frames):
        for d in range(N_DIRS):
            facing = d * math.pi / 4
            tile = render_direction(frame_voxels, shade_table, facing, tilt_rad,
                                    target_x_per_dir[d], target_y, ppu, out_width, out_height)
            sheet.paste(tile, (d * out_width, f_idx * out_height), tile)
        print(f"  frame {f_idx + 1}/{len(frames)} done ({time.time() - t0:.1f}s elapsed)")

    if align_baseline:
        sheet = align_columns_to_baseline(sheet, N_DIRS, len(frames), out_width, out_height)

    bounds = tight_content_bounds(sheet, N_DIRS, len(frames), out_width, out_height)
    if bounds is None:
        print("warning: rendered sheet is fully transparent -- skipping recenter/resize")
    else:
        min_x, min_y, max_x, max_y = bounds
        content_w, content_h = max_x - min_x + 1, max_y - min_y + 1
        final_width = round_up_to_multiple(content_w + AUTO_SIZE_PAD, AUTO_SIZE_MULTIPLE)
        final_height = round_up_to_multiple(content_h + AUTO_SIZE_PAD, AUTO_SIZE_MULTIPLE)
        sheet = recenter_and_resize(sheet, N_DIRS, len(frames), out_width, out_height,
                                    final_width, final_height, bounds)
        out_width, out_height = final_width, final_height
        print(f"recentered + resized: tile size now {out_width}x{out_height}px "
              f"(actual sprite content {content_w}x{content_h}px)")

    sheet.save(out_path)
    print(f"saved {out_path}  ({time.time() - t0:.1f}s total)")
    return out_path


def _load_and_report(vox_path):
    frames, palette_rgb = load_viewer_frames(vox_path)
    total_voxels = sum(len(f) for f in frames)
    if len(frames) > 1:
        print(f"  {vox_path}: {len(frames)} models (frames), {total_voxels} voxels total, "
              f"{len(palette_rgb)}-color palette")
    else:
        print(f"  {vox_path}: {total_voxels} voxels, {len(palette_rgb)}-color palette")
    return frames, palette_rgb


def convert(vox_path, degree, scale, align_baseline=False):
    frames, palette_rgb = _load_and_report(vox_path)
    out_dir = os.path.dirname(os.path.abspath(vox_path))
    base = os.path.splitext(os.path.basename(vox_path))[0]
    out_path = os.path.join(out_dir, f"{base}_2d.png")
    return render_sheet(frames, palette_rgb, degree, scale, out_path, align_baseline)


def derive_multi_out_path(vox_paths):
    """Picks an output filename for a combined multi-file sheet: the
    longest common alphabetic prefix of the input basenames, with any
    trailing digits/underscore trimmed (e.g. run1.vox..run4.vox -> "run"),
    falling back to the first file's own name if the inputs don't share
    one (e.g. genuinely different models rendered together for a
    side-by-side comparison, not frames of one animation)."""
    stems = [os.path.splitext(os.path.basename(p))[0] for p in vox_paths]
    prefix = os.path.commonprefix(stems).rstrip("_0123456789")
    base = prefix if prefix else stems[0]
    out_dir = os.path.dirname(os.path.abspath(vox_paths[0]))
    return os.path.join(out_dir, f"{base}_2d.png")


def convert_many(vox_paths, degree, scale, out_path=None, align_baseline=False):
    """Combines several standalone single-pose .vox files (this project's
    convention for hand-authored animation frames, e.g. infantry_generic's
    run1..run4.vox) into ONE multi-frame sheet, one input file per row,
    sharing a single camera fit -- the same treatment a single multi-model
    .vox file gets from `convert`, just sourced from separate files instead
    of separate MAIN-chunk models."""
    print(f"loading {len(vox_paths)} files as frames:")
    frames = []
    palette_rgb = None
    for path in vox_paths:
        file_frames, file_palette = _load_and_report(path)
        frames.extend(file_frames)
        if palette_rgb is None:
            palette_rgb = file_palette
        elif file_palette != palette_rgb:
            print(f"  warning: {path}'s palette differs from the first file's -- "
                  f"using the first file's palette for every frame's colors.")

    if out_path is None:
        out_path = derive_multi_out_path(vox_paths)
    return render_sheet(frames, palette_rgb, degree, scale, out_path, align_baseline)


def main():
    parser = argparse.ArgumentParser(
        description="Convert a .vox model into a static 8-direction orthographic sprite sheet.")
    parser.add_argument("vox_file", nargs="+",
                        help="path to the input .vox file. Give more than one -- or a "
                             "folder, expanded to every .vox file directly inside it, "
                             "alphabetically -- to combine several standalone single-pose "
                             "files (e.g. hand-authored run1.vox..run4.vox) into one "
                             "multi-frame sheet, one file per row, sharing a single camera "
                             "fit -- the same treatment a single multi-model .vox file "
                             "already gets. A folder argument can be mixed with explicit "
                             "file arguments in any order.")
    parser.add_argument("--angle", "--degree", "-a", dest="degree", type=float, default=45.0,
                        help="camera elevation angle above the ground, in degrees "
                             "(0=horizontal side view, 90=top-down). Default 45.")
    parser.add_argument("--scale", type=float, default=DEFAULT_PIXELS_PER_UNIT,
                        help="output pixels per voxel unit (uniform, never stretched). "
                             "The tile auto-sizes to the model's projected footprint at this "
                             "scale, then is padded and rounded up to a multiple of 16. "
                             "Default 1.0.")
    parser.add_argument("--out", "-o", type=str, default=None,
                        help="output PNG path. Default: <model>_2d.png next to the input "
                             "(single file), or a name derived from the shared prefix of "
                             "all input filenames (multiple files).")
    parser.add_argument("--align-baseline", action="store_true",
                        help="shift each direction so its own lowest visible pixel lines up "
                             "with the others, removing the vertical wobble a pose with real "
                             "depth asymmetry (e.g. a running stride) produces across facings. "
                             "Off by default: it assumes the lowest pixel in every direction is "
                             "the same grounded part (true for a standing character's feet), "
                             "which is wrong for a model with an asymmetric protruding part "
                             "(e.g. a tank's gun barrel) that becomes the apparent lowest point "
                             "only at some facings -- only enable it when you've checked that "
                             "doesn't apply.")

    # "help" as a bare positional (no leading dashes) is a common enough
    # instinct -- argparse alone wouldn't catch it (it'd be treated as a
    # vox_file and fail with "file not found: help"), so special-case it.
    if sys.argv[1:] == ["help"]:
        parser.print_help()
        sys.exit(0)

    args = parser.parse_args()

    vox_paths = []
    for vox_path in args.vox_file:
        if os.path.isdir(vox_path):
            found = sorted(f for f in os.listdir(vox_path) if f.lower().endswith(".vox"))
            if not found:
                print(f"error: no .vox files found in folder: {vox_path}", file=sys.stderr)
                sys.exit(1)
            vox_paths.extend(os.path.join(vox_path, f) for f in found)
            continue
        if not os.path.isfile(vox_path) and not vox_path.lower().endswith(".vox"):
            candidate = vox_path + ".vox"
            if os.path.isfile(candidate):
                vox_path = candidate
        if not os.path.isfile(vox_path):
            print(f"error: file not found: {vox_path}", file=sys.stderr)
            sys.exit(1)
        vox_paths.append(vox_path)
    if not (0.0 <= args.degree <= 90.0):
        print(f"error: --angle must be between 0 and 90, got {args.degree}", file=sys.stderr)
        sys.exit(1)
    if args.scale <= 0:
        print(f"error: --scale must be positive, got {args.scale}", file=sys.stderr)
        sys.exit(1)

    # Exactly 0 or 90 degrees makes sin/cos of the tilt hit zero, which the
    # ray-box test divides by -- nudge a hair off the singularity. No
    # visible difference at this scale.
    degree = min(max(args.degree, 1e-3), 90.0 - 1e-3)

    if len(vox_paths) == 1 and args.out is None:
        convert(vox_paths[0], degree, args.scale, args.align_baseline)
    else:
        convert_many(vox_paths, degree, args.scale, args.out,
                     args.align_baseline)


if __name__ == "__main__":
    main()
