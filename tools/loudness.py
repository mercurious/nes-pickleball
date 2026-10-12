#!/usr/bin/env python3
"""Measures what a recording from test/record.lua sounds like: loudness, not activity. For every game event in
the test's log, the peak and the loudest 50 ms RMS (dBFS) in the window from the event to the next one (at most
WIN ms), and per event kind the median. A timeline of 100 ms RMS bars with --bars. --cut DIR writes each event's
window as its own WAV. Standard library only (etk-cloud has no numpy).
Usage: loudness.py rec.wav record.log [--win 400] [--bars] [--cut DIR] [--min CODE=DB ...]
--min CODE=DB fails (exit 1) when the median loudest-50-ms RMS of event CODE is below DB dBFS."""
import sys, wave, array, math, os, argparse, statistics

EV = ["NONE", "SCORE_CALL", "SERVE", "RETURN", "THIRD_SHOT", "DINK", "DRIVE", "LOB", "VOLLEY", "SMASH",
      "BOUNCE_IT", "MINE", "YOURS", "SWITCH", "POINT_WON", "FAULT_OUT", "FAULT_NET", "FAULT_LET", "FAULT_SHORT",
      "FAULT_WRONG_COURT", "FAULT_FOOT", "FAULT_TWO_BOUNCE", "FAULT_DOUBLE_BOUNCE", "FAULT_KITCHEN", "FAULT_BODY",
      "FAULT_NET_TOUCH", "SIDE_OUT", "SECOND_SERVER", "GAME_OVER", "NICE_SHOT", "LONG_RALLY", "PASSED"]
FPS = 60.0988

def read(path):
    w = wave.open(path)
    ch, sr, n = w.getnchannels(), w.getframerate(), w.getnframes()
    a = array.array("h"); a.frombytes(w.readframes(n)); w.close()
    if ch == 2: a = array.array("h", ((a[i] + a[i + 1]) >> 1 for i in range(0, len(a), 2)))
    return a, sr

def db(x): return 20 * math.log10(max(x, 1e-9) / 32768)

def measure(a, sr, s0, s1):
    seg = a[s0:s1]
    if not seg: return -180.0, -180.0
    peak = max(abs(v) for v in seg)
    n = max(1, int(0.05 * sr)); hop = max(1, n // 4); best = 0.0
    for i in range(0, max(1, len(seg) - n + 1), hop):
        part = seg[i:i + n]
        mean = sum(part) / len(part)
        r = math.sqrt(sum((v - mean) ** 2 for v in part) / len(part))
        best = max(best, r)
    return db(peak), db(best)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wav"); ap.add_argument("log")
    ap.add_argument("--win", type=int, default=400); ap.add_argument("--bars", action="store_true")
    ap.add_argument("--cut"); ap.add_argument("--min", nargs="*", default=[])
    o = ap.parse_args()
    a, sr = read(o.wav)
    evs = []
    for line in open(o.log):
        p = line.split()
        if len(p) >= 4 and p[0] == "EV": evs.append((int(p[1]), int(p[2]), int(p[3])))
    print("%s: %.1f s at %d Hz, peak %.1f dBFS" % (o.wav, len(a) / sr, sr, db(max(abs(v) for v in a) if a else 0)))
    kinds = {}
    for i, (f, code, arg) in enumerate(evs):
        s0 = int(f / FPS * sr)
        s1 = int(min(evs[i + 1][0] if i + 1 < len(evs) else 1e9, f + o.win / 1000 * FPS) / FPS * sr)
        pk, rms = measure(a, sr, s0, s1)
        name = EV[code] if code < len(EV) else str(code)
        if code: kinds.setdefault(code, []).append(rms)
        print("%7.2f s  %-20s arg %d   peak %6.1f  rms50 %6.1f dBFS" % (f / FPS, name, arg, pk, rms))
        if o.cut:
            os.makedirs(o.cut, exist_ok=True)
            w = wave.open("%s/%03d_%s.wav" % (o.cut, i, name.lower()), "wb")
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr); w.writeframes(a[s0:s1].tobytes()); w.close()
    print("per event kind (median loudest-50-ms RMS, dBFS):")
    for code in sorted(kinds):
        print("  %-20s n=%3d  %6.1f" % (EV[code] if code < len(EV) else code, len(kinds[code]), statistics.median(kinds[code])))
    if o.bars:
        n = int(0.1 * sr)
        for i in range(0, len(a), n):
            part = a[i:i + n]; mean = sum(part) / len(part)
            r = db(math.sqrt(sum((v - mean) ** 2 for v in part) / len(part)))
            print("%6.1f %6.1f %s" % (i / sr, r, "#" * max(0, int((r + 60) / 2))))
    bad = 0
    for m in o.min:
        code, lim = m.split("="); code = int(code); lim = float(lim)
        got = statistics.median(kinds[code]) if code in kinds else -180.0
        if got < lim: print("FAIL: %s median %.1f dBFS < %.1f" % (EV[code], got, lim)); bad = 1
    sys.exit(bad)

main()
