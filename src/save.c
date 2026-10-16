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

/*
 * Formato PPS2 (96 byte, big endian):
 *   0  "PPS2"          4  versione (2)      5  stanza   6  direzione
 *   7  n. oggetti      8  x (2 byte)       10  y (2 byte)
 *  12  flag (16 byte, 128 flag)            28  inventario (8 byte)
 *  36  giorno 37 mese 38 anno (2 byte) 40 ora 41 minuti
 *  42  riservati (a zero, per le prossime versioni)
 *  94  somma di controllo Fletcher-16 dei byte 0..93
 * Il vecchio formato PPS1 (64 byte, 32 flag) si carica ancora.
 */
#define OFF_FLAGS 12
#define OFF_INV   28
#define OFF_DATE  36
#define OFF_SUM   (SAVE_LEN - 2)

static unsigned fletcher16(const unsigned char *d, int n)
{
    unsigned a = 0, b = 0;
    int i;
    for (i = 0; i < n; i++) {
        a = (a + d[i]) % 255;
        b = (b + a) % 255;
    }
    return (b << 8) | a;
}

void serialize(unsigned char *d)
{
    int i, day, mon, year, hour, min;
    memset(d, 0, SAVE_LEN);
    d[0] = 'P'; d[1] = 'P'; d[2] = 'S'; d[3] = '2';
    d[4] = SAVE_VERSION;
    d[5] = G.room;
    d[6] = (unsigned char)G.dir;
    d[7] = G.ninv;
    put16(d + 8, G.hx);
    put16(d + 10, G.hy);
    memcpy(d + OFF_FLAGS, G.flags, FLAG_BYTES);
    for (i = 0; i < IT_COUNT; i++) d[OFF_INV + i] = (unsigned char)G.inv[i];
    plat_datetime(&day, &mon, &year, &hour, &min);
    d[OFF_DATE] = (unsigned char)day;
    d[OFF_DATE + 1] = (unsigned char)mon;
    put16(d + OFF_DATE + 2, year);
    d[OFF_DATE + 4] = (unsigned char)hour;
    d[OFF_DATE + 5] = (unsigned char)min;
    put16(d + OFF_SUM, (int)fletcher16(d, OFF_SUM));
}

/* Controlla un salvataggio e ne estrae stanza e data (per la lista).
   Ritorna 1 (PPS1), 2 (PPS2) o 0 se non e' valido. */
int save_info(const unsigned char *d, int len, SaveInfo *si)
{
    if (len >= 64 && d[0] == 'P' && d[1] == 'P' && d[2] == 'S' && d[3] == '1') {
        if (d[4] >= ROOM_COUNT || d[14] > IT_COUNT) return 0;
        si->room = d[4];
        si->day = d[23]; si->month = d[24]; si->year = get16(d + 25);
        si->hour = d[27]; si->minute = d[28];
        return 1;
    }
    if (len >= SAVE_LEN && d[0] == 'P' && d[1] == 'P' && d[2] == 'S' && d[3] == '2') {
        if ((get16(d + OFF_SUM) & 0xFFFF) != (int)fletcher16(d, OFF_SUM)) return 0;   /* file alterato */
        if (d[4] > SAVE_VERSION || d[5] >= ROOM_COUNT || d[7] > IT_COUNT) return 0;
        si->room = d[5];
        si->day = d[OFF_DATE]; si->month = d[OFF_DATE + 1]; si->year = get16(d + OFF_DATE + 2);
        si->hour = d[OFF_DATE + 4]; si->minute = d[OFF_DATE + 5];
        return 2;
    }
    return 0;
}

int deserialize(const unsigned char *d, int len)
{
    int i;
    SaveInfo si;
    int ver = save_info(d, len, &si);
    if (!ver) return -1;
    memset(&G, 0, sizeof(G));
    G.room = (unsigned char)si.room;
    if (ver == 1) {
        unsigned long f = ((unsigned long)d[10] << 24) | ((unsigned long)d[11] << 16) |
                          ((unsigned long)d[12] << 8) | d[13];
        G.dir = (signed char)d[5];
        G.hx = (short)get16(d + 6);
        G.hy = (short)get16(d + 8);
        for (i = 0; i < 32; i++)
            if (f & (1UL << i)) G.flags[i >> 3] |= (unsigned char)(1 << (i & 7));
        G.ninv = d[14];
        for (i = 0; i < IT_COUNT; i++) G.inv[i] = (signed char)d[15 + i];
    } else {
        G.dir = (signed char)d[6];
        G.ninv = d[7];
        G.hx = (short)get16(d + 8);
        G.hy = (short)get16(d + 10);
        memcpy(G.flags, d + OFF_FLAGS, FLAG_BYTES);
        for (i = 0; i < IT_COUNT; i++) G.inv[i] = (signed char)d[OFF_INV + i];
    }
    /* valori impossibili (file alterato): si riparte da un punto sicuro */
    for (i = 0; i < G.ninv; i++)
        if (G.inv[i] < 0 || G.inv[i] >= IT_COUNT) { G.ninv = (unsigned char)i; break; }
    if (G.dir != 1 && G.dir != -1) G.dir = 1;
    if (!walkable(G.hx, G.hy)) clamp_walk(&G.hx, &G.hy);
    return 0;
}

void refresh_slots(void)
{
    int i;
    for (i = 0; i < SAVE_SLOTS; i++) {
        SaveInfo si;
        int n = plat_load(i, slot_data[i], SAVE_LEN);
        slot_len[i] = n;
        slot_used[i] = n > 0 && save_info(slot_data[i], n, &si) != 0;
    }
}

void slot_label(int i, char *buf, int max)
{
    SaveInfo si;
    buf[0] = 0;
    num_cat(buf, i + 1, 1, max);
    str_cat(buf, ".  ", max);
    if (!slot_used[i] || !save_info(slot_data[i], slot_len[i], &si)) {
        str_cat(buf, "Vuoto", max);
        return;
    }
    {
        int n;
        str_cat(buf, "Puerto Polli - ", max);
        str_cat(buf, room_title(si.room), max);
        n = gfx_text_len(buf);
        while (n++ < 36) str_cat(buf, " ", max);
        num_cat(buf, si.day, 2, max); str_cat(buf, "/", max);
        num_cat(buf, si.month, 2, max); str_cat(buf, "/", max);
        num_cat(buf, si.year, 4, max); str_cat(buf, "  ", max);
        num_cat(buf, si.hour, 2, max); str_cat(buf, ":", max);
        num_cat(buf, si.minute, 2, max);
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
    if (!slot_used[slot] || deserialize(slot_data[slot], slot_len[slot]) != 0) {
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

