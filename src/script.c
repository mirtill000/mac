/*
 * script.c - la storia di Puerto Polli: stanze, oggetti, dialoghi, enigmi.
 *
 * Soluzione (attenzione, spoiler!):
 *   1. Taberna: Dai moneta all'oste            -> bottiglia di grog
 *   2. Spiaggia: Tira il cappello dal relitto  -> cappello
 *   3. Usa cappello (te lo metti in testa)
 *   4. Molo: Dai bottiglia a Barbapollo        -> chiave
 *   5. Spiaggia: Apri cassa / Usa chiave con cassa -> calamaro + libro
 *   6. Molo: Dai calamaro al gabbiano, Raccogli piuma
 *   7. Rovine: parla col Custode e dagli cappello, piuma e libro
 */
#include "engine.h"
#include "assets.h"
#include "anim.h"

/* ------------------------------------------------------------------ */
/* Dati                                                               */
/* ------------------------------------------------------------------ */

const char *const verb_names[V_COUNT + 1] = {
    "Dai", "Apri", "Chiudi", "Raccogli", "Guarda", "Spingi",
    "Usa", "Parla con", "Tira", "Vai a"
};

const char *const speaker_names[SP_COUNT] = {
    "Tu", "Oste Bartolo", "Capitan Barbapollo", "Mozzo Pino",
    "Il Custode", "Gabbiano", ""
};

static const char *const item_names[IT_COUNT] = {
    "bottiglia di grog", "moneta", "chiave", "mappa",
    "piuma", "calamaro", "cappello", "libro"
};

static const char *const item_titles[IT_COUNT] = {
    "Bottiglia", "Moneta", "Chiave", "Mappa",
    "Piuma", "Calamaro", "Cappello", "Libro"
};

/* L'ordine conta: a parita' di posizione vince il primo della lista. */
const Hotspot hotspots[] = {
    /* id            room         nome                 x    y    w    h    wx   wy  face exit */
    { H_PIUMA,      ROOM_DOCK,   "piuma",             40, 204,  28,  26,  98, 228, -1, -1 },
    { H_TABERNA,    ROOM_DOCK,   "porta della Taberna", 412, 111, 34, 76, 428, 203, 1, ROOM_TAVERN },
    { H_INSEGNA,    ROOM_DOCK,   "insegna",          390,  86,  88,  22, 428, 203,  1, -1 },
    { H_BARBA,      ROOM_DOCK,   "Capitan Barbapollo", 425, 141, 66, 93, 392, 228, 1, -1 },
    { H_MOZZO,      ROOM_DOCK,   "Mozzo Pino",       333, 126,  40,  76, 312, 214,  1, -1 },
    { H_GABBIANO,   ROOM_DOCK,   "gabbiano",          84, 152,  32,  32, 132, 222, -1, -1 },
    { H_BANCARELLA, ROOM_DOCK,   "bancarella",       235, 101,  75,  95, 270, 210,  1, -1 },
    { H_VELIERO,    ROOM_DOCK,   "veliero",            0,  26,  84, 115,  60, 210, -1, -1 },
    { H_PAESE,      ROOM_DOCK,   "Puerto Polli",     150,  10, 180,  80,  -1,   0,  0, -1 },
    { H_SENTIERO_D, ROOM_DOCK,   "sentiero",           0, 190,  18,  50,  10, 226, -1, ROOM_MAP },

    { H_OSTE,       ROOM_TAVERN, "oste",             307, 113,  99,  98, 204, 234,  1, -1 },
    { H_BANDIERA,   ROOM_TAVERN, "bandiera",          10,  51,  60,  77,  70, 234, -1, -1 },
    { H_QUADRO,     ROOM_TAVERN, "quadretto",         19, 140,  32,  32,  70, 234, -1, -1 },
    { H_LANTERNA,   ROOM_TAVERN, "lanterna",         182, 106,  34,  50, 190, 234,  1, -1 },
    { H_FINESTRA,   ROOM_TAVERN, "finestra",         258, 103,  42,  45, 214, 234,  1, -1 },
    { H_BOTTIGLIE,  ROOM_TAVERN, "bottiglie",        461,  41,  51, 140, 220, 234,  1, -1 },
    { H_BOCCALE,    ROOM_TAVERN, "boccale",          242, 174,  34,  32, 214, 234,  1, -1 },
    { H_USCITA_T,   ROOM_TAVERN, "uscita",             0, 150,  22,  90,  12, 234, -1, ROOM_DOCK },

    { H_CAPPELLO,   ROOM_BEACH,  "cappello",         372, 180,  30,  26, 352, 232,  1, -1 },
    { H_CASSA,      ROOM_BEACH,  "cassa",            120, 182,  42,  32, 176, 218, -1, -1 },
    { H_RELITTO,    ROOM_BEACH,  "relitto",          296, 166, 156,  68, 290, 230,  1, -1 },
    { H_FARO,       ROOM_BEACH,  "faro",             210,  18,  32,  58,  -1,   0,  0, -1 },
    { H_SCOGLIERA,  ROOM_BEACH,  "scogliera",        150,  76, 220,  80,  -1,   0,  0, -1 },
    { H_PALME,      ROOM_BEACH,  "palme",              0,   0, 150, 170,  60, 215, -1, -1 },
    { H_MARE,       ROOM_BEACH,  "mare",             370, 121, 142,  45,  -1,   0,  0, -1 },
    { H_SENTIERO_B, ROOM_BEACH,  "sentiero",           0, 180,  30,  60,  42, 226, -1, ROOM_MAP },

    { H_CUSTODE,    ROOM_NIGHT,  "pollo gigante",    314,  92, 104, 128, 274, 224,  1, -1 },
    { H_LUNA,       ROOM_NIGHT,  "luna",              92,  75,  40,  36,  -1,   0,  0, -1 },
    { H_NAVE_F,     ROOM_NIGHT,  "nave fantasma",     17, 105,  62,  60,  60, 222, -1, -1 },
    { H_BITTA,      ROOM_NIGHT,  "bitta",            262, 160,  34,  54, 250, 226,  1, -1 },
    { H_PALMA,      ROOM_NIGHT,  "palma",            420,  10,  92, 220,  -1,   0,  0, -1 },
    { H_SENTIERO_N, ROOM_NIGHT,  "sentiero",           0, 185,  18,  55,  12, 226, -1, ROOM_MAP },
};
const int hotspot_count = sizeof(hotspots) / sizeof(hotspots[0]);

