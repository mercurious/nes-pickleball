/* The players: intent (pad or AI), movement, the swing, contact, the shot model, the serve. */
#include "game.h"
#include "sound.h"

extern unsigned char ai_aim_x[4];
extern unsigned char team_level[2];
unsigned char dbg[8];   /* the last shot: kind, d, L, T, vmag/8, vz/8, z0 px, tries */

static const unsigned char TARGET_X[2][2] = { { 156, 100 }, { 172, 84 } };  /* [team][server on the screen's right] */

static void human_intent(unsigned char s, unsigned char pad, unsigned char trig)
{
    in_dx[s] = 0; in_dy[s] = 0; in_swing[s] = 0;
    if (pad & PAD_LEFT) in_dx[s] = -1;
    if (pad & PAD_RIGHT) in_dx[s] = 1;
    if (pad & PAD_UP) in_dy[s] = -1;
    if (pad & PAD_DOWN) in_dy[s] = 1;
    if (trig & PAD_A) in_swing[s] = SHOT_HARD + 1;
    else if (trig & PAD_B) in_swing[s] = ((pad & PAD_DOWN) ? SHOT_LOB : SHOT_SOFT) + 1;
}

static unsigned char ball_on_our_side(unsigned char s)
{
    return IS_NEAR(s) ? (by > PX(NET_Y)) : (by < PX(NET_Y));
}

/* the ball is in reach of the paddle: at most 18 px aside, from 6 px behind to 14 px toward the net, under the head */
unsigned char __fastcall__ in_reach(unsigned char s)
{
    int d, lead;
    if (!ball_live || bz >= (int)PX(BALL_OVERHEAD) || !ball_on_our_side(s)) return 0;
    if (last_hitter != 0xFF && TEAM(last_hitter) == TEAM(s)) return 0;
    d = (int)(bx - px[s]) >> 8; if (d < -18 || d > 18) return 0;
    d = (int)(by - py[s]) >> 8; if (d < -40 || d > 40) return 0;
    lead = IS_NEAR(s) ? -d : d;
    if (lead < -6 || lead > 14) return 0;
    return 1;
}

/* the shot model: speed by kind, the lift that lands it L px past the net, slowed until it clears the net */
void __fastcall__ do_hit(unsigned char s, unsigned char kind)
{
    unsigned char d, L, tries, tn, zn, ev;
    int vmag, z0, aim, lead;
    unsigned int T;
    unsigned char volley = (bounce_side == 0);
    if ((shots == 1 || shots == 2) && volley) { end_rally(R_TWO_BOUNCE, TEAM(s)); return; }
    if (volley && in_kitchen(s)) { end_rally(R_KITCHEN, TEAM(s)); return; }
    z0 = bz; if (z0 < (int)PX(2)) z0 = PX(2);
    d = IS_NEAR(s) ? ((py[s] - PX(NET_Y)) >> 8) : ((PX(NET_Y) - py[s]) >> 8);
    if (d < 4) d = 4;
    if (kind == SHOT_HARD) {
        if (bz >= (int)PX(14)) { vmag = 832; ev = EV_SMASH; } else { vmag = 704; ev = volley ? EV_VOLLEY : EV_DRIVE; }
        L = 52;
    } else if (kind == SHOT_LOB) {
        vmag = 282; L = 56; ev = EV_LOB;
    } else {
        if (d <= 26) { vmag = 230; L = 12; } else { vmag = 358; L = 14; }
        ev = EV_DINK;
    }
    if (!phuman[s]) {                                    /* the computer misjudges, less at higher levels */
        static const unsigned char ERR[6] = { 10, 10, 8, 6, 4, 2 };
        unsigned char e = ERR[team_level[TEAM(s)]];
        L += (rand8() % (2 * e + 1)) - e;
    }
    for (tries = 0; tries < 4; ++tries) {
        T = ((unsigned int)(d + L) << 8) / (unsigned int)vmag; if (T < 4) T = 4;
        if (T > 120) T = 120;
        tn = ((unsigned int)d << 8) / (unsigned int)vmag;
        vz = 10 * (int)T - z0 / (int)T;
        zn = (z0 >> 8) + (((vz >> 4) * tn) >> 4) - (((((int)tn * tn) >> 4) * 5) >> 3);
        if (zn >= NET_H + 2 || vmag <= 160) break;
        vmag -= (kind == SHOT_HARD) ? 128 : 48;
    }
    vmag += ((vmag >> 3) * (int)T) >> 6;                /* drag eats about T/512 of the speed over the flight */
    dbg[0] = kind; dbg[1] = d; dbg[2] = L; dbg[3] = T; dbg[4] = vmag >> 3; dbg[5] = vz >> 3; dbg[6] = z0 >> 8; dbg[7] = tries;
    lead = IS_NEAR(s) ? ((int)(py[s] - by) >> 8) : ((int)(by - py[s]) >> 8);
    aim = 4 - lead;                                      /* early (ball still in front) aims one way, late the other */
    if (aim > 10) aim = 10; if (aim < -10) aim = -10;
    if (!phuman[s]) aim += (rand8() & 3) - 1;
    if (phuman[s]) {
        vx = (aim * vmag) / 32;
        if (sw_side[s]) vx = -vx;                        /* backhand: mirrored */
        if (!IS_NEAR(s)) vx = -vx;                       /* the far player's racket side is our left */
        vx += ((int)(PX(CENTER_X) - bx) / (int)T) / 2;   /* every shot drifts half-way back toward the centre line */
    } else {
        int lim = (vmag >> 1) + (vmag >> 2);              /* no sharper than 37 degrees */
        vx = (int)(PX(ai_aim_x[s]) - bx) / (int)T + (rand8() & 31) - 16;
        if (vx > lim) vx = lim;
        if (vx < -lim) vx = -lim;
    }
    vy = IS_NEAR(s) ? -vmag : vmag;
    if (bz < (int)PX(2)) bz = PX(2);
    bounce_side = 0;
    last_hitter = s;
    ++shots; ++rally_len;
    sfx(kind == SHOT_HARD ? SFX_HIT_HARD : SFX_HIT_SOFT);
    if (shots == 2) event(EV_RETURN, s);
    else if (shots == 3) event(EV_THIRD_SHOT, s);
    else event(ev, s);
    if (rally_len >= 10 && (rally_len % 10) == 0) event(EV_LONG_RALLY, rally_len);
}

