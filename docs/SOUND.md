# The sound (v0.1.0)

How the cartridge makes its sound, how it is tested, and what was found on the way. The plan it follows is
`docs/DOSSIER_v0.1.0.md` §6.

## The parts

| part | where | what |
|---|---|---|
| Pently | `lib/pently/` (zlib), `src/pentlyconfig.inc` | the music engine: the jingles, the title tune, the synthesized effects; pulse 1, pulse 2, triangle, noise |
| the score | `audio/pickleball.pently`, `audio/crowd.pently` (`tools/crowd.py`) | the v0.0.1 jingles note for note (placeholders until the new music), the menu and pause blips, the two-frame ring under each hit, the bounce's knock, the net cord, applause |
| the samples | `audio/samples.txt` → `tools/dpcm.py` → `audio/dpcm/` (committed) | the DMC's sounds: one pock per shot kind (drive, serve, volley, smash, dink), a far-court copy of each 7 dB down, the bounce (the dink's far copy at a slower rate), and the umpire's 28 words |
| the umpire | `tools/umpire_tts.py` (Kokoro in Docker on etk-cloud), `tools/voiceprep.py` | 0-20, fault, out, let, kitchen, side out, second server, game; voice `am_michael` |
| the driver | `src/sound.c` | maps the game's moments to sounds, plays the DMC (one sample at a time, its bank mapped at $C000), queues the voice's words, holds a jingle until the umpire has spoken |

`snd_style` (RAM) picks how a hit sounds: 0 the sample over the synth ring (the default), 1 the sample alone, 2 the
synth alone. The listening tests use it.

## The DPCM encoder (`tools/dpcm.py`)

v0.0.1's encoder resampled without a low-pass filter, encoded greedily one bit at a time at 7 kHz, and left the
words 25 dB down. Now, per sample:

1. a 250 Hz high-pass (the field recordings' wind rumble pinned the level against its end);
2. a band-limit at 0.45 of the DMC rate, then the resample (33,144 Hz, rate 15, for everything but the bounce);
3. a low-pass for the target (4 kHz for the pocks, 5 kHz for the voice), a 1 ms fade in and 8 ms out;
4. the gain: for the pocks, the loudest at which the encoded level still follows the signal to within -6 dB
   below 5 kHz (the DMC moves 2 levels a sample, so a 1.3 kHz ring can swing only about +-8 levels: some slope
   overload is the punch); for the voice, the same loudness for every word (rms=11 levels);
5. a Viterbi search over the 64 even levels that minimises the squared error, from 64 back to 64 (an even
   number of bits, or the level cannot come back to where it started);
6. padding to 16n+1 bytes with alternating bits; at most 4,081 bytes, or the next lower rate.

## Rules the driver keeps

- **The DMC's resting level is 64**, written once by `pently_init`. Every sample starts and ends there, so the
  level never jumps (a jump clicks) and the triangle and noise keep one loudness (the 2A03's mixer makes them
  quieter as the DMC's level rises). A sample cut off by another is set back to 64 once the DMC is idle: the
  only other write to `$4011`.
- **Nothing reads `$4015`, and the controllers are read in step with the OAM DMA** (`src/crt0.s`, the NESdev
  wiki's DMC-safe reading). On the 2A03, a DMC fetch that lands while the CPU reads `$4015-$4017` hands the DMC a
  corrupted byte when the sample's address ends in `$15-$17` (and costs the controller a bit). Mesen emulates it;
  it knocked the voice's level 8 at a time off its resting point. The DMC's time left is counted in frames.

## The tests

- `test/voice.lua`: the score call and the first serves are recorded and measured (`tools/loudness.py`): the
  call's loudest 50 ms must reach -32 dBFS, a serve's hit -40 (they measure about -29 and -32).
- `test/cues.lua`: 150 s of an exhibition, reading the APU's own state (Mesen's `emu.getState`): every channel
  silent before every serve, and the DMC's level back on 64 within a frame whenever it is idle.
- `test/record.lua` + `tools/loudness.py`: any recording, every event's peak and loudest 50 ms, per kind.
- `test/apulog.lua`: any ROM, recorded, with every APU write logged (how NES Tennis was studied).

The recordings need MesenCE with `tools/mesen-wave.patch` (Lua `emu.startWaveRecording`/`stopWaveRecording`),
built on etk-cloud.

## Findings

1. **v0.0.1's effects never ended.** `fx_step` never advanced to the next step, so each effect's first step held
   until the next effect: a 445 Hz tone from the menu blip until the serve, then 695 Hz, then a 130 Hz triangle
   drone under every rally, over the voice. That is the "truly unpleasant" sound; the original NROM ROM and the
   MMC3 port record the same.
2. **NES Tennis** (the maintainer's dump, never committed), register by register: the hit is a 50 % pulse at
   about 1 kHz, volume 6, its pitch swept down by the hardware sweep and faded over 0.4 s; another tone near
   525 Hz with an upward sweep; a crowd murmur on pulse 2 the whole time (25 % duty, volume 1-2, a new pitch every
   frame); no noise channel, no samples; `$4011` ramped by hand. It peaks around -29 dBFS in Mesen's mix.
3. **A real pock** (the CC0 recordings): a click with a hollow ring at 0.7-1.4 kHz that falls 20 dB within
   20-25 ms. Tennis's 0.4 s "ponk" is a tennis sound; the synthesized ring here is two frames.
4. **Loudness in Mesen's default mix**: a layered hit -30 to -34 dBFS (loudest 50 ms), the sample alone 6-10 dB
   quieter, a word of the umpire about -26 to -29, a jingle about -27.
5. **The ball was drawn 10-12 px below where it was** (and its shadow): its art sits at the bottom of the 8x16
   sprite and `render.c` placed it as if at the top. Fixed with the paddle (plan step 7).

## The verdict (2026-10-24)

The maintainer heard the sound as built (the layered hits, these pocks, `am_michael` in the chair) and approved
it: "the sound is good". It went out as shipped in the v0.0.1 re-release. The listening packs on car12
(`/storage/nes-pickleball/listen1/`, `listen2/`) remain for comparing any later change against.
