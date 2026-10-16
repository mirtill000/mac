/*
 * save.c - salvataggi: formato, lettura e scrittura degli slot
 */
#include <string.h>
#include "core.h"
#include "platform.h"

/* ------------------------------------------------------------------ */
/* Salvataggi                                                          */
/* ------------------------------------------------------------------ */

void put16(unsigned char *p, int v) { p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; }
int  get16(const unsigned char *p)  { return (short)((p[0] << 8) | p[1]); }

void serialize(unsigned char *d)
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

int deserialize(const unsigned char *d)
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

void refresh_slots(void)
{
    int i;
    for (i = 0; i < SAVE_SLOTS; i++)
        slot_used[i] = plat_load(i, slot_data[i], SAVE_LEN) == 0 && slot_data[i][0] == 'P';
}

void slot_label(int i, char *buf, int max)
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

void clear_interaction(void)
{
    msg_count = 0;
    msg_head = 0;
    dlg_npc = -1;
    dlg_ending = 0;
    walking = 0;
    pend_active = 0;
    reset_sentence();
}

void do_save(int slot)
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

void do_load(int slot)
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

void new_game(void)
{
    memset(&G, 0, sizeof(G));
    clear_interaction();
    G.room = ROOM_DOCK;
    mode = MODE_PLAY;
    go_room(ROOM_DOCK);
    G.hx = 60;
    script_new_game();
}

void to_title(void)
{
    clear_interaction();
    mode = MODE_TITLE;
    modal = MODAL_NONE;
    need_rebuild = 1;
    need_full = 1;
}