const WalkArea walk_areas[ROOM_COUNT] = {
    {  8, 200, 505, 236 },     /* molo     */
    { 10, 230, 228, 238 },     /* taberna: davanti al bancone */
    { 40, 200, 470, 236 },     /* spiaggia */
    { 10, 206, 300, 236 },     /* notte    */
};

/* prospettiva: { y lontano, altezza lontano, y vicino, altezza vicino } */
const RoomDepth room_depth[ROOM_COUNT] = {
    { 200,  96, 236, 112 },    /* molo     */
    { 230, 124, 238, 124 },    /* taberna  */
    { 200,  88, 236, 100 },    /* spiaggia */
    { 206, 100, 236, 112 },    /* notte    */
};

/* Animazioni degli sfondi (vedi anim.h). x0 e x1 multipli di 8.
   Regola: al massimo due effetti continui per scena, cosi' l'occhio non si
   disperde e il 68000 respira. I personaggi si muovono solo mentre parlano. */
const Anim anims[] = {
    /* scena          tipo        x0   y0   x1   y1   p1          p2   p3 */
    /* titolo: riflessi del mare ai lati del menu, un gabbiano ogni tanto */
    { SCENE_TITLE,  AN_WAVE,      0, 250, 184, 342, 1,          12,  4 },
    { SCENE_TITLE,  AN_WAVE,    328, 250, 512, 342, 1,          12,  4 },
    { SCENE_TITLE,  AN_FLYER,     0, 158, 512, 196, 2,           0,  0 },

    /* molo: il mare; i pirati annuiscono quando parlano */
    { ROOM_DOCK,    AN_WAVE,    120, 128, 232, 182, 1,          10,  4 },
    { ROOM_DOCK,    AN_BOB,     424, 140, 496, 196, SP_BARBA,    0,  1 },
    { ROOM_DOCK,    AN_BOB,     328, 126, 376, 178, SP_MOZZO,    0,  1 },

    /* taberna: la lanterna; l'oste annuisce quando parla */
    { ROOM_TAVERN,  AN_FLICKER, 168,  96, 232, 168, 0,           0,  0 },
    { ROOM_TAVERN,  AN_BOB,     304, 112, 408, 194, SP_OSTE,     0,  1 },

    /* spiaggia: il mare e un gabbiano di passaggio */
    { ROOM_BEACH,   AN_WAVE,    336, 122, 512, 164, 1,          12,  4 },
    { ROOM_BEACH,   AN_FLYER,   152,  12, 512,  60, 2,           0,  0 },

    /* rovine: le stelle; il Custode annuisce quando parla */
    { ROOM_NIGHT,   AN_TWINKLE,   0,   0, 344, 112, 0,           0,  0 },
    { ROOM_NIGHT,   AN_BOB,     328,  88, 424, 178, SP_CUSTODE,  0,  1 },
};
const int anim_count = sizeof(anims) / sizeof(anims[0]);

/* luoghi della mappa (coordinate dentro l'immagine della mappa) */
const MapPlace map_places[] = {
    { "Il Faro",           268,   0,  84,  32, -1 },
    { "Puerto Polli (il molo)", 196, 72, 108, 90, ROOM_DOCK },
    { "La Taberna",         98, 116,  84,  40, ROOM_TAVERN },
    { "Cala dei Gabbiani",  44, 204, 144,  34, ROOM_BEACH },
    { "Le Rovine",         300, 196, 104,  50, ROOM_NIGHT },
};
const int map_place_count = sizeof(map_places) / sizeof(map_places[0]);

int hotspot_visible(int id)
{
    switch (id) {
    case H_PIUMA:    return flag(F_GULL_FED) && !flag(F_FEATHER_TAKEN);
    case H_CAPPELLO: return !flag(F_HAT_TAKEN);
    default:         return 1;
    }
}

const char *object_name(int obj)
{
    int i;
    if (IS_ITEM(obj)) return item_names[obj];
    if (obj == H_CUSTODE && flag(F_MET_CUSTODE)) return "Custode";
    for (i = 0; i < hotspot_count; i++)
        if (hotspots[i].id == obj) return hotspots[i].name;
    return "";
}

const char *item_title(int it) { return item_titles[it]; }

const char *room_title(int room)
{
    switch (room) {
    case ROOM_DOCK:   return "Il molo";
    case ROOM_TAVERN: return "La Taberna";
    case ROOM_BEACH:  return "Cala dei Gabbiani";
    default:          return "Le Rovine";
    }
}

int room_background(int room)
{
    switch (room) {
    case ROOM_DOCK:   return BG_DOCK;
    case ROOM_TAVERN: return BG_TAVERN;
    case ROOM_BEACH:  return BG_BEACH;
    default:          return BG_NIGHT;
    }
}

void script_draw_props(int room)
{
    if (room == ROOM_DOCK && hotspot_visible(H_PIUMA))
        draw_prop(IT_PIUMA, 38, 200);
    if (room == ROOM_BEACH && hotspot_visible(H_CAPPELLO))
        draw_prop(IT_CAPPELLO, 371, 176);
}

