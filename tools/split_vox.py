#!/usr/bin/env python3
"""
split_vox.py -- split a MagicaVoxel .vox file holding several models (e.g.
animation frames) into one .vox file per model.

Usage:
    python split_vox.py <model>.vox [<model2>.vox ...]

Output goes to a folder named after the input file, next to it:
    medic/run.vox  ->  medic/run/run1.vox, run2.vox, run3.vox, run4.vox

Numbering follows the animation frame order (each model's _f tag in the
scene graph), falling back to the models' order in the file for untagged
models; 1-based, zero-padded when there are 10 or more so an alphabetical
listing keeps frame order. That
matches convert_vox_to_sprite.py's folder input, which sorts alphabetically
and names the combined sheet after the shared prefix (run1..run4 -> run_2d.png).

Each output keeps everything from the source that is not per-model: palette
(RGBA), materials (MATL), layers (LAYR), render/camera settings (rOBJ,
rCAM), palette notes and index map (NOTE, IMAP), META, and any other global
chunk, byte for byte. The scene graph is rebuilt as a single object: the
root transform, a group, then the model's own transform (name, layer,
rotation, translation) and shape attributes carried over from the source.
Transforms of outer groups are not composed into it -- MagicaVoxel exports
of single objects and animation frames have none.
"""
import struct
import sys
from pathlib import Path

SCENE_CHUNKS = {b"nTRN", b"nGRP", b"nSHP"}


# --------------------------------------------------------------------------
# Low-level readers/writers for the .vox chunk format.
# --------------------------------------------------------------------------
class Reader:
    def __init__(self, data, pos=0):
        self.data, self.pos = data, pos

    def i32(self):
        (v,) = struct.unpack_from("<i", self.data, self.pos)
        self.pos += 4
        return v

    def string(self):
        n = self.i32()
        s = self.data[self.pos:self.pos + n].decode("utf-8", "replace")
        self.pos += n
        return s

    def dict(self):
        return {self.string(): self.string() for _ in range(self.i32())}


def w_i32(v):
    return struct.pack("<i", v)


def w_string(s):
    b = s.encode("utf-8")
    return w_i32(len(b)) + b


def w_dict(d):
    return w_i32(len(d)) + b"".join(w_string(k) + w_string(v) for k, v in d.items())


def chunk(cid, content, children=b""):
    return cid + w_i32(len(content)) + w_i32(len(children)) + content + children


# --------------------------------------------------------------------------
# Parsing
# --------------------------------------------------------------------------
def read_chunks(path):
    data = Path(path).read_bytes()
    if data[:4] != b"VOX ":
        raise ValueError(f"{path}: not a .vox file (missing 'VOX ' magic bytes)")
    (version,) = struct.unpack_from("<i", data, 4)
    if data[8:12] != b"MAIN":
        raise ValueError(f"{path}: expected a MAIN chunk, found {data[8:12]!r}")
    n, m = struct.unpack_from("<ii", data, 12)
    pos, end = 20 + n, 20 + n + m
    chunks = []  # (id, content bytes)
    while pos < end:
        cid = data[pos:pos + 4]
        n, m = struct.unpack_from("<ii", data, pos + 4)
        chunks.append((cid, data[pos + 12:pos + 12 + n]))
        pos += 12 + n + m  # none of the chunks MagicaVoxel writes inside MAIN have children
    return version, chunks


def parse_scene(chunks):
    """Returns {node_id: node} for nTRN/nGRP/nSHP chunks."""
    nodes = {}
    for cid, content in chunks:
        if cid not in SCENE_CHUNKS:
            continue
        r = Reader(content)
        node_id, attrs = r.i32(), r.dict()
        if cid == b"nTRN":
            child, _reserved, layer = r.i32(), r.i32(), r.i32()
            frames = [r.dict() for _ in range(r.i32())]
            nodes[node_id] = {"type": "trn", "attrs": attrs, "child": child, "layer": layer,
                              "frames": frames}
        elif cid == b"nGRP":
            nodes[node_id] = {"type": "grp", "attrs": attrs,
                              "children": [r.i32() for _ in range(r.i32())]}
        else:
            models = [(r.i32(), r.dict()) for _ in range(r.i32())]
            nodes[node_id] = {"type": "shp", "attrs": attrs, "models": models}
    return nodes


