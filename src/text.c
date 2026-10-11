/* Text: ASCII to the font's tile ids (tools/font.py ORDER), direct and queued VRAM writes. */
#include "kernel.h"
#include "text.h"

static const char PUNCT[] = "-.!:/()@*?<>,'";

unsigned char __fastcall__ tile_of(char c)
{
    unsigned char i;
    if (c == ' ') return 0;
    if (c >= '0' && c <= '9') return 1 + (c - '0');
    if (c >= 'A' && c <= 'Z') return 11 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 11 + (c - 'a');
    for (i = 0; PUNCT[i]; ++i) if (PUNCT[i] == c) return 37 + i;
    return 0;
}

void __fastcall__ put_text(unsigned int adr, const char *s)
{
    vram_adr(adr);
    while (*s) vram_put(tile_of(*s++));
}

void __fastcall__ vbuf_text(unsigned int adr, const char *s)
{
    unsigned char n = 0;
    const char *p = s;
    while (*p++) ++n;
    if (n == 0 || (unsigned char)(vbuf_len + 3 + n) > 200 || vbuf_len > 200) return;
    vbuf[vbuf_len++] = adr >> 8;
    vbuf[vbuf_len++] = adr & 0xFF;
    vbuf[vbuf_len++] = n;
    while (*s) vbuf[vbuf_len++] = tile_of(*s++);
}

void __fastcall__ vbuf_run(unsigned int adr, const unsigned char *src, unsigned char n)
{
    if (n == 0 || (unsigned char)(vbuf_len + 3 + n) > 200 || vbuf_len > 200) return;
    vbuf[vbuf_len++] = adr >> 8;
    vbuf[vbuf_len++] = adr & 0xFF;
    vbuf[vbuf_len++] = n;
    while (n--) vbuf[vbuf_len++] = *src++;
}

void __fastcall__ vbuf_fill(unsigned int adr, unsigned char tile, unsigned char n)
{
    if (n == 0 || (unsigned char)(vbuf_len + 3 + n) > 200 || vbuf_len > 200) return;
    vbuf[vbuf_len++] = adr >> 8;
    vbuf[vbuf_len++] = adr & 0xFF;
    vbuf[vbuf_len++] = n;
    while (n--) vbuf[vbuf_len++] = tile;
}
