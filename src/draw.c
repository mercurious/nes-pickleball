/* Sprites into the OAM shadow: 8x16 sprites, 16x32 frames (4 pairs: col0 top/bottom, col1 top/bottom). */
#include "kernel.h"
#include "draw.h"

#pragma bss-name(push, "ZEROPAGE")
unsigned char oam_i;
#pragma bss-name(pop)

void draw_begin(void)
{
    oam_i = 0;
}

void draw_end(void)
{
    do {
        oam[oam_i] = 0xF0;
        oam_i += 4;
    } while (oam_i);
}

void __fastcall__ draw_spr(unsigned char x, unsigned char y, unsigned char tile, unsigned char attr)
{
    if (y < 2 || y > 230) return;
    oam[oam_i] = y - 1;
    oam[oam_i + 1] = tile;
    oam[oam_i + 2] = attr;
    oam[oam_i + 3] = x;
    oam_i += 4;
}

void __fastcall__ draw_frame16(unsigned char x, unsigned char ytop, unsigned char tile, unsigned char attr)
{
    unsigned char x0 = x, x1 = x + 8;
    if (attr & ATTR_HFLIP) { x0 = x + 8; x1 = x; }
    draw_spr(x0, ytop, tile, attr);
    draw_spr(x0, ytop + 16, tile + 2, attr);
    draw_spr(x1, ytop, tile + 4, attr);
    draw_spr(x1, ytop + 16, tile + 6, attr);
}