/* ------------------------------------------------------------------ */
/* Inizio partita e ingresso nelle stanze                              */
/* ------------------------------------------------------------------ */

void script_new_game(void)
{
    add_item(IT_MONETA);
    add_item(IT_MAPPA);
    say(SP_HERO, "Eccomi a Puerto Polli! L'isola dove chiunque può diventare un pirata.");
    say(SP_HERO, "O almeno così diceva il volantino.");
    say(SP_HERO, "Primo passo: trovare qualcuno che mi spieghi come si fa. "
                 "La Taberna sembra un buon inizio.");
}

void script_enter_room(int room)
{
    if (room == ROOM_NIGHT && !flag(F_SEEN_NIGHT)) {
        set_flag(F_SEEN_NIGHT);
        say(SP_NARRATORE, "Quando arrivi alle Rovine è già calata la notte. "
                          "A Puerto Polli il tempo scorre in modo... pollesco.");
        say(SP_HERO, "C'è un pollo. Un pollo ENORME. E mi sta fissando.");
    }
}

/* ------------------------------------------------------------------ */
/* Le prove del Custode                                                */
/* ------------------------------------------------------------------ */

static void check_proofs(void)
{
    if (flag(F_PROOF_HAT) && flag(F_PROOF_FEATHER) && flag(F_PROOF_BOOK)) {
        set_flag(F_GAME_WON);
        say(SP_CUSTODE, "Cappello, piuma e Codice. Hai superato tutte le prove!");
        say(SP_CUSTODE, "Con l'autorità conferitami dal Consiglio dei Polli, ti proclamo...");
        say(SP_CUSTODE, "...PIRATA DI PUERTO POLLI!");
        say(SP_HERO, "Evviva! E adesso cosa faccio?");
        say(SP_CUSTODE, "Quello che fanno tutti i pirati: torni alla Taberna e racconti "
                        "questa storia esagerandola un po'.");
        say(SP_CUSTODE, "E ricorda: mai fidarsi di un pollo. Tranne me. Forse.");
        queue_call(CALL_ENDING);
    } else {
        int missing = !flag(F_PROOF_HAT) + !flag(F_PROOF_FEATHER) + !flag(F_PROOF_BOOK);
        say(SP_CUSTODE, missing == 1 ? "Ti manca ancora una prova." : "Ti mancano ancora due prove.");
    }
}

static void custode_proofs_summary(void)
{
    say(SP_CUSTODE, "Le prove sono tre.");
    say(SP_CUSTODE, flag(F_PROOF_HAT) ? "Il cappello: fatto." :
                    "Uno: un cappello da pirata. In testa, non in tasca.");
    say(SP_CUSTODE, flag(F_PROOF_FEATHER) ? "La piuma: fatto." :
                    "Due: una piuma di un uccello che NON sia un pollo. Non chiedermi di spiumarmi.");
    say(SP_CUSTODE, flag(F_PROOF_BOOK) ? "Il Codice: fatto." :
                    "Tre: il Codice dei Pirati, l'antico libro delle regole.");
}

/* ------------------------------------------------------------------ */
/* Azioni                                                              */
/* ------------------------------------------------------------------ */

static int open_crate(void)
{
    if (flag(F_CRATE_OPEN)) {
        say(SP_HERO, "È già aperta. Ed è vuota.");
        return 1;
    }
    if (!has_item(IT_CHIAVE)) {
        say(SP_HERO, "È chiusa con un lucchetto. Mi serve una chiave.");
        return 1;
    }
    set_flag(F_CRATE_OPEN);
    remove_item(IT_CHIAVE);
    add_item(IT_CALAMARO);
    add_item(IT_LIBRO);
    say(SP_HERO, "Clic! La chiave gira. La cassa si apre con un cigolio drammatico.");
    say(SP_HERO, "Dentro ci sono un calamaro essiccato e un vecchio libro. Li prendo entrambi.");
    return 1;
}

static int give_to_custode(int it)
{
    if (!flag(F_KNOWS_PROOFS)) {
        say(SP_CUSTODE, "Un regalo? Prima parliamo, straniero. Le formalità sono importanti.");
        return 1;
    }
    switch (it) {
    case IT_CAPPELLO:
        if (!flag(F_HAT_WORN)) {
            say(SP_CUSTODE, "In TESTA, ho detto. Un cappello in mano è solo... un cappello.");
            return 1;
        }
        if (flag(F_PROOF_HAT)) { say(SP_CUSTODE, "Il cappello l'ho già approvato."); return 1; }
        set_flag(F_PROOF_HAT);
        say(SP_CUSTODE, "Mmh. Tricorno autentico, profumo di relitto. Approvato!");
        check_proofs();
        return 1;
    case IT_PIUMA:
        remove_item(IT_PIUMA);
        set_flag(F_PROOF_FEATHER);
        say(SP_CUSTODE, "Una piuma di gabbiano! Sa ancora di calamaro. Approvata!");
        check_proofs();
        return 1;
    case IT_LIBRO:
        remove_item(IT_LIBRO);
        set_flag(F_PROOF_BOOK);
        say(SP_CUSTODE, "Il Codice dei Pirati! Articolo 1: mai fidarsi di un pollo. Saggio. Approvato!");
        check_proofs();
        return 1;
    case IT_CALAMARO:
        say(SP_CUSTODE, "Non mangio calamari. Sono granivoro. E ho un certo stile.");
        return 1;
    case IT_BOTTIGLIA:
        say(SP_CUSTODE, "Grog? Io? Sono in servizio.");
        return 1;
    case IT_MONETA:
        say(SP_CUSTODE, "Il Custode non si compra. Al massimo si noleggia.");
        return 1;
    }
    return 0;
}

