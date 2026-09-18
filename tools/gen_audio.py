#!/usr/bin/env python3
"""Generate placeholder audio assets for Conflict Converge.

rfxgen (the referenced tool) is an interactive GUI app with no scriptable
interface, so this script synthesizes the equivalent rfxgen-style SFX
(square/sine/noise blips with pitch slides and exponential decay) plus a
looped music track, using only the Python standard library. Re-run to
regenerate: `python tools/gen_audio.py`. Output: 16-bit mono WAVs in
`data/audio/` (22050 Hz; music 8 s loop).
"""

import math
import struct
import wave
from pathlib import Path

RATE = 22050
OUT = Path(__file__).parent.parent / "data" / "audio"


def envelope(n, decay, fade_in=0.005):
    out = []
    for i in range(n):
        t = i / RATE
        attack = min(1.0, t / fade_in) if fade_in > 0 else 1.0
        out.append(attack * math.exp(-t * decay))
    return out


def tone(freq0, freq1, dur, kind="sine", decay=12.0, mag=0.5):
    n = int(dur * RATE)
    env = envelope(n, decay)
    phase = 0.0
    frames = []
    for i in range(n):
        f = freq0 + (freq1 - freq0) * (i / max(n - 1, 1))
        phase += 2.0 * math.pi * f / RATE
        if kind == "square":
            v = 1.0 if math.sin(phase) >= 0 else -1.0
            v *= 0.6  # tame harshness
        elif kind == "saw":
            v = 2.0 * ((phase / (2.0 * math.pi)) % 1.0) - 1.0
        elif kind == "triangle":
            v = 2.0 * abs(2.0 * ((phase / (2.0 * math.pi)) % 1.0) - 1.0) - 1.0
        else:
            v = math.sin(phase)
        frames.append(int(max(-1.0, min(1.0, v * mag)) * env[i] * 32767))
    return frames


def noise(dur, decay=8.0, mag=0.5):
    import random

    random.seed(1234)  # deterministic builds
    n = int(dur * RATE)
    env = envelope(n, decay)
    last = 0.0
    frames = []
    for i in range(n):
        # lightly low-passed noise reads as "explosion", not static
        last = 0.7 * last + 0.3 * random.uniform(-1.0, 1.0)
        frames.append(int(max(-1.0, min(1.0, last * mag * 2.0)) * env[i] * 32767))
    return frames


def sequence(notes, kind="sine", decay=6.0, mag=0.5):
    """Concatenate [(freq, dur), ...] with per-note decay restart."""
    frames = []
    for freq, dur in notes:
        frames += tone(freq, freq, dur, kind, decay, mag)
    return frames


def save(name, frames):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / name
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(struct.pack("<%dh" % len(frames), *frames))
    print(f"wrote {path} ({len(frames) / RATE:.2f}s)")


def music_loop():
    # 8 s loop: Am - F - C - G, soft square bass + sine lead arpeggio.
    chords = [(110.0, [220.0, 261.63, 329.63]),  # Am
              (87.31, [174.61, 220.0, 261.63]),   # F
              (130.81, [261.63, 329.63, 392.0]),  # C
              (98.0, [196.0, 246.94, 293.66])]    # G
    frames = [0] * (8 * RATE)
    for c, (bass, arp) in enumerate(chords):
        start = c * 2 * RATE
        for i in range(2 * RATE):
            t = i / RATE
            v = 0.25 * math.sin(2 * math.pi * bass * t)
            note = arp[(i // (RATE // 4)) % 3]  # eighth-note arpeggio
            v += 0.20 * math.sin(2 * math.pi * note * t)
            # 50 ms edge fades to soften the loop seam
            edge = min(1.0, i / (0.05 * RATE), (2 * RATE - i) / (0.05 * RATE))
            frames[start + i] = int(max(-1.0, min(1.0, v)) * edge * 32767)
    return frames


def main():
    save("select.wav", tone(660, 660, 0.07, "square", 30.0))
    save("confirm.wav", tone(520, 520, 0.09, "square", 22.0))
    save("attack.wav", tone(180, 140, 0.12, "saw", 18.0))
    save("explosion.wav", noise(0.40, 8.0))
    save("place.wav", tone(300, 300, 0.15, "triangle", 14.0))
    save("deplete.wav", tone(220, 110, 0.25, "sine", 10.0))
    save("victory.wav", sequence([(523.25, 0.13), (659.25, 0.13), (783.99, 0.26)]))
    save("defeat.wav", tone(330, 196, 0.50, "sine", 6.0))
    save("music_loop.wav", music_loop())


if __name__ == "__main__":
    main()
