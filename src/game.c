/* The match: states, positions, the serving sequence, scoring, the umpire's calls, the mailbox. */
#include "game.h"
#include "text.h"
#include "sound.h"

#pragma bss-name(push, "ZEROPAGE")
unsigned int bx, by;
int bz, vx, vy, vz;
unsigned char ball_live, bounce_side, shots, last_hitter, serve_to_right;
unsigned int px[4], py[4];
unsigned char state, state_t;
#pragma bss-name(pop)

unsigned char pactive[4], phuman[4], pstate[4], sw_t[4], sw_kind[4], sw_side[4], anim_t[4], mom_t[4];
int pspeed[4];
signed char in_dx[4], in_dy[4];
unsigned char in_swing[4];
unsigned char home_x[4], home_y[4];
unsigned char mode, level, doubles;
unsigned char score[2], games[2], serving_team, server_num, server_slot, receiver_slot, start_server[2];
unsigned char game_to, best_of3, lets_replay, dead_reason, point_winner, rally_len, serve_t;
unsigned char pads[2], trigs[2];
unsigned char hud_dirty, call_dirty;
char call_text[12];
unsigned char sidecar;                       /* a sidecar announced itself (MB_CALL = $A5): it calls the score */

#pragma bss-name(push, "MAILBOX")
unsigned char mb_magic, mb_version, mb_state, mb_score_a, mb_score_b, mb_server, mb_mode, mb_event_n,
    mb_event, mb_event_arg, mb_call, mb_directive, mb_chatter, mb_bubble, mb_bubble_slot, mb_reserved;
#pragma bss-name(pop)

static const unsigned char SIDE_X_A[2] = { 84, 172 };   /* team A: left, right (screen x) */
static const unsigned char SIDE_X_B[2] = { 156, 100 };  /* team B: their left (our right), their right (our left) */

void __fastcall__ event(unsigned char code, unsigned char arg)
{
    mb_event = code;
    mb_event_arg = arg;
    ++mb_event_n;
}

/* the slot that stands on the team's right by its score (doubles), or its only player */
static unsigned char right_slot(unsigned char team)
{
    unsigned char s = start_server[team];
    if (!doubles) return s;
    return (score[team] & 1) ? PARTNER(s) : s;
}

static unsigned char slot_is_right(unsigned char s)
{
    return right_slot(TEAM(s)) == s;
}

void match_init(unsigned char m, unsigned char lv)
{
    unsigned char i;
    mode = m; level = lv;
    ai_set_levels(lv);
    doubles = (m == MODE_1P_DOUBLES || m == MODE_2P_DOUBLES || m == MODE_EXHIBITION);
    for (i = 0; i < 4; ++i) {
        pactive[i] = doubles ? 1 : ((i & 1) == 0);
        phuman[i] = 0;
        pstate[i] = 0; sw_t[i] = 0; anim_t[i] = 0; mom_t[i] = 0;
        pspeed[i] = 288;
    }
    if (m != MODE_EXHIBITION) phuman[0] = 1;
    if (m == MODE_2P_DOUBLES) phuman[1] = 2;
    if (m == MODE_2P_SINGLES) phuman[2] = 2;
    score[0] = score[1] = 0; games[0] = games[1] = 0;
    start_server[0] = 0; start_server[1] = 2;
    serving_team = 0;
    server_num = 2;                              /* the starting server serves as "2" */
    server_slot = start_server[0];
    ball_live = 0; shots = 0; bounce_side = 0; last_hitter = 0xFF;
    state = ST_PRESERVE; state_t = 0;
    mb_magic = 0x5B; mb_version = 1; mb_directive = 0; mb_bubble = 0;
    if (mb_call == 0xA5) sidecar = 1;          /* the announcement is kept as a flag across matches */
    mb_call = 0;
    hud_dirty = 1;
}