static int give_to_barba(int it)
{
    if (!flag(F_HAT_WORN)) {
        say(SP_BARBA, "Prima mettiti un cappello. Non faccio affari con i turisti.");
        return 1;
    }
    if (it == IT_BOTTIGLIA) {
        remove_item(IT_BOTTIGLIA);
        add_item(IT_CHIAVE);
        set_flag(F_GOT_KEY);
        say(SP_BARBA, "Il mio grog! Fumante come piace a me!");
        say(SP_NARRATORE, "Il capitano lo beve tutto d'un fiato. Gli fumano anche le orecchie.");
        say(SP_BARBA, "Affare fatto, mozzo. Ecco la chiave della cassa sulla spiaggia.");
        say(SP_BARBA, "Non chiedermi perché odora di aringa.");
        return 1;
    }
    if (it == IT_MONETA) {
        say(SP_BARBA, "Una moneta? Cosa me ne faccio? Portami del grog, piuttosto!");
        return 1;
    }
    say(SP_BARBA, "Non mi serve. A me serve qualcosa che fumi e che si beva.");
    return 1;
}

int script_action(int verb, int o1, int o2)
{
    /* ---- oggetti dell'inventario ---- */
    if (verb == V_GUARDA && IS_ITEM(o1)) {
        switch (o1) {
        case IT_BOTTIGLIA: say(SP_HERO, "Una bottiglia di grog. Fuma. Le bottiglie non dovrebbero fumare."); break;
        case IT_MONETA:    say(SP_HERO, "Un doblone. Tutti i miei risparmi."); break;
        case IT_CHIAVE:    say(SP_HERO, "Una chiave arrugginita che odora di aringa."); break;
        case IT_MAPPA:     say(SP_HERO, "La mappa di Puerto Polli. Se la uso posso spostarmi sull'isola."); break;
        case IT_PIUMA:     say(SP_HERO, "Una piuma di gabbiano. Decisamente NON di pollo."); break;
        case IT_CALAMARO:  say(SP_HERO, "Un calamaro essiccato. Il portafortuna del capitano. Puzza di fortuna."); break;
        case IT_CAPPELLO:
            say(SP_HERO, flag(F_HAT_WORN) ? "Il mio cappello da pirata. Ce l'ho in testa e mi sta benissimo."
                                          : "Un cappello da pirata. Starebbe meglio sulla mia testa.");
            break;
        case IT_LIBRO:
            say(SP_HERO, "\"Il Codice dei Pirati\". Articolo 1: mai fidarsi di un pollo.");
            say(SP_HERO, "Articolo 2: vedi articolo 1.");
            break;
        }
        return 1;
    }
    if (verb == V_USA && IS_ITEM(o1) && o2 == OBJ_NONE) {
        switch (o1) {
        case IT_MAPPA: open_map(); return 1;
        case IT_CAPPELLO:
            if (flag(F_HAT_WORN)) {
                say(SP_HERO, "Ce l'ho già in testa. Toglierlo? Mai più.");
            } else {
                set_flag(F_HAT_WORN);
                say(SP_HERO, "Me lo metto in testa. Mi sento già più pirata. E un po' umido.");
            }
            return 1;
        case IT_BOTTIGLIA: say(SP_HERO, "Berlo io? Fuma! Neanche morto. Beh, forse da morto."); return 1;
        case IT_LIBRO:     say(SP_HERO, "Leggo a caso: \"Articolo 14: il grog va bevuto, non annusato.\" Profondo."); return 1;
        case IT_PIUMA:     say(SP_HERO, "Mi faccio il solletico. Ih ih. Ok, basta."); return 1;
        case IT_CALAMARO:  say(SP_HERO, "Non ho COSÌ fame."); return 1;
        case IT_MONETA:    say(SP_HERO, "Potrei spenderla. Alla Taberna, per esempio."); return 1;
        }
    }
    if (verb == V_APRI && o1 == IT_LIBRO) {
        say(SP_HERO, "\"Articolo 7: un vero pirata ha cappello, piuma e questo libro.\" Interessante...");
        return 1;
    }

    /* combinazioni "Usa X con Y" */
    if (verb == V_USA && o1 == IT_CHIAVE && o2 == H_CASSA) return open_crate();
    /* "Usa X con personaggio" equivale a "Dai X a personaggio" */
    if (verb == V_USA && IS_ITEM(o1) &&
        (o2 == H_GABBIANO || o2 == H_CUSTODE || o2 == H_BARBA || o2 == H_OSTE || o2 == H_MOZZO)) {
        int t = o1;
        o1 = o2;
        o2 = t;
        verb = V_DAI;
    }

    /* ---- molo ---- */
    switch (o1) {
    case H_TABERNA:
        if (verb == V_GUARDA) { say(SP_HERO, "La Taberna. Il posto più malfamato dell'isola. E anche l'unico."); return 1; }
        if (verb == V_APRI || verb == V_VAI) { go_room(ROOM_TAVERN); return 1; }
        if (verb == V_CHIUDI) { say(SP_HERO, "È già chiusa. Più o meno."); return 1; }
        break;
    case H_INSEGNA:
        if (verb == V_GUARDA) {
            say(SP_HERO, "\"TABERNA\". Qualcuno ha cancellato una V. O aggiunto una B.");
            say(SP_HERO, "I filologi dell'isola ne discutono ancora.");
            return 1;
        }
        break;
    case H_BARBA:
        if (verb == V_GUARDA) {
            say(SP_HERO, "Un pirata enorme, con benda, barba e un tricorno che ha visto giorni migliori.");
            if (!flag(F_GOT_KEY)) say(SP_HERO, "Ha l'aria assetata.");
            return 1;
        }
        if (verb == V_PARLA) {
            if (!flag(F_HAT_WORN)) {
                say(SP_BARBA, "Bah! Non parlo con chi va in giro senza cappello.");
                say(SP_BARBA, "Senza cappello sei un turista. E i turisti si buttano a mare.");
                if (has_item(IT_CAPPELLO)) say(SP_HERO, "(Forse dovrei mettermi il cappello che ho in tasca.)");
                return 1;
            }
            start_dialog(SP_BARBA);
            return 1;
        }
        if (verb == V_DAI && IS_ITEM(o2)) return give_to_barba(o2);
        if (verb == V_RACCOGLI || verb == V_SPINGI || verb == V_TIRA) {
            say(SP_BARBA, "Mettimi le mani addosso e ti uso come àncora.");
            return 1;
        }
        break;
    case H_MOZZO:
        if (verb == V_GUARDA) { say(SP_HERO, "Un pirata piccolo ma determinato. Ha un cappello più grande delle sue ambizioni."); return 1; }
        if (verb == V_PARLA) {
            if (!flag(F_MOZZO_1)) {
                set_flag(F_MOZZO_1);
                say(SP_MOZZO, "Ciao! Sono Pino, il mozzo. Il capitano dice che sono il miglior mozzo che abbia mai avuto.");
                say(SP_MOZZO, "Sono anche l'unico.");
            } else if (!flag(F_MOZZO_2)) {
                set_flag(F_MOZZO_2);
                say(SP_MOZZO, "Il capitano oggi è di cattivo umore. Quando è di cattivo umore, di solito ha sete.");
                say(SP_MOZZO, "Beve solo il grog della Taberna. Quello che fuma.");
            } else {
                clear_flag(F_MOZZO_1);
                clear_flag(F_MOZZO_2);
                say(SP_MOZZO, "Lo sai che quel gabbiano va matto per i calamari? Una volta ne ha rubato uno al capitano!");
                say(SP_MOZZO, "Il capitano non gliel'ha mai perdonata. Da allora tiene il suo calamaro sotto chiave.");
            }
            return 1;
        }
        if (verb == V_DAI && IS_ITEM(o2)) {
            say(SP_MOZZO, "Per me? No, grazie. Il capitano dice di non accettare regali dagli sconosciuti.");
            return 1;
        }
        break;
    case H_GABBIANO:
        if (verb == V_GUARDA) { say(SP_HERO, "Un gabbiano. Mi fissa con l'aria di chi sa qualcosa che io non so."); return 1; }
        if (verb == V_RACCOGLI) { say(SP_HERO, "Ci ho provato. Mi ha beccato. Due volte."); return 1; }
        if (verb == V_PARLA) {
            say(SP_HERO, "Ciao, gabbiano.");
            say(SP_GABBIANO, "Squaaak.");
            say(SP_HERO, "Credo fosse un \"no\".");
            return 1;
        }
        if (verb == V_DAI && o2 == IT_CALAMARO) {
            remove_item(IT_CALAMARO);
            set_flag(F_GULL_FED);
            say(SP_NARRATORE, "Il gabbiano inghiotte il calamaro intero, senza masticare.");
            say(SP_NARRATORE, "Poi, per gratitudine (o per digestione), lascia cadere una piuma sul molo.");
            say(SP_HERO, "Grazie... credo.");
            return 1;
        }
        if (verb == V_DAI && IS_ITEM(o2)) { say(SP_HERO, "Il gabbiano lo guarda con disprezzo. È un gabbiano esigente."); return 1; }
        break;
    case H_PIUMA:
        if (verb == V_GUARDA) { say(SP_HERO, "Una piuma di gabbiano, ancora tiepida. Bleah."); return 1; }
        if (verb == V_RACCOGLI) {
            set_flag(F_FEATHER_TAKEN);
            add_item(IT_PIUMA);
            say(SP_HERO, "Presa! Una piuma di un uccello che NON è un pollo.");
            return 1;
        }
        break;
    case H_BANCARELLA:
        if (verb == V_GUARDA) { say(SP_HERO, "Vende souvenir: calamite a forma di pollo, cartoline di polli e... polli."); return 1; }
        if (verb == V_RACCOGLI) { say(SP_HERO, "Ho già abbastanza polli nella mia vita."); return 1; }
        if (verb == V_PARLA) { say(SP_HERO, "Non c'è nessuno. Il venditore sarà andato a comprare altri polli."); return 1; }
        break;
    case H_VELIERO:
        if (verb == V_GUARDA) { say(SP_HERO, "Un bel veliero. Non è mio. Niente qui è mio, a parte i debiti."); return 1; }
        break;
    case H_PAESE:
        if (verb == V_GUARDA) {
            say(SP_HERO, "Puerto Polli: case bianche e tetti rossi. Credo. Qui è tutto in bianco e nero.");
            return 1;
        }
        break;

    /* ---- taberna ---- */
    case H_OSTE:
        if (verb == V_GUARDA) { say(SP_HERO, "L'oste. Un occhio solo, ma ti tiene d'occhio lo stesso."); return 1; }
        if (verb == V_PARLA) { start_dialog(SP_OSTE); return 1; }
        if (verb == V_DAI && o2 == IT_MONETA) {
            if (flag(F_GOT_GROG)) { say(SP_OSTE, "Hai già avuto il tuo grog. Due grog per un novellino sono un rischio sanitario."); return 1; }
            remove_item(IT_MONETA);
            add_item(IT_BOTTIGLIA);
            set_flag(F_GOT_GROG);
            say(SP_OSTE, "Una moneta? Affare fatto! Ecco a te una bottiglia di grog.");
            say(SP_HERO, "Ehm... perché la bottiglia fuma?");
            say(SP_OSTE, "È il grog. Fa così. Non berlo vicino alle candele.");
            return 1;
        }
        if (verb == V_DAI && IS_ITEM(o2)) { say(SP_OSTE, "Niente baratti. Accetto solo monete. E qualche volta galline."); return 1; }
        break;
    case H_BANDIERA:
        if (verb == V_GUARDA) { say(SP_HERO, "Un teschio con due ossa incrociate. O due cosce di pollo? Con questa luce non si capisce."); return 1; }
        if (verb == V_RACCOGLI || verb == V_TIRA) { say(SP_OSTE, "Giù le mani dalla bandiera! È un cimelio di famiglia."); return 1; }
        break;
    case H_QUADRO:
        if (verb == V_GUARDA) { say(SP_HERO, "Il ritratto di un pollo con la benda sull'occhio. C'è una dedica: \"Al mio oste preferito\"."); return 1; }
        break;
    case H_LANTERNA:
        if (verb == V_GUARDA) { say(SP_HERO, "Una lanterna. Fa più fumo che luce. Come il grog."); return 1; }
        if (verb == V_RACCOGLI) { say(SP_HERO, "Scotta. E poi resterebbe al buio l'oste, che già ci vede per metà."); return 1; }
        break;
    case H_FINESTRA:
        if (verb == V_GUARDA) { say(SP_HERO, "Una finestra così sporca che fuori sembra sempre notte."); return 1; }
        if (verb == V_APRI) { say(SP_HERO, "È sigillata da anni di grasso di pollo fritto."); return 1; }
        break;
    case H_BOTTIGLIE:
        if (verb == V_GUARDA) { say(SP_HERO, "Grog, grog d'annata, grog light e un barattolo con scritto \"NON grog\"."); return 1; }
        if (verb == V_RACCOGLI) { say(SP_OSTE, "Ehi! Quelle si pagano. Una moneta a bottiglia."); return 1; }
        break;
    case H_BOCCALE:
        if (verb == V_GUARDA) { say(SP_HERO, "Un boccale vuoto. O quasi. Meglio non indagare."); return 1; }
        if (verb == V_RACCOGLI) { say(SP_HERO, "È appiccicato al bancone. Non voglio sapere con cosa."); return 1; }
        break;
    case H_USCITA_T:
        if (verb == V_GUARDA) { say(SP_HERO, "L'uscita. Porta al molo."); return 1; }
        if (verb == V_VAI || verb == V_APRI) { go_room(ROOM_DOCK); return 1; }
        break;

    /* ---- spiaggia ---- */
    case H_CAPPELLO:
        if (verb == V_GUARDA) { say(SP_HERO, "Un cappello da pirata! Un po' bagnato, un po' mangiucchiato, ma ha stile."); return 1; }
        if (verb == V_RACCOGLI) { say(SP_HERO, "È incastrato tra le assi del relitto. Devo tirarlo con forza."); return 1; }
        if (verb == V_TIRA) {
            set_flag(F_HAT_TAKEN);
            add_item(IT_CAPPELLO);
            say(SP_HERO, "Ooooh... issa! Ce l'ho!");
            say(SP_NARRATORE, "Un granchio, che ci abitava dentro, se ne va profondamente offeso.");
            return 1;
        }
        break;
    case H_CASSA:
        if (verb == V_GUARDA) {
            say(SP_HERO, flag(F_CRATE_OPEN) ? "La cassa è aperta e vuota. Come il mio portafoglio."
                                            : "Una vecchia cassa chiusa con un lucchetto. Sopra c'è scritto \"B.P.\"");
            return 1;
        }
        if (verb == V_APRI) return open_crate();
        if (verb == V_CHIUDI) { say(SP_HERO, flag(F_CRATE_OPEN) ? "Meglio lasciarla aperta: non ho più la chiave." : "È già chiusa."); return 1; }
        if (verb == V_SPINGI || verb == V_TIRA) { say(SP_HERO, "Pesa una tonnellata. Sarà piena di piombo. O di zuppa."); return 1; }
        if (verb == V_RACCOGLI) { say(SP_HERO, "Troppo pesante. E poi non mi entra in tasca."); return 1; }
        break;
    case H_RELITTO:
        if (verb == V_GUARDA) {
            say(SP_HERO, "Il relitto di una barca. Sulla fiancata c'è scritto \"Pollo Volante\". Non ha volato a lungo.");
            if (!flag(F_HAT_TAKEN)) say(SP_HERO, "C'è un cappello incastrato tra le assi.");
            return 1;
        }
        if (verb == V_SPINGI || verb == V_TIRA) { say(SP_HERO, "Non si muove. È più arenato di me."); return 1; }
        break;
    case H_FARO:
        if (verb == V_GUARDA) { say(SP_HERO, "Il faro. Dicono che sia chiuso per ferie da vent'anni."); return 1; }
        break;
    case H_SCOGLIERA:
        if (verb == V_GUARDA) { say(SP_HERO, "Scogli a strapiombo. Ottimi per meditare, pessimi per cadere."); return 1; }
        break;
    case H_PALME:
        if (verb == V_GUARDA) { say(SP_HERO, "Palme. Tante palme. Nessuna noce di cocco, ovviamente."); return 1; }
        if (verb == V_SPINGI || verb == V_TIRA || verb == V_RACCOGLI) { say(SP_HERO, "Non sono così forte. Non ancora."); return 1; }
        break;
    case H_MARE:
        if (verb == V_GUARDA) { say(SP_HERO, "Il mare. Bagnato, come sempre."); return 1; }
        break;

    /* ---- rovine ---- */
    case H_CUSTODE:
        if (verb == V_GUARDA) {
            if (flag(F_MET_CUSTODE)) say(SP_HERO, "Il Custode di Puerto Polli. Un pollo con un contratto a tempo indeterminato.");
            else say(SP_HERO, "Un pollo. Un pollo ENORME, alto quanto me, con l'aria molto seria.");
            return 1;
        }
        if (verb == V_PARLA) { start_dialog(SP_CUSTODE); return 1; }
        if (verb == V_DAI && IS_ITEM(o2)) { if (give_to_custode(o2)) return 1; }
        if (verb == V_RACCOGLI) { say(SP_CUSTODE, "Prova a sollevarmi e ti becco fino a Natale."); return 1; }
        if (verb == V_SPINGI || verb == V_TIRA) { say(SP_CUSTODE, "Ehi! Rispetto per le istituzioni!"); return 1; }
        break;
    case H_LUNA:
        if (verb == V_GUARDA) { say(SP_HERO, "La luna piena. Ha la faccia di qualcuno che ha visto troppe storie di pirati."); return 1; }
        break;
    case H_NAVE_F:
        if (verb == V_GUARDA) { say(SP_HERO, "Una nave fantasma! O una nave normale molto pallida."); return 1; }
        break;
    case H_BITTA:
        if (verb == V_GUARDA) { say(SP_HERO, "Una bitta per legare le barche. Qualcuno ci ha inciso: \"W i polli\"."); return 1; }
        if (verb == V_SPINGI || verb == V_TIRA) { say(SP_HERO, "Non si muove. Il molo ringrazia."); return 1; }
        break;
    case H_PALMA:
        if (verb == V_GUARDA) { say(SP_HERO, "Una palma che si sporge sul mare, come se volesse andarsene anche lei."); return 1; }
        break;

    case H_SENTIERO_D: case H_SENTIERO_B: case H_SENTIERO_N:
        if (verb == V_GUARDA) { say(SP_HERO, "Un sentiero che porta al resto dell'isola."); return 1; }
        if (verb == V_VAI) { open_map(); return 1; }
        break;
    }
    return 0;
}

