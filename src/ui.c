/*
 * ui.c - pannello (frase, verbi, inventario, scelte), finestre in stile
 * Macintosh, titolo, mappa e schermata finale
 */
#include <string.h>
#include "core.h"
#include "platform.h"

/* ------------------------------------------------------------------ */
/* Pannello (frase, verbi, inventario, scelte di dialogo)               */
/* ------------------------------------------------------------------ */

static const char *const panel_verbs[V_COUNT] = {
    "Dai", "Apri", "Chiudi", "Raccogli", "Guarda", "Spingi", "Usa", "Parla", "Tira"
};


int verb_at(int x, int y)
{
    int i;
    for (i = 0; i < V_COUNT; i++)
        if (in_rect(x, y, VERB_X(i) - 6, VERB_Y(i) - 4, 84, 22)) return i;
    return -1;
}

int inv_at(int x, int y)
{
    int i;
    for (i = 0; i < 8; i++)
        if (in_rect(x, y, INV_X(i), INV_Y(i), 56, 38)) return i < G.ninv ? G.inv[i] : -2;
    return -1;
}

int choice_at(int x, int y)
{
    int i;
    if (dlg_npc < 0 || msg_count) return -1;
    for (i = 0; i < dlg_n; i++)
        if (in_rect(x, y, 8, CHOICE_Y(i) - 1, 496, 16)) return i;
    return -1;
}

void build_sentence(char *buf, int max)
{
    buf[0] = 0;
    if (dlg_npc >= 0) return;
    str_cat(buf, verb_names[cur_verb], max);
    if (sel_item != OBJ_NONE) {
        str_cat(buf, " ", max);
        str_cat(buf, object_name(sel_item), max);
        str_cat(buf, cur_verb == V_DAI ? " a" : " con", max);
    }
    if (hover_obj != OBJ_NONE && hover_obj != sel_item) {
        str_cat(buf, " ", max);
        str_cat(buf, object_name(hover_obj), max);
    }
}

/* riga della frase ("Usa chiave con cassa") e pulsante Menu */
void draw_sentence(void)
{
    char sentence[96];
    gfx_fill(0, SENTENCE_Y, FB_W, PANEL_Y - SENTENCE_Y, PAT_BLACK);
    build_sentence(sentence, sizeof(sentence));
    text_center(SENTENCE_Y + 1, sentence, FONT_REGULAR, COL_WHITE);
    /* pulsante Menu */
    gfx_frame(MENU_BTN_X, SENTENCE_Y + 1, 48, 13, COL_WHITE);
    gfx_text(MENU_BTN_X + 10, SENTENCE_Y, "Menu", FONT_REGULAR, COL_WHITE);
    if (hover_menu == 0) gfx_invert(MENU_BTN_X + 1, SENTENCE_Y + 2, 46, 11);
    sentence_dirty = 0;
}

void draw_panel(void)
{
    int i;

    draw_sentence();
    gfx_fill(0, PANEL_Y, FB_W, FB_H - PANEL_Y, PAT_BLACK);

    if (dlg_npc >= 0) {
        if (!msg_count) {
            dlg_n = dialog_options(dlg_npc, dlg_ids, dlg_texts);
            for (i = 0; i < dlg_n; i++) {
                gfx_text(16, CHOICE_Y(i), dlg_texts[i], i == hover_choice ? FONT_BOLD : FONT_REGULAR, COL_WHITE);
                if (i == hover_choice) gfx_glyph(6, CHOICE_Y(i), '>', FONT_BOLD, COL_WHITE);
            }
        }
        gfx_dirty(0, SENTENCE_Y, FB_W, FB_H);
        return;
    }

    /* verbi */
    gfx_frame(4, PANEL_Y + 2, 264, FB_H - PANEL_Y - 5, COL_WHITE);
    gfx_vline(94, PANEL_Y + 8, FB_H - 9, COL_WHITE);
    gfx_vline(182, PANEL_Y + 8, FB_H - 9, COL_WHITE);
    for (i = 0; i < V_COUNT; i++) {
        int sel = i == cur_verb;
        if (sel) gfx_fill(VERB_X(i) - 6, VERB_Y(i) - 3, 80, 20, PAT_WHITE);
        gfx_text(VERB_X(i), VERB_Y(i), panel_verbs[i],
                 (i == hover_verb || sel) ? FONT_BOLD : FONT_REGULAR, sel ? COL_BLACK : COL_WHITE);
    }

    /* inventario */
    gfx_frame(272, PANEL_Y + 2, 236, FB_H - PANEL_Y - 5, COL_WHITE);
    for (i = 0; i < 8; i++) {
        int x = INV_X(i), y = INV_Y(i);
        gfx_frame(x, y, 56, 38, COL_WHITE);
        if (i < G.ninv) {
            int it = G.inv[i];
            gfx_fill(x + 2, y + 2, 52, 34, PAT_WHITE);
            gfx_bitmap(&asset_icon_small[it], x + 12, y + 3);
            if (it == IT_CAPPELLO && flag(F_HAT_WORN)) gfx_glyph(x + 45, y + 2, '*', FONT_BOLD, COL_BLACK);
            if (it == sel_item) gfx_invert(x + 2, y + 2, 52, 34);
        }
    }
    gfx_dirty(0, SENTENCE_Y, FB_W, FB_H);
}


