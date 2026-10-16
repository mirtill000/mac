/*
 * walk.c - camminata nelle zone calpestabili, oggetti cliccabili, verbi e azioni
 */
#include <string.h>
#include "core.h"
#include "platform.h"

/* ------------------------------------------------------------------ */
/* Camminata e azioni                                                  */
/* ------------------------------------------------------------------ */

int in_poly(const WalkPoly *p, int x, int y)
{
    int i;
    for (i = 0; i < p->n; i++) {
        int j = (i + 1) % p->n;
        long cr = (long)(p->x[j] - p->x[i]) * (y - p->y[i]) - (long)(p->y[j] - p->y[i]) * (x - p->x[i]);
        if (cr < 0) return 0;               /* a sinistra di un lato: fuori */
    }
    return 1;
}

int walkable(int x, int y)
{
    const WalkArea *w = &walk_areas[G.room];
    int i;
    for (i = 0; i < w->npoly; i++)
        if (in_poly(&w->poly[i], x, y)) return 1;
    return 0;
}

/* porta (x,y) nel punto calpestabile piu' vicino */
void clamp_walk(short *x, short *y)
{
    const WalkArea *w = &walk_areas[G.room];
    long best = -1;
    int bx = *x, by = *y, i, k, step;
    if (walkable(*x, *y)) return;
    for (i = 0; i < w->npoly; i++) {
        const WalkPoly *p = &w->poly[i];
        for (k = 0; k < p->n; k++) {
            int j = (k + 1) % p->n;
            long ex = p->x[j] - p->x[k], ey = p->y[j] - p->y[k];
            long len2 = ex * ex + ey * ey, t, qx, qy, d;
            t = len2 ? ((long)(*x - p->x[k]) * ex + (long)(*y - p->y[k]) * ey) * 1024 / len2 : 0;
            if (t < 0) t = 0;
            if (t > 1024) t = 1024;
            qx = p->x[k] + ex * t / 1024;
            qy = p->y[k] + ey * t / 1024;
            d = (qx - *x) * (qx - *x) + (qy - *y) * (qy - *y);
            if (best < 0 || d < best) { best = d; bx = (int)qx; by = (int)qy; }
        }
    }
    /* gli arrotondamenti possono lasciarlo sul bordo: un passo verso l'interno */
    for (step = 0; step < 8 && !walkable(bx, by); step++) {
        const WalkPoly *p = &w->poly[0];
        int cx = 0, cy = 0;
        for (k = 0; k < p->n; k++) { cx += p->x[k]; cy += p->y[k]; }
        cx /= p->n; cy /= p->n;
        bx += (cx > bx) - (cx < bx);
        by += (cy > by) - (cy < by);
    }
    *x = (short)bx;
    *y = (short)by;
}

void walk_to(int x, int y)
{
    tx = (short)x; ty = (short)y;
    clamp_walk(&tx, &ty);
    walking = (tx != G.hx || ty != G.hy);
}

static const Hotspot *find_hotspot(int id)
{
    int i;
    for (i = 0; i < hotspot_count; i++)
        if (hotspots[i].id == id) return &hotspots[i];
    return 0;
}

int hotspot_at(int x, int y)
{
    int i;
    if (y >= SCENE_H) return OBJ_NONE;
    for (i = 0; i < hotspot_count; i++) {
        const Hotspot *h = &hotspots[i];
        if (h->room == G.room && hotspot_visible(h->id) && in_rect(x, y, h->x, h->y, h->w, h->h))
            return h->id;
    }
    return OBJ_NONE;
}

void reset_sentence(void)
{
    cur_verb = V_VAI;
    sel_item = OBJ_NONE;
    panel_dirty = 1;
}

void execute(int verb, int o1, int o2, int exit_to)
{
    reset_sentence();
    if (verb == V_VAI) {
        if (exit_to == ROOM_MAP) open_map();
        else if (exit_to >= 0) go_room(exit_to);
        return;
    }
    if (!script_action(verb, o1, o2)) script_default(verb, o1, o2);
}

