/* Drawing the match: the ball and its shadow, the players nearest first, the server's badge. */
#include "game.h"
#include "draw.h"
#include "sprdefs.h"

static unsigned char order[4];

static unsigned char frame_of(unsigned char s)
{
    unsigned char base = IS_NEAR(s) ? SPR_N_STAND : SPR_F_STAND;   /* STAND, RUN1, RUN2, SWING1, SWING2, SERVE1, SERVE2 */
    if (state == ST_SERVE_DROP && s == server_slot) return base + 5 * 8;
    if (pstate[s] == 2) return base + 6 * 8;
    if (sw_t[s]) return base + ((sw_t[s] < CW_START + 2) ? 3 : 4) * 8;
    if (pstate[s] == 1) return base + ((anim_t[s] & 8) ? 1 : 2) * 8;
    return base;
}

void draw_match(void)
{
    unsigned char i, j, t, s, attr, x, y;
    draw_begin();
    x = bx >> 8; y = by >> 8;
    draw_spr(x - 4, y - (bz >> 8) - 3, SPR_BALL, 2);
    draw_spr(x - 4, y - 2, SPR_SHADOW, 2);
    for (i = 0; i < 4; ++i) order[i] = i;
    for (i = 1; i < 4; ++i) {                      /* nearest (largest y) first: in front */
        t = order[i]; j = i;
        while (j && py[order[j - 1]] < py[t]) { order[j] = order[j - 1]; --j; }
        order[j] = t;
    }
    for (i = 0; i < 4; ++i) {
        s = order[i];
        if (!pactive[s]) continue;
        attr = TEAM(s);
        if (sw_t[s] && sw_side[s]) attr |= ATTR_HFLIP;
        x = px[s] >> 8; y = py[s] >> 8;
        draw_frame16(x - 8, y - 31, frame_of(s), attr);
        if (doubles && s == server_slot && (state == ST_CALL || state == ST_SERVE_DROP))
            draw_spr(x - 4, y - 44, server_num == 1 ? SPR_BADGE1 : SPR_BADGE2, 3);
    }
    draw_end();
}
