/*
 * engine.c - il motore dell'avventura: stato del gioco, rendering
 * incrementale, protagonista, input e ciclo principale. Il resto e' in
 * msg.c, walk.c, save.c e ui.c (stato condiviso in core.h).
 *
 * Il codice e' C portabile: non usa il Toolbox direttamente. Tutto viene
 * disegnato in gfx_fb e la piattaforma lo porta a video (plat_present).
 */
#include <string.h>
#include "core.h"
#include "platform.h"
#include "anim.h"

/* ------------------------------------------------------------------ */
/* Stato                                                              */
/* ------------------------------------------------------------------ */

GameState G;

int mode = MODE_TITLE;
int modal = MODAL_NONE;
int modal_from_title;
int quit_requested;
int text_speed = 1;                 /* 0 lenta, 1 normale, 2 veloce */
unsigned long frame_count;

short tx, ty;
int walking, walk_anim;
int pend_active, pend_verb, pend_o1, pend_o2, pend_face, pend_exit;

int cur_verb = V_VAI;
int sel_item = OBJ_NONE;
int hover_obj = OBJ_NONE;
int hover_verb = -1;
int hover_choice = -1;
int hover_menu = -1;
int mouse_x, mouse_y;

Msg msgs[MSG_MAX];
int msg_head, msg_count, msg_timer;

int dlg_npc = -1;
int dlg_ending;
int dlg_ids[MAX_CHOICES];
const char *dlg_texts[MAX_CHOICES];
int dlg_n;

int modal_sel = -1;
int map_hover = -1;

unsigned char bgbuf[FB_H * FB_RB];
unsigned char basebuf[FB_H * FB_RB];
int need_full = 1, need_rebuild = 1, panel_dirty = 1;
int sentence_dirty;
GRect hero_prev, box_prev;
int hero_prev_valid, box_prev_valid;
long hero_prev_key, box_prev_key;

GRect anim_rects[ANIM_RECTS];
int anim_nrects;

int blink_timer = 60, blinking;

unsigned char slot_data[SAVE_SLOTS][SAVE_LEN];
int slot_used[SAVE_SLOTS], slot_len[SAVE_SLOTS];

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */

void rebuild_background(void)
{
    int scene = -1;
    need_rebuild = 0;
    switch (mode) {
    case MODE_TITLE: gfx_unpack(&asset_bg[BG_TITLE], basebuf); scene = SCENE_TITLE; break;
    case MODE_MAP:   gfx_unpack(&asset_bg[BG_MAP], basebuf); break;
    case MODE_END:   gfx_unpack(&asset_bg[BG_NIGHT], basebuf); scene = ROOM_NIGHT; break;
    default:
        gfx_unpack(&asset_bg[room_background(G.room)], basebuf);
        /* gli oggetti di scena vengono "stampati" nello sfondo */
        memcpy(gfx_fb, basebuf, SCENE_H * FB_RB);
        gfx_clip(0, 0, FB_W, SCENE_H);
        script_draw_props(G.room);
        gfx_noclip();
        memcpy(basebuf, gfx_fb, SCENE_H * FB_RB);
        scene = G.room;
        break;
    }
    memcpy(bgbuf, basebuf, sizeof(bgbuf));
    anim_nrects = 0;
    anim_reset(scene, basebuf, bgbuf);
}

/* misura del protagonista secondo la profondita' (prospettiva) */
/* Misura del protagonista secondo la profondita' (prospettiva).
   Con isteresi: si cambia misura solo quando la nuova e' chiaramente piu'
   adatta, cosi' camminando su una soglia non "pulsa" avanti e indietro. */
#define SIZE_HYSTERESIS 4

int hero_size(void)
{
    static int last = -1, last_room = -1;
    const RoomDepth *d = &room_depth[G.room];
    int h, i, best = 0;
    if (d->y_near == d->y_far) h = d->h_near;
    else h = d->h_far + (G.hy - d->y_far) * (d->h_near - d->h_far) / (d->y_near - d->y_far);
    for (i = 1; i < HERO_SIZES; i++) {
        int a = asset_hero_height[i] - h, b = asset_hero_height[best] - h;
        if ((a < 0 ? -a : a) < (b < 0 ? -b : b)) best = i;
    }
    if (last >= 0 && last_room == G.room && best != last) {
        int dl = asset_hero_height[last] - h, db = asset_hero_height[best] - h;
        if (dl < 0) dl = -dl;
        if (db < 0) db = -db;
        if (dl - db < SIZE_HYSTERESIS) best = last;
    }
    last = best;
    last_room = G.room;
    return best;
}

