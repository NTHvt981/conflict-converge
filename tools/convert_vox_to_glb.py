#!/usr/bin/env python3
"""
convert_vox_to_glb.py -- turn MagicaVoxel .vox models into glTF 2.0 binary (.glb) meshes.

Usage:
    python tools/convert_vox_to_glb.py <input...> [--out PATH] [--scale F]
                                        [--no-center] [--no-linear] [--merge]

    Each input is a .vox file or a folder (expanded to every .vox file
    directly inside it, alphabetically, non-recursive).

    --out PATH: with a single input file, that exact file is written;
        with several input files, one combined .glb is written (every
        input's models as nodes, named with their source stem).
    No --out: one <stem>.glb is written beside each input file.
    --scale F: multiply all coordinates by F (default 1.0).
    --no-center: keep raw coordinates instead of recentering the model's
        bounding-box center to the origin.
    --no-linear: write raw normalized sRGB colors instead of converting
        to linear (glTF COLOR_0 is linear).
    --merge: combine every model of a file into a single mesh/node
        instead of one node per model.

The .vox reader is reused from the sibling sprite tool
(convert_vox_to_sprite.read_vox), so the same plain SIZE/XYZI/RGBA
chunks are understood and scene-graph transform chunks (nTRN/nGRP/nSHP)
are ignored -- each model is meshed in its own local coordinates.

Mesh generation: for each model, the solid voxel set is built and only
EXTERIOR faces are emitted (a quad per solid voxel per axis direction
whose neighbour is empty -- no interior geometry). Each quad is 4
unique vertices + 2 triangles with that face's axis normal and the
voxel's palette color (no vertex dedup).

Coordinates: .vox is x=width, y=depth, z=up; mapped to glTF y-up as
(x, z, -y), then scaled, then (by default) recentered so the model's
bounding-box center sits at the origin. Every model of one file shares
the same centering offset so multi-model files stay in one frame.

GLB writing: hand-written glTF 2.0 binary with no third-party
dependencies (stdlib + numpy only) -- 12-byte header, a JSON chunk and
a BIN chunk, one buffer, one material, per-model bufferViews/accessors
for POSITION (with min/max), NORMAL, COLOR_0 and uint32 indices.

Limitations (same reader scope as the sibling tool):
  - Requires SIZE/XYZI chunks; files with no voxel data are rejected.
  - Files without an RGBA chunk get a synthetic fallback palette plus
    the same warning the sibling tool prints (colors will not match
    MagicaVoxel's built-in default palette).
"""
import argparse
import json
import math
import os
import struct
import sys

import numpy as np

import convert_vox_to_sprite as cv

# --------------------------------------------------------------------------
# Face table: one entry per axis direction -- neighbour offset in .vox
# space, outward unit normal, and the 4 quad corners (relative to the
# voxel's integer minimum corner) wound counter-clockwise seen from
# outside, so front faces stay CCW after the rotation into glTF space.
# --------------------------------------------------------------------------
FACES = [
    ((1, 0, 0), (1.0, 0.0, 0.0),
     [(1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1)]),
    ((-1, 0, 0), (-1.0, 0.0, 0.0),
     [(0, 0, 0), (0, 0, 1), (0, 1, 1), (0, 1, 0)]),
    ((0, 1, 0), (0.0, 1.0, 0.0),
     [(0, 1, 0), (0, 1, 1), (1, 1, 1), (1, 1, 0)]),
    ((0, -1, 0), (0.0, -1.0, 0.0),
     [(0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)]),
    ((0, 0, 1), (0.0, 0.0, 1.0),
     [(0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)]),
    ((0, 0, -1), (0.0, 0.0, -1.0),
     [(0, 0, 0), (0, 1, 0), (1, 1, 0), (1, 0, 0)]),
]

GLTF_MAGIC = 0x46546C67  # "glTF" little-endian
GLTF_VERSION = 2
JSON_CHUNK_TYPE = 0x4E4F534A
BIN_CHUNK_TYPE = 0x004E4942
COMP_FLOAT = 5126
COMP_UINT = 5125
TARGET_ARRAY_BUFFER = 34962
TARGET_ELEMENT_ARRAY_BUFFER = 34963
GENERATOR = "conflict-converge tools/convert_vox_to_glb.py"


def srgb_to_linear(c):
    """Standard sRGB -> linear piecewise transfer (c in 0..1)."""
    if c <= 0.04045:
        return c / 12.92
    return ((c + 0.055) / 1.055) ** 2.4


def vox_to_gltf(x, y, z):
    """Map .vox coords (x=width, y=depth, z=up) to glTF y-up."""
    return (x, z, -y)


