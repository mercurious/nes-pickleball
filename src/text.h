#ifndef TEXT_H
#define TEXT_H
unsigned char __fastcall__ tile_of(char c);
void __fastcall__ put_text(unsigned int adr, const char *s);              /* rendering off */
void __fastcall__ vbuf_text(unsigned int adr, const char *s);             /* queued for the next NMI */
void __fastcall__ vbuf_run(unsigned int adr, const unsigned char *src, unsigned char n);
void __fastcall__ vbuf_fill(unsigned int adr, unsigned char tile, unsigned char n);
#endif
