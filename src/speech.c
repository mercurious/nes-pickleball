/* The cartridge's own voice: DPCM words (art/speech.bin, in the DMC segment at $D400) played one after another
   through the APU's delta channel; the score call before a serve when no sidecar is present, the umpire's words. */
#include "game.h"
#include "speech.h"
#define SPEECH_TABLES
#include "speech_words.h"

extern const unsigned char speech_data[];
static unsigned char q[8], qn, qi, gap, was;

void speech_clear(void) { qn = 0; qi = 0; gap = 0; }
static void push(unsigned char w) { if (qn < 8) q[qn++] = w; }
static void number(unsigned char n)
{
    if (n <= 20) push(n);
    else { push(W_TWENTY); push(n - 20); }
}

void speech_call(void)
{
    speech_clear();
    number(score[serving_team]);
    number(score[serving_team ^ 1]);
    if (doubles) number(server_num);
}

void __fastcall__ speech_word(unsigned char w) { speech_clear(); push(w); }

unsigned char speech_busy(void) { return (qi < qn) || (APU(0x15) & 0x10) || gap; }

void speech_update(void)
{
    unsigned char w;
    if (APU(0x15) & 0x10) { was = 1; return; }
    if (was) { was = 0; gap = 8; }                 /* a breath between words */
    if (gap) { --gap; return; }
    if (qi >= qn) return;
    w = q[qi++];
    APU(0x10) = SPEECH_RATE;
    APU(0x11) = 64;
    APU(0x12) = (unsigned char)(((unsigned int)speech_data >> 6) & 0xFF) + SPEECH_OFF[w];
    APU(0x13) = SPEECH_LEN[w];
    APU(0x15) = 0x0F;
    APU(0x15) = 0x1F;
}
