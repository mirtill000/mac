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
    AN_BOB,       /* personaggio che respira / annuisce      p1 chi parla (-1), p2 periodo, p3 pixel */
    AN_FLICKER,   /* lanterna che tremola */
    AN_TWINKLE,   /* stelle che brillano */
    AN_FLYER      /* gabbiano che attraversa il cielo        p1 velocita' */
};

#define SCENE_TITLE 10       /* scena del titolo (le stanze vanno da 0 a 3) */

typedef struct {
    signed char scene;
    unsigned char type;
    short x0, y0, x1, y1;    /* x0 e x1 multipli di 8 */
    short p1, p2, p3;
} Anim;

extern const Anim anims[];
extern const int anim_count;

void anim_reset(int scene, const unsigned char *base, unsigned char *cur);

/* Un passo di animazione (chiamato 20 volte al secondo).
 * speaker: chi sta parlando ora (-1 nessuno).
 * avoid:   zona da non toccare (il box dei dialoghi), o NULL.
 * Scrive in out[] i rettangoli cambiati (al massimo max) e ne ritorna il numero. */
int anim_step(int speaker, const GRect *avoid, GRect *out, int max);

#endif
