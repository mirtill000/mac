/*
 * gfx.h - grafica 1-bit su framebuffer 512x342 (formato BitMap del Mac:
 * 64 byte per riga, bit a 1 = nero, MSB = pixel piu' a sinistra).
 */
#ifndef GFX_H
#define GFX_H

#include "assets.h"

#define FB_W  512
#define FB_H  342
#define FB_RB 64

extern unsigned char gfx_fb[FB_H * FB_RB];

typedef struct { short x0, y0, x1, y1; } GRect;   /* semiaperto [x0,x1) */

/* retini 8x8 */
extern const unsigned char PAT_WHITE[8], PAT_BLACK[8], PAT_GRAY[8],
                           PAT_LTGRAY[8], PAT_DKGRAY[8];

enum { COL_WHITE = 0, COL_BLACK = 1 };
enum { FONT_REGULAR = 0, FONT_BOLD = 1 };

/* area di clipping per tutte le primitive */
void gfx_clip(int x0, int y0, int x1, int y1);
void gfx_noclip(void);

/* zona "sporca" da ricopiare sullo schermo */
void gfx_dirty(int x0, int y0, int x1, int y1);
void gfx_dirty_all(void);
int  gfx_take_dirty(GRect *r);

void gfx_unpack(const PackedBitmap *pb, unsigned char *dst);
void gfx_copy_from(const unsigned char *src, int x0, int y0, int x1, int y1);

void gfx_fill(int x, int y, int w, int h, const unsigned char *pat);
void gfx_invert(int x, int y, int w, int h);
void gfx_frame(int x, int y, int w, int h, int col);
void gfx_hline(int x0, int x1, int y, int col);
void gfx_vline(int x, int y0, int y1, int col);
void gfx_plot(int x, int y, int col);

/* bitmap opaca (1 = nero) */
void gfx_bitmap(const Bitmap *b, int x, int y);
/* bitmap con maschera (bit a 1 nella maschera = pixel disegnato) */
void gfx_masked(const unsigned char *bits, const unsigned char *mask,
                int w, int h, int rb, int x, int y);

/* come gfx_masked ma specchiato orizzontalmente (w multiplo di 8) */
void gfx_masked_mirror(const unsigned char *bits, const unsigned char *mask,
                       int w, int h, int rb, int x, int y);
/* velatura per le bitmap con maschera (luce della stanza): retino 8x8
   allineato allo schermo, aggiunto ai pixel disegnati; NULL = nessuna */
void gfx_set_tint(const unsigned char *pat);
/* ombra a terra: ellisse retinata al 50% (scurisce soltanto) */
void gfx_shadow(int cx, int cy, int rx, int ry);

/* testo UTF-8 (vengono riconosciute le lettere accentate italiane) */
int  gfx_text(int x, int y, const char *s, int font, int col);
void gfx_text_outline(int x, int y, const char *s, int font, int col);
int  gfx_text_len(const char *s);          /* numero di caratteri */
#define gfx_text_width(s) (gfx_text_len(s) * FONT_W)
void gfx_glyph(int x, int y, int code, int font, int col);

/* decodifica UTF-8 -> Latin-1 (con i glifi speciali) */
int  gfx_next_char(const char **s);
/* converte al massimo `bytes` byte UTF-8 in Latin-1; ritorna i caratteri */
int  gfx_to_latin1(const char *s, int bytes, unsigned char *out, int max);
/* testo gia' convertito in Latin-1 (n caratteri) */
int  gfx_text_l1(int x, int y, const unsigned char *s, int n, int font, int col);
/* linea di 1 pixel (Bresenham) */
void gfx_line(int x0, int y0, int x1, int y1, int col);

#endif