/* Esegue subito o dopo aver raggiunto il punto del bersaglio. */
void act(int verb, int o1, int o2, int where)
{
    const Hotspot *h = IS_ITEM(where) ? 0 : find_hotspot(where);
    int exit_to = (h && verb == V_VAI) ? h->exit_to : -1;
    if (h && h->wx >= 0) {
        walk_to(h->wx, h->wy);
        pend_active = 1;
        pend_verb = verb; pend_o1 = o1; pend_o2 = o2;
        pend_face = h->face; pend_exit = exit_to;
        if (!walking) {
            pend_active = 0;
            hero_face(h->face);
            execute(verb, o1, o2, exit_to);
        }
    } else {
        if (h && h->face == 0 && h->x + h->w / 2 != G.hx)
            G.dir = (signed char)((h->x + h->w / 2 > G.hx) ? 1 : -1);
        execute(verb, o1, o2, exit_to);
    }
}

/* "da solo": oggetti che si possono usare senza bersaglio */
int usable_alone(int it)
{
    return it == IT_MAPPA || it == IT_CAPPELLO || it == IT_LIBRO;
}

void click_object(int obj)
{
    int verb = cur_verb;
    if (sel_item != OBJ_NONE) {
        /* seconda parte di "Usa X con Y" / "Dai X a Y" */
        int it = sel_item;
        if (obj == it) {
            if (verb == V_USA) act(V_USA, it, OBJ_NONE, it);
            else reset_sentence();
            return;
        }
        if (verb == V_DAI) act(V_DAI, obj, it, obj);
        else act(V_USA, it, obj, obj);
        return;
    }
    if (IS_ITEM(obj)) {
        if (verb == V_VAI) {
            if (obj == IT_MAPPA) act(V_USA, obj, OBJ_NONE, obj);
            else act(V_GUARDA, obj, OBJ_NONE, obj);
            return;
        }
        if (verb == V_DAI || (verb == V_USA && !usable_alone(obj))) {
            sel_item = obj;
            panel_dirty = 1;
            return;
        }
        act(verb, obj, OBJ_NONE, obj);
        return;
    }
    /* oggetto in scena */
    if (verb == V_USA || verb == V_DAI) {
        /* "Usa porta": nessun oggetto scelto, vale come azione semplice */
        act(verb, obj, OBJ_NONE, obj);
        return;
    }
    act(verb, obj, OBJ_NONE, obj);
}

void update_walk(void)
{
    int dx, dy, sx, sy;
    if (!walking) return;
    dx = tx - G.hx;
    dy = ty - G.hy;
    {
        static const signed char speed[HERO_SIZES] = {4, 5, 6};
        int v = speed[hero_size()];
        sx = dx > v ? v : (dx < -v ? -v : dx);
    }
    sy = dy > 2 ? 2 : (dy < -2 ? -2 : dy);
    /* mai fuori dalla zona calpestabile (niente passeggiate sull'acqua):
       se il passo in diagonale esce, si prova solo in orizzontale o solo
       in verticale; se nemmeno cosi' si puo', ci si ferma qui */
    if (!walkable(G.hx + sx, G.hy + sy)) {
        if (sx && walkable(G.hx + sx, G.hy)) sy = 0;
        else if (sy && walkable(G.hx, G.hy + sy)) sx = 0;
        else { tx = G.hx; ty = G.hy; sx = sy = 0; }
    }
    if (sx > 0) G.dir = 1;
    if (sx < 0) G.dir = -1;
    G.hx = (short)(G.hx + sx);
    G.hy = (short)(G.hy + sy);
    walk_anim++;
    if (G.hx == tx && G.hy == ty) {
        walking = 0;
        walk_anim = 0;
        if (pend_active) {
            pend_active = 0;
            hero_face(pend_face);
            execute(pend_verb, pend_o1, pend_o2, pend_exit);
        }
    }
}

