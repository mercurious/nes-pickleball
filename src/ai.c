/* The computer players: five levels, pickleball strategy, doubles team logic, directives from the mailbox.
   Level 1 stays back and pushes the ball over; 2 comes in behind its return; 3 drops the third shot and dinks;
   4 speeds up any high ball and lobs now and then; 5 also aims at the gap and poaches the middle. */
#include "game.h"
#include "hud.h"

static const int LV_SPEED[6] = { 240, 240, 270, 300, 330, 372 };
static const unsigned char LV_REACT[6] = { 16, 16, 12, 8, 4, 2 };
static const unsigned char LV_SERVE_Z[6] = { 10, 10, 11, 12, 12, 13 };

static unsigned char ai_t[4];          /* reaction frames left before this player moves to a new ball */
static unsigned char own[2];           /* the slot that takes the incoming ball, per team */
static unsigned char seen_hit[4];      /* shots count at each player's last decision */
static unsigned char said_bounce, said_mine;
unsigned char ai_aim_x[4];             /* where a computer's shot is aimed (screen x) */
unsigned char ai_mid[4];               /* 1: takes the middle, 2: leaves it (directive) */

unsigned char team_level[2];
static unsigned char tier_of(unsigned char s) { unsigned char l = team_level[TEAM(s)]; return l ? l : 1; }

static unsigned char crossing_x(unsigned char s)
{
    int dy, dv, frames, txp;
    dy = (int)(py[s] >> 8) - (int)(by >> 8);
    dv = vy >> 4;
    if (dv == 0) return bx >> 8;
    frames = (dy * 16) / dv;
    if (frames < 0) frames = 0;
    if (frames > 90) frames = 90;
    txp = (int)(bx >> 8) + (((vx >> 4) * frames) >> 4);
    if (txp < 8) txp = 8;
    if (txp > 248) txp = 248;
    return txp;
}

/* the ball's height when it reaches the row y (px); negative when it lands first */
static int height_at_row(unsigned char y)
{
    int dy, dv, t;
    dy = (int)y - (int)(by >> 8);
    dv = vy >> 4;
    if (dv == 0) return -1;
    t = (dy * 16) / dv;
    if (t < 0 || t > 120) return -1;
    return (bz >> 8) + (((vz >> 4) * t) >> 4) - (((((int)t * t) >> 4) * 5) >> 3);
}

/* when the other side hits, the team decides who takes the ball and whether it is going long */
static void team_decide(unsigned char team)
{
    unsigned char a = team << 1, b = a + 1, cx, s, mid;
    int h;
    cx = crossing_x(a);
    mid = (cx > CENTER_X - 24 && cx < CENTER_X + 24);
    if (!pactive[b]) { own[team] = a; }
    else {
        unsigned char da = (px[a] >> 8) > cx ? (px[a] >> 8) - cx : cx - (px[a] >> 8);
        unsigned char db = (px[b] >> 8) > cx ? (px[b] >> 8) - cx : cx - (px[b] >> 8);
        s = (da <= db) ? a : b;
        if (mid) {                                   /* the middle goes to the forehand, or as told */
            if (ai_mid[a] == 1 || ai_mid[b] == 2) s = a;
            else if (ai_mid[b] == 1 || ai_mid[a] == 2) s = b;
            else s = (team == 0) ? ((px[a] < px[b]) ? a : b) : ((px[a] > px[b]) ? a : b);
            if (!phuman[s] && !said_mine) { event(EV_MINE, s); bubble(s, 1); said_mine = 1; }
        }
        own[team] = s;
    }
    if (tier_of(a) >= 2 && !said_bounce) {            /* a ball that will fly past the baseline: let it go */
        h = height_at_row(team == 0 ? NEAR_Y + 2 : FAR_Y - 2);
        if (h > 14 && bounce_side == 0 && tier_of(a) >= 3) {
            s = (own[team] == a) ? b : a;
            if (!pactive[s]) s = a;
            event(EV_BOUNCE_IT, s); bubble(s, 5); said_bounce = 2;
        }
    }
}

