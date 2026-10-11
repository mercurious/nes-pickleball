# Design: NES Pickleball (2026-10-10, before the build)

The game *Tennis* (1984) would have been if its sport had been pickleball: the same camera, the same two-button
swing, the same small sprites and umpire chair, with pickleball's court, rules and rhythm, which are different
enough to make a different game: a short court, one serve, side-out scoring with a server number, the two-bounce
rule, and the kitchen, where the point is usually won softly rather than with a smash.

## 1. Presentation and fiction

- Title screen: **PICKLEBALL** in a 1985 block font, a court in the background, the menu below, the copyright
  line to be decided (see PLAN.md). No Nintendo marks, no Mario: our own umpire in the chair.
- Black-box era look: a 256 x 240 single screen, no scrolling, two court tones with white lines, a plain
  surround, players as 16 x 24 sprites in team colours, the opponents' shirts coloured by level as in *Tennis*.
- Sound: a short title jingle, point and side-out stings, a game-over fanfare, paddle and bounce effects, noise
  applause, and (if the ROM has room) the digitized score call. Every word and jingle must be plausible for a 1985
  cartridge: nothing sampled from a real voice except what the DPCM channel can carry.
- The manual (`docs/MANUAL.md`) in the 1985 Nintendo manual's voice: "Object of the game", "Names of controller
  parts", "How to play", "Pickleball terminology" (the kitchen, the two-bounce rule, side out).

## 2. Modes

| mode | who | notes |
|---|---|---|
| 1 PLAYER SINGLES | you vs the computer | *Tennis*'s first mode |
| 1 PLAYER DOUBLES | you + a computer partner vs a computer pair | new; the partner takes directives (section 10) |
| 2 PLAYERS DOUBLES | two controllers as a team vs a computer pair | *Tennis*'s second mode |
| 2 PLAYERS SINGLES | controller 1 vs controller 2 | cheap once pad 2 is read; *Tennis* lacked it; decision for the maintainer |
| EXHIBITION | computer vs computer | the attract mode, and the "watchable contest" for autogamer; chatter on |

Options on the menu: LEVEL 1-5 (opponents), GAME TO 11 / 15 / 21, ONE GAME / BEST OF 3, LETS: REPLAY (1984) /
PLAY (modern). SELECT moves the asterisk as in *Tennis*; START begins. START during play pauses with the pause
tone.

## 3. Controls

| input | effect |
|---|---|
| pad | move; up is toward the net |
| **A** | hard shot: drive, volley, smash on a high ball; **the serve** (press when the dropped ball is at hip height) |
| **B** | soft shot: dink from the line, drop from the back; with Down held, a lob |
| START | pause; on the menu, begin |
| SELECT | menu cursor; on the pre-serve screen, ask the umpire for the score (shows it again) |

Shot direction follows *Tennis*: where the swing meets the ball (early/late, and the stand-off from the ball)
sets the angle; the pad never steers the ball. Reach windows per stroke (forehand/backhand) are tuned so that a
player standing still returns a ball hit straight at them.

## 4. Screen and court geometry (starting numbers, tuned in the build)

```
y  56   HUD: score panel top-left, umpire chair sprite at the right net post
y  72   far baseline      x  72 .. 184  (112 px wide)
y 116   far kitchen line
y 136   net band (8 px tall, with the centre dip drawn 1 px lower)
y 156   near kitchen line
y 200   near baseline     x  40 .. 216  (176 px wide)
```

- 44 ft -> 128 px (2.9 px/ft) vertically; sidelines are straight lines: `left(y) = 72 - (y - 72) / 4`,
  `right(y) = 184 + (y - 72) / 4`, kept as 2 tables of 128 bytes. The centerline is x = 128 between kitchen line
  and baseline on both ends. The kitchen is tinted one shade.
- Players move on the ground plane in screen pixels (x 24..232, y 60..228, so they can stand behind the baseline
  and outside the sidelines: the serving areas). Sprites are not scaled by depth (*Tennis* did not either).
- The ball has a ground position (x, y) and a height z; the shadow is drawn at (x, y), the ball at (x, y - z/2).

