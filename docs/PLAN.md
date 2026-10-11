# The one-shot build plan (2026-10-10)

One autonomous session builds the ROM, its tools and tests, and the voice sidecar, committing at every gate
and sending screenshots as it goes; a second, shorter session with the maintainer at a handheld does the
listening. This file is the brief for that session. Research is in `RESEARCH_*.md`, the design in `DESIGN.md`.

## 0. The premise, corrected

Pickleball (1965) is older than the Famicom (1983). What is true is that nobody had heard of it: the USAPA and its
first rulebook date from 1984, the year of *Tennis*. So the cartridge is an alternate-history 1985 launch-window
title that plays the **1984 rules**: underhand volley serve only, one serve, let serves replayed, side-out scoring
to 11, two-bounce rule, 7-foot kitchen. (The 1984 rulebook was not found online; the rule-change history since is
documented, so the 1984 set is "today's rules minus the 2021-2026 changes".)

## 1. Goals and non-goals

Goals: a ROM that is fun to play and to watch, that an NES purist would believe is from 1985 (NROM-256, 1985
sprites and sound), that implements the sport's rules exactly, and that exposes a documented RAM mailbox so the
handheld can give the players voices. Non-goals for the one-shot: a tournament/career mode, four human players,
scrolling or large sprites, a sampled soundtrack, online anything, publishing (the repo stays private until the
maintainer flips it).

## 2. Deliverables

1. `build/pickleball.nes` (32 KB + 8 KB, mapper 0) with `sha256`, built by `make` from `src/`, `art/`, `tools/`.
2. `tools/`: `chr.py`, `court.py`, `dmc.py`, `check_rom.py`, `ramsyms.py` (Python 3, Pillow/numpy only).
3. `test/`: Lua tests for MesenCE's test runner, a rules oracle, `test/run.sh` (exit 0 = all green), screenshots.
4. `sidecar/`: `pickleball_voice.py` + the autogamer modules it reuses (`ra.py`, `pad.py`, `voice.py`, `talk.py`,
   copied with their notice), `phrases.py`, `cache.sh` (pre-synthesize the phrase WAVs), `run.sh` (handheld launch).
5. `docs/RAM_MAP.md` (generated), `docs/MANUAL.md` (1985 style), `docs/Findings_<date>.md` per session, a box-art
   PNG in `art/`.
6. Later, in autogamer's repo: a `pickleball-voice` game entry (a separate PR).

## 3. Hosts and set-up (day 0)