static unsigned char pick_shot(unsigned char s, unsigned char d)
{
    unsigned char t = tier_of(s), r = rand8();
    if (mb_directive == 6 && phuman[PARTNER(s)]) return SHOT_SOFT;
    if (mb_directive == 7 && phuman[PARTNER(s)] && bz >= (int)PX(8)) return SHOT_HARD;
    if (bz >= (int)PX(14)) return (t >= 2) ? SHOT_HARD : SHOT_SOFT;           /* a high ball: put it away */
    if (shots == 2) return (t >= 3) ? SHOT_SOFT : (t == 2 ? SHOT_HARD : SHOT_SOFT);  /* the third shot */
    if (d <= 26) return (t >= 4 && bz >= (int)PX(9) && r < 90) ? SHOT_HARD : SHOT_SOFT;  /* at the line: dink */
    if (t >= 4 && r < 24 && d > 40) return SHOT_LOB;
    if (t >= 2 && bz >= (int)PX(9)) return SHOT_HARD;
    return (t >= 3 && r < 128) ? SHOT_SOFT : (t == 1 ? SHOT_SOFT : SHOT_HARD);
}

static unsigned char adist(unsigned char a, unsigned char b) { return a > b ? a - b : b - a; }

/* where to put the shot: the gap between the opponents (their middle or either sideline, whichever is farthest
   from both), from level 3 up; a scatter around the centre below that. The sidelines are taken at the row
   where this shot lands: deep for a drive or a lob, in the kitchen for a soft shot (the far court narrows
   toward its baseline, the near court widens) */
static void aim(unsigned char s, unsigned char deep)
{
    unsigned char team = TEAM(s), t = tier_of(s), o1 = (team ^ 1) << 1, o2, ly, lx, rx, mx, x1, x2, dl, dr, dm, pick;
    o2 = o1 + 1;
    x1 = px[o1] >> 8; x2 = pactive[o2] ? (px[o2] >> 8) : x1;
    if (t <= 2) { ai_aim_x[s] = CENTER_X - 20 + (rand8() % 41); return; }
    if (deep) ly = (team == 0) ? FAR_Y + 12 : NEAR_Y - 12;
    else ly = (team == 0) ? FAR_KIT + 8 : NEAR_KIT - 8;
    lx = side_l[ly - FAR_Y] + 12; rx = side_r[ly - FAR_Y] - 12;
    mx = (unsigned char)(((unsigned int)x1 + x2) >> 1);
    dl = adist(lx, x1); if (adist(lx, x2) < dl) dl = adist(lx, x2);
    dr = adist(rx, x1); if (adist(rx, x2) < dr) dr = adist(rx, x2);
    dm = adist(mx, x1); if (adist(mx, x2) < dm) dm = adist(mx, x2);
    pick = mx;
    if (dl > dm && dl >= dr) pick = lx; else if (dr > dm) pick = rx;
    if (t < 5 && (rand8() & 3) == 0) pick = (rand8() & 1) ? lx : rx;   /* a little unpredictable below champion */
    ai_aim_x[s] = pick + (rand8() & 7) - 4;
}

void ai_set_levels(unsigned char lv)
{
    team_level[0] = lv; team_level[1] = lv;
}

void ai_new_rally(void)
{
    unsigned char i;
    said_bounce = 0; said_mine = 0;
    for (i = 0; i < 4; ++i) { seen_hit[i] = 0xFF; ai_aim_x[i] = CENTER_X; ai_t[i] = 0; }
    own[0] = 0; own[1] = 2;
}