void __fastcall__ serve_hit(unsigned char s)
{
    unsigned char d = 72, L;
    unsigned int T;
    int z0, tx;
    z0 = bz; if (z0 < (int)PX(1)) z0 = PX(1);
    L = 24 + ((z0 >> 8) * 3) / 2;                       /* a ball hit high goes deep; above the waist it goes long */
    if (!phuman[s]) L += (rand8() % (9 - team_level[TEAM(s)])) - (4 - team_level[TEAM(s)] / 2);
    T = ((unsigned int)(d + L) << 8) / 512;
    vz = 10 * (int)T - z0 / (int)T;
    tx = TARGET_X[TEAM(s)][(px[s] >> 8) >= CENTER_X];
    tx += (rand8() & 15) - 8;
    vx = (int)(PX(tx) - bx) / (int)T;
    vx += ((vx >> 3) * (int)T) >> 6;
    vy = 512 + ((64 * (int)T) >> 6);
    if (IS_NEAR(s)) vy = -vy;
    bounce_side = 0;
    last_hitter = s;
    shots = 1; rally_len = 1;
    state = ST_FLIGHT; state_t = 0;
    pstate[s] = 2; anim_t[s] = 0;
    sfx(SFX_SERVE);
    event(EV_SERVE, s);
}

static void move_player(unsigned char s)
{
    int sp = pspeed[s];
    unsigned char x, y;
    if (sw_t[s]) sp >>= 1;
    if (in_dx[s] > 0) px[s] += sp; else if (in_dx[s] < 0) px[s] -= sp;
    if (in_dy[s] > 0) py[s] += sp; else if (in_dy[s] < 0) py[s] -= sp;
    if (px[s] < PX(20)) px[s] = PX(20);
    if (px[s] > PX(236)) px[s] = PX(236);
    if (IS_NEAR(s)) {
        if (py[s] < PX(NET_Y + 6)) py[s] = PX(NET_Y + 6);
        if (py[s] > PX(226)) py[s] = PX(226);
    } else {
        if (py[s] > PX(NET_Y - 6)) py[s] = PX(NET_Y - 6);
        if (py[s] < PX(46)) py[s] = PX(46);
    }
    x = in_dx[s] != 0; y = in_dy[s] != 0;
    if (x || y) { ++anim_t[s]; pstate[s] = 1; } else if (pstate[s] == 1) pstate[s] = 0;
}

void players_update(void)
{
    unsigned char s, v;
    for (s = 0; s < 4; ++s) {
        if (!pactive[s]) continue;
        if (phuman[s]) human_intent(s, pads[phuman[s] - 1], trigs[phuman[s] - 1]);
        else ai_intent(s);
        if (mom_t[s]) --mom_t[s];
        if (state == ST_CALL || (state == ST_SERVE_DROP && s == server_slot)) {
            in_dx[s] = 0; in_dy[s] = 0;             /* nobody moves before the serve; the server stands still */
        }
        if (state == ST_SERVE_DROP && s == server_slot) {
            if (in_swing[s] && bz > (int)PX(1)) { serve_hit(s); return; }
            continue;
        }
        if (state == ST_CALL || state == ST_SERVE_DROP) { in_swing[s] = 0; }
        move_player(s);
        if (sw_t[s]) {
            ++sw_t[s];
            if (sw_t[s] >= CW_START && sw_t[s] <= CW_END && in_reach(s)) {
                v = (bounce_side == 0);
                do_hit(s, sw_kind[s]);
                if (state == ST_DEAD) return;
                mom_t[s] = v ? 12 : 0;               /* after a volley the momentum rule holds for 12 frames */
                sw_t[s] = CW_END + 1;                /* one contact per swing */
            }
            if (sw_t[s] > SWING_LEN) sw_t[s] = 0;
        } else if (in_swing[s] && state != ST_DEAD && state != ST_CALL) {
            sw_t[s] = 1;
            sw_kind[s] = in_swing[s] - 1;
            sw_side[s] = IS_NEAR(s) ? (bx < px[s]) : (bx > px[s]);   /* 0 forehand, 1 backhand */
        }
        if (pstate[s] == 2 && ++anim_t[s] > 12) pstate[s] = 0;
    }
}