def emit_faces(solid, palette, linear):
    """Build exterior-face geometry for one model's solid voxel set.

    solid maps (x, y, z) -> 0-based palette index. Returns (positions,
    normals, colors, indices) as plain lists in glTF y-up space but
    pre-scale and pre-centering; colors are RGBA floats 0..1 (linear
    unless linear=False) and indices are flat (3 per triangle).
    """
    positions, normals, colors, indices = [], [], [], []
    for (x, y, z), ci in solid.items():
        r, g, b, a = palette[ci]
        frgb = [r / 255.0, g / 255.0, b / 255.0]
        if linear:
            frgb = [srgb_to_linear(c) for c in frgb]
        rgba = (frgb[0], frgb[1], frgb[2], a / 255.0)
        for (ox, oy, oz), normal, corners in FACES:
            if (x + ox, y + oy, z + oz) in solid:
                continue  # interior face -- skip
            base = len(positions)
            gn = vox_to_gltf(*normal)
            for (dx, dy, dz) in corners:
                positions.append(vox_to_gltf(x + dx, y + dy, z + dz))
                normals.append(gn)
                colors.append(rgba)
            indices.extend((base, base + 1, base + 2,
                            base, base + 2, base + 3))
    return positions, normals, colors, indices


def file_meshes(vox_path, scale, center, linear, merge):
    """Mesh every model in one .vox file; returns (stem, node_list,
    model_count) where each node is (name, pos, nrm, col, idx) as numpy
    arrays. The centering offset is computed over the file's combined
    bounding box so all its models share one frame."""
    stem = os.path.splitext(os.path.basename(vox_path))[0]
    models, palette = cv.read_vox(vox_path)
    if palette is None:
        print("warning: no RGBA chunk in this file -- using a synthetic "
              "fallback palette (colors will not match MagicaVoxel's "
              "built-in default palette).")
        palette = cv.fallback_palette()

    raw = []  # per-model (positions, normals, colors, indices) lists
    lo = [math.inf, math.inf, math.inf]
    hi = [-math.inf, -math.inf, -math.inf]
    for _size, raw_voxels in models:
        solid = {(x, y, z): c - 1 for (x, y, z, c) in raw_voxels}
        entry = emit_faces(solid, palette, linear)
        raw.append(entry)
        for p in entry[0]:
            for i in range(3):
                lo[i] = min(lo[i], p[i])
                hi[i] = max(hi[i], p[i])

    offset = [0.0, 0.0, 0.0]
    if center:
        # Center the scaled bounding box on the origin.
        offset = [((lo[i] + hi[i]) / 2.0) * scale for i in range(3)]

    nodes = []
    for i, (pos, nrm, col, idx) in enumerate(raw):
        arr_pos = np.asarray(pos, dtype=np.float64) * scale - offset
        arr_nrm = np.asarray(nrm, dtype=np.float32)
        arr_col = np.asarray(col, dtype=np.float32)
        arr_idx = np.asarray(idx, dtype=np.uint32)
        nodes.append((f"{stem}_{i}", arr_pos.astype(np.float32),
                      arr_nrm, arr_col, arr_idx))

    if merge:
        # Concatenate every model into one mesh/node, fixing up indices.
        mpos = np.concatenate([n[1] for n in nodes], axis=0)
        mnrm = np.concatenate([n[2] for n in nodes], axis=0)
        mcol = np.concatenate([n[3] for n in nodes], axis=0)
        parts, base = [], 0
        for n in nodes:
            parts.append(n[4] + base)
            base += len(n[1])
        midx = np.concatenate(parts, axis=0).astype(np.uint32)
        nodes = [(stem, mpos, mnrm, mcol, midx)]
    return stem, nodes, len(models)


