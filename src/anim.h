/*
 * anim.h - animazioni degli sfondi e dei personaggi "dipinti" nelle scene.
 *
 * Le animazioni lavorano su due copie dello sfondo: `base` (immutabile, con
 * gli oggetti di scena gia' stampati) e `cur` (lo sfondo attuale, da cui il
 * motore ripristina i pixel sotto il protagonista). Ogni passo modifica solo
 * piccole zone di `cur` e restituisce il rettangolo cambiato.
 */
#ifndef ANIM_H
#define ANIM_H

#include "gfx.h"

enum {
    AN_WAVE,      /* righe che ondeggiano (mare)            p1 ampiezza, p2 lunghezza d'onda, p3 lentezza */
    AN_SWAY,      /* oscillazione con perno in basso         p1 ampiezza, p2 periodo */
    AN_BOB,       /* personaggio che annuisce quando parla   p1 chi parla, p2 periodo di respiro (0 = mai), p3 pixel */
    AN_FLICKER,   /* luce che tremola o gira                 p1 quale (asset_flicker), p2 1 = in sequenza, p3 lentezza */
    AN_TWINKLE,   /* stelle che brillano */
    AN_FLYER,     /* gabbiano che attraversa il cielo        p1 velocita' */
    AN_TALK,      /* bocca che si muove mentre parla: cambiano solo i pixel della
                     bocca (riga y0, da x p2 a p3, e le due righe sotto);
                     p1 chi parla (+ TALK_LIGHT: denti chiari invece della bocca
                     scura, per visi e barbe scure). x0..x1/y1: zona per Mela-D */
    AN_FALL,      /* cascata: le righe scorrono verso il basso p1 pixel per passo, p3 lentezza */
    AN_BEAM       /* lente del faro che gira: ombra dentro l'ellisse del rettangolo
                     p1 passi per giro, p3 lentezza */
};

#define TALK_LIGHT 16
#define SCENE_TITLE 30       /* scena del titolo (le stanze vanno da 0 a ROOM_COUNT-1) */

typedef struct {
    signed char scene;
    unsigned char type;
    short x0, y0, x1, y1;    /* x0 e x1 multipli di 8 */
    short p1, p2, p3;
    short cond;              /* 0 sempre; f+1 solo con la flag f; -(f+1) solo senza */
} Anim;

extern const Anim anims[];
extern const int anim_count;

void anim_reset(int scene, const unsigned char *base, unsigned char *cur);

/* fornita dal motore: la condizione di un'animazione e' vera? */
int anim_cond_ok(int cond);

/* Un passo di animazione (chiamato 20 volte al secondo).
 * speaker: chi sta parlando ora (-1 nessuno).
 * avoid:   zona da non toccare (il box dei dialoghi), o NULL.
 * Scrive in out[] i rettangoli cambiati (al massimo max) e ne ritorna il numero. */
int anim_step(int speaker, const GRect *avoid, GRect *out, int max);

#endif
