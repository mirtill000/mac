/*
 * gfx.c - primitive grafiche 1-bit.
 *
 * Tutto viene disegnato in gfx_fb, che ha lo stesso formato di una BitMap
 * QuickDraw: sul Mac basta un CopyBits per portarlo a video.
 * Le routine lavorano a byte (con scorrimento) per essere rapide anche
 * su un 68000 a 8 MHz.
 */
#include <string.h>
#include "gfx.h"

unsigned char gfx_fb[FB_H * FB_RB];

const unsigned char PAT_WHITE[8]  = {0, 0, 0, 0, 0, 0, 0, 0};
const unsigned char PAT_BLACK[8]  = {255, 255, 255, 255, 255, 255, 255, 255};
const unsigned char PAT_GRAY[8]   = {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
const unsigned char PAT_LTGRAY[8] = {0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22};
const unsigned char PAT_DKGRAY[8] = {0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD};

static GRect clip = {0, 0, FB_W, FB_H};
/* limiti di clipping in byte, precalcolati per put8 (chiamata per ogni
   byte di sprite e testo: deve fare il minimo indispensabile) */
static int clip_b0 = 0, clip_b1 = FB_RB - 1;
static unsigned char clip_lm = 0xFF, clip_rm = 0xFF;

void gfx_clip(int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > FB_W) x1 = FB_W;
    if (y1 > FB_H) y1 = FB_H;
    if (x1 < x0) x1 = x0;
    clip.x0 = (short)x0; clip.y0 = (short)y0; clip.x1 = (short)x1; clip.y1 = (short)y1;
    clip_b0 = x0 >> 3;
    clip_b1 = (x1 - 1) >> 3;
    clip_lm = (unsigned char)(0xFF >> (x0 & 7));
    clip_rm = (x1 & 7) ? (unsigned char)(0xFF << (8 - (x1 & 7))) : 0xFF;
    if (x1 == x0) clip_b1 = clip_b0 - 1;        /* area vuota */
}

void gfx_noclip(void) { gfx_clip(0, 0, FB_W, FB_H); }

#define MAX_DIRTY 8
static GRect dirty[MAX_DIRTY];
static int ndirty;

/* due zone si fondono se si toccano o sono vicine (meno CopyBits) */
static int near_rect(const GRect *a, int x0, int y0, int x1, int y1)
{
    return x0 <= a->x1 + 8 && a->x0 <= x1 + 8 && y0 <= a->y1 + 2 && a->y0 <= y1 + 2;
}

static void merge_into(GRect *a, int x0, int y0, int x1, int y1)
{
    if (x0 < a->x0) a->x0 = (short)x0;
    if (y0 < a->y0) a->y0 = (short)y0;
    if (x1 > a->x1) a->x1 = (short)x1;
    if (y1 > a->y1) a->y1 = (short)y1;
}

void gfx_dirty(int x0, int y0, int x1, int y1)
{
    int i;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > FB_W) x1 = FB_W;
    if (y1 > FB_H) y1 = FB_H;
    if (x0 >= x1 || y0 >= y1) return;
    for (i = 0; i < ndirty; i++) {
        if (near_rect(&dirty[i], x0, y0, x1, y1)) {
            int j;
            merge_into(&dirty[i], x0, y0, x1, y1);
            /* la zona allargata puo' ora toccarne altre */
            for (j = 0; j < ndirty; j++) {
                if (j != i && near_rect(&dirty[i], dirty[j].x0, dirty[j].y0, dirty[j].x1, dirty[j].y1)) {
                    merge_into(&dirty[i], dirty[j].x0, dirty[j].y0, dirty[j].x1, dirty[j].y1);
                    dirty[j] = dirty[--ndirty];
                    if (i == ndirty) i = j;
                    j = -1;
                }
            }
            return;
        }
    }
    if (ndirty < MAX_DIRTY) {
        dirty[ndirty].x0 = (short)x0; dirty[ndirty].y0 = (short)y0;
        dirty[ndirty].x1 = (short)x1; dirty[ndirty].y1 = (short)y1;
        ndirty++;
    } else {
        merge_into(&dirty[MAX_DIRTY - 1], x0, y0, x1, y1);
    }
}

