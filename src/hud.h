#ifndef HUD_H
#define HUD_H
void hud_score(void);
void __fastcall__ hud_call(const char *s);
void hud_update(void);
void __fastcall__ bubble(unsigned char slot, unsigned char code);
void hud_reset(void);
#endif
