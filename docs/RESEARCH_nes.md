# Research: building and testing an NES ROM here (2026-10-10)

## 1. The target cartridge

- **NES Tennis is NROM-128**: the maintainer's own dump has the iNES header `4E 45 53 1A 01 01 00 00`: one 16 KB
  PRG bank, one 8 KB CHR bank, mapper 0, horizontal mirroring; 24,592 bytes with the header. All of the first
  Famicom games were 16 + 8 KB.
- **Pickleball targets NROM-256**: 32 KB PRG at $8000-$FFFF, 8 KB CHR ROM, mapper 0, 2 KB of RAM ($0000-$07FF),
  no WRAM, no bank switching. Still a 1985 board (Super Mario Bros. is NROM-256). The extra 16 KB pays for the
  rules engine, doubles AI and digitized speech. Budget estimate: code 12-16 KB, tables and nametables 3-5 KB,
  DPCM words 9-11 KB; it fits, and DPCM words are the first thing to cut.
- **Sprites**: 64 hardware sprites, 8 per scanline. Four players (16 x 24 px, 2 x 3 sprites in 8 x 8 mode or two
  8 x 16 columns), ball, shadow: 26 sprites. Two partners side by side at the kitchen line plus ball and shadow
  are 6 on a scanline, under the limit; the near and far pairs never share scanlines.

## 2. Toolchain

| tool | role | where it comes from |
|---|---|---|
| **cc65 2.19** (cc65, ca65, ld65) | C for the game logic, ca65 for the kernel (NMI, OAM DMA, VRAM buffer, controllers, sound, DPCM) | Fedora 44: `sudo dnf install cc65` (2.19-15); Ubuntu 24.04 on `etk-cloud`: `sudo apt install cc65` (2.19-1); or `git clone https://github.com/cc65/cc65 && make` into `~/.local` (minutes) |
| **neslib** (Shiru; clbr fork, zlib) | the C-callable NES kernel: `ppu_on_all`, `oam_meta_spr`, `pad_poll/pad_trigger`, `vram_adr/vram_write`, `set_vram_update`, `pal_*`, `scroll`, `rand8`, FamiTone2 hooks | vendored at a pinned commit; its `crt0.s` and `nrom_32k_vert.cfg` (NROM-256) replace cc65's `nes.cfg`, whose layout assumes WRAM at $6000 and a 26-byte zero page |
| **Python 3.14 + Pillow 12 + numpy** (on the M1) | `tools/chr.py` (text tiles to CHR + a PNG preview), `tools/court.py` (the perspective court to tiles, a nametable and attributes; must stay under 256 unique background tiles with the font), `tools/dmc.py` (WAV to DPCM), `tools/check_rom.py` (header, size, vectors, free space) | installed |
| **cc65 flags** | `cc65 -Oirs --cpu 6502 -t nes`, `ca65 -t nes`, `ld65 -C nrom_32k_vert.cfg --dbgfile` | |

Considered and parked: **asm6f** (pure assembly; trivial to build; chosen against because the rules engine and the
AI are big and C is faster to get right in one session); **NESFab** (its own language and IDE, a C++ build;
another thing to learn mid-session); **FamiStudio** (music; no Linux ARM64 build, so no engine export here);
**FamiTone2** (needs FamiTracker text exports; possible later). Sound is therefore a small driver of our own:
pulse/triangle note tables for jingles, noise bursts for applause and the paddle, DPCM for words.

## 3. Emulators and the test runner

- **MesenCE** (community fork of Mesen2, GPL-3): the debugger emulator. Linux ARM64 development builds exist;
  building from source needs the .NET 10 SDK, SDL2 and clang (`make`; `USE_GCC=true make`). Build it on
  `etk-cloud` (aarch64, so the same binary runs on the M1). **Test runner**: `Mesen --testRunner game.nes
  test.lua --timeout=60` (the switch matches "testrunner" with or without dashes, any case): it loads the ROM and
  the Lua script, runs at maximum speed without saving settings, and the process exit code is the value the script
  gives `emu.stop(code)` (default 0); -1 means a timeout or a ROM that failed to load. If the runner still wants a
  display, `atonamy/mesen-agent` is a headless fork of the same code; `cppchriscpp/nes-test` wraps Mesen in a
  Jasmine runner.
