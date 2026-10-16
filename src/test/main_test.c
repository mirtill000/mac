/*
 * main_test.c - test automatico: gioca l'intera avventura dall'inizio alla
 * fine simulando i clic del mouse, controlla lo stato e salva alcune
 * schermate (formato PBM) nella cartella indicata.
 *
 * Uso: puertopolli_test <cartella_output>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../game.h"
#include "../engine.h"
#include "../gfx.h"
#include "../platform.h"
#include "../core.h"      /* per i test dei singoli moduli */

static const char *outdir = ".";
static unsigned char saves[8][128];
static int saved[8];
static int failures;

void plat_present(int x0, int y0, int x1, int y1) { (void)x0; (void)y0; (void)x1; (void)y1; }
int plat_save(int slot, const unsigned char *d, int len) { memcpy(saves[slot], d, len); saved[slot] = len; return 0; }
int plat_load(int slot, unsigned char *d, int maxlen)
{
    int n = saved[slot] < maxlen ? saved[slot] : maxlen;
    if (!saved[slot]) return -1;
    memcpy(d, saves[slot], n);
    return n;
}
void plat_datetime(int *d, int *mo, int *y, int *h, int *mi) { *d = 16; *mo = 10; *y = 2026; *h = 14; *mi = 32; }
void plat_beep(void) {}
static unsigned long fake_ticks;
unsigned long plat_ticks(void) { return fake_ticks; }

static void shot(const char *name)
{
    char path[512];
    FILE *f;
    sprintf(path, "%s/%s.pbm", outdir, name);
    f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P4\n%d %d\n", FB_W, FB_H);
    fwrite(gfx_fb, 1, sizeof(gfx_fb), f);
    fclose(f);
}

static int render_mismatch;

/* il rendering incrementale deve dare lo stesso risultato di uno completo */
static void check_render(void)
{
    static unsigned char inc[sizeof(gfx_fb)];
    memcpy(inc, gfx_fb, sizeof(gfx_fb));
    game_debug(DBG_REDRAW, 0);
    game_render();
    if (memcmp(inc, gfx_fb, sizeof(gfx_fb)) != 0) {
        if (render_mismatch++ < 3) {
            int i;
            for (i = 0; i < (int)sizeof(gfx_fb) && inc[i] == gfx_fb[i]; i++) ;
            printf("  FAIL rendering incrementale diverso (x=%d y=%d)\n", (i % FB_RB) * 8, i / FB_RB);
            memcpy(gfx_fb, inc, sizeof(gfx_fb));
            shot("errore_rendering");
        }
        failures++;
    }
}

static void frames(int n)
{
    while (n-- > 0) { fake_ticks += TICKS_PER_FRAME; game_tick(); game_render(); check_render(); }
}

static void click(int x, int y)
{
    game_mouse(x, y);
    frames(1);
    game_click(x, y);
    frames(1);
}

/* attende la fine di camminata e messaggi, cliccando per far avanzare */
static void settle(void)
{
    int guard = 0;
    while (game_debug(DBG_BUSY, 0) && guard++ < 2000) {
        frames(1);
        if (guard % 25 == 0 && !game_debug(DBG_BUSY, 0)) break;
    }
    frames(2);
}

static void settle_skip(void)
{
    int guard = 0;
    while (game_debug(DBG_BUSY, 0) && guard++ < 400) {
        frames(4);
        game_key(' ', 0);
    }
    frames(2);
}

#define CHECK(cond, what) do { if (cond) printf("  ok   %s\n", what); \
    else { printf("  FAIL %s\n", what); failures++; } } while (0)

#define TVERB_X(i) (VERB_X(i) + 10)
#define TVERB_Y(i) (VERB_Y(i) + 6)
static void verb(int v) { click(TVERB_X(v), TVERB_Y(v)); }

static void inv_click(int item)
{
    int s = game_debug(DBG_SLOT, item);
    if (s < 0) { printf("  FAIL oggetto %d non in inventario\n", item); failures++; return; }
    click(277 + (s % 4) * 58 + 28, 259 + (s / 4) * 40 + 19);
}

static void hot(int id)
{
    int i;
    for (i = 0; i < hotspot_count; i++)
        if (hotspots[i].id == id) {
            click(hotspots[i].x + hotspots[i].w / 2, hotspots[i].y + hotspots[i].h / 2);
            return;
        }
}