static void place_players(void)
{
    unsigned char s, t, r, right, other;
    unsigned char srv_right = slot_is_right(server_slot);
    r = serving_team ^ 1;
    receiver_slot = doubles ? (srv_right ? right_slot(r) : PARTNER(right_slot(r))) : start_server[r];
    for (s = 0; s < 4; ++s) {
        if (!pactive[s]) continue;
        t = TEAM(s);
        if (doubles) right = slot_is_right(s);
        else right = (t == serving_team) ? srv_right : srv_right;   /* singles: both by the server's score */
        if (t == 0) {
            home_x[s] = SIDE_X_A[right];
            if (s == server_slot) home_y[s] = NEAR_Y + 8;
            else if (s == receiver_slot) home_y[s] = NEAR_Y + 2;
            else if (t == serving_team) home_y[s] = NEAR_Y + 4;
            else home_y[s] = NEAR_KIT + 6;
        } else {
            home_x[s] = SIDE_X_B[right];
            if (s == server_slot) home_y[s] = FAR_Y - 8;
            else if (s == receiver_slot) home_y[s] = FAR_Y - 2;
            else if (t == serving_team) home_y[s] = FAR_Y - 4;
            else home_y[s] = FAR_KIT - 6;
        }
        px[s] = PX(home_x[s]); py[s] = PX(home_y[s]);
        pstate[s] = 0; sw_t[s] = 0; mom_t[s] = 0; in_dx[s] = 0; in_dy[s] = 0; in_swing[s] = 0;
    }
    other = srv_right;
    serve_to_right = (TEAM(server_slot) == 0) ? !other : other;   /* the half (screen x >= 128) the serve must land in */
}

void __fastcall__ fmt_call(unsigned char with_server)
{
    unsigned char i = 0, a = score[serving_team], b = score[serving_team ^ 1];
    if (a >= 10) call_text[i++] = '0' + a / 10;
    call_text[i++] = '0' + a % 10;
    call_text[i++] = '-';
    if (b >= 10) call_text[i++] = '0' + b / 10;
    call_text[i++] = '0' + b % 10;
    if (with_server) { call_text[i++] = '-'; call_text[i++] = '0' + server_num; }
    call_text[i] = 0;
}

static void start_serve(void)
{
    place_players();
    ball_live = 0; shots = 0; bounce_side = 0; last_hitter = 0xFF; rally_len = 0;
    bx = IS_NEAR(server_slot) ? px[server_slot] - PX(6) : px[server_slot] + PX(6);
    by = IS_NEAR(server_slot) ? py[server_slot] - PX(6) : py[server_slot] + PX(6);
    bz = PX(30); vx = 0; vy = 0; vz = 0;
    fmt_call(doubles);
    hud_call(call_text);
    ai_new_rally();
    event(EV_SCORE_CALL, server_slot);
    if (mb_call == 0xA5) sidecar = 1;
    mb_call = 1;                                 /* "call the score now"; a sidecar answers with 2 */
    if (!sidecar) voice_call();                  /* no sidecar: the cartridge calls it */
    state = ST_CALL; state_t = 0;
    hud_dirty = 1;
}

void end_rally(unsigned char reason, unsigned char loser_team)
{
    static const char *WORDS[] = { "", "OUT!", "NET", "LET", "SHORT!", "WRONG COURT", "FAULT", "TWO BOUNCE",
                                   "", "KITCHEN!", "FAULT", "NET TOUCH", "", "GAME" };
    static const unsigned char EVS[] = { EV_NONE, EV_FAULT_OUT, EV_FAULT_NET, EV_FAULT_LET, EV_FAULT_SHORT,
                                         EV_FAULT_WRONG_COURT, EV_FAULT_FOOT, EV_FAULT_TWO_BOUNCE,
                                         EV_FAULT_DOUBLE_BOUNCE, EV_FAULT_KITCHEN, EV_FAULT_BODY,
                                         EV_FAULT_NET_TOUCH, EV_PASSED, EV_GAME_OVER };
    ball_live = 0;
    dead_reason = reason;
    point_winner = loser_team ^ 1;
    state = ST_DEAD; state_t = 0;
    event(EVS[reason], loser_team);
    if (WORDS[reason][0]) hud_call(WORDS[reason]);
    else hud_call(point_winner == serving_team ? "POINT" : (doubles && server_num == 1 ? "2ND SERVER" : "SIDE OUT"));
    if (reason == R_LET) jingle(JG_NONE);
    else if (reason == R_PASSED || reason == R_DOUBLE_BOUNCE) { jingle(point_winner == serving_team ? JG_POINT : JG_SIDEOUT); if (rally_len >= 6) applause(70); }
    else jingle(JG_FAULT);
    if (!sidecar) {
        if (reason == R_LET) voice_word(VW_LET);
        else if (reason == R_PASSED || reason == R_DOUBLE_BOUNCE) {
            if (point_winner != serving_team) voice_word(doubles && server_num == 1 ? VW_SECOND_SERVER : VW_SIDE_OUT);
        }
        else if (reason == R_OUT) voice_word(VW_OUT);
        else if (reason == R_KITCHEN) voice_word(VW_KITCHEN);
        else voice_word(VW_FAULT);
    }
}

