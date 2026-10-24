/* Drawing the match: the ball and its shadow, the players nearest first with their paddles, the server's badge.
   Each player, their paddle and their badge are drawn in the player's own sprite palette (shirt[s]); the ball in
   palette 2, whose shirt colour is its yellow.
   The paddle is its own sprite. In the swing's contact window it is drawn where the ball is, held inside the box
   in_reach allows (src/player.c: up to 18 px aside, so the face's centre up to 15 px plus its half-width), so a
   hit is seen to meet the paddle; before the window it is drawn back, after it across the body. */
#include "game.h"
#include "draw.h"
#include "sprdefs.h"

static unsigned char order[4];
extern unsigned char ct_t[4];
extern signed char ct_dx[4], ct_dy[4];

/* the paddle's face centre for player s, as an offset from the player's feet (x across, y up the screen) */
static void draw_paddle(unsigned char s, unsigned char x, unsigned char y)
{
    signed char side, dx, dy;
    int bdx, bdy;
    unsigned char t = sw_t[s], tile = SPR_PADDLE_V, attr = shirt[s];
    side = (IS_NEAR(s) ^ (t ? sw_side[s] : 0)) ? 1 : -1;       /* the forehand: our right for the near player */
    if (!t) { dx = 9; dy = -16; }                              /* held at the side */
    else if (t < CW_START) { dx = 11; dy = IS_NEAR(s) ? -10 : -22; }   /* drawn back */
    else if (t <= CW_END || ct_t[s]) {                         /* the contact window: to the ball, and held */
        tile = SPR_PADDLE_H;                                   /* there for 4 frames after it is hit */
        if (ct_t[s]) { bdx = ct_dx[s]; bdy = ct_dy[s]; }
        else { bdx = (int)(bx >> 8) - x; bdy = (int)(by >> 8) - (bz >> 8) - y; }
        dx = (side > 0) ? (bdx < 7 ? 7 : bdx > 15 ? 15 : bdx) : (bdx > -7 ? 7 : bdx < -15 ? 15 : -bdx);
        dy = bdy < -30 ? -30 : bdy > 14 ? 14 : bdy;
    } else { dx = -4; dy = -26; side = -side; }                /* the follow-through, across and high */
    if (side < 0) { dx = -dx; attr |= ATTR_HFLIP; }
    if (tile == SPR_PADDLE_H) draw_spr(x + dx - 4, y + dy - 12, tile, attr);
    else draw_spr(x + dx - 3, y + dy - 9, tile, attr);
}

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
    unsigned char i, j, t, s, attr, x, y, flip;
    draw_begin();
    flip = doubles && (frame_count & 1);           /* read once: an NMI during the loop must not change the order */
    x = bx >> 8; y = by >> 8;
    /* the ball's and the shadow's art sit at the bottom of their 8x16 sprites (tools/chr.py pads at the top), so
       their tops are 13 and 14 px up; v0.0.1 drew them 10-12 px below where the ball was */
    draw_spr(x - 4, y - (bz >> 8) - 13, SPR_BALL, 2);
    draw_spr(x - 4, y - 14, SPR_SHADOW, 2);
    for (i = 0; i < 4; ++i) order[i] = i;
    for (i = 1; i < 4; ++i) {                      /* nearest (largest y) first: in front */
        t = order[i]; j = i;
        while (j && py[order[j - 1]] < py[t]) { order[j] = order[j - 1]; --j; }
        order[j] = t;
    }
    for (i = 0; i < 4; ++i) {
        /* doubles: the order flips every other frame, so if a line holds more than 8 sprites they flicker
           instead of one always vanishing */
        s = flip ? order[3 - i] : order[i];
        if (!pactive[s]) continue;
        attr = shirt[s];
        if (sw_t[s] && sw_side[s]) attr |= ATTR_HFLIP;
        x = px[s] >> 8; y = py[s] >> 8;
        draw_paddle(s, x, y);
        draw_frame16(x - 8, y - 31, frame_of(s), attr);
        if (doubles && s == server_slot && (state == ST_CALL || state == ST_SERVE_DROP))
            draw_spr(x - 4, y - 44, server_num == 1 ? SPR_BADGE1 : SPR_BADGE2, shirt[s]);
    }
    draw_end();
}
