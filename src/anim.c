/*
 * anim.c - animazioni degli sfondi (vedi anim.h).
 *
 * Tutto e' pensato per un 68000 a 8 MHz: si lavora a byte, si aggiornano
 * solo poche righe per volta e ogni animazione ha il suo ritmo.
 */
#include <string.h>
#include "anim.h"

#define MAX_ANIMS 16
#define MAX_DIM   4

typedef struct {
    short phase;           /* fase attuale (onde, oscillazioni, respiro) */
    short timer;
    short fx, fy, fdir;    /* gabbiano: posizione e direzione */
    short active;
} AnimState;

static const unsigned char *base;
static unsigned char *cur;
static int scene;
static unsigned long tick;
static unsigned long rnd = 12345;
static const Anim *act[MAX_ANIMS];
static AnimState st[MAX_ANIMS];
static int nact;

/* stelle attenuate o scintillanti */
static short dim_star[MAX_DIM], dim_timer[MAX_DIM], dim_kind[MAX_DIM];

/* seno su 64 passi, ampiezza 127 */
static const signed char sin64[64] = {
    0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126,
    127, 126, 125, 122, 117, 112, 106, 98, 90, 81, 71, 60, 49, 37, 25, 12,
    0, -12, -25, -37, -49, -60, -71, -81, -90, -98, -106, -112, -117, -122, -125, -126,
    -127, -126, -125, -122, -117, -112, -106, -98, -90, -81, -71, -60, -49, -37, -25, -12
};

static int rand_n(int n)
{
    rnd = rnd * 1103515245UL + 12345UL;
    return (int)((rnd >> 16) % (unsigned long)n);
}

static void grow(GRect *r, int *any, int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > FB_W) x1 = FB_W;
    if (y1 > FB_H) y1 = FB_H;
    if (x0 >= x1 || y0 >= y1) return;
    if (!*any) {
        r->x0 = (short)x0; r->y0 = (short)y0; r->x1 = (short)x1; r->y1 = (short)y1;
        *any = 1;
        return;
    }
    if (x0 < r->x0) r->x0 = (short)x0;
    if (y0 < r->y0) r->y0 = (short)y0;
    if (x1 > r->x1) r->x1 = (short)x1;
    if (y1 > r->y1) r->y1 = (short)y1;
}

static int touches(const Anim *a, const GRect *r)
{
    return r && a->x0 < r->x1 && r->x0 < a->x1 && a->y0 < r->y1 && r->y0 < a->y1;
}

/* riga y di cur = riga y di base spostata di dx pixel (solo i byte bx0..bx1).
   |dx| < 8: ogni byte di destinazione nasce da due byte vicini. */
static void shift_row(int y, int bx0, int bx1, int dx)
{
    const unsigned char *s = base + y * FB_RB;
    unsigned char *d = cur + y * FB_RB;
    int b;
    if (dx == 0) {
        memcpy(d + bx0, s + bx0, bx1 - bx0);
    } else if (dx > 0) {
        int l = 8 - dx;
        unsigned char prev = bx0 > 0 ? s[bx0 - 1] : s[bx0];
        for (b = bx0; b < bx1; b++) {
            unsigned char c = s[b];
            d[b] = (unsigned char)((prev << l) | (c >> dx));
            prev = c;
        }
    } else {
        int sh = -dx, r = 8 + dx;
        for (b = bx0; b < bx1; b++) {
            unsigned char next = b + 1 < FB_RB ? s[b + 1] : s[b];
            d[b] = (unsigned char)((s[b] << sh) | (next >> r));
        }
    }
}

static void restore_rect(int x0, int y0, int x1, int y1)
{
    int y, bx0, bx1;
    if (x0 < 0) x0 = 0;
    if (x1 > FB_W) x1 = FB_W;
    if (y0 < 0) y0 = 0;
    if (y1 > FB_H) y1 = FB_H;
    if (x0 >= x1) return;
    bx0 = x0 >> 3;
    bx1 = (x1 + 7) >> 3;
    for (y = y0; y < y1; y++)
        memcpy(cur + y * FB_RB + bx0, base + y * FB_RB + bx0, bx1 - bx0);
}

static void set_px(int x, int y, int black)
{
    unsigned char *p;
    if (x < 0 || x >= FB_W || y < 0 || y >= FB_H) return;
    p = cur + y * FB_RB + (x >> 3);
    if (black) *p |= (unsigned char)(0x80 >> (x & 7));
    else *p &= (unsigned char)~(0x80 >> (x & 7));
}