| step | host | who | command |
|---|---|---|---|
| cc65 | M1 | maintainer (sudo) or the agent from source into `~/.local` | `sudo dnf install cc65` |
| cc65 on the node (second build host) | etk-cloud | maintainer (sudo) | `sudo apt install cc65 make` |
| MesenCE test runner | etk-cloud | agent (needs the .NET 10 SDK, SDL2, clang via apt: maintainer's sudo once) | `git clone https://github.com/nesdev-org/MesenCE && make` → copy `Mesen` to the M1 as well |
| RetroArch visual check | M1 | agent | `retroarch -L /usr/lib64/libretro/nestopia_libretro.so build/pickleball.nes --appendconfig test/ra.cfg` |
| handheld period | car8 or car12 | maintainer grants a period; the agent deploys under `/storage/nes-pickleball/` | fceumm + llamatalk servers as autogamer starts them |

If MesenCE will not build or run headless on the node within an hour, fall back to **RetroArch on the M1 driven
by Python** (network commands + network remote, the autogamer way; `READ_CORE_MEMORY` must work with nestopia,
else build fceumm from source) for the same tests, slower and with less determinism; screenshots via `SCREENSHOT`.

## 4. Phases and gates

Each phase ends with a commit, a line in `docs/Findings_<date>.md`, and the evidence named. The order is chosen so
that something plays as early as possible and the riskiest unknowns (the toolchain, the test runner, the sprite
and CPU budgets) are hit first.

**Phase 0: skeleton (1-2 h).** Makefile, cc65 + neslib vendored at a pinned commit, `nrom_32k_vert.cfg`, NMI/OAM/
VRAM-buffer kernel, `chr.py` + `court.py` producing the court, a title screen and the court with one movable
player sprite. *Gate:* `make` on the M1 produces a 40,976-byte file that `check_rom.py` accepts; it boots in the
test runner (a Lua test waits 300 frames and reads a "booted" byte) and in RetroArch; a screenshot of the court
goes to the maintainer.

**Phase 1: ball, players, swing (3-4 h).** Physics (DESIGN §5), two players, the serve drop, the swing with timing
angle, the net, bounces, the shadow, in/out tables, the ball sprite priority at the net. *Gate:* Lua tests for the
tables and the physics invariants (a served ball lands in the diagonal court for a range of timings; a dink never
clears the baseline; the CoR); a scripted human rally of 10 hits against a mirror opponent; a screenshot mid-rally.

**Phase 2: the rules engine (3-4 h).** The state machine, scoring, serving sequence (singles and doubles, server
numbers, 0-0-2, parity positions), all faults, the umpire box, lets, game to 11/15/21, best of 3. *Gate:* the
**rules oracle**: a Lua (or Python-via-RetroArch) model of Sections 4-13 fed the same rally outcomes; 200 games of
random rally outcomes (through a debug "rally result" hook that exists only in test builds) must produce identical
score, server and position sequences; every fault type is provoked at least once by a scripted input test.

**Phase 3: computer players and exhibition (3-4 h).** DESIGN §7 for singles and doubles, the five levels, team
logic (ownership, switch, bounce-it), the attract mode. *Gate:* a 20-game CPU-vs-CPU soak per level pair on the
node with no hang and no fault storms (fewer than 1 in 10 points ends in a kitchen or two-bounce fault by the
computer); level 5 beats level 1 in at least 15 of 20; mean rally length 4-12 shots; the Mesen profiler shows
the worst frame under 80 % of the frame budget, else hot loops go to asm; three screenshots.

**Phase 4: modes, menu, HUD, sound (2-3 h).** Menu and options, 2-player input, pause, the score panel, the server
badge, the sound driver with the jingles and effects, the game-over screen. *Gate:* a Lua test drives every menu
path and plays one point in each mode; the ROM is under 22 KB of PRG so that Phase 6 fits; screenshots of every
screen; a WAV capture of the jingles from Mesen (the maintainer judges them later).

**Phase 5: the mailbox and the sidecar (3-4 h).** DESIGN §9-10. The mailbox, events, the serve gate, directives,
`RAM_MAP.md`; the sidecar with a stub voice on the M1 (prints and beeps), then real voices. *Gate:* on the M1, a
Lua test sets MB_CALL and checks the ROM waits and proceeds; the sidecar against RetroArch logs every event of
an exhibition game and the serve gate round-trips; on a handheld (the maintainer's period): one exhibition game
and one 1-player doubles game with voices, chatter and a directive by R1, WAVs kept, **the maintainer's verdict
on the voices** recorded in the findings.

**Phase 6: the cartridge's own voice (1-2 h, optional).** `dmc.py`, Pocket TTS words rendered on a handheld (or
any WAV), the DPCM word table, the score call from the ROM when no sidecar is present. *Gate:* ROM still 32 KB with
all words, the call plays in Mesen (a memory callback sees the DMC reads), the maintainer listens on a handheld.

**Phase 7: polish and packaging (1-2 h).** The manual, box art, release ROM + sha256, findings, the autogamer
integration brief. *Gate:* a clean clone builds byte-identical on the M1 and the node (`sha256` in the findings).

Total: roughly 18-25 hours of agent time in one session (Phases 0-4 and the M1 half of 5 need no one else),
plus one handheld session with the maintainer (an hour), plus the optional Phase 6.

## 5. The session's protocol

- Commit per gate on `main` (it is a private repo with one author): the message states the gate's evidence.
- Send the maintainer a screenshot at each gate (and any time the look changes), never a wall of logs.
- Hosts per AGENTS.md: builds on the M1, soaks on the node, nothing heavy on the laptop, ask before car12.
- When a gate cannot be reached in two attempts: record why in the findings, scale the feature down (a lower
  level count, fewer words, a simpler AI tier) and continue; never skip a gate silently.
- Keep *Tennis*'s assets out: nothing is copied from the ROM; the manual scan and the video were references for
  the look only.
- Identity: `mercurious` only; `Assisted-by: Claude Code (<model>)` trailer; no private strings.

## 6. Risks

| risk | mitigation |
|---|---|
| MesenCE does not build or needs a display on the node | the RetroArch + Python fallback (§3); `mesen-agent` headless fork |
| cc65 code too slow for four AI players at 60 fps | profile in Phase 3; AI every other frame per player; asm for the hot loops; the budget check is a gate |
| More than 256 unique court tiles | the court generator reports the count; straighten the perspective (fewer slopes) or drop the kitchen tint |
| The ROM outgrows 32 KB | DPCM words go first, then the best-of-3, then level 5's extras; `check_rom.py` reports free space at every build |
| The swing feels wrong (the one thing tests cannot judge) | copy *Tennis*'s known-good timing mechanics first (the lab measured them), tune only with the maintainer |
| Voices talk over each other or lag | the phrase cache; per-voice cooldowns; the serve gate's 90-frame timeout |
| RAM handshake races (the sidecar writes while the ROM reads) | single-byte flags only, written once; the ROM treats any unexpected value as "proceed" |
| nestopia on the M1 lacks a memory map | `READ_CORE_RAM` (achievement map) works with any core; else build fceumm |

## 7. Decisions for the maintainer (recommendations in bold)

1. Title: **PICKLEBALL** (generic; "Pickle-Ball" is a paddle company's trademark) or PICKLE-BALL for the 1972 look.
2. Copyright line on the title screen: **"© 2026 MERCURIOUS · 1985 EDITION"** or a plain "© 1985" as fiction.
3. Court palette: **two greens like *Tennis*** or 1984 asphalt grey with a green surround.
4. 2 PLAYERS SINGLES (not in *Tennis*): **in** (cheap, and a human contest).
5. The cartridge's own DPCM score call (Phase 6): **yes if it fits**; purists can be offered a build flag.
6. Best of 3 default: **one game to 11** by default, best of 3 as an option.
7. Licence: **MIT for code and tools**, art and ROM under a notice allowing free distribution (decide before
   anything is public). The sidecar reuses autogamer's MIT modules with their notice.
8. The autogamer integration: **after** the one-shot, as its own PR there.
9. Which handheld for the voice session and when: car8 is the free one; car12 has the 12 GB.

## 8. Day-0 checklist (run before the session)

```bash
sudo dnf install cc65
```

```bash
ssh etk-cloud 'sudo apt-get install -y cc65 make clang libsdl2-dev dotnet-sdk-10.0 || true; cc65 --version'
```

(If `dotnet-sdk-10.0` is not in Ubuntu's repos, the Microsoft package feed or the `dotnet-install.sh` script
provides it; the agent can do that part without sudo in `~/.dotnet`.)

Then start the session with: "Build NES Pickleball per docs/PLAN.md, phases 0-5 (M1 half), commit per gate,
screenshots at each gate, stop before the handheld session and before Phase 6; here are the decisions: ...".
