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

/* le regole elencano solo i campi che servono: gli altri valgono zero */
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

/* ------------------------------------------------------------------ */
/* Dati                                                               */
/* ------------------------------------------------------------------ */

const char *const verb_names[V_COUNT + 1] = {
    "Dai", "Apri", "Chiudi", "Raccogli", "Guarda", "Spingi",
    "Usa", "Parla con", "Tira", "Vai a"
};

const char *const speaker_names[SP_COUNT] = {
    "Tu", "Oste Bartolo", "Capitan Barbapollo", "Mozzo Pino",
    "Il Custode", "Gabbiano", "Ossobuco", "Capitan Brodo", ""
};

static const char *const item_names[IT_COUNT] = {
    "bottiglia di grog", "moneta", "chiave", "mappa",
    "piuma", "calamaro", "cappello", "libro",
    "bussola", "lanterna", "carta nautica", "teschio"
};

static const char *const item_titles[IT_COUNT] = {
    "Bottiglia", "Moneta", "Chiave", "Mappa",
    "Piuma", "Calamaro", "Cappello", "Libro",
    "Bussola", "Lanterna", "Carta nautica", "Teschio"
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
    { H_VELIERO,    ROOM_DOCK,   "veliero",            0,  26,  84, 115,  60, 222, -1, -1 },
    { H_PAESE,      ROOM_DOCK,   "Puerto Polli",     150,  10, 180,  80,  -1,   0,  0, -1 },
    { H_SENTIERO_D, ROOM_DOCK,   "sentiero",           0, 190,  18,  50,  10, 226, -1, ROOM_MAP },

    { H_OSTE,       ROOM_TAVERN, "oste",             307, 113,  99,  98, 170, 281,  1, -1 },
    { H_BANDIERA,   ROOM_TAVERN, "bandiera",          10,  51,  60,  77,  70, 281, -1, -1 },
    { H_QUADRO,     ROOM_TAVERN, "quadretto",         19, 140,  32,  32,  70, 281, -1, -1 },
    { H_LANTERNA,   ROOM_TAVERN, "lanterna",         182, 106,  34,  50, 190, 281,  1, -1 },
    { H_FINESTRA,   ROOM_TAVERN, "finestra",         258, 103,  42,  45, 214, 281,  1, -1 },
    { H_BOTTIGLIE,  ROOM_TAVERN, "bottiglie",        461,  41,  51, 140, 220, 281,  1, -1 },
    { H_BOCCALE,    ROOM_TAVERN, "boccale",          242, 174,  34,  32, 214, 281,  1, -1 },
    { H_USCITA_T,   ROOM_TAVERN, "uscita",             0, 150,  22,  90,  44, 281, -1, ROOM_DOCK },

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
    { H_NAVE_F,     ROOM_NIGHT,  "nave fantasma",     17, 105,  62,  60,  60, 230, -1, -1 },
    { H_BITTA,      ROOM_NIGHT,  "bitta",            262, 160,  34,  54, 250, 226,  1, -1 },
    { H_PALMA,      ROOM_NIGHT,  "palma",            420,  10,  92, 220,  -1,   0,  0, -1 },
    { H_SENTIERO_N, ROOM_NIGHT,  "sentiero",           0, 185,  40,  55,  44, 230, -1, ROOM_MAP },

    /* ---- Parte II: il faro ---- */
    { H_GABBIANO_F, ROOM_FARO,   "gabbiano",         482, 144,  30,  28, 340, 212,  1, -1 },
    { H_PALO,       ROOM_FARO,   "palo",             484, 172,  26,  46, 340, 212,  1, -1 },
    { H_PORTA_F,    ROOM_FARO,   "porta del faro",   342, 118,  24,  32, 386, 181,  1, -1 },
    { H_SCALA_F,    ROOM_FARO,   "scalinata",        370, 146,  30,  34, 386, 181,  1, -1 },
    { H_FARO_E,     ROOM_FARO,   "faro",             355,   2,  52, 116,  -1,   0,  0, -1 },
    { H_CASA,       ROOM_FARO,   "casa del guardiano", 292, 98, 50,  52,  -1,   0,  0, -1 },
    { H_CARTELLO,   ROOM_FARO,   "cartello",         124, 170,  66,  56, 200, 226, -1, -1 },
    { H_VELA,       ROOM_FARO,   "barca a vela",     132, 120,  32,  32,  -1,   0,  0, -1 },
    { H_SOLE,       ROOM_FARO,   "sole",              80, 106,  44,  26,  -1,   0,  0, -1 },
    { H_MARE_F,     ROOM_FARO,   "mare",               0, 130, 124,  70,  -1,   0,  0, -1 },
    { H_STACCIONATA, ROOM_FARO,  "staccionata",      350, 206,  92,  34, 340, 226,  1, -1 },
    { H_SENTIERO_F, ROOM_FARO,   "sentiero",           0, 206,  26,  34,  34, 230, -1, ROOM_MAP },

    { H_LANTERNA_F, ROOM_FARO_IN, "lanterna",        372, 150,  52,  64, 384, 184,  1, -1 },
    { H_CARTA,      ROOM_FARO_IN, "carta nautica",   204, 178, 100,  30, 252, 182,  1, -1 },
    { H_LUMINO,     ROOM_FARO_IN, "lumino",          222, 150,  26,  30, 236, 180,  1, -1 },
    { H_MECCANISMO, ROOM_FARO_IN, "meccanismo",      262, 110, 118,  66, 300, 180,  1, -1 },
    { H_LENTE,      ROOM_FARO_IN, "lente",           268,   0, 104, 110, 300, 180,  1, -1 },
    { H_TAVOLO,     ROOM_FARO_IN, "tavolo",          140, 178, 212,  62, 252, 182,  1, -1 },
    { H_SCALA_I,    ROOM_FARO_IN, "scala",            50,   0,  76, 176, 112, 182, -1, ROOM_FARO },
    { H_PORTICINA,  ROOM_FARO_IN, "porticina",         0,  48,  36, 140,  46, 184, -1, -1 },
    { H_FINESTRE,   ROOM_FARO_IN, "finestre",        140,  10, 124, 110,  -1,   0,  0, -1 },
    { H_FINESTRE,   ROOM_FARO_IN, "finestre",        384,   0, 128, 148,  -1,   0,  0, -1 },

    /* ---- Parte III: la grotta e la nave ---- */
    { H_OSSOBUCO,   ROOM_GROTTA, "scheletro",        352, 146,  50,  52, 336, 198,  1, -1 },
    { H_BARCA,      ROOM_GROTTA, "barca",            402, 152, 106,  64, 390, 212,  1, -1 },
    { H_CASSETTA,   ROOM_GROTTA, "cassetta",          66, 180,  32,  32, 112, 214, -1, -1 },
    { H_INGRESSO,   ROOM_GROTTA, "ingresso della grotta", 130, 45, 200, 115, 230, 172, 1, -1 },
    { H_RUSCELLO,   ROOM_GROTTA, "acqua",            228, 204, 104,  36,  -1,   0,  0, -1 },
    { H_ROCCE,      ROOM_GROTTA, "rocce",              0,   0, 512,  45,  -1,   0,  0, -1 },
    { H_SENTIERO_G, ROOM_GROTTA, "sentiero",           0, 176,  28,  64,  44, 226, -1, ROOM_MAP },

    { H_TESCHIO,    ROOM_GROTTA_IN, "teschio",        56, 142,  48,  38, 112, 186, -1, -1 },
    { H_FORZIERE,   ROOM_GROTTA_IN, "forziere",       54,  92,  78,  50, 146, 158, -1, -1 },
    { H_CASCATA,    ROOM_GROTTA_IN, "cascata",       328,   0,  76, 128, 284, 172,  1, -1 },
    { H_POZZA,      ROOM_GROTTA_IN, "acqua",         300, 128, 212, 112, 290, 182,  1, -1 },
    { H_STALATTITI, ROOM_GROTTA_IN, "stalattiti",      0,   0, 328,  60,  -1,   0,  0, -1 },
    { H_USCITA_G,   ROOM_GROTTA_IN, "uscita",          0, 150,  26,  90,  36, 200, -1, ROOM_GROTTA },

    { H_BARCA_N,    ROOM_NAVE,   "barca di Ossobuco",  0, 192, 200,  48,  -1,   0,  0, -1 },
    { H_NAVE_E,     ROOM_NAVE,   "nave fantasma",    100,   8, 250, 170,  -1,   0,  0, -1 },
    { H_LUNA_N,     ROOM_NAVE,   "luna",             410,   4,  64,  56,  -1,   0,  0, -1 },
    { H_MARE_N,     ROOM_NAVE,   "mare",               0, 150, 512,  90,  -1,   0,  0, -1 },

    { H_BRODO,      ROOM_NAVE_IN, "Capitan Brodo",   300,  84,  72,  88, 205, 214,  1, -1 },
    { H_CANDELE,    ROOM_NAVE_IN, "candele",         228, 100,  52,  62, 205, 214,  1, -1 },
    { H_LANTERNA_N, ROOM_NAVE_IN, "lanterna appesa", 198,  18,  40,  70, 180, 206,  1, -1 },
    { H_CATENA,     ROOM_NAVE_IN, "catena",          434,   0,  78,  64, 292, 228,  1, -1 },
    { H_TIMONE,     ROOM_NAVE_IN, "timone",          440,  90,  72, 110, 292, 228,  1, -1 },
    { H_OBLO,       ROOM_NAVE_IN, "finestra",        345,  48, 102,  72,  -1,   0,  0, -1 },
    { H_TAVOLA,     ROOM_NAVE_IN, "tavolo",          190, 160, 166,  46, 205, 214,  1, -1 },
    { H_USCITA_N,   ROOM_NAVE_IN, "scaletta",          0,  60,  40, 150,  48, 222, -1, -1 },
};
const int hotspot_count = sizeof(hotspots) / sizeof(hotspots[0]);

