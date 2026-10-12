/* The ball: flight, drag, the net, bounces, and what a bounce means (in, out, short, double). */
#include "game.h"
#include "sound.h"

static unsigned int prev_by;

static void judge_bounce(void)
{
    unsigned char x = bx >> 8, y = by >> 8, side, in, ht;
    side = (y < NET_Y) ? 1 : 0;                       /* the team whose court the ball is in */
    ht = TEAM(last_hitter);
    if (bounce_side == 1) {
        in = (y >= FAR_Y && y <= NEAR_Y && x >= side_l[y - FAR_Y] && x <= side_r[y - FAR_Y]);
        if (side == ht) { end_rally(R_NET, ht); return; }        /* bounced on the hitter's own side */
        if (!in) { end_rally(R_OUT, ht); return; }
        if (shots == 1) {                                         /* a serve: past the kitchen, the right box */
            if (side == 1 ? (y >= FAR_KIT) : (y <= NEAR_KIT)) { end_rally(R_SHORT, ht); return; }
            if (serve_to_right ? (x < CENTER_X) : (x > CENTER_X)) { end_rally(R_WRONG_COURT, ht); return; }
            state = ST_RALLY;
        }
    } else if (bounce_side >= 2) {
        end_rally(R_DOUBLE_BOUNCE, side);
    }
}

void ball_update(void)
{
    if (state == ST_SERVE_DROP) {                     /* the ball drops from the server's hand */
        bz += vz; vz -= G_FP;
        if (bz <= 0) { bz = 0; end_rally(R_SERVE_FAULT, serving_team); }
        return;
    }
    prev_by = by;
    bx += vx; by += vy; bz += vz; vz -= G_FP;
    /* drag, the same for both directions (an arithmetic shift of a negative value rounds away from zero) */
    if (vx < 0) vx += (-vx) >> 7; else vx -= vx >> 7;
    if (vy < 0) vy += (-vy) >> 7; else vy -= vy >> 7;
    if ((prev_by < PX(NET_Y)) != (by < PX(NET_Y))) {  /* crossing the net line */
        if (bz < (int)PX(NET_H) - 256) {
            by = prev_by; vx = 0; vy = 0; vz = 0;
            end_rally(R_NET, TEAM(last_hitter));
            return;
        }
        if (bz < (int)PX(NET_H) + 128) {              /* clipped the tape: a let on a serve, a net cord otherwise */
            vy >>= 1; vz = 160;
            sfx(SFX_NETCORD);
            if (shots == 1 && lets_replay) { end_rally(R_LET, 0); return; }
        }
    }
    if (bz <= 0 && vz < 0) {
        bz = 0;
        vz = -(vz - (vz >> 2) - (vz >> 3));
        if (vz < 64) vz = 0;
        vx -= vx >> 3; vy -= vy >> 3;
        if (vz) snd_bounce(by < PX(NET_Y));
        ++bounce_side;
        judge_bounce();
        return;
    }
    if (by < PX(2) || by > PX(236) || bx < PX(2) || bx > PX(252)) {
        if (bounce_side == 0) end_rally(R_OUT, TEAM(last_hitter));
        else end_rally(R_PASSED, (by < PX(NET_Y)) ? 1 : 0);
    }
}
