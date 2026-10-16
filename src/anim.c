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
/* onde: spostamento attuale di ogni riga (127 = non ancora disegnata) */
#define WAVE_ROWS 160
static signed char wave_dx[MAX_ANIMS][WAVE_ROWS];
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
        if (anims[i].scene != sc || !anim_cond_ok(anims[i].cond)) continue;
        act[nact] = &anims[i];
        memset(&st[nact], 0, sizeof(AnimState));
        if (anims[i].type == AN_TALK) {
            st[nact].timer = 1;                  /* la bocca parte subito */
            st[nact].phase = 0;
        } else {
            st[nact].timer = (short)(rand_n(40) + 20);
            st[nact].phase = (short)rand_n(64);
        }
        nact++;
    }
    for (i = 0; i < MAX_DIM; i++) dim_timer[i] = 0;
    memset(wave_dx, 127, sizeof(wave_dx));
}

/* Onde: ogni riga e' spostata di dx pixel secondo un seno. Da un passo al
   successivo cambia solo una riga su otto circa: le altre restano come sono
   (si ricorda lo spostamento di ogni riga) e non vanno ne' ricalcolate ne'
   portate a video. */
static int step_wave(const Anim *a, AnimState *s, int idx, GRect *r, int *any)
{
    int y, lambda = a->p2 > 0 ? a->p2 : 16, ylo = -1, yhi = -1;
    signed char *last = wave_dx[idx];
    if (tick % (unsigned long)(a->p3 > 0 ? a->p3 : 3)) return 0;
    s->phase = (short)((s->phase + 1) & 63);
    for (y = a->y0; y < a->y1; y++) {
        int k = (s->phase * 2 + (y - a->y0) * 64 / lambda) & 63;
        int dx = (a->p1 * sin64[k] + (sin64[k] >= 0 ? 63 : -63)) / 127;
        int i = y - a->y0;
        if (i < WAVE_ROWS && last[i] == dx) continue;
        if (i < WAVE_ROWS) last[i] = (signed char)dx;
        shift_row(y, a->x0 >> 3, a->x1 >> 3, dx);
        if (ylo < 0) ylo = y;
        yhi = y + 1;
    }
    if (ylo < 0) return 0;
    grow(r, any, a->x0, ylo, a->x1, yhi);
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

/* riga dy di cur, pixel [x0,x1) = riga sy di base (a byte, con maschere ai bordi) */
static void copy_bits_row(int dy, int sy, int x0, int x1)
{
    int b0 = x0 >> 3, b1 = (x1 - 1) >> 3, b;
    unsigned char *d = cur + dy * FB_RB;
    const unsigned char *s = base + sy * FB_RB;
    for (b = b0; b <= b1; b++) {
        unsigned char m = 0xFF;
        if (b == b0) m &= (unsigned char)(0xFF >> (x0 & 7));
        if (b == b1 && (x1 & 7)) m &= (unsigned char)(0xFF << (8 - (x1 & 7)));
        d[b] = (unsigned char)((d[b] & ~m) | (s[b] & m));
    }
}

/* riga y di cur, pixel [x0,x1) tutti neri o tutti bianchi */
static void fill_bits_row(int y, int x0, int x1, int black)
{
    int b0 = x0 >> 3, b1 = (x1 - 1) >> 3, b;
    unsigned char *d = cur + y * FB_RB;
    for (b = b0; b <= b1; b++) {
        unsigned char m = 0xFF;
        if (b == b0) m &= (unsigned char)(0xFF >> (x0 & 7));
        if (b == b1 && (x1 & 7)) m &= (unsigned char)(0xFF << (8 - (x1 & 7)));
        d[b] = black ? (unsigned char)(d[b] | m) : (unsigned char)(d[b] & ~m);
    }
}

/* Bocca aperta di k (0 chiusa, 1 socchiusa, 2 aperta): cambiano SOLO i
   pixel della bocca, sulla riga y0 tra p2 e p3 e sulle due righe sotto.
   Bocca scura su visi chiari; denti chiari su visi e barbe scure. */
static void draw_mouth(const Anim *a, int k)
{
    int y, x0 = a->p2, x1 = a->p3 + 1, light = (a->p1 & TALK_LIGHT) != 0;
    for (y = a->y0; y < a->y0 + 3; y++)                      /* bocca chiusa */
        copy_bits_row(y, y, x0, x1);
    if (k >= 1) fill_bits_row(a->y0, x0 + 1, x1 - 1, !light);
    if (k >= 2) {
        if (light) {
            fill_bits_row(a->y0 + 1, x0, x1, 1);             /* denti sopra e sotto */
            fill_bits_row(a->y0 + 2, x0 + 1, x1 - 1, 0);
        } else {
            fill_bits_row(a->y0 + 1, x0, x1, 1);
            fill_bits_row(a->y0 + 2, x0 + 1, x1 - 1, 1);
        }
    }
}

static int step_talk(const Anim *a, AnimState *s, int speaker, GRect *r, int *any)
{
    static const unsigned char pattern[8] = {1, 2, 1, 0, 2, 1, 0, 1};
    int want;
    if ((a->p1 & ~TALK_LIGHT) == speaker) {
        if (--s->timer > 0) return 0;
        s->timer = (short)(2 + rand_n(2));
        want = pattern[rand_n(8)];
        if (want == s->phase) want = (want + 1) % 3;  /* mentre parla si muove sempre */
    } else {
        want = 0;                                    /* bocca chiusa quando tace */
    }
    if (want == s->phase) return 0;
    s->phase = (short)want;
    draw_mouth(a, want);
    grow(r, any, a->p2, a->y0, a->p3 + 1, a->y0 + 3);
    return 1;
}

/* luce che tremola (lanterna, candela: fotogrammi a caso) o che gira
   (lente del faro: fotogrammi in sequenza) */
static int step_flicker(const Anim *a, AnimState *s, GRect *r, int *any)
{
    const FlickerSet *fs = &asset_flicker[a->p1];
    int y, f, rb = fs->w >> 3;
    const unsigned char *bits;
    if (--s->timer > 0) return 0;
    if (a->p2) {
        s->timer = (short)(a->p3 > 0 ? a->p3 : 2);
        f = (s->phase + 1) % fs->n;
    } else {
        s->timer = (short)(2 + rand_n(5));
        f = rand_n(fs->n);
        if (f == s->phase) return 0;
    }
    s->phase = (short)f;
    bits = fs->data + (long)f * rb * fs->h;
    for (y = 0; y < fs->h; y++)
        memcpy(cur + (fs->y + y) * FB_RB + (fs->x >> 3), bits + y * rb, rb);
    grow(r, any, fs->x, fs->y, fs->x + fs->w, fs->y + fs->h);
    return 1;
}

/* Lente del faro che gira: una fascia d'ombra scorre da sinistra a destra
   dentro l'ellisse della lente (il rettangolo dell'animazione). Si lavora a
   byte: ogni byte della fascia riceve un retino piu' o meno fitto secondo
   la distanza dal centro dell'ombra. Niente fotogrammi salvati. */
static const unsigned char beam_pat[3][4] = {
    { 0x88, 0x00, 0x22, 0x00 },           /* 25% */
    { 0xAA, 0x55, 0xAA, 0x55 },           /* 50% */
    { 0xEE, 0xBB, 0xEE, 0xBB },           /* 75% */
};

static int step_beam(const Anim *a, AnimState *s, GRect *r, int *any)
{
    int cx = (a->x0 + a->x1) / 2, cy = (a->y0 + a->y1) / 2;
    int rx = (a->x1 - a->x0) / 2, ry = (a->y1 - a->y0) / 2;
    int n = a->p1 > 0 ? a->p1 : 10, span = 2 * rx + 36, bx, y;
    int bx0 = a->x0 >> 3, nb = (a->x1 - a->x0) >> 3;
    if (--s->timer > 0) return 0;
    s->timer = (short)(a->p3 > 0 ? a->p3 : 3);
    s->phase = (short)((s->phase + 1) % n);
    bx = cx - rx - 18 + span * s->phase / n;
    for (y = a->y0; y < a->y1; y++) {
        unsigned char *d = cur + y * FB_RB + bx0;
        const unsigned char *b = base + y * FB_RB + bx0;
        long dy = y - cy;
        int half, xl, xr, c;
        memcpy(d, b, nb);
        /* meta' larghezza dell'ellisse a questa riga */
        {
            long q = (long)rx * rx - (long)rx * rx * dy * dy / ((long)ry * ry);
            if (q <= 0) continue;
            for (half = 0; (long)(half + 1) * (half + 1) <= q; half++) ;
        }
        xl = cx - half;
        xr = cx + half;
        for (c = 0; c < nb; c++) {
            int x = a->x0 + c * 8, dist = x + 4 - bx, lv;
            unsigned char m = 0xFF;
            if (dist < 0) dist = -dist;
            lv = dist < 6 ? 2 : dist < 12 ? 1 : dist < 18 ? 0 : -1;
            if (lv < 0 || x + 8 <= xl || x >= xr) continue;
            if (x < xl) m &= (unsigned char)(0xFF >> (xl - x));
            if (x + 8 > xr) m &= (unsigned char)(0xFF << (x + 8 - xr));
            d[c] = (unsigned char)(b[c] | (beam_pat[lv][y & 3] & m));
        }
    }
    grow(r, any, a->x0, a->y0, a->x1, a->y1);
    return 1;
}

/* acqua che cade: le righe della zona scorrono verso il basso (a byte) */
static int step_fall(const Anim *a, AnimState *s, GRect *r, int *any)
{
    int y, h = a->y1 - a->y0, bx0 = a->x0 >> 3, n = (a->x1 - a->x0) >> 3;
    if (tick % (unsigned long)(a->p3 > 0 ? a->p3 : 2)) return 0;
    s->phase = (short)((s->phase + (a->p1 > 0 ? a->p1 : 2)) % h);
    for (y = a->y0; y < a->y1; y++) {
        int sy = a->y0 + (y - a->y0 - s->phase + h) % h;
        memcpy(cur + y * FB_RB + bx0, base + sy * FB_RB + bx0, n);
    }
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
        case AN_WAVE:    step_wave(a, &st[i], i, &r, &any); break;
        case AN_SWAY:    step_sway(a, &st[i], &r, &any); break;
        case AN_BOB:     step_bob(a, &st[i], speaker, &r, &any); break;
        case AN_FLICKER: step_flicker(a, &st[i], &r, &any); break;
        case AN_TWINKLE: step_twinkle(a, &r, &any); break;
        case AN_FLYER:   step_flyer(a, &st[i], &r, &any); break;
        case AN_TALK:    step_talk(a, &st[i], speaker, &r, &any); break;
        case AN_FALL:    step_fall(a, &st[i], &r, &any); break;
        case AN_BEAM:    step_beam(a, &st[i], &r, &any); break;
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
