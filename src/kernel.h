/* NES Pickleball: the kernel's C interface (crt0.s). */
#ifndef KERNEL_H
#define KERNEL_H

#define PAD_A      0x80
#define PAD_B      0x40
#define PAD_SELECT 0x20
#define PAD_START  0x10
#define PAD_UP     0x08
#define PAD_DOWN   0x04
#define PAD_LEFT   0x02
#define PAD_RIGHT  0x01

extern unsigned char oam[256];       /* sprite shadow: y, tile, attr, x per sprite; DMA'd every frame */
extern unsigned char vbuf[256];      /* VRAM update buffer: [hi][lo][len][data...] runs, flushed in the NMI */
extern unsigned char pal_buf[32];    /* palette shadow; set pal_dirty to upload it */
extern unsigned char vbuf_len;
extern unsigned char pal_dirty;
extern unsigned char frame_count;
extern unsigned char ppu_ctrl_var;
extern unsigned char ppu_mask_var;
extern unsigned char scroll_x;
extern unsigned char scroll_y;
#pragma zpsym("vbuf_len")
#pragma zpsym("pal_dirty")
#pragma zpsym("frame_count")
#pragma zpsym("ppu_ctrl_var")
#pragma zpsym("ppu_mask_var")
#pragma zpsym("scroll_x")
#pragma zpsym("scroll_y")

void ppu_off(void);
void ppu_on_all(void);
void ppu_wait_nmi(void);
void __fastcall__ vram_adr(unsigned int adr);
void __fastcall__ vram_put(unsigned char v);
void __fastcall__ vram_write(const void *src, unsigned int n);
void __fastcall__ vram_fill(unsigned char v, unsigned int n);
unsigned char __fastcall__ pad_poll(unsigned char pad);
unsigned char rand8(void);
void __fastcall__ set_rand(unsigned int seed);
void __fastcall__ dmc_bank(unsigned char bank);   /* maps a sample bank (0-27) at $C000 */

#define APU(r) (*(volatile unsigned char *)(0x4000 + (r)))
#define NTADR(x, y) (0x2000 + (y) * 32 + (x))

#endif
