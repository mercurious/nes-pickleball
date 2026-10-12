# Sound credits

## The pocks and the bounce (`audio/samples/`)

Single hits cut by `tools/findpocks.py` from field recordings of pickleball games, all dedicated to the public
domain (CC0 1.0). CC0 asks for nothing; the recordists are named here anyway, with thanks.

| file | from | recordist |
|---|---|---|
| `fkunze_022.51.wav`, `fkunze_099.04.wav`, `fkunze_105.58.wav`, `fkunze_106.97.wav` | ["PickleBall.m4a"](https://freesound.org/people/fkunze/sounds/547092/), at those seconds | fkunze (Freesound) |
| `oldome_067.14.wav` | [park ambience with pickleball](https://freesound.org/people/Oldome/sounds/856310/), at 67.14 s | Oldome (Freesound) |

They were cut from Freesound's high-quality previews; the original files (a free Freesound login) can replace
them without other changes.

## The umpire (`audio/voice/`)

Rendered with [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M) (hexgrad/Kokoro-82M), whose weights and voices
are under the Apache License 2.0, which allows redistribution, in a ROM too. Voice `am_michael`. Rendered by
`tools/umpire_tts.py`, trimmed by `tools/voiceprep.py`.

## The engine

[Pently](https://github.com/pinobatch/pently) by Damian Yerrick, zlib licence (`lib/pently/`).
