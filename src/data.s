; Data baked by the tools: the court screen (nametable + attributes) and the two CHR banks.
.export _court_nt
.segment "RODATA"
_court_nt:
    .incbin "build/court_nt.bin"
.segment "CHARS"
    .incbin "build/bg.chr"
    .incbin "build/spr.chr"