void gfx_dirty_all(void)
{
    ndirty = 0;
    gfx_dirty(0, 0, FB_W, FB_H);
}

int gfx_take_dirty(GRect *r)
{
    if (!ndirty) return 0;
    *r = dirty[--ndirty];
    return 1;
}

/* ------------------------------------------------------------------ */

void gfx_unpack(const PackedBitmap *pb, unsigned char *dst)
{
    const unsigned char *p = pb->packed;
    int rb = (pb->w + 7) >> 3;
    int y;
    for (y = 0; y < pb->h; y++) {
        unsigned char *d = dst + y * rb;
        unsigned char *end = d + rb;
        while (d < end) {
            int n = *p++;
            if (n < 128) {
                n++;
                while (n--) *d++ = *p++;
            } else if (n > 128) {
                unsigned char v = *p++;
                n = 257 - n;
                while (n--) *d++ = v;
            }
        }
    }
}

/* copia un rettangolo (allargato ai byte) da un buffer 64 byte/riga */
void gfx_copy_from(const unsigned char *src, int x0, int y0, int x1, int y1)
{
    int bx0, bx1, y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > FB_W) x1 = FB_W;
    if (y1 > FB_H) y1 = FB_H;
    if (x0 >= x1 || y0 >= y1) return;
    bx0 = x0 >> 3;
    bx1 = (x1 + 7) >> 3;
    if (bx0 == 0 && bx1 == FB_RB) {
        memcpy(gfx_fb + y0 * FB_RB, src + y0 * FB_RB, (y1 - y0) * FB_RB);
    } else {
        for (y = y0; y < y1; y++)
            memcpy(gfx_fb + y * FB_RB + bx0, src + y * FB_RB + bx0, bx1 - bx0);
    }
    gfx_dirty(bx0 << 3, y0, bx1 << 3, y1);
}

/* ------------------------------------------------------------------ */

static unsigned char lmask(int x) { return (unsigned char)(0xFF >> (x & 7)); }
static unsigned char rmask(int x) { return (unsigned char)(0xFF << (8 - (x & 7))); }

/* op: 0 = retino, 1 = inverti */
static void span_op(int x, int y, int w, int h, const unsigned char *pat, int op)
{
    int x1 = x + w, y1 = y + h, yy;
    if (x < clip.x0) x = clip.x0;
    if (y < clip.y0) y = clip.y0;
    if (x1 > clip.x1) x1 = clip.x1;
    if (y1 > clip.y1) y1 = clip.y1;
    if (x >= x1 || y >= y1) return;
    gfx_dirty(x, y, x1, y1);
    {
        /* maschere dei byte di bordo calcolate una volta sola; i byte interi
           in mezzo si scrivono senza maschera (memset per i colori pieni) */
        int b0 = x >> 3, b1 = (x1 - 1) >> 3, b, n;
        unsigned char lm = lmask(x), rm = (x1 & 7) ? rmask(x1) : 0xFF;
        if (b0 == b1) lm &= rm;
        n = b1 - b0 - 1;
        for (yy = y; yy < y1; yy++) {
            unsigned char *row = gfx_fb + yy * FB_RB;
            unsigned char p = pat ? pat[yy & 7] : 0;
            if (op) {
                row[b0] ^= lm;
                if (b1 > b0) {
                    for (b = b0 + 1; b < b1; b++) row[b] = (unsigned char)~row[b];
                    row[b1] ^= rm;
                }
            } else {
                row[b0] = (unsigned char)((row[b0] & ~lm) | (p & lm));
                if (b1 > b0) {
                    if (n > 0) memset(row + b0 + 1, p, (size_t)n);
                    row[b1] = (unsigned char)((row[b1] & ~rm) | (p & rm));
                }
            }
        }
    }
}

void gfx_fill(int x, int y, int w, int h, const unsigned char *pat) { span_op(x, y, w, h, pat, 0); }
void gfx_invert(int x, int y, int w, int h) { span_op(x, y, w, h, 0, 1); }