void script_default(int verb, int o1, int o2)
{
    (void)o1;
    (void)o2;
    switch (verb) {
    case V_GUARDA:   say(SP_HERO, "Niente di speciale."); break;
    case V_RACCOGLI: say(SP_HERO, "Non posso prenderlo."); break;
    case V_APRI:     say(SP_HERO, "Non si apre."); break;
    case V_CHIUDI:   say(SP_HERO, "Non si chiude."); break;
    case V_SPINGI:   say(SP_HERO, "Non si sposta di un millimetro."); break;
    case V_TIRA:     say(SP_HERO, "Tiro, tiro... non succede niente."); break;
    case V_PARLA:    say(SP_HERO, "Non mi sembra un grande conversatore."); break;
    case V_DAI:      say(SP_HERO, "Non credo che lo voglia."); break;
    case V_USA:      say(SP_HERO, "Non funziona."); break;
    default: break;
    }
}

/* ------------------------------------------------------------------ */
/* Dialoghi                                                            */
/* ------------------------------------------------------------------ */

enum {
    D_CHI, D_ISOLA, D_PIRATA, D_GROG, D_CIAO,
    D_BP_CAPPELLO, D_BP_CUSTODE, D_BP_CHIAVE,
    D_CU_PIRATA, D_CU_PERCHE, D_CU_PROVE, D_CU_CAPPELLO, D_CU_TASCA
};

