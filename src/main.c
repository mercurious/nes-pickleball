/* NES Pickleball: the title, the menu, the match loop with pause, the attract mode. */
#include "game.h"
#include "text.h"
#include "draw.h"
#include "hud.h"
#include "sound.h"

extern const unsigned char court_nt[];

static const unsigned char pal_court[32] = {
    0x09, 0x1A, 0x30, 0x2A,  0x09, 0x0F, 0x30, 0x28,  0x09, 0x30, 0x00, 0x0F,  0x09, 0x17, 0x37, 0x30,
    0x09, 0x37, 0x30, 0x0F,  0x09, 0x37, 0x16, 0x0F,  0x09, 0x37, 0x28, 0x0F,  0x09, 0x37, 0x12, 0x0F
};   /* the four sprite palettes are {skin, shirt, dark}; the shirts are drawn by assign_shirts */
static const unsigned char pal_title[32] = {
    0x0F, 0x0F, 0x30, 0x28,  0x0F, 0x0F, 0x30, 0x28,  0x0F, 0x0F, 0x30, 0x28,  0x0F, 0x0F, 0x30, 0x28,
    0x0F, 0x37, 0x30, 0x0F,  0x0F, 0x37, 0x2B, 0x0F,  0x0F, 0x28, 0x18, 0x0F,  0x0F, 0x0F, 0x30, 0x28
};
/* the shirts a match draws from: white, red, blue, orange, magenta, teal; none near the court's greens, the skin
   or the ball's yellow */
static const unsigned char SHIRTS[6] = { 0x30, 0x16, 0x12, 0x27, 0x24, 0x2C };
static const char *MODE_NAMES[MODE_COUNT] = { "1 PLAYER  SINGLES", "1 PLAYER  DOUBLES", "2 PLAYERS DOUBLES",
                                              "2 PLAYERS SINGLES", "EXHIBITION       " };
static const char *GAME_TO_NAMES[3] = { "GAME TO 11  ", "GAME TO 15  ", "GAME TO 21  " };
static const unsigned char GAME_TO_VALUES[3] = { 11, 15, 21 };

static unsigned char old[2];
static unsigned char cursor, game_to_i, demo, paused;
static unsigned int idle;

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

static void menu_line(unsigned char line)
{
    static char buf[20];
    unsigned char i;
    const char *s;
    switch (line) {
    case 0: s = MODE_NAMES[mode]; break;
    case 1: s = (level == 1) ? "LEVEL 1  BEGINNER" : (level == 5) ? "LEVEL 5  CHAMPION" : "LEVEL 3          "; break;
    case 2: s = GAME_TO_NAMES[game_to_i]; break;
    case 3: s = best_of3 ? "BEST OF 3   " : "ONE GAME    "; break;
    default: s = lets_replay ? "LETS: REPLAY" : "LETS: PLAY  "; break;
    }
    buf[0] = (cursor == line) ? '*' : ' ';
    buf[1] = ' ';
    for (i = 0; s[i] && i < 17; ++i) buf[2 + i] = s[i];
    buf[2 + i] = 0;
    if (line == 1) { buf[2 + 6] = '0' + level; }
    vbuf_text(NTADR(6, 14 + line * 2), buf);
}

static void menu_redraw(void)
{
    unsigned char i;
    for (i = 0; i < 5; ++i) menu_line(i);
}