## 5. Physics (8.8 fixed point, per frame)

- Gravity on z; `vz *= -0.64` at a bounce (the CoR of a pickleball), `vx, vy *= 0.98` in the air (the wiffle ball
  slows); the ball dies after the second bounce or when `z` stays under 1 for 10 frames.
- The net: when the ball's y crosses 136, if `z < net_height(x)` (10 px at the sides, 9 at the centre) it is a net
  ball; a serve within 1 px of the top is a LET (replayed when LETS: REPLAY).
- In/out at the first bounce: `x` against `left(y)` / `right(y)` and `y` against 72 / 200, inclusive (lines are
  in); for a serve, the kitchen line is out and the landing must be in the diagonal service court.
- Shots are a table: (shot type, ball height band at contact) -> (speed, lift, lateral range), plus the timing
  angle. Drives ~3 px/frame flat, serves ~2 px/frame lifted, drops ~1.2 px/frame high and short, dinks ~1 px/frame
  from the line, lobs slow and very high. The computer adds level-based noise; a human's shot is exact.
- The swing: a stroke animation of 8 frames with a 3-frame contact window; contact needs the ball inside the
  reach box (x stand-off 4-16 px on the forehand side or 8-18 px on the backhand side, |y - feet| <= reach) and z
  under the paddle's height for the shot (a smash needs z high, a dink z low).

## 6. The rules engine (a state machine, one byte of state in RAM)

```
TITLE -> MENU -> PRE_SERVE -> CALL -> SERVE_DROP -> SERVE_FLIGHT -> RALLY -> DEAD (call shown) -> PRE_SERVE ...
                                                                              -> SIDE_OUT -> PRE_SERVE
                                                                              -> GAME_OVER -> (next game | TITLE)
```

- PRE_SERVE places everyone: server and receiver by the score parity (singles) or the team's score and starting
  server (doubles, 5.B.3); partners at their correct side; the receiving team's non-receiver at the kitchen line.
- CALL: the HUD shows the call ("4-2-1"), the server number badge appears over the server, and the game waits:
  for the ROM's own voice (DPCM words, if present), for the sidecar's voice (mailbox, section 9), or 90 frames.
  Serving before the call ends is impossible rather than a fault (the button is ignored).
- SERVE_DROP: the ball is released from the server's hand; A within the window hits it underhand; too late and
  the ball bounces: FAULT (a 1984 server does not get a drop serve). The computer serves after a level-based delay
  under the 10-second rule.
- RALLY tracks `bounces_needed` (2 after the serve: the receiver's and the server's side's first hits must follow a
  bounce), `last_hitter`, `bounces_this_side`, and each player's `in_kitchen` and `volley_momentum` timer (10
  frames after a volley during which stepping into the kitchen is a fault).
- Faults named by the umpire: OUT, NET, LET (replay), SHORT (serve in the kitchen), WRONG COURT, FOOT FAULT, TWO
  BOUNCE, DOUBLE BOUNCE, KITCHEN, BODY, NET TOUCH (a player sprite overlapping the net band while the ball is
  live). Each fault is a DEAD state with a 60-90 frame display, the sting, and the mailbox event.
- Scoring: side-out only; `score[2]`, `server_num` (1/2), `serving_team`, `starting_server[2]` (which player of
  each team is the "starting server" for parity), `game_to`, `games[2]` for best-of-3; the win check is "reached
  game_to and leads by 2".

## 7. The computer players

One routine per computer player, run every frame, with level parameters (reaction delay 0-20 frames, speed 1.0-2.0
px/frame, error noise, and a strategy tier):

- **Positioning**: a target spot by role and state: serving team stays back until the return bounces (the
  two-bounce rule), then the hitter of the third shot follows a drop forward; receiver's partner starts at the
  kitchen line; at the line, cover the middle toward the ball; after a lob, retreat and **switch** with the
  partner; back to home spots between points.
- **Shot choice**: level 1-2 push the ball back over the net (B shots) and rarely come in; 3 drops the third shot
  and dinks; 4 speeds up any ball above net height (A) and lobs now and then; 5 also poaches in doubles and goes
  for the body. Error: a per-level chance to misjudge depth (long) or hit the net on a hard shot.
