/*
 * engine.c - il motore dell'avventura: modalita' di gioco, camminata,
 * interfaccia a verbi, inventario, messaggi, dialoghi, mappa, finestre
 * (menu, salvataggi, opzioni, inventario) e rendering.
 *
 * Il codice e' C portabile: non usa il Toolbox direttamente. Tutto viene
 * disegnato in gfx_fb e la piattaforma lo porta a video (plat_present).
 */
#include <string.h>
#include "game.h"
#include "engine.h"
#include "gfx.h"
#include "platform.h"
#include "anim.h"

/* ------------------------------------------------------------------ */
/* Stato                                                              */
/* ------------------------------------------------------------------ */

enum { MODE_TITLE, MODE_PLAY, MODE_MAP, MODE_END };
enum { MODAL_NONE, MODAL_PAUSE, MODAL_SAVE, MODAL_LOAD, MODAL_OPTIONS, MODAL_INVENTORY };

typedef struct {
    unsigned char room;
    signed char dir;
    short hx, hy;
    unsigned long flags;
    unsigned char ninv;
    signed char inv[IT_COUNT];
} GameState;

static GameState G;

static int mode = MODE_TITLE;
static int modal = MODAL_NONE;
static int modal_from_title;
static int quit_requested;
static int text_speed = 1;                 /* 0 lenta, 1 normale, 2 veloce */
static unsigned long frame_count;

/* camminata */
static short tx, ty;
static int walking, walk_anim;

/* azione in attesa (eseguita all'arrivo) */
static int pend_active, pend_verb, pend_o1, pend_o2, pend_face, pend_exit;

/* interfaccia */
static int cur_verb = V_VAI;
static int sel_item = OBJ_NONE;
static int hover_obj = OBJ_NONE;
static int hover_verb = -1;
static int hover_choice = -1;
static int hover_menu = -1;
static int mouse_x, mouse_y;

/* messaggi */
typedef struct { signed char speaker; signed char call; const char *text; } Msg;
#define MSG_MAX 48
static Msg msgs[MSG_MAX];
static int msg_head, msg_count, msg_timer;

/* dialoghi */
static int dlg_npc = -1;
static int dlg_ending;
static int dlg_ids[MAX_CHOICES];
static const char *dlg_texts[MAX_CHOICES];
static int dlg_n;

/* finestre */
static int modal_sel = -1;
static int map_hover = -1;

/* rendering */
static unsigned char bgbuf[FB_H * FB_RB];
static int need_full = 1, need_rebuild = 1, panel_dirty = 1;
/* elementi sopra lo sfondo nel fotogramma precedente */
static GRect hero_prev, box_prev;
static int hero_prev_valid, box_prev_valid;
static long hero_prev_key, box_prev_key;

/* sfondo originale (con gli oggetti di scena); bgbuf e' quello animato */
static unsigned char basebuf[FB_H * FB_RB];

/* animazioni: zone dello sfondo cambiate e non ancora portate a video */
#define ANIM_RECTS 8
static GRect anim_rects[ANIM_RECTS];
static int anim_nrects;

/* il protagonista sbatte le palpebre */
static int blink_timer = 60, blinking;

/* dati dei salvataggi (per la lista) */
#define SAVE_SLOTS 8
#define SAVE_LEN 64
static unsigned char slot_data[SAVE_SLOTS][SAVE_LEN];
static int slot_used[SAVE_SLOTS];

static void hero_draw(void);
static int hero_size(void);
static void render_full(void);

/* ------------------------------------------------------------------ */
/* Servizi per i copioni                                              */
/* ------------------------------------------------------------------ */

void say(int speaker, const char *text)
{
    if (msg_count >= MSG_MAX) return;
    {
        Msg *m = &msgs[(msg_head + msg_count) % MSG_MAX];
        m->speaker = (signed char)speaker;
        m->call = CALL_NONE;
        m->text = text;
    }
    if (msg_count == 0) msg_timer = 0;
    msg_count++;
    need_full = 1;
}

void queue_call(int what)
{
    if (msg_count >= MSG_MAX) return;
    {
        Msg *m = &msgs[(msg_head + msg_count) % MSG_MAX];
        m->speaker = -1;
        m->call = (signed char)what;
        m->text = 0;
    }
    msg_count++;
}

