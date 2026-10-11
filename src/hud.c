/* The score panel (rows 2-4, cols 3-10), the umpire's call box (row 2, cols 14-23), and speech bubbles:
   a phrase written over the court beside a player for 45 frames, then the court tiles are put back. */
#include "game.h"
#include "text.h"

extern const unsigned char court_nt[];

static char pending_call[12];
static unsigned char call_pending;
static unsigned char bub_t, bub_len;
static unsigned int bub_adr;

static const char *NAME_A[MODE_COUNT] = { "YOU", "YOU", "YOU", "P1 ", "RED" };
static const char *NAME_B[MODE_COUNT] = { "COM", "COM", "COM", "P2 ", "BLU" };
static const char *PHRASES[] = { "", "MINE!", "YOURS!", "SWITCH!", "GOT IT", "BOUNCE IT", "NICE!", "SORRY", "OUT!",
                                 "GO!", "BACK!", "UP!", "MIDDLE!", "STAY!", "NO!", "GOOD", "HELP!" };
#define PHRASE_COUNT 17

static void row(unsigned int adr, unsigned char team, const char *name)
{
    static char line[9];
    unsigned char sc = score[team];
    line[0] = (team == serving_team) ? '*' : ' ';
    line[1] = name[0]; line[2] = name[1]; line[3] = name[2];
    line[4] = ' '; line[5] = ' ';
    line[6] = (sc >= 10) ? '0' + sc / 10 : ' ';
    line[7] = '0' + sc % 10;
    line[8] = 0;
    vbuf_text(adr, line);
}

void hud_score(void)
{
    static char srv[9] = "SERVER 1";
    static char gm[9] = "GAMES 0-0";
    row(NTADR(3, 2), 0, NAME_A[mode]);
    row(NTADR(3, 3), 1, NAME_B[mode]);
    if (best_of3) { gm[6] = '0' + games[0]; gm[8] = '0' + games[1]; vbuf_text(NTADR(3, 4), gm); }
    else if (doubles) { srv[7] = '0' + server_num; vbuf_text(NTADR(3, 4), srv); }
    else vbuf_text(NTADR(3, 4), "        ");
}

void __fastcall__ hud_call(const char *s)
{
    unsigned char i = 0;
    while (s[i] && i < 10) { pending_call[i] = s[i]; ++i; }
    while (i < 10) pending_call[i++] = ' ';
    pending_call[10] = 0;
    call_pending = 1;
}

void hud_reset(void)
{
    bub_t = 0; call_pending = 0;
}

void __fastcall__ bubble(unsigned char slot, unsigned char code)
{
    unsigned char tx, ty, n;
    const char *s;
    if (!code || code >= PHRASE_COUNT || !pactive[slot]) return;
    if (bub_t) vbuf_run(bub_adr, court_nt + (bub_adr - 0x2000), bub_len);   /* put the old one back first */
    s = PHRASES[code];
    for (n = 0; s[n]; ++n) ;
    if (IS_NEAR(slot)) {
        tx = ((px[slot] >> 8) >> 3); ty = (((py[slot] >> 8) - 44) >> 3);
        tx = (tx > n / 2) ? tx - n / 2 : 0;
    } else {
        tx = ((px[slot] >> 8) >> 3) + 2; ty = (((py[slot] >> 8) - 30) >> 3);
    }
    if (ty < 5) ty = 5;
    if (ty > 27) ty = 27;
    if (tx + n > 31) tx = 31 - n;
    bub_adr = NTADR(tx, ty); bub_len = n; bub_t = 45;
    vbuf_text(bub_adr, s);
}

void hud_update(void)
{
    if (hud_dirty) { hud_score(); hud_dirty = 0; }
    if (call_pending) { vbuf_text(NTADR(14, 2), pending_call); call_pending = 0; }
    if (bub_t) {
        --bub_t;
        if (!bub_t) vbuf_run(bub_adr, court_nt + (bub_adr - 0x2000), bub_len);
    }
    if (mb_bubble) { bubble(mb_bubble_slot & 3, mb_bubble); mb_bubble = 0; }
}
