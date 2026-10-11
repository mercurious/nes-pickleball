# NES Pickleball: cc65 (C + ca65) -> build/pickleball.nes (NROM-256). `make` on the M1 or the node.
ifneq ($(wildcard /usr/bin/cc65),)
B :=
else
B := $(HOME)/.local/cc65/bin/
export CC65_HOME := $(HOME)/.local/cc65/share/cc65
endif
CC65 = $(B)cc65
CA65 = $(B)ca65
LD65 = $(B)ld65
CFLAGS = -g -Oirs --cpu 6502 -t nes -I src -I build -I art
CSRC = $(wildcard src/*.c)
OBJS = build/crt0.o $(patsubst src/%.c,build/%.o,$(CSRC)) build/tables.o build/data.o
ROM = build/pickleball.nes

all: $(ROM)

$(shell mkdir -p build build/tests)

build/bg.chr build/court_nt.bin build/tables.c build/geom.h build/court.png: tools/court.py tools/font.py
	python3 tools/court.py build

build/spr.chr build/sprdefs.h build/sprites.png: tools/chr.py art/sprites.txt
	python3 tools/chr.py art/sprites.txt build

build/%.o: src/%.c src/kernel.h build/geom.h build/sprdefs.h
	$(CC65) $(CFLAGS) -o build/$*.s $<
	$(CA65) -g -t nes -o $@ build/$*.s

build/tables.o: build/tables.c
	$(CC65) $(CFLAGS) -o build/tables.s $<
	$(CA65) -g -t nes -o $@ build/tables.s

build/crt0.o: src/crt0.s
	$(CA65) -g -t nes -o $@ $<

build/data.o: src/data.s build/bg.chr build/spr.chr build/court_nt.bin art/speech.bin
	$(CA65) -g -t nes -o $@ $<

$(ROM): nes.cfg $(OBJS)
	$(LD65) -C nes.cfg -o $@ -m build/pickleball.map --dbgfile build/pickleball.dbg $(OBJS) nes.lib
	python3 tools/check_rom.py $@ build/pickleball.map
	python3 tools/ramsyms.py build/pickleball.dbg build/ramsyms.json docs/RAM_MAP.md

speech:
	tools/speak.sh 5

clean:
	rm -rf build/*

.PHONY: all clean speech
