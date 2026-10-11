#!/usr/bin/env python3
"""Checks the built ROM: header, size, vectors; reports the PRG budget from the linker map."""
import sys, re
rom = open(sys.argv[1], "rb").read()
assert rom[:4] == b"NES\x1a", "bad header"
prg, chr_ = rom[4] * 16384, rom[5] * 8192
assert len(rom) == 16 + prg + chr_, "size %d != header" % len(rom)
mapper = (rom[6] >> 4) | (rom[7] & 0xF0)
nmi, rst, irq = [int.from_bytes(rom[16 + prg - 6 + 2 * i: 16 + prg - 4 + 2 * i], "little") for i in range(3)]
assert 0x8000 <= rst < 0xFFFA and 0x8000 <= nmi < 0xFFFA, "bad vectors"
used = {}; starts = {}
if len(sys.argv) > 2:
    txt = open(sys.argv[2]).read()
    m = re.search(r"Segment list:.*?\n(.*?)\n\n", txt, re.S)
    if m:
        for line in m.group(1).splitlines():
            p = line.split()
            if len(p) >= 4 and re.match(r"^[0-9A-F]{6}$", p[1]):
                used[p[0]] = int(p[3], 16); starts[p[0]] = int(p[1], 16)
code = sum(v for k, v in used.items() if k in ("STARTUP", "CODE", "RODATA", "DATA"))
print("ROM ok: %d bytes, PRG %d KB, CHR %d KB, mapper %d, reset $%04X nmi $%04X" % (len(rom), prg // 1024, chr_ // 1024, mapper, rst, nmi))
if used:
    dmc_start = starts.get("DMC", 0xFFFA)
    print("PRG used: code+rodata+data %d bytes (%d free below the DMC segment at $%04X), DMC %d bytes (%d free); BSS %d, ZP %d" %
          (code, dmc_start - 0x8000 - code, dmc_start, used.get("DMC", 0), 0xFFFA - dmc_start - used.get("DMC", 0), used.get("BSS", 0), used.get("ZEROPAGE", 0)))