#define ADD(i, t) do { ids[n] = (i); texts[n] = (t); n++; } while (0)

int dialog_options(int npc, int ids[MAX_CHOICES], const char *texts[MAX_CHOICES])
{
    int n = 0;
    switch (npc) {
    case SP_OSTE:
        ADD(D_CHI, "Chi sei?");
        ADD(D_ISOLA, "Che posto è Puerto Polli?");
        ADD(D_PIRATA, "Voglio diventare un pirata!");
        ADD(D_GROG, "Vorrei qualcosa da bere.");
        ADD(D_CIAO, "Arrivederci.");
        break;
    case SP_BARBA:
        ADD(D_CHI, "Chi sei?");
        ADD(D_BP_CAPPELLO, "Ti piace il mio cappello?");
        ADD(D_BP_CUSTODE, "Conosci il Custode?");
        if (flag(F_KNOWS_KEY) && !flag(F_GOT_KEY)) ADD(D_BP_CHIAVE, "Mi daresti la chiave della cassa?");
        ADD(D_CIAO, "Ci vediamo, capitano.");
        break;
    case SP_CUSTODE:
        if (!flag(F_KNOWS_PROOFS)) ADD(D_CU_PIRATA, "Voglio diventare un pirata.");
        ADD(D_CU_PERCHE, "Perché un pollo fa da custode?");
        if (flag(F_KNOWS_PROOFS)) ADD(D_CU_PROVE, "Quali erano le prove?");
        if (flag(F_KNOWS_PROOFS) && flag(F_HAT_WORN) && !flag(F_PROOF_HAT))
            ADD(D_CU_CAPPELLO, "Guarda il mio cappello!");
        if (flag(F_KNOWS_PROOFS) && has_item(IT_CAPPELLO) && !flag(F_HAT_WORN))
            ADD(D_CU_TASCA, "Ho un cappello in tasca, vale?");
        ADD(D_CIAO, "Arrivederci.");
        break;
    }
    return n;
}

