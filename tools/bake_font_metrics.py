#!/usr/bin/env python3
"""Bake raylib's default-font width table into data/font_metrics.txt.

The default font's metrics are fully device-independent: rtext.c's
LoadFontDefault() builds glyph rects straight from the static
charsWidth[224] table (first char 32, charsHeight 10 -> baseSize 10,
advanceX 0 for every glyph so raygui always falls back to rec.width).
No GPU is involved anywhere in measuring -- only in uploading the atlas
texture for drawing. So "baking" here is pure textual extraction, not a
GPU run: this script regexes the table out of deps/raylib/src/rtext.c,
emits it plus self-check goldens, and the headless suite replicates
raygui's GetLineWidth over it bit-for-bit.

Re-run after any raylib update and confirm `git diff` on the data file
is empty (or review it if not -- a changed table means in-engine text
widths moved and layout goldens must be re-examined).

Golden safety: the C++ replication sums in float32 while a naive Python
reference sums in float64; the two can only disagree across an integer
truncation boundary. Every probe is therefore computed BOTH ways -- with
bit-exact float32 emulation (round-after-every-op, same order as
GetLineWidth) and in float64 -- and emitted only if both agree. A
disagreeing probe fails the bake loudly instead of flaking the suite.
"""

import math
import os
import re
import struct
import sys

RTEXT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     "..", "deps", "raylib", "src", "rtext.c")
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "..", "data", "font_metrics.txt")

TEXT_SPACING = 1  # raygui DEFAULT/TEXT_SPACING (raygui.h GuiLoadStyleDefault)

# Real UI strings, at every UI-scale step the game offers (0.75x..2x).
PROBES = [
    "1+ 2- 3+  (F6-8 save, S+F6-8 load)",
    "Squad attack-move",
    "Move at slowest speed",
    "CONFLICT CONVERGE",
    "Press a key for the armed action (Esc cancels)",
]
SIZES = [7, 10, 15, 20]


def main():
    with open(RTEXT, encoding="utf-8") as f:
        src = f.read()

    m = re.search(r"int charsWidth\[(\d+)\] = \{([^}]*)\}", src)
    assert m, "charsWidth table not found in rtext.c"
    count = int(m.group(1))
    widths = [int(x) for x in re.findall(r"\d+", m.group(2))]
    assert len(widths) == count == 224, f"expected 224 widths, got {len(widths)}"

    m = re.search(r"int charsHeight = (\d+);", src)
    assert m, "charsHeight not found in rtext.c"
    base_size = int(m.group(1))

    m = re.search(r"glyphs\[i\]\.value = (\d+) \+ i;", src)
    assert m, "first-char expression not found in rtext.c"
    first_char = int(m.group(1))

    assert "advanceX = 0" in src, "default font grew advances: replication invalid"

    def f32(x):
        # Bit-exact emulation of MSVC's default FP model (IEEE-754
        # round-nearest, no mul/add contraction): round after EVERY op,
        # in the same order raygui's GetLineWidth performs them.
        return struct.unpack("f", struct.pack("f", x))[0]

    def width(ops_f32, text, size):
        # Mirror GetLineWidth: scaleFactor = fontSize/baseSize, then per
        # glyph x += glyphWidth + TEXT_SPACING, truncated to int.
        rnd = f32 if ops_f32 else (lambda v: v)
        scale = rnd(size / base_size)
        x = 0.0
        for c in text:
            glyph = rnd(widths[ord(c) - first_char] * scale)
            x = rnd(x + rnd(glyph + TEXT_SPACING))
        return int(x)

    goldens = []
    for size in SIZES:
        for probe in PROBES:
            exact = width(True, probe, size)
            reference = width(False, probe, size)
            assert exact == reference, (
                f"probe unstable across precisions (size={size} {probe!r} "
                f"f32={exact} f64={reference}): refusing to emit flaky golden")
            goldens.append((size, probe, exact))

    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("# Baked default-font metrics -- DO NOT EDIT. Regenerate with\n")
        f.write("# tools/bake_font_metrics.py (parses deps/raylib/src/rtext.c).\n")
        f.write(f"baseSize={base_size}\n")
        f.write(f"firstChar={first_char}\n")
        f.write(f"count={count}\n")
        f.write(f"textSpacing={TEXT_SPACING}\n")
        f.write("advances=" + ",".join(str(w) for w in widths) + "\n")
        for size, probe, w in goldens:
            f.write(f"golden|{size}|{probe}|{w}\n")
    print(f"baked {count} advances + {len(goldens)} goldens -> {OUT}")


if __name__ == "__main__":
    sys.exit(main())