static void choose(int n) { click(40, 258 + n * 16 + 6); }

static void travel(int place)
{
    inv_click(IT_MAPPA);
    frames(2);
    click(map_places[place].x + map_places[place].w / 2, 30 + map_places[place].y + map_places[place].h / 2);
    settle_skip();
}

/* ---- grafica: niente scritture fuori dall'area di clipping ---- */
static void test_clipping(void)
{
    static unsigned char before[sizeof(gfx_fb)];
    const HeroSprite *sp = &asset_hero[HERO_SIZES - 1];
    int x, y, bad = 0;
    const int cx0 = 101, cy0 = 50, cx1 = 299, cy1 = 151;    /* bordi non allineati al byte */
    printf("Clipping\n");
    memset(gfx_fb, 0x5A, sizeof(gfx_fb));
    memcpy(before, gfx_fb, sizeof(gfx_fb));
    gfx_clip(cx0, cy0, cx1, cy1);
    gfx_sprite_rows(sp->pool, sp->rows, sp->w, sp->h, sp->rowBytes, cx0 - 40, cy0 - 90, 0);
    gfx_sprite_rows(sp->pool, sp->rows, sp->w, sp->h, sp->rowBytes, cx1 - 20, cy1 - 30, 1);
    gfx_sprite_rows(sp->pool, sp->rows, sp->w, sp->h, sp->rowBytes, -50, -60, 0);   /* fuori schermo */
    gfx_sprite_rows(sp->pool, sp->rows, sp->w, sp->h, sp->rowBytes, FB_W - 10, FB_H - 10, 1);
    gfx_text(cx1 - 30, cy0 - 5, "Bordo àèìòù", FONT_BOLD, COL_BLACK);
    gfx_text(-20, cy1 - 7, "Fuori", FONT_REGULAR, COL_WHITE);
    gfx_fill(cx0 - 20, cy0 - 20, 400, 400, PAT_GRAY);
    gfx_line(-100, -100, 600, 400, COL_BLACK);
    gfx_shadow(cx0, cy1, 30, 4);
    gfx_noclip();
    for (y = 0; y < FB_H; y++)
        for (x = 0; x < FB_W; x++) {
            int inside = x >= cx0 && x < cx1 && y >= cy0 && y < cy1;
            int bit = 0x80 >> (x & 7);
            if (!inside && ((gfx_fb[y * FB_RB + (x >> 3)] ^ before[y * FB_RB + (x >> 3)]) & bit)) bad++;
        }
    CHECK(bad == 0, "nessun pixel modificato fuori dall'area di clipping");
    /* il percorso veloce (riga intera) deve dare gli stessi pixel di quello
       lento (byte per byte, usato quando il ritaglio non e' allineato) */
    {
        static unsigned char fast[sizeof(gfx_fb)];
        int sh, mir, diff = 0;
        for (mir = 0; mir < 2; mir++)
            for (sh = 0; sh < 8; sh++) {
                int sz;
                for (sz = 0; sz < HERO_SIZES; sz++) {
                    const HeroSprite *h = &asset_hero[sz];
                    memset(gfx_fb, 0x5A, sizeof(gfx_fb));
                    gfx_set_tint(sh & 1 ? PAT_LTGRAY : 0);
                    gfx_clip(0, 0, FB_W, FB_H);
                    gfx_sprite_rows(h->pool, h->rows, h->w, h->h, h->rowBytes, 100 + sh, 40, mir);
                    memcpy(fast, gfx_fb, sizeof(gfx_fb));
                    memset(gfx_fb, 0x5A, sizeof(gfx_fb));
                    gfx_clip(1, 0, FB_W - 1, FB_H);        /* non allineato: percorso lento */
                    gfx_sprite_rows(h->pool, h->rows, h->w, h->h, h->rowBytes, 100 + sh, 40, mir);
                    gfx_set_tint(0);
                    if (memcmp(fast, gfx_fb, sizeof(gfx_fb))) diff++;
                }
            }
        gfx_noclip();
        CHECK(diff == 0, "disegno veloce e lento del protagonista identici (anche specchiato)");
    }
    game_debug(DBG_REDRAW, 0);
    frames(2);
}

