#ifndef SPEECH_H
#define SPEECH_H
void speech_clear(void);
void speech_call(void);
void __fastcall__ speech_word(unsigned char w);
unsigned char speech_busy(void);
void speech_update(void);
#endif