void dialog_choose(int npc, int id)
{
    switch (npc) {
    case SP_OSTE:
        switch (id) {
        case D_CHI:
            say(SP_HERO, "Chi sei?");
            say(SP_OSTE, "Bartolo, l'oste. Servo grog da quando ho perso l'occhio.");
            say(SP_HERO, "E come l'hai perso?");
            say(SP_OSTE, "Cercando il resto nella cassa. Non chiedere.");
            break;
        case D_ISOLA:
            say(SP_HERO, "Che posto è Puerto Polli?");
            say(SP_OSTE, "Un'isola di pirati. E di polli. Più polli che pirati, a dire il vero.");
            say(SP_OSTE, "A nord c'è il faro, a sud la Cala dei Gabbiani, a est le Rovine. Usa la mappa.");
            break;
        case D_PIRATA:
            set_flag(F_KNOWS_CUSTODE);
            say(SP_HERO, "Voglio diventare un pirata!");
            say(SP_OSTE, "Tutti vogliono diventare pirati. Ma qui serve la benedizione del Custode di Puerto Polli.");
            say(SP_OSTE, "Lo trovi alle Rovine. Porta rispetto, e non nominare mai il brodo.");
            break;
        case D_GROG:
            say(SP_HERO, "Vorrei qualcosa da bere.");
            if (flag(F_GOT_GROG)) say(SP_OSTE, "Hai già avuto il tuo grog. Il resto è per i clienti abituali.");
            else say(SP_OSTE, "Grog fresco, distillato stamattina! Costa una moneta. Dammi la moneta e la bottiglia è tua.");
            break;
        default:
            say(SP_HERO, "Arrivederci.");
            say(SP_OSTE, "Torna quando hai sete. O dei soldi. Meglio entrambi.");
            end_dialog();
            break;
        }
        break;
    case SP_BARBA:
        switch (id) {
        case D_CHI:
            say(SP_HERO, "Chi sei?");
            say(SP_BARBA, "Capitan Barbapollo, terrore dei sette mari!");
            say(SP_BARBA, "Beh... di tre mari. Gli altri quattro non li ho ancora trovati.");
            break;
        case D_BP_CAPPELLO:
            say(SP_HERO, "Ti piace il mio cappello?");
            say(SP_BARBA, "Mmh. Puzza di relitto e di granchio. Un vero cappello da pirata. Ora possiamo parlare.");
            break;
        case D_BP_CUSTODE:
            set_flag(F_KNOWS_KEY);
            set_flag(F_KNOWS_CUSTODE);
            say(SP_HERO, "Conosci il Custode?");
            say(SP_BARBA, "Il pollone delle Rovine? Vuole sempre le stesse cose: cappello, piuma e il Codice dei Pirati.");
            say(SP_BARBA, "Il Codice ce l'avevo... l'ho chiuso nella cassa sulla spiaggia, insieme al mio calamaro portafortuna.");
            say(SP_HERO, "E la chiave?");
            say(SP_BARBA, "Ce l'ho io. Ma parlare mi mette sete. Tanta sete.");
            break;
        case D_BP_CHIAVE:
            say(SP_HERO, "Mi daresti la chiave della cassa?");
            say(SP_BARBA, "In cambio di un bel grog della Taberna, quello che fuma. Niente grog, niente chiave.");
            break;
        default:
            say(SP_HERO, "Ci vediamo, capitano.");
            say(SP_BARBA, "Vento in poppa, mozzo.");
            end_dialog();
            break;
        }
        break;
    case SP_CUSTODE:
        switch (id) {
        case D_CU_PIRATA:
            set_flag(F_KNOWS_PROOFS);
            say(SP_HERO, "Voglio diventare un pirata.");
            say(SP_CUSTODE, "Ah, un aspirante. Per avere la mia benedizione dovrai superare tre prove.");
            custode_proofs_summary();
            break;
        case D_CU_PERCHE:
            say(SP_HERO, "Perché un pollo fa da custode?");
            say(SP_CUSTODE, "Perché i ladri scappano davanti ai cani, ma nessuno sospetta di un pollo.");
            say(SP_CUSTODE, "E poi ho un ottimo contratto. Mangime incluso.");
            break;
        case D_CU_PROVE:
            say(SP_HERO, "Quali erano le prove?");
            custode_proofs_summary();
            break;
        case D_CU_CAPPELLO:
            say(SP_HERO, "Guarda il mio cappello!");
            give_to_custode(IT_CAPPELLO);
            if (flag(F_GAME_WON)) end_dialog();
            break;
        case D_CU_TASCA:
            say(SP_HERO, "Ho un cappello in tasca, vale?");
            say(SP_CUSTODE, "No. Un cappello in tasca è un fazzoletto ambizioso. In TESTA!");
            break;
        default:
            say(SP_HERO, "Arrivederci.");
            say(SP_CUSTODE, "Coccodè... cioè, a presto, straniero.");
            end_dialog();
            break;
        }
        break;
    }
}