/* ---- salvataggi: file alterati, troncati e vecchio formato ---- */
static void test_saves(void)
{
    unsigned char d[SAVE_LEN];
    printf("Salvataggi\n");
    serialize(d);
    memcpy(saves[3], d, SAVE_LEN); saved[3] = SAVE_LEN;
    saves[3][20] ^= 0x10;                               /* un bit cambiato */
    memcpy(saves[4], d, 10); saved[4] = 10;             /* file troncato */
    /* vecchio formato PPS1: spiaggia, cappello in testa, moneta e mappa */
    memset(saves[5], 0, 64);
    memcpy(saves[5], "PPS1", 4);
    saves[5][4] = ROOM_BEACH; saves[5][5] = 1;
    saves[5][6] = 0; saves[5][7] = 120; saves[5][8] = 0; saves[5][9] = 220;
    saves[5][13] = (unsigned char)(1 << F_HAT_WORN);
    saves[5][14] = 2; saves[5][15] = IT_MONETA; saves[5][16] = IT_MAPPA;
    saved[5] = 64;
    memcpy(saves[6], d, SAVE_LEN); saved[6] = SAVE_LEN;
    refresh_slots();
    CHECK(!slot_used[3], "salvataggio alterato rifiutato");
    CHECK(!slot_used[4], "salvataggio troncato rifiutato");
    CHECK(slot_used[5] && slot_used[6], "salvataggi validi riconosciuti (PPS1 e PPS2)");
    do_load(5);
    CHECK(G.room == ROOM_BEACH && flag(F_HAT_WORN) && has_item(IT_MAPPA) && G.ninv == 2,
          "il vecchio formato PPS1 si carica");
    serialize(d);
    CHECK(d[3] == '2' && save_info(d, SAVE_LEN, &(SaveInfo){0}) == 2, "si salva nel formato PPS2");
    frames(2);
}

/* ---- testi lunghi: spezzati in piu' box, niente troncato ---- */
static void test_long_text(void)
{
    static const char *lungo =
        "Questa è una frase molto lunga, scritta apposta per il test: deve essere spezzata "
        "in più riquadri consecutivi senza perdere neanche una parola, perché prima o poi "
        "qualcuno scriverà un dialogo più lungo del previsto e non vogliamo che il gioco "
        "tagli il testo a metà come se niente fosse. Ecco ancora qualche parola per "
        "superare abbondantemente le quattro righe di un solo riquadro: pollo, grog, "
        "cappello, piuma, Codice dei Pirati, Capitan Barbapollo e il Custode di Puerto Polli.";
    int i, chars = 0, boxes, ok = 1;
    printf("Testi lunghi\n");
    clear_interaction();
    say(SP_HERO, lungo);
    boxes = msg_count;
    for (i = 0; i < msg_count; i++) {
        const Msg *m = &msgs[(msg_head + i) % MSG_MAX];
        GRect r = msg_box_rect(m);
        chars += m->nchar;
        if (r.y1 - r.y0 > 10 + MSG_LINES * FONT_H + 8) ok = 0;
    }
    CHECK(boxes > 1, "il testo lungo e' diviso in piu' riquadri");
    CHECK(ok, "nessun riquadro supera le righe previste");
    /* tra un riquadro e il successivo si perde solo lo spazio dell'a-capo */
    CHECK(chars >= gfx_text_len(lungo) - (boxes - 1) && chars <= gfx_text_len(lungo),
          "tutti i caratteri sono mostrati");
    clear_interaction();
    frames(2);
}

/* ---- punti di arrivo e ingressi dentro le zone calpestabili ---- */
static void test_walk_points(void)
{
    int i, bad = 0, r, saved_room = G.room;
    printf("Zone calpestabili\n");
    for (i = 0; i < hotspot_count; i++) {
        const Hotspot *h = &hotspots[i];
        if (h->wx < 0) continue;
        G.room = (unsigned char)h->room;
        if (!walkable(h->wx, h->wy)) {
            printf("       punto di arrivo fuori: %s (%d,%d)\n", h->name, h->wx, h->wy);
            bad++;
        }
    }
    CHECK(bad == 0, "tutti i punti di arrivo sono calpestabili");
    bad = 0;
    for (r = 0; r < ROOM_COUNT; r++) {
        go_room(r);
        if (hero_hidden()) continue;          /* in barca: niente camminata */
        if (!walkable(G.hx, G.hy)) { printf("       ingresso fuori: stanza %d (%d,%d)\n", r, G.hx, G.hy); bad++; }
    }
    CHECK(bad == 0, "tutti gli ingressi delle stanze sono calpestabili");
    clear_interaction();
    go_room(saved_room);
    clear_interaction();
    frames(2);
}

