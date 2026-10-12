#!/usr/bin/env python3
"""Prepares the umpire's words for the DPCM encoder: trims the silence around each word (45 dB under its loudest
5 ms, with 15 ms kept either side), evens out their loudness (the same RMS for every word), and writes them as
16-bit mono WAVs, plus the manifest lines for audio/samples.txt.
Usage: voiceprep.py SRC_DIR OUT_DIR > lines   (SRC_DIR from tools/umpire_tts.py, one voice)"""
import sys, os, wave, glob
import numpy as np

src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
for fn in sorted(glob.glob(os.path.join(src, "*.wav"))):
    w = wave.open(fn); sr = w.getframerate(); x = np.frombuffer(w.readframes(w.getnframes()), np.int16) / 32768.0; w.close()
    n = int(0.005 * sr)
    e = np.convolve(x ** 2, np.ones(n) / n, "same")
    idx = np.where(e > e.max() * 10 ** (-45 / 10))[0]
    a, b = max(0, idx[0] - int(0.015 * sr)), min(len(x), idx[-1] + int(0.015 * sr))
    y = x[a:b]
    y = y / (np.sqrt(np.mean(y ** 2)) + 1e-9) * 0.1          # -20 dBFS RMS
    y = np.clip(y, -1, 1)
    name = os.path.basename(fn)
    w = wave.open(os.path.join(out, name), "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr)
    w.writeframes((y * 32767).astype(np.int16).tobytes()); w.close()
    print("%-18s %s/%s  0 %5.0f  15  0  rms=11 lp=5000" % (name[:-4], os.path.relpath(out, "audio"), name, (b - a) / sr * 1000 + 1))
