#!/usr/bin/env python3
# Renders the phrase cache of the teammate add-on: every phrase in voices.PHRASES, in each of the five voices (the
# four player slots and the umpire), as WAVs under DIR/<voice>/. Run once at install or first start, with
# llamatalk's tts-server up on PB_TTS (default port 8093); it only renders what is missing, so a second run is quick.
# Usage: cache.py DIR [--voices alba,jean,marius,cosette,george] [--wait SECONDS]
#   --wait: how long to wait for the TTS server (default 120 s). Exit 1: no server, or something failed to render.
# Env: PB_TTS, PB_VOICES (the same as --voices).
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import voices

if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1].startswith("--"): sys.exit(__doc__ or "usage: cache.py DIR [--voices a,b,c,d,u] [--wait S]")
    d = sys.argv[1]; wait = 120.0; want = voices.voices_from_env()
    if "--voices" in sys.argv: want = tuple(sys.argv[sys.argv.index("--voices") + 1].split(","))
    if "--wait" in sys.argv: wait = float(sys.argv[sys.argv.index("--wait") + 1])
    t0 = time.time()
    while not voices.tts_up():
        if time.time() - t0 > wait: sys.exit("cache: no tts-server on %s" % voices.TTS)
        time.sleep(1)
    have = voices.tts_voices()
    if have:
        bad = [v for v in want if v not in have]
        if bad: print("cache: the server has no voice %s (it has: %s)" % (", ".join(bad), ", ".join(have)), flush=True)
    done, failed = voices.ensure(d, want, log=lambda s: print(s, flush=True))
    n = sum(len(os.listdir(os.path.join(d, v))) for v in want if os.path.isdir(os.path.join(d, v)))
    print("cache: %d WAVs in %s for %s" % (n, d, ", ".join(want)), flush=True)
    sys.exit(1 if failed else 0)
