; Data baked by the tools: the court screen (nametable + attributes) and the two CHR banks. The DPCM samples are
; in audio/dpcm/samples.s (tools/dpcm.py), the music in build/music.s (Pently's assembler).
.export _court_nt
.segment "RODATA"
_court_nt:
    .incbin "build/court_nt.bin"
.segment "CHARS"
    .incbin "build/bg.chr"
    .incbin "build/spr.chr"
