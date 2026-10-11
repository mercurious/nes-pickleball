; NES Pickleball kernel (ca65): reset, NMI (OAM DMA, palette, VRAM buffer, scroll), PPU helpers, pads, RNG.
; The C side (kernel.h) talks to it through __fastcall__ functions and a few zero-page variables.

.export __STARTUP__ : absolute = 1
.export _ppu_off, _ppu_on_all, _ppu_wait_nmi
.export _vram_adr, _vram_put, _vram_write, _vram_fill
.export _pad_poll, _rand8, _set_rand
.export _oam, _vbuf, _pal_buf
.exportzp _vbuf_len, _pal_dirty, _frame_count, _ppu_ctrl_var, _ppu_mask_var, _scroll_x, _scroll_y
.import _main, zerobss, copydata, popax, popa
.importzp sp
.import __STACK_START__, __STACK_SIZE__

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

.segment "HEADER"
    .byte "NES", $1A, 2, 1, $01, $00, 0, 0, 0, 0, 0, 0, 0, 0   ; 32 KB PRG, 8 KB CHR, mapper 0, vertical mirroring

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
pad_a:         .res 1
pad_b:         .res 1

.segment "OAM"
_oam: .res 256

.segment "VBUF"
_vbuf: .res 256

.segment "BSS"
_pal_buf: .res 32

.segment "STARTUP"
reset:
    sei
    cld
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

.segment "CODE"
nmi:
    pha
    txa
    pha
    tya
    pha
    inc _frame_count
    lda nmi_flag
    beq @done           ; the main loop has not finished a frame: touch nothing
    lda #0
    sta OAM_ADDR
    lda #>_oam
    sta OAM_DMA
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

; unsigned char __fastcall__ pad_poll(unsigned char pad): bits A B Sel St U D L R from 7 to 0; DMC-safe (read twice)
_pad_poll:
    tax
    jsr padread
    sta pad_a
    jsr padread
    cmp pad_a
    beq @ok
    jsr padread
@ok:
    rts
padread:
    lda #1
    sta JOY1
    lda #0
    sta JOY1
    ldy #8
@r:
    lda JOY1,x
    lsr
    rol pad_b
    dey
    bne @r
    lda pad_b
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