int hero_frame(void)
{
    Msg *m = cur_msg();
    if (walking) return 2 + (walk_anim % 6);
    if (m && m->text && m->speaker == SP_HERO && ((frame_count / 3) & 1)) return 1;
    if (blinking) return 8;
    return 0;
}

/* i fotogrammi sono ritagliati: si riposiziona il ritaglio dentro il
   disegno pieno, centrato sui piedi (anche quando e' specchiato) */
GRect hero_rect(void)
{
    int sz = hero_size();
    const Sprite *s = &asset_hero[sz][0];
    int full = asset_hero_full_w[sz], ox = asset_hero_ox[sz];
    GRect r;
    r.x0 = (short)(G.hx - full / 2 + (G.dir < 0 ? full - ox - s->w : ox));
    r.y0 = (short)(G.hy - s->h + 1);
    r.x1 = (short)(r.x0 + s->w);
    r.y1 = (short)(G.hy + 4);                 /* comprende l'ombra a terra */
    return r;
}

long hero_key(void)
{
    return ((long)hero_frame() << 24) ^ ((long)hero_size() << 20) ^ ((long)(G.dir < 0) << 19) ^
           ((long)flag(F_HAT_WORN) << 18) ^ ((long)G.hx << 8) ^ G.hy;
}

long box_key(void)
{
    Msg *m = cur_msg();
    if (!m || !m->text) return 0;
    return 1 + ((long)msg_head << 1) + ((long)msg_count << 8);
}

