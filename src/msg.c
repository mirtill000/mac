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

/* righe di testo di un box e caratteri per riga */
#define MSG_COLS 66

int msg_overflow;          /* messaggi persi perche' la coda era piena */

static void enqueue(int speaker, const char *text, int bytes, int nchar)
{
    Msg *m;
    if (msg_count >= MSG_MAX) { msg_overflow++; return; }
    m = &msgs[(msg_head + msg_count) % MSG_MAX];
    m->speaker = (signed char)speaker;
    m->call = CALL_NONE;
    m->text = text;
    m->bytes = (short)bytes;
    m->nchar = (short)nchar;
    if (msg_count == 0) msg_timer = 0;
    msg_count++;
    /* in gioco il box viene aggiornato da update_play_overlays (box_key):
       ridisegnare tutto lo schermo a ogni battuta era lento */
    if (mode != MODE_PLAY) need_full = 1;
    panel_dirty = 1;
}

/* Un testo che non entra in MSG_LINES righe viene spezzato in piu' box
   consecutivi (a fine riga), invece di essere troncato. */
void say(int speaker, const char *text)
{
    static unsigned char l1[MSG_LINES * MSG_COLS * 8];
    int starts[MSG_LINES * 8], lens[MSG_LINES * 8];
    int nc = gfx_to_latin1(text, (int)strlen(text), l1, (int)sizeof(l1));
    int nl = wrap_l1(l1, nc, MSG_COLS, starts, lens, MSG_LINES * 8);
    int line = 0;
    const char *p = text;
    int at_char = 0;
    while (line < nl) {
        int last = line + MSG_LINES < nl ? line + MSG_LINES : nl;
        int c_from = starts[line];
        int c_to = last < nl ? starts[last] : nc;
        const char *from, *q;
        /* da indici di carattere a posizioni nella stringa UTF-8 */
        while (at_char < c_from) { gfx_next_char(&p); at_char++; }
        from = p;
        q = p;
        {
            int c = at_char;
            while (c < c_to) { gfx_next_char(&q); c++; }
        }
        enqueue(speaker, from, (int)(q - from), c_to - c_from);
        line = last;
    }
}

void queue_call(int what)
{
    if (msg_count >= MSG_MAX) { msg_overflow++; return; }
    {
        Msg *m = &msgs[(msg_head + msg_count) % MSG_MAX];
        m->speaker = -1;
        m->call = (signed char)what;
        m->text = 0;
        m->bytes = m->nchar = 0;
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

int  flag(int f)       { return (G.flags[f >> 3] >> (f & 7)) & 1; }
void set_flag(int f)   { G.flags[f >> 3] |= (unsigned char)(1 << (f & 7)); need_rebuild = 1; }
void clear_flag(int f) { G.flags[f >> 3] &= (unsigned char)~(1 << (f & 7)); need_rebuild = 1; }
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

/* Divide il testo (gia' in Latin-1) in righe di al massimo maxc caratteri,
   andando a capo sugli spazi; ritorna il numero di righe. */
int wrap_l1(const unsigned char *s, int n, int maxc, int starts[], int lens[], int maxlines)
{
    int pos = 0, lines = 0;
    while (pos < n && lines < maxlines) {
        int end = pos + maxc, cut, i;
        if (end >= n) {
            starts[lines] = pos;
            lens[lines++] = n - pos;
            break;
        }
        cut = -1;
        for (i = end; i > pos; i--)
            if (s[i] == ' ') { cut = i; break; }
        starts[lines] = pos;
        if (cut > pos) {
            lens[lines++] = cut - pos;
            pos = cut + 1;
        } else {
            lens[lines++] = maxc;          /* parola piu' lunga di una riga */
            pos = end;
        }
    }
    return lines;
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
    if (mode != MODE_PLAY) need_full = 1;
    panel_dirty = 1;
}

int msg_duration(const Msg *m)
{
    int len = m->nchar;
    static const int per10[3] = {22, 15, 9};   /* fotogrammi ogni 10 caratteri */
    return 30 + len * per10[text_speed] / 10;
}

/* Il testo del box viene convertito in Latin-1 e impaginato una volta
   sola, finche' il messaggio resta a video. */
static const Msg *wrap_msg;
static const char *wrap_text_of;
static unsigned char wrap_buf[MSG_LINES * MSG_COLS + 8];
static int wrap_n, wrap_starts[MSG_LINES], wrap_lens[MSG_LINES];

static void layout_msg(const Msg *m)
{
    int nc;
    if (m == wrap_msg && m->text == wrap_text_of) return;
    wrap_msg = m;
    wrap_text_of = m->text;
    nc = gfx_to_latin1(m->text, m->bytes, wrap_buf, (int)sizeof(wrap_buf));
    wrap_n = wrap_l1(wrap_buf, nc, MSG_COLS, wrap_starts, wrap_lens, MSG_LINES);
}

GRect msg_box_rect(const Msg *m)
{
    GRect r;
    int has_name = m->speaker != SP_HERO && m->speaker != SP_NARRATORE;
    layout_msg(m);
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
    int i, y;
    Msg *m = cur_msg();
    GRect r;
    if (!m || !m->text) return;
    r = msg_box_rect(m);
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
    for (i = 0; i < wrap_n; i++, y += FONT_H)
        gfx_text_l1(r.x0 + 12, y, wrap_buf + wrap_starts[i], wrap_lens[i], FONT_REGULAR, COL_WHITE);
    draw_triangle(r, (int)((frame_count / 8) & 1));
    gfx_dirty(r.x0, r.y0, r.x1, r.y1);
    box_prev = r;
    box_prev_valid = 1;
}