const WalkArea walk_areas[ROOM_COUNT] = {
    /* molo: le assi; a sinistra il bordo scende in diagonale verso il mare */
    { 1, { { 5, {   8, 232, 505, 505,   8 }, { 218, 201, 200, 236, 236 } } } },
    /* taberna: davanti al bancone, con i piedi fuori dall'inquadratura */
    { 1, { { 4, {  40, 230, 230,  40 },      { 279, 279, 283, 283 } } } },
    /* spiaggia: la sabbia, fino al relitto */
    { 1, { { 4, {  40, 455, 455,  40 },      { 198, 198, 236, 236 } } } },
    /* rovine: pontile a "L" (striscia in basso + piattaforma della bitta) */
    { 2, { { 4, {  40, 216, 216,  40 },      { 224, 224, 237, 237 } },
           { 4, { 200, 300, 300, 200 },      { 206, 206, 237, 237 } } } },
    /* faro: il prato in basso e il sentiero che sale alla scalinata */
    { 2, { { 4, {  30, 345, 345,  30 },      { 222, 205, 238, 238 } },
           { 4, { 300, 376, 398, 350 },      { 206, 177, 181, 226 } } } },
    /* dentro il faro: il pavimento tra la scala, la lente e il tavolo */
    { 1, { { 4, {  40, 480, 480,  40 },      { 178, 168, 184, 186 } } } },
    /* grotta: la spiaggia davanti all'ingresso, fino allo scheletro */
    { 1, { { 5, {  40, 340, 395, 395,  40 }, { 175, 162, 195, 238, 238 } } } },
    /* dentro la grotta: la sponda rocciosa (l'acqua e' brodo bollente) */
    { 1, { { 4, {  30, 290, 300,  30 },      { 150, 145, 200, 205 } } } },
    /* la barca davanti alla nave: il protagonista e' dipinto nella scena */
    { 0 },
    /* cabina della nave: il pavimento davanti al tavolo */
    { 1, { { 5, {  40, 185, 300, 300,  40 }, { 190, 190, 215, 238, 238 } } } },
};

/* prospettiva: { y lontano, altezza lontano, y vicino, altezza vicino } */
const RoomDepth room_depth[ROOM_COUNT] = {
    /* molo: Barbapollo (basso e tarchiato) e' alto 93 px con i piedi a y 233 */
    { 200,  96, 236, 108, 0 },
    /* taberna: inquadratura a mezzo busto come nel mockup, in scala con
       l'oste: il protagonista e' alto 172 px e ha i piedi sotto lo schermo */
    { 279, 172, 283, 172, 0 },
    /* spiaggia: in lontananza, scala del relitto e della cassa */
    { 200,  90, 236, 100, 0 },
    /* notte: scala del pontile e del Custode; luce lunare */
    { 206,  96, 236, 108, 1 },
    /* faro: inquadratura larga, il protagonista e' piccolo */
    { 177,  72, 238,  88, 0 },
    { 168,  76, 186,  76, 0 },
    /* grotta */
    { 162,  88, 238, 106, 0 },
    { 145,  96, 205, 104, 0 },
    /* in barca: niente sprite */
    {   0,   0,   0,   0, 0 },
    /* cabina di Brodo, a lume di candela */
    { 190, 100, 238, 108, 0 },
};

/* Animazioni degli sfondi (vedi anim.h). x0 e x1 multipli di 8.
   Regola: al massimo due effetti continui per scena, cosi' l'occhio non si
   disperde e il 68000 respira. Quando un personaggio parla si muove solo la
   sua bocca (AN_TALK): pochi pixel, non un riquadro. */
const Anim anims[] = {
    /* scena          tipo        x0   y0   x1   y1   p1          p2   p3 */
    /* titolo: riflessi del mare ai lati del menu, un gabbiano ogni tanto */
    { SCENE_TITLE,  AN_WAVE,      0, 250, 184, 342, 1,          12,  4 },
    { SCENE_TITLE,  AN_WAVE,    328, 250, 512, 342, 1,          12,  4 },
    { SCENE_TITLE,  AN_FLYER,     0, 158, 512, 196, 2,           0,  0 },

    /* molo: il mare; le bocche dei pirati */
    { ROOM_DOCK,    AN_WAVE,    120, 128, 232, 182, 1,          10,  4 },
    { ROOM_DOCK,    AN_TALK,    446, 175, 455, 178, SP_BARBA + TALK_LIGHT, 446, 454 },
    { ROOM_DOCK,    AN_TALK,    345, 153, 353, 156, SP_MOZZO,  345, 352 },

    /* taberna: la lanterna; la bocca dell'oste tra baffi e barba */
    { ROOM_TAVERN,  AN_FLICKER, 168,  96, 232, 168, 0,           0,  0 },
    { ROOM_TAVERN,  AN_TALK,    343, 158, 355, 161, SP_OSTE,   343, 354 },

    /* spiaggia: il mare e un gabbiano di passaggio */
    { ROOM_BEACH,   AN_WAVE,    336, 122, 512, 164, 1,          12,  4 },
    { ROOM_BEACH,   AN_FLYER,   152,  12, 512,  60, 2,           0,  0 },

    /* rovine: le stelle; il becco del Custode si apre quando parla */
    { ROOM_NIGHT,   AN_TWINKLE,   0,   0, 344, 112, 0,           0,  0 },
    { ROOM_NIGHT,   AN_TALK,    338, 122, 355, 125, SP_CUSTODE, 338, 354 },

    /* faro: il mare a sinistra e i gabbiani */
    { ROOM_FARO,    AN_WAVE,      0, 132, 120, 198, 1,          10,  4 },
    { ROOM_FARO,    AN_FLYER,     0,  14, 512,  70, 2,           0,  0 },
    /* dentro il faro: la lente gira, ma solo quando l'hai riparata */
    { ROOM_FARO_IN, AN_FLICKER, 264,   0, 376, 112, 1,           1,  3, F_LIGHT_ON + 1 },
    /* grotta: il ruscello e la mandibola di Ossobuco */
    { ROOM_GROTTA,  AN_WAVE,    232, 206, 328, 240, 1,          10,  4 },
    { ROOM_GROTTA,  AN_TALK,    368, 160, 384, 163, SP_OSSOBUCO, 374, 380 },
    /* dentro la grotta: la cascata di brodo e la pozza */
    { ROOM_GROTTA_IN, AN_FALL,  336,   0, 392, 124, 2,           0,  2 },
    { ROOM_GROTTA_IN, AN_WAVE,  304, 132, 512, 200, 1,          12,  4 },
    /* la nave fantasma tremola sul mare */
    { ROOM_NAVE,    AN_WAVE,    152,  20, 344, 150, 1,          24,  6 },
    { ROOM_NAVE,    AN_WAVE,    208, 160, 512, 240, 1,          10,  4 },
    /* cabina: le candele; la mandibola di Brodo */
    { ROOM_NAVE_IN, AN_FLICKER, 224,  96, 288, 168, 2,           0,  0 },
    { ROOM_NAVE_IN, AN_TALK,    320, 125, 336, 128, SP_BRODO,  322, 329 },
};
const int anim_count = sizeof(anims) / sizeof(anims[0]);

/* luoghi della mappa (coordinate dentro l'immagine della mappa) */
const MapPlace map_places[] = {
    { "Il Faro",           268,   0,  84,  32, -1 },
    { "Puerto Polli (il molo)", 196, 72, 108, 90, ROOM_DOCK },
    { "La Taberna",         98, 116,  84,  40, ROOM_TAVERN },
    { "Cala dei Gabbiani",  44, 204, 144,  34, ROOM_BEACH },
    { "Le Rovine",         300, 196, 104,  50, ROOM_NIGHT },
    { "La Grotta del Brodo", 416, 84, 92,  40, ROOM_GROTTA },
};
const int map_place_count = sizeof(map_places) / sizeof(map_places[0]);

int hotspot_visible(int id)
{
    switch (id) {
    case H_PIUMA:    return flag(F_GULL_FED) && !flag(F_FEATHER_TAKEN);
    case H_CAPPELLO: return !flag(F_HAT_TAKEN);
    case H_LANTERNA_F: return !flag(F_LANTERN_TAKEN);
    case H_CARTA:    return !flag(F_CHART_TAKEN);
    case H_TESCHIO:  return !flag(F_SKULL_TAKEN);
    default:         return 1;
    }
}

const char *object_name(int obj)
{
    int i;
    if (IS_ITEM(obj)) return item_names[obj];
    if (obj == H_CUSTODE && flag(F_MET_CUSTODE)) return "Custode";
    if (obj == H_OSSOBUCO && flag(F_MET_OSSO)) return "Ossobuco";
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
    case ROOM_FARO:   return "Il Faro";
    case ROOM_FARO_IN: return "Dentro il Faro";
    case ROOM_GROTTA: return "La Grotta del Brodo";
    case ROOM_GROTTA_IN: return "Dentro la Grotta";
    case ROOM_NAVE:   return "La nave fantasma";
    case ROOM_NAVE_IN: return "La cabina di Brodo";
    default:          return "Le Rovine";
    }
}

int room_background(int room)
{
    switch (room) {
    case ROOM_DOCK:   return BG_DOCK;
    case ROOM_TAVERN: return BG_TAVERN;
    case ROOM_BEACH:  return BG_BEACH;
    case ROOM_FARO:   return BG_FARO;
    case ROOM_FARO_IN: return BG_FARO_IN;
    case ROOM_GROTTA: return BG_GROTTA;
    case ROOM_GROTTA_IN: return BG_GROTTA_IN;
    case ROOM_NAVE:   return BG_NAVE;
    case ROOM_NAVE_IN: return BG_NAVE_IN;
    default:          return BG_NIGHT;
    }
}

void script_draw_props(int room)
{
    if (room == ROOM_DOCK && hotspot_visible(H_PIUMA))
        draw_prop(IT_PIUMA, 38, 200);
    if (room == ROOM_BEACH && hotspot_visible(H_CAPPELLO))
        draw_prop(IT_CAPPELLO, 371, 176);
    /* nelle scene nuove gli oggetti sono dipinti: si ridisegnano finche'
       non si raccolgono (lo sfondo li ha gia' cancellati) */
    if (room == ROOM_FARO_IN && hotspot_visible(H_LANTERNA_F)) draw_patch(PATCH_LANTERNA);
    if (room == ROOM_FARO_IN && hotspot_visible(H_CARTA))      draw_patch(PATCH_CARTA);
    if (room == ROOM_GROTTA_IN && hotspot_visible(H_TESCHIO))  draw_patch(PATCH_TESCHIO);
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
    if (room == ROOM_FARO && !flag(F_SEEN_FARO)) {
        set_flag(F_SEEN_FARO);
        say(SP_NARRATORE, "Il faro svetta sulla scogliera, come un occhio che non dorme mai.");
        say(SP_HERO, "Un occhio che non dorme, ma nemmeno si guarda intorno: la luce è ferma.");
    }
    if (room == ROOM_FARO_IN && !flag(F_SEEN_FARO_IN)) {
        set_flag(F_SEEN_FARO_IN);
        say(SP_HERO, "Che panorama! E che polvere. Il guardiano non fa le pulizie da vent'anni.");
    }
    if (room == ROOM_GROTTA && !flag(F_SEEN_GROTTA)) {
        set_flag(F_SEEN_GROTTA);
        say(SP_NARRATORE, "La grotta si apre come un respiro della terra. "
                          "L'acqua, all'interno, ha un colore innaturale.");
        say(SP_HERO, "E un odore... di brodo di pollo.");
    }
    if (room == ROOM_GROTTA_IN && !flag(F_SEEN_GROTTA_IN)) {
        set_flag(F_SEEN_GROTTA_IN);
        say(SP_NARRATORE, "L'acqua è così calma da sembrare finta. Eppure, qualcosa si muove sotto la superficie...");
        say(SP_HERO, "...un pezzo di carota. È brodo. Un lago di brodo.");
    }
    if (room == ROOM_NAVE && !flag(F_SEEN_NAVE)) {
        set_flag(F_SEEN_NAVE);
        say(SP_NARRATORE, "La nave fantasma appare e scompare tra le onde, come un sogno che non vuole finire.");
        say(SP_OSSOBUCO, "Ecco la Minestrina. Io aspetto qui. Se torni.");
    }
    if (room == ROOM_NAVE_IN && !flag(F_MET_BRODO)) {
        say(SP_NARRATORE, "Il capitano non è più qui, ma la sua presenza si sente ancora. "
                          "E forse non è solo un ricordo.");
    }
}

