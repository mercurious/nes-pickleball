#!/usr/bin/env python3
"""Renders the umpire's words with Kokoro (hexgrad/Kokoro-82M, Apache-2.0 weights and voices) into
OUT/<voice>/<word>.wav at 24 kHz. Runs in Docker on etk-cloud, never on the laptop (AGENTS.md):
  docker run --rm -v ~/nes-pickleball-tools/tts/hf:/root/.cache/huggingface -v $PWD:/work pb-tts \
      python3 tools/umpire_tts.py OUT am_michael af_heart bm_george
The image: python:3.12-slim + espeak-ng + torch (CPU) + kokoro + soundfile (docs/SOUND.md)."""
import sys, os
import numpy as np
import soundfile as sf
from kokoro import KPipeline

WORDS = [("w_%s" % n, n) for n in ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
         "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen", "twenty"]]
WORDS += [("w_fault", "Fault!"), ("w_out", "Out!"), ("w_let", "Let."), ("w_kitchen", "Kitchen!"),
          ("w_side_out", "Side out."), ("w_second_server", "Second server."), ("w_game", "Game!")]

out = sys.argv[1]
for voice in sys.argv[2:]:
    pipe = KPipeline(lang_code=voice[0], repo_id="hexgrad/Kokoro-82M")
    os.makedirs(os.path.join(out, voice), exist_ok=True)
    for wid, text in WORDS:
        audio = np.concatenate([np.asarray(a) for _, _, a in pipe(text, voice=voice, speed=1.05)])
        sf.write(os.path.join(out, voice, wid + ".wav"), audio, 24000, subtype="PCM_16")
        print(voice, wid, "%.2f s" % (len(audio) / 24000))