- **Lua API** (Mesen2): `emu.read(addr, emu.memType.nesMemory)` / `emu.write(addr, value, memType)` (16-bit
  variants, `...Debug` types without side effects; NES types `nesMemory`, `nesPpuMemory`, `nesPrgRom`,
  `nesInternalRam`, `nesChrRom`, `nesNametableRam`, `nesSpriteRam`, `nesPaletteRam`),
  `emu.addEventCallback(fn, emu.eventType.endFrame)` (events `nmi irq startFrame endFrame reset scriptEnded
  inputPolled stateLoaded stateSaved codeBreak`), `emu.addMemoryCallback(fn, callbackType, start, end)`,
  `emu.setInput({a=true, left=true}, port)` / `emu.getInput(port)`, `emu.getState()` (has `frameCount`),
  `emu.createSavestate()` / `emu.loadSavestate()`, `emu.takeScreenshot()` (PNG bytes), `emu.log`,
  `emu.displayMessage`, `emu.drawString`, `emu.getAccessCounters`, `emu.stop(code)`.
- **RetroArch on the M1**: 1.22.0 with Fedora's `libretro-nestopia` (fceumm is not packaged for Fedora and the
  libretro buildbot offers no Linux ARM64 cores; build fceumm from source if nestopia lacks a memory map). Network
  commands are off in its config; enable them per launch with `--appendconfig` as autogamer does.