- **Who takes it** in doubles: the player whose forehand side the ball is on, else the nearer; the decision is
  made when the ball crosses the net and is sticky (no two-player hits, 10.D.2); it raises the MINE/YOURS event.
- **A ball going out**: if the predicted landing is beyond the baseline by a margin, the non-hitter raises
  BOUNCE_IT and the hitter lets it go (a level 1 player still swings).
- Directives from the mailbox (section 10) override the choice of side, depth and ownership.

## 8. HUD and umpire

- Top-left panel: `TEAM A 4   TEAM B 2   SERVER 1`, the serving side highlighted; best-of-3 shows games won.
- A small `1` or `2` badge sprite floats over the server during CALL and SERVE_DROP (the video's overlay, as a
  1985 sprite); a tiny open-palm/fist sprite over a computer partner shows a planned switch/stay (doubles).
- The umpire's chair at the right net post; its speech box (2 tiles tall, top-centre) shows FAULT / OUT / LET /
  SIDE OUT / POINT / GAME, replacing *Tennis*'s Mario calls.

## 9. The RAM map and the mailbox (the ROM's public API)

All in internal RAM; documented in `docs/RAM_MAP.md` with every address, so a sidecar or a test never
reverse-engineers anything. The **mailbox** is the last 16 bytes of RAM:

| addr | name | meaning |
|---|---|---|
| $07F0 | MB_MAGIC | $5B when the game is running (sidecars check it) |
| $07F1 | MB_VERSION | mailbox version (1) |
| $07F2 | MB_STATE | the engine state (section 6 enum) |
| $07F3 | MB_SCORE_A, $07F4 MB_SCORE_B | the two scores |
| $07F5 | MB_SERVER | bit 0 serving team, bits 1-2 server number (1/2), bits 3-4 the serving player's slot 0-3 |
| $07F6 | MB_MODE | mode and level: bits 0-2 mode, bits 4-6 level |
| $07F7 | MB_EVENT_N | increments on every event (a sidecar polls this) |
| $07F8 | MB_EVENT | the last event code |
| $07F9 | MB_EVENT_ARG | the slot (0-3) or team the event concerns |
| $07FA | MB_CALL | the serve gate: ROM writes 1 = "call the score now"; a sidecar writes 2 = "said"; 0 = idle. The ROM proceeds on 2 or after 90 frames; a sidecar writes $A5 here at startup to announce itself, which also mutes the ROM's DPCM call |
| $07FB | MB_DIRECTIVE | partner directive from the sidecar: 0 auto, 1 I take the middle, 2 you take the middle, 3 stay back, 4 come up, 5 switch sides now, 6 let me serve/return (handled where legal) |
| $07FC | MB_CHATTER | bit 0 chatter wanted, bit 1 human calls the score |
| $07FD-$07FF | reserved | |

Events (`MB_EVENT`): SCORE_CALL (the call text is derivable from the scores), SERVE, RETURN, THIRD_SHOT, DINK,
DRIVE, LOB, VOLLEY, SMASH, BOUNCE_IT (the non-hitter thinks the ball is going long), MINE, YOURS, SWITCH,
POINT_WON (arg team), FAULT_OUT, FAULT_NET, FAULT_LET, FAULT_SHORT, FAULT_WRONG_COURT, FAULT_FOOT,
FAULT_TWO_BOUNCE, FAULT_DOUBLE_BOUNCE, FAULT_KITCHEN, FAULT_BODY, FAULT_NET_TOUCH, SIDE_OUT, SECOND_SERVER,
GAME_OVER (arg winner), NICE_SHOT (a winner that the opponent never reached), LONG_RALLY (every 10 shots).

The other documented locations: player x/y/z-state per slot, ball x/y/z and velocities, bounces_needed,
last_hitter, the frame counter, RNG state, menu cursor. Tests and the sidecar include `docs/RAM_MAP.md` by
address constants generated from the build's symbol file (`tools/ramsyms.py` from `ld65 --dbgfile`).

## 10. The voice layer (sidecar, on the handheld)

