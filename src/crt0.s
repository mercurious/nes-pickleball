; NES Pickleball kernel (ca65): reset, NMI (OAM DMA, palette, VRAM buffer, scroll), PPU helpers, pads, RNG, and
; the MMC3's banks. The C side (kernel.h) talks to it through __fastcall__ functions and a few zero-page variables.
; All of it sits in bank 31, the one the MMC3 always maps at $E000.

.export __STARTUP__ : absolute = 1
.export _ppu_off, _ppu_on_all, _ppu_wait_nmi
.export _vram_adr, _vram_put, _vram_write, _vram_fill
.export _pad_poll, _rand8, _set_rand, _dmc_bank, _snd_cmd, _snd_on
.import pently_update, pently_start_sound, pently_start_music, pently_stop_music
.export _oam, _vbuf, _pal_buf
.exportzp _vbuf_len, _pal_dirty, _frame_count, _ppu_ctrl_var, _ppu_mask_var, _scroll_x, _scroll_y, pently_zptemp
.import _main, zerobss, copydata, popax, popa
.importzp sp
.import __STACK_START__, __STACK_SIZE__
.import __CODE_W_LOAD__, __CODE_W_RUN__, __CODE_W_SIZE__

PPU_CTRL   = $2000
PPU_MASK   = $2001
PPU_STATUS = $2002
OAM_ADDR   = $2003
PPU_SCROLL = $2005
PPU_ADDR   = $2006
PPU_DATA   = $2007
OAM_DMA    = $4014
APU_STATUS = $4015
JOY1       = $4016
JOY2       = $4017
DMC_FREQ   = $4010
MMC3_SEL   = $8000      ; bits 0-2 the register to set next; bit 6 PRG mode 1 ($C000 switchable)
MMC3_BANK  = $8001
MMC3_MIRR  = $A000      ; 0 vertical
MMC3_WRAM  = $A001      ; $80 PRG-RAM on, writable
MMC3_IRQOFF = $E000
SEL        = $40        ; PRG mode 1, CHR A12 not inverted
CODE_A_BANK = 29        ; R7, kept at $A000
WCODE_BANK  = 28        ; the image of the PRG-RAM code

.segment "HEADER"
    ; NES 2.0: 256 KB PRG, 32 KB CHR, mapper 4 (MMC3), 8 KB PRG-RAM (64 << 7) without a battery, NTSC
    .byte "NES", $1A, 16, 4, $40, $08, 0, 0, $07, 0, 0, 0, 0, 0

.segment "ZEROPAGE"
_frame_count:  .res 1
_vbuf_len:     .res 1
_pal_dirty:    .res 1
_ppu_ctrl_var: .res 1
_ppu_mask_var: .res 1
_scroll_x:     .res 1
_scroll_y:     .res 1
nmi_flag:      .res 1
vb_cnt:        .res 1
kptr:          .res 2
kcnt:          .res 2
rand_lo:       .res 1
rand_hi:       .res 1
pad1:          .res 1         ; the controllers as the NMI last read them (bits A B Sel St U D L R)
pad2:          .res 1
_snd_on:       .res 1         ; the NMI runs Pently once this is set (after pently_init)
snd_qh:        .res 1         ; the sound queue: the main thread adds at the tail, the NMI takes from the head
snd_qt:        .res 1
pently_zptemp: .res 5         ; Pently's scratch

.segment "OAM"
_oam: .res 256

.segment "VBUF"
_vbuf: .res 256

.segment "BSS"
_pal_buf: .res 32
snd_q:    .res 8

.segment "STARTUP"
reset:
    sei
    cld
    ldx #0              ; the MMC3 first, from here in bank 31: CHR 2 KB 0 and 2 at $0000, 1 KB 4-7 at $1000
@mmc3:
    lda mmc3_init,x
    sta MMC3_SEL
    lda mmc3_init+1,x
    sta MMC3_BANK
    inx
    inx
    cpx #16
    bne @mmc3
    lda #0
    sta MMC3_MIRR
    lda #$80
    sta MMC3_WRAM
    sta MMC3_IRQOFF
    ldx #$40
    stx JOY2            ; frame IRQ off
    ldx #$FF
    txs
    inx
    stx PPU_CTRL        ; NMI off
    stx PPU_MASK        ; rendering off
    stx DMC_FREQ        ; DMC IRQ off
    bit PPU_STATUS
@vb1:
    bit PPU_STATUS
    bpl @vb1
    lda #0
    tax
