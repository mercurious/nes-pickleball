/* The cartridge's sound. Pently (lib/pently, the score in audio/pickleball.pently) plays the jingles, the title tune
   and the synthesized effects on the pulse, triangle and noise channels. The DMC is ours: the sampled pocks and
   bounce (audio/samples.txt, encoded by tools/dpcm.py) and the umpire's voice, one sample at a time; a sample's
   bank is mapped at $C000 while it plays. The DMC's resting level is 64 from pently_init on and every sample
   starts and ends on it, so nothing here writes $4011.
   A hit is the real pock (a sample, quieter from the far court) over a short synthesized ring for body; a drive
   or a smash adds a crack of noise. snd_style picks either half alone, for the listening tests. */
#include "game.h"
#include "sound.h"
#include "pently.h"
#include "music.h"
#define SAMPLE_TABLES
#include "samples.h"

unsigned char snd_style;
static unsigned char vq[8], vqn, vqi, vgap, vwas;   /* the voice: a queue of words, a breath between them */
static unsigned char pend_jingle;                    /* a jingle waits until the umpire has spoken */

static void dmc_play(unsigned char id)
{
    APU(0x15) = 0x0F;                                 /* stop the DMC (the other four channels stay on) */
    dmc_bank(SMP_BANK[id]);
    APU(0x10) = SMP_RATE[id];
    APU(0x12) = SMP_ADDR[id];
    APU(0x13) = SMP_LEN[id];
    APU(0x15) = 0x1F;
}

void snd_init(void)
{
    pently_init();
    vqn = 0; vqi = 0; vgap = 0; pend_jingle = 0;
}

void __fastcall__ sfx(unsigned char id)
{
    if (id == SFX_MENU) pently_start_sound(PE_menu);
    else if (id == SFX_PAUSE) pently_start_sound(PE_pause);
    else if (id == SFX_NETCORD) { pently_start_sound(PE_cord); pently_start_sound(PE_cord_tri); }
}

static const unsigned char HIT_NEAR[6] = { SMP_POCK_DINK, SMP_POCK_DRIVE, SMP_POCK_DINK, SMP_POCK_VOLLEY, SMP_POCK_SMASH, SMP_POCK_SERVE };
static const unsigned char HIT_FAR[6] = { SMP_POCK_DINK_FAR, SMP_POCK_DRIVE_FAR, SMP_POCK_DINK_FAR, SMP_POCK_VOLLEY_FAR, SMP_POCK_SMASH_FAR, SMP_POCK_SERVE_FAR };

void __fastcall__ snd_hit(unsigned char slot, unsigned char ev)
{
    unsigned char k = (ev == EV_SERVE) ? 5 : ev - EV_DINK;   /* 0 dink, 1 drive, 2 lob, 3 volley, 4 smash, 5 serve */
    unsigned char nr = IS_NEAR(slot);
    if (snd_style != STYLE_SYNTH && !voice_busy()) dmc_play(nr ? HIT_NEAR[k] : HIT_FAR[k]);
    if (snd_style != STYLE_SAMPLE) {
        pently_start_sound((nr && k != 0 && k != 2) ? PE_ponk : PE_ponk_soft);
        if (k == 1 || k == 4) pently_start_sound(PE_crack);
    }
}

void __fastcall__ snd_bounce(unsigned char far_court)
{
    if (snd_style != STYLE_SYNTH && !voice_busy() && !(APU(0x15) & 0x10)) dmc_play(SMP_BOUNCE);
    if (snd_style != STYLE_SAMPLE) pently_start_sound(far_court ? PE_tick : PE_knock);
}

static void start_jingle(unsigned char id)
{
    static const unsigned char SONG[JG_COUNT] = { 0, PS_fault, PS_point, PS_sideout, PS_game, PS_title_tune };
    if (id) pently_start_music(SONG[id]); else pently_stop_music();
}

void __fastcall__ jingle(unsigned char id)
{
    if (id >= JG_COUNT) return;
    pend_jingle = 0;
    if (id && voice_busy()) { pend_jingle = id; return; }
    start_jingle(id);
}

void __fastcall__ applause(unsigned char big) { pently_start_sound(big >= 100 ? PE_applause_big : PE_applause); }

/* the voice ------------------------------------------------------------------------------------------------------ */

static void push(unsigned char w) { if (vqn < 8) vq[vqn++] = w; }

#ifdef SMP_W_ZERO
static const unsigned char VW_SMP[VW_COUNT] = { SMP_W_FAULT, SMP_W_OUT, SMP_W_LET, SMP_W_KITCHEN, SMP_W_SIDE_OUT,
                                                SMP_W_SECOND_SERVER, SMP_W_GAME };
#endif

#ifdef SMP_W_ZERO
static void number(unsigned char n)
{
    if (n <= 20) push(SMP_W_ZERO + n);
    else { push(SMP_W_ZERO + 20); push(SMP_W_ZERO + n - 20); }
}
#endif

void voice_call(void)
{
    vqn = 0; vqi = 0; vgap = 0;
#ifdef SMP_W_ZERO
    number(score[serving_team]);
    number(score[serving_team ^ 1]);
    if (doubles) number(server_num);
#endif
}

void __fastcall__ voice_word(unsigned char w)
{
    vqn = 0; vqi = 0; vgap = 0;
#ifdef SMP_W_ZERO
    push(VW_SMP[w]);
#else
    (void)w;                                          /* no voice yet (the TTS words come with plan step 6) */
#endif
}

unsigned char voice_busy(void) { return (vqi < vqn) || vgap || (vwas && (APU(0x15) & 0x10)); }

static void voice_update(void)
{
    if (APU(0x15) & 0x10) return;
    if (vwas) { vwas = 0; vgap = 6; }                 /* a breath between words */
    if (vgap) { --vgap; return; }
    if (vqi >= vqn) return;
    dmc_play(vq[vqi++]);
    vwas = 1;
}

void snd_update(void)
{
    voice_update();
    if (pend_jingle && !voice_busy()) { start_jingle(pend_jingle); pend_jingle = 0; }
    pently_update();
}