/* Chiamata dal motore quando si apre un dialogo: frasi d'apertura. */
void dialog_greeting(int npc)
{
    switch (npc) {
    case SP_OSTE:
        if (!flag(F_MET_OSTE)) {
            set_flag(F_MET_OSTE);
            say(SP_OSTE, "Ah, tu sei il nuovo arrivato, eh? A Puerto Polli non si arriva per caso.");
            say(SP_OSTE, "C'è sempre un motivo... e di solito è un guaio.");
        } else {
            say(SP_OSTE, "Di nuovo tu. Cosa ti servo?");
        }
        break;
    case SP_BARBA:
        say(SP_BARBA, "Arr! Ora sì che sembri un pirata. Cosa vuoi?");
        break;
    case SP_CUSTODE:
        if (!flag(F_MET_CUSTODE)) {
            set_flag(F_MET_CUSTODE);
            say(SP_HERO, "Ehm... ciao, pollo.");
            say(SP_CUSTODE, "Non sono un pollo qualunque... Sono il Custode di Puerto Polli!");
        } else {
            say(SP_CUSTODE, "Sei tornato, aspirante pirata.");
        }
        break;
    }
}

void script_map_place(int idx)
{
    const MapPlace *p = &map_places[idx];
    if (p->room < 0) {
        say(SP_NARRATORE, "Il faro è chiuso. Sulla porta c'è un cartello: \"Torno subito. Il guardiano\".");
        say(SP_NARRATORE, "Il cartello sembra lì da vent'anni.");
        return;
    }
    go_room(p->room);
}
