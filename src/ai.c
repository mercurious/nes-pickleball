/* The computer players, Phase 1 version: go home, run to the ball, swing when it is in reach, serve on time. */
#include "game.h"

static unsigned char ball_coming(unsigned char s)
{
    if (!ball_live) return 0;
    if (last_hitter == 0xFF) return 0;
    if (TEAM(last_hitter) == TEAM(s)) return 0;
    return 1;
}

void __fastcall__ ai_intent(unsigned char s)
{
    int txp, typ, dy, frames, dv;
    in_dx[s] = 0; in_dy[s] = 0; in_swing[s] = 0;
    if (state == ST_SERVE_DROP) {
        if (s == server_slot && bz <= (int)PX(12)) in_swing[s] = 1;
        return;
    }
    if (state != ST_FLIGHT && state != ST_RALLY) return;
    txp = home_x[s]; typ = home_y[s];
    if (ball_coming(s)) {
        /* where the ball's ground spot will be when it reaches our depth, in pixels */
        dy = (int)(py[s] >> 8) - (int)(by >> 8);
        dv = vy >> 4;
        if ((IS_NEAR(s) && dv > 0) || (!IS_NEAR(s) && dv < 0)) {
            frames = (dy * 16) / dv;
            if (frames < 0) frames = 0;
            if (frames > 90) frames = 90;
            txp = (int)(bx >> 8) + (((vx >> 4) * frames) >> 4);
        } else {
            txp = bx >> 8;
        }
        txp += IS_NEAR(s) ? -8 : 8;                       /* keep the ball on the forehand side */
        if (txp < 20) txp = 20; if (txp > 236) txp = 236;
        /* depth: a ball that has bounced is met 4 px in front; a soft ball still in the air is met at the line */
        if (bounce_side) typ = (int)(by >> 8) + (IS_NEAR(s) ? 4 : -4);
        else if (dv > -24 && dv < 24) typ = IS_NEAR(s) ? NEAR_KIT + 3 : FAR_KIT - 3;
        else typ = py[s] >> 8;
        if (in_reach(s)) {
            if (!((shots == 1 || shots == 2) && bounce_side == 0))
                in_swing[s] = (bz >= (int)PX(12) ? SHOT_HARD : SHOT_SOFT) + 1;
        }
    }
    dy = txp - (int)(px[s] >> 8);
    if (dy > 1) in_dx[s] = 1; else if (dy < -1) in_dx[s] = -1;
    dy = typ - (int)(py[s] >> 8);
    if (dy > 1) in_dy[s] = 1; else if (dy < -1) in_dy[s] = -1;
}