void gfx_hline(int x0, int x1, int y, int col)
{
    gfx_fill(x0, y, x1 - x0 + 1, 1, col ? PAT_BLACK : PAT_WHITE);
}

void gfx_vline(int x, int y0, int y1, int col)
{
    gfx_fill(x, y0, 1, y1 - y0 + 1, col ? PAT_BLACK : PAT_WHITE);
}

void gfx_frame(int x, int y, int w, int h, int col)
{
    gfx_hline(x, x + w - 1, y, col);
    gfx_hline(x, x + w - 1, y + h - 1, col);
    gfx_vline(x, y, y + h - 1, col);
    gfx_vline(x + w - 1, y, y + h - 1, col);
}

void gfx_plot(int x, int y, int col)
{
    gfx_fill(x, y, 1, 1, col ? PAT_BLACK : PAT_WHITE);
}

/* ------------------------------------------------------------------ */

/* velatura corrente (solo durante il disegno delle bitmap con maschera) */
static const unsigned char *tint_pat;
static unsigned char cur_tint;

void gfx_set_tint(const unsigned char *pat) { tint_pat = pat; }

/* un byte di destinazione con maschera, nel rettangolo di clipping */
#define PUT_BYTE(row, b, s, m) do {                                         \
        if ((b) >= clip_b0 && (b) <= clip_b1) {                             \
            unsigned char mm_ = (m);                                        \
            if ((b) == clip_b0) mm_ &= clip_lm;                             \
            if ((b) == clip_b1) mm_ &= clip_rm;                             \
            (row)[b] = (unsigned char)(((row)[b] & ~mm_) | (((s) | cur_tint) & mm_)); \
        }                                                                   \
    } while (0)

/* scrive 8 pixel (src, con maschera m) alla posizione x della riga */
static void put8(unsigned char *row, int x, unsigned char src, unsigned char m)
{
    int b, sh;
    if (!m) return;
    b = x >> 3;
    sh = x & 7;
    if (!sh) {
        PUT_BYTE(row, b, src, m);
        return;
    }
    PUT_BYTE(row, b, (unsigned char)(src >> sh), (unsigned char)(m >> sh));
    PUT_BYTE(row, b + 1, (unsigned char)(src << (8 - sh)), (unsigned char)(m << (8 - sh)));
}

void gfx_masked(const unsigned char *bits, const unsigned char *mask,
                int w, int h, int rb, int x, int y)
{
    int yy, i;
    if (x >= clip.x1 || x + w <= clip.x0 || y >= clip.y1 || y + h <= clip.y0) return;
    gfx_dirty(x, y, x + w, y + h);
    for (yy = 0; yy < h; yy++) {
        int dy = y + yy;
        unsigned char *row;
        if (dy < clip.y0 || dy >= clip.y1) continue;
        row = gfx_fb + dy * FB_RB;
        cur_tint = tint_pat ? tint_pat[dy & 7] : 0;
        for (i = 0; i < rb; i++) {
            unsigned char m = mask ? mask[yy * rb + i] : 0xFF;
            int px = x + (i << 3);
            if (!mask && i == rb - 1 && (w & 7)) m = (unsigned char)(0xFF << (8 - (w & 7)));
            if (px + 8 <= clip.x0 || px >= clip.x1) continue;
            put8(row, px, bits[yy * rb + i], m);
        }
    }
    cur_tint = 0;
    /* il dirty e' gia' clippato dal chiamante del present */
}

static unsigned char rev_tab[256];
static int rev_ready;

static void init_rev(void)
{
    int i, b;
    for (i = 0; i < 256; i++) {
        unsigned char r = 0;
        for (b = 0; b < 8; b++)
            if (i & (1 << b)) r |= (unsigned char)(0x80 >> b);
        rev_tab[i] = r;
    }
    rev_ready = 1;
}

/* Una riga di sprite interamente dentro il ritaglio (e con il ritaglio
   allineato al byte): niente controlli per ogni byte, lo scorrimento di
   sh bit si porta avanti da un byte al successivo. E' il ciclo che conta
   di piu' su un 68000: disegna il protagonista a ogni passo. */
