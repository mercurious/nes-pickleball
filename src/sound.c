/* A small APU driver. Effects own pulse 1 and the noise (and the triangle for a bounce); jingles and the title
   tune own pulse 2 and the triangle. The pickleball thwap: a 25 % pulse "pop" dropping from about 900 to 700 Hz
   over three frames, under a burst of high noise that decays 10 to 3: the hollow plastic ball over the
   plywood paddle's slap. The dink is the same, quieter and one frame shorter. The bounce is a triangle "tock". */
#include "kernel.h"
#include "sound.h"

/* periods for notes A2 (110 Hz) up to G#5, one semitone apart; the triangle sounds an octave lower */
static const unsigned int PERIOD[36] = {
    1016,959,905,854,806,761,718,678,640,604,570,538,508,479,452,427,403,380,359,339,320,302,285,269,254,239,226,213,201,190,179,169,160,151,142,134
};

/* an effect step: frames, pulse volume (0 = off) | duty << 6, pulse period, noise volume (0 = off), noise period,
   triangle period (0 = off) */
typedef struct { unsigned char dur, pv; unsigned int pp; unsigned char nv, np; unsigned int tp; } Step;
static const Step FX_HIT_HARD[] = { {1, 0x4C, 123, 10, 3, 0}, {1, 0x48, 150, 7, 4, 0}, {1, 0x44, 190, 4, 5, 0}, {1, 0x42, 230, 2, 6, 0}, {0} };
static const Step FX_HIT_SOFT[] = { {1, 0x47, 110, 6, 2, 0}, {1, 0x43, 140, 3, 4, 0}, {1, 0x41, 170, 1, 5, 0}, {0} };
static const Step FX_BOUNCE[]   = { {1, 0, 0, 4, 8, 420}, {1, 0, 0, 2, 9, 480}, {0} };
static const Step FX_NETCORD[]  = { {2, 0, 0, 6, 11, 0}, {2, 0, 0, 3, 12, 0}, {0} };
static const Step FX_SERVE[]    = { {1, 0x49, 160, 7, 3, 0}, {1, 0x45, 200, 4, 5, 0}, {1, 0x42, 240, 2, 6, 0}, {0} };
static const Step FX_MENU[]     = { {2, 0x88, 254, 0, 0, 0}, {2, 0x86, 190, 0, 0, 0}, {0} };
static const Step FX_PAUSE[]    = { {4, 0x8A, 302, 0, 0, 0}, {4, 0x8A, 226, 0, 0, 0}, {0} };
static const Step * const FX[SFX_COUNT] = { 0, FX_HIT_HARD, FX_HIT_SOFT, FX_BOUNCE, FX_NETCORD, FX_SERVE, FX_MENU, FX_PAUSE };

/* jingles: pairs of (note, frames) for pulse 2, then 0xFF; a second list for the triangle; 0xFE = rest */
static const unsigned char JG_FAULT_P[] = { 7, 6, 4, 6, 0xFE, 2, 2, 10, 0xFF };
static const unsigned char JG_FAULT_T[] = { 0xFE, 12, 2, 10, 0xFF };
static const unsigned char JG_POINT_P[] = { 15, 4, 19, 4, 22, 4, 27, 10, 0xFF };
static const unsigned char JG_POINT_T[] = { 3, 12, 15, 10, 0xFF };
static const unsigned char JG_SIDEOUT_P[] = { 22, 5, 19, 5, 15, 8, 0xFF };
static const unsigned char JG_SIDEOUT_T[] = { 10, 10, 3, 8, 0xFF };
static const unsigned char JG_GAME_P[] = { 15, 6, 15, 6, 15, 6, 19, 12, 22, 6, 19, 6, 22, 6, 27, 20, 0xFF };
static const unsigned char JG_GAME_T[] = { 3, 12, 3, 6, 7, 12, 10, 6, 3, 12, 10, 6, 15, 20, 0xFF };
/* the title tune, 150 bpm, an eighth = 12 frames */
static const unsigned char JG_TITLE_P[] = {
    15,12, 15,12, 19,12, 19,12, 22,12, 19,12, 15,24,  17,12, 17,12, 20,12, 20,12, 22,12, 20,12, 17,24,
    15,12, 19,12, 22,12, 27,12, 26,12, 22,12, 19,24,  24,12, 22,12, 20,12, 19,12, 17,12, 19,12, 15,24, 0xFF };