int overlap(GRect a, GRect b)
{
    return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

void hero_draw(void)
{
    int f = hero_frame(), sz = hero_size();
    GRect r = hero_rect();
    const Sprite *s = &asset_hero[sz][f];
    gfx_clip(0, 0, FB_W, SCENE_H);
    /* luce della stanza: negli ambienti bui il bianco si vela */
    if (room_depth[G.room].tint) gfx_set_tint(PAT_LTGRAY);
    /* ombra sotto i piedi: lega il personaggio al pavimento */
    gfx_shadow(G.hx + (G.dir < 0 ? 1 : -1), G.hy + 1, s->h / 6, 2);
    if (G.dir < 0)
        gfx_masked_mirror(s->bits, s->mask, s->w, s->h, s->rowBytes, r.x0, r.y0);
    else
        gfx_masked(s->bits, s->mask, s->w, s->h, s->rowBytes, r.x0, r.y0);
    if (flag(F_HAT_WORN)) {
        /* il tricorno e' uno strato sopra la testa */
        const Sprite *h = &asset_hero_hat[sz][asset_hero_head_down[f]];
        if (G.dir < 0)
            gfx_masked_mirror(h->bits, h->mask, h->w, h->h, h->rowBytes, r.x0, r.y0);
        else
            gfx_masked(h->bits, h->mask, h->w, h->h, h->rowBytes, r.x0, r.y0);
    }
    gfx_set_tint(0);
    gfx_noclip();
    hero_prev = r;
    hero_prev_valid = 1;
    hero_prev_key = hero_key();
}

/* disegna tutto sopra lo sfondo appena copiato */
void draw_play_overlays(void)
{
    hero_draw();
    box_prev_valid = 0;
    draw_msg();
    box_prev_key = box_key();
}

/* aggiornamento incrementale: solo cio' che e' cambiato */
void update_play_overlays(void)
{
    long hk = hero_key(), bk = box_key();
    GRect hr = hero_rect();
    int hero_changed = !hero_prev_valid || hk != hero_prev_key;
    int box_changed = bk != box_prev_key;
    int hero_redraw, i;

    /* le animazioni hanno ridisegnato parti di sfondo: se toccano il
       protagonista o il box, vanno ridisegnati sopra */
    for (i = 0; i < anim_nrects; i++) {
        GRect a = anim_rects[i];
        if (a.y1 > SCENE_H) a.y1 = SCENE_H;
        gfx_copy_from(bgbuf, a.x0, a.y0, a.x1, a.y1);
        if (hero_prev_valid && overlap(a, hero_prev)) hero_changed = 1;
        if (box_prev_valid && overlap(a, box_prev)) box_changed = 1;
    }
    anim_nrects = 0;

    if (hero_changed && box_prev_valid && (overlap(hero_prev, box_prev) || overlap(hr, box_prev)))
        box_changed = 1;
    if (hero_changed && hero_prev_valid)
        gfx_copy_from(bgbuf, hero_prev.x0, hero_prev.y0, hero_prev.x1,
                      hero_prev.y1 < SCENE_H ? hero_prev.y1 : SCENE_H);
    if (box_changed && box_prev_valid)
        gfx_copy_from(bgbuf, box_prev.x0, box_prev.y0, box_prev.x1, box_prev.y1);
    hero_redraw = hero_changed || (box_changed && box_prev_valid && overlap(hr, box_prev));
    if (hero_redraw) hero_draw();
    if (box_changed) {
        box_prev_valid = 0;
        draw_msg();
        box_prev_key = bk;
    } else if (box_prev_valid && tri_shown != (int)((frame_count / 8) & 1)) {
        /* lampeggio: si ridisegna solo il triangolino */
        draw_triangle(box_prev, !tri_shown);
    }
}

/* Strumento di calibrazione (Mela-D / Ctrl-D): mostra sulla scena le
   aree cliccabili, le zone calpestabili, le zone animate e il protagonista. */
static void draw_debug_overlay(void)
{
    const WalkArea *w = &walk_areas[G.room];
    GRect hr = hero_rect();
    int i, k;
    gfx_clip(0, 0, FB_W, SCENE_H);
    for (i = 0; i < hotspot_count; i++) {
        const Hotspot *h = &hotspots[i];
        if (h->room != G.room || !hotspot_visible(h->id)) continue;
        gfx_frame(h->x, h->y, h->w, h->h, COL_BLACK);
        gfx_frame(h->x + 1, h->y + 1, h->w - 2, h->h - 2, COL_WHITE);
        gfx_text_outline(h->x + 3, h->y + 2, h->name, FONT_REGULAR, COL_WHITE);
        if (h->wx >= 0) {                       /* punto di arrivo */
            gfx_fill(h->wx - 2, h->wy - 2, 5, 5, PAT_BLACK);
            gfx_fill(h->wx - 1, h->wy - 1, 3, 3, PAT_WHITE);
        }
    }
    for (i = 0; i < w->npoly; i++) {           /* zone calpestabili */
        const WalkPoly *p = &w->poly[i];
        for (k = 0; k < p->n; k++) {
            int j = (k + 1) % p->n;
            gfx_line(p->x[k], p->y[k], p->x[j], p->y[j], COL_WHITE);
            gfx_line(p->x[k], p->y[k] + 1, p->x[j], p->y[j] + 1, COL_BLACK);
        }
    }
    for (i = 0; i < anim_count; i++) {         /* animazioni e bocche */
        const Anim *a = &anims[i];
        if (a->scene != G.room) continue;
        gfx_frame(a->x0, a->y0, a->x1 - a->x0, a->y1 - a->y0, COL_WHITE);
    }
    gfx_frame(hr.x0, hr.y0, hr.x1 - hr.x0, hr.y1 - hr.y0, COL_BLACK);
    gfx_noclip();
}

void render_full(void)
{
    if (need_rebuild) rebuild_background();
    need_full = 0;
    anim_nrects = 0;
    gfx_noclip();
    switch (mode) {
    case MODE_TITLE:
        draw_title();
        break;
    case MODE_MAP:
        draw_map();
        draw_msg();
        break;
    case MODE_END:
        draw_end();
        break;
    default:
        memcpy(gfx_fb, bgbuf, SCENE_H * FB_RB);
        gfx_dirty(0, 0, FB_W, SCENE_H);
        draw_play_overlays();
        if (debug_overlay) draw_debug_overlay();
        draw_panel();
        panel_dirty = 0;
        break;
    }
    if (modal != MODAL_NONE) draw_modal();
    gfx_dirty_all();
}

/* contatore delle prestazioni (Mela-F / Ctrl-F) */
int perf_on, perf_fps, perf_peak_ms;
int debug_overlay;
static int perf_frames, perf_peak;
static unsigned long perf_t0;

void game_render(void)
{
    GRect r;
    unsigned long t_start = perf_on ? plat_ticks() : 0;
    int presented = 0;
    if (need_rebuild) { rebuild_background(); need_full = 1; }
    if (debug_overlay && mode == MODE_PLAY && modal == MODAL_NONE) need_full = 1;   /* sempre tutto */
    if (need_full || modal != MODAL_NONE || mode != MODE_PLAY) {
        /* le finestre e le schermate fisse si ridisegnano solo se serve */
        if (need_full) render_full();
    } else {
        update_play_overlays();
        if (panel_dirty || sentence_dirty) { panel_update(); panel_dirty = 0; sentence_dirty = 0; }
    }
    if (!need_full && modal == MODAL_NONE && mode == MODE_MAP && cur_msg() && cur_msg()->text &&
        box_prev_valid && tri_shown != (int)((frame_count / 8) & 1))
        draw_triangle(box_prev, !tri_shown);          /* mappa: lampeggio del box */
    if (!need_full && modal == MODAL_NONE && (mode == MODE_TITLE || mode == MODE_END) && anim_nrects) {
        /* titolo e finale: si ricopiano solo le zone animate */
        int i, menu = 0;
        for (i = 0; i < anim_nrects; i++) {
            GRect a = anim_rects[i];
            if (mode == MODE_END && a.y1 > SCENE_H) a.y1 = SCENE_H;
            gfx_copy_from(bgbuf, a.x0, a.y0, a.x1, a.y1);
            if (a.y1 > TITLE_Y(0) - 3) menu = 1;
        }
        anim_nrects = 0;
        if (mode == MODE_TITLE && menu) draw_title_menu();
    }
    while (gfx_take_dirty(&r)) { plat_present(r.x0, r.y0, r.x1, r.y1); presented = 1; }

    if (perf_on) {
        unsigned long now = plat_ticks();
        int dt = (int)(now - t_start);
        if (presented) perf_frames++;
        if (dt > perf_peak) perf_peak = dt;
        if (now - perf_t0 >= 60) {
            perf_fps = (int)(perf_frames * 60L / (long)(now - perf_t0));
            perf_peak_ms = perf_peak * 1000 / 60;
            perf_frames = 0;
            perf_peak = 0;
            perf_t0 = now;
            sentence_dirty = 1;           /* i numeri stanno nella riga della frase */
        }
    }
}


/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */

void game_mouse(int x, int y)
{
    int v;
    if (x == mouse_x && y == mouse_y) return;
    mouse_x = x;
    mouse_y = y;

    if (modal != MODAL_NONE) {
        switch (modal) {
        case MODAL_PAUSE:     v = pause_at(x, y); break;
        case MODAL_SAVE:
        case MODAL_LOAD:      v = saveload_at(x, y); break;
        case MODAL_OPTIONS:   v = options_at(x, y); break;
        default:              v = invwin_at(x, y); if (v >= G.ninv) v = -1; break;
        }
        if (v != hover_menu) { hover_menu = v; need_full = 1; }
        return;
    }
    switch (mode) {
    case MODE_TITLE:
        v = title_at(x, y);
        if (v != hover_menu) { hover_menu = v; need_full = 1; }
        break;
    case MODE_MAP:
        v = map_at(x, y);
        if (v != map_hover) { map_hover = v; need_full = 1; }
        break;
    case MODE_PLAY: {
        int obj = hotspot_at(x, y), vb = -1, ch, mb;
        if (y >= PANEL_Y && dlg_npc < 0) {
            int it = inv_at(x, y);
            if (it >= 0) obj = it;
            vb = verb_at(x, y);
        }
        ch = choice_at(x, y);
        mb = in_rect(x, y, MENU_BTN_X, SENTENCE_Y, 48, 14) ? 0 : -1;
        if (vb != hover_verb || ch != hover_choice) {
            panel_dirty = 1;              /* evidenziazione di verbi o scelte */
        } else if (obj != hover_obj || mb != hover_menu) {
            sentence_dirty = 1;           /* cambia solo la frase */
        }
        hover_obj = obj;
        hover_verb = vb;
        hover_choice = ch;
        hover_menu = mb;
        break;
    }
    default:
        break;
    }
}

void click_modal(int x, int y)
{
    int v;
    switch (modal) {
    case MODAL_PAUSE:
        if (in_close_box(pause_win, x, y)) { modal = MODAL_NONE; break; }
        v = pause_at(x, y);
        switch (v) {
        case 0: modal = MODAL_NONE; break;
        case 1: open_modal(MODAL_INVENTORY); break;
        case 2: open_modal(MODAL_SAVE); break;
        case 3: open_modal(MODAL_LOAD); break;
        case 4: open_modal(MODAL_OPTIONS); break;
        case 5: to_title(); break;
        case 6: quit_requested = 1; break;
        }
        break;
    case MODAL_SAVE:
    case MODAL_LOAD:
        if (in_close_box(save_win, x, y)) { modal = modal_from_title ? MODAL_NONE : MODAL_PAUSE; break; }
        v = saveload_at(x, y);
        if (v >= 0 && v < SAVE_SLOTS) {
            if (v == modal_sel) {                     /* doppio clic */
                if (modal == MODAL_SAVE) do_save(v); else do_load(v);
            } else {
                modal_sel = v;
            }
        } else if (v == 100) {
            if (modal_sel < 0) plat_beep();
            else if (modal == MODAL_SAVE) do_save(modal_sel);
            else do_load(modal_sel);
        } else if (v == 101) {
            modal = modal_from_title ? MODAL_NONE : MODAL_PAUSE;
        }
        break;
    case MODAL_OPTIONS:
        v = options_at(x, y);
        if (v == 0) text_speed = (text_speed + 1) % 3;
        if (v == 1 || in_close_box(opt_win, x, y)) modal = modal_from_title ? MODAL_NONE : MODAL_PAUSE;
        break;
    case MODAL_INVENTORY:
        if (in_close_box(inv_win, x, y)) { modal = MODAL_NONE; break; }
        v = invwin_at(x, y);
        if (v >= 0 && v < G.ninv) {
            int it = G.inv[v];
            modal = MODAL_NONE;
            reset_sentence();
            act(V_GUARDA, it, OBJ_NONE, it);
        } else if (!in_rect(x, y, inv_win.x, inv_win.y, inv_win.w, inv_win.h)) {
            modal = MODAL_NONE;
        }
        break;
    }
    hover_menu = -1;
    need_full = 1;
    mouse_x = -1;
    game_mouse(x, y);
}

void game_click(int x, int y)
{
    game_mouse(x, y);
    if (modal != MODAL_NONE) { click_modal(x, y); return; }

    switch (mode) {
    case MODE_TITLE: {
        int v = title_at(x, y);
        modal_from_title = 1;
        if (v == 0) new_game();
        else if (v == 1) open_modal(MODAL_LOAD);
        else if (v == 2) open_modal(MODAL_OPTIONS);
        else if (v == 3) quit_requested = 1;
        return;
    }
    case MODE_END:
        to_title();
        return;
    case MODE_MAP:
        if (msg_count) { next_msg(); return; }
        if (map_hover >= 0) script_map_place(map_hover);
        else { mode = MODE_PLAY; need_rebuild = 1; need_full = 1; }
        return;
    default:
        break;
    }

    /* --- gioco --- */
    modal_from_title = 0;
    if (in_rect(x, y, MENU_BTN_X, SENTENCE_Y, 48, 14)) { open_modal(MODAL_PAUSE); return; }
    if (msg_count) { next_msg(); return; }

    if (dlg_npc >= 0) {
        int c = choice_at(x, y);
        if (c >= 0) {
            hover_choice = -1;
            dialog_choose(dlg_npc, dlg_ids[c]);
            panel_dirty = 1;
            if (!msg_count && dlg_ending) { dlg_npc = -1; dlg_ending = 0; }
        }
        return;
    }

    if (y < SCENE_H) {
        int obj = hotspot_at(x, y);
        if (obj == OBJ_NONE) {
            reset_sentence();
            pend_active = 0;
            walk_to(x, y);
        } else {
            click_object(obj);
        }
        return;
    }
    if (y >= PANEL_Y) {
        int v = verb_at(x, y), it = inv_at(x, y);
        if (v >= 0) {
            cur_verb = v;
            sel_item = OBJ_NONE;
            panel_dirty = 1;
        } else if (it >= 0) {
            click_object(it);
        }
    }
}

void game_key(int ch, int command)
{
    if (command) {
        if (ch == 'q' || ch == 'Q') { quit_requested = 1; return; }
        if (ch == 'f' || ch == 'F') {           /* contatore delle prestazioni */
            perf_on = !perf_on;
            perf_t0 = plat_ticks();
            perf_fps = perf_peak_ms = 0;
            need_full = 1;
            return;
        }
        if (ch == 'd' || ch == 'D') {           /* aree cliccabili e zone calpestabili */
            debug_overlay = !debug_overlay;
            need_full = 1;
            return;
        }
        if (mode == MODE_PLAY && (ch == 's' || ch == 'S')) { modal_from_title = 0; open_modal(MODAL_SAVE); return; }
        if (ch == 'l' || ch == 'L' || ch == 'o' || ch == 'O') {
            modal_from_title = mode != MODE_PLAY;
            if (mode == MODE_PLAY || mode == MODE_TITLE) open_modal(MODAL_LOAD);
            return;
        }
        return;
    }
    if (ch == KEY_ESC) {
        if (modal == MODAL_PAUSE || modal_from_title) { modal = MODAL_NONE; need_full = 1; }
        else if (modal != MODAL_NONE) { modal = MODAL_PAUSE; need_full = 1; }
        else if (mode == MODE_PLAY) open_modal(MODAL_PAUSE);
        else if (mode == MODE_MAP) { mode = MODE_PLAY; need_rebuild = 1; }
        return;
    }
    if (modal != MODAL_NONE) return;
    if (mode == MODE_TITLE && ch == KEY_RETURN) { new_game(); return; }
    if (mode == MODE_END) { to_title(); return; }
    if ((ch == ' ' || ch == KEY_RETURN || ch == '.') && msg_count) { next_msg(); return; }
    if (mode == MODE_PLAY && (ch == KEY_TAB || ch == 'i' || ch == 'I') && !msg_count && dlg_npc < 0) {
        modal_from_title = 0;
        open_modal(MODAL_INVENTORY);
        return;
    }
    if (mode == MODE_PLAY && dlg_npc >= 0 && !msg_count && ch >= '1' && ch <= '5') {
        int c = ch - '1';
        if (c < dlg_n) {
            dialog_choose(dlg_npc, dlg_ids[c]);
            panel_dirty = 1;
            if (!msg_count && dlg_ending) { dlg_npc = -1; dlg_ending = 0; }
        }
    }
}


/* ------------------------------------------------------------------ */
/* Ciclo principale                                                    */
/* ------------------------------------------------------------------ */

void game_tick(void)
{
    frame_count++;
    if (modal != MODAL_NONE) return;

    if (msg_count) {
        Msg *m = cur_msg();
        if (!m->text) {
            next_msg();
        } else if (++msg_timer > msg_duration(m)) {
            next_msg();
        }
    }
    if (mode == MODE_PLAY && modal == MODAL_NONE) update_walk();

    /* il protagonista sbatte le palpebre ogni tanto */
    if (blinking) {
        if (--blinking == 0) blink_timer = 50 + (int)(frame_count * 7 % 60);
    } else if (--blink_timer <= 0) {
        blinking = 3;
    }

    /* animazioni degli sfondi */
    if (!need_rebuild && (mode == MODE_PLAY || mode == MODE_TITLE || mode == MODE_END)) {
        GRect box, got[ANIM_RECTS];
        const GRect *avoid = 0;
        int speaker = -1, n, i;
        Msg *m = cur_msg();
        if (m && m->text) {
            box = msg_box_rect(m);
            avoid = &box;
            speaker = m->speaker;
        }
        n = anim_step(speaker, avoid, got, ANIM_RECTS);
        {
            for (i = 0; i < n; i++) {
                if (anim_nrects < ANIM_RECTS) anim_rects[anim_nrects++] = got[i];
                else {
                    GRect *a = &anim_rects[ANIM_RECTS - 1];
                    if (got[i].x0 < a->x0) a->x0 = got[i].x0;
                    if (got[i].y0 < a->y0) a->y0 = got[i].y0;
                    if (got[i].x1 > a->x1) a->x1 = got[i].x1;
                    if (got[i].y1 > a->y1) a->y1 = got[i].y1;
                }
            }
        }
    }
}

int game_wants_quit(void) { return quit_requested; }

int game_debug(int what, int arg)
{
    int i;
    switch (what) {
    case DBG_BUSY:   return msg_count > 0 || walking;
    case DBG_FLAG:   return flag(arg);
    case DBG_HAS:    return has_item(arg);
    case DBG_MODE:   return mode;
    case DBG_ROOM:   return G.room;
    case DBG_MODAL:  return modal;
    case DBG_DIALOG: return dlg_npc;
    case DBG_REDRAW: need_full = 1; return 0;
    case DBG_HX:     return G.hx;
    case DBG_HY:     return G.hy;
    case DBG_SLOT:
        for (i = 0; i < G.ninv; i++) if (G.inv[i] == arg) return i;
        return -1;
    }
    return 0;
}

void game_init(void)
{
    memset(&G, 0, sizeof(G));
    mode = MODE_TITLE;
    need_rebuild = 1;
    need_full = 1;
    mouse_x = mouse_y = -1;
}