int has_item(int it)
{
    int i;
    for (i = 0; i < G.ninv; i++) if (G.inv[i] == it) return 1;
    return 0;
}

void add_item(int it)
{
    if (has_item(it) || G.ninv >= IT_COUNT) return;
    G.inv[G.ninv++] = (signed char)it;
    panel_dirty = 1;
}

void remove_item(int it)
{
    int i, j;
    for (i = 0; i < G.ninv; i++) {
        if (G.inv[i] == it) {
            for (j = i; j < G.ninv - 1; j++) G.inv[j] = G.inv[j + 1];
            G.ninv--;
            break;
        }
    }
    if (sel_item == it) sel_item = OBJ_NONE;
    panel_dirty = 1;
}

int  flag(int f)       { return (G.flags >> f) & 1; }
void set_flag(int f)   { G.flags |= 1UL << f; need_rebuild = 1; }
void clear_flag(int f) { G.flags &= ~(1UL << f); need_rebuild = 1; }
int  current_room(void) { return G.room; }
void hero_face(int dir) { if (dir) G.dir = (signed char)dir; }

void draw_prop(int item, int x, int y)
{
    const Bitmap *b = &asset_icon_small[item];
    gfx_masked(b->bits, asset_icon_mask + item * b->rowBytes * b->h,
               b->w, b->h, b->rowBytes, x, y);
}

void go_room(int room)
{
    int from = G.room;
    G.room = (unsigned char)room;
    walking = 0;
    pend_active = 0;
    switch (room) {
    case ROOM_DOCK:
        if (from == ROOM_TAVERN && mode == MODE_PLAY) { G.hx = 428; G.hy = 204; G.dir = -1; }
        else { G.hx = 34; G.hy = 226; G.dir = 1; }
        break;
    case ROOM_TAVERN: G.hx = 34;  G.hy = 234; G.dir = 1; break;
    case ROOM_BEACH:  G.hx = 60;  G.hy = 226; G.dir = 1; break;
    default:          G.hx = 40;  G.hy = 226; G.dir = 1; break;
    }
    mode = MODE_PLAY;
    need_rebuild = 1;
    need_full = 1;
    script_enter_room(room);
}

void open_map(void)
{
    mode = MODE_MAP;
    map_hover = -1;
    walking = 0;
    pend_active = 0;
    need_rebuild = 1;
    need_full = 1;
}

void start_dialog(int npc)
{
    dlg_npc = npc;
    dlg_ending = 0;
    hover_choice = -1;
    panel_dirty = 1;
    dialog_greeting(npc);
}

void end_dialog(void) { dlg_ending = 1; }

/* ------------------------------------------------------------------ */
/* Utilita'                                                           */
/* ------------------------------------------------------------------ */

