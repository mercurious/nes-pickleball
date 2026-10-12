#!/usr/bin/env python3
"""Checks the built ROM: header, size, vectors; reports how full each code window and sample bank is from the
linker map (nes.cfg: MMC3, PRG mode 1)."""
import sys, re
rom = open(sys.argv[1], "rb").read()
assert rom[:4] == b"NES\x1a", "bad header"
prg, chr_ = rom[4] * 16384, rom[5] * 8192
assert len(rom) == 16 + prg + chr_, "size %d != header" % len(rom)
mapper = (rom[6] >> 4) | (rom[7] & 0xF0)
nmi, rst, irq = [int.from_bytes(rom[16 + prg - 6 + 2 * i: 16 + prg - 4 + 2 * i], "little") for i in range(3)]
assert 0xE000 <= rst < 0xFFFA and 0xE000 <= nmi < 0xFFFA, "the vectors must point into the fixed bank at $E000"
print("ROM ok: %d bytes, PRG %d KB, CHR %d KB, mapper %d, reset $%04X nmi $%04X" % (len(rom), prg // 1024, chr_ // 1024, mapper, rst, nmi))
if len(sys.argv) > 2:
    txt = open(sys.argv[2]).read()
    m = re.search(r"Segment list:.*?\n(.*?)\n\n", txt, re.S)
    used = {}
    for line in m.group(1).splitlines():
        p = line.split()
        if len(p) >= 4 and re.match(r"^[0-9A-F]{6}$", p[1]):
            used[p[0]] = int(p[3], 16)
    win = [("$8000 bank 30", ["CODE"], 0x2000), ("$A000 bank 29", ["CODE_A", "RODATA_A"], 0x2000),
           ("$6000 PRG-RAM", ["CODE_W"], 0x2000), ("$E000 bank 31", ["STARTUP", "KERNEL", "RODATA", "PENTLY", "DATA"], 0x1FFA)]
    print("code windows: " + ", ".join("%s %d (%d free)" % (n, sum(used.get(k, 0) for k in ks), size - sum(used.get(k, 0) for k in ks)) for n, ks, size in win))
    dmc = sorted((k, v) for k, v in used.items() if k.startswith("DMC"))
    print("sample banks: " + (", ".join("%s %d" % (k, v) for k, v in dmc) or "none") + "; BSS %d, ZP %d" % (used.get("BSS", 0), used.get("ZEROPAGE", 0)))
