#!/usr/bin/env python3
"""A listening set for the pock: each clip as recorded (resampled to 33,144 Hz), then as the DMC would play it at
rate index 15 (a first-cut greedy encoder: one bit per sample, the 7-bit level stepping by 2, 80 ms per clip).
Writes OUTDIR/shortlist.wav and the numbered list OUTDIR/shortlist.txt. Usage: pockset.py OUTDIR clip.wav ..."""
import sys, wave
import numpy as np

out = sys.argv[1]
R = 33144

def read(path):
    w = wave.open(path); sr = w.getframerate()
    x = np.frombuffer(w.readframes(w.getnframes()), np.int16) / 32768.0; w.close()
    return np.interp(np.arange(int(len(x) * R / sr)) / R, np.arange(len(x)) / sr, x)

def dmc(x):
    x = x - x[:200].mean(); x = x / max(1e-6, np.abs(x).max())
    level, y = 64, []
    for v in x:
        level = min(126, level + 2) if 64 + v * 60 > level else max(0, level - 2)
        y.append(level)
    return (np.array(y, float) - 64) / 64, (len(x) + 7) // 8

def write(path, a):
    w = wave.open(path, "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(R)
    w.writeframes((np.clip(a, -1, 1) * 32767).astype(np.int16).tobytes()); w.close()

gap = np.zeros(int(0.35 * R)); seq = []; rows = []
for i, path in enumerate(sys.argv[2:]):
    x = read(path)[:int(0.08 * R)]
    y, nbytes = dmc(x)
    seq += [x / max(1e-6, np.abs(x).max()) * 0.7, gap[:R // 8], y * 0.7, gap]
    rows.append("%2d  %s  (%d bytes as DPCM, 80 ms)" % (i + 1, path.split('/')[-1], nbytes))
write(out + "/shortlist.wav", np.concatenate(seq))
with open(out + "/shortlist.txt", "w") as f:
    f.write("Each number: the recording, then the NES sample (rate 15, 33 kHz, first-cut encoder)\n")
    f.write("\n".join(rows) + "\n")
print("\n".join(rows))