/* ---- bug corretti: non devono tornare ---- */
static void test_regressions(void)
{
    unsigned char d[SAVE_LEN];
    GameState keep = G;
    printf("Regressioni\n");
    go_room(ROOM_DOCK);
    clear_interaction();
    CHECK(hotspot_at(450, 170) == H_BARBA && hotspot_at(416, 149) == H_TABERNA,
          "il viso di Barbapollo non apre la Taberna");
    /* dal verbo al pulsante Menu in un colpo solo: il ridisegno parziale
       deve evidenziare il pulsante (lo controlla check_render) */
    game_mouse(TVERB_X(V_GUARDA), TVERB_Y(V_GUARDA));
    frames(1);
    game_mouse(MENU_BTN_X + 10, SENTENCE_Y + 6);
    frames(1);
    game_mouse(200, 100);
    frames(1);
    /* salvataggio della vecchia versione dopo la vittoria della Parte I */
    memset(&G, 0, sizeof(G));
    G.room = ROOM_NIGHT; G.hx = 100; G.hy = 230; G.dir = 1;
    set_flag(F_GAME_WON);
    serialize(d);
    deserialize(d, SAVE_LEN);
    CHECK(flag(F_PART2), "un vecchio salvataggio vinto apre la Parte II");
    G = keep;
    clear_interaction();
    go_room(G.room);
    clear_interaction();
    frames(2);
}

/* ---- prospettiva: nessun cambio di misura avanti e indietro ---- */
static void test_perspective(void)
{
    int y, changes = 0, last;
    printf("Prospettiva\n");
    go_room(ROOM_DOCK);
    clear_interaction();
    G.hy = 200;
    last = hero_size();
    for (y = 200; y <= 236; y++) {                  /* avanti... */
        G.hy = (short)y;
        if (hero_size() != last) { changes++; last = hero_size(); }
    }
    for (y = 236; y >= 200; y--) {                  /* ...e indietro */
        G.hy = (short)y;
        if (hero_size() != last) { changes++; last = hero_size(); }
    }
    CHECK(changes == 2, "un solo cambio di misura per verso");
    changes = 0;
    for (y = 0; y < 40; y++) {                      /* fermo sulla soglia */
        G.hy = (short)(212 + (y & 1));
        if (hero_size() != last) { changes++; last = hero_size(); }
    }
    CHECK(changes <= 1, "sulla soglia la misura non pulsa");
    clear_interaction();
    frames(2);
}

