#!/usr/bin/env python3
"""The DPCM encoder and sample packer. Reads audio/samples.txt (one sample a line: name, source WAV, start and
length in ms, DMC rate index, gain in dB, and options), and writes audio/dpcm/ (committed, so a build needs no
numpy): one .dmc file per sample, samples.s (each in its sample bank's segment, 64-byte aligned, never across a
bank) and samples.h (the tables src/sound.c plays them from). Run it on the laptop when a sample changes:
`make samples`.

The encoding, per sample:
  1. band-limit: a windowed-sinc low-pass at 0.45 of the DMC rate, then resample to it (no aliasing);
  2. fade in 1 ms and out 8 ms, so the sample starts and ends at rest;
  3. scale to the level the DMC can follow: the 7-bit level moves 2 steps a sample, so the gain is the loudest
     at which the encoded level still tracks the signal to within -9 dB, then the gain option, through a soft
     limiter (tanh); a 250 Hz high-pass first (the field recordings' wind rumble);
  4. encode by a Viterbi search over the 64 even levels that minimises the squared error (the old encoder was
     greedy: one bit at a time, no lookahead), starting and ending on the resting level, 64;
  5. pad to 16n+1 bytes with alternating bits (the level holds).
Options: far (a quieter copy, -7 dB, for the far court), flip (inverts the polarity), lp=Hz (the target's
low-pass, 4000 by default).
Also writes, with --preview DIR, each sample as the DMC would play it, as a WAV at the DMC rate."""
import sys, os, wave, argparse
import numpy as np

RATES = [4182, 4710, 5264, 5593, 6258, 7046, 8363, 9420, 11186, 12604, 13983, 16885, 18634, 22372, 26584, 33144]
REST = 64
BANK = 0x2000

def read_wav(path):
    w = wave.open(path)
    ch, sr, sw = w.getnchannels(), w.getframerate(), w.getsampwidth()
    raw = w.readframes(w.getnframes()); w.close()
    x = np.frombuffer(raw, {1: np.uint8, 2: np.int16}[sw]).astype(np.float64)
    x = (x - 128) / 128 if sw == 1 else x / 32768
    if ch > 1: x = x.reshape(-1, ch).mean(1)
    return x, sr

def resample(x, sr, rate):
    """windowed-sinc low-pass at 0.45 * rate, then linear interpolation from the (now band-limited) signal"""
    fc = 0.45 * rate / sr
    if fc < 0.5:
        n = 129
        t = np.arange(n) - (n - 1) / 2
        h = 2 * fc * np.sinc(2 * fc * t) * np.blackman(n)
        x = np.convolve(x, h / h.sum(), mode="same")
    m = int(len(x) * rate / sr)
    return np.interp(np.arange(m) * sr / rate, np.arange(len(x)), x)

