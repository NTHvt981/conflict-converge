"""Procedural voxel LightTank generator (MagicaVoxel .vox output).

Why code, not art: voxel tanks are boxes, and boxes are exactly describable.
The model is original (own silhouette/palette in the game's team colors),
so there is no Advance Wars licensing exposure. Open the .vox in
MagicaVoxel, pose/copy it per animation frame, and feed frames to the
3D-to-2D sprite tool.

Usage: python tools/gen_voxels.py [--team 0|1] [--unit NAME] [--out path]
Verifies itself by re-parsing the file and printing top/side slices.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

# --------------------------------------------------------------------------
# Minimal .vox writer (format version 150).
# File: 'VOX ' + int32 version, then MAIN chunk (id + content + children
# sizes). Inside MAIN: SIZE (x,y,z int32) + XYZI (count int32 +
# x,y,z,colorIndex bytes) + RGBA palette (256 RGBA quads; colorIndex is
# 1-based into it). Axes: x = length, y = width, z = up.
# --------------------------------------------------------------------------


def chunk(chunk_id: bytes, content: bytes, children: bytes = b"") -> bytes:
    return chunk_id + struct.pack("<ii", len(content), len(children)) + content + children


def write_vox(path: Path, size: tuple[int, int, int],
              voxels: list[tuple[int, int, int, int]],
              palette: list[tuple[int, int, int, int]]) -> None:
    assert len(palette) == 256, "palette must hold exactly 256 RGBA entries"
    size_content = struct.pack("<iii", *size)
    xyzi_content = struct.pack("<i", len(voxels))
    for x, y, z, c in voxels:
        assert 0 <= x < size[0] and 0 <= y < size[1] and 0 <= z < size[2], f"voxel out of bounds: {(x, y, z)}"
        assert 1 <= c <= 255, f"color index out of range: {c}"
        xyzi_content += struct.pack("BBBB", x, y, z, c)
    rgba_content = b"".join(struct.pack("BBBB", *entry) for entry in palette)
    main_children = chunk(b"SIZE", size_content) + chunk(b"XYZI", xyzi_content) \
        + chunk(b"RGBA", rgba_content)
    path.write_bytes(b"VOX " + struct.pack("<i", 150) + chunk(b"MAIN", b"", main_children))


def read_vox_voxels(path: Path) -> tuple[tuple[int, int, int], list[tuple[int, int, int, int]]]:
    """Re-parse just SIZE + XYZI (enough to verify our own output)."""
    data = path.read_bytes()
    assert data[:4] == b"VOX ", "bad magic"
    assert struct.unpack("<i", data[4:8])[0] == 150, "bad version"
    assert data[8:12] == b"MAIN", "missing MAIN chunk"
    pos = 20  # MAIN header is id + content size + children size
    size = (0, 0, 0)
    voxels: list[tuple[int, int, int, int]] = []
    while pos < len(data):
        cid, content_len, children_len = data[pos:pos + 4], *struct.unpack("<ii", data[pos + 4:pos + 12])
        body = data[pos + 12:pos + 12 + content_len]
        if cid == b"SIZE":
            size = struct.unpack("<iii", body)
        elif cid == b"XYZI":
            (count,) = struct.unpack("<i", body[:4])
            for i in range(count):
                x, y, z, c = body[4 + 4 * i:8 + 4 * i]
                voxels.append((x, y, z, c))
        pos += 12 + content_len + children_len
    return size, voxels


# --------------------------------------------------------------------------
# LightTank model. Footprint 21 x 11, height 8. Factions differ only in trim.
# --------------------------------------------------------------------------

# Palette slots (1-based): body, dark, trim, metal, light, skin.
BODY = 1
DARK = 2
TRIM = 3
METAL = 4
LIGHT = 5
SKIN = 6

TEAM_BODY = {
    0: (58, 130, 200, 255),    # ally blue
    1: (200, 70, 60, 255),     # enemy red
}
DARK_C = (28, 30, 38, 255)
TRIM_C = (230, 230, 235, 255)
METAL_C = (90, 95, 105, 255)
LIGHT_C = (255, 215, 130, 255)
SKIN_C = (218, 178, 140, 255)
EMPTY_C = (0, 0, 0, 0)


def build_tank(team: int) -> tuple[tuple[int, int, int], list[tuple[int, int, int, int]]]:
    sx, sy, sz = 21, 11, 8
    vox: dict[tuple[int, int, int], int] = {}

    def put(x0: int, x1: int, y0: int, y1: int, z0: int, z1: int, color: int) -> None:
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                for z in range(z0, z1 + 1):
                    vox[(x, y, z)] = color

    # --- Tracks: full-length dark skids, individual road wheels outside. ---
    put(0, 20, 0, 2, 0, 2, DARK)
    put(0, 20, 8, 10, 0, 2, DARK)
    for x in (1, 4, 7, 10, 13, 16, 19):
        put(x, x, 0, 0, 0, 1, TRIM)   # road wheels, near side
        put(x, x, 10, 10, 0, 1, TRIM)  # road wheels, far side
    put(0, 0, 0, 2, 0, 2, METAL)   # idler wheel, front
    put(0, 0, 8, 10, 0, 2, METAL)
    put(20, 20, 0, 2, 0, 2, METAL)  # drive sprocket, rear
    put(20, 20, 8, 10, 0, 2, METAL)
    # --- Fenders + side skirts over the tracks. ---
    put(0, 20, 0, 2, 3, 3, BODY)
    put(0, 20, 8, 10, 3, 3, BODY)
    put(3, 17, 2, 2, 1, 3, BODY)   # skirt, near side
    put(3, 17, 8, 8, 1, 3, BODY)   # skirt, far side
    put(3, 17, 2, 2, 1, 1, TRIM)   # skirt lower edge
    put(3, 17, 8, 8, 1, 1, TRIM)
    put(0, 0, 0, 2, 3, 3, DARK)    # mudflaps, front
    put(0, 0, 8, 10, 3, 3, DARK)
    # --- Hull: nose block, main body, rear deck. ---
    put(0, 3, 3, 7, 1, 3, BODY)    # nose + glacis step
    put(2, 18, 3, 7, 2, 4, BODY)   # main hull
    put(2, 18, 3, 3, 3, 3, TRIM)   # side stripe, near
    put(2, 18, 7, 7, 3, 3, TRIM)   # side stripe, far
    put(0, 0, 3, 3, 2, 2, LIGHT)   # headlight, near
    put(0, 0, 7, 7, 2, 2, LIGHT)   # headlight, far
    put(19, 20, 3, 7, 2, 2, DARK)  # rear plate
    put(19, 19, 4, 6, 3, 3, METAL)  # exhaust pipe
    put(17, 18, 3, 4, 4, 4, DARK)  # stowage boxes, rear deck
    put(17, 18, 6, 7, 4, 4, DARK)
    put(4, 4, 4, 4, 4, 4, METAL)   # deck hatches
    put(4, 4, 6, 6, 4, 4, METAL)
    # --- Turret: ring, body, roof details, mantlet, barrel, extras. ---
    put(8, 14, 3, 7, 5, 5, BODY)   # turret ring
    put(8, 13, 3, 7, 5, 6, BODY)   # turret body
    put(9, 12, 4, 6, 6, 6, TRIM)   # roof inlay
    put(13, 14, 4, 6, 5, 5, DARK)  # mantlet
    put(14, 18, 5, 5, 5, 5, METAL)  # barrel
    put(18, 20, 4, 6, 4, 6, DARK)  # muzzle brake
    put(20, 20, 5, 5, 5, 5, METAL)  # muzzle tip
    put(10, 10, 5, 5, 6, 6, DARK)  # commander hatch ring
    put(9, 9, 5, 5, 6, 7, METAL)   # machine gun post
    put(9, 9, 5, 6, 7, 7, METAL)   # machine gun receiver
    put(8, 8, 3, 3, 6, 7, METAL)   # antenna

    voxels = [(x, y, z, c) for (x, y, z), c in sorted(vox.items())]
    return (sx, sy, sz), voxels


# --------------------------------------------------------------------------
# Remaining vehicles, same builder pattern as the LightTank.
# ifv:        17 x 9 x 7 fast scout — low hull, small turret, short autocannon.
# artillery:  23 x 9 x 7 SPG — long gun, gun shield, rear trail spades.
# heavytank:  23 x 13 x 9 breakthrough — wide twin tracks, twin barrels.
# --------------------------------------------------------------------------

def build_vehicle(kind: str) -> tuple[tuple[int, int, int], list[tuple[int, int, int, int]]]:
    assert kind in ("ifv", "artillery", "heavytank"), kind
    vox: dict[tuple[int, int, int], int] = {}

    def put(x0: int, x1: int, y0: int, y1: int, z0: int, z1: int, color: int) -> None:
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                for z in range(z0, z1 + 1):
                    vox[(x, y, z)] = color

    if kind == "ifv":
        sx, sy, sz = 17, 9, 7
        put(0, 16, 0, 1, 0, 1, DARK)
        put(0, 16, 7, 8, 0, 1, DARK)
        for x in (1, 4, 7, 10, 13, 15):
            put(x, x, 0, 0, 0, 1, TRIM)
            put(x, x, 8, 8, 0, 1, TRIM)
        put(0, 0, 0, 1, 0, 1, METAL)
        put(0, 0, 7, 8, 0, 1, METAL)
        put(16, 16, 0, 1, 0, 1, METAL)
        put(16, 16, 7, 8, 0, 1, METAL)
        put(0, 16, 0, 1, 2, 2, BODY)
        put(0, 16, 7, 8, 2, 2, BODY)
        put(0, 1, 2, 6, 1, 2, BODY)    # nose
        put(2, 14, 2, 6, 1, 3, BODY)  # low hull
        put(2, 14, 2, 2, 2, 2, TRIM)  # stripes
        put(2, 14, 6, 6, 2, 2, TRIM)
        put(0, 0, 2, 2, 2, 2, LIGHT)  # headlights
        put(0, 0, 6, 6, 2, 2, LIGHT)
        put(15, 16, 2, 6, 1, 2, DARK)  # rear plate
        put(15, 15, 3, 5, 2, 2, METAL)  # exhaust
        put(8, 11, 3, 5, 4, 4, BODY)  # turret ring
        put(8, 10, 3, 5, 4, 5, BODY)  # turret body
        put(8, 10, 3, 5, 5, 5, TRIM)  # roof
        put(11, 15, 4, 4, 4, 5, METAL)  # autocannon (short, thin)
        put(15, 15, 4, 4, 4, 5, DARK)  # muzzle
        put(8, 8, 3, 3, 5, 6, METAL)  # MG post
        put(11, 11, 5, 5, 5, 6, METAL)  # antenna
        put(9, 9, 4, 4, 5, 5, DARK)   # hatch
    elif kind == "artillery":
        sx, sy, sz = 23, 9, 7
        put(0, 22, 0, 1, 0, 1, DARK)
        put(0, 22, 7, 8, 0, 1, DARK)
        for x in (1, 4, 7, 10, 13, 16, 19, 21):
            put(x, x, 0, 0, 0, 1, TRIM)
            put(x, x, 8, 8, 0, 1, TRIM)
        put(0, 0, 0, 1, 0, 1, METAL)
        put(0, 0, 7, 8, 0, 1, METAL)
        put(22, 22, 0, 1, 0, 1, METAL)
        put(22, 22, 7, 8, 0, 1, METAL)
        put(0, 22, 0, 1, 2, 2, BODY)
        put(0, 22, 7, 8, 2, 2, BODY)
        put(0, 1, 2, 6, 1, 1, BODY)    # nose
        put(2, 20, 2, 6, 1, 2, BODY)  # hull
        put(2, 20, 2, 2, 2, 2, TRIM)  # stripes
        put(2, 20, 6, 6, 2, 2, TRIM)
        put(0, 0, 2, 2, 1, 1, LIGHT)  # headlights
        put(0, 0, 6, 6, 1, 1, LIGHT)
        put(6, 7, 3, 5, 2, 2, TRIM)   # ammo boxes on deck
        put(10, 11, 2, 6, 2, 4, DARK)  # gun shield
        put(11, 12, 3, 5, 2, 4, BODY)  # trunnion (raised to meet the gun)
        put(12, 21, 4, 4, 3, 4, METAL)  # long barrel, 2 tall for presence
        put(20, 22, 3, 5, 2, 4, DARK)  # muzzle brake
        put(22, 22, 4, 4, 3, 4, METAL)  # muzzle tip
        put(20, 22, 0, 0, 0, 1, DARK)  # trail legs
        put(20, 22, 8, 8, 0, 1, DARK)
        put(22, 22, 0, 0, 0, 0, METAL)  # spade blades
        put(22, 22, 8, 8, 0, 0, METAL)
        put(19, 19, 5, 5, 2, 2, METAL)  # exhaust
    else:  # heavytank
        sx, sy, sz = 23, 13, 9
        put(0, 22, 0, 3, 0, 2, DARK)   # twin wide tracks
        put(0, 22, 9, 12, 0, 2, DARK)
        for x in (1, 4, 7, 10, 13, 16, 19, 21):
            put(x, x, 0, 0, 0, 1, TRIM)
            put(x, x, 12, 12, 0, 1, TRIM)
        put(0, 0, 0, 3, 0, 2, METAL)   # idlers
        put(0, 0, 9, 12, 0, 2, METAL)
        put(22, 22, 0, 3, 0, 2, METAL)  # sprockets
        put(22, 22, 9, 12, 0, 2, METAL)
        put(0, 22, 0, 3, 3, 3, BODY)   # fenders
        put(0, 22, 9, 12, 3, 3, BODY)
        put(3, 19, 3, 3, 1, 4, BODY)   # skirts
        put(3, 19, 9, 9, 1, 4, BODY)
        put(3, 19, 3, 3, 1, 1, TRIM)   # skirt edges
        put(3, 19, 9, 9, 1, 1, TRIM)
        put(0, 0, 0, 3, 3, 3, DARK)    # mudflaps
        put(0, 0, 9, 12, 3, 3, DARK)
        put(0, 3, 4, 8, 1, 3, BODY)    # glacis nose
        put(2, 20, 4, 8, 2, 4, BODY)   # main hull
        put(2, 20, 4, 4, 3, 3, TRIM)   # stripes
        put(2, 20, 8, 8, 3, 3, TRIM)
        put(0, 0, 4, 4, 2, 2, LIGHT)   # headlights
        put(0, 0, 8, 8, 2, 2, LIGHT)
        put(21, 22, 4, 8, 2, 3, DARK)  # rear plate
        put(21, 21, 5, 5, 2, 3, METAL)  # exhausts
        put(21, 21, 7, 7, 2, 3, METAL)
        put(18, 19, 4, 5, 4, 4, DARK)  # stowage
        put(18, 19, 7, 8, 4, 4, DARK)
        put(9, 15, 4, 8, 5, 5, BODY)   # turret ring
        put(9, 14, 4, 8, 5, 6, BODY)   # turret body
        put(10, 13, 5, 7, 6, 6, TRIM)  # roof inlay
        put(15, 16, 4, 8, 5, 6, DARK)  # mantlets
        put(17, 21, 5, 5, 6, 6, METAL)  # twin barrels
        put(17, 21, 7, 7, 6, 6, METAL)
        put(20, 21, 4, 6, 5, 7, DARK)  # muzzle brakes
        put(21, 21, 5, 5, 6, 6, METAL)  # muzzle tips
        put(21, 21, 7, 7, 6, 6, METAL)
        put(11, 11, 6, 6, 6, 6, DARK)  # hatch
        put(10, 10, 6, 6, 6, 7, METAL)  # MG post
        put(10, 10, 6, 7, 7, 7, METAL)  # MG receiver
        put(14, 14, 4, 4, 6, 8, METAL)  # antenna

    voxels = [(x, y, z, c) for (x, y, z), c in sorted(vox.items())]
    assert max(x for x, _, _, _ in voxels) < sx, f"{kind} x overflow"
    assert max(y for _, y, _, _ in voxels) < sy, f"{kind} y overflow"
    assert max(z for _, _, z, _ in voxels) < sz, f"{kind} z overflow"
    return (sx, sy, sz), voxels


# --------------------------------------------------------------------------
# Buildings. Team color reads from trim/roof accents (structures are mostly
# concrete + metal so both factions share the same gray masses).
# base:    25 x 25 x 10 HQ — slab, wings, tower, antenna, corner lamps.
# depot:   13 x 13 x 9  resource depot — silo, crates, office, beacon.
# factory: 25 x 21 x 12 production — hall, skylights, big door, chimney,
#          vents, storage tanks, beacon.
# --------------------------------------------------------------------------

def build_building(kind: str) -> tuple[tuple[int, int, int], list[tuple[int, int, int, int]]]:
    assert kind in ("base", "depot", "factory"), kind
    vox: dict[tuple[int, int, int], int] = {}

    def put(x0: int, x1: int, y0: int, y1: int, z0: int, z1: int, color: int) -> None:
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                for z in range(z0, z1 + 1):
                    vox[(x, y, z)] = color

    if kind == "base":
        sx, sy, sz = 25, 25, 10
        put(0, 24, 0, 24, 0, 0, DARK)    # foundation slab
        put(0, 24, 0, 0, 0, 0, TRIM)    # slab border
        put(0, 24, 24, 24, 0, 0, TRIM)
        put(0, 0, 0, 24, 0, 0, TRIM)
        put(24, 24, 0, 24, 0, 0, TRIM)
        put(1, 23, 1, 23, 1, 1, METAL)  # parade concrete
        put(8, 16, 8, 16, 1, 5, BODY)   # HQ block
        put(8, 16, 8, 8, 3, 4, LIGHT)   # windows, front
        put(8, 8, 8, 16, 3, 4, LIGHT)   # windows, near side
        put(16, 16, 8, 16, 3, 4, LIGHT)  # windows, far side
        put(8, 16, 8, 8, 5, 5, TRIM)    # banner stripe
        put(11, 13, 16, 16, 1, 3, DARK)  # entrance
        put(7, 17, 7, 17, 6, 6, DARK)   # HQ roof slab
        put(11, 13, 11, 13, 6, 8, BODY)  # tower
        put(11, 13, 11, 11, 7, 7, LIGHT)  # tower windows
        put(12, 12, 12, 12, 7, 8, METAL)  # antenna mast
        put(12, 12, 12, 12, 9, 9, LIGHT)  # beacon
        put(3, 7, 10, 14, 1, 3, BODY)   # west wing
        put(17, 21, 10, 14, 1, 3, BODY)  # east wing
        put(3, 7, 10, 14, 4, 4, DARK)   # wing roofs
        put(17, 21, 10, 14, 4, 4, DARK)
        put(3, 7, 10, 10, 2, 3, LIGHT)  # wing windows
        put(17, 21, 10, 10, 2, 3, LIGHT)
        for cx, cy in ((1, 1), (23, 1), (1, 23), (23, 23)):
            put(cx, cx, cy, cy, 1, 3, METAL)  # corner posts
            put(cx, cx, cy, cy, 4, 4, LIGHT)  # lamps
    elif kind == "depot":
        sx, sy, sz = 13, 13, 9
        put(0, 12, 0, 12, 0, 0, DARK)    # pad
        put(0, 12, 0, 0, 0, 0, TRIM)
        put(0, 12, 12, 12, 0, 0, TRIM)
        put(0, 0, 0, 12, 0, 0, TRIM)
        put(12, 12, 0, 12, 0, 0, TRIM)
        put(4, 8, 4, 8, 1, 6, METAL)    # silo body
        put(4, 8, 4, 4, 2, 2, TRIM)     # silo rings
        put(4, 8, 8, 8, 2, 2, TRIM)
        put(4, 8, 4, 4, 4, 4, TRIM)
        put(4, 8, 8, 8, 4, 4, TRIM)
        put(4, 8, 4, 4, 6, 6, TRIM)
        put(4, 8, 8, 8, 6, 6, TRIM)
        put(4, 8, 4, 8, 7, 7, DARK)     # silo cap
        put(6, 6, 6, 6, 8, 8, LIGHT)    # beacon
        put(8, 12, 6, 6, 1, 1, METAL)   # feed pipe
        put(9, 11, 9, 11, 1, 2, TRIM)   # crates
        put(10, 12, 2, 4, 1, 1, DARK)
        put(1, 2, 9, 11, 1, 3, DARK)
        put(1, 3, 1, 3, 1, 3, BODY)     # office
        put(1, 3, 1, 1, 2, 2, LIGHT)    # office window
        put(2, 2, 3, 3, 1, 2, DARK)     # office door
        put(1, 3, 1, 3, 4, 4, DARK)     # office roof
    else:  # factory
        sx, sy, sz = 25, 21, 12
        put(0, 24, 0, 20, 0, 0, DARK)   # slab
        put(0, 24, 0, 0, 0, 0, TRIM)
        put(0, 24, 20, 20, 0, 0, TRIM)
        put(0, 0, 0, 20, 0, 0, TRIM)
        put(24, 24, 0, 20, 0, 0, TRIM)
        put(3, 21, 4, 16, 1, 6, BODY)   # main hall
        put(3, 21, 4, 16, 7, 7, DARK)   # roof slab
        put(5, 19, 9, 11, 7, 7, LIGHT)  # skylight band
        put(3, 3, 8, 12, 1, 4, DARK)    # big door opening
        put(3, 3, 8, 12, 5, 5, TRIM)    # door lintel
        put(4, 20, 4, 4, 3, 4, LIGHT)   # side windows
        put(4, 20, 16, 16, 3, 4, LIGHT)
        put(3, 21, 4, 4, 5, 5, TRIM)    # team stripes
        put(3, 21, 16, 16, 5, 5, TRIM)
        put(19, 21, 5, 7, 1, 10, DARK)  # chimney stack
        put(19, 21, 5, 7, 4, 4, TRIM)   # chimney bands
        put(19, 21, 5, 7, 7, 7, TRIM)
        put(19, 21, 5, 7, 10, 10, METAL)  # chimney cap
        put(20, 20, 6, 6, 11, 11, LIGHT)  # beacon
        put(6, 7, 6, 8, 8, 8, METAL)    # roof vents
        put(12, 13, 12, 14, 8, 8, METAL)
        put(1, 4, 1, 3, 1, 3, BODY)     # annex office
        put(1, 4, 1, 1, 2, 2, LIGHT)
        put(1, 4, 1, 3, 4, 4, DARK)
        put(22, 23, 14, 15, 1, 3, METAL)  # storage tanks
        put(22, 23, 17, 18, 1, 3, METAL)
        put(22, 23, 14, 15, 4, 4, DARK)
        put(22, 23, 17, 18, 4, 4, DARK)
        put(21, 21, 14, 18, 1, 1, METAL)  # tank feed pipe

    voxels = [(x, y, z, c) for (x, y, z), c in sorted(vox.items())]
    assert max(x for x, _, _, _ in voxels) < sx, f"{kind} x overflow"
    assert max(y for _, y, _, _ in voxels) < sy, f"{kind} y overflow"
    assert max(z for _, _, z, _ in voxels) < sz, f"{kind} z overflow"
    return (sx, sy, sz), voxels


# --------------------------------------------------------------------------
# Foot set: one parametric soldier (7 x 7 x 11, facing +x), gear per role.
# kind: "infantry" (rifle), "antiarmor" (shoulder launcher), "engineer"
# (backpack + beacon, no rifle).
# --------------------------------------------------------------------------

def build_soldier(kind: str) -> tuple[tuple[int, int, int], list[tuple[int, int, int, int]]]:
    assert kind in ("infantry", "antiarmor", "engineer"), kind
    sx, sy, sz = 7, 7, 11
    vox: dict[tuple[int, int, int], int] = {}

    def put(x0: int, x1: int, y0: int, y1: int, z0: int, z1: int, color: int) -> None:
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                for z in range(z0, z1 + 1):
                    vox[(x, y, z)] = color

    # Boots + legs (dark fatigues).
    put(2, 4, 2, 2, 0, 0, METAL)  # boot toes, forward
    put(2, 4, 4, 4, 0, 0, METAL)
    put(2, 3, 2, 2, 0, 3, DARK)
    put(2, 3, 4, 4, 0, 3, DARK)
    # Torso: team-color uniform, trim belt + chest plate.
    put(2, 4, 2, 4, 4, 7, BODY)
    put(2, 4, 2, 4, 4, 4, TRIM)   # belt
    put(4, 4, 2, 4, 5, 6, TRIM)   # chest plate, front
    # Arms at the sides.
    put(2, 3, 1, 1, 4, 6, BODY)
    put(2, 3, 5, 5, 4, 6, BODY)
    put(2, 2, 1, 1, 4, 4, SKIN)   # hands
    put(2, 2, 5, 5, 4, 4, SKIN)
    # Head: skin block + team helmet with trim band.
    put(2, 3, 2, 4, 8, 9, SKIN)
    put(2, 4, 2, 4, 10, 10, BODY)  # helmet crown
    put(2, 4, 2, 4, 9, 9, BODY)    # helmet band (sides read as rim)
    put(2, 4, 2, 2, 10, 10, TRIM)  # helmet stripe, near side
    put(2, 4, 4, 4, 10, 10, TRIM)  # helmet stripe, far side

    if kind == "infantry":
        # Service rifle held forward at chest height.
        put(4, 6, 3, 3, 6, 6, METAL)  # barrel + body
        put(3, 3, 3, 3, 5, 6, DARK)   # stock/grip
        put(4, 4, 2, 2, 6, 6, SKIN)   # forward hand on rifle
    elif kind == "antiarmor":
        # Shoulder launcher: fat tube over the far shoulder, warhead tip.
        put(1, 5, 4, 4, 8, 8, METAL)
        put(5, 6, 4, 4, 8, 8, TRIM)   # warhead
        put(0, 0, 4, 4, 8, 8, DARK)   # backblast bell
        put(2, 2, 4, 4, 7, 7, SKIN)   # supporting hand under tube
    else:  # engineer
        # Field backpack + beacon lamp (no rifle: hands free for repair).
        put(1, 1, 2, 4, 5, 7, DARK)
        put(1, 1, 2, 4, 7, 7, TRIM)   # pack lid
        put(1, 1, 4, 4, 8, 8, LIGHT)  # beacon
        put(4, 4, 1, 1, 4, 5, METAL)  # wrench at the hip

    voxels = [(x, y, z, c) for (x, y, z), c in sorted(vox.items())]
    return (sx, sy, sz), voxels


def make_palette(team: int) -> list[tuple[int, int, int, int]]:
    pal = [EMPTY_C] * 256
    pal[0] = TEAM_BODY[team]  # index 1
    pal[1] = DARK_C           # index 2
    pal[2] = TRIM_C           # index 3
    pal[3] = METAL_C          # index 4
    pal[4] = LIGHT_C          # index 5
    pal[5] = SKIN_C           # index 6
    return pal


def preview(size: tuple[int, int, int], voxels: list[tuple[int, int, int, int]]) -> str:
    filled = {(x, y, z) for x, y, z, _ in voxels}
    sx, sy, sz = size
    glyph = {BODY: "#", DARK: "=", TRIM: "+", METAL: "-", LIGHT: "*", SKIN: "o"}
    color_of = {(x, y, z): c for x, y, z, c in voxels}
    lines = ["top view (x right, y down):"]
    for y in range(sy):
        lines.append("".join(glyph[color_of[(x, y, max(z for (xx, yy, z) in filled if xx == x and yy == y))]]
                             if any((xx, yy) == (x, y) for (xx, yy, _) in filled) else "."
                             for x in range(sx)))
    lines.append("side view (x right, z up):")
    mid = sy // 2
    for z in reversed(range(sz)):
        lines.append("".join(glyph[color_of[(x, mid, z)]] if (x, mid, z) in filled else "."
                             for x in range(sx)))
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Generate voxel unit .vox files")
    parser.add_argument("--team", type=int, choices=(0, 1), default=0)
    parser.add_argument("--unit", type=str,
                        choices=("lighttank", "ifv", "artillery", "heavytank",
                                 "infantry", "antiarmor", "engineer",
                                 "base", "depot", "factory"),
                        default="lighttank")
    parser.add_argument("--out", type=str, default="")
    args = parser.parse_args(argv)
    out = Path(args.out) if args.out else Path(f"data/voxels/{args.unit}_t{args.team}.vox")
    out.parent.mkdir(parents=True, exist_ok=True)

    if args.unit == "lighttank":
        size, voxels = build_tank(args.team)
    elif args.unit in ("infantry", "antiarmor", "engineer"):
        size, voxels = build_soldier(args.unit)
    elif args.unit in ("base", "depot", "factory"):
        size, voxels = build_building(args.unit)
    else:
        size, voxels = build_vehicle(args.unit)
    write_vox(out, size, voxels, make_palette(args.team))

    # Self-verify: re-parse and compare.
    size2, voxels2 = read_vox_voxels(out)
    assert tuple(size) == tuple(size2), f"size mismatch: {size} vs {size2}"
    assert sorted(map(tuple, voxels)) == sorted(map(tuple, voxels2)), "voxel mismatch"
    print(f"WROTE: {out} ({len(voxels)} voxels, {size[0]}x{size[1]}x{size[2]})")
    print(preview(size, voxels))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