/* ------------------------------------------------------------------ */

void anim_reset(int sc, const unsigned char *b, unsigned char *c)
{
    int i;
    base = b;
    cur = c;
    scene = sc;
    nact = 0;
    for (i = 0; i < anim_count && nact < MAX_ANIMS; i++) {
        if (anims[i].scene != sc) continue;
        act[nact] = &anims[i];
        memset(&st[nact], 0, sizeof(AnimState));
        st[nact].timer = (short)(rand_n(40) + 20);
        st[nact].phase = (short)rand_n(64);
        nact++;
    }
    for (i = 0; i < MAX_DIM; i++) dim_timer[i] = 0;
}

static int step_wave(const Anim *a, AnimState *s, GRect *r, int *any)
{
    int y, lambda = a->p2 > 0 ? a->p2 : 16;
    if (tick % (unsigned long)(a->p3 > 0 ? a->p3 : 3)) return 0;
    s->phase = (short)((s->phase + 1) & 63);
    for (y = a->y0; y < a->y1; y++) {
        int k = (s->phase * 2 + (y - a->y0) * 64 / lambda) & 63;
        int dx = (a->p1 * sin64[k] + (sin64[k] >= 0 ? 63 : -63)) / 127;
        shift_row(y, a->x0 >> 3, a->x1 >> 3, dx);
    }
    grow(r, any, a->x0, a->y0, a->x1, a->y1);
    return 1;
}

static int step_sway(const Anim *a, AnimState *s, GRect *r, int *any)
{
    int y, h = a->y1 - a->y0, period = a->p2 > 0 ? a->p2 : 64;
    long amp;
    int k;
    if (tick % 3) return 0;
    s->timer = (short)((s->timer + 3) % period);
    k = (int)((long)s->timer * 64 / period) & 63;
    amp = (long)a->p1 * sin64[k];                    /* ampiezza * 127 */
    for (y = a->y0; y < a->y1; y++) {
        long v = amp * (a->y1 - y) / h;              /* in alto si muove di piu' */
        int dx = (int)((v + (v >= 0 ? 63 : -63)) / 127);
        shift_row(y, a->x0 >> 3, a->x1 >> 3, dx);
    }
    grow(r, any, a->x0, a->y0, a->x1, a->y1);
    return 1;
}

static int step_bob(const Anim *a, AnimState *s, int speaker, GRect *r, int *any)
{
    int want, y, dy;
    int talking = a->p1 >= 0 && a->p1 == speaker;
    if (talking) {
        want = (tick / 3) & 1;                       /* annuisce mentre parla */
    } else if (a->p2 <= 0) {
        want = 0;                                    /* fermo quando tace */
    } else {
        if (--s->timer > 0) return 0;
        want = !s->phase;                            /* respiro lento */
        s->timer = (short)(a->p2 + rand_n(a->p2 / 2 + 1));
    }
    want = want ? 1 : 0;
    if (want == (s->phase ? 1 : 0)) return 0;
    s->phase = (short)want;
    dy = want ? (a->p3 > 0 ? a->p3 : 1) : 0;
    for (y = a->y0; y < a->y1; y++) {
        int sy = y + dy;
        memcpy(cur + y * FB_RB + (a->x0 >> 3), base + sy * FB_RB + (a->x0 >> 3),
               (a->x1 - a->x0) >> 3);
    }
    grow(r, any, a->x0, a->y0, a->x1, a->y1);
    return 1;
}

static int step_flicker(const Anim *a, AnimState *s, GRect *r, int *any)
{
    const Bitmap *b;
    int y, f;
    if (--s->timer > 0) return 0;
    s->timer = (short)(2 + rand_n(5));
    f = rand_n(LANTERN_FRAMES);
    if (f == s->phase) return 0;
    s->phase = (short)f;
    b = &asset_lantern[f];
    for (y = 0; y < b->h; y++)
        memcpy(cur + (LANTERN_Y + y) * FB_RB + (LANTERN_X >> 3), b->bits + y * b->rowBytes, b->rowBytes);
    grow(r, any, a->x0, a->y0, a->x1, a->y1);
    return 1;
}

static void star_restore(int i, GRect *r, int *any)
{
    int x = asset_stars[i][0] * 2, y = asset_stars[i][1];
    restore_rect(x - 3, y - 3, x + 4, y + 4);
    grow(r, any, x - 8, y - 3, x + 9, y + 4);
}