static const unsigned char JG_TITLE_T[] = {
    3,24, 10,24, 3,24, 10,24,  5,24, 12,24, 5,24, 12,24,  3,24, 10,24, 3,24, 10,24,  8,24, 10,24, 3,48, 0xFF };
static const unsigned char * const JGP[JG_COUNT] = { 0, JG_FAULT_P, JG_POINT_P, JG_SIDEOUT_P, JG_GAME_P, JG_TITLE_P };
static const unsigned char * const JGT[JG_COUNT] = { 0, JG_FAULT_T, JG_POINT_T, JG_SIDEOUT_T, JG_GAME_T, JG_TITLE_T };

static const Step *fx_cur; static unsigned char fx_t;
static const unsigned char *jp, *jt; static unsigned char jp_t, jt_t, j_loop, j_id;
static unsigned char ap_t;

void snd_init(void)
{
    APU(0x15) = 0x0F;
    APU(0x00) = 0x30; APU(0x04) = 0x30; APU(0x0C) = 0x30; APU(0x08) = 0x80;
    fx_cur = 0; jp = 0; jt = 0; ap_t = 0;
}

static void tri_off(void) { APU(0x08) = 0x80; APU(0x0B) = 0; }
static void tri_period(unsigned int p) { APU(0x08) = 0xFF; APU(0x0A) = p & 0xFF; APU(0x0B) = (p >> 8) | 0x08; }

void __fastcall__ sfx(unsigned char id)
{
    if (!id || id >= SFX_COUNT) return;
    fx_cur = FX[id]; fx_t = 0;
}

void __fastcall__ jingle(unsigned char id)
{
    if (id >= JG_COUNT) return;
    j_id = id;
    if (!id) { jp = 0; jt = 0; APU(0x04) = 0x30; tri_off(); return; }
    jp = JGP[id]; jt = JGT[id]; jp_t = 0; jt_t = 0; j_loop = (id == JG_TITLE);
}

void __fastcall__ applause(unsigned char frames) { ap_t = frames; }

static void fx_step(void)
{
    const Step *s = fx_cur;
    if (s->pv) { APU(0x00) = (s->pv & 0xC0) | 0x30 | (s->pv & 0x0F); APU(0x02) = s->pp & 0xFF; APU(0x03) = (s->pp >> 8) | 0x08; }
    else APU(0x00) = 0x30;
    if (s->nv) { APU(0x0C) = 0x30 | s->nv; APU(0x0E) = s->np; APU(0x0F) = 0x08; }
    else if (!ap_t) APU(0x0C) = 0x30;
    if (s->tp) tri_period(s->tp);
    fx_t = s->dur;
}

static unsigned char voice(const unsigned char **seq, unsigned char *t, unsigned char tri)
{
    unsigned char n;
    if (!*seq) return 0;
    if (*t) { --*t; return 1; }
    n = **seq;
    if (n == 0xFF) { *seq = 0; return 0; }
    *t = (*seq)[1] - 1; *seq += 2;
    if (n == 0xFE) { if (tri) tri_off(); else APU(0x04) = 0x30; return 1; }
    if (tri) tri_period(PERIOD[n]);
    else { APU(0x04) = 0xB0 | 0x0A; APU(0x06) = PERIOD[n] & 0xFF; APU(0x07) = (PERIOD[n] >> 8) | 0x08; }
    return 1;
}

void snd_update(void)
{
    unsigned char live;
    if (fx_cur) {
        if (fx_t == 0) {
            if (fx_cur->dur == 0) { fx_cur = 0; APU(0x00) = 0x30; if (!ap_t) APU(0x0C) = 0x30; if (!jt) tri_off(); }
            else fx_step();
        }
        if (fx_t) --fx_t;
    }
    if (ap_t) {                                           /* applause: noise that crackles and fades */
        --ap_t;
        if (!fx_cur || !fx_cur->nv) {
            APU(0x0C) = 0x30 | ((ap_t >> 3) > 9 ? 9 : (ap_t >> 3) + 1);
            if ((ap_t & 3) == 0) { APU(0x0E) = 4 + (rand8() & 3); APU(0x0F) = 0x08; }
            if (!ap_t) APU(0x0C) = 0x30;
        }
    }
    live = voice(&jp, &jp_t, 0);
    live |= voice(&jt, &jt_t, 1);
    if (!live && j_id) {
        if (j_loop) { jp = JGP[j_id]; jt = JGT[j_id]; jp_t = 0; jt_t = 0; }
        else { j_id = 0; APU(0x04) = 0x30; tri_off(); }
    }
}