@clr:                   ; clear all 2 KB of RAM
    sta $0000,x
    sta $0100,x
    sta $0200,x
    sta $0300,x
    sta $0400,x
    sta $0500,x
    sta $0600,x
    sta $0700,x
    inx
    bne @clr
    lda #$F0            ; sprites off screen
@oamclr:
    sta _oam,x
    inx
    inx
    inx
    inx
    bne @oamclr
@vb2:
    bit PPU_STATUS
    bpl @vb2
    lda #<(__STACK_START__ + __STACK_SIZE__)
    sta sp
    lda #>(__STACK_START__ + __STACK_SIZE__)
    sta sp+1
    jsr zerobss
    jsr copydata
    lda #<__CODE_W_LOAD__ ; the PRG-RAM code: bank 28 is mapped at $C000 for the copy, then sample bank 0
    sta kptr
    lda #>__CODE_W_LOAD__
    sta kptr+1
    lda #<__CODE_W_RUN__
    sta kcnt
    lda #>__CODE_W_RUN__
    sta kcnt+1
    ldy #0
    ldx #>(__CODE_W_SIZE__ + 255)
    beq @wdone
@wcopy:
    lda (kptr),y
    sta (kcnt),y
    iny
    bne @wcopy
    inc kptr+1
    inc kcnt+1
    dex
    bne @wcopy
@wdone:
    lda #0
    jsr _dmc_bank
    lda #$0F
    sta APU_STATUS      ; pulse 1, pulse 2, triangle, noise on; DMC off
    lda #$A0            ; NMI on, 8x16 sprites, BG table 0, nametable 0
    sta _ppu_ctrl_var
    lda #0
    sta _ppu_mask_var
    sta _vbuf_len
    sta nmi_flag
    lda #$A5
    sta rand_lo
    lda #$5A
    sta rand_hi
    lda _ppu_ctrl_var
    sta PPU_CTRL        ; NMI enabled from here on
    jsr _main
@halt:
    jmp @halt

.segment "KERNEL"
; the MMC3's register pairs at reset: CHR R0-R5, R6 the WRAM code image (for the copy), R7 the $A000 code bank
mmc3_init:
    .byte SEL|0, 0, SEL|1, 2, SEL|2, 4, SEL|3, 5, SEL|4, 6, SEL|5, 7, SEL|6, WCODE_BANK, SEL|7, CODE_A_BANK

; void __fastcall__ snd_cmd(unsigned char c): queues a request for Pently, run by the NMI: $00-$3F starts sound
; effect c, $40-$7F starts song c-$40, $80 stops the music
_snd_cmd:
    ldx snd_qt
    sta snd_q,x
    inx
    txa
    and #7
    sta snd_qt
    rts

; void __fastcall__ dmc_bank(unsigned char bank): maps a sample bank (0-27) at $C000 for the DMC. Only the main
; thread switches banks; the NMI never does.
_dmc_bank:
    ldx #SEL|6
    stx MMC3_SEL
    sta MMC3_BANK
    rts

nmi:
    pha
    txa
    pha
    tya
    pha
    inc _frame_count
    lda nmi_flag
    bne @draw
    jmp @done           ; the main loop has not finished a frame: touch nothing
