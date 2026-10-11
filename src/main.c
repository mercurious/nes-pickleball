/* NES Pickleball: the title, the match loop. */
#include "game.h"
#include "text.h"
#include "draw.h"
#include "hud.h"

extern const unsigned char court_nt[];

static const unsigned char pal_court[32] = {
    0x09, 0x1A, 0x30, 0x2A,  0x09, 0x0F, 0x30, 0x28,  0x09, 0x30, 0x00, 0x0F,  0x09, 0x17, 0x37, 0x30,
    0x09, 0x37, 0x30, 0x0F,  0x09, 0x37, 0x2B, 0x0F,  0x09, 0x28, 0x18, 0x0F,  0x09, 0x0F, 0x30, 0x28
};
static const unsigned char pal_title[32] = {
    0x0F, 0x0F, 0x30, 0x28,  0x0F, 0x0F, 0x30, 0x28,  0x0F, 0x0F, 0x30, 0x28,  0x0F, 0x0F, 0x30, 0x28,
    0x0F, 0x37, 0x30, 0x0F,  0x0F, 0x37, 0x2B, 0x0F,  0x0F, 0x28, 0x18, 0x0F,  0x0F, 0x0F, 0x30, 0x28
};
static const unsigned char LEVEL_SHIRT[6] = { 0x2B, 0x2B, 0x25, 0x21, 0x13, 0x02 };

static unsigned char old[2];

static void set_pal(const unsigned char *p)
{
    unsigned char i;
    for (i = 0; i < 32; ++i) pal_buf[i] = p[i];
    pal_dirty = 1;
}

static void read_pads(void)
{
    unsigned char i, p;
    for (i = 0; i < 2; ++i) {
        p = pad_poll(i);
        trigs[i] = p & ~old[i];
        old[i] = p;
        pads[i] = p;
    }
}

static void title_screen(void)
{
    unsigned int seed = 0x1234;
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
    old[0] = 0xFF; old[1] = 0xFF;
    for (;;) {
        read_pads();
        ++seed;
        if (trigs[0] & PAD_START) break;
        draw_begin();
        draw_end();
        ppu_wait_nmi();
    }
    set_rand(seed ^ (frame_count << 8));
}

static void court_screen(void)
{
    ppu_off();
    set_pal(pal_court);
    pal_buf[22] = LEVEL_SHIRT[level];
    vram_adr(0x2000);
    vram_write(court_nt, 1024);
    ppu_on_all();
}

void main(void)
{
    for (;;) {
        title_screen();
        match_init(MODE_1P_SINGLES, 1);
        court_screen();
        for (;;) {
            read_pads();
            match_update();
            draw_match();
            hud_update();
            ppu_wait_nmi();
            if (state == ST_GAMEOVER && state_t > 240) break;
        }
    }
}