- **RetroArch on the handhelds** (ROCKNIX, 1.22.2, fceumm): the proven transport. UDP 55355 commands
  `READ_CORE_MEMORY <hex addr> <n>` → `READ_CORE_MEMORY <addr> <b1> <b2> ...`, `WRITE_CORE_MEMORY <hex addr> <b1>
  ...` → `WRITE_CORE_MEMORY <addr> <bytes written>` (the system memory map; fceumm's RAM is writable; it turns off
  achievements' hardcore mode), `SHOW_MSG text`, `PAUSE_TOGGLE`, `FRAMEADVANCE`, `SCREENSHOT`, `GET_STATUS`,
  `GET_CONFIG_PARAM`, `SAVE_STATE`/`LOAD_STATE`, `QUIT` (unreliable; kill by PID). The network remote (UDP 55400 +
  user) takes a 20-byte `remote_message` per button change (one per user per frame). autogamer's `app/ra.py`,
  `app/pad.py`, `app/voice.py` (Pocket TTS → `pacat`), `app/talk.py` (hold R1 → Qwen3-ASR) are the reference
  implementations and are reused as-is.

## 4. Hosts

| host | use | notes |
|---|---|---|
| M1 laptop (Fedora Asahi 44, 8 GB, usually 1 GB free) | cc65 builds (seconds), the tools, RetroArch + nestopia for a visual check, the sidecar with a stub voice | no .NET builds, no model servers, no soaks |
| `etk-cloud` (OCI Ampere, 4 OCPU, 24 GB, Ubuntu 24.04, aarch64, docker) | build MesenCE once; headless test soaks (thousands of games); cc65 via apt as a second build host | no cc65, make or gcc installed yet (apt) |
| `car8` (8 GB Flip 2, ETK) / `car12` (12 GB Flip 2, stock ROCKNIX, shared) | the real thing: fceumm, llamatalk's servers (Gemma 4 E2B/E4B, Pocket TTS, Qwen3-ASR), autogamer 0.3.2 installed on both | ask before using car12; coordinate listening tests |

## 5. Graphics pipeline

- CHR: 2 bpp planar, 16 bytes per 8 x 8 tile (8 bytes plane 0, then plane 1); 4 KB background table (256 tiles) +
  4 KB sprite table (256 tiles, 128 in 8 x 16 mode). Palettes: 4 x 4 colours for background (colour 0 shared), 4 x 3
  + transparent for sprites.
- Tiles are authored as text (`art/*.txt`, one char 0-3 per pixel) so they can be written and diffed; `tools/chr.py`
  renders a PNG preview the maintainer can look at. The court is not hand-drawn: `tools/court.py` draws the
  perspective lines onto a 256 x 240 canvas, cuts it into 8 x 8 tiles, deduplicates them and emits the nametable
  (960 bytes), attribute table (64 bytes) and unique tiles; it fails loudly over 256 - (font + HUD) tiles.
- NTSC safe area: keep the HUD inside x 16-240, y 16-224.

## 6. Sound and digitized speech

- APU: two pulses (jingles, the ball), triangle (bass/ump), noise (applause, paddle thwack, footsteps), DMC.
- **DPCM**: 1-bit delta, 7-bit level, 16 NTSC rates (index 0-15: 4182, 4710, 5264, 5593, 6258, 7046, 8363, 9420,
  11186, 12604, 13983, 16885, 18634, 22372, 26584, 33144 Hz); bytes per second = rate / 8; a sample starts at
  $C000 + 64 x n, length 16 x m + 1 bytes, at most 4,081 bytes. Speech at index 7 (8,363 Hz) costs 1,045 bytes/s;
  index 9 (12,604 Hz) 1,575 bytes/s. The words "zero" ... "eleven", "two" (server number), "side out", "fault",
  "out", "game", at 0.4-0.6 s each, come to about 10 KB at index 7. Longer games (15, 21) count up with the same
  digits ("fif-teen" = two words, or a cap at 11 for the voice).
- Encoder: `tools/dmc.py` in Python: resample, pre-emphasis, 1-bit delta modulation with a 7-bit accumulator and
  slope-overload tracking; the source voice is Pocket TTS rendered offline on a handheld (or any WAV).
- Playback: write $4010 (rate, no loop), $4012 (address >> 6), $4013 (length >> 4), then set bit 4 of $4015; poll
  $4015 bit 4 for "still playing"; chain words from the frame handler.

## 7. What NES Tennis does (what we borrow)

From the 1985 NES manual (archive.org scan, read page by page) and the autogamer lab's RAM work:

- **Modes**: 1 player singles against the computer, or 2 players as a doubles team against a computer team. No
  2-player singles, no 1-player doubles. Five opponent levels, each a shirt colour (light green, pink, light blue,
  purple, black). A match is best of 3 sets, 6 games a set, tiebreak at 6-6; two serves; the umpire (Mario) calls
  faults and outs.
- **Controls**: pad moves (up = toward the net); A swings and serves; B lobs. The serve: the player is placed
  behind the baseline automatically, the ball is tossed automatically, A in time with the dropping ball; mistimed
  serves go into the net or long. The *direction* of a return is set by *when* the swing meets the ball, not by
  the pad: a forehand met late goes right, met early goes left; the stand-off (the ball's x relative to the player)
  decides forehand or backhand and whether it is a miss. Holding a direction at contact does not steer.
  Returning the serve on the volley loses the point (Tennis's own bounce rule).
- **RAM shape** (lab, 2026-10-09/10): ball ground x/y on screen plus a height and a vertical speed (the shadow is
  the ground position; the ball sprite is drawn at ground y minus height); player feet x/y; points per side; an
  "object active" flag. Physics in screen space with a perspective-drawn court; sprites are the same size far and
  near; the computer at level 5 rushes the net and volleys.
- **Look** (video): trapezoid court, net band across mid-screen, two court greens, a plain surround, the umpire
  chair at the net post, the score panel top-left, the publisher's banner at the top.

## Sources

- NESdev wiki: NROM https://nesdev.org/wiki/NROM ; tools https://nesdev.org/wiki/Tools ; APU DMC https://nesdev.org/wiki/APU_DMC
- cc65: https://github.com/cc65/cc65 ; its NES config https://raw.githubusercontent.com/cc65/cc65/master/cfg/nes.cfg
- neslib (clbr fork): https://github.com/clbr/neslib ; the cnrom sample https://github.com/clbr/nes/tree/master/cnrom
- MesenCE: https://github.com/nesdev-org/MesenCE (COMPILING.md) ; Mesen2 test runner and Lua API:
  https://github.com/SourMesen/Mesen2 (UI/Utilities/CommandLineHelper.cs, UI/Utilities/TestRunner.cs,
  Core/Debugger/LuaApi.cpp, Core/Shared/EventType.h, Core/Shared/MemoryType.h) ; headless fork https://github.com/atonamy/mesen-agent
- RetroArch network control interface: https://docs.libretro.com/development/retroarch/network-control-interface/
- NES Tennis manual (scan): https://archive.org/download/NESManuals/Tennis%20%28Japan%2C%20USA%29_text.pdf ;
  Wikipedia https://en.wikipedia.org/wiki/Tennis_(1984_video_game)
- DPCM conversion threads: https://nesdev.nes.science/f6/t6975.xhtml (RJDMC) ; https://nesdev.nes.science/f22/t8671.xhtml