```
RetroArch (fceumm, pickleball.nes)  <-UDP 55355->  sidecar/pickleball_voice.py  --->  pacat (speaker)
      ^ network remote 55400 (optional: the sidecar can also be a player)      |
      | mailbox, RAM map                                                       +-- Pocket TTS (tts-server 8093): phrase cache + live lines
                                                                               +-- Gemma 4 E2B (llama-server): colour lines between points
                                                                               +-- Qwen3-ASR (llama-server 8091): hold R1, talk to your partner
```

- **Voices**: one Pocket TTS voice per player slot (four distinct voices from the English pack; `alba` and `jean`
  are known good) and one for the umpire. Each player's lines are rendered by their voice.
- **Latency**: the score call and all chatter words are **pre-synthesized** at install (or first run) into a WAV
  cache per voice (digits 0-21, "two", "side out", "fault", "mine", "yours", "switch", "bounce it", "out",
  "nice", "sorry", ...), so a call plays within a frame of the event. Live synthesis (2x real time) is only for
  the model's lines.
- **The score call**: on MB_CALL = 1 the sidecar reads the scores and the server, plays "four – two – one" in the
  server's voice, then writes 2. A human server's call is spoken by the human's player voice unless MB_CHATTER bit
  1 is set, in which case the human says it (hold R1): the ASR hears it, a right call releases the serve, a wrong
  one makes the opponents challenge ("wrong score!") and the umpire re-call it (6.F.1), a stretch feature.
- **Chatter**: events -> short phrases with per-voice cooldowns so the court is not a chorus: MINE/YOURS on the
  owner decision, BOUNCE_IT, SWITCH, "nice" after NICE_SHOT (from the opponent too, at level 5 never), "sorry" after
  an own fault, "side out", "second server". Volume ducks under the model's lines.
- **The model**: between points only (E2B, ≤ 12 words, the facts given, the faithfulness guard from autogamer's
  `narrate.py`: the line must name the right winner and never claim a game that is not over); it answers the
  human's remarks (ASR text) in the partner's voice, 5-9 s later; a canned acknowledgement ("got it") comes at
  once when a directive is recognised.
- **Talking to your partner** (1 PLAYER DOUBLES; hold R1): "I've got the middle" / "you take the middle" ->
  directive 1/2; "stay back", "hang back" -> 3; "come up", "get to the line" -> 4; "switch" -> 5; "your call" ->
  0. Anything else goes to the model. The partner confirms out loud. Without a microphone the mode still plays;
  the partner just decides for itself.
- **Standalone**: no sidecar, no change: the ROM waits 90 frames at CALL (or plays its own DPCM call) and the
  computer partner plays by its own rules.
- **autogamer**: a game entry (`pickleball-voice`) that bundles the ROM, launches RetroArch with the usual
  `--appendconfig` and the sidecar, and a level picker; the first entry that needs no ROM from the player. The
  integration PR lives in autogamer's repo; this repo ships `sidecar/` and the ROM.

## 11. Memory budget

| resource | budget | plan |
|---|---|---|
| PRG 32 KB | code 12-16 KB, tables 3-5 KB, DPCM 9-11 KB, vectors | `tools/check_rom.py` prints free bytes; DPCM words are cut first |
| CHR 8 KB | BG: court tiles (≤ 150) + font (64) + HUD/umpire (30); SPR: 4 player poses x 2 sides x 2 teams as palettes, ball, shadow, badges | palettes, not tiles, distinguish teams |
| RAM 2 KB | $00-$FF zero page (hot vars), $100 stack, $200 OAM, $300-$3FF VRAM buffer, $400-$7EF game state, $7F0-$7FF mailbox | |
| per scanline | 8 sprites | 6 worst case at the kitchen line |
| CPU per frame | ~29,000 cycles | 4 AI players + physics + collision in C: measured with Mesen's profiler in Phase 3; move hot loops to asm if over 70 % |

## 12. Open decisions (for the maintainer; PLAN.md lists them with recommendations)

Title spelling and the copyright line; the court palette (two greens vs asphalt); 2 PLAYERS SINGLES in or out;
the in-ROM DPCM voice (purist vs feature); best-of-3 default; licence.
