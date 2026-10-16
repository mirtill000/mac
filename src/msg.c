/*
 * msg.c - servizi per la storia (say, oggetti, flag, stanze), coda dei
 * messaggi e box dei dialoghi
 */
#include <string.h>
#include "core.h"
#include "platform.h"

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
    case ROOM_TAVERN: G.hx = 60;  G.hy = 281; G.dir = 1; break;
    case ROOM_BEACH:  G.hx = 60;  G.hy = 226; G.dir = 1; break;
    default:          G.hx = 48;  G.hy = 230; G.dir = 1; break;
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

int in_rect(int x, int y, int rx, int ry, int rw, int rh)
{
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

void str_cat(char *d, const char *s, int max)
{
    int n = (int)strlen(d);
    while (*s && n < max - 1) d[n++] = *s++;
    d[n] = 0;
}

void num_cat(char *d, int v, int digits, int max)
{
    char t[8];
    int i;
    for (i = digits - 1; i >= 0; i--) { t[i] = (char)('0' + v % 10); v /= 10; }
    t[digits] = 0;
    str_cat(d, t, max);
}

/* Divide il testo in righe di al massimo maxc caratteri. */
#define MAX_LINES 8
int wrap_text(const char *s, int maxc, const char *starts[], int lens[])
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
void text_n(int x, int y, const char *s, int n, int font, int col)
{
    while (n-- > 0 && *s) {
        int c = gfx_next_char(&s);
        gfx_glyph(x, y, c, font, col);
        x += FONT_W;
    }
}

void text_center(int y, const char *s, int font, int col)
{
    gfx_text((FB_W - gfx_text_width(s)) / 2, y, s, font, col);
}


/* ------------------------------------------------------------------ */
/* Messaggi                                                           */
/* ------------------------------------------------------------------ */

Msg *cur_msg(void) { return msg_count ? &msgs[msg_head] : 0; }

void run_call(int c)
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

void next_msg(void)
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

int msg_duration(const Msg *m)
{
    int len = gfx_text_len(m->text);
    static const int per10[3] = {22, 15, 9};   /* fotogrammi ogni 10 caratteri */
    return 30 + len * per10[text_speed] / 10;
}

/* L'impaginazione del messaggio viene calcolata una volta sola e
   riutilizzata finche' il messaggio resta a video. */
static const char *wrap_text_of;
int wrap_speaker = -1, wrap_n, wrap_lens[MAX_LINES];
static const char *wrap_starts[MAX_LINES];

GRect msg_box_rect(const Msg *m, int *nlines, const char *starts[], int lens[])
{
    GRect r;
    int i, has_name = m->speaker != SP_HERO && m->speaker != SP_NARRATORE;
    if (m->text != wrap_text_of || m->speaker != wrap_speaker) {
        wrap_text_of = m->text;
        wrap_speaker = m->speaker;
        wrap_n = wrap_text(m->text, 66, wrap_starts, wrap_lens);
    }
    *nlines = wrap_n;
    for (i = 0; i < wrap_n; i++) { starts[i] = wrap_starts[i]; lens[i] = wrap_lens[i]; }
    r.x0 = 12; r.x1 = FB_W - 12;
    r.y0 = 6;
    r.y1 = (short)(r.y0 + 10 + (has_name ? 15 : 0) + wrap_n * FONT_H + 8);
    return r;
}

/* triangolino "continua" in basso a destra del box */
int tri_shown;

void draw_triangle(GRect r, int on)
{
    gfx_fill(r.x1 - 18, r.y1 - 18, FONT_W + 1, FONT_H - 4, PAT_BLACK);
    if (on) gfx_glyph(r.x1 - 18, r.y1 - 18, GLYPH_TRIANGLE, FONT_REGULAR, COL_WHITE);
    tri_shown = on;
}

void draw_msg(void)
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
    draw_triangle(r, (int)((frame_count / 8) & 1));
    gfx_dirty(r.x0, r.y0, r.x1, r.y1);
    box_prev = r;
    box_prev_valid = 1;
}