/* ------------------------------------------------------------------ */
/* Finestre in stile Macintosh                                          */
/* ------------------------------------------------------------------ */


void draw_window(WinRect r, const char *title)
{
    int i, tw;
    gfx_fill(r.x + 3, r.y + 3, r.w, r.h, PAT_BLACK);      /* ombra */
    gfx_fill(r.x, r.y, r.w, r.h, PAT_WHITE);
    gfx_frame(r.x, r.y, r.w, r.h, COL_BLACK);
    gfx_frame(r.x + 3, r.y + 22, r.w - 6, r.h - 25, COL_BLACK);
    for (i = 0; i < 6; i++) gfx_hline(r.x + 3, r.x + r.w - 4, r.y + 5 + i * 2, COL_BLACK);
    tw = gfx_text_width(title) + 14;
    gfx_fill(r.x + 8, r.y + 2, tw, 15, PAT_WHITE);
    gfx_text(r.x + 15, r.y + 3, title, FONT_BOLD, COL_BLACK);
    /* casella di chiusura (a destra, come nel mockup) */
    gfx_fill(r.x + r.w - 24, r.y + 2, 18, 15, PAT_WHITE);
    gfx_frame(r.x + r.w - 21, r.y + 3, 13, 13, COL_BLACK);
    for (i = 0; i < 7; i++) {
        gfx_plot(r.x + r.w - 18 + i, r.y + 6 + i, COL_BLACK);
        gfx_plot(r.x + r.w - 12 - i, r.y + 6 + i, COL_BLACK);
    }
    gfx_hline(r.x, r.x + r.w - 1, r.y + 19, COL_BLACK);
}

int in_close_box(WinRect r, int x, int y)
{
    return in_rect(x, y, r.x + r.w - 24, r.y, 24, 20);
}

void draw_button(int x, int y, int w, const char *label, int hot, int def)
{
    gfx_fill(x, y, w, 20, PAT_WHITE);
    gfx_frame(x, y, w, 20, COL_BLACK);
    if (def) gfx_frame(x - 3, y - 3, w + 6, 26, COL_BLACK);
    gfx_text(x + (w - gfx_text_width(label)) / 2, y + 3, label, FONT_REGULAR, COL_BLACK);
    if (hot) gfx_invert(x + 1, y + 1, w - 2, 18);
}

/* --- menu di pausa --- */
static const char *const pause_items[] = {
    "Riprendi", "Inventario", "Salva partita", "Carica partita",
    "Opzioni", "Torna al titolo", "Esci dal gioco"
};
#define PAUSE_N 7
const WinRect pause_win = {156, 40, 200, 214};

void draw_pause(void)
{
    int i;
    draw_window(pause_win, "Menu");
    for (i = 0; i < PAUSE_N; i++)
        draw_button(pause_win.x + 20, pause_win.y + 32 + i * 25, 160, pause_items[i], i == hover_menu, 0);
}

int pause_at(int x, int y)
{
    int i;
    for (i = 0; i < PAUSE_N; i++)
        if (in_rect(x, y, pause_win.x + 20, pause_win.y + 32 + i * 25, 160, 20)) return i;
    return -1;
}

/* --- salva / carica --- */
const WinRect save_win = {40, 50, 432, 220};

