; Pently's configuration for NES Pickleball (see lib/pently/usage.md). Music: the jingles and the title tune;
; sound effects: the synthesized court sounds. The DMC is the cartridge's own (src/sound.c): samples and the voice.
.ifndef PENTLYCONFIG_INC
PENTLYCONFIG_INC = 1

PENTLY_USE_MUSIC = 1
PENTLY_USE_VIBRATO = 1
PENTLY_USE_PORTAMENTO = 0
PENTLY_USE_303_PORTAMENTO = 0
PENTLY_USE_ARPEGGIO = 1
PENTLY_USE_ATTACK_PHASE = 1
PENTLY_USE_ATTACK_TRACK = 1       ; noise pooling needs its fields
PENTLY_USE_CHANNEL_VOLUME = 1
PENTLY_USE_VARMIX = 0

PENTLY_USE_SQUARE_POOLING = 1       ; a pulse effect moves to pulse 2 when pulse 1 is busy
PENTLY_USE_NOISE_POOLING = 1
PENTLY_USE_MUSIC_IF_LOUDER = 1
PENTLY_USE_PAL_ADJUST = 0
PENTLY_USE_TRIANGLE_DUTY_FIX = 1

PENTLY_USE_BPMMATH = 0
PENTLY_USE_ROW_CALLBACK = 0
PENTLY_USE_VIS = 0
PENTLY_USE_REHEARSAL = 0
PENTLY_USE_TEMPO_ROUNDING_SEGNO = 0
PENTLY_USE_TEMPO_ROUNDING_PLAY_CH = -1
PENTLY_USE_TEMPO_ROUNDING_BEAT = 0
PENTLY_USE_NSF_SOUND_FX = 0
PENTLY_USE_NSF2 = 0

; the DMC's resting level, written once here and never again: every sample starts and ends on it (tools/dpcm.py),
; so the level never jumps (a jump clicks, and in the NES's mixer this level also sets how loud the triangle and
; noise are)
PENTLY_INITIAL_4011 = 64

PENTLY_USE_PREFIXED_DATA = 0
.importzp pently_zptemp

.define PENTLY_CODE "PENTLY"
.define PENTLY_RODATA "PENTLY"
.endif