/* ------------------------------------------------------------------ */
/* Cartelli tra una parte e l'altra, e il finale                       */
/* ------------------------------------------------------------------ */

const char *chapter_text(int part, int line)
{
    static const char *const t[3][3] = {
        { "~  F I N E  ~", "Capitano della Minestrina e Pirata di Puerto Polli!",
          "Grazie per aver giocato." },
        { "~  P A R T E   I I  ~", "La luce del Faro",
          "Un pirata senza nave è solo un tizio col cappello." },
        { "~  P A R T E   I I I  ~", "Il vascello di Capitan Brodo",
          "La nave fantasma si nasconde in una grotta a est." },
    };
    return t[part == 2 ? 1 : part == 3 ? 2 : 0][line];
}

int chapter_room(int part)
{
    return part == 2 ? ROOM_NIGHT : ROOM_FARO_IN;
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
        say(SP_CUSTODE, "Prima di tutto ti serve una nave. Ma da vent'anni nessuna nave "
                        "attracca a Puerto Polli.");
        say(SP_CUSTODE, "Il faro è acceso, ma non gira più: dal mare sembra una stella "
                        "qualunque e le navi tirano dritto.");
        say(SP_CUSTODE, "Fallo girare di nuovo, pirata, e le navi torneranno. "
                        "E ricorda: mai fidarsi di un pollo. Tranne me. Forse.");
        set_flag(F_PART2);
        queue_call(CALL_PART2);
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
/* Azioni speciali (usate dalla tabella delle regole)                  */
/* ------------------------------------------------------------------ */

static int open_crate(int o1, int o2)
{
    (void)o1; (void)o2;
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

static int fn_give_custode(int o1, int it) { (void)o1; return give_to_custode(it); }

static int fn_mozzo_talk(int o1, int o2)
{
    (void)o1; (void)o2;
    if (flag(F_PART3)) {
        say(SP_MOZZO, "Hai visto la nave fantasma? Il capitano dice che è di Capitan Brodo.");
        say(SP_MOZZO, "Se diventi capitano di una nave vera, mi prendi come nostromo? Ho già il cappello!");
        return 1;
    }
    if (flag(F_PART2) && !flag(F_FARO_KEY)) {
        say(SP_MOZZO, "Al faro c'è un gabbiano che ruba tutto quello che luccica. Una volta mi ha preso un bottone.");
        say(SP_MOZZO, "Il capitano invece ha una bussola che luccica come il sole. Non la presta a nessuno. "
                      "Ai pirati veri, forse.");
        return 1;
    }
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

static int fn_open_map(int o1, int o2)   { (void)o1; (void)o2; open_map(); return 1; }
static int fn_to_tavern(int o1, int o2)  { (void)o1; (void)o2; go_room(ROOM_TAVERN); return 1; }
static int fn_to_dock(int o1, int o2)    { (void)o1; (void)o2; go_room(ROOM_DOCK); return 1; }
static int fn_talk_oste(int o1, int o2)  { (void)o1; (void)o2; start_dialog(SP_OSTE); return 1; }
static int fn_talk_barba(int o1, int o2) { (void)o1; (void)o2; start_dialog(SP_BARBA); return 1; }
static int fn_talk_custode(int o1, int o2) { (void)o1; (void)o2; start_dialog(SP_CUSTODE); return 1; }
static int fn_talk_osso(int o1, int o2)  { (void)o1; (void)o2; start_dialog(SP_OSSOBUCO); return 1; }
static int fn_talk_brodo(int o1, int o2) { (void)o1; (void)o2; start_dialog(SP_BRODO); return 1; }

/* ---- Parte II ---- */

static int fn_faro_door(int o1, int o2)
{
    (void)o1; (void)o2;
    if (flag(F_FARO_OPEN)) {
        go_room(ROOM_FARO_IN);
        return 1;
    }
    if (!has_item(IT_CHIAVE) || !flag(F_FARO_KEY)) {
        say(SP_HERO, "È chiusa a chiave. Il biglietto dice: «Torno subito. Il guardiano.»");
        say(SP_HERO, "La carta è gialla come un pulcino. Quel «subito» ha vent'anni.");
        return 1;
    }
    set_flag(F_FARO_OPEN);
    remove_item(IT_CHIAVE);
    say(SP_HERO, "Clic! La chiave del gabbiano funziona. Entro.");
    go_room(ROOM_FARO_IN);
    return 1;
}

static int fn_push_lens(int o1, int o2)
{
    (void)o1; (void)o2;
    if (flag(F_LIGHT_ON)) {
        say(SP_HERO, "Gira già benissimo da sola.");
        return 1;
    }
    if (!flag(F_GEARS_OILED)) {
        say(SP_HERO, "Spingo, spingo... niente. Il meccanismo è bloccato dalla ruggine.");
        return 1;
    }
    set_flag(F_LIGHT_ON);
    set_flag(F_PART3);
    refresh_room();                       /* parte l'animazione della lente */
    say(SP_HERO, "Spingo con tutte le forze... e la lente gira! Lenta, ma continua.");
    say(SP_NARRATORE, "Il fascio di luce spazza il mare. Lontano, verso est, illumina una "
                      "nave pallida come un lenzuolo.");
    say(SP_NARRATORE, "La nave fantasma scivola verso una grotta sulla costa est... e sparisce.");
    say(SP_HERO, "L'oste mi aveva detto di non nominare mai il brodo. "
                 "Ho la brutta sensazione che c'entri qualcosa.");
    queue_call(CALL_PART3);
    return 1;
}

static int fn_read_chart(int o1, int o2)
{
    (void)o1; (void)o2;
    set_flag(F_KNOWS_GROTTA);
    say(SP_HERO, "La carta nautica del guardiano. Sulla costa est c'è una grotta segnata "
                 "con una X: «Grotta del Brodo. NON ENTRARE».");
    say(SP_HERO, flag(F_PART3) ? "Ecco dove si nasconde la nave fantasma! Ora so come arrivarci: "
                                 "è segnata anche sulla mia mappa."
                               : "Il guardiano aveva una calligrafia molto nervosa.");
    return 1;
}

/* ---- Parte III ---- */

static int fn_enter_cave(int o1, int o2)
{
    (void)o1; (void)o2;
    if (!has_item(IT_LANTERNA)) {
        say(SP_HERO, "È buio pesto. Senza una luce mi romperei il naso su una stalattite.");
        return 1;
    }
    say(SP_HERO, "Accendo la lanterna ed entro.");
    go_room(ROOM_GROTTA_IN);
    return 1;
}

static int fn_ferry(int o1, int o2)
{
    (void)o1; (void)o2;
    if (!flag(F_FERRY)) {
        set_flag(F_KNOWS_TOLL);
        say(SP_OSSOBUCO, "Ehi! La barca è mia. Prima si paga il pedaggio: un teschio.");
        return 1;
    }
    say(SP_OSSOBUCO, "Si parte! Tieni le mani dentro la barca: i pesci, qui, sono speziati.");
    queue_call(CALL_FERRY);
    return 1;
}

static int fn_back_to_cave(int o1, int o2)
{
    (void)o1; (void)o2;
    say(SP_OSSOBUCO, "Si torna a riva? Saggio. Molto saggio.");
    go_room(ROOM_GROTTA);
    return 1;
}

static int fn_board(int o1, int o2)
{
    (void)o1; (void)o2;
    say(SP_HERO, "Salgo a bordo. Silenzio... e odore di dado.");
    go_room(ROOM_NAVE_IN);
    return 1;
}

static int fn_leave_ship(int o1, int o2)
{
    (void)o1; (void)o2;
    go_room(ROOM_NAVE);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Le regole: chi fa cosa con che cosa                                  */
/* ------------------------------------------------------------------ */
/*
 * Ogni regola dice: con questi verbi, su quest'oggetto (e con questo
 * secondo oggetto), se valgono queste condizioni, si dicono queste battute
 * e si applicano questi effetti. Vince la PRIMA regola che corrisponde,
 * quindi le varianti con condizioni vanno prima di quella generica.
 * Per i casi complessi c'e' una funzione (fn) che fa tutto da sola.
 *
 * Convenzioni di script_action (vedi engine.h):
 *   V_DAI: o1 = chi riceve, o2 = oggetto dato;  V_USA: o1 = oggetto, o2 = bersaglio.
 */
typedef struct { signed char sp; const char *text; } Line;
typedef struct {
    short verbs;                 /* maschera di verbi: VB(V_GUARDA) | ... */
    short o1, o2;                /* o2: NONE, ANY_ITEM o un oggetto preciso */
    short c1, c2;                /* condizioni: IF, IFNOT, HAS, HASNOT (0 = nessuna) */
    Line lines[4];
    short set;                   /* SET(flag) */
    short add, rem;              /* ADD(oggetto), REM(oggetto) */
    int (*fn)(int o1, int o2);
} Rule;

#define VB(v)       (1 << (v))
#define NONE        OBJ_NONE
#define ANY_ITEM    (-3)
#define IF(f)       ((f) + 1)
#define IFNOT(f)    (-((f) + 1))
#define HAS(it)     (1000 + (it) + 1)
#define HASNOT(it)  (-(1000 + (it) + 1))
#define SET(f)      ((f) + 1)
#define ADD(it)     ((it) + 1)
#define REM(it)     ((it) + 1)
#define TU          SP_HERO
#define NARR        SP_NARRATORE
#define TOCCA       (VB(V_RACCOGLI) | VB(V_SPINGI) | VB(V_TIRA))
#define SPOSTA      (VB(V_SPINGI) | VB(V_TIRA))

static const Rule rules[] = {
    /* ---- oggetti dell'inventario ---- */
    { VB(V_GUARDA), IT_BOTTIGLIA, NONE, 0, 0, { {TU, "Una bottiglia di grog. Fuma. Le bottiglie non dovrebbero fumare."} } },
    { VB(V_GUARDA), IT_MONETA,    NONE, 0, 0, { {TU, "Un doblone. Tutti i miei risparmi."} } },
    { VB(V_GUARDA), IT_CHIAVE,    NONE, IF(F_FARO_KEY), 0, { {TU, "La chiave del faro. Ha ancora una piuma di gabbiano attaccata."} } },
    { VB(V_GUARDA), IT_CHIAVE,    NONE, 0, 0, { {TU, "Una chiave arrugginita che odora di aringa."} } },
    { VB(V_GUARDA), IT_BUSSOLA,   NONE, 0, 0, { {TU, "Una bussola d'ottone, lucida come uno specchio. L'ago punta sempre verso la Taberna."} } },
    { VB(V_USA),    IT_BUSSOLA,   NONE, 0, 0, { {TU, "Punta verso la Taberna. Come sempre. Forse è rotta, forse è saggia."} } },
    { VB(V_GUARDA), IT_LANTERNA,  NONE, 0, 0, { {TU, "Una lanterna a olio del faro. Fa luce e un po' di fumo."} } },
    { VB(V_GUARDA) | VB(V_USA) | VB(V_APRI), IT_CARTA, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_read_chart },
    { VB(V_GUARDA), IT_TESCHIO,   NONE, 0, 0, { {TU, "Un teschio sorridente. Gli ho dato un nome: Gennaro."} } },
    { VB(V_USA),    IT_TESCHIO,   NONE, 0, 0, { {TU, "Essere o non essere... No, troppo teatrale."} } },
    { VB(V_USA), IT_CHIAVE,    H_PORTA_F, 0, 0, { {0, 0} }, 0, 0, 0, fn_faro_door },
    { VB(V_USA), IT_LANTERNA,  H_MECCANISMO, IF(F_GEARS_OILED), 0, { {TU, "Ho già oliato tutto. Brillano come i denti dell'oste. Cioè, no: molto di più."} } },
    { VB(V_USA), IT_LANTERNA,  H_MECCANISMO, 0, 0, {
        {TU, "Verso un po' d'olio della lanterna negli ingranaggi. Glu glu."},
        {TU, "Ne resta abbastanza per far luce. Ottimo, non si sa mai."} }, SET(F_GEARS_OILED) },
    { VB(V_USA), IT_LANTERNA,  H_LENTE, 0, 0, { {TU, "La lente ha già la sua luce. Il problema è che non gira."} } },
    { VB(V_USA), IT_LANTERNA,  H_INGRESSO, 0, 0, { {0, 0} }, 0, 0, 0, fn_enter_cave },
    { VB(V_USA), IT_TESCHIO,   H_BARCA, 0, 0, { {SP_OSSOBUCO, "Il pedaggio si dà a me, non alla barca. La barca non colleziona niente."} } },
    { VB(V_GUARDA), IT_MAPPA,     NONE, 0, 0, { {TU, "La mappa di Puerto Polli. Se la uso posso spostarmi sull'isola."} } },
    { VB(V_GUARDA), IT_PIUMA,     NONE, 0, 0, { {TU, "Una piuma di gabbiano. Decisamente NON di pollo."} } },
    { VB(V_GUARDA), IT_CALAMARO,  NONE, 0, 0, { {TU, "Un calamaro essiccato. Il portafortuna del capitano. Puzza di fortuna."} } },
    { VB(V_GUARDA), IT_CAPPELLO,  NONE, IF(F_HAT_WORN), 0, { {TU, "Il mio cappello da pirata. Ce l'ho in testa e mi sta benissimo."} } },
    { VB(V_GUARDA), IT_CAPPELLO,  NONE, 0, 0, { {TU, "Un cappello da pirata. Starebbe meglio sulla mia testa."} } },
    { VB(V_GUARDA), IT_LIBRO,     NONE, 0, 0, { {TU, "\"Il Codice dei Pirati\". Articolo 1: mai fidarsi di un pollo."},
                                                {TU, "Articolo 2: vedi articolo 1."} } },
    { VB(V_USA), IT_MAPPA,     NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_open_map },
    { VB(V_USA), IT_CAPPELLO,  NONE, IF(F_HAT_WORN), 0, { {TU, "Ce l'ho già in testa. Toglierlo? Mai più."} } },
    { VB(V_USA), IT_CAPPELLO,  NONE, 0, 0, { {TU, "Me lo metto in testa. Mi sento già più pirata. E un po' umido."} }, SET(F_HAT_WORN) },
    { VB(V_USA), IT_BOTTIGLIA, NONE, 0, 0, { {TU, "Berlo io? Fuma! Neanche morto. Beh, forse da morto."} } },
    { VB(V_USA), IT_LIBRO,     NONE, 0, 0, { {TU, "Leggo a caso: \"Articolo 14: il grog va bevuto, non annusato.\" Profondo."} } },
    { VB(V_USA), IT_PIUMA,     NONE, 0, 0, { {TU, "Mi faccio il solletico. Ih ih. Ok, basta."} } },
    { VB(V_USA), IT_CALAMARO,  NONE, 0, 0, { {TU, "Non ho COSÌ fame."} } },
    { VB(V_USA), IT_MONETA,    NONE, 0, 0, { {TU, "Potrei spenderla. Alla Taberna, per esempio."} } },
    { VB(V_APRI), IT_LIBRO,    NONE, 0, 0, { {TU, "\"Articolo 7: un vero pirata ha cappello, piuma e questo libro.\" Interessante..."} } },
    { VB(V_USA), IT_CHIAVE,    H_CASSA, 0, 0, { {0, 0} }, 0, 0, 0, open_crate },

    /* ---- molo ---- */
    { VB(V_GUARDA), H_TABERNA, NONE, 0, 0, { {TU, "La Taberna. Il posto più malfamato dell'isola. E anche l'unico."} } },
    { VB(V_APRI) | VB(V_VAI), H_TABERNA, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_to_tavern },
    { VB(V_CHIUDI), H_TABERNA, NONE, 0, 0, { {TU, "È già chiusa. Più o meno."} } },
    { VB(V_GUARDA), H_INSEGNA, NONE, 0, 0, { {TU, "\"TABERNA\". Qualcuno ha cancellato una V. O aggiunto una B."},
                                             {TU, "I filologi dell'isola ne discutono ancora."} } },

    { VB(V_GUARDA), H_BARBA, NONE, IFNOT(F_GOT_KEY), 0, { {TU, "Un pirata enorme, con benda, barba e un tricorno che ha visto giorni migliori."},
                                                          {TU, "Ha l'aria assetata."} } },
    { VB(V_GUARDA), H_BARBA, NONE, 0, 0, { {TU, "Un pirata enorme, con benda, barba e un tricorno che ha visto giorni migliori."} } },
    { VB(V_PARLA),  H_BARBA, NONE, IFNOT(F_HAT_WORN), HAS(IT_CAPPELLO), {
        {SP_BARBA, "Bah! Non parlo con chi va in giro senza cappello."},
        {SP_BARBA, "Senza cappello sei un turista. E i turisti si buttano a mare."},
        {TU, "(Forse dovrei mettermi il cappello che ho in tasca.)"} } },
    { VB(V_PARLA),  H_BARBA, NONE, IFNOT(F_HAT_WORN), 0, {
        {SP_BARBA, "Bah! Non parlo con chi va in giro senza cappello."},
        {SP_BARBA, "Senza cappello sei un turista. E i turisti si buttano a mare."} } },
    { VB(V_PARLA),  H_BARBA, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_talk_barba },
    { VB(V_DAI), H_BARBA, ANY_ITEM, IFNOT(F_HAT_WORN), 0, { {SP_BARBA, "Prima mettiti un cappello. Non faccio affari con i turisti."} } },
    { VB(V_DAI), H_BARBA, IT_BOTTIGLIA, 0, 0, {
        {SP_BARBA, "Il mio grog! Fumante come piace a me!"},
        {NARR, "Il capitano lo beve tutto d'un fiato. Gli fumano anche le orecchie."},
        {SP_BARBA, "Affare fatto, mozzo. Ecco la chiave della cassa sulla spiaggia."},
        {SP_BARBA, "Non chiedermi perché odora di aringa."} }, SET(F_GOT_KEY), ADD(IT_CHIAVE), REM(IT_BOTTIGLIA) },
    { VB(V_DAI), H_BARBA, IT_MONETA, 0, 0, { {SP_BARBA, "Una moneta? Cosa me ne faccio? Portami del grog, piuttosto!"} } },
    { VB(V_DAI), H_BARBA, ANY_ITEM, 0, 0, { {SP_BARBA, "Non mi serve. A me serve qualcosa che fumi e che si beva."} } },
    { TOCCA, H_BARBA, NONE, 0, 0, { {SP_BARBA, "Mettimi le mani addosso e ti uso come àncora."} } },

    { VB(V_GUARDA), H_MOZZO, NONE, 0, 0, { {TU, "Un pirata piccolo ma determinato. Ha un cappello più grande delle sue ambizioni."} } },
    { VB(V_PARLA),  H_MOZZO, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_mozzo_talk },
    { VB(V_DAI),    H_MOZZO, ANY_ITEM, 0, 0, { {SP_MOZZO, "Per me? No, grazie. Il capitano dice di non accettare regali dagli sconosciuti."} } },

    { VB(V_GUARDA),   H_GABBIANO, NONE, 0, 0, { {TU, "Un gabbiano. Mi fissa con l'aria di chi sa qualcosa che io non so."} } },
    { VB(V_RACCOGLI), H_GABBIANO, NONE, 0, 0, { {TU, "Ci ho provato. Mi ha beccato. Due volte."} } },
    { VB(V_PARLA),    H_GABBIANO, NONE, 0, 0, { {TU, "Ciao, gabbiano."}, {SP_GABBIANO, "Squaaak."}, {TU, "Credo fosse un \"no\"."} } },
    { VB(V_DAI), H_GABBIANO, IT_CALAMARO, 0, 0, {
        {NARR, "Il gabbiano inghiotte il calamaro intero, senza masticare."},
        {NARR, "Poi, per gratitudine (o per digestione), lascia cadere una piuma sul molo."},
        {TU, "Grazie... credo."} }, SET(F_GULL_FED), 0, REM(IT_CALAMARO) },
    { VB(V_DAI), H_GABBIANO, ANY_ITEM, 0, 0, { {TU, "Il gabbiano lo guarda con disprezzo. È un gabbiano esigente."} } },

    { VB(V_GUARDA),   H_PIUMA, NONE, 0, 0, { {TU, "Una piuma di gabbiano, ancora tiepida. Bleah."} } },
    { VB(V_RACCOGLI), H_PIUMA, NONE, 0, 0, { {TU, "Presa! Una piuma di un uccello che NON è un pollo."} }, SET(F_FEATHER_TAKEN), ADD(IT_PIUMA) },

    { VB(V_GUARDA),   H_BANCARELLA, NONE, 0, 0, { {TU, "Vende souvenir: calamite a forma di pollo, cartoline di polli e... polli."} } },
    { VB(V_RACCOGLI), H_BANCARELLA, NONE, 0, 0, { {TU, "Ho già abbastanza polli nella mia vita."} } },
    { VB(V_PARLA),    H_BANCARELLA, NONE, 0, 0, { {TU, "Non c'è nessuno. Il venditore sarà andato a comprare altri polli."} } },
    { VB(V_GUARDA),   H_VELIERO, NONE, 0, 0, { {TU, "Un bel veliero. Non è mio. Niente qui è mio, a parte i debiti."} } },
    { VB(V_GUARDA),   H_PAESE, NONE, 0, 0, { {TU, "Puerto Polli: case bianche e tetti rossi. Credo. Qui è tutto in bianco e nero."} } },

    /* ---- taberna ---- */
    { VB(V_GUARDA), H_OSTE, NONE, 0, 0, { {TU, "L'oste. Un occhio solo, ma ti tiene d'occhio lo stesso."} } },
    { VB(V_PARLA),  H_OSTE, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_talk_oste },
    { VB(V_DAI), H_OSTE, IT_MONETA, IF(F_GOT_GROG), 0, { {SP_OSTE, "Hai già avuto il tuo grog. Due grog per un novellino sono un rischio sanitario."} } },
    { VB(V_DAI), H_OSTE, IT_MONETA, 0, 0, {
        {SP_OSTE, "Una moneta? Affare fatto! Ecco a te una bottiglia di grog."},
        {TU, "Ehm... perché la bottiglia fuma?"},
        {SP_OSTE, "È il grog. Fa così. Non berlo vicino alle candele."} }, SET(F_GOT_GROG), ADD(IT_BOTTIGLIA), REM(IT_MONETA) },
    { VB(V_DAI), H_OSTE, ANY_ITEM, 0, 0, { {SP_OSTE, "Niente baratti. Accetto solo monete. E qualche volta galline."} } },
    { VB(V_GUARDA), H_BANDIERA, NONE, 0, 0, { {TU, "Un teschio con due ossa incrociate. O due cosce di pollo? Con questa luce non si capisce."} } },
    { VB(V_RACCOGLI) | VB(V_TIRA), H_BANDIERA, NONE, 0, 0, { {SP_OSTE, "Giù le mani dalla bandiera! È un cimelio di famiglia."} } },
    { VB(V_GUARDA), H_QUADRO, NONE, 0, 0, { {TU, "Il ritratto di un pollo con la benda sull'occhio. C'è una dedica: \"Al mio oste preferito\"."} } },
    { VB(V_GUARDA),   H_LANTERNA, NONE, 0, 0, { {TU, "Una lanterna. Fa più fumo che luce. Come il grog."} } },
    { VB(V_RACCOGLI), H_LANTERNA, NONE, 0, 0, { {TU, "Scotta. E poi resterebbe al buio l'oste, che già ci vede per metà."} } },
    { VB(V_GUARDA), H_FINESTRA, NONE, 0, 0, { {TU, "Una finestra così sporca che fuori sembra sempre notte."} } },
    { VB(V_APRI),   H_FINESTRA, NONE, 0, 0, { {TU, "È sigillata da anni di grasso di pollo fritto."} } },
    { VB(V_GUARDA),   H_BOTTIGLIE, NONE, 0, 0, { {TU, "Grog, grog d'annata, grog light e un barattolo con scritto \"NON grog\"."} } },
    { VB(V_RACCOGLI), H_BOTTIGLIE, NONE, 0, 0, { {SP_OSTE, "Ehi! Quelle si pagano. Una moneta a bottiglia."} } },
    { VB(V_GUARDA),   H_BOCCALE, NONE, 0, 0, { {TU, "Un boccale vuoto. O quasi. Meglio non indagare."} } },
    { VB(V_RACCOGLI), H_BOCCALE, NONE, 0, 0, { {TU, "È appiccicato al bancone. Non voglio sapere con cosa."} } },
    { VB(V_GUARDA), H_USCITA_T, NONE, 0, 0, { {TU, "L'uscita. Porta al molo."} } },
    { VB(V_VAI) | VB(V_APRI), H_USCITA_T, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_to_dock },

    /* ---- spiaggia ---- */
    { VB(V_GUARDA),   H_CAPPELLO, NONE, 0, 0, { {TU, "Un cappello da pirata! Un po' bagnato, un po' mangiucchiato, ma ha stile."} } },
    { VB(V_RACCOGLI), H_CAPPELLO, NONE, 0, 0, { {TU, "È incastrato tra le assi del relitto. Devo tirarlo con forza."} } },
    { VB(V_TIRA), H_CAPPELLO, NONE, 0, 0, {
        {TU, "Ooooh... issa! Ce l'ho!"},
        {NARR, "Un granchio, che ci abitava dentro, se ne va profondamente offeso."} }, SET(F_HAT_TAKEN), ADD(IT_CAPPELLO) },
    { VB(V_GUARDA), H_CASSA, NONE, IF(F_CRATE_OPEN), 0, { {TU, "La cassa è aperta e vuota. Come il mio portafoglio."} } },
    { VB(V_GUARDA), H_CASSA, NONE, 0, 0, { {TU, "Una vecchia cassa chiusa con un lucchetto. Sopra c'è scritto \"B.P.\""} } },
    { VB(V_APRI),   H_CASSA, NONE, 0, 0, { {0, 0} }, 0, 0, 0, open_crate },
    { VB(V_CHIUDI), H_CASSA, NONE, IF(F_CRATE_OPEN), 0, { {TU, "Meglio lasciarla aperta: non ho più la chiave."} } },
    { VB(V_CHIUDI), H_CASSA, NONE, 0, 0, { {TU, "È già chiusa."} } },
    { SPOSTA, H_CASSA, NONE, 0, 0, { {TU, "Pesa una tonnellata. Sarà piena di piombo. O di zuppa."} } },
    { VB(V_RACCOGLI), H_CASSA, NONE, 0, 0, { {TU, "Troppo pesante. E poi non mi entra in tasca."} } },
    { VB(V_GUARDA), H_RELITTO, NONE, IFNOT(F_HAT_TAKEN), 0, {
        {TU, "Il relitto di una barca. Sulla fiancata c'è scritto \"Pollo Volante\". Non ha volato a lungo."},
        {TU, "C'è un cappello incastrato tra le assi."} } },
    { VB(V_GUARDA), H_RELITTO, NONE, 0, 0, { {TU, "Il relitto di una barca. Sulla fiancata c'è scritto \"Pollo Volante\". Non ha volato a lungo."} } },
    { SPOSTA, H_RELITTO, NONE, 0, 0, { {TU, "Non si muove. È più arenato di me."} } },
    { VB(V_GUARDA), H_FARO, NONE, 0, 0, { {TU, "Il faro. Dicono che sia chiuso per ferie da vent'anni."} } },
    { VB(V_GUARDA), H_SCOGLIERA, NONE, 0, 0, { {TU, "Scogli a strapiombo. Ottimi per meditare, pessimi per cadere."} } },
    { VB(V_GUARDA), H_PALME, NONE, 0, 0, { {TU, "Palme. Tante palme. Nessuna noce di cocco, ovviamente."} } },
    { TOCCA, H_PALME, NONE, 0, 0, { {TU, "Non sono così forte. Non ancora."} } },
    { VB(V_GUARDA), H_MARE, NONE, 0, 0, { {TU, "Il mare. Bagnato, come sempre."} } },

    /* ---- rovine ---- */
    { VB(V_GUARDA), H_CUSTODE, NONE, IF(F_MET_CUSTODE), 0, { {TU, "Il Custode di Puerto Polli. Un pollo con un contratto a tempo indeterminato."} } },
    { VB(V_GUARDA), H_CUSTODE, NONE, 0, 0, { {TU, "Un pollo. Un pollo ENORME, alto quanto me, con l'aria molto seria."} } },
    { VB(V_PARLA),  H_CUSTODE, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_talk_custode },
    { VB(V_DAI), H_CUSTODE, ANY_ITEM, 0, 0, { {0, 0} }, 0, 0, 0, fn_give_custode },
    { VB(V_RACCOGLI), H_CUSTODE, NONE, 0, 0, { {SP_CUSTODE, "Prova a sollevarmi e ti becco fino a Natale."} } },
    { SPOSTA, H_CUSTODE, NONE, 0, 0, { {SP_CUSTODE, "Ehi! Rispetto per le istituzioni!"} } },
    { VB(V_GUARDA), H_LUNA, NONE, 0, 0, { {TU, "La luna piena. Ha la faccia di qualcuno che ha visto troppe storie di pirati."} } },
    { VB(V_GUARDA), H_NAVE_F, NONE, 0, 0, { {TU, "Una nave fantasma! O una nave normale molto pallida."} } },
    { VB(V_GUARDA), H_BITTA, NONE, 0, 0, { {TU, "Una bitta per legare le barche. Qualcuno ci ha inciso: \"W i polli\"."} } },
    { SPOSTA, H_BITTA, NONE, 0, 0, { {TU, "Non si muove. Il molo ringrazia."} } },
    { VB(V_GUARDA), H_PALMA, NONE, 0, 0, { {TU, "Una palma che si sporge sul mare, come se volesse andarsene anche lei."} } },

    /* ---- faro, esterno ---- */
    { VB(V_GUARDA), H_FARO_E, NONE, IF(F_LIGHT_ON), 0, { {TU, "Il faro gira! Ora sì che sembra un faro, e non una stella pigra."} } },
    { VB(V_GUARDA), H_FARO_E, NONE, 0, 0, { {TU, "Il faro. È acceso, ma la luce è ferma: dal mare sembra una stella qualunque."},
                                            {TU, "Nessuna nave lo prende sul serio."} } },
    { VB(V_GUARDA), H_CASA, NONE, 0, 0, { {TU, "La casa del guardiano. Le finestre sono chiuse da vent'anni. Le tende pure."} } },
    { VB(V_GUARDA), H_PORTA_F, NONE, IF(F_FARO_OPEN), 0, { {TU, "La porta del faro. L'ho aperta io, con la chiave del gabbiano."} } },
    { VB(V_GUARDA), H_PORTA_F, NONE, 0, 0, { {TU, "Una porta robusta, chiusa a chiave. C'è un biglietto: «Torno subito. Il guardiano.»"} } },
    { VB(V_APRI) | VB(V_VAI) | VB(V_SPINGI), H_PORTA_F, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_faro_door },
    { VB(V_CHIUDI), H_PORTA_F, NONE, 0, 0, { {TU, "Meglio lasciarla com'è."} } },
    { VB(V_GUARDA), H_GABBIANO_F, NONE, IF(F_FARO_KEY), 0, { {TU, "Il gabbiano si specchia nella bussola. È innamorato. Di sé stesso."} } },
    { VB(V_GUARDA), H_GABBIANO_F, NONE, 0, 0, {
        {TU, "Un gabbiano grasso e solenne. Ha una chiave appesa al collo con uno spago."},
        {TU, "Si dice che i gabbiani vadano matti per le cose che luccicano."} }, SET(F_KNOWS_GULL_KEY) },
    { VB(V_PARLA), H_GABBIANO_F, NONE, IF(F_FARO_KEY), 0, { {TU, "Grazie per la chiave."}, {SP_GABBIANO, "Squaak."} } },
    { VB(V_PARLA), H_GABBIANO_F, NONE, 0, 0, {
        {TU, "Bel gabbiano. Mi presti quella chiave?"},
        {SP_GABBIANO, "Squaaak! Squak squak."},
        {TU, "Credo voglia dire «solo in cambio di qualcosa che luccica»."} }, SET(F_KNOWS_GULL_KEY) },
    { TOCCA, H_GABBIANO_F, NONE, 0, 0, { {TU, "Mi ha beccato il naso. Un altro gabbiano. È una persecuzione."} } },
    { VB(V_DAI), H_GABBIANO_F, IT_BUSSOLA, 0, 0, {
        {NARR, "Il gabbiano fissa la bussola. La bussola fissa il gabbiano."},
        {NARR, "Scambio fatto: il gabbiano si tiene la bussola e ti lancia la chiave."},
        {TU, "Una chiave! Spero sia del faro e non della sua cassetta delle lettere."} }, SET(F_FARO_KEY), ADD(IT_CHIAVE), REM(IT_BUSSOLA) },
    { VB(V_DAI), H_GABBIANO_F, ANY_ITEM, 0, 0, { {TU, "Il gabbiano scuote la testa. Non luccica abbastanza."} } },
    { VB(V_GUARDA), H_PALO, NONE, 0, 0, { {TU, "Il palo preferito del gabbiano. Si vede dalle... decorazioni."} } },
    { VB(V_GUARDA), H_CARTELLO, NONE, 0, 0, { {TU, "«FARO». Utile, per chi non avesse notato l'enorme torre bianca."} } },
    { TOCCA, H_CARTELLO, NONE, 0, 0, { {TU, "È piantato bene. Più del faro, forse."} } },
    { VB(V_GUARDA), H_VELA, NONE, IF(F_LIGHT_ON), 0, { {TU, "Una barca a vela. Ora che il faro gira, forse si fermerà a Puerto Polli."} } },
    { VB(V_GUARDA), H_VELA, NONE, 0, 0, { {TU, "Una barca a vela che passa al largo, senza fermarsi. Come tutte."} } },
    { VB(V_GUARDA), H_SOLE, NONE, 0, 0, { {TU, "Il sole sul mare. Tramonta o sorge? A Puerto Polli non si capisce mai."} } },
    { VB(V_GUARDA), H_MARE_F, NONE, 0, 0, { {TU, "Il mare, giù in fondo. Meglio non sporgersi."} } },
    { VB(V_GUARDA), H_STACCIONATA, NONE, 0, 0, { {TU, "Una staccionata che separa il sentiero dal vuoto. Grazie, staccionata."} } },
    { SPOSTA, H_STACCIONATA, NONE, 0, 0, { {TU, "Toccare la sola cosa che mi separa dal vuoto? No, grazie."} } },
    { VB(V_GUARDA), H_SCALA_F, NONE, 0, 0, { {TU, "Una scalinata scavata nella roccia. Porta alla porta del faro."} } },
    { VB(V_GUARDA), H_SENTIERO_F, NONE, 0, 0, { {TU, "Il sentiero che torna verso il resto dell'isola."} } },

    /* ---- faro, interno ---- */
    { VB(V_GUARDA), H_LENTE, NONE, IF(F_LIGHT_ON), 0, { {TU, "La luce del faro ruota lenta, ma continua. Un segnale che sfida il buio."} } },
    { VB(V_GUARDA), H_LENTE, NONE, 0, 0, { {TU, "Una lente enorme, accesa ma ferma."},
                                           {TU, "Una luce che non gira è solo una lampadina presuntuosa."} } },
    { SPOSTA, H_LENTE, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_push_lens },
    { SPOSTA, H_MECCANISMO, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_push_lens },
    { VB(V_RACCOGLI), H_LENTE, NONE, 0, 0, { {TU, "Pesa quanto una mucca di vetro."} } },
    { VB(V_GUARDA), H_MECCANISMO, NONE, IF(F_GEARS_OILED), 0, { {TU, "Ingranaggi lucidi d'olio. Pronti a girare: basta una bella spinta."} } },
    { VB(V_GUARDA), H_MECCANISMO, NONE, 0, 0, { {TU, "Il meccanismo che fa girare la lente. Gli ingranaggi sono pieni di ruggine."},
                                                {TU, "Cigolano solo a guardarli. Ci vorrebbe dell'olio."} } },
    { VB(V_APRI), H_MECCANISMO, NONE, 0, 0, { {TU, "Gli ingranaggi sono già in vista. Ci vuole olio, non un cacciavite."} } },
    { VB(V_GUARDA), H_LANTERNA_F, NONE, 0, 0, { {TU, "Una lanterna a olio, piena fino all'orlo."} } },
    { VB(V_RACCOGLI), H_LANTERNA_F, NONE, 0, 0, { {TU, "La prendo. La luce non è mai troppa. Nemmeno dentro un faro."} }, SET(F_LANTERN_TAKEN), ADD(IT_LANTERNA) },
    { VB(V_GUARDA), H_CARTA, NONE, 0, 0, { {TU, "Una carta nautica dell'isola, piena di appunti del guardiano."} } },
    { VB(V_RACCOGLI), H_CARTA, NONE, 0, 0, { {TU, "La arrotolo e la metto via. Il guardiano non se ne accorgerà: non c'è da vent'anni."} }, SET(F_CHART_TAKEN), ADD(IT_CARTA) },
    { VB(V_GUARDA), H_LUMINO, NONE, 0, 0, { {TU, "Un lumino da notte. Il guardiano aveva paura del buio. In un faro."} } },
    { VB(V_RACCOGLI), H_LUMINO, NONE, 0, 0, { {TU, "È avvitato al pavimento. Il guardiano ci teneva molto."} } },
    { VB(V_GUARDA), H_TAVOLO, NONE, 0, 0, { {TU, "Il tavolo del guardiano: compassi, righelli e una macchia di caffè a forma di pollo."} } },
    { VB(V_GUARDA), H_SCALA_I, NONE, 0, 0, { {TU, "La scala a chiocciola che scende. Gira più lei del faro."} } },
    { VB(V_GUARDA), H_PORTICINA, NONE, 0, 0, { {TU, "Una porticina. Uno sgabuzzino, a giudicare dall'odore di scopa."} } },
    { VB(V_APRI), H_PORTICINA, NONE, 0, 0, { {TU, "Scope, secchi e un calendario di vent'anni fa. Richiudo prima che mi cada tutto addosso."} } },
    { VB(V_GUARDA), H_FINESTRE, NONE, 0, 0, { {TU, "Da quassù si vede tutto il mare. E tutto il mare vede me."} } },
    { VB(V_APRI), H_FINESTRE, NONE, 0, 0, { {TU, "Con questo vento? Volerei via insieme alla carta nautica."} } },

    /* ---- grotta, esterno ---- */
    { VB(V_GUARDA), H_OSSOBUCO, NONE, 0, 0, { {TU, "Uno scheletro seduto accanto a una barca. Ha l'aria di chi aspetta da molto, molto tempo."} } },
    { VB(V_PARLA),  H_OSSOBUCO, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_talk_osso },
    { VB(V_DAI), H_OSSOBUCO, IT_TESCHIO, 0, 0, {
        {SP_OSSOBUCO, "Un teschio! E che zigomi! Va dritto nella mia collezione."},
        {SP_OSSOBUCO, "Pedaggio pagato. Quando vuoi partire, sali sulla barca."} }, SET(F_FERRY), 0, REM(IT_TESCHIO) },
    { VB(V_DAI), H_OSSOBUCO, ANY_ITEM, 0, 0, { {SP_OSSOBUCO, "Non è un teschio. Accetto solo teschi. È una questione di principio."} } },
    { TOCCA, H_OSSOBUCO, NONE, 0, 0, { {SP_OSSOBUCO, "Piano! Se mi smonti, poi chi mi rimonta?"} } },
    { VB(V_GUARDA), H_BARCA, NONE, 0, 0, { {TU, "La barca di Ossobuco. Sulla prua c'è scritto: «Solo andata»."} } },
    { VB(V_USA) | VB(V_VAI) | VB(V_SPINGI), H_BARCA, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_ferry },
    { VB(V_GUARDA), H_CASSETTA, NONE, 0, 0, { {TU, "Una cassetta di legno con la scritta «Dispensa»."} } },
    { VB(V_APRI), H_CASSETTA, NONE, 0, 0, { {TU, "Solo lische di pesce e una ricevuta: «Capitan Brodo, 1 pollo. Da pagare.»"} } },
    { VB(V_RACCOGLI), H_CASSETTA, NONE, 0, 0, { {TU, "Puzza troppo di lische. Resta qui."} } },
    { VB(V_GUARDA), H_INGRESSO, NONE, 0, 0, { {TU, "L'ingresso della grotta. Dentro è buio come la stiva di un galeone."} } },
    { VB(V_VAI) | VB(V_APRI), H_INGRESSO, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_enter_cave },
    { VB(V_GUARDA), H_RUSCELLO, NONE, 0, 0, { {TU, "Un ruscello caldo che esce dalla grotta. Profuma di sedano."} } },
    { VB(V_RACCOGLI), H_RUSCELLO, NONE, 0, 0, { {TU, "Scotta! E poi non ho una tazza."} } },
    { VB(V_GUARDA), H_ROCCE, NONE, 0, 0, { {TU, "Rocce scure e appuntite. Non è un posto per picnic."} } },
    { VB(V_GUARDA), H_SENTIERO_G, NONE, 0, 0, { {TU, "Il sentiero che torna verso il resto dell'isola."} } },

    /* ---- grotta, interno ---- */
    { VB(V_GUARDA), H_TESCHIO, NONE, 0, 0, { {TU, "Un teschio. Sorride, ma non so di cosa."} } },
    { VB(V_RACCOGLI), H_TESCHIO, NONE, 0, 0, { {TU, "Scusa, amico. Ti porto in un posto migliore: una collezione."} }, SET(F_SKULL_TAKEN), ADD(IT_TESCHIO) },
    { VB(V_GUARDA), H_FORZIERE, NONE, IF(F_CHEST2_OPEN), 0, { {TU, "Il forziere di Capitan Brodo. Dentro c'è solo il quaderno dei proverbi."} } },
    { VB(V_GUARDA), H_FORZIERE, NONE, 0, 0, { {TU, "Un forziere con le iniziali «C.B.». Capitan Brodo. O Cotoletta Bollita."} } },
    { VB(V_APRI), H_FORZIERE, NONE, IF(F_CHEST2_OPEN), 0, { {TU, "I proverbi li so già a memoria. Più o meno."} } },
    { VB(V_APRI), H_FORZIERE, NONE, 0, 0, {
        {TU, "Dentro c'è un quaderno: «I proverbi invincibili di Capitan Brodo»."},
        {TU, "«Chi dorme in coffa... si sveglia in mare.» «Rosso di sera... grog a volontà.»"},
        {TU, "«Can che abbaia... non sale a bordo.» Me li imparo a memoria. Non si sa mai."} }, SET(F_CHEST2_OPEN) },
    { VB(V_CHIUDI), H_FORZIERE, NONE, 0, 0, { {TU, "Lo lascio com'è: Brodo potrebbe notarlo. Ah no, è un fantasma distratto."} } },
    { SPOSTA | VB(V_RACCOGLI), H_FORZIERE, NONE, 0, 0, { {TU, "È incastrato tra le rocce. E pesa come un pentolone."} } },
    { VB(V_GUARDA), H_CASCATA, NONE, 0, 0, { {TU, "Una cascata di brodo caldo. Ecco da dove viene l'odore."} } },
    { VB(V_GUARDA), H_POZZA, NONE, 0, 0, { {TU, "Brodo. Brodo ovunque. Manca solo il pollo... e credo che qualcuno ci stia pensando."} } },
    { VB(V_RACCOGLI), H_POZZA, NONE, 0, 0, { {TU, "Scotta! E poi non ho un cucchiaio abbastanza grande."} } },
    { VB(V_GUARDA), H_STALATTITI, NONE, 0, 0, { {TU, "Stalattiti. O stalagmiti? Quelle che scendono. Credo."} } },
    { VB(V_GUARDA), H_USCITA_G, NONE, 0, 0, { {TU, "L'uscita della grotta."} } },

    /* ---- nave fantasma ---- */
    { VB(V_GUARDA), H_NAVE_E, NONE, 0, 0, { {TU, "La Minestrina, la nave di Capitan Brodo. Trasparente come un brodo ben sgrassato."} } },
    { VB(V_VAI) | VB(V_USA) | VB(V_APRI), H_NAVE_E, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_board },
    { VB(V_GUARDA), H_BARCA_N, NONE, 0, 0, { {TU, "La barca di Ossobuco. Rema senza muscoli: è un mistero della scienza."} } },
    { VB(V_VAI) | VB(V_USA), H_BARCA_N, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_back_to_cave },
    { VB(V_PARLA), H_BARCA_N, NONE, 0, 0, { {TU, "Ossobuco, tutto bene?"}, {SP_OSSOBUCO, "Mai stato più morto."} } },
    { VB(V_GUARDA), H_LUNA_N, NONE, 0, 0, { {TU, "La luna. Anche lei sembra un po' trasparente, stanotte."} } },
    { VB(V_GUARDA), H_MARE_N, NONE, 0, 0, { {TU, "Il mare di notte. Nero e pieno di cose che non voglio conoscere."} } },

    { VB(V_GUARDA), H_BRODO, NONE, 0, 0, { {TU, "Capitan Brodo. Uno scheletro con il tricorno, seduto a tavola da cent'anni."},
                                           {TU, "Aspetta la cena. Ho paura di sapere quale."} } },
    { VB(V_PARLA),  H_BRODO, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_talk_brodo },
    { VB(V_DAI), H_BRODO, ANY_ITEM, 0, 0, { {SP_BRODO, "Non mi serve niente. A parte un pollo. Un pollo grosso. Uno che fa il Custode."} } },
    { TOCCA, H_BRODO, NONE, 0, 0, { {SP_BRODO, "Giù le mani! Un pirata si sfida con la lingua, non con le mani."} } },
    { VB(V_GUARDA), H_CANDELE, NONE, 0, 0, { {TU, "Candele accese. Chi le accende, su una nave di fantasmi?"} } },
    { VB(V_RACCOGLI), H_CANDELE, NONE, 0, 0, { {SP_BRODO, "Lascia stare le mie candele. È una cena romantica: io e la pentola."} } },
    { VB(V_GUARDA), H_LANTERNA_N, NONE, 0, 0, { {TU, "Una lanterna appesa. Dondola anche quando la nave sta ferma."} } },
    { VB(V_GUARDA), H_CATENA, NONE, 0, 0, { {TU, "La catena dell'àncora."} } },
    { SPOSTA, H_CATENA, NONE, 0, 0, { {TU, "È la catena dell'àncora. Prima devo occuparmi del capitano."} } },
    { VB(V_GUARDA), H_TIMONE, NONE, 0, 0, { {TU, "Il timone della Minestrina. Ancora un po' e sarà mio."} } },
    { SPOSTA | VB(V_USA), H_TIMONE, NONE, 0, 0, { {SP_BRODO, "Il timone è del capitano. E il capitano sono io. Per ora."} } },
    { VB(V_GUARDA), H_OBLO, NONE, 0, 0, { {TU, "Fuori c'è il mare. Dentro, il brodo."} } },
    { VB(V_GUARDA), H_TAVOLA, NONE, 0, 0, { {TU, "Una tavola apparecchiata per uno: tovagliolo, cucchiaio e una pentola vuota."} } },
    { VB(V_GUARDA), H_USCITA_N, NONE, 0, 0, { {TU, "La scaletta che riporta alla barca di Ossobuco."} } },
    { VB(V_VAI) | VB(V_USA), H_USCITA_N, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_leave_ship },

    /* ---- sentieri (uscite verso la mappa) ---- */
    { VB(V_GUARDA), H_SENTIERO_D, NONE, 0, 0, { {TU, "Un sentiero che porta al resto dell'isola."} } },
    { VB(V_GUARDA), H_SENTIERO_B, NONE, 0, 0, { {TU, "Un sentiero che porta al resto dell'isola."} } },
    { VB(V_GUARDA), H_SENTIERO_N, NONE, 0, 0, { {TU, "Un sentiero che porta al resto dell'isola."} } },
    { VB(V_VAI), H_SENTIERO_D, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_open_map },
    { VB(V_VAI), H_SENTIERO_B, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_open_map },
    { VB(V_VAI), H_SENTIERO_N, NONE, 0, 0, { {0, 0} }, 0, 0, 0, fn_open_map },
};
static const int rule_count = sizeof(rules) / sizeof(rules[0]);

static int cond_ok(int c)
{
    int neg = c < 0, v, ok;
    if (!c) return 1;
    v = neg ? -c : c;
    ok = v > 1000 ? has_item(v - 1001) : flag(v - 1);
    return neg ? !ok : ok;
}

static int rule_matches(const Rule *r, int verb, int o1, int o2)
{
    if (!(r->verbs & VB(verb)) || r->o1 != o1) return 0;
    if (r->o2 == ANY_ITEM) { if (!IS_ITEM(o2)) return 0; }
    else if (r->o2 != o2) return 0;
    return cond_ok(r->c1) && cond_ok(r->c2);
}

/* Le regole valide per ogni oggetto (per i test e lo strumento di debug) */
int script_rule_count(void) { return rule_count; }

int script_action(int verb, int o1, int o2)
{
    int i, k;
    /* "Usa X con personaggio" equivale a "Dai X a personaggio" */
    if (verb == V_USA && IS_ITEM(o1) &&
        (o2 == H_GABBIANO || o2 == H_CUSTODE || o2 == H_BARBA || o2 == H_OSTE || o2 == H_MOZZO ||
         o2 == H_GABBIANO_F || o2 == H_OSSOBUCO || o2 == H_BRODO)) {
        int t = o1;
        o1 = o2;
        o2 = t;
        verb = V_DAI;
    }
    for (i = 0; i < rule_count; i++) {
        const Rule *r = &rules[i];
        if (!rule_matches(r, verb, o1, o2)) continue;
        if (r->fn) return r->fn(o1, o2);
        if (r->rem) remove_item(r->rem - 1);
        if (r->add) add_item(r->add - 1);
        if (r->set) set_flag(r->set - 1);
        for (k = 0; k < 4 && r->lines[k].text; k++)
            say(r->lines[k].sp, r->lines[k].text);
        return 1;
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
    D_CU_PIRATA, D_CU_PERCHE, D_CU_PROVE, D_CU_CAPPELLO, D_CU_TASCA,
    D_BRODO, D_BP_BRODO, D_CU_FARO, D_CU_BRODO,
    D_OS_NAVE, D_BR_SFIDA, D_BR_RESA,
    D_FINE_A, D_FINE_B, D_FINE_C            /* le tre risposte del duello */
};

/* Il duello di proverbi con Capitan Brodo: tre proverbi di fila. Le fini
   giuste sono nel quaderno del forziere; chi non lo ha letto vede solo
   quelle sbagliate (e quelle "da terraferma" non valgono). */
static int duel;                      /* 0 nessun duello, 1..3 proverbio in corso */
static const char *const duel_start[3] = {
    "Chi dorme in coffa...", "Rosso di sera...", "Can che abbaia..."
};
static const char *const duel_end[3][4] = {
    /* sbagliata, giusta, sbagliata, sbagliata (se non conosci la giusta) */
    { "...non piglia pesci.",       "...si sveglia in mare.", "...ha il mal di mare.",  "...russa come un tricheco." },
    { "...bel tempo si spera.",     "...grog a volontà.",     "...pollo arrosto.",      "...ehm... sera rossa?" },
    { "...non morde.",              "...non sale a bordo.",   "...ha perso l'osso.",    "...abbaia ancora un po'." },
};
static const unsigned char duel_order[3][3] = { {0, 1, 2}, {1, 2, 0}, {2, 0, 1} };

static int duel_answer(int k)        /* k-esima opzione mostrata -> risposta */
{
    int r = duel - 1, a = duel_order[r][k];
    if (a == 1 && !flag(F_CHEST2_OPEN)) a = 3;
    return a;
}

#define OPT(i, t) do { ids[n] = (i); texts[n] = (t); n++; } while (0)

int dialog_options(int npc, int ids[MAX_CHOICES], const char *texts[MAX_CHOICES])
{
    int n = 0;
    switch (npc) {
    case SP_OSTE:
        OPT(D_CHI, "Chi sei?");
        OPT(D_ISOLA, "Che posto è Puerto Polli?");
        OPT(D_PIRATA, "Voglio diventare un pirata!");
        if (flag(F_PART3)) OPT(D_BRODO, "Chi è Capitan Brodo?");
        else OPT(D_GROG, "Vorrei qualcosa da bere.");
        OPT(D_CIAO, "Arrivederci.");
        break;
    case SP_BARBA:
        OPT(D_CHI, "Chi sei?");
        OPT(D_BP_CAPPELLO, "Ti piace il mio cappello?");
        OPT(D_BP_CUSTODE, "Conosci il Custode?");
        if (flag(F_KNOWS_KEY) && !flag(F_GOT_KEY)) OPT(D_BP_CHIAVE, "Mi daresti la chiave della cassa?");
        if (flag(F_PART3)) OPT(D_BP_BRODO, "Conosci Capitan Brodo?");
        OPT(D_CIAO, "Ci vediamo, capitano.");
        break;
    case SP_CUSTODE:
        if (!flag(F_KNOWS_PROOFS)) OPT(D_CU_PIRATA, "Voglio diventare un pirata.");
        OPT(D_CU_PERCHE, "Perché un pollo fa da custode?");
        if (flag(F_KNOWS_PROOFS)) OPT(D_CU_PROVE, "Quali erano le prove?");
        if (flag(F_KNOWS_PROOFS) && flag(F_HAT_WORN) && !flag(F_PROOF_HAT))
            OPT(D_CU_CAPPELLO, "Guarda il mio cappello!");
        if (flag(F_KNOWS_PROOFS) && has_item(IT_CAPPELLO) && !flag(F_HAT_WORN))
            OPT(D_CU_TASCA, "Ho un cappello in tasca, vale?");
        if (flag(F_PART2) && !flag(F_LIGHT_ON)) OPT(D_CU_FARO, "Come faccio a far girare il faro?");
        if (flag(F_PART3)) OPT(D_CU_BRODO, "Hai paura di Capitan Brodo?");
        OPT(D_CIAO, "Arrivederci.");
        break;
    case SP_OSSOBUCO:
        OPT(D_CHI, "Chi sei?");
        if (!flag(F_FERRY)) OPT(D_OS_NAVE, "Mi porti alla nave fantasma?");
        else OPT(D_OS_NAVE, "Partiamo!");
        OPT(D_CIAO, "Ciao, scheletro.");
        break;
    case SP_BRODO:
        if (duel) {
            int k;
            for (k = 0; k < 3; k++) OPT(D_FINE_A + k, duel_end[duel - 1][duel_answer(k)]);
            OPT(D_BR_RESA, "Mi arrendo.");
        } else {
            OPT(D_BR_SFIDA, "Ti sfido a un duello di proverbi!");
            OPT(D_CHI, "Chi sei?");
            OPT(D_CIAO, "Me ne vado.");
        }
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
        case D_BRODO:
            say(SP_HERO, "Chi è Capitan Brodo?");
            say(SP_OSTE, "Shh! Ti avevo detto di non nominarlo!");
            say(SP_OSTE, "È un pirata fantasma. Da cent'anni vuole fare il brodo con il Custode, "
                         "e la sua nave si nasconde in una grotta che non sta su nessuna mappa.");
            say(SP_OSTE, "Su nessuna mappa normale, intendo. Il guardiano del faro, però, "
                         "segnava tutto sulle sue carte nautiche.");
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
        case D_BP_BRODO:
            say(SP_HERO, "Conosci Capitan Brodo?");
            say(SP_BARBA, "Brodo! Sfida tutti a duello di proverbi. Nessuno l'ha mai battuto.");
            say(SP_BARBA, "I suoi proverbi non finiscono come quelli di terraferma. Chi risponde "
                          "come la nonna, perde. Li tiene scritti in un forziere, dicono.");
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
    case SP_OSSOBUCO:
        switch (id) {
        case D_CHI:
            say(SP_HERO, "Chi sei?");
            say(SP_OSSOBUCO, "Ossobuco, traghettatore di Capitan Brodo. Porto i clienti alla sua nave.");
            say(SP_OSSOBUCO, "Andata e basta, di solito. Il ritorno è un optional.");
            break;
        case D_OS_NAVE:
            if (flag(F_FERRY)) {
                say(SP_HERO, "Partiamo!");
                fn_ferry(0, 0);
                end_dialog();
            } else {
                set_flag(F_KNOWS_TOLL);
                say(SP_HERO, "Mi porti alla nave fantasma?");
                say(SP_OSSOBUCO, "Certo. Il pedaggio è un teschio. Li colleziono: è un hobby di famiglia.");
                say(SP_HERO, "E dove lo trovo un teschio?");
                say(SP_OSSOBUCO, "In una grotta piena di pirati? Prova a guardarti intorno. Dentro.");
            }
            break;
        default:
            say(SP_HERO, "Ciao, scheletro.");
            say(SP_OSSOBUCO, "A presto. Prima o poi passano tutti di qui.");
            end_dialog();
            break;
        }
        break;
    case SP_BRODO:
        switch (id) {
        case D_CHI:
            say(SP_HERO, "Chi sei?");
            say(SP_BRODO, "Capitan Brodo, il terrore dei pollai! Cent'anni fa giurai di "
                          "cucinare il brodo più famoso dei sette mari.");
            say(SP_BRODO, "Poi sono morto. Ma un giuramento è un giuramento.");
            break;
        case D_BR_SFIDA:
            duel = 1;
            say(SP_HERO, "Ti sfido a un duello di proverbi!");
            say(SP_BRODO, "Ah ah! Un novellino! Vediamo se sai finire un proverbio da pirata.");
            say(SP_BRODO, duel_start[0]);
            break;
        case D_BR_RESA:
            duel = 0;
            say(SP_HERO, "Mi arrendo.");
            say(SP_BRODO, "Saggio. Torna quando avrai studiato. Io sono qui. Sono sempre qui.");
            break;
        case D_FINE_A: case D_FINE_B: case D_FINE_C: {
            int a = duel_answer(id - D_FINE_A);
            say(SP_HERO, duel_end[duel - 1][a]);
            if (a != 1) {
                duel = 0;
                say(SP_BRODO, "Ah ah ah! Sbagliato! Quella è roba da nonne di terraferma!");
                if (!flag(F_CHEST2_OPEN))
                    say(SP_HERO, "(Devo scoprire come finiscono i suoi proverbi. Li avrà scritti da qualche parte.)");
                else
                    say(SP_BRODO, "Ricominciamo da capo, se hai il coraggio.");
            } else if (duel < 3) {
                say(SP_BRODO, duel == 1 ? "Grrr... giusto. Fortuna del principiante." : "Argh! Giusto anche questo!");
                duel++;
                say(SP_BRODO, duel_start[duel - 1]);
            } else {
                duel = 0;
                set_flag(F_BRODO_WON);
                say(SP_BRODO, "No... NOOO! Sconfitto da un novellino!");
                say(SP_BRODO, "Un fantasma battuto a proverbi... mi sciolgo... come un dado nel brodo...");
                say(SP_NARRATORE, "Capitan Brodo svanisce in una nuvola di vapore che sa di sedano.");
                say(SP_HERO, "La Minestrina è senza capitano. Cioè no: ha un capitano. Io!");
                say(SP_NARRATORE, "Quella notte la Minestrina attracca a Puerto Polli, guidata dalla luce del faro. "
                                  "Pino è il nuovo nostromo. Il Custode dorme tranquillo.");
                end_dialog();
                queue_call(CALL_ENDING);
            }
            break;
        }
        default:
            say(SP_HERO, "Me ne vado.");
            say(SP_BRODO, "Torna presto. La pentola è sempre sul fuoco.");
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
        case D_CU_FARO:
            say(SP_HERO, "Come faccio a far girare il faro?");
            say(SP_CUSTODE, "Il guardiano se n'è andato e ha lasciato la chiave al suo gabbiano.");
            say(SP_CUSTODE, "Quel gabbiano scambierebbe sua madre per qualcosa che luccica. "
                            "E Barbapollo, di cose che luccicano, ne ha sempre una in tasca.");
            break;
        case D_CU_BRODO:
            say(SP_HERO, "Hai paura di Capitan Brodo?");
            say(SP_CUSTODE, "Io? Paura? Ho solo... la pelle d'oca. Ce l'ho sempre, sono un pollo.");
            say(SP_CUSTODE, "I fantasmi non si battono con la spada, pirata. Si battono con le parole: "
                            "un proverbio finito bene li scioglie come un dado.");
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
        if (flag(F_PART2) && !flag(F_GOT_COMPASS)) {
            set_flag(F_GOT_COMPASS);
            add_item(IT_BUSSOLA);
            say(SP_BARBA, "Arr! Ho saputo: il Custode ti ha fatto pirata! Questo merita un regalo.");
            say(SP_BARBA, "Tieni la mia vecchia bussola. Luccica come un doblone e punta sempre verso il grog.");
            say(SP_HERO, "Grazie, capitano!");
        } else {
            say(SP_BARBA, "Arr! Ora sì che sembri un pirata. Cosa vuoi?");
        }
        break;
    case SP_OSSOBUCO:
        if (!flag(F_MET_OSSO)) {
            set_flag(F_MET_OSSO);
            say(SP_HERO, "Ehm... salve?");
            say(SP_OSSOBUCO, "Salve a te, vivo. Raro vederne da queste parti.");
        } else {
            say(SP_OSSOBUCO, "Di nuovo tu. Ancora vivo? Complimenti.");
        }
        break;
    case SP_BRODO:
        duel = 0;
        if (!flag(F_MET_BRODO)) {
            set_flag(F_MET_BRODO);
            say(SP_BRODO, "Un ospite! Proprio in tempo per la cena.");
            say(SP_BRODO, "Ho la pentola, ho l'acqua, ho il sedano... mi manca solo il pollo. "
                          "E stanotte vado a prenderlo: il Custode di Puerto Polli!");
            say(SP_HERO, "Non ci provare! Il Custode è... beh, è un pollo. Ma è il NOSTRO pollo.");
            say(SP_BRODO, "Vuoi fermarmi? Allora sfidami. A duello di proverbi! Nessuno mi ha mai battuto.");
        } else {
            say(SP_BRODO, "Sei tornato a perdere ancora?");
        }
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

int script_map_visible(int idx)
{
    if (map_places[idx].room == ROOM_GROTTA) return flag(F_PART3) && flag(F_KNOWS_GROTTA);
    return 1;
}

/* la grotta compare sulla mappa solo dopo averla vista sulla carta nautica */
void script_draw_map(void)
{
    int i;
    for (i = 0; i < map_place_count; i++) {
        const MapPlace *p = &map_places[i];
        if (p->room != ROOM_GROTTA || !script_map_visible(i)) continue;
        gfx_fill(p->x + 2, MAP_Y + p->y + 14, 12, 12, PAT_BLACK);       /* l'ingresso */
        gfx_fill(p->x + 4, MAP_Y + p->y + 12, 8, 2, PAT_BLACK);
        gfx_text(p->x + 16, MAP_Y + p->y + 4, "Grotta", FONT_BOLD, COL_BLACK);
        gfx_text(p->x + 16, MAP_Y + p->y + 18, "del Brodo", FONT_REGULAR, COL_BLACK);
    }
}

void script_map_place(int idx)
{
    const MapPlace *p = &map_places[idx];
    if (p->room < 0 && flag(F_PART2)) {
        go_room(ROOM_FARO);
        return;
    }
    if (p->room < 0) {
        say(SP_NARRATORE, "Il faro è chiuso. Sulla porta c'è un cartello: \"Torno subito. Il guardiano\".");
        say(SP_NARRATORE, "Il cartello sembra lì da vent'anni.");
        return;
    }
    go_room(p->room);
}