void draw_saveload(void)
{
    int i;
    char buf[80];
    int saving = modal == MODAL_SAVE;
    draw_window(save_win, saving ? "Salva Partita" : "Carica Partita");
    gfx_frame(save_win.x + 12, save_win.y + 32, save_win.w - 24, 8 * 17 + 6, COL_BLACK);
    for (i = 0; i < SAVE_SLOTS; i++) {
        int y = save_win.y + 35 + i * 17;
        slot_label(i, buf, sizeof(buf));
        gfx_text(save_win.x + 20, y + 1, buf, FONT_REGULAR, COL_BLACK);
        if (i == modal_sel) gfx_invert(save_win.x + 14, y, save_win.w - 28, 16);
        else if (i == hover_menu) gfx_frame(save_win.x + 14, y, save_win.w - 28, 16, COL_BLACK);
    }
    draw_button(save_win.x + save_win.w - 112, save_win.y + save_win.h - 34, 96,
                saving ? "Salva" : "Carica", hover_menu == 100, 1);
    draw_button(save_win.x + save_win.w - 222, save_win.y + save_win.h - 34, 96,
                "Annulla", hover_menu == 101, 0);
}

int saveload_at(int x, int y)
{
    int i;
    for (i = 0; i < SAVE_SLOTS; i++)
        if (in_rect(x, y, save_win.x + 14, save_win.y + 35 + i * 17, save_win.w - 28, 16)) return i;
    if (in_rect(x, y, save_win.x + save_win.w - 112, save_win.y + save_win.h - 34, 96, 20)) return 100;
    if (in_rect(x, y, save_win.x + save_win.w - 222, save_win.y + save_win.h - 34, 96, 20)) return 101;
    return -1;
}

/* --- opzioni --- */
const WinRect opt_win = {116, 90, 280, 120};
static const char *const speed_names[3] = {"Lenta", "Normale", "Veloce"};

void draw_options(void)
{
    char buf[48];
    draw_window(opt_win, "Opzioni");
    buf[0] = 0;
    str_cat(buf, "Velocità del testo: ", sizeof(buf));
    str_cat(buf, speed_names[text_speed], sizeof(buf));
    draw_button(opt_win.x + 20, opt_win.y + 38, 240, buf, hover_menu == 0, 0);
    draw_button(opt_win.x + 160, opt_win.y + 80, 100, "OK", hover_menu == 1, 1);
}

int options_at(int x, int y)
{
    if (in_rect(x, y, opt_win.x + 20, opt_win.y + 38, 240, 20)) return 0;
    if (in_rect(x, y, opt_win.x + 160, opt_win.y + 80, 100, 20)) return 1;
    return -1;
}

/* --- inventario grande --- */
const WinRect inv_win = {48, 30, 416, 240};
#define INVW_X(i) (inv_win.x + 10 + ((i) % 4) * 99)
#define INVW_Y(i) (inv_win.y + 28 + ((i) / 4) * 102)

void draw_inventory_window(void)
{
    int i;
    draw_window(inv_win, "Inventario");
    for (i = 0; i < 8; i++) {
        int x = INVW_X(i), y = INVW_Y(i);
        gfx_frame(x, y, 98, 101, COL_BLACK);
        if (i < G.ninv) {
            int it = G.inv[i];
            const char *t = item_title(it);
            gfx_bitmap(&asset_icon_big[it], x + 27, y + 14);
            gfx_text(x + (98 - gfx_text_width(t)) / 2, y + 72, t, FONT_REGULAR, COL_BLACK);
            if (i == hover_menu) gfx_frame(x + 2, y + 2, 94, 97, COL_BLACK);
        }
    }
}

int invwin_at(int x, int y)
{
    int i;
    for (i = 0; i < 8; i++)
        if (in_rect(x, y, INVW_X(i), INVW_Y(i), 98, 101)) return i;
    return -1;
}

void draw_modal(void)
{
    switch (modal) {
    case MODAL_PAUSE:     draw_pause(); break;
    case MODAL_SAVE:
    case MODAL_LOAD:      draw_saveload(); break;
    case MODAL_OPTIONS:   draw_options(); break;
    case MODAL_INVENTORY: draw_inventory_window(); break;
    }
}

