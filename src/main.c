/* NES Pickleball: Phase 0: the title screen, the court, one player who walks, a ball on the ground. */
#include "kernel.h"
#include "geom.h"
#include "sprdefs.h"
#include "text.h"
#include "draw.h"

extern const unsigned char court_nt[];

/* background: 0 court (surround, court, lines, kitchen), 1 HUD boxes, 2 unused, 3 chair; sprites: 0 team A,
   1 team B (shirt by level), 2 ball + shadow, 3 badges */
static const unsigned char pal_court[32] = {
    0x09, 0x1A, 0x30, 0x2A,  0x09, 0x0F, 0x30, 0x28,  0x09, 0x30, 0x00, 0x0F,  0x09, 0x17, 0x37, 0x30,
    0x09, 0x37, 0x30, 0x0F,  0x09, 0x37, 0x2B, 0x0F,  0x09, 0x28, 0x18, 0x0F,  0x09, 0x0F, 0x30, 0x28
};
static const unsigned char pal_title[32] = {
    0x0F, 0x0F, 0x30, 0x28,  0x0F, 0x0F, 0x30, 0x28,  0x0F, 0x0F, 0x30, 0x28,  0x0F, 0x0F, 0x30, 0x28,
    0x0F, 0x37, 0x30, 0x0F,  0x0F, 0x37, 0x2B, 0x0F,  0x0F, 0x28, 0x18, 0x0F,  0x0F, 0x0F, 0x30, 0x28
};

static unsigned char pad, pad_old, trig;
static unsigned char px, py, pf, pflip, ptimer;
static unsigned char bx, by, bz;

static void set_pal(const unsigned char *p)
{
    unsigned char i;
    for (i = 0; i < 32; ++i) pal_buf[i] = p[i];
    pal_dirty = 1;
}

static void title_screen(void)
{
    ppu_off();
    set_pal(pal_title);
    vram_adr(0x2000);
    vram_fill(0, 1024);
    put_text(NTADR(11, 8), "PICKLEBALL");
    put_text(NTADR(9, 11), "1985 EDITION");
    put_text(NTADR(7, 20), "@ 2026 MERCURIOUS");
    put_text(NTADR(10, 24), "PUSH START");
    scroll_x = 0; scroll_y = 0;
    ppu_on_all();
    pad_old = 0xFF;
    for (;;) {
        pad = pad_poll(0);
        trig = pad & ~pad_old;
        pad_old = pad;
        if (trig & PAD_START) break;
        draw_begin();
        draw_end();
        ppu_wait_nmi();
    }
}

static void court_screen(void)
{
    ppu_off();
    set_pal(pal_court);
    vram_adr(0x2000);
    vram_write(court_nt, 1024);
    put_text(NTADR(3, 2), "YOU   0");
    put_text(NTADR(3, 3), "COM   0");
    put_text(NTADR(14, 2), "0-0-2");
    ppu_on_all();
}

void main(void)
{
    title_screen();
    court_screen();
    px = 128; py = 190; pf = 0; pflip = 0;
    bx = 128; by = 170; bz = 0;
    pad_old = 0xFF;
    for (;;) {
        pad = pad_poll(0);
        trig = pad & ~pad_old;
        pad_old = pad;
        if (pad & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN)) {
            if ((pad & PAD_LEFT) && px > 24) { --px; pflip = ATTR_HFLIP; }
            if ((pad & PAD_RIGHT) && px < 232) { ++px; pflip = 0; }
            if ((pad & PAD_UP) && py > 60) --py;
            if ((pad & PAD_DOWN) && py < 228) ++py;
            ++ptimer;
            pf = (ptimer & 8) ? SPR_N_RUN1 : SPR_N_RUN2;
        } else {
            pf = SPR_N_STAND;
        }
        if (pad & PAD_A) pf = SPR_N_SWING2;
        if (pad & PAD_B) pf = SPR_N_SWING1;
        draw_begin();
        draw_spr(bx - 4, by - bz - 3, SPR_BALL, 2);
        draw_spr(bx - 4, by - 1, SPR_SHADOW, 2);
        draw_frame16(px - 8, py - 31, pf, pflip);
        draw_frame16(100 - 8, 100 - 31, SPR_F_STAND, 1);
        draw_end();
        ppu_wait_nmi();
    }
}