int main(int argc, char **argv)
{
    if (argc > 1) outdir = argv[1];
    game_init();
    frames(80);                /* il titolo e' animato: mare e gabbiano */
    shot("01_titolo");

    printf("Nuova partita\n");
    click(256, 268);
    frames(10);
    shot("02_intro");
    settle_skip();
    CHECK(game_debug(DBG_MODE, 0) == 1 && game_debug(DBG_ROOM, 0) == ROOM_DOCK, "inizio sul molo");
    CHECK(game_debug(DBG_HAS, IT_MONETA) && game_debug(DBG_HAS, IT_MAPPA), "moneta e mappa in inventario");
    game_mouse(450, 180);
    frames(3);
    shot("03_molo");

    printf("Taberna\n");
    hot(H_TABERNA);
    settle();
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_TAVERN, "entrato nella Taberna");
    verb(V_PARLA);
    hot(H_OSTE);
    settle();
    frames(5);
    shot("04_oste");
    settle_skip();
    CHECK(game_debug(DBG_DIALOG, 0) == SP_OSTE, "dialogo con l'oste");
    shot("05_scelte");
    choose(2);
    settle_skip();
    CHECK(game_debug(DBG_FLAG, F_KNOWS_CUSTODE), "saputo del Custode");
    choose(4);
    settle_skip();
    CHECK(game_debug(DBG_DIALOG, 0) < 0, "dialogo chiuso");
    verb(V_DAI);
    inv_click(IT_MONETA);
    hot(H_OSTE);
    settle_skip();
    CHECK(game_debug(DBG_HAS, IT_BOTTIGLIA) && !game_debug(DBG_HAS, IT_MONETA), "comprato il grog");
    hot(H_USCITA_T);
    settle();
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_DOCK, "tornato al molo");

    printf("Spiaggia\n");
    inv_click(IT_MAPPA);
    frames(2);
    game_mouse(100, 250);
    frames(2);
    shot("06_mappa");
    click(map_places[3].x + 20, 30 + map_places[3].y + 10);
    settle_skip();
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_BEACH, "arrivato alla Cala dei Gabbiani");
    shot("07_spiaggia");
    verb(V_RACCOGLI);
    hot(H_CAPPELLO);
    settle_skip();
    CHECK(!game_debug(DBG_HAS, IT_CAPPELLO), "il cappello non si raccoglie");
    verb(V_TIRA);
    hot(H_CAPPELLO);
    settle_skip();
    CHECK(game_debug(DBG_HAS, IT_CAPPELLO), "tirato il cappello");
    verb(V_APRI);
    hot(H_CASSA);
    settle_skip();
    CHECK(!game_debug(DBG_FLAG, F_CRATE_OPEN), "cassa chiusa senza chiave");
    verb(V_USA);
    inv_click(IT_CAPPELLO);
    settle_skip();
    CHECK(game_debug(DBG_FLAG, F_HAT_WORN), "cappello in testa");

    printf("Salvataggio\n");
    game_key('s', 1);
    frames(2);
    click(60, 50 + 35 + 8);
    frames(2);
    shot("08_salva");
    click(40 + 432 - 112 + 40, 50 + 220 - 34 + 10);
    settle_skip();
    CHECK(saved[0], "partita salvata nello slot 1");

    printf("Barbapollo\n");
    travel(1);
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_DOCK, "molo");
    verb(V_PARLA);
    hot(H_BARBA);
    settle_skip();
    choose(2);
    settle_skip();
    CHECK(game_debug(DBG_FLAG, F_KNOWS_KEY), "Barbapollo parla della chiave");
    choose(game_debug(DBG_FLAG, F_GOT_KEY) ? 3 : 4);
    settle_skip();
    verb(V_DAI);
    inv_click(IT_BOTTIGLIA);
    hot(H_BARBA);
    settle_skip();
    CHECK(game_debug(DBG_HAS, IT_CHIAVE), "ottenuta la chiave");
    shot("09_cappello");

    printf("Cassa\n");
    travel(3);
    verb(V_USA);
    inv_click(IT_CHIAVE);
    hot(H_CASSA);
    settle_skip();
    CHECK(game_debug(DBG_HAS, IT_CALAMARO) && game_debug(DBG_HAS, IT_LIBRO), "calamaro e libro");

    printf("Gabbiano\n");
    travel(1);
    /* molo: il mare a sinistra non e' calpestabile */
    click(40, 190);
    settle();
    CHECK(game_debug(DBG_HY, 0) >= 214, "non cammina sull'acqua (molo)");
    verb(V_DAI);
    inv_click(IT_CALAMARO);
    hot(H_GABBIANO);
    settle_skip();
    CHECK(game_debug(DBG_FLAG, F_GULL_FED), "gabbiano nutrito");
    shot("10_piuma");
    verb(V_RACCOGLI);
    hot(H_PIUMA);
    settle_skip();
    CHECK(game_debug(DBG_HAS, IT_PIUMA), "presa la piuma");

    printf("Rovine\n");
    travel(4);
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_NIGHT, "alle Rovine");
    /* clic sull'acqua a sinistra del pontile: deve restare sulle assi */
    click(120, 196);
    settle();
    CHECK(game_debug(DBG_HY, 0) >= 224, "non cammina sull'acqua (rovine)");
    click(290, 210);
    settle();
    CHECK(game_debug(DBG_HX, 0) >= 200 && game_debug(DBG_HY, 0) >= 206, "sale sulla piattaforma");
    click(60, 180);
    settle();
    CHECK(game_debug(DBG_HY, 0) >= 224, "ridiscende senza tagliare sull'acqua");
    verb(V_PARLA);
    hot(H_CUSTODE);
    settle();
    frames(4);
    shot("11_custode");
    settle_skip();
    choose(0);
    settle_skip();
    CHECK(game_debug(DBG_FLAG, F_KNOWS_PROOFS), "conosco le prove");
    shot("12_scelte_custode");
    choose(2);   /* "Guarda il mio cappello!" */
    settle_skip();
    CHECK(game_debug(DBG_FLAG, F_PROOF_HAT), "prova del cappello");
    /* esce dal dialogo con l'ultima opzione */
    {
        int i;
        for (i = 4; i >= 0 && game_debug(DBG_DIALOG, 0) >= 0; i--) { choose(i); settle_skip(); }
    }
    verb(V_DAI);
    inv_click(IT_PIUMA);
    hot(H_CUSTODE);
    settle_skip();
    CHECK(game_debug(DBG_FLAG, F_PROOF_FEATHER), "prova della piuma");
    verb(V_DAI);
    inv_click(IT_LIBRO);
    hot(H_CUSTODE);
    settle_skip();
    CHECK(game_debug(DBG_FLAG, F_GAME_WON), "partita vinta");
    settle_skip();
    frames(10);
    CHECK(game_debug(DBG_MODE, 0) == 3, "cartello della Parte II");
    shot("13_parte2");
    click(256, 300);
    frames(4);
    CHECK(game_debug(DBG_MODE, 0) == 1 && game_debug(DBG_ROOM, 0) == ROOM_NIGHT, "si riprende alle Rovine");

    printf("Parte II: il faro\n");
    travel(1);
    verb(V_PARLA);
    hot(H_BARBA);
    settle_skip();
    CHECK(game_debug(DBG_HAS, IT_BUSSOLA), "Barbapollo regala la bussola");
    {
        int i;
        for (i = 4; i >= 0 && game_debug(DBG_DIALOG, 0) >= 0; i--) { choose(i); settle_skip(); }
    }
    CHECK(game_debug(DBG_DIALOG, 0) < 0, "dialogo chiuso");
    travel(0);
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_FARO, "al faro");
    frames(20);
    shot("14_faro");
    hot(H_PORTA_F);
    settle_skip();
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_FARO && !game_debug(DBG_FLAG, F_FARO_OPEN), "la porta e' chiusa");
    verb(V_DAI);
    inv_click(IT_BUSSOLA);
    hot(H_GABBIANO_F);
    settle_skip();
    CHECK(game_debug(DBG_HAS, IT_CHIAVE) && !game_debug(DBG_HAS, IT_BUSSOLA), "il gabbiano scambia la chiave");
    verb(V_APRI);
    hot(H_PORTA_F);
    settle_skip();
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_FARO_IN && !game_debug(DBG_HAS, IT_CHIAVE), "dentro il faro");
    shot("15_faro_dentro");
    verb(V_SPINGI);
    hot(H_LENTE);
    settle_skip();
    CHECK(!game_debug(DBG_FLAG, F_LIGHT_ON), "la lente e' bloccata");
    verb(V_RACCOGLI);
    hot(H_LANTERNA_F);
    settle_skip();
    verb(V_RACCOGLI);
    hot(H_CARTA);
    settle_skip();
    CHECK(game_debug(DBG_HAS, IT_LANTERNA) && game_debug(DBG_HAS, IT_CARTA), "lanterna e carta nautica");
    verb(V_USA);
    inv_click(IT_LANTERNA);
    hot(H_MECCANISMO);
    settle_skip();
    CHECK(game_debug(DBG_FLAG, F_GEARS_OILED), "ingranaggi oliati");
    verb(V_SPINGI);
    hot(H_LENTE);
    settle();
    frames(10);
    shot("16_luce");
    settle_skip();
    frames(10);
    CHECK(game_debug(DBG_FLAG, F_LIGHT_ON) && game_debug(DBG_MODE, 0) == 3, "il faro gira: Parte III");
    click(256, 300);
    frames(4);
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_FARO_IN, "si riprende nel faro");
    frames(30);
    shot("17_lente");

    printf("Parte III: la grotta\n");
    verb(V_GUARDA);
    inv_click(IT_CARTA);
    settle_skip();
    CHECK(game_debug(DBG_FLAG, F_KNOWS_GROTTA), "la grotta e' sulla carta");
    inv_click(IT_MAPPA);
    frames(2);
    shot("18_mappa_grotta");
    click(map_places[5].x + 20, 30 + map_places[5].y + 10);
    settle_skip();
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_GROTTA, "alla grotta");
    shot("19_grotta");
    verb(V_PARLA);
    hot(H_OSSOBUCO);
    settle();
    frames(4);
    shot("20_ossobuco");
    settle_skip();
    choose(1);
    settle_skip();
    CHECK(game_debug(DBG_FLAG, F_KNOWS_TOLL), "il pedaggio e' un teschio");
    choose(2);
    settle_skip();
    hot(H_INGRESSO);
    settle_skip();
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_GROTTA_IN, "dentro la grotta");
    frames(20);
    shot("21_grotta_dentro");
    verb(V_RACCOGLI);
    hot(H_TESCHIO);
    settle_skip();
    verb(V_APRI);
    hot(H_FORZIERE);
    settle_skip();
    CHECK(game_debug(DBG_HAS, IT_TESCHIO) && game_debug(DBG_FLAG, F_CHEST2_OPEN), "teschio e proverbi");
    hot(H_USCITA_G);
    settle_skip();
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_GROTTA, "di nuovo fuori");
    verb(V_DAI);
    inv_click(IT_TESCHIO);
    hot(H_OSSOBUCO);
    settle_skip();
    CHECK(game_debug(DBG_FLAG, F_FERRY), "pedaggio pagato");
    verb(V_USA);
    hot(H_BARCA);
    settle_skip();
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_NAVE, "in barca verso la nave");
    frames(20);
    shot("22_nave");
    hot(H_NAVE_E);
    settle_skip();
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_NAVE_IN, "a bordo");
    shot("23_cabina");

    printf("Parte III: il duello\n");
    verb(V_PARLA);
    hot(H_BRODO);
    settle();
    frames(4);
    shot("24_brodo");
    settle_skip();
    CHECK(game_debug(DBG_DIALOG, 0) == SP_BRODO, "dialogo con Brodo");
    choose(0);                 /* sfida */
    settle_skip();
    choose(0);                 /* risposta sbagliata */
    settle_skip();
    CHECK(game_debug(DBG_DIALOG, 0) == SP_BRODO && !game_debug(DBG_FLAG, F_BRODO_WON), "risposta sbagliata: si ricomincia");
    choose(0);
    settle_skip();
    shot("25_duello");
    choose(1);                 /* le risposte giuste: vedi duel_order */
    settle_skip();
    choose(0);
    settle_skip();
    choose(2);
    settle_skip();
    frames(10);
    CHECK(game_debug(DBG_FLAG, F_BRODO_WON), "Brodo sconfitto");
    CHECK(game_debug(DBG_MODE, 0) == 3, "schermata finale");
    shot("26_fine");

    printf("Caricamento\n");
    click(256, 300);
    frames(2);
    CHECK(game_debug(DBG_MODE, 0) == 0, "di nuovo al titolo");
    click(256, 262 + 19 + 6);
    frames(2);
    click(60, 50 + 35 + 8);
    click(40 + 432 - 112 + 40, 50 + 220 - 34 + 10);
    frames(2);
    CHECK(game_debug(DBG_ROOM, 0) == ROOM_BEACH && game_debug(DBG_FLAG, F_HAT_WORN) &&
          !game_debug(DBG_FLAG, F_GAME_WON), "partita caricata");
    game_key(KEY_TAB, 0);
    frames(2);
    shot("14_inventario");
    game_key(KEY_ESC, 0);      /* dall'inventario si torna al menu di pausa */
    frames(2);
    CHECK(game_debug(DBG_MODAL, 0) == 1, "menu di pausa");
    shot("15_menu");

    printf("Coda dei messaggi\n");
    CHECK(msg_overflow == 0, "nessun messaggio perso durante l'avventura");

    test_clipping();
    test_saves();
    test_long_text();
    test_walk_points();
    test_regressions();
    test_perspective();

    printf("\n%s (%d errori)\n", failures ? "TEST FALLITO" : "TUTTO OK", failures);
    return failures ? 1 : 0;
}
