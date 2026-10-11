# The build plan (2026-10-10, revised the same evening)

**Status 2026-10-11:** Release 1 (v0.0.1, the cartridge) is built; the phases' gates and what each one taught are in
`Findings_20261011.md`. Release 2 (the teammate add-on) has not started.

Two releases. **v0.0.1 is the cartridge**: a ROM that plays on every NES emulator, on a flash cart and on the
Flip 2 as a plain NES game, built in one autonomous session. **v0.0.2 is the teammate add-on**: on ROCKNIX with
llamatalk, your doubles partner talks, calls the score, and you talk back while you play, the way autogamer's
Contra co-op works. v0.0.1 is built *for* the add-on (the interface is in the ROM from day one and tested there)
but ships without it, and the add-on is built only after the core game is validated. Research is in
`RESEARCH_*.md`, the design in `DESIGN.md`.

## 0. The premise, corrected

Pickleball (1965) is older than the Famicom (1983). What is true is that nobody had heard of it: the USAPA and its
first rulebook date from 1984, the year of *Tennis*. So the cartridge is an alternate-history 1985 launch-window
title that plays the **1984 rules**: underhand volley serve only, one serve, let serves replayed, side-out scoring
to 11, two-bounce rule, 7-foot kitchen. (The 1984 rulebook was not found online; the rule-change history since is
documented, so the 1984 set is "today's rules minus the 2021-2026 changes".)

## 1. The releases

| version | what it is | runs on | validated by |
|---|---|---|---|
| **v0.0.1 the cartridge** | the ROM (NROM-256), tools, tests, manual; mailbox v1 and the RAM map built in and tested with a fake sidecar | any NES emulator, a flash cart, the Flip 2 under ES-DE as a plain ROM | the session's gates, then the maintainer playing it on an emulator and on the Flip 2 |
| **v0.0.2 the teammate add-on** | `sidecar/`: Pocket TTS voices, the score call before every serve, court chatter, hold R1 and talk to your partner, the model's calls between points; packaged as an autogamer game entry | ROCKNIX handhelds with llamatalk (the Flip 2) | a second session, then a listening session with the maintainer |
| later | the cartridge's own DPCM voice if it is not in 0.0.1; "the model plays" modes for autogamer (singles, exhibition commentary) | | |

The ROM does not change between 0.0.1 and 0.0.2: the add-on is validated against the frozen mailbox v1. If the
add-on needs something new, that is a ROM release (0.0.3) with `MB_VERSION` bumped and the old behaviour kept.

## 2. Goals and non-goals

v0.0.1: a ROM that is fun to play and to watch, that an NES purist would believe is from 1985, that implements
the sport's rules exactly, and whose state is readable and steerable from outside through a documented contract.
v0.0.2: the Contra co-op feeling on a court: a partner with a voice and opinions, who takes your orders and
answers you, with no change to how the game plays. Non-goals: a career mode, four human players, scrolling or
large sprites, a sampled soundtrack, online anything, publishing (private until the maintainer flips it).

## 3. Deliverables

v0.0.1:
1. `build/pickleball.nes` (32 KB + 8 KB, mapper 0) with `sha256`, from `make` over `src/`, `art/`, `tools/`.
2. `tools/`: `chr.py`, `court.py`, `dmc.py`, `check_rom.py`, `ramsyms.py` (Python 3, Pillow/numpy only).
3. `test/`: Lua tests for MesenCE's test runner, the rules oracle, the fake-sidecar test, `test/run.sh`, screenshots.
4. `sidecar/stub.py`: a desktop sidecar with a printed "voice" (RetroArch network commands, no models) that
   exercises the whole contract on the M1; the base the add-on grows from.
5. `docs/RAM_MAP.md` (generated), `docs/MAILBOX.md` (the contract, versioned), `docs/MANUAL.md` (1985 style),
   `docs/Findings_<date>.md`, box art in `art/`; a GitHub release `v0.0.1` with the ROM.

v0.0.2:
6. `sidecar/pickleball_coop.py` and friends (`phrases.py`, `calls.py`, `cache.sh`, `run.sh`), reusing autogamer's
   `ra.py`, `pad.py`, `voice.py`, `talk.py`, `llm.py` with their notice; a `state/pickleball_record.json` like
   Contra's.