def viterbi(target, start=REST, end=REST):
    """target: desired levels (float), one per DMC sample. Returns the bits that track it best, from start to end.
    Every bit moves the level, so the length must be even to come back to the level it started on."""
    if len(target) & 1: target = np.append(target, end)
    L = np.arange(0, 128, 2)                       # the even levels the DMC can reach from 64
    S = len(L)
    INF = 1e18
    cost = np.full(S, INF); cost[start // 2] = 0.0
    back = np.zeros((len(target), S), np.int8)    # 0: came from below (bit 1), 1: from above (bit 0), 2: clamped
    for i, tv in enumerate(target):
        up = np.full(S, INF); up[1:] = cost[:-1]                 # level-2 -> level with a 1 bit
        dn = np.full(S, INF); dn[:-1] = cost[1:]                 # level+2 -> level with a 0 bit
        # clamping: at the top a 1 bit stays (126 is not reachable by +2 from 126); at 0 a 0 bit stays
        stay_top = np.full(S, INF); stay_top[-1] = cost[-1]
        stay_bot = np.full(S, INF); stay_bot[0] = cost[0]
        cand = np.stack([up, dn, stay_top, stay_bot])
        k = np.argmin(cand, axis=0)
        cost = cand[k, np.arange(S)] + (L - tv) ** 2
        back[i] = k
    s = end // 2
    if cost[s] >= INF: s = int(np.argmin(cost))
    bits = []
    for i in range(len(target) - 1, -1, -1):
        k = back[i, s]
        if k == 0: bits.append(1); s -= 1
        elif k == 1: bits.append(0); s += 1
        elif k == 2: bits.append(1)
        else: bits.append(0)
    return bits[::-1]

def decode(bits, start=REST):
    lv, out = start, []
    for b in bits:
        if b and lv <= 125: lv += 2
        elif not b and lv >= 2: lv -= 2
        out.append(lv)
    return np.array(out, float)

def highpass(x, sr, fc=250.0):
    """a 2nd-order Butterworth high-pass: field recordings carry wind rumble, which the DMC would follow at full
    swing (low frequencies have gentle slopes) and pin the level against its end"""
    w = np.tan(np.pi * fc / sr); k = 1 / (1 + np.sqrt(2) * w + w * w)
    b0, b1, b2 = k, -2 * k, k
    a1, a2 = 2 * (w * w - 1) * k, (1 - np.sqrt(2) * w + w * w) * k
    y = np.zeros_like(x); x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        o = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, v, y1, o; y[i] = o
    return y

def lowpass(x, rate, fc):
    n = 63
    t = np.arange(n) - (n - 1) / 2
    h = 2 * fc / rate * np.sinc(2 * fc / rate * t) * np.blackman(n)
    return np.convolve(x, h / h.sum(), mode="same")

def track(y, g, rate):
    t = np.tanh(y * g / 60) * 60 + REST            # a soft limit at +-60 levels
    if len(t) & 1: t = np.append(t, REST)
    bits = viterbi(t)
    lev = decode(bits)
    # the error that is heard: below 5 kHz (the level's fine toggling above it is not counted against it)
    e = lowpass(lev - t, rate, 5000)
    ref = lowpass(t - REST, rate, 5000)
    return bits, lev, t, np.sqrt(np.mean(e ** 2)) / (np.sqrt(np.mean(ref ** 2)) + 1e-9)

def encode(x, sr, rate_idx, gain_db, flip=False, lp=4000, rms=None):
    """the target is low-passed at lp Hz (the DMC cannot follow a steep click loudly), then the gain is the
    loudest at which the level still follows it to within -6 dB below 5 kHz (some slope overload: the crunch
    that gives NES drum samples their punch); the manifest's gain goes on top"""
    rate = RATES[rate_idx]
    y = resample(highpass(x - np.mean(x), sr), sr, rate)
    if lp < 0.45 * rate: y = lowpass(y, rate, lp)
    if flip: y = -y
    n = len(y)
    fi, fo = max(1, int(0.001 * rate)), max(1, int(0.008 * rate))
    env = np.ones(n); env[:fi] = np.linspace(0, 1, fi); env[-fo:] = np.linspace(1, 0, fo)
    y = y * env
    pk = np.abs(y).max() + 1e-9
    best = 4 / pk
    if rms:                                         # a fixed loudness (the voice: every word alike)
        best = rms / (np.sqrt(np.mean(y ** 2)) + 1e-9)
    else:
        for g in 4 / pk * 2 ** (np.arange(0, 64) / 4):  # quarter-octave steps upward from +-4 levels at the peak
            if track(y, g, rate)[3] <= 0.5: best = g
    bits, lev, t, rel = track(y, best * 10 ** (gain_db / 20), rate)
    while (len(bits) - 8) % 128: bits.append(len(bits) & 1)
    data = bytes(sum(bits[i + j] << j for j in range(8)) for i in range(0, len(bits), 8))
    assert len(data) <= 4081, "%d bytes: a DMC sample holds at most 4081 (shorten it or lower the rate)" % len(data)
    return data, decode(bits), t

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifest", nargs="?", default="audio/samples.txt")
    ap.add_argument("--out", default="audio/dpcm")
    ap.add_argument("--preview")
    o = ap.parse_args()
    os.makedirs(o.out, exist_ok=True)
    base = os.path.dirname(o.manifest)
    entries = []
    for line in open(o.manifest):
        line = line.split("#")[0].strip()
        if not line: continue
        p = line.split()
        if p[1].startswith("@"):                   # an alias: another sample's data at another rate
            entries.append((p[0], int(p[2]), p[1][1:]))
            print("%-16s rate %2d %5d Hz  = %s" % (p[0], int(p[2]), RATES[int(p[2])], p[1][1:]))
            continue
        name, src, t0, dur, rate, gain = p[0], p[1], float(p[2]), float(p[3]), int(p[4]), float(p[5])
        opts = set(p[6:])
        x, sr = read_wav(os.path.join(base, src))
        x = x[int(t0 / 1000 * sr):int((t0 + dur) / 1000 * sr)]
        variants = [(name, gain)] + ([(name + "_far", gain - 7)] if "far" in opts else [])
        for vname, vg in variants:
            lp = ([float(o[3:]) for o in opts if o.startswith("lp=")] or [4000])[0]
            rms = ([float(o[4:]) for o in opts if o.startswith("rms=")] or [None])[0]
            data, lev, t = encode(x, sr, rate, vg, "flip" in opts, lp, rms)
            err = np.sqrt(np.mean((lev[:len(t)] - t) ** 2))
            ac = lev - lev.mean()
            print("%-16s rate %2d %5d Hz  %4d bytes  %4.0f ms  swing %3d-%3d  rms %4.1f levels  track err %4.1f" %
                  (vname, rate, RATES[rate], len(data), len(lev) / RATES[rate] * 1000, lev.min(), lev.max(),
                   np.sqrt(np.mean(ac ** 2)), err))
            open(os.path.join(o.out, vname + ".dmc"), "wb").write(data)
            entries.append((vname, rate, data))
            if o.preview:
                os.makedirs(o.preview, exist_ok=True)
                w = wave.open(os.path.join(o.preview, vname + ".wav"), "wb")
                w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATES[rate])
                w.writeframes(((lev - REST) / 64 * 32767 * 0.9).astype(np.int16).tobytes()); w.close()
    # pack: first fit, in manifest order, into 8 KB banks; 64-byte aligned
    banks, place, at = [], [], {}
    for name, rate, data in entries:
        if isinstance(data, str):
            o_name, o_rate, o_data, o_b, o_off = at[data]
            place.append((name, rate, o_data, o_b, o_off)); continue
        size = (len(data) + 63) // 64 * 64
        for b, used in enumerate(banks):
            if used + size <= BANK: break
        else:
            banks.append(0); b = len(banks) - 1
        place.append((name, rate, data, b, banks[b])); at[name] = place[-1]; banks[b] += size
    with open(os.path.join(o.out, "samples.s"), "w") as f:
        f.write("; generated by tools/dpcm.py from audio/samples.txt: the DPCM samples in their banks\n")
        for b in range(len(banks)):
            f.write('.segment "DMC%02d"\n' % b)
            for name, rate, data, pb, off in place:
                if pb == b and at.get(name) == (name, rate, data, pb, off):
                    f.write('.align 64\n.incbin "%s/%s.dmc"\n' % (o.out, name))
    with open(os.path.join(o.out, "samples.h"), "w") as f:
        f.write("/* generated by tools/dpcm.py from audio/samples.txt: the DPCM samples (bank at $C000, $4012 address,\n"
                "   $4013 length, $4010 rate) */\n#define SMP_COUNT %d\n#define SMP_BANKS %d\n" % (len(place), len(banks)))
        for i, (name, *_ ) in enumerate(place): f.write("#define SMP_%s %d\n" % (name.upper(), i))
        f.write("#ifdef SAMPLE_TABLES\n")
        f.write("static const unsigned char SMP_BANK[%d] = {%s};\n" % (len(place), ",".join(str(p[3]) for p in place)))
        f.write("static const unsigned char SMP_ADDR[%d] = {%s};\n" % (len(place), ",".join(str(p[4] // 64) for p in place)))
        f.write("static const unsigned char SMP_LEN[%d] = {%s};\n" % (len(place), ",".join(str((len(p[2]) - 1) // 16) for p in place)))
        f.write("static const unsigned char SMP_RATE[%d] = {%s};\n" % (len(place), ",".join(str(p[1]) for p in place)))
        f.write("#endif\n")
    print("%d samples in %d banks: %s" % (len(place), len(banks), ", ".join("%d bytes" % u for u in banks)))

main()
