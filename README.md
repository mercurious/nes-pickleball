# NES Pickleball

A homebrew NES cartridge: pickleball, built as the sports cartridge Nintendo of America could have shipped in
the 1985 launch window, in the mould of *Tennis* (Famicom 1984, NES 1985). The sport is older than the console
(Bainbridge Island, 1965) and had just got its first rulebook (the USAPA's, March 1984), but nobody outside the
Pacific Northwest had heard of it. The cartridge plays the rules of that era: side-out scoring to 11, the
two-bounce rule, the 7-foot non-volley zone, underhand serves, one serve, let serves replayed.

**Status (2026-10-10): planning.** Nothing is built yet.

| document | what it is |
|---|---|
| [docs/PLAN.md](docs/PLAN.md) | the one-shot build plan: phases, gates, hosts, risks, decisions |
| [docs/DESIGN.md](docs/DESIGN.md) | the game design: modes, controls, court, physics, rules, AI, sound, the RAM mailbox and the voice layer |
| [docs/RESEARCH_pickleball.md](docs/RESEARCH_pickleball.md) | the sport: the 2026 rulebook digested, the 1984 differences, strategy, what players say |
| [docs/RESEARCH_nes.md](docs/RESEARCH_nes.md) | the toolchain: cc65 + neslib, MesenCE's test runner, RetroArch's network commands, the hosts, NES Tennis's facts |

## Two releases

| version | what | runs on |
|---|---|---|
| **v0.0.1 the cartridge** | the ROM: NROM-256 (32 KB PRG + 8 KB CHR), C and 6502 assembly with cc65. Singles and doubles against the computer, two players as a doubles team, five levels, an exhibition to watch. The server calls the score before every serve; the umpire calls faults. A documented RAM mailbox is built in and tested, unused by the cartridge itself | any NES emulator, a flash cart, the Flip 2 as a plain NES game |
| **v0.0.2 the teammate add-on** | the sidecar: autogamer's Contra co-op moved onto a court. Your doubles partner has a voice (Pocket TTS), calls the score, chatters ("mine", "bounce it", "switch"), makes its own calls between points (a language model), and takes yours: hold R1 and say "I've got the middle", "stay back", "switch". No ROM change: it reads and writes the mailbox through RetroArch | ROCKNIX handhelds with [llamatalk](https://github.com/mercurious/llamatalk); packaged as an [autogamer](https://github.com/mercurious/autogamer) entry, the first one that needs no dump from the player |

The add-on is built only after the cartridge is validated; the cartridge is built so that the add-on needs
nothing from it but the frozen mailbox and RAM map.

> **AI disclosure.** This project is written with Claude Code (Anthropic's Claude Fable 5.1). Most of the code
> and these documents are model output, produced under the author's direction. The author chooses what to build,
> runs it on the hardware and makes every judgment about what is fun or sounds right. Treat it as a working
> prototype, not a finished product.

## Planned layout

```
src/        the game: C (game logic) and ca65 assembly (NMI, PPU, sound, DPCM, controllers)
art/        tiles as text (0-3 per pixel), metasprites, palettes; tools/ turn them into CHR
tools/      Python 3: chr.py, court.py (the perspective court to a nametable), dmc.py (WAV to DPCM), check_rom.py
test/       Lua tests for MesenCE's test runner, with a rules oracle
sidecar/    v0.0.1: stub.py (a desktop fake voice over RetroArch); v0.0.2: the teammate (Pocket TTS, Qwen3-ASR, the model)
docs/       plan, design, research, the RAM map, a 1985-style manual
build/      pickleball.nes (not committed)
```

## Licence

Not chosen yet. The family default (llamatalk, autogamer, retro-voice-chat) is MIT for the code; the art and the
ROM may get a separate notice.