static void row_fast(unsigned char *d, const unsigned char *bs, const unsigned char *ms,
                     int rb, int sh, int mirror)
{
    unsigned char t = cur_tint, m, b;
    int i;
/* un byte del disegno: FETCH legge (m, b) dal byte i dello sprite */
#define ROW_LOOP(FETCH)                                                       \
    if (!sh) {                                                                \
        for (i = 0; i < rb; i++) {                                            \
            FETCH;                                                            \
            if (m) d[i] = (unsigned char)((d[i] & ~m) | ((b | t) & m));       \
        }                                                                     \
    } else {                                                                  \
        int r = 8 - sh;                                                       \
        unsigned char pm = 0, pb = 0, mm;                                     \
        for (i = 0; i < rb; i++) {                                            \
            FETCH;                                                            \
            mm = (unsigned char)((pm << r) | (m >> sh));                      \
            if (mm) d[i] = (unsigned char)((d[i] & ~mm) |                     \
                           ((((pb << r) | (b >> sh)) | t) & mm));             \
            pm = m;                                                           \
            pb = b;                                                           \
        }                                                                     \
        mm = (unsigned char)(pm << r);                                        \
        if (mm) d[rb] = (unsigned char)((d[rb] & ~mm) | (((pb << r) | t) & mm)); \
    }
    if (mirror) {
        bs += rb - 1;
        ms += rb - 1;
        ROW_LOOP((m = rev_tab[*ms--], b = rev_tab[*bs--]))
    } else {
        ROW_LOOP((m = *ms++, b = *bs++))
    }
#undef ROW_LOOP
}

void gfx_sprite_rows(const unsigned char *pool, const unsigned short *rows,
                     int w, int h, int rb, int x, int y, int mirror)
{
    int yy, i, fast;
    if (!rev_ready) init_rev();
    if (x >= clip.x1 || x + w <= clip.x0 || y >= clip.y1 || y + h <= clip.y0) return;
    gfx_dirty(x, y, x + w, y + h);
    /* tutta la larghezza (compreso il byte di scorrimento) dentro il ritaglio */
    fast = x >= 0 && (x >> 3) >= clip_b0 && (x >> 3) + rb + ((x & 7) ? 1 : 0) - 1 <= clip_b1 &&
           clip_lm == 0xFF && clip_rm == 0xFF;
    for (yy = 0; yy < h; yy++) {
        int dy = y + yy;
        const unsigned char *bs, *ms;
        unsigned char *row;
        if (dy < clip.y0 || dy >= clip.y1) continue;
        bs = pool + rows[yy];
        ms = bs + rb;
        row = gfx_fb + dy * FB_RB;
        cur_tint = tint_pat ? tint_pat[dy & 7] : 0;
        if (fast) {
            row_fast(row + (x >> 3), bs, ms, rb, x & 7, mirror);
            continue;
        }
        for (i = 0; i < rb; i++) {
            int src = mirror ? rb - 1 - i : i;
            unsigned char m = mirror ? rev_tab[ms[src]] : ms[src];
            int px = x + (i << 3);
            if (!m || px + 8 <= clip.x0 || px >= clip.x1) continue;
            put8(row, px, mirror ? rev_tab[bs[src]] : bs[src], m);
        }
    }
    cur_tint = 0;
}

void gfx_shadow(int cx, int cy, int rx, int ry)
{
    int dy;
    for (dy = -ry; dy <= ry; dy++) {
        int y = cy + dy, half, x;
        long t = (long)rx * rx * (ry * ry - dy * dy);
        if (y < clip.y0 || y >= clip.y1) continue;
        /* semi-larghezza = rx * sqrt(1 - dy^2/ry^2) */
        half = 0;
        while ((long)(half + 1) * (half + 1) * ry * ry <= t) half++;
        for (x = cx - half; x <= cx + half; x++) {
            if (x < clip.x0 || x >= clip.x1 || ((x + y) & 1)) continue;
            gfx_fb[y * FB_RB + (x >> 3)] |= (unsigned char)(0x80 >> (x & 7));
        }
        gfx_dirty(cx - half, y, cx + half + 1, y + 1);
    }
}