void __fastcall__ ai_intent(unsigned char s)
{
    int txp, typ, d;
    unsigned char team = TEAM(s), t = tier_of(s), hitter, line_y, base_y, coming;
    in_dx[s] = 0; in_dy[s] = 0; in_swing[s] = 0;
    pspeed[s] = LV_SPEED[t];
    if (state == ST_SERVE_DROP) {
        if (s == server_slot && bz <= (int)PX(LV_SERVE_Z[t]) + (int)(rand8() & 0x7F)) in_swing[s] = 1;
        return;
    }
    if (state != ST_FLIGHT && state != ST_RALLY) return;
    coming = ball_live && last_hitter != 0xFF && TEAM(last_hitter) != team;
    if (shots != seen_hit[s]) {                     /* somebody hit: a new decision */
        seen_hit[s] = shots;
        if (coming) { if ((s & 1) == 0 || !pactive[s & 2]) team_decide(team); ai_t[s] = LV_REACT[t]; }
    }
    line_y = IS_NEAR(s) ? NEAR_KIT + 3 : FAR_KIT - 3;
    base_y = IS_NEAR(s) ? NEAR_Y + 2 : FAR_Y - 2;
    hitter = (own[team] == s);
    txp = home_x[s]; typ = home_y[s];
    if (coming && hitter && ai_t[s]) { --ai_t[s]; return; }
    if (coming && hitter) {
        txp = crossing_x(s);
        txp += IS_NEAR(s) ? -8 : 8;                 /* the ball on the forehand side */
        d = (int)(IS_NEAR(s) ? (by >> 8) : (NET_Y - (by >> 8)));
        if (bounce_side) typ = (int)(by >> 8) + (IS_NEAR(s) ? 4 : -4);
        else if (vy > -400 && vy < 400) typ = line_y;        /* a soft ball: meet it at the line */
        else typ = py[s] >> 8;
        if (IS_NEAR(s) && typ < line_y && !bounce_side) typ = line_y;   /* never volley from the kitchen */
        if (!IS_NEAR(s) && typ > line_y && !bounce_side) typ = line_y;
        if (said_bounce == 2 && t >= 2 && bounce_side == 0 && !(shots == 1 || shots == 2)) {
            /* letting a long ball go: step back out of its way */
            typ = base_y;
        } else if (in_reach(s)) {
            if (!((shots == 1 || shots == 2) && bounce_side == 0) && !(bounce_side == 0 && in_kitchen(s))) {
                in_swing[s] = pick_shot(s, IS_NEAR(s) ? ((py[s] >> 8) - NET_Y) : (NET_Y - (py[s] >> 8))) + 1;
                aim(s, in_swing[s] != SHOT_SOFT + 1);
            }
        }
    } else {
        /* not hitting: the serving team waits at the baseline for the return to bounce, then everyone
           goes to the line from level 2 on; the partner covers the other half */
        unsigned char partner = PARTNER(s), stay_back = (team == serving_team && shots < 3);
        if (mb_directive == 3 && phuman[partner]) stay_back = 1;
        if (mb_directive == 4 && phuman[partner]) stay_back = 0;
        typ = (t >= 2 && !stay_back) ? line_y : base_y;
        if (t == 1) typ = base_y;
        if (pactive[partner]) {
            /* cover the other half, a quarter of the court's width at this depth from the centre */
            unsigned char row = py[s] >> 8, off;
            if (row < FAR_Y) row = FAR_Y; if (row > NEAR_Y) row = NEAR_Y;
            off = (side_r[row - FAR_Y] - side_l[row - FAR_Y]) >> 2;
            if (coming) txp = (px[partner] >> 8) < CENTER_X ? CENTER_X + off : CENTER_X - off;
            else txp = home_x[s];
            if (mb_directive == 5 && phuman[partner]) txp = (px[partner] >> 8) < CENTER_X ? CENTER_X + off : CENTER_X - off;
        } else if (!coming) {
            txp = CENTER_X + (IS_NEAR(s) ? -8 : 8);
        }
    }
    if (txp < 20) txp = 20; if (txp > 236) txp = 236;
    d = txp - (int)(px[s] >> 8);
    if (d > 1) in_dx[s] = 1; else if (d < -1) in_dx[s] = -1;
    d = typ - (int)(py[s] >> 8);
    if (d > 1) in_dy[s] = 1; else if (d < -1) in_dy[s] = -1;
}