# --------------------------------------------------------------------------
# GLB writer: one buffer covering the BIN chunk; per-mesh bufferViews and
# accessors for POSITION/NORMAL/COLOR_0/indices. Every array's byte
# length is a multiple of 4 (12/16/12/4 bytes per element), so sequential
# packing keeps each bufferView offset 4-byte aligned.
# --------------------------------------------------------------------------
def build_glb(nodes):
    bin_parts = []
    buffer_views = []
    accessors = []
    meshes = []
    gltf_nodes = []
    has_transparency = any(float(n[3][:, 3].min()) < 1.0 for n in nodes)

    for name, pos, nrm, col, idx in nodes:
        blobs = [(pos.tobytes(), TARGET_ARRAY_BUFFER),
                 (nrm.tobytes(), TARGET_ARRAY_BUFFER),
                 (col.tobytes(), TARGET_ARRAY_BUFFER),
                 (idx.tobytes(), TARGET_ELEMENT_ARRAY_BUFFER)]
        view_ids = []
        for blob, target in blobs:
            view_ids.append(len(buffer_views))
            buffer_views.append({"buffer": 0,
                                 "byteOffset": sum(len(b) for b in bin_parts),
                                 "byteLength": len(blob),
                                 "target": target})
            bin_parts.append(blob)

        accessor_ids = []
        specs = [(COMP_FLOAT, "VEC3", len(pos)),
                 (COMP_FLOAT, "VEC3", len(nrm)),
                 (COMP_FLOAT, "VEC4", len(col)),
                 (COMP_UINT, "SCALAR", len(idx))]
        for attr, (component_type, acc_type, count), view_id in zip(
                ("POSITION", "NORMAL", "COLOR_0", "indices"), specs, view_ids):
            accessor = {"bufferView": view_id,
                        "componentType": component_type,
                        "count": count,
                        "type": acc_type}
            if attr == "POSITION":
                # min/max are required on POSITION accessors.
                accessor["min"] = [float(v) for v in pos.min(axis=0)]
                accessor["max"] = [float(v) for v in pos.max(axis=0)]
            accessor_ids.append(len(accessors))
            accessors.append(accessor)

        mesh_id = len(meshes)
        meshes.append({"name": name,
                       "primitives": [{"attributes":
                                       {"POSITION": accessor_ids[0],
                                        "NORMAL": accessor_ids[1],
                                        "COLOR_0": accessor_ids[2]},
                                       "indices": accessor_ids[3],
                                       "material": 0}]})
        gltf_nodes.append({"name": name, "mesh": mesh_id})

    material = {"name": "voxel",
                "pbrMetallicRoughness":
                    {"baseColorFactor": [1.0, 1.0, 1.0, 1.0],
                     "metallicFactor": 0.0,
                     "roughnessFactor": 1.0},
                "doubleSided": False,
                "alphaMode": "BLEND" if has_transparency else "OPAQUE"}

    binary = b"".join(bin_parts)
    gltf = {"asset": {"version": "2.0", "generator": GENERATOR},
            "scene": 0,
            "scenes": [{"nodes": list(range(len(gltf_nodes)))}],
            "nodes": gltf_nodes,
            "meshes": meshes,
            "materials": [material],
            "buffers": [{"byteLength": len(binary)}],
            "bufferViews": buffer_views,
            "accessors": accessors}

    json_raw = json.dumps(gltf, separators=(",", ":")).encode("ascii")
    json_pad = (4 - len(json_raw) % 4) % 4
    json_chunk = json_raw + b" " * json_pad  # spec: spaces (0x20)
    bin_pad = (4 - len(binary) % 4) % 4
    bin_chunk = binary + b"\x00" * bin_pad
    total = 12 + 8 + len(json_chunk) + 8 + len(bin_chunk)
    header = struct.pack("<III", GLTF_MAGIC, GLTF_VERSION, total)
    return (header + struct.pack("<II", len(json_chunk), JSON_CHUNK_TYPE)
            + json_chunk + struct.pack("<II", len(bin_chunk), BIN_CHUNK_TYPE)
            + bin_chunk)


# --------------------------------------------------------------------------
# Self-validation: re-read written bytes and verify the container plus
# every accessor's shape. Raises ValueError with a clear message on any
# failure; returns (meshes, nodes, position_count, triangle_count).
# --------------------------------------------------------------------------
def validate_glb(data):
    if len(data) < 12:
        raise ValueError(f"file too short for a glTF header: {len(data)} bytes")
    magic, version, total = struct.unpack("<III", data[:12])
    if magic != GLTF_MAGIC:
        raise ValueError(f"bad magic 0x{magic:08X}, expected 0x{GLTF_MAGIC:08X}")
    if version != GLTF_VERSION:
        raise ValueError(f"unsupported glTF version {version}, expected 2")
    if total != len(data):
        raise ValueError(f"declared length {total} != actual {len(data)}")

    json_len, json_type = struct.unpack("<II", data[12:20])
    if json_type != JSON_CHUNK_TYPE:
        raise ValueError(f"first chunk type 0x{json_type:08X} is not JSON")
    try:
        gltf = json.loads(data[20:20 + json_len].decode("ascii"))
    except (UnicodeDecodeError, ValueError) as e:
        raise ValueError(f"JSON chunk does not parse: {e}")

    bin_off = 20 + json_len
    if len(data) < bin_off + 8:
        raise ValueError("file ends before the BIN chunk header")
    bin_len, bin_type = struct.unpack("<II", data[bin_off:bin_off + 8])
    if bin_type != BIN_CHUNK_TYPE:
        raise ValueError(f"second chunk type 0x{bin_type:08X} is not BIN")
    if bin_off + 8 + bin_len != len(data):
        raise ValueError("BIN chunk length does not match file size")
    binary = data[bin_off + 8:]

    comp_size = {COMP_FLOAT: 4, COMP_UINT: 4}
    comp_count = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}
    for i, acc in enumerate(gltf.get("accessors", [])):
        try:
            kind = comp_count[acc["type"]]
            size = comp_size[acc["componentType"]]
        except KeyError:
            raise ValueError(f"accessor {i} has unknown type/component")
        view = gltf["bufferViews"][acc["bufferView"]]
        want = acc["count"] * kind * size
        if view["byteLength"] != want:
            raise ValueError(f"accessor {i}: count {acc['count']} x {acc['type']}"
                             f" needs {want} bytes, bufferView has {view['byteLength']}")
        if view["byteOffset"] % size != 0:
            raise ValueError(f"accessor {i}: bufferView offset misaligned")
        if view["byteOffset"] + view["byteLength"] > len(binary):
            raise ValueError(f"accessor {i}: bufferView runs past the BIN chunk")
    for i, mesh in enumerate(gltf.get("meshes", [])):
        for prim in mesh["primitives"]:
            pos_id = prim["attributes"]["POSITION"]
            pos = gltf["accessors"][pos_id]
            if "min" not in pos or "max" not in pos:
                raise ValueError(f"mesh {i}: POSITION accessor lacks min/max")

    pos_total, tri_total = 0, 0
    for mesh in gltf.get("meshes", []):
        for prim in mesh["primitives"]:
            pos_total += gltf["accessors"][prim["attributes"]["POSITION"]]["count"]
            tri_total += gltf["accessors"][prim["indices"]]["count"] // 3
    return len(gltf.get("meshes", [])), len(gltf.get("nodes", [])), pos_total, tri_total


