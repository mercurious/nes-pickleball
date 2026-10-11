/* NES Pickleball: the match engine's shared state and constants. Positions are screen pixels in 8.8 fixed point. */
#ifndef GAME_H
#define GAME_H
#include "kernel.h"
#include "geom.h"

#define PX(v) ((unsigned int)(v) << 8)
#define G_FP 20                 /* gravity, 0.08 px per frame^2 */
#define NET_H 9                 /* the net's height in px (36 in at the sides) */
#define SWING_LEN 14
#define CW_START 3              /* the swing's contact window, frames */
#define CW_END 9
#define BALL_OVERHEAD 26        /* a ball higher than this cannot be reached */
#define DEAD_FRAMES 75
#define CALL_FRAMES 90

enum { ST_PRESERVE, ST_CALL, ST_SERVE_DROP, ST_FLIGHT, ST_RALLY, ST_DEAD, ST_GAMEOVER };
enum { SHOT_HARD, SHOT_SOFT, SHOT_LOB, SHOT_SERVE };
enum { MODE_1P_SINGLES, MODE_1P_DOUBLES, MODE_2P_DOUBLES, MODE_2P_SINGLES, MODE_EXHIBITION, MODE_COUNT };
enum { R_NONE, R_OUT, R_NET, R_LET, R_SHORT, R_WRONG_COURT, R_SERVE_FAULT, R_TWO_BOUNCE, R_DOUBLE_BOUNCE,
       R_KITCHEN, R_BODY, R_NET_TOUCH, R_PASSED, R_GAME };
enum { EV_NONE, EV_SCORE_CALL, EV_SERVE, EV_RETURN, EV_THIRD_SHOT, EV_DINK, EV_DRIVE, EV_LOB, EV_VOLLEY, EV_SMASH,
       EV_BOUNCE_IT, EV_MINE, EV_YOURS, EV_SWITCH, EV_POINT_WON, EV_FAULT_OUT, EV_FAULT_NET, EV_FAULT_LET,
       EV_FAULT_SHORT, EV_FAULT_WRONG_COURT, EV_FAULT_FOOT, EV_FAULT_TWO_BOUNCE, EV_FAULT_DOUBLE_BOUNCE,
       EV_FAULT_KITCHEN, EV_FAULT_BODY, EV_FAULT_NET_TOUCH, EV_SIDE_OUT, EV_SECOND_SERVER, EV_GAME_OVER,
       EV_NICE_SHOT, EV_LONG_RALLY, EV_PASSED };

/* the ball (zero page) */
extern unsigned int bx, by;
extern int bz, vx, vy, vz;
extern unsigned char ball_live, bounce_side, shots, last_hitter, serve_to_right;
#pragma zpsym("bx")
#pragma zpsym("by")
#pragma zpsym("bz")
#pragma zpsym("vx")
#pragma zpsym("vy")
#pragma zpsym("vz")
#pragma zpsym("ball_live")
#pragma zpsym("bounce_side")
#pragma zpsym("shots")
#pragma zpsym("last_hitter")
#pragma zpsym("serve_to_right")

/* the players: slots 0,1 team A (near), 2,3 team B (far) */
extern unsigned int px[4], py[4];
#pragma zpsym("px")
#pragma zpsym("py")
extern unsigned char pactive[4], phuman[4], pstate[4], sw_t[4], sw_kind[4], sw_side[4], anim_t[4], mom_t[4];
extern int pspeed[4];
extern signed char in_dx[4], in_dy[4];
extern unsigned char in_swing[4];
extern unsigned char home_x[4], home_y[4];

/* the match */
extern unsigned char state, state_t, mode, level, doubles;
#pragma zpsym("state")
#pragma zpsym("state_t")
extern unsigned char score[2], games[2], serving_team, server_num, server_slot, receiver_slot, start_server[2];
extern unsigned char game_to, best_of3, lets_replay, dead_reason, point_winner, rally_len, serve_t;
extern unsigned char pads[2], trigs[2];
extern unsigned char hud_dirty, call_dirty;
extern char call_text[12];

/* the mailbox ($07F0-$07FF): what a sidecar reads and writes, see docs/MAILBOX.md */
extern unsigned char mb_magic, mb_version, mb_state, mb_score_a, mb_score_b, mb_server, mb_mode, mb_event_n,
       mb_event, mb_event_arg, mb_call, mb_directive, mb_chatter, mb_bubble, mb_bubble_slot, mb_reserved;

#define TEAM(s) ((s) >> 1)
#define PARTNER(s) ((s) ^ 1)
#define IS_NEAR(s) ((s) < 2)

void match_init(unsigned char m, unsigned char lv);
void match_update(void);
void end_rally(unsigned char reason, unsigned char loser_team);
void __fastcall__ event(unsigned char code, unsigned char arg);
unsigned char __fastcall__ in_kitchen(unsigned char s);

void ball_update(void);
void players_update(void);
void __fastcall__ do_hit(unsigned char s, unsigned char kind);
unsigned char __fastcall__ in_reach(unsigned char s);
void __fastcall__ serve_hit(unsigned char s);
void __fastcall__ ai_intent(unsigned char s);
void ai_new_rally(void);
void ai_set_levels(unsigned char lv);
extern unsigned char ai_mid[4];
void draw_match(void);

void hud_score(void);
void __fastcall__ hud_call(const char *s);
void hud_update(void);
void __fastcall__ bubble(unsigned char slot, unsigned char code);
void __fastcall__ fmt_call(unsigned char with_server);

#endif
