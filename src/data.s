; Data baked by the tools: the court screen (nametable + attributes) and the two CHR banks.
.export _court_nt, _speech_data
.segment "RODATA"
_court_nt:
    .incbin "build/court_nt.bin"
.segment "DMC"
.align 64
_speech_data:
    .incbin "art/speech.bin"
.segment "CHARS"
    .incbin "build/bg.chr"
    .incbin "build/spr.chr"
