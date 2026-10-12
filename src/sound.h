/* The cartridge's sound (src/sound.c): Pently (lib/pently) plays the music and the synthesized effects; the DMC
   plays the sampled court sounds and the umpire's voice. */
#ifndef SOUND_H
#define SOUND_H
enum { SFX_NONE, SFX_MENU, SFX_PAUSE, SFX_NETCORD, SFX_COUNT };
enum { JG_NONE, JG_FAULT, JG_POINT, JG_SIDEOUT, JG_GAME, JG_TITLE, JG_COUNT };
enum { VW_FAULT, VW_OUT, VW_LET, VW_KITCHEN, VW_SIDE_OUT, VW_SECOND_SERVER, VW_GAME, VW_COUNT };  /* the umpire */
enum { STYLE_LAYERED, STYLE_SAMPLE, STYLE_SYNTH };      /* how a hit sounds: for the listening tests */
extern unsigned char snd_style;
void snd_init(void);
void snd_update(void);
void __fastcall__ sfx(unsigned char id);
void __fastcall__ snd_hit(unsigned char slot, unsigned char ev);   /* ev: EV_SERVE or the shot (EV_DINK..EV_SMASH) */
void __fastcall__ snd_bounce(unsigned char far_court);
void __fastcall__ jingle(unsigned char id);
void __fastcall__ applause(unsigned char big);
void voice_call(void);                       /* the score, as the umpire calls it */
void __fastcall__ voice_word(unsigned char w);   /* VW_ */
unsigned char voice_busy(void);
#endif
