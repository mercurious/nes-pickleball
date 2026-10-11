#ifndef DRAW_H
#define DRAW_H
extern unsigned char oam_i;
#pragma zpsym("oam_i")
#define ATTR_HFLIP 0x40
#define ATTR_VFLIP 0x80
#define ATTR_BEHIND 0x20
void draw_begin(void);
void draw_end(void);
void __fastcall__ draw_spr(unsigned char x, unsigned char y, unsigned char tile, unsigned char attr);
void __fastcall__ draw_frame16(unsigned char x, unsigned char ytop, unsigned char tile, unsigned char attr);
#endif