7. The autogamer game entry `pickleball-coop` (a PR in autogamer's repo) that bundles the ROM, so it is the first
   entry that needs no dump from the player; a GitHub release `v0.0.2`.

## 4. Built for the add-on: the v0.0.1 contract

What v0.0.1 must contain so that v0.0.2 is an add-on and not a ROM change (details in DESIGN.md §9-10):

- **Mailbox v1** at $07F0-$07FF with a magic byte and a version byte; the state, scores, server and mode mirrored
  there every frame; an event counter and the last event with its argument; the **serve gate** (`MB_CALL`: the ROM
  asks for the call, a sidecar answers, a 90-frame timeout keeps the game moving alone); the **partner directive**
  byte (unknown values mean "auto"); a **speech bubble** byte the ROM displays over a player for 45 frames (so the
  add-on is visible on screen and testable without audio); a sidecar-present byte that mutes the ROM's own voice.
- **The RAM map** generated from the linker's symbols into `docs/RAM_MAP.md`, with every byte a sidecar or a reflex
  player needs: ball ground x/y, height and velocities, each player's feet, state and swing, bounces needed, last
  hitter, the frame counter. Addresses are fixed within a release and listed with their version.
- **The event vocabulary** (DESIGN.md §9) fixed in v1, with reserved codes for later.
- **2 PLAYERS DOUBLES drives the partner from controller 2 completely** (no hidden assist), so a reflex player on
  RetroArch's network remote (user 2) can be the partner's body if the add-on prefers the Contra way.
- **1 PLAYER DOUBLES obeys directives** from the mailbox exactly as it obeys its own decisions: ownership of the
  middle, depth (stay back / come up), switch, style (patient / aggressive).
- **Robustness**: the ROM never waits on the mailbox except at the serve gate, never trusts a value outside its
  range, and plays identically when nothing writes to it.
- **Tested in v0.0.1**: a Lua fake sidecar (announces itself, answers serve gates, writes directives and bubbles,
  checks the ROM's reactions) and `sidecar/stub.py` against RetroArch on the M1 (the UDP path, the same
  reads and writes the handheld will do).

## 5. Hosts and set-up (day 0)

| step | host | who | command |
|---|---|---|---|
| cc65 | M1 | maintainer (sudo) or the agent from source into `~/.local` | `sudo dnf install cc65` |
| cc65 on the node (second build host) | etk-cloud | maintainer (sudo) | `sudo apt install cc65 make` |
| MesenCE test runner | etk-cloud | agent (needs .NET 10 SDK, SDL2, clang via apt: maintainer's sudo once) | `git clone https://github.com/nesdev-org/MesenCE && make` → copy `Mesen` to the M1 as well |
| RetroArch visual check and the stub sidecar | M1 | agent | `retroarch -L /usr/lib64/libretro/nestopia_libretro.so build/pickleball.nes --appendconfig test/ra.cfg` |
| handheld period (v0.0.2) | car8 or car12 | maintainer grants a period; the agent deploys under `/storage/nes-pickleball/` | fceumm + llamatalk servers as autogamer starts them |

If MesenCE will not build or run headless on the node within an hour, fall back to **RetroArch on the M1 driven by
Python** (network commands + network remote, the autogamer way; `READ_CORE_MEMORY` must work with nestopia, else
`READ_CORE_RAM`, else build fceumm from source) for the same tests, slower and less deterministic.

## 6. Phases and gates

Each phase ends with a commit, a line in `docs/Findings_<date>.md`, and the evidence named. The order gets
something playing early and hits the riskiest unknowns first (toolchain, test runner, sprite and CPU budgets).

### Release 1: v0.0.1, the cartridge (one session, roughly 16-22 hours of agent time)

**Phase 0: skeleton (1-2 h).** Makefile, cc65 + neslib vendored at a pinned commit, `nrom_32k_vert.cfg`, the
NMI/OAM/VRAM-buffer kernel, `chr.py` + `court.py` producing the court, a title screen and the court with one
movable player. *Gate:* `make` on the M1 produces a 40,976-byte file that `check_rom.py` accepts; it boots in the
test runner (a Lua test waits 300 frames and reads a "booted" byte) and in RetroArch; a screenshot to the maintainer.

**Phase 1: ball, players, swing (3-4 h).** Physics (DESIGN §5), two players, the serve drop, the swing with timing
angle, the net, bounces, the shadow, in/out tables, sprite priority at the net. *Gate:* Lua tests for the tables
and invariants (a served ball lands in the diagonal court across the timing window; a dink never clears the
baseline; the CoR); a scripted human rally of 10 hits against a mirror opponent; a screenshot mid-rally.

**Phase 2: the rules engine (3-4 h).** The state machine, scoring, serving sequence (singles and doubles, server
numbers, 0-0-2, parity positions), every fault, the umpire box, lets, game to 11/15/21, best of 3. *Gate:* the
**rules oracle**: a Lua model of Sections 4-13 fed the same rally outcomes through a test-build-only "rally result"
hook; 200 games of random outcomes must give identical score, server and position sequences; every fault type is
provoked at least once by a scripted-input test.

**Phase 3: computer players and exhibition (3-4 h).** DESIGN §7 for singles and doubles, five levels, team logic
(ownership, switch, bounce-it), the attract mode. *Gate:* a 20-game CPU-vs-CPU soak per level pair on the node with
no hang and no fault storms (under 1 in 10 points ends in a computer's kitchen or two-bounce fault); level 5 beats
level 1 in at least 15 of 20; mean rally length 4-12 shots; the worst frame under 80 % of the budget in Mesen's
profiler, else hot loops move to asm; three screenshots.

**Phase 4: modes, menu, HUD, sound (2-3 h).** Menu and options, controller 2, pause, the score panel, the server
badge, the sound driver with jingles and effects, the game-over screen. *Gate:* a Lua test drives every menu path
and plays one point in each mode; PRG under 22 KB so Phase 6 can fit; screenshots of every screen; WAV captures of
the jingles for the maintainer.

**Phase 5: the contract (2-3 h).** Mailbox v1, events, the serve gate, directives, bubbles, `RAM_MAP.md`,
`MAILBOX.md`; the Lua fake sidecar; `sidecar/stub.py` on the M1. *Gate:* the fake-sidecar test passes (the ROM
waits at the gate, proceeds on the answer and on timeout, obeys each directive in a staged situation, shows
bubbles); the stub logs every event of an exhibition game through RetroArch and round-trips the gate; a
screenshot with a bubble over the partner.

**Phase 6: the cartridge's own voice (1-2 h, only if PRG room remains).** `dmc.py`, words rendered with Pocket TTS
on a handheld (or any WAV), the DPCM word table, the score call from the ROM when no sidecar is present. *Gate:* ROM
still 32 KB with all words; a memory callback in Mesen sees the DMC reads during the call. Otherwise it moves to a
later ROM release.

**Phase 7: packaging v0.0.1 (1-2 h).** The manual, box art, findings, `v0.0.1` tag and GitHub release with the ROM
and `sha256`. *Gate:* a clean clone builds byte-identical on the M1 and the node; the ROM also starts on the Flip 2
from `/storage/roms/nes/` as a plain game (the maintainer or a quick ssh check).

**Validation before Release 2 (the maintainer):** play singles and doubles on an emulator and on the Flip 2; the
swing feel, the AI levels and the rules calls are judged by a human. Findings go into a 0.0.1.x fix cycle if needed.

### Release 2: v0.0.2, the teammate add-on (a second session, roughly 8-12 hours, plus the listening session)

**Phase 8: voices on ROCKNIX (3-4 h).** `sidecar/pickleball_coop.py` from the stub: Pocket TTS phrase cache per
voice (`cache.sh`), one voice per slot and one for the umpire, the score call through the serve gate in the
server's voice, chatter from events with per-voice cooldowns, the umpire's calls, `SHOW_MSG` captions, volume
ducking. *Gate:* an exhibition game on a handheld with every point called and the chatter in character; WAVs kept;
no serve waits past the 90-frame timeout (the call is faster than the gate).

**Phase 9: the teammate (3-4 h).** DESIGN §10: hold R1 and talk (autogamer's `talk.py`, Qwen3-ASR); the phrase map
to directives with a spoken acknowledgement; everything else to the model with the point's facts; the model's own
calls between points (E4B if installed, else E2B) with the per-call record kept across games; "your call" hands
control back. *Gate:* a 1 PLAYER DOUBLES game with the maintainer: five orders obeyed and acknowledged, two
free-form remarks answered in the partner's voice within 10 s, the record file written; the maintainer's verdict on
the voices and the timing recorded.

**Phase 10: packaging v0.0.2 (1-2 h).** The autogamer entry `pickleball-coop` (bundled ROM, `--appendconfig`, the
level picker is not needed: real time), the Ports launch, `v0.0.2` release. *Gate:* installed from Ports on a
handheld and started with the gamepad only.

## 7. The session's protocol

- Commit per gate on `main` (a private repo with one author); the message states the gate's evidence.
- Send the maintainer a screenshot at each gate (and whenever the look changes), never a wall of logs.
- Hosts per AGENTS.md: builds on the M1, soaks on the node, nothing heavy on the laptop, ask before car12.
- When a gate cannot be reached in two attempts: record why in the findings, scale the feature down (fewer levels,
  fewer words, a simpler AI tier) and continue; never skip a gate silently.
- Keep *Tennis*'s assets out: nothing is copied from the ROM; the manual scan and the video were references only.
- Identity: `mercurious` only; `Assisted-by: Claude Code (<model>)` trailer; no private strings.

## 8. Risks

| risk | mitigation |
|---|---|
| MesenCE does not build or needs a display on the node | the RetroArch + Python fallback (§5); `mesen-agent` headless fork |
| cc65 code too slow for four AI players at 60 fps | profile in Phase 3; AI every other frame per player; asm for hot loops; a gate |
| More than 256 unique court tiles | the generator reports the count; straighten the perspective or drop the kitchen tint |
| The ROM outgrows 32 KB | DPCM words go first, then best-of-3, then level 5's extras; `check_rom.py` reports free space |
| The swing feels wrong (what tests cannot judge) | copy *Tennis*'s measured timing mechanics first; tune only with the maintainer |
| The add-on wants a ROM change after all | the contract is tested by a fake sidecar in 0.0.1; reserved bytes and event codes; a versioned 0.0.3 if needed |
| Voices talk over each other or lag | the phrase cache; per-voice cooldowns; the serve gate's timeout |
| RAM handshake races | single-byte flags written once; any unexpected value means "proceed" |
| nestopia on the M1 lacks a memory map | `READ_CORE_RAM` works with any core; else build fceumm |

## 9. Decisions for the maintainer (recommendations in bold)

1. Title: **PICKLEBALL** (generic; "Pickle-Ball" is a paddle company's trademark) or PICKLE-BALL for the 1972 look.
2. Copyright line on the title screen: **"© 2026 MERCURIOUS · 1985 EDITION"** or a plain "© 1985" as fiction.
3. Court palette: **two greens like *Tennis*** or 1984 asphalt grey with a green surround.
4. 2 PLAYERS SINGLES (not in *Tennis*): **in** (cheap, and a human contest).
5. The cartridge's own DPCM score call (Phase 6): **yes if it fits**; purists can be offered a build flag.
6. Best of 3 default: **one game to 11** by default, best of 3 as an option.
7. Licence: **MIT for code and tools**, art and ROM under a notice allowing free distribution (before anything is
   public). The sidecar reuses autogamer's MIT modules with their notice.
8. The partner's body in v0.0.2: **the in-ROM partner steered by directives** (60 fps, no UDP timing), with the
   Contra-style reflex player on controller 2 as the alternative if the maintainer prefers the model's partner to be
   entirely outside the ROM.
9. Which handheld for the v0.0.2 session and when: car8 is the free one; car12 has the 12 GB.

## 10. Day-0 checklist (run before the session)

```bash
sudo dnf install cc65
```

```bash
ssh etk-cloud 'sudo apt-get install -y cc65 make clang libsdl2-dev dotnet-sdk-10.0 || true; cc65 --version'
```

(If `dotnet-sdk-10.0` is not in Ubuntu's repos, Microsoft's package feed or `dotnet-install.sh` provides it; the
agent can do that part without sudo in `~/.dotnet`.)

Then start the session with: "Build NES Pickleball v0.0.1 per docs/PLAN.md, phases 0-7, commit per gate,
screenshots at each gate; decisions: ...". v0.0.2 starts after the maintainer has played 0.0.1.
