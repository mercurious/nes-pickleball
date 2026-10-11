#ifndef SOUND_H
#define SOUND_H
enum { SFX_NONE, SFX_HIT_HARD, SFX_HIT_SOFT, SFX_BOUNCE, SFX_NETCORD, SFX_SERVE, SFX_MENU, SFX_PAUSE, SFX_COUNT };
enum { JG_NONE, JG_FAULT, JG_POINT, JG_SIDEOUT, JG_GAME, JG_TITLE, JG_COUNT };
void snd_init(void);
void snd_update(void);
void __fastcall__ sfx(unsigned char id);
void __fastcall__ jingle(unsigned char id);
void __fastcall__ applause(unsigned char frames);
#endif
