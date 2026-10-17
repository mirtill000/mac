/*
 * core.h - stato e funzioni condivise tra i moduli del motore
 * (engine.c, msg.c, walk.c, save.c, ui.c). Non e' un'interfaccia pubblica:
 * la piattaforma usa solo game.h, la storia solo engine.h.
 */
#ifndef CORE_H
#define CORE_H

#include "game.h"
#include "engine.h"
#include "gfx.h"

enum { MODE_TITLE, MODE_PLAY, MODE_MAP, MODE_END };
enum { MODAL_NONE, MODAL_PAUSE, MODAL_SAVE, MODAL_LOAD, MODAL_OPTIONS, MODAL_INVENTORY };

#define FLAG_BYTES 16            /* 128 flag della storia */

typedef struct {
    unsigned char room;
    signed char dir;
    short hx, hy;
    unsigned char flags[FLAG_BYTES];
    unsigned char ninv;
    signed char inv[INV_MAX];
} GameState;

extern GameState G;

extern int mode, modal, modal_from_title, quit_requested, text_speed;
extern int chapter;               /* MODE_END: 0 finale, 2 o 3 cartello della parte */
extern unsigned long frame_count;

/* camminata */
extern short tx, ty;
extern int walking, walk_anim;

/* azione in attesa (eseguita all'arrivo) */
extern int pend_active, pend_verb, pend_o1, pend_o2, pend_face, pend_exit;

/* interfaccia */
extern int cur_verb, sel_item, hover_obj, hover_verb, hover_choice, hover_menu;
extern int mouse_x, mouse_y;

/* messaggi */
typedef struct {
    signed char speaker, call;
    short bytes, nchar;          /* porzione di testo di questo box */
    const char *text;
} Msg;
#define MSG_MAX 64
#define MSG_LINES 4              /* righe massime per box; oltre si spezza */
extern int msg_overflow;
extern Msg msgs[MSG_MAX];
extern int msg_head, msg_count, msg_timer;

/* dialoghi */
extern int dlg_npc, dlg_ending, dlg_n;
extern int dlg_ids[MAX_CHOICES];
extern const char *dlg_texts[MAX_CHOICES];

/* finestre */
extern int modal_sel, map_hover;

/* rendering */
extern unsigned char bgbuf[FB_H * FB_RB], basebuf[FB_H * FB_RB];
extern int need_full, need_rebuild, panel_dirty, sentence_dirty;
extern int need_modal;         /* finestra aperta, chiusa o cambiata */
extern int need_modal_hover;   /* nella finestra cambia solo l'evidenziazione */
extern int need_title_menu;    /* titolo: cambia la voce evidenziata */
extern int panel_shown;        /* a video c'e' il pannello del gioco, aggiornato */
extern GRect hero_prev, box_prev;
extern int hero_prev_valid, box_prev_valid;
extern long hero_prev_key, box_prev_key;
extern int tri_shown;

/* animazioni: zone dello sfondo cambiate e non ancora portate a video */
#define ANIM_RECTS 8
extern GRect anim_rects[ANIM_RECTS];
extern int anim_nrects;

/* il protagonista sbatte le palpebre */
extern int blink_timer, blinking;

/* salvataggi */
#define SAVE_SLOTS 8
#define SAVE_LEN 96
#define SAVE_VERSION 2
typedef struct { int room, day, month, year, hour, minute; } SaveInfo;
extern unsigned char slot_data[SAVE_SLOTS][SAVE_LEN];
extern int slot_used[SAVE_SLOTS], slot_len[SAVE_SLOTS];

/* layout dello schermo */
#define VERB_X(i)   (16 + ((i) / 3) * 88)
#define VERB_Y(i)   (263 + ((i) % 3) * 24)
#define INV_X(i)    (277 + ((i) % 4) * 58)
#define INV_Y(i)    (259 + ((i) / 4) * 40)
#define MENU_BTN_X  458
#define CHOICE_Y(i) (258 + (i) * 16)
#define TITLE_Y(i)  (262 + (i) * 19)

/* finestre in stile Macintosh */
typedef struct { short x, y, w, h; } WinRect;
extern const WinRect pause_win, save_win, opt_win, inv_win;

/* strumenti di sviluppo: contatore delle prestazioni e disegno delle aree */
extern int perf_on, perf_fps, perf_peak_ms, debug_overlay;

/* --- msg.c: messaggi e box dei dialoghi --- */
Msg  *cur_msg(void);
void  run_call(int c);
void  next_msg(void);
int   msg_duration(const Msg *m);
GRect msg_box_rect(const Msg *m);
void  draw_triangle(GRect r, int on);
void  draw_msg(void);
int   wrap_l1(const unsigned char *s, int n, int maxc, int starts[], int lens[], int maxlines);
/* utilita' di testo */
int   in_rect(int x, int y, int rx, int ry, int rw, int rh);
void  str_cat(char *d, const char *s, int max);
void  num_cat(char *d, int v, int digits, int max);
void  text_center(int y, const char *s, int font, int col);

/* --- walk.c: camminata, oggetti cliccabili, azioni --- */
int   in_poly(const WalkPoly *p, int x, int y);
int   walkable(int x, int y);
void  clamp_walk(short *x, short *y);
void  walk_to(int x, int y);
int   hotspot_at(int x, int y);
void  reset_sentence(void);
void  execute(int verb, int o1, int o2, int exit_to);
void  act(int verb, int o1, int o2, int where);
int   usable_alone(int it);
void  click_object(int obj);
void  update_walk(void);

/* --- save.c: salvataggi, nuova partita --- */
void  put16(unsigned char *p, int v);
int   get16(const unsigned char *p);
void  serialize(unsigned char *d);
int   deserialize(const unsigned char *d, int len);
int   save_info(const unsigned char *d, int len, SaveInfo *si);
void  refresh_slots(void);
void  slot_label(int i, char *buf, int max);
void  clear_interaction(void);
void  do_save(int slot);
void  do_load(int slot);
void  new_game(void);
void  to_title(void);

/* --- ui.c: pannello, finestre, titolo, mappa, finale --- */
int   verb_at(int x, int y);
int   inv_at(int x, int y);
int   choice_at(int x, int y);
void  build_sentence(char *buf, int max);
void  draw_sentence(void);
void  menu_button_off(void);
void  draw_panel(void);
void  panel_update(void);
void  panel_invalidate(void);
void  draw_window(WinRect r, const char *title);
int   in_close_box(WinRect r, int x, int y);
void  draw_button(int x, int y, int w, const char *label, int hot, int def);
void  draw_pause(void);
int   pause_at(int x, int y);
void  draw_saveload(void);
int   saveload_at(int x, int y);
void  draw_options(void);
int   options_at(int x, int y);
void  draw_inventory_window(void);
int   invwin_at(int x, int y);
void  draw_modal(void);
void  modal_hover_update(void);
void  open_modal(int m);
int   title_at(int x, int y);
void  draw_title_menu(void);
void  draw_title(void);
int   map_at(int x, int y);
void  draw_map(void);
void  map_hover_changed(int old);
void  draw_end(void);

/* --- engine.c: rendering e protagonista --- */
void  rebuild_background(void);
int   hero_size(void);
int   hero_frame(void);
GRect hero_rect(void);
long  hero_key(void);
long  box_key(void);
int   overlap(GRect a, GRect b);
void  hero_draw(void);
void  draw_play_overlays(void);
void  update_play_overlays(void);
void  render_full(void);
void  click_modal(int x, int y);

#endif