void gfx_bitmap(const Bitmap *b, int x, int y)
{
    gfx_masked(b->bits, 0, b->w, b->h, b->rowBytes, x, y);
}

/* ------------------------------------------------------------------ */

int gfx_next_char(const char **sp)
{
    const unsigned char *s = (const unsigned char *)*sp;
    int c = *s++;
    if (c >= 0x80) {
        if ((c & 0xE0) == 0xC0 && s[0]) {
            c = ((c & 0x1F) << 6) | (s[0] & 0x3F);
            s++;
        } else if ((c & 0xF0) == 0xE0 && s[0] && s[1]) {
            int cp = ((c & 0x0F) << 12) | ((s[0] & 0x3F) << 6) | (s[1] & 0x3F);
            s += 2;
            if (cp == 0x2019 || cp == 0x2018) c = '\'';
            else if (cp == 0x201C || cp == 0x201D) c = '"';
            else if (cp == 0x2026) c = GLYPH_ELLIPSIS;
            else if (cp == 0x25BC) c = GLYPH_TRIANGLE;
            else if (cp == 0x2013 || cp == 0x2014) c = '-';
            else c = '?';
        } else {
            c = '?';
        }
    }
    *sp = (const char *)s;
    return c;
}

int gfx_text_len(const char *s)
{
    int n = 0;
    while (*s) { gfx_next_char(&s); n++; }
    return n;
}

/* disegna un carattere senza segnalare la zona sporca */
static void glyph_raw(int x, int y, int code, int font, int col)
{
    const unsigned char *g;
    int r, r0 = 0, r1 = FONT_H;
    if (code < FONT_FIRST || code > 255) code = '?';
    if (x >= clip.x1 || x + FONT_W <= clip.x0) return;
    if (y < clip.y0) r0 = clip.y0 - y;
    if (y + FONT_H > clip.y1) r1 = clip.y1 - y;
    g = asset_font[font] + (code - FONT_FIRST) * FONT_H;
    for (r = r0; r < r1; r++)
        if (g[r]) put8(gfx_fb + (y + r) * FB_RB, x, col ? 0xFF : 0x00, g[r]);
}

void gfx_glyph(int x, int y, int code, int font, int col)
{
    glyph_raw(x, y, code, font, col);
    gfx_dirty(x, y, x + FONT_W + 1, y + FONT_H);
}

int gfx_text(int x, int y, const char *s, int font, int col)
{
    int x0 = x;
    while (*s) {
        glyph_raw(x, y, gfx_next_char(&s), font, col);
        x += FONT_W;
    }
    gfx_dirty(x0, y, x + 1, y + FONT_H);
    return x - x0;
}

int gfx_text_l1(int x, int y, const unsigned char *s, int n, int font, int col)
{
    int x0 = x;
    while (n-- > 0) {
        glyph_raw(x, y, *s++, font, col);
        x += FONT_W;
    }
    gfx_dirty(x0, y, x + 1, y + FONT_H);
    return x - x0;
}

int gfx_to_latin1(const char *s, int bytes, unsigned char *out, int max)
{
    const char *end = s + bytes;
    int n = 0;
    while (s < end && *s && n < max)
        out[n++] = (unsigned char)gfx_next_char(&s);
    return n;
}

void gfx_line(int x0, int y0, int x1, int y1, int col)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (x0 >= clip.x0 && x0 < clip.x1 && y0 >= clip.y0 && y0 < clip.y1) {
            unsigned char *p = gfx_fb + y0 * FB_RB + (x0 >> 3);
            if (col) *p |= (unsigned char)(0x80 >> (x0 & 7));
            else *p &= (unsigned char)~(0x80 >> (x0 & 7));
        }
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
    gfx_dirty(0, 0, FB_W, FB_H);
}

void gfx_text_outline(int x, int y, const char *s, int font, int col)
{
    int dx, dy;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (dx || dy) gfx_text(x + dx, y + dy, s, font, !col);
    gfx_text(x, y, s, font, col);
}
