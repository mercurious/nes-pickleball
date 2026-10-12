/* The cartridge's sound. Pently (lib/pently, the score in audio/pickleball.pently) plays the jingles, the title tune
   and the synthesized effects on the pulse, triangle and noise channels; it runs in the NMI every frame (crt0.s),
   and this file only queues requests for it (snd_cmd), because the main loop often takes more than a frame and
   Pently's tempo and envelopes count frames. The DMC is ours: the sampled pocks and
   bounce (audio/samples.txt, encoded by tools/dpcm.py) and the umpire's voice, one sample at a time; a sample's
   bank is mapped at $C000 while it plays. The DMC's resting level is 64 from pently_init on and every sample
   starts and ends on it. When a sample is done the level is set to 64 again: a no-op when it ended where it
   should, a small click when it did not: when a hit cut a bounce off, or when the 2A03 handed the DMC a corrupted
   byte (a DMC fetch that meets a CPU read of $4000-$401F, as Pently's `sta $4000,x` stores make in their dummy
   read cycle, gets an internal register instead whenever the sample's address ends in $15-$17).
   Nothing reads $4015: a read while the DMC fetches can corrupt the fetched byte (the 2A03 bug that crt0.s's
   controller reading avoids), so when the DMC is done is a frame number (SMP_FRAMES after the start, on the
   NMI's frame_count, which keeps counting when the main loop is late).
   A hit is the real pock (a sample, quieter from the far court) over a short synthesized ring for body; a drive
   or a smash adds a crack of noise. snd_style picks either half alone, for the listening tests. */
#include "game.h"
#include "sound.h"
#include "pently.h"
#include "music.h"
#define SAMPLE_TABLES
#include "samples.h"

unsigned char snd_style;
unsigned char dmc_fixes;                             /* how many samples were cut off (for test/cues.lua) */
static unsigned char dmc_live, dmc_end;             /* a sample is playing; the frame it is done */
/* a frame number is a byte, so "not reached yet" holds only while it is within 127 frames ahead: each deadline
   has a flag that snd_update clears once it is reached, or it would come round again 128 frames later */
static unsigned char dmc_busy(void) { return dmc_live && (signed char)(dmc_end - frame_count) > 0; }
static unsigned char vq[8], vqn, vqi, vgap, vgap_on, vwas;   /* the voice: a queue of words, a breath between them */
static unsigned char pend_jingle;                    /* a jingle waits until the umpire has spoken */

static void dmc_play(unsigned char id)
{
    if (dmc_busy()) ++dmc_fixes;                      /* cutting a sample off: its level is not 64 */
    APU(0x15) = 0x0F;                                 /* stop the DMC (the other four channels stay on) */
    dmc_bank(SMP_BANK[id]);
    APU(0x10) = SMP_RATE[id];
    APU(0x12) = SMP_ADDR[id];
    APU(0x13) = SMP_LEN[id];
    APU(0x15) = 0x1F;
    dmc_end = frame_count + SMP_FRAMES[id];
    dmc_live = 1;
}

void snd_init(void)
{
    pently_init();
    vqn = 0; vqi = 0; vgap_on = 0; pend_jingle = 0;
    snd_on = 1;                                       /* the NMI runs Pently from now on */
}

void __fastcall__ sfx(unsigned char id)
{
    if (id == SFX_MENU) snd_cmd(PE_menu);
    else if (id == SFX_PAUSE) snd_cmd(PE_pause);
    else if (id == SFX_NETCORD) { snd_cmd(PE_cord); snd_cmd(PE_cord_tri); }
}

static const unsigned char HIT_NEAR[6] = { SMP_POCK_DINK, SMP_POCK_DRIVE, SMP_POCK_DINK, SMP_POCK_VOLLEY, SMP_POCK_SMASH, SMP_POCK_SERVE };
static const unsigned char HIT_FAR[6] = { SMP_POCK_DINK_FAR, SMP_POCK_DRIVE_FAR, SMP_POCK_DINK_FAR, SMP_POCK_VOLLEY_FAR, SMP_POCK_SMASH_FAR, SMP_POCK_SERVE_FAR };

void __fastcall__ snd_hit(unsigned char slot, unsigned char ev)
{
    unsigned char k = (ev == EV_SERVE) ? 5 : ev - EV_DINK;   /* 0 dink, 1 drive, 2 lob, 3 volley, 4 smash, 5 serve */
    unsigned char nr = IS_NEAR(slot);
    if (snd_style != STYLE_SYNTH && !voice_busy()) dmc_play(nr ? HIT_NEAR[k] : HIT_FAR[k]);
    if (snd_style != STYLE_SAMPLE) {
        snd_cmd((nr && k != 0 && k != 2) ? PE_ponk : PE_ponk_soft);
        if (k == 1 || k == 4) snd_cmd(PE_crack);
    }
}

void __fastcall__ snd_bounce(unsigned char far_court)
{
    if (snd_style != STYLE_SYNTH && !voice_busy() && !dmc_busy()) dmc_play(SMP_BOUNCE);
    if (snd_style != STYLE_SAMPLE) snd_cmd(far_court ? PE_tick : PE_knock);
}

static void start_jingle(unsigned char id)
{
    static const unsigned char SONG[JG_COUNT] = { 0, PS_fault, PS_point, PS_sideout, PS_game, PS_title_tune };
    snd_cmd(id ? 0x40 | SONG[id] : 0x80);
}

void __fastcall__ jingle(unsigned char id)
{
    if (id >= JG_COUNT) return;
    pend_jingle = 0;
    if (id && voice_busy()) { pend_jingle = id; return; }
    start_jingle(id);
}

void __fastcall__ applause(unsigned char big) { snd_cmd(big >= 100 ? PE_applause_big : PE_applause); }

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
    vqn = 0; vqi = 0; vgap_on = 0;
#ifdef SMP_W_ZERO
    number(score[serving_team]);
    number(score[serving_team ^ 1]);
    if (doubles) number(server_num);
#endif
}

void __fastcall__ voice_word(unsigned char w)
{
    vqn = 0; vqi = 0; vgap_on = 0;
#ifdef SMP_W_ZERO
    push(VW_SMP[w]);
#else
    (void)w;                                          /* no voice yet (the TTS words come with plan step 6) */
#endif
}

unsigned char voice_busy(void) { return (vqi < vqn) || vwas || vgap_on; }

static void voice_update(void)
{
    if (dmc_busy()) return;
    if (vwas) { vwas = 0; vgap = frame_count + 6; vgap_on = 1; }   /* a breath between words, to frame vgap */
    if (vgap_on) { if ((signed char)(vgap - frame_count) > 0) return; vgap_on = 0; }
    if (vqi >= vqn) return;
    dmc_play(vq[vqi++]);
    vwas = 1;
}

void snd_update(void)
{
    if (dmc_live && !dmc_busy()) { APU(0x11) = 64; dmc_live = 0; }   /* done: back on the resting level */
    voice_update();
    if (pend_jingle && !voice_busy()) { start_jingle(pend_jingle); pend_jingle = 0; }
}
