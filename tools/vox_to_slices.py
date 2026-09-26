"""Convert a MagicaVoxel .vox model into top-down 2D slices for sprite stacking.

Usage: python vox_to_slices.py chr_knight.vox [out_dir]

Writes one PNG per Z layer (slice_00.png = bottom) plus a horizontal sheet
(<name>_sheet.png) with every slice side by side, bottom layer first.
"""
import struct
import sys
from pathlib import Path

from PIL import Image


def default_palette():
    # MagicaVoxel's built-in palette, used when the file has no RGBA chunk.
    pal = []
    for r in (0xFF, 0xCC, 0x99, 0x66, 0x33, 0x00):
        for g in (0xFF, 0xCC, 0x99, 0x66, 0x33, 0x00):
            for b in (0xFF, 0xCC, 0x99, 0x66, 0x33, 0x00):
                pal.append((r, g, b, 255))
    pal = pal[:-1]  # drop the final black; index 0 is empty
    for base in ((1, 0, 0), (0, 1, 0), (0, 0, 1), (1, 1, 1)):
        for v in (0xEE, 0xDD, 0xBB, 0xAA, 0x88, 0x77, 0x55, 0x44, 0x22, 0x11):
            pal.append(tuple(c * v for c in base) + (255,))
    pal = pal[:255]
    return [(0, 0, 0, 0)] + pal


def read_vox(path):
    data = Path(path).read_bytes()
    if data[:4] != b"VOX ":
        raise ValueError(f"{path} is not a MagicaVoxel file")

    size = None
    voxels = []
    palette = None
    pos = 8
    end = len(data)
    while pos < end:
        cid = data[pos:pos + 4]
        content, children = struct.unpack_from("<ii", data, pos + 4)
        body = pos + 12
        if cid == b"MAIN":
            pos = body + content  # step into the children
            continue
        if cid == b"SIZE" and size is None:
            size = struct.unpack_from("<iii", data, body)
        elif cid == b"XYZI" and not voxels:
            (count,) = struct.unpack_from("<i", data, body)
            voxels = [tuple(data[body + 4 + i * 4: body + 8 + i * 4]) for i in range(count)]
        elif cid == b"RGBA":
            raw = [tuple(data[body + i * 4: body + i * 4 + 4]) for i in range(256)]
            # RGBA chunk entry i maps to color index i + 1.
            palette = [(0, 0, 0, 0)] + raw[:255]
        pos = body + content + children

    if size is None:
        raise ValueError("no SIZE chunk found")
    return size, voxels, palette or default_palette()


def export_slices(vox_path, out_dir=None):
    vox_path = Path(vox_path)
    out_dir = Path(out_dir) if out_dir else vox_path.with_name(vox_path.stem + "_slices")
    out_dir.mkdir(parents=True, exist_ok=True)

    (sx, sy, sz), voxels, palette = read_vox(vox_path)
    slices = [Image.new("RGBA", (sx, sy), (0, 0, 0, 0)) for _ in range(sz)]
    for x, y, z, c in voxels:
        # Z is up in MagicaVoxel; the model faces -Y, so flip Y to put the front
        # at the bottom of each top-down slice.
        slices[z].putpixel((x, sy - 1 - y), palette[c])

    sheet = Image.new("RGBA", (sx * sz, sy), (0, 0, 0, 0))
    for z, img in enumerate(slices):
        img.save(out_dir / f"slice_{z:02d}.png")
        sheet.paste(img, (z * sx, 0))
    sheet_path = out_dir / f"{vox_path.stem}_sheet.png"
    sheet.save(sheet_path)

    print(f"{vox_path.name}: {sx}x{sy}x{sz}, {len(voxels)} voxels -> {sz} slices in {out_dir}")
    return sheet_path, (sx, sy, sz)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    export_slices(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