/* the title and the menu; returns when START is pressed, or with demo set after 25 s idle */
static void title_screen(void)
{
    unsigned char i;
    ppu_off();
    set_pal(pal_title);
    vram_adr(0x2000);
    vram_fill(0, 1024);
    put_text(NTADR(11, 5), "PICKLEBALL");
    put_text(NTADR(9, 8), "1985 EDITION");
    put_text(NTADR(5, 25), "SELECT: CHOOSE  START: PLAY");
    put_text(NTADR(7, 27), "@ 2026 MERCURIOUS");
    scroll_x = 0; scroll_y = 0;
    cursor = 0;
    for (i = 0; i < 5; ++i) menu_line(i);
    ppu_on_all();
    jingle(JG_TITLE);
    old[0] = 0xFF; old[1] = 0xFF;
    idle = 0; demo = 0;
    for (;;) {
        read_pads();
        if (trigs[0] & PAD_START) break;
        if (trigs[0] & (PAD_SELECT | PAD_DOWN)) { cursor = (cursor + 1) % 5; sfx(SFX_MENU); menu_redraw(); }
        if (trigs[0] & PAD_UP) { cursor = (cursor + 4) % 5; sfx(SFX_MENU); menu_redraw(); }
        if (trigs[0] & (PAD_LEFT | PAD_RIGHT | PAD_A)) {
            signed char dir = (trigs[0] & PAD_LEFT) ? -1 : 1;
            sfx(SFX_MENU);
            switch (cursor) {
            case 0: mode = (mode + MODE_COUNT + dir) % MODE_COUNT; break;
            case 1: level = (level + 4 + dir) % 5 + 1; break;
            case 2: game_to_i = (game_to_i + 3 + dir) % 3; break;
            case 3: best_of3 ^= 1; break;
            default: lets_replay ^= 1; break;
            }
            menu_line(cursor);
        }
        if (pads[0] || pads[1]) idle = 0; else if (++idle > 1500) { demo = 1; break; }
        draw_begin();
        draw_end();
        snd_update();
        ppu_wait_nmi();
    }
    jingle(JG_NONE);
    if (!demo) sfx(SFX_MENU);
    set_rand(idle ^ (frame_count << 8) ^ 0x5A17);
}

/* Every player wears a shirt of their own, drawn at random for the match, so they can be told apart: three of the
   SHIRTS go to sprite palettes 0, 1 and 3, and palette 2's shirt is the ball's yellow, which one player wears in
   doubles (the ball is drawn in that palette). Which player gets which palette is shuffled too. */
static void assign_shirts(void)
{
    unsigned char pool[6], perm[4], i, j, k, n;
    for (i = 0; i < 6; ++i) pool[i] = SHIRTS[i];
    for (i = 0; i < 3; ++i) { j = i + rand8() % (6 - i); k = pool[i]; pool[i] = pool[j]; pool[j] = k; }
    pal_buf[16 + 2] = pool[0]; pal_buf[20 + 2] = pool[1]; pal_buf[28 + 2] = pool[2]; pal_buf[24 + 2] = 0x28;
    for (i = 0; i < 4; ++i) perm[i] = i;
    n = doubles ? 4 : 2;                                   /* singles: palettes 0 and 1, the ball's stays its own */
    for (i = 0; i + 1 < n; ++i) { j = i + rand8() % (n - i); k = perm[i]; perm[i] = perm[j]; perm[j] = k; }
    if (doubles) for (i = 0; i < 4; ++i) shirt[i] = perm[i];
    else { shirt[0] = perm[0]; shirt[2] = perm[1]; shirt[1] = shirt[3] = 0; }
}

static void court_screen(void)
{
    ppu_off();
    set_pal(pal_court);
    assign_shirts();
    vram_adr(0x2000);
    vram_write(court_nt, 1024);
    if (demo) put_text(NTADR(13, 26), "DEMO");
    hud_reset();
    ppu_on_all();
}

static void match_loop(void)
{
    paused = 0;
    for (;;) {
        read_pads();
        if (demo) {
            if (trigs[0] & (PAD_START | PAD_A | PAD_B | PAD_SELECT)) return;
            if (state == ST_GAMEOVER && state_t > 240) return;
        } else if (trigs[0] & PAD_START) {
            paused ^= 1;
            sfx(SFX_PAUSE);
            hud_call(paused ? "PAUSE" : call_text);
        }
        if (!paused) match_update();
        draw_match();
        hud_update();
        snd_update();
        ppu_wait_nmi();
        if (state == ST_GAMEOVER && state_t >= 250 && !(best_of3 && games[0] < 2 && games[1] < 2)) return;
    }
}

void main(void)
{
    snd_init();
    mode = MODE_1P_SINGLES; level = 1; game_to_i = 0; best_of3 = 0; lets_replay = 1;
    for (;;) {
        title_screen();
        game_to = GAME_TO_VALUES[game_to_i];
        match_init(demo ? MODE_EXHIBITION : mode, demo ? 3 : level);
        court_screen();
        match_loop();
    }
}
