# Dossier: the next cartridge, v0.1.0 (2026-10-11)

What the maintainer found playing v0.0.1, what a review session found behind it, the decisions taken, and the plan.
Pick up here: §8 says where the build-out stands. The sound's design and findings are in `docs/SOUND.md`.

## 1. The maintainer's verdict on v0.0.1

- **The sound is a disappointment**: "truly unpleasant", and it sounds nothing like pickleball. Making it as archaic
  as possible was a joke that didn't land. The bar is NES *Tennis*, which sounds like tennis.
- **No speech was heard** in the maintainer's tests.
- **1 PLAYER DOUBLES didn't work.**
- **The paddle** doesn't stick out from the player enough to look like it meets the ball.
- To Fable's credit: a fairly functional homebrew cartridge, and a strong foundation to build on.

## 2. Decisions (the maintainer, 2026-10-11)

1. **Period authenticity was proven in v0.0.1.** From here on the cartridge is free to have more fun. It stays
   a real NES cartridge that plays on stock hardware and in any emulator, but it uses the hardware fully, not the
   way a 1985 launch title would have.
2. **A bigger board is fine.** Recommended: MMC3 (mapper 4, TxROM), see §5.
3. **The umpire's calls come from TTS**, rendered by us.
4. **The pock**: use a CC0 clip if there is one. If not, use a placeholder and keep the repo private until the
   maintainer records their own. A CC0 source was found (§4), so publishing isn't blocked on this.
5. **Music**: the v0.0.1 jingles stay as placeholders. New music comes later, before a release.
6. **Version**: v0.1.0 for the next cartridge (proposed; v0.0.2 stays the teammate add-on).

## 3. Findings

### 3.1 1 PLAYER DOUBLES: the partner never takes a ball

A bug, not a missing phase. It needs no sidecar or llamatalk. `src/ai.c:147` runs `team_decide` (who takes the
incoming ball) only from a team's even slot. The `!pactive[s & 2]` alternative never fires for slot 1, because
`s & 2` is 0 and slot 0 is active. In 1 PLAYER DOUBLES slot 0 is the human, and `ai_intent` never runs for a human,
so `own[0]` stays 0 for the whole match. The computer partner (slot 1) walks to its cover spot and watches every
ball, including the serves aimed at it. 2 PLAYERS DOUBLES (both human) and the exhibition (both computer) aren't
affected, which is why the soaks never showed it.

