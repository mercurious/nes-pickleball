/* The score panel (rows 2-4, cols 3-10) and the umpire's call box (row 2, cols 14-23). */
#include "game.h"
#include "text.h"

static char pending_call[12];
static unsigned char call_pending;

static const char *NAME_A[MODE_COUNT] = { "YOU", "YOU", "YOU", "P1 ", "COM" };
static const char *NAME_B[MODE_COUNT] = { "COM", "COM", "COM", "P2 ", "COM" };

static void row(unsigned int adr, unsigned char team, const char *name)
{
    static char line[9];
    unsigned char sc = score[team];
    line[0] = (team == serving_team) ? '*' : ' ';
    line[1] = name[0]; line[2] = name[1]; line[3] = name[2];
    line[4] = ' '; line[5] = ' ';
    line[6] = (sc >= 10) ? '1' : ' ';
    line[7] = '0' + sc % 10;
    line[8] = 0;
    vbuf_text(adr, line);
}

void hud_score(void)
{
    row(NTADR(3, 2), 0, NAME_A[mode]);
    row(NTADR(3, 3), 1, NAME_B[mode]);
    if (doubles) {
        static char srv[9] = "SERVER 1";
        srv[7] = '0' + server_num;
        vbuf_text(NTADR(3, 4), srv);
    } else {
        vbuf_text(NTADR(3, 4), "        ");
    }
}

void __fastcall__ hud_call(const char *s)
{
    unsigned char i = 0;
    while (s[i] && i < 10) { pending_call[i] = s[i]; ++i; }
    while (i < 10) pending_call[i++] = ' ';
    pending_call[10] = 0;
    call_pending = 1;
}

void hud_update(void)
{
    if (hud_dirty) { hud_score(); hud_dirty = 0; }
    if (call_pending) { vbuf_text(NTADR(14, 2), pending_call); call_pending = 0; }
}
