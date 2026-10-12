# NES Pickleball: cc65 (C + ca65) -> build/pickleball.nes (MMC3). `make` on the M1 or the node.
ifneq ($(wildcard /usr/bin/cc65),)
B :=
else
B := $(HOME)/.local/cc65/bin/
export CC65_HOME := $(HOME)/.local/cc65/share/cc65
endif
CC65 = $(B)cc65
CA65 = $(B)ca65
LD65 = $(B)ld65
CFLAGS = -g -Oirs --cpu 6502 -t nes -I src -I build -I lib/pently -I audio/dpcm
CSRC = $(wildcard src/*.c)
OBJS = build/crt0.o $(patsubst src/%.c,build/%.o,$(CSRC)) build/tables.o build/data.o build/samples.o \
       build/pentlysound.o build/pentlymusic.o build/music.o
AFLAGS = -g -t nes -I src -I build -I lib/pently
ROM = build/pickleball.nes

all: $(ROM)

$(shell mkdir -p build build/tests)

build/bg.chr build/court_nt.bin build/tables.c build/geom.h build/court.png: tools/court.py tools/font.py
	python3 tools/court.py build

build/spr.chr build/sprdefs.h build/sprites.png: tools/chr.py art/sprites.txt
	python3 tools/chr.py art/sprites.txt build

# which 8 KB window each module's code lives in (nes.cfg): CODE is bank 30 at $8000, CODE_A bank 29 at $A000,
# CODE_W runs from PRG-RAM at $6000
build/ai.o build/game.o build/text.o: SEG = --code-name CODE_A
build/main.o build/hud.o build/sound.o: SEG = --code-name CODE_W

build/%.o: src/%.c src/kernel.h build/geom.h build/sprdefs.h build/music.h audio/dpcm/samples.h
	$(CC65) $(CFLAGS) $(SEG) -o build/$*.s $<
	$(CA65) -g -t nes -o $@ build/$*.s

build/tables.o: build/tables.c
	$(CC65) $(CFLAGS) -o build/tables.s $<
	$(CA65) -g -t nes -o $@ build/tables.s

build/crt0.o: src/crt0.s
	$(CA65) $(AFLAGS) -o $@ $<

build/data.o: src/data.s build/bg.chr build/spr.chr build/court_nt.bin
	$(CA65) $(AFLAGS) -o $@ $<

# the sound: Pently (lib/pently, zlib licence) with our configuration, the score, and the DPCM samples
build/pentlybss.inc: lib/pently/pentlybss.py src/pentlyconfig.inc
	python3 lib/pently/pentlybss.py src/pentlyconfig.inc pentlymusicbase -o $@

build/pentlysound.o: lib/pently/pentlysound.s src/pentlyconfig.inc build/music.s
	$(CA65) $(AFLAGS) -o $@ $<

build/pentlymusic.o: lib/pently/pentlymusic.s src/pentlyconfig.inc build/pentlybss.inc
	$(CA65) $(AFLAGS) -o $@ $<

build/music.s: lib/pently/pentlyas.py audio/pickleball.pently audio/crowd.pently
	python3 lib/pently/pentlyas.py audio/pickleball.pently --periods 76 --segment PENTLY -o $@.tmp
	sed 's|^.include ".*pentlyseq.inc"|.include "pentlyseq.inc"|' $@.tmp > $@ && rm $@.tmp

build/music.h: build/music.s
	python3 tools/musich.py $< > $@

build/music.o: build/music.s
	$(CA65) $(AFLAGS) -o $@ $<

build/samples.o: audio/dpcm/samples.s audio/dpcm/samples.h
	$(CA65) $(AFLAGS) -o $@ $<

samples:
	python3 tools/dpcm.py --preview build/listen/dpcm

audio/crowd.pently: tools/crowd.py
	python3 tools/crowd.py > $@

$(ROM): nes.cfg $(OBJS)
	$(LD65) -C nes.cfg -o $@ -m build/pickleball.map --dbgfile build/pickleball.dbg $(OBJS) nes.lib
	python3 tools/check_rom.py $@ build/pickleball.map
	python3 tools/ramsyms.py build/pickleball.dbg build/ramsyms.json docs/RAM_MAP.md

clean:
	rm -rf build/*

.PHONY: all clean samples
