#!/usr/bin/env python3
"""Finds isolated paddle hits (pocks) in field recordings: the energy of the pre-emphasised signal in 2 ms frames,
onsets more than 18 dB over the median of the 150 ms before them that fall 15 dB within 60-80 ms (a hit, not a
voice), at most one per 200 ms. Writes the 40 most prominent as 120 ms clips, ranked, with their spectral centroid
and peak. Usage: findpocks.py OUTDIR file.wav ...   (mono 16-bit WAVs; ffmpeg -ac 1 -ar 32000 makes them)"""
import sys, wave
import numpy as np
from numpy.lib.stride_tricks import sliding_window_view

out = sys.argv[1]
rows = []
for path in sys.argv[2:]:
    w = wave.open(path); sr = w.getframerate()
    x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float32) / 32768; w.close()
    y = np.empty_like(x); y[0] = 0; y[1:] = x[1:] - 0.95 * x[:-1]
    hop = sr // 500
    m = len(y) // hop
    e = 10 * np.log10(np.mean(y[:m * hop].reshape(m, hop) ** 2, axis=1) + 1e-12)
    bg = np.median(sliding_window_view(np.concatenate([np.full(75, e[:75].mean()), e]), 75)[:m], axis=1)
    last = -1000
    for i in np.where((e - bg > 18) & (e > -50))[0]:
        if i - last < 100: continue
        peak = i + int(np.argmax(e[i:i + 5]))
        after = e[peak + 30:peak + 40]
        if len(after) < 10 or e[peak] - after.mean() < 15: continue
        last = i
        s0 = max(0, peak * hop - int(0.005 * sr))
        clip = x[s0:s0 + int(0.12 * sr)]
        n = int(0.04 * sr)
        if len(clip) < n: continue
        spec = np.abs(np.fft.rfft(clip[:n] * np.hanning(n))); f = np.fft.rfftfreq(n, 1 / sr)
        hi = f > 300
        rows.append((float(e[peak] - bg[i]), path.split('/')[-1][:6], peak * hop / sr,
                     float((spec * f).sum() / spec.sum()), float(f[hi][np.argmax(spec[hi])]), clip, sr))
rows.sort(key=lambda r: -r[0])
for k, (prom, name, t, cent, fpk, clip, sr) in enumerate(rows[:40]):
    c = (clip / max(1e-6, np.abs(clip).max()) * 0.9 * 32767).astype(np.int16)
    w = wave.open("%s/pock%02d_%s_%07.2fs.wav" % (out, k, name, t), "wb")
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr); w.writeframes(c.tobytes()); w.close()
    print("%2d %s t=%8.2fs  prominence %4.1f dB  centroid %5.0f Hz  peak %5.0f Hz" % (k, name, t, prom, cent, fpk))
print("candidates:", len(rows))
