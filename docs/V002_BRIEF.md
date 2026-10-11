# v0.0.2 brief: the teammate add-on (not started)

What the next session builds on top of the frozen v0.0.1 cartridge, in this repo's `sidecar/` and then as an
autogamer game entry. Nothing here changes the ROM; everything programs against `docs/MAILBOX.md` and
`docs/RAM_MAP.md`.

## What exists

- `sidecar/stub.py`: announces itself (`MB_CALL = $A5`), answers every score call through the serve gate (reads
  the scores and the server from the mailbox, "speaks" the call in the server's slot voice, writes 2), turns
  events into lines, sends speech bubbles, takes a directive from a file. On the desktop it reads with
  `READ_CORE_RAM` (nestopia); on the handheld `--mem` switches to `READ_CORE_MEMORY` (fceumm).
- `sidecar/ra.py` and `sidecar/pad.py`: RetroArch's command and remote-pad interfaces (from autogamer).
- The ROM on car8 at `/storage/roms/nes/Pickleball.nes`.

## What to build (DESIGN.md §10, PLAN.md Phases 8-10)

1. **Voices**: Pocket TTS through llamatalk's `tts-server` (port 8093), one voice per slot and one for the
   umpire; a phrase cache rendered at install (`cache.sh`): digits 0-21, "two", "side out", "second server",
   "fault", "mine", "yours", "switch", "bounce it", "out", "nice", "sorry", "got it", "kitchen", "let it
   bounce"; `pacat` playback with the game's volume ducked (autogamer's `voice.py`, `talk.py` do this).
2. **The score call**: the stub's logic with the cached WAVs; well under the 240-frame gate.
3. **Chatter**: events to lines with per-voice cooldowns (MINE/YOURS/BOUNCE_IT/SWITCH as they happen, NICE after
   a winner, SORRY after an own fault, SIDE OUT, SECOND SERVER); the umpire's calls; `SHOW_MSG` captions and
   the ROM's bubbles.
4. **The partner's calls**: between points, Gemma 4 (E4B if installed, else E2B) gets the score, who serves, how
   the last point ended, the rally length, the shots, and the record of each call in this situation; it answers
   one call (MIDDLE/YOURS, UP/BACK, SWITCH/STAY, AGGRESSIVE/PATIENT) that becomes `MB_DIRECTIVE` (1/2, 4/3, 5,
   7/6) and a spoken line in the partner's voice; the record goes to `state/pickleball_record.json` as in
   Contra's `contra_record.json`.
5. **Talking to the partner**: hold R1 (autogamer's `talk.py`, Qwen3-ASR on 8091): the phrase map ("I've got the
   middle", "you take the middle", "stay back", "come up", "switch", "be patient", "attack", "hold up"/"go",
   "your call") to directives with a spoken acknowledgement; anything else to the model with the point's facts,
   answered in a few words. Autogamer's faithfulness guard for every model line.
6. **Packaging**: an autogamer game entry `pickleball-coop` (mode 1 PLAYER DOUBLES; the ROM bundled with the app,
   so `games.py` needs a "bundled ROM" path next to the CRC search; RetroArch with `--appendconfig
   autogamer.cfg`; real time, no level picker; the level from a menu line), and an exhibition entry
   (`pickleball-watch`: four voices and the umpire). Both start the sidecar next to RetroArch as `run.sh` does for
   the other games.

## Gotchas known from v0.0.1

- The mailbox event byte holds only the last event; a sidecar polling at 20 Hz misses events that come in the same
  60th of a second (POINT_WON is followed by SCORE_CALL in the same frame). Poll at 50-60 Hz, and treat
  SCORE_CALL as the point's end for the score (the scores in the mailbox are already updated).
- RetroArch's network remote drops taps for about 2.5 s after a launch; the UDP replies to a memory read must be
  matched by address (`ra.py` drains late replies).
- The ROM's own DPCM voice stays silent once a sidecar has announced; if the sidecar dies mid-match the ROM waits
  240 frames per serve until the next power-on (a restart of the sidecar re-announces and answers again).