def model_context(nodes, model_id):
    """Finds the shape that references `model_id` and the transform directly
    above it. Returns (transform_attrs, layer, frame_attrs, shape_attrs,
    model_attrs), with empty defaults when the file has no scene graph."""
    for sid, shp in nodes.items():
        if shp["type"] != "shp":
            continue
        for mid, m_attrs in shp["models"]:
            if mid != model_id:
                continue
            trn = next((t for t in nodes.values() if t["type"] == "trn" and t["child"] == sid), None)
            if trn is None:
                return {}, 0, {}, shp["attrs"], m_attrs
            # Animated shapes tag each model with its frame (_f); take the
            # transform keyframe at or before it, else the first one.
            frame = trn["frames"][0] if trn["frames"] else {}
            f_idx = int(m_attrs.get("_f", 0))
            for fr in trn["frames"]:
                if int(fr.get("_f", 0)) <= f_idx:
                    frame = fr
            return trn["attrs"], trn["layer"], frame, shp["attrs"], m_attrs
    return {}, 0, {}, {}, {}


def model_order(nodes, count):
    """Model indices in output order. An animated shape lists its models with
    a frame number (_f), and MagicaVoxel is free to store them in any order,
    so tagged models come first in frame order; the rest follow in file
    order."""
    tagged = sorted((int(m_attrs["_f"]), mid)
                    for shp in nodes.values() if shp["type"] == "shp"
                    for mid, m_attrs in shp["models"] if "_f" in m_attrs and mid < count)
    order = list(dict.fromkeys(mid for _f, mid in tagged))
    return order + [i for i in range(count) if i not in order]


def strip_frame_key(d):
    return {k: v for k, v in d.items() if k != "_f"}


# --------------------------------------------------------------------------
# Writing
# --------------------------------------------------------------------------
def build_scene(trn_attrs, layer, frame_attrs, shp_attrs, model_attrs):
    root = w_i32(0) + w_dict({}) + w_i32(1) + w_i32(-1) + w_i32(-1) + w_i32(1) + w_dict({})
    group = w_i32(1) + w_dict({}) + w_i32(1) + w_i32(2)
    trn = (w_i32(2) + w_dict(trn_attrs) + w_i32(3) + w_i32(-1) + w_i32(layer)
           + w_i32(1) + w_dict(strip_frame_key(frame_attrs)))
    shp = w_i32(3) + w_dict(shp_attrs) + w_i32(1) + w_i32(0) + w_dict(strip_frame_key(model_attrs))
    return (chunk(b"nTRN", root) + chunk(b"nGRP", group)
            + chunk(b"nTRN", trn) + chunk(b"nSHP", shp))


def build_file(version, chunks, size_content, xyzi_content, scene_bytes):
    body = b""
    wrote_model = wrote_scene = False
    for cid, content in chunks:
        if cid in (b"SIZE", b"XYZI", b"PACK"):
            if not wrote_model:
                body += chunk(b"SIZE", size_content) + chunk(b"XYZI", xyzi_content)
                wrote_model = True
        elif cid in SCENE_CHUNKS:
            if not wrote_scene:
                body += scene_bytes
                wrote_scene = True
        else:
            body += chunk(cid, content)
    return b"VOX " + w_i32(version) + chunk(b"MAIN", b"", body)


def split(path):
    path = Path(path)
    version, chunks = read_chunks(path)
    sizes = [c for cid, c in chunks if cid == b"SIZE"]
    xyzis = [c for cid, c in chunks if cid == b"XYZI"]
    if not xyzis or len(sizes) != len(xyzis):
        raise ValueError(f"{path}: found {len(sizes)} SIZE and {len(xyzis)} XYZI chunks")
    nodes = parse_scene(chunks)

    out_dir = path.with_suffix("")
    out_dir.mkdir(exist_ok=True)
    width = len(str(len(xyzis))) if len(xyzis) >= 10 else 1
    for n, i in enumerate(model_order(nodes, len(xyzis)), start=1):
        size_c, xyzi_c = sizes[i], xyzis[i]
        scene = build_scene(*model_context(nodes, i)) if nodes else b""
        out = out_dir / f"{path.stem}{n:0{width}d}.vox"
        out.write_bytes(build_file(version, chunks, size_c, xyzi_c, scene))
        sx, sy, sz = struct.unpack_from("<iii", size_c)
        (count,) = struct.unpack_from("<i", xyzi_c)
        print(f"  {out.name}  model {i}  ({sx}x{sy}x{sz}, {count} voxels)")
    print(f"{path}: {len(xyzis)} model(s) -> {out_dir}")
    return out_dir


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help", "help"):
        print(__doc__.strip())
        sys.exit(0 if len(sys.argv) >= 2 else 1)
    for arg in sys.argv[1:]:
        p = Path(arg)
        if not p.is_file() and p.with_suffix(".vox").is_file():
            p = p.with_suffix(".vox")
        if not p.is_file():
            print(f"error: file not found: {arg}", file=sys.stderr)
            sys.exit(1)
        split(p)


if __name__ == "__main__":
    main()