Evidence (MesenCE's test runner on `etk-cloud`, `test/doubles.lua`, the human idle for one game):
`own[0]` was 0 on every frame of the 5,127, and team B served 11 times with 0 returns from slot 1.

The fix: the team's decision is made by whichever player on it is the computer. A human partner is treated as
owning the balls nearer them. The middle still goes to the forehand or as the directives say (MB_DIRECTIVE 1/2).
`test/doubles.lua` joins `run.sh`'s default list once it passes.

### 3.2 The speech was there, but inaudible

Decoding `art/speech.bin` the way the DMC does: every word swings the 7-bit level only between about 54 and 78,
an RMS of 2–3 levels out of 63, which is −23 to −26 dB of the channel's range. Three causes:

- **1-bit delta modulation at 7,046 Hz (rate index 5) can't follow speech.** The level moves at most 2 steps per
  sample, so a 1 kHz component can swing only about ±2 levels.
- **The source was resampled without a low-pass filter first**, so it aliased.
- **The point and side-out jingles start in the same frame**, at volume 10, and cover what's left.

`test/speech.lua` only checked that the DMC channel was busy, not that anything audible came out. The lesson for
every sound test from now on: **measure loudness, not activity.**

### 3.3 Why the rest of the sound is unpleasant (`src/sound.c`)

- **The hit is a laser zap**: a 25 % pulse falling from 900 to 700 Hz in whole-frame steps, over noise at periods
  3–6, which is very high hiss. A real pock is a 20–40 ms click with a hollow ring that peaks around 1 kHz (§4).
  A driver that changes registers once a frame can't make it.
- **The jingles are flat squares**: 50 % duty at a constant volume 10, with no envelope, decay or vibrato.
- **Nothing tells the shots apart**: near and far, dink and drive all sound alike, and the bounce is a triangle
  blip. The applause is a hiss that fades.
- **The DMC's resting level ($4011) is set to 64 for every word and left there.** In the NES's mixer this turns
  down the triangle and the noise. Changing it suddenly clicks.

### 3.4 The paddle

In the sprites (`art/sprites.txt`) the paddle is a 2–3 px blob in colour 3, the dark of hair, shorts and shoes. It
sits at the hip, smaller than the ball, and nearly invisible on the green court. `in_reach` (`src/player.c:33`)
hits the ball up to 18 px to either side of the player's centre, while the drawn paddle reaches about 7 px.
The ball is hit by empty air.

## 4. The pock: CC0 sources

There is no CC0 clip of a single hit on Freesound. There are CC0 field recordings of pickleball games, and
`tools/findpocks.py` cuts isolated hits out of them. Most of the strongest hits peak at 0.7–1.4 kHz: the hollow
pock.

| recording | licence | use |
|---|---|---|
| [fkunze, "PickleBall.m4a"](https://freesound.org/people/fkunze/sounds/547092/): two outdoor games in Minnesota, December | CC0 | the best: across all four recordings, all of the 40 most prominent hits (37–51 dB) come from this one |
| [Oldome, park ambience with pickleball](https://freesound.org/people/Oldome/sounds/856310/): Zoom F6 and Rode NTG5 shotgun | CC0 | real pocks, a second character |
| [kvgarlic, "PickelballSummerDay"](https://freesound.org/people/kvgarlic/sounds/825456/) | CC0 | not usable: its sharpest transients peak at 7–8 kHz (birds, clicks) |
| [lori.mortimer, "Abyss"](https://freesound.org/people/lori.mortimer/sounds/723795/): a pickleball dropped in a bathtub, with reverb | CC0 | a bounce at most |

The shortlist: fkunze at 22.51, 84.11, 90.53, 99.04, 65.76, 38.19, 68.67, 79.04, 105.58, 49.77, 106.97 and 48.55 s,
then Oldome at 31.65, 84.71, 101.96, 4.71, 15.97 and 67.14 s. **The maintainer picks the numbers by ear.** As a first-cut
DPCM sample at 33 kHz (rate index 15), an 80 ms pock is 332 bytes and plays about 6 dB quieter than the recording,
because the level can't move faster. A better encoder (§6, step 5) is meant to win part of that back.

To rebuild the listening set (on the laptop: ffmpeg streams, and the analysis is under 100 MB):

```
# the HQ previews (no login); for the final samples use the original files (a free Freesound login)
curl -sL https://cdn.freesound.org/previews/547/547092_3793224-hq.mp3 -o 547092.mp3
curl -sL https://cdn.freesound.org/previews/856/856310_12380230-hq.mp3 -o 856310.mp3
ffmpeg -i 547092.mp3 -ac 1 -ar 32000 -c:a pcm_s16le 547092.wav     # likewise 856310
python3 -I tools/findpocks.py OUT 547092.wav                         # ranked clips, prominence, centroid, peak
python3 -I tools/pockset.py build/listen/pocks OUT/pock0*.wav ...    # shortlist.wav: each recording, then as DPCM
```

Keep downloads in their own directory, outside the repo. CC0 needs no attribution, but a CREDITS file will name the
recordists anyway.

## 5. The board: MMC3

| | NROM-256 (v0.0.1) | MMC3 (TxROM), proposed |
|---|---|---|
| PRG | 32 KB, about 1.9 KB free | 128 or 256 KB in 8 KB banks |
| samples | must sit in $C000-$FFFF; 10 KB of it was speech | PRG mode 1 switches $C000-$DFFF, so each sample bank is visible to the DMC when needed |
| CHR | 8 KB fixed | 1 KB / 2 KB banks: paddle and swing frames, more animation |
| extras | | a scanline IRQ (split screens, a scoreboard band) |
| support | everything | fceumm, nestopia, Mesen, flash carts (EverDrive, PowerPak) |

The code (about 21 KB) goes into the fixed banks plus one bank kept mapped at $A000. The mailbox stays at
$07F0-$07FF (v1); `docs/RAM_MAP.md` regenerates. Expansion-audio boards (VRC6, MMC5, Sunsoft 5B) are ruled out:
a front-loading NES doesn't mix cartridge audio.

## 6. The plan

Laptop builds and tests run on etk-cloud; listening is on car12. Each listening gate puts WAV pairs and a ROM on
car12 and waits for the maintainer's verdict.

1. **Fix 1 PLAYER DOUBLES** (§3.1). *Gate:* `test/doubles.lua` passes; the oracle and the soaks still pass.
2. **The listening rig.**
   - A Lua logger records every write to $4000-$4017 with its CPU cycle in MesenCE's test runner.
   - A renderer turns the log into WAV: VGM through libvgm's `vgm2wav`, or a small Python model of the APU. First
     check whether MesenCE's own WAV recorder can run headless.
   - **Loudness is measured, not just activity**, and every test asserts it.
   - The Tennis references: the maintainer's dump (on car12, `/storage/roms/nes/Tennis.zip`; never committed),
     rendered event by event: a hit, a bounce, the crowd, the stings.
   - *Gate:* a WAV of v0.0.1's own hit and score call that matches what the maintainer heard.
3. **Port to MMC3, with no change in behaviour** (§5). *Gate:* boot, oracle and soak give the same results as on
   NROM-256.
4. **The new driver.**
   - **Pently** (zlib licence): the driver is in ca65, the music a text score built by a Python tool, the sound
     effects take a channel briefly by priority, and it has envelopes, vibrato, arpeggios and drums. The FamiStudio
     engine is the alternative if the maintainer wants to compose in a tracker.
   - `sound.c`, `speech.c`, the espeak words and `tools/dmc.py` go.
   - The v0.0.1 jingles are ported note for note as placeholders, a little quieter.
   - The game's event codes become the list of cues.
5. **The court sounds.**
   - The pock from the CC0 shortlist, in variants: drive, dink, volley, smash and serve. A faster or slower DMC rate
     gives variety for free.
   - The bounce on hard court, the net cord, shoe squeaks perhaps.
   - Far-court shots quieter than near ones. The DMC has no volume control, so the quieter version is a separate
     sample.
   - Each sound is tried as a sample and as synth (the noise channel's short mode is metallic and tonal, close to
     the ball's ring), and the two are compared.
   - A better DPCM encoder: band-limit, then compress to fit what the level can follow, then search ahead rather
     than encode greedily.
   - The DMC's resting level is managed (§3.3).
   - *Gate:* listening on car12.
6. **The umpire's voice.**
   - The calls: the numbers 0-21, "side out", "second server", "fault", "out", "let", "kitchen", "game".
   - TTS, rendered on etk-cloud in Docker (it has no pip or venv, see §7), never on the laptop. Pocket TTS would
     match the v0.0.2 sidecar's umpire.
   - **The voice's licence must allow redistribution in a ROM before anything is published.**
   - Encoded at 33 kHz with the step 5 encoder. Each sample holds at most 4,081 bytes (about 1 s), so the score
     call chains words.
   - The jingles wait until the call finishes.
   - *Gate:* loudness measured, then listening on car12.
7. **The paddle and the contact.**
   - The paddle becomes its own sprite in a light colour, drawn on an outstretched arm. During the swing it's
     placed where the ball is actually met.
   - `in_reach` is matched to what's drawn.
   - The swing frames are redrawn (MMC3 CHR banks).
   - Watch the 8-sprites-per-scanline limit: two partners side by side at the kitchen line with paddles, the ball
     and its shadow are exactly 8. Use the usual flicker rotation if needed.
   - The contact change moves the game's balance. *Gate:* the soaks again, then the maintainer playing it.
8. **The finish.**
   - A Lua test that every cue starts and ends with no channel left sounding.
   - A long soak on etk-cloud, the listening session on car12.
   - Findings, and the v0.1.0 cartridge.

New music comes after this, before a public release.

## 7. Hosts, as found on 2026-10-11

- **The laptop**: cc65 and the tools. ffmpeg and numpy are installed.
- **`etk-cloud`**:
  - MesenCE is at `~/nes-pickleball-tools/MesenCE/bin/linux-arm64/Release/linux-arm64/publish/Mesen`.
  - No ffmpeg, no numpy. `python3 -m venv` fails (no `ensurepip`), so Python audio work there goes in Docker.
  - `~/pb-scratch` holds this session's doubles run.
- **car12**:
  - Reached by its WiFi address; its mDNS is unreliable. The address isn't recorded here.
  - The maintainer's Tennis dump is there. llamatalk is not installed (only a `models-dir` pointer), which doesn't
    matter until v0.0.2.
  - Another project shares it: deploy to our own directory under `/storage`, use `--appendconfig`, kill RetroArch
    by PID.
- **car8**: busy with another session.

## 8. Progress (2026-10-12)

| step | state |
|---|---|
| 1. doubles | done: the partner returned 85 balls where it returned 0; `test/doubles.lua` is in the default list |
| 2. listening rig | done: MesenCE records from Lua (`tools/mesen-wave.patch`), `test/record.lua`, `tools/loudness.py`, `test/apulog.lua`; NES Tennis studied register by register. It found v0.0.1's real fault: no effect ever ended (`docs/SOUND.md`) |
| 3. MMC3 | done: 256 KB PRG, 32 KB CHR, 8 KB PRG-RAM (8 KB of code runs from it); boot, oracle, doubles, soak pass |
| 4. driver | done: Pently; the jingles ported; `sound.c`, `speech.c`, espeak and `dmc.py` gone |
| 5. court sounds | built, waiting on ears: CC0 pocks per shot kind with far copies, the bounce, a two-frame synth ring under each hit, net cord, applause; a new encoder (`tools/dpcm.py`) |
| 6. umpire | built, waiting on ears: Kokoro (Apache-2.0), three voices rendered; `am_michael` in the ROM; `test/voice.lua` measures the call's loudness |
| 7. paddle | done, waiting on a hand: its own red sprite, drawn where the ball is met and held there 4 frames; the ball's 10-12 px drawing offset fixed; flicker in doubles |
| 8. finish | in progress: `test/cues.lua` (every channel silent before every serve, the DMC back on its resting level); the DMC-safe controller reading |

The first listening pack is on car12 (`/storage/nes-pickleball/listen1/`, with a README of the questions).