void open_modal(int m)
{
    modal = m;
    modal_sel = -1;
    hover_menu = -1;
    if (m == MODAL_SAVE || m == MODAL_LOAD) refresh_slots();
    walking = 0;
    need_full = 1;
}


/* ------------------------------------------------------------------ */
/* Titolo, mappa, finale                                                */
/* ------------------------------------------------------------------ */

static const char *const title_items[4] = {"Nuova Partita", "Carica Partita", "Opzioni", "Esci"};

int title_at(int x, int y)
{
    int i;
    for (i = 0; i < 4; i++) {
        int w = gfx_text_width(title_items[i]) + 16;
        if (in_rect(x, y, (FB_W - w) / 2, TITLE_Y(i) - 2, w, 18)) return i;
    }
    return -1;
}

void draw_title_menu(void)
{
    int i;
    for (i = 0; i < 4; i++) {
        int w = gfx_text_width(title_items[i]);
        gfx_fill((FB_W - w) / 2 - 8, TITLE_Y(i) - 2, w + 16, 18, PAT_BLACK);
        if (i == hover_menu && modal == MODAL_NONE) {
            gfx_fill((FB_W - w) / 2 - 8, TITLE_Y(i) - 2, w + 16, 18, PAT_WHITE);
            gfx_frame((FB_W - w) / 2 - 8, TITLE_Y(i) - 2, w + 16, 18, COL_BLACK);
            text_center(TITLE_Y(i), title_items[i], FONT_BOLD, COL_BLACK);
        } else {
            gfx_text((FB_W - w) / 2, TITLE_Y(i), title_items[i], FONT_REGULAR, COL_WHITE);
        }
    }
    gfx_text(4, FB_H - 14, "v1.1", FONT_REGULAR, COL_WHITE);
}

void draw_title(void)
{
    gfx_copy_from(bgbuf, 0, 0, FB_W, FB_H);
    draw_title_menu();
}


int map_at(int x, int y)
{
    int i;
    for (i = 0; i < map_place_count; i++) {
        const MapPlace *p = &map_places[i];
        if (in_rect(x, y - MAP_Y, p->x, p->y, p->w, p->h)) return i;
    }
    return -1;
}

void draw_map(void)
{
    gfx_fill(0, 0, FB_W, MAP_Y, PAT_BLACK);
    gfx_fill(0, MAP_Y + 282, FB_W, FB_H - MAP_Y - 282, PAT_BLACK);
    memcpy(gfx_fb + MAP_Y * FB_RB, bgbuf, 282 * FB_RB);
    text_center(8, "Mappa di Puerto Polli", FONT_BOLD, COL_WHITE);
    if (map_hover >= 0) {
        const MapPlace *p = &map_places[map_hover];
        gfx_frame(p->x - 1, MAP_Y + p->y - 1, p->w + 2, p->h + 2, COL_BLACK);
        gfx_frame(p->x, MAP_Y + p->y, p->w, p->h, COL_BLACK);
        {
            char buf[48];
            buf[0] = 0;
            str_cat(buf, "Vai a: ", sizeof(buf));
            str_cat(buf, p->name, sizeof(buf));
            text_center(MAP_Y + 290, buf, FONT_BOLD, COL_WHITE);
        }
    } else {
        text_center(MAP_Y + 290, "Scegli una destinazione  (clic fuori per tornare)", FONT_REGULAR, COL_WHITE);
    }
    gfx_dirty_all();
}

void draw_end(void)
{
    memcpy(gfx_fb, bgbuf, SCENE_H * FB_RB);
    gfx_fill(0, SCENE_H, FB_W, FB_H - SCENE_H, PAT_BLACK);
    gfx_hline(0, FB_W - 1, SCENE_H + 2, COL_WHITE);
    text_center(SCENE_H + 12, "~  F I N E  ~", FONT_BOLD, COL_WHITE);
    text_center(SCENE_H + 34, "Sei ufficialmente un Pirata di Puerto Polli!", FONT_REGULAR, COL_WHITE);
    text_center(SCENE_H + 52, "Grazie per aver giocato.", FONT_REGULAR, COL_WHITE);
    text_center(SCENE_H + 80, "Clicca per tornare al titolo", FONT_REGULAR, COL_WHITE);
    gfx_dirty_all();
}