static int step_twinkle(const Anim *a, GRect *r, int *any)
{
    int i, changed = 0;
    (void)a;
    for (i = 0; i < MAX_DIM; i++) {
        if (dim_timer[i] > 0 && --dim_timer[i] == 0) {
            star_restore(dim_star[i], r, any);
            changed = 1;
        }
    }
    if (tick % 4 == 0) {
        for (i = 0; i < MAX_DIM; i++) {
            if (dim_timer[i] == 0) {
                int s = rand_n(STAR_COUNT);
                int x = asset_stars[s][0] * 2, y = asset_stars[s][1], k;
                dim_star[i] = (short)s;
                dim_kind[i] = (short)(rand_n(3) == 0);
                dim_timer[i] = (short)(4 + rand_n(10));
                if (dim_kind[i]) {
                    /* scintilla: piccola croce luminosa */
                    for (k = -2; k <= 2; k++) { set_px(x + k, y, 0); set_px(x, y + k, 0); }
                } else {
                    /* si spegne per un attimo */
                    for (k = -1; k <= 1; k++) { set_px(x + k, y - 1, 1); set_px(x + k, y, 1); set_px(x + k, y + 1, 1); }
                }
                grow(r, any, x - 8, y - 3, x + 9, y + 4);
                changed = 1;
                break;
            }
        }
    }
    return changed;
}

static void draw_gull(int x, int y, int frame)
{
    const unsigned char *g = asset_gull + frame * GULL_H * 2;
    int row, i;
    for (row = 0; row < GULL_H; row++) {
        unsigned int bits = ((unsigned int)g[row * 2] << 8) | g[row * 2 + 1];
        for (i = 0; i < GULL_W; i++)
            if (bits & (0x8000u >> i)) set_px(x + i, y + row, 1);
    }
}

static int step_flyer(const Anim *a, AnimState *s, GRect *r, int *any)
{
    if (!s->active) {
        if (--s->timer > 0) return 0;
        s->active = 1;
        s->fdir = (short)(rand_n(2) ? 1 : -1);
        s->fx = (short)(s->fdir > 0 ? -GULL_W : FB_W);
        s->fy = (short)(a->y0 + rand_n(a->y1 - a->y0 - GULL_H * 3 + 1));
    }
    if (tick & 1) return 0;
    /* cancella il fotogramma precedente */
    restore_rect(s->fx, s->fy - 3, s->fx + GULL_W, s->fy + GULL_H + 3);
    grow(r, any, s->fx, s->fy - 3, s->fx + GULL_W, s->fy + GULL_H + 3);
    s->fx = (short)(s->fx + s->fdir * (a->p1 > 0 ? a->p1 : 3));
    s->phase = (short)((s->phase + 1) & 63);
    if (s->fx < -GULL_W - 2 || s->fx > FB_W + 2) {
        s->active = 0;
        s->timer = (short)(120 + rand_n(300));
        return 1;
    }
    {
        int bob = sin64[(s->phase * 4) & 63] / 64;   /* planata leggera */
        int y = s->fy + bob;
        draw_gull(s->fx, y, (s->phase >> 1) & 1);
        grow(r, any, s->fx, y - 3, s->fx + GULL_W, y + GULL_H + 3);
    }
    return 1;
}

int anim_step(int speaker, const GRect *avoid, GRect *out, int max)
{
    int i, n = 0;
    tick++;
    if (!base || !cur) return 0;
    for (i = 0; i < nact; i++) {
        const Anim *a = act[i];
        GRect r;
        int any = 0;
        if (touches(a, avoid)) continue;     /* non disturbare il box dei dialoghi */
        switch (a->type) {
        case AN_WAVE:    step_wave(a, &st[i], &r, &any); break;
        case AN_SWAY:    step_sway(a, &st[i], &r, &any); break;
        case AN_BOB:     step_bob(a, &st[i], speaker, &r, &any); break;
        case AN_FLICKER: step_flicker(a, &st[i], &r, &any); break;
        case AN_TWINKLE: step_twinkle(a, &r, &any); break;
        case AN_FLYER:   step_flyer(a, &st[i], &r, &any); break;
        }
        if (!any) continue;
        if (n < max) {
            out[n++] = r;
        } else {
            int dummy = 1;
            grow(&out[max - 1], &dummy, r.x0, r.y0, r.x1, r.y1);
        }
    }
    return n;
}