def write_output(nodes, out_path, source_desc, n_models):
    glb = build_glb(nodes)
    parent = os.path.dirname(os.path.abspath(out_path))
    os.makedirs(parent, exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(glb)
    try:
        validate_glb(glb)
    except ValueError as e:
        print(f"error: self-validation failed for {out_path}: {e}", file=sys.stderr)
        sys.exit(1)
    verts = sum(len(n[1]) for n in nodes)
    tris = sum(len(n[4]) // 3 for n in nodes)
    print(f"{source_desc}: {n_models} model(s), {len(nodes)} node(s), "
          f"{verts} vertices, {tris} triangles -> {out_path} ({len(glb)} bytes)")


def expand_inputs(inputs):
    vox_paths = []
    for vox_path in inputs:
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
    return vox_paths


def main():
    parser = argparse.ArgumentParser(
        description="Convert MagicaVoxel .vox models into glTF 2.0 binary (.glb) files.")
    parser.add_argument("input", nargs="+",
                        help="input .vox files and/or folders (a folder expands to every "
                             ".vox file directly inside it, sorted, non-recursive).")
    parser.add_argument("--out", "-o", type=str, default=None,
                        help="output .glb path: with one input file that exact file, "
                             "with several inputs one combined .glb. Default: one "
                             "<stem>.glb beside each input.")
    parser.add_argument("--scale", type=float, default=1.0,
                        help="multiply all coordinates by this factor (default 1.0).")
    parser.add_argument("--no-center", action="store_true",
                        help="keep raw coordinates instead of recentering the model's "
                             "bounding-box center to the origin.")
    parser.add_argument("--no-linear", action="store_true",
                        help="write raw normalized sRGB colors instead of converting "
                             "them to linear for COLOR_0.")
    parser.add_argument("--merge", action="store_true",
                        help="combine every model of a file into a single mesh/node "
                             "instead of one node per model.")

    # "help" as a bare positional (no leading dashes) is a common enough
    # instinct -- argparse alone wouldn't catch it (it'd be treated as an
    # input path and fail with "file not found: help"), so special-case it.
    if sys.argv[1:] == ["help"]:
        parser.print_help()
        sys.exit(0)

    args = parser.parse_args()
    if args.scale <= 0:
        print(f"error: --scale must be positive, got {args.scale}", file=sys.stderr)
        sys.exit(1)

    vox_paths = expand_inputs(args.input)
    center = not args.no_center
    linear = not args.no_linear

    if args.out is None:
        for path in vox_paths:
            stem, nodes, n_models = file_meshes(path, args.scale, center, linear, args.merge)
            out_path = os.path.join(os.path.dirname(os.path.abspath(path)), stem + ".glb")
            write_output(nodes, out_path, path, n_models)
    elif len(vox_paths) == 1:
        stem, nodes, n_models = file_meshes(vox_paths[0], args.scale, center, linear, args.merge)
        write_output(nodes, args.out, vox_paths[0], n_models)
    else:
        combined = []
        n_models = 0
        for path in vox_paths:
            _stem, nodes, count = file_meshes(path, args.scale, center, linear, args.merge)
            combined.extend(nodes)
            n_models += count
        write_output(combined, args.out, f"combined {len(vox_paths)} files", n_models)


if __name__ == "__main__":
    main()