static int in_rect(int x, int y, int rx, int ry, int rw, int rh)
{
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

static void str_cat(char *d, const char *s, int max)
{
    int n = (int)strlen(d);
    while (*s && n < max - 1) d[n++] = *s++;
    d[n] = 0;
}

static void num_cat(char *d, int v, int digits, int max)
{
    char t[8];
    int i;
    for (i = digits - 1; i >= 0; i--) { t[i] = (char)('0' + v % 10); v /= 10; }
    t[digits] = 0;
    str_cat(d, t, max);
}

/* Divide il testo in righe di al massimo maxc caratteri. */
#define MAX_LINES 8
static int wrap_text(const char *s, int maxc, const char *starts[], int lens[])
{
    int n = 0;
    while (*s && n < MAX_LINES) {
        const char *p = s, *last_sp = 0;
        int c = 0, c_at_sp = 0;
        while (*p && c < maxc) {
            const char *q = p;
            int ch = gfx_next_char(&q);
            if (ch == ' ') { last_sp = p; c_at_sp = c; }
            p = q;
            c++;
        }
        starts[n] = s;
        if (!*p) {
            lens[n++] = c;
            break;
        }
        if (last_sp) {
            lens[n++] = c_at_sp;
            s = last_sp + 1;
        } else {
            lens[n++] = c;
            s = p;
        }
    }
    return n;
}

/* disegna i primi n caratteri della stringa */
static void text_n(int x, int y, const char *s, int n, int font, int col)
{
    while (n-- > 0 && *s) {
        int c = gfx_next_char(&s);
        gfx_glyph(x, y, c, font, col);
        x += FONT_W;
    }
}

static void text_center(int y, const char *s, int font, int col)
{
    gfx_text((FB_W - gfx_text_width(s)) / 2, y, s, font, col);
}

/* ------------------------------------------------------------------ */
/* Messaggi                                                           */
/* ------------------------------------------------------------------ */

static Msg *cur_msg(void) { return msg_count ? &msgs[msg_head] : 0; }

static void run_call(int c)
{
    switch (c) {
    case CALL_ENDING:
        mode = MODE_END;
        dlg_npc = -1;
        need_rebuild = 1;
        need_full = 1;
        break;
    default:
        break;
    }
}

static void next_msg(void)
{
    if (!msg_count) return;
    msg_head = (msg_head + 1) % MSG_MAX;
    msg_count--;
    msg_timer = 0;
    /* esegue le chiamate differite in testa alla coda */
    while (msg_count && msgs[msg_head].call != CALL_NONE) {
        int c = msgs[msg_head].call;
        msg_head = (msg_head + 1) % MSG_MAX;
        msg_count--;
        run_call(c);
    }
    if (!msg_count && dlg_ending) {
        dlg_npc = -1;
        dlg_ending = 0;
    }
    need_full = 1;
    panel_dirty = 1;
}

static int msg_duration(const Msg *m)
{
    int len = gfx_text_len(m->text);
    static const int per10[3] = {22, 15, 9};   /* fotogrammi ogni 10 caratteri */
    return 30 + len * per10[text_speed] / 10;
}

static GRect msg_box_rect(const Msg *m, int *nlines, const char *starts[], int lens[])
{
    GRect r;
    int has_name = m->speaker != SP_HERO && m->speaker != SP_NARRATORE;
    *nlines = wrap_text(m->text, 66, starts, lens);
    r.x0 = 12; r.x1 = FB_W - 12;
    r.y0 = 6;
    r.y1 = (short)(r.y0 + 10 + (has_name ? 15 : 0) + *nlines * FONT_H + 8);
    return r;
}

static void draw_msg(void)
{
    const char *starts[MAX_LINES];
    int lens[MAX_LINES], n, i, y;
    Msg *m = cur_msg();
    GRect r;
    if (!m || !m->text) return;
    r = msg_box_rect(m, &n, starts, lens);
    gfx_fill(r.x0, r.y0, r.x1 - r.x0, r.y1 - r.y0, PAT_BLACK);
    gfx_frame(r.x0 + 2, r.y0 + 2, r.x1 - r.x0 - 4, r.y1 - r.y0 - 4, COL_WHITE);
    /* angoli arrotondati */
    gfx_plot(r.x0 + 2, r.y0 + 2, COL_BLACK);
    gfx_plot(r.x1 - 3, r.y0 + 2, COL_BLACK);
    gfx_plot(r.x0 + 2, r.y1 - 3, COL_BLACK);
    gfx_plot(r.x1 - 3, r.y1 - 3, COL_BLACK);
    y = r.y0 + 8;
    if (m->speaker != SP_HERO && m->speaker != SP_NARRATORE) {
        gfx_text(r.x0 + 12, y, speaker_names[m->speaker], FONT_BOLD, COL_WHITE);
        y += 15;
    }
    for (i = 0; i < n; i++, y += FONT_H)
        text_n(r.x0 + 12, y, starts[i], lens[i], FONT_REGULAR, COL_WHITE);
    if ((frame_count / 8) & 1)
        gfx_glyph(r.x1 - 18, r.y1 - 18, GLYPH_TRIANGLE, FONT_REGULAR, COL_WHITE);
    gfx_dirty(r.x0, r.y0, r.x1, r.y1);
    box_prev = r;
    box_prev_valid = 1;
}

/* ------------------------------------------------------------------ */
/* Camminata e azioni                                                  */
/* ------------------------------------------------------------------ */

static void clamp_walk(short *x, short *y)
{
    const WalkArea *w = &walk_areas[G.room];
    if (*x < w->x0) *x = w->x0;
    if (*x > w->x1) *x = w->x1;
    if (*y < w->y0) *y = w->y0;
    if (*y > w->y1) *y = w->y1;
}

static void walk_to(int x, int y)
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

static int hotspot_at(int x, int y)
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

static void reset_sentence(void)
{
    cur_verb = V_VAI;
    sel_item = OBJ_NONE;
    panel_dirty = 1;
}

static void execute(int verb, int o1, int o2, int exit_to)
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
static void act(int verb, int o1, int o2, int where)
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
static int usable_alone(int it)
{
    return it == IT_MAPPA || it == IT_CAPPELLO || it == IT_LIBRO;
}

static void click_object(int obj)
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

static void update_walk(void)
{
    int dx, dy, sx, sy;
    if (!walking) return;
    dx = tx - G.hx;
    dy = ty - G.hy;
    {
        static const signed char speed[HERO_SIZES] = {4, 5, 5, 6};
        int v = speed[hero_size()];
        sx = dx > v ? v : (dx < -v ? -v : dx);
    }
    sy = dy > 2 ? 2 : (dy < -2 ? -2 : dy);
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

/* ------------------------------------------------------------------ */
/* Salvataggi                                                          */
/* ------------------------------------------------------------------ */

static void put16(unsigned char *p, int v) { p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; }
static int  get16(const unsigned char *p)  { return (short)((p[0] << 8) | p[1]); }

static void serialize(unsigned char *d)
{
    int i, day, mon, year, hour, min;
    memset(d, 0, SAVE_LEN);
    d[0] = 'P'; d[1] = 'P'; d[2] = 'S'; d[3] = '1';
    d[4] = G.room;
    d[5] = (unsigned char)G.dir;
    put16(d + 6, G.hx);
    put16(d + 8, G.hy);
    d[10] = (unsigned char)(G.flags >> 24);
    d[11] = (unsigned char)(G.flags >> 16);
    d[12] = (unsigned char)(G.flags >> 8);
    d[13] = (unsigned char)G.flags;
    d[14] = G.ninv;
    for (i = 0; i < IT_COUNT; i++) d[15 + i] = (unsigned char)G.inv[i];
    plat_datetime(&day, &mon, &year, &hour, &min);
    d[23] = (unsigned char)day;
    d[24] = (unsigned char)mon;
    put16(d + 25, year);
    d[27] = (unsigned char)hour;
    d[28] = (unsigned char)min;
}

static int deserialize(const unsigned char *d)
{
    int i;
    if (d[0] != 'P' || d[1] != 'P' || d[2] != 'S' || d[3] != '1') return -1;
    if (d[4] >= ROOM_COUNT || d[14] > IT_COUNT) return -1;
    memset(&G, 0, sizeof(G));
    G.room = d[4];
    G.dir = (signed char)d[5];
    G.hx = (short)get16(d + 6);
    G.hy = (short)get16(d + 8);
    G.flags = ((unsigned long)d[10] << 24) | ((unsigned long)d[11] << 16) |
              ((unsigned long)d[12] << 8) | d[13];
    G.ninv = d[14];
    for (i = 0; i < IT_COUNT; i++) G.inv[i] = (signed char)d[15 + i];
    return 0;
}

static void refresh_slots(void)
{
    int i;
    for (i = 0; i < SAVE_SLOTS; i++)
        slot_used[i] = plat_load(i, slot_data[i], SAVE_LEN) == 0 && slot_data[i][0] == 'P';
}

static void slot_label(int i, char *buf, int max)
{
    buf[0] = 0;
    num_cat(buf, i + 1, 1, max);
    str_cat(buf, ".  ", max);
    if (!slot_used[i]) {
        str_cat(buf, "Vuoto", max);
        return;
    }
    {
        const unsigned char *d = slot_data[i];
        int n;
        str_cat(buf, "Puerto Polli - ", max);
        str_cat(buf, d[4] < ROOM_COUNT ? room_title(d[4]) : "?", max);
        n = gfx_text_len(buf);
        while (n++ < 36) str_cat(buf, " ", max);
        num_cat(buf, d[23], 2, max); str_cat(buf, "/", max);
        num_cat(buf, d[24], 2, max); str_cat(buf, "/", max);
        num_cat(buf, get16(d + 25), 4, max); str_cat(buf, "  ", max);
        num_cat(buf, d[27], 2, max); str_cat(buf, ":", max);
        num_cat(buf, d[28], 2, max);
    }
}

static void clear_interaction(void)
{
    msg_count = 0;
    msg_head = 0;
    dlg_npc = -1;
    dlg_ending = 0;
    walking = 0;
    pend_active = 0;
    reset_sentence();
}

static void do_save(int slot)
{
    unsigned char d[SAVE_LEN];
    serialize(d);
    if (plat_save(slot, d, SAVE_LEN) == 0) {
        modal = MODAL_NONE;
        say(SP_NARRATORE, "Partita salvata.");
    } else {
        plat_beep();
        modal = MODAL_NONE;
        say(SP_NARRATORE, "Impossibile salvare la partita. Il disco è protetto o pieno?");
    }
}

static void do_load(int slot)
{
    if (!slot_used[slot] || deserialize(slot_data[slot]) != 0) {
        plat_beep();
        return;
    }
    clear_interaction();
    modal = MODAL_NONE;
    mode = MODE_PLAY;
    need_rebuild = 1;
    need_full = 1;
}

/* ------------------------------------------------------------------ */
/* Nuova partita / titolo                                              */
/* ------------------------------------------------------------------ */

static void new_game(void)
{
    memset(&G, 0, sizeof(G));
    clear_interaction();
    G.room = ROOM_DOCK;
    mode = MODE_PLAY;
    go_room(ROOM_DOCK);
    G.hx = 60;
    script_new_game();
}

static void to_title(void)
{
    clear_interaction();
    mode = MODE_TITLE;
    modal = MODAL_NONE;
    need_rebuild = 1;
    need_full = 1;
}

/* ------------------------------------------------------------------ */
/* Pannello (frase, verbi, inventario, scelte di dialogo)               */
/* ------------------------------------------------------------------ */

static const char *const panel_verbs[V_COUNT] = {
    "Dai", "Apri", "Chiudi", "Raccogli", "Guarda", "Spingi", "Usa", "Parla", "Tira"
};

#define VERB_X(i)  (16 + ((i) / 3) * 88)
#define VERB_Y(i)  (263 + ((i) % 3) * 24)
#define INV_X(i)   (277 + ((i) % 4) * 58)
#define INV_Y(i)   (259 + ((i) / 4) * 40)
#define MENU_BTN_X 458
#define CHOICE_Y(i) (258 + (i) * 16)

static int verb_at(int x, int y)
{
    int i;
    for (i = 0; i < V_COUNT; i++)
        if (in_rect(x, y, VERB_X(i) - 6, VERB_Y(i) - 4, 84, 22)) return i;
    return -1;
}

static int inv_at(int x, int y)
{
    int i;
    for (i = 0; i < 8; i++)
        if (in_rect(x, y, INV_X(i), INV_Y(i), 56, 38)) return i < G.ninv ? G.inv[i] : -2;
    return -1;
}

static int choice_at(int x, int y)
{
    int i;
    if (dlg_npc < 0 || msg_count) return -1;
    for (i = 0; i < dlg_n; i++)
        if (in_rect(x, y, 8, CHOICE_Y(i) - 1, 496, 16)) return i;
    return -1;
}

static void build_sentence(char *buf, int max)
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

static void draw_panel(void)
{
    char sentence[96];
    int i;

    gfx_fill(0, SENTENCE_Y, FB_W, FB_H - SENTENCE_Y, PAT_BLACK);

    build_sentence(sentence, sizeof(sentence));
    text_center(SENTENCE_Y + 1, sentence, FONT_REGULAR, COL_WHITE);
    /* pulsante Menu */
    gfx_frame(MENU_BTN_X, SENTENCE_Y + 1, 48, 13, COL_WHITE);
    gfx_text(MENU_BTN_X + 10, SENTENCE_Y, "Menu", FONT_REGULAR, COL_WHITE);
    if (hover_menu == 0) gfx_invert(MENU_BTN_X + 1, SENTENCE_Y + 2, 46, 11);

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

typedef struct { short x, y, w, h; } WinRect;

static void draw_window(WinRect r, const char *title)
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

static int in_close_box(WinRect r, int x, int y)
{
    return in_rect(x, y, r.x + r.w - 24, r.y, 24, 20);
}

static void draw_button(int x, int y, int w, const char *label, int hot, int def)
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
static const WinRect pause_win = {156, 40, 200, 214};

static void draw_pause(void)
{
    int i;
    draw_window(pause_win, "Menu");
    for (i = 0; i < PAUSE_N; i++)
        draw_button(pause_win.x + 20, pause_win.y + 32 + i * 25, 160, pause_items[i], i == hover_menu, 0);
}

static int pause_at(int x, int y)
{
    int i;
    for (i = 0; i < PAUSE_N; i++)
        if (in_rect(x, y, pause_win.x + 20, pause_win.y + 32 + i * 25, 160, 20)) return i;
    return -1;
}

/* --- salva / carica --- */
static const WinRect save_win = {40, 50, 432, 220};

static void draw_saveload(void)
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

static int saveload_at(int x, int y)
{
    int i;
    for (i = 0; i < SAVE_SLOTS; i++)
        if (in_rect(x, y, save_win.x + 14, save_win.y + 35 + i * 17, save_win.w - 28, 16)) return i;
    if (in_rect(x, y, save_win.x + save_win.w - 112, save_win.y + save_win.h - 34, 96, 20)) return 100;
    if (in_rect(x, y, save_win.x + save_win.w - 222, save_win.y + save_win.h - 34, 96, 20)) return 101;
    return -1;
}

/* --- opzioni --- */
static const WinRect opt_win = {116, 90, 280, 120};
static const char *const speed_names[3] = {"Lenta", "Normale", "Veloce"};

static void draw_options(void)
{
    char buf[48];
    draw_window(opt_win, "Opzioni");
    buf[0] = 0;
    str_cat(buf, "Velocità del testo: ", sizeof(buf));
    str_cat(buf, speed_names[text_speed], sizeof(buf));
    draw_button(opt_win.x + 20, opt_win.y + 38, 240, buf, hover_menu == 0, 0);
    draw_button(opt_win.x + 160, opt_win.y + 80, 100, "OK", hover_menu == 1, 1);
}

static int options_at(int x, int y)
{
    if (in_rect(x, y, opt_win.x + 20, opt_win.y + 38, 240, 20)) return 0;
    if (in_rect(x, y, opt_win.x + 160, opt_win.y + 80, 100, 20)) return 1;
    return -1;
}

/* --- inventario grande --- */
static const WinRect inv_win = {48, 30, 416, 240};
#define INVW_X(i) (inv_win.x + 10 + ((i) % 4) * 99)
#define INVW_Y(i) (inv_win.y + 28 + ((i) / 4) * 102)

static void draw_inventory_window(void)
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

static int invwin_at(int x, int y)
{
    int i;
    for (i = 0; i < 8; i++)
        if (in_rect(x, y, INVW_X(i), INVW_Y(i), 98, 101)) return i;
    return -1;
}

static void draw_modal(void)
{
    switch (modal) {
    case MODAL_PAUSE:     draw_pause(); break;
    case MODAL_SAVE:
    case MODAL_LOAD:      draw_saveload(); break;
    case MODAL_OPTIONS:   draw_options(); break;
    case MODAL_INVENTORY: draw_inventory_window(); break;
    }
}

static void open_modal(int m)
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
#define TITLE_Y(i) (262 + (i) * 19)

static int title_at(int x, int y)
{
    int i;
    for (i = 0; i < 4; i++) {
        int w = gfx_text_width(title_items[i]) + 16;
        if (in_rect(x, y, (FB_W - w) / 2, TITLE_Y(i) - 2, w, 18)) return i;
    }
    return -1;
}

static void draw_title_menu(void)
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

static void draw_title(void)
{
    gfx_copy_from(bgbuf, 0, 0, FB_W, FB_H);
    draw_title_menu();
}

#define MAP_Y 30

static int map_at(int x, int y)
{
    int i;
    for (i = 0; i < map_place_count; i++) {
        const MapPlace *p = &map_places[i];
        if (in_rect(x, y - MAP_Y, p->x, p->y, p->w, p->h)) return i;
    }
    return -1;
}

static void draw_map(void)
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

static void draw_end(void)
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

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */

static void rebuild_background(void)
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
static int hero_size(void)
{
    const RoomDepth *d = &room_depth[G.room];
    int h, i, best = 0;
    if (d->y_near == d->y_far) h = d->h_near;
    else h = d->h_far + (G.hy - d->y_far) * (d->h_near - d->h_far) / (d->y_near - d->y_far);
    for (i = 1; i < HERO_SIZES; i++) {
        int a = asset_hero_height[i] - h, b = asset_hero_height[best] - h;
        if ((a < 0 ? -a : a) < (b < 0 ? -b : b)) best = i;
    }
    return best;
}

static int hero_frame(void)
{
    int base = flag(F_HAT_WORN) ? 9 : 0;
    Msg *m = cur_msg();
    if (walking) return base + 2 + (walk_anim % 6);
    if (m && m->text && m->speaker == SP_HERO && ((frame_count / 3) & 1)) return base + 1;
    if (blinking) return base + 8;
    return base;
}

static GRect hero_rect(void)
{
    const Sprite *s = &asset_hero[hero_size()][0];
    GRect r;
    r.x0 = (short)(G.hx - s->w / 2);
    r.y0 = (short)(G.hy - s->h + 1);
    r.x1 = (short)(r.x0 + s->w);
    r.y1 = (short)(G.hy + 4);                 /* comprende l'ombra a terra */
    return r;
}

static long hero_key(void)
{
    return ((long)hero_frame() << 24) ^ ((long)hero_size() << 20) ^ ((long)(G.dir < 0) << 19) ^
           ((long)G.hx << 8) ^ G.hy;
}

static long box_key(void)
{
    Msg *m = cur_msg();
    if (!m || !m->text) return 0;
    return 1 + ((long)msg_head << 1) + ((frame_count / 8) & 1) + ((long)msg_count << 8);
}

static int overlap(GRect a, GRect b)
{
    return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

static void hero_draw(void)
{
    int f = hero_frame();
    GRect r = hero_rect();
    const Sprite *s = &asset_hero[hero_size()][f];
    gfx_clip(0, 0, FB_W, SCENE_H);
    /* ombra sotto i piedi: lega il personaggio al pavimento */
    gfx_shadow(G.hx + (G.dir < 0 ? 1 : -1), G.hy + 1, s->h / 6, 2);
    if (G.dir < 0)
        gfx_masked_mirror(s->bits, s->mask, s->w, s->h, s->rowBytes, r.x0, r.y0);
    else
        gfx_masked(s->bits, s->mask, s->w, s->h, s->rowBytes, r.x0, r.y0);
    gfx_noclip();
    hero_prev = r;
    hero_prev_valid = 1;
    hero_prev_key = hero_key();
}

/* disegna tutto sopra lo sfondo appena copiato */
static void draw_play_overlays(void)
{
    hero_draw();
    box_prev_valid = 0;
    draw_msg();
    box_prev_key = box_key();
}

/* aggiornamento incrementale: solo cio' che e' cambiato */
static void update_play_overlays(void)
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
    }
}

static void render_full(void)
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
        draw_panel();
        panel_dirty = 0;
        break;
    }
    if (modal != MODAL_NONE) draw_modal();
    gfx_dirty_all();
}

void game_render(void)
{
    GRect r;
    if (need_rebuild) { rebuild_background(); need_full = 1; }
    if (need_full || modal != MODAL_NONE || mode != MODE_PLAY) {
        /* le finestre e le schermate fisse si ridisegnano solo se serve */
        if (need_full) render_full();
    } else {
        update_play_overlays();
        if (panel_dirty) { draw_panel(); panel_dirty = 0; }
    }
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
    while (gfx_take_dirty(&r)) plat_present(r.x0, r.y0, r.x1, r.y1);
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
        if (obj != hover_obj || vb != hover_verb || ch != hover_choice || mb != hover_menu) {
            hover_obj = obj;
            hover_verb = vb;
            hover_choice = ch;
            hover_menu = mb;
            panel_dirty = 1;
        }
        break;
    }
    default:
        break;
    }
}

static void click_modal(int x, int y)
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
        } else if ((frame_count & 7) == 0) {
            /* triangolo lampeggiante: ridisegno del box */
            if (mode == MODE_MAP) need_full = 1;
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
            int nl;
            const char *starts[MAX_LINES];
            int lens[MAX_LINES];
            box = msg_box_rect(m, &nl, starts, lens);
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