static void apply_point(void)
{
    unsigned char w = point_winner, o;
    if (dead_reason == R_LET) { start_serve(); return; }
    if (w == serving_team) {
        ++score[w];
        event(EV_POINT_WON, w);
        o = w ^ 1;
        if (score[w] >= game_to && (int)score[w] - (int)score[o] >= 2) {
            ++games[w];
            hud_call("GAME");
            jingle(JG_GAME); applause(120);
            if (!sidecar) voice_word(VW_GAME);
            event(EV_GAME_OVER, w);
            state = ST_GAMEOVER; state_t = 0; hud_dirty = 1;
            return;
        }
    } else {
        if (doubles && server_num == 1) {
            server_num = 2;
            server_slot = PARTNER(server_slot);
            event(EV_SECOND_SERVER, server_slot);
        } else {
            serving_team ^= 1;
            server_num = 1;
            server_slot = right_slot(serving_team);
            event(EV_SIDE_OUT, serving_team);
        }
    }
    hud_dirty = 1;
    start_serve();
}

unsigned char __fastcall__ in_kitchen(unsigned char s)
{
    unsigned char y = py[s] >> 8;
    if (IS_NEAR(s)) return y <= NEAR_KIT;
    return y >= FAR_KIT;
}

void match_update(void)
{
    unsigned char s;
    switch (state) {
    case ST_PRESERVE:
        start_serve();
        break;
    case ST_CALL:
        ++state_t;
        if (mb_call == 0xA5) { sidecar = 1; mb_call = 1; }
        if (mb_call == 2 || (!sidecar && state_t >= CALL_FRAMES && !voice_busy()) || state_t >= 240) {
            mb_call = 0;
            state = ST_SERVE_DROP; state_t = 0;
            ball_live = 1; vz = 0;
        }
        players_update();
        break;
    case ST_SERVE_DROP:
        ++state_t;
        players_update();
        if (state == ST_SERVE_DROP) ball_update();
        break;
    case ST_FLIGHT:
    case ST_RALLY:
        ++state_t;
        if (mb_reserved) {                 /* the test hook: 1 or 2 ends the rally at once, team A or B losing */
            s = (mb_reserved - 1) & 1; mb_reserved = 0;
            end_rally(R_PASSED, s);
            break;
        }
        players_update();
        if (ball_live) ball_update();
        for (s = 0; s < 4; ++s) {         /* momentum after a volley carries a player into the kitchen */
            if (pactive[s] && mom_t[s] && in_kitchen(s)) { mom_t[s] = 0; end_rally(R_KITCHEN, TEAM(s)); break; }
        }
        break;
    case ST_DEAD:
        ++state_t;
        players_update();
        for (s = 0; s < 4; ++s) {
            if (pactive[s] && mom_t[s] && in_kitchen(s) && dead_reason != R_KITCHEN) {
                mom_t[s] = 0;
                if (TEAM(s) == point_winner) { dead_reason = R_KITCHEN; point_winner ^= 1; hud_call("KITCHEN!"); event(EV_FAULT_KITCHEN, TEAM(s)); }
            }
        }
        if (state_t >= DEAD_FRAMES) apply_point();
        break;
    case ST_GAMEOVER:
        ++state_t;
        if (state_t == 200 && best_of3 && games[0] < 2 && games[1] < 2) {   /* the next game of the match */
            score[0] = score[1] = 0;
            serving_team = (games[0] + games[1]) & 1;
            server_num = 2; server_slot = start_server[serving_team];
            hud_dirty = 1;
            start_serve();
        }
        break;
    }
    mb_state = state;
    mb_score_a = score[0]; mb_score_b = score[1];
    mb_server = serving_team | (server_num << 1) | (server_slot << 3);
    mb_mode = mode | (level << 4);
}