@draw:
    lda #0
    sta OAM_ADDR
    lda #>_oam
    sta OAM_DMA
    ; The controllers, read in step with the OAM DMA (the NESdev wiki's "DMC-safe controller reading synced to OAM
    ; DMA"): after the DMA the cycle parity is known, and every $4016/$4017 read lands on a cycle where the DMC's
    ; DMA cannot fetch. Otherwise a fetch during the read corrupts both: the controller loses a bit, and the DMC
    ; gets a byte ANDed with the joypad lines whenever its address ends in $16 or $17, which knocks the level off
    ; its resting point (found in Mesen, which emulates the 2A03 doing it).
    ldx #1              ; the strobe takes an odd number of cycles
    stx pad1
    stx JOY1
    dex
    stx JOY1
@pads:
    lda JOY2            ; the loop takes an even number of cycles
    and #3
    cmp #1
    rol pad2,x          ; X = 0: zp,x for the cycle count
    lda JOY1
    and #3
    cmp #1
    rol pad1
    bcc @pads
    lda _pal_dirty
    beq @nopal
    bit PPU_STATUS
    lda #$3F
    sta PPU_ADDR
    lda #0
    sta PPU_ADDR
    ldx #0
@pal:
    lda _pal_buf,x
    sta PPU_DATA
    inx
    cpx #32
    bne @pal
    lda #0
    sta _pal_dirty
@nopal:
    ldx #0
@vloop:
    cpx _vbuf_len
    bcs @vdone
    lda _vbuf,x         ; address high
    sta PPU_ADDR
    inx
    lda _vbuf,x         ; address low
    sta PPU_ADDR
    inx
    lda _vbuf,x         ; run length
    sta vb_cnt
    inx
@vdata:
    lda _vbuf,x
    sta PPU_DATA
    inx
    dec vb_cnt
    bne @vdata
    jmp @vloop
@vdone:
    lda #0
    sta _vbuf_len
    bit PPU_STATUS
    lda _scroll_x
    sta PPU_SCROLL
    lda _scroll_y
    sta PPU_SCROLL
    lda _ppu_ctrl_var
    sta PPU_CTRL
    lda _ppu_mask_var
    sta PPU_MASK
    lda #0
    sta nmi_flag
@done:
    ; the sound, every frame, also when the main loop is late (it often is: the match logic takes more than a
    ; frame about four frames in ten). Pently's tempo and envelopes count frames, so it runs here; the main
    ; thread only queues requests (snd_cmd), and only the NMI calls Pently, so it is never reentered.
    lda _snd_on
    beq @nosnd
@sq:
    ldx snd_qh
    cpx snd_qt
    beq @sqdone
    lda snd_q,x
    pha
    inx
    txa
    and #7
    sta snd_qh
    pla
    cmp #$80
    bcs @sstop
    cmp #$40
    bcs @smusic
    jsr pently_start_sound
    jmp @sq
@smusic:
    and #$3F
    jsr pently_start_music
    jmp @sq
@sstop:
    jsr pently_stop_music
    jmp @sq
@sqdone:
    jsr pently_update
@nosnd:
    pla
    tay
    pla
    tax
    pla
irq:
    rti

; void ppu_wait_nmi(void): the frame is ready; wait until the NMI has shown it
_ppu_wait_nmi:
    lda #1
    sta nmi_flag
@w:
    lda nmi_flag
    bne @w
    rts

; void ppu_off(void): rendering off now (then VRAM may be written directly)
_ppu_off:
    lda #0
    sta _ppu_mask_var
    sta PPU_MASK
    rts

; void ppu_on_all(void): BG + sprites on at the next frame
_ppu_on_all:
    lda #$1E
    sta _ppu_mask_var
    rts

; void __fastcall__ vram_adr(unsigned int adr)
_vram_adr:
    bit PPU_STATUS
    stx PPU_ADDR
    sta PPU_ADDR
    rts

; void __fastcall__ vram_put(unsigned char v)
_vram_put:
    sta PPU_DATA
    rts

; void __fastcall__ vram_write(const void *src, unsigned int n)
_vram_write:
    sta kcnt
    stx kcnt+1
    jsr popax
    sta kptr
    stx kptr+1
    ldy #0
@l:
    lda kcnt
    ora kcnt+1
    beq @done
    lda (kptr),y
    sta PPU_DATA
    inc kptr
    bne :+
    inc kptr+1
:
    lda kcnt
    bne :+
    dec kcnt+1
:
    dec kcnt
    jmp @l
@done:
    rts

; void __fastcall__ vram_fill(unsigned char v, unsigned int n)
_vram_fill:
    sta kcnt
    stx kcnt+1
    jsr popa
@loop:
    ldx kcnt
    bne @dec
    ldx kcnt+1
    beq @done
    dec kcnt+1
@dec:
    dec kcnt
    sta PPU_DATA
    jmp @loop
@done:
    rts

; unsigned char __fastcall__ pad_poll(unsigned char pad): bits A B Sel St U D L R from 7 to 0, as the NMI read them
_pad_poll:
    tax
    lda pad1,x
    ldx #0
    rts

; unsigned char rand8(void): 16-bit LFSR, 8 new bits
_rand8:
    ldx #8
    lda rand_lo
@s:
    asl
    rol rand_hi
    bcc :+
    eor #$39
:
    dex
    bne @s
    sta rand_lo
    cmp #0
    rts

; void __fastcall__ set_rand(unsigned int seed)
_set_rand:
    sta rand_lo
    stx rand_hi
    ora rand_hi
    bne :+
    inc rand_lo
:
    rts

.segment "VECTORS"
    .addr nmi, reset, irq
