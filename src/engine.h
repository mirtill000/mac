/*
 * Nota sulle convenzioni di script_action:
 *   V_DAI:  o1 = chi riceve (scena), o2 = oggetto dell'inventario
 *   V_USA:  o1 = oggetto dell'inventario, o2 = bersaglio (o OBJ_NONE)
 *   altri:  o1 = oggetto, o2 = OBJ_NONE
 */
/*
 * engine.h - interfaccia interna tra il motore (engine.c) e la storia
 * (script.c): stanze, oggetti, verbi, flag e funzioni per i copioni.
 */
#ifndef ENGINE_H
#define ENGINE_H

/* ---- layout dello schermo ---------------------------------------- */
#define SCENE_H     240
#define SENTENCE_Y  240
#define PANEL_Y     254
#define MAP_Y       30         /* la mappa e' disegnata da questa riga in giu' */

/* ---- stanze ------------------------------------------------------ */
enum { ROOM_DOCK, ROOM_TAVERN, ROOM_BEACH, ROOM_NIGHT,
       /* Parte II e III */
       ROOM_FARO, ROOM_FARO_IN, ROOM_GROTTA, ROOM_GROTTA_IN, ROOM_NAVE, ROOM_NAVE_IN,
       ROOM_COUNT };
#define ROOM_MAP 99            /* destinazione speciale: la mappa */

/* ---- oggetti dell'inventario (stesso ordine delle icone) ---------- */
enum {
    IT_BOTTIGLIA, IT_MONETA, IT_CHIAVE, IT_MAPPA,
    IT_PIUMA, IT_CALAMARO, IT_CAPPELLO, IT_LIBRO,
    IT_BUSSOLA, IT_LANTERNA, IT_CARTA, IT_TESCHIO, IT_COUNT
};
#define INV_MAX 8              /* oggetti portati insieme (caselle del pannello) */

/* ---- verbi ------------------------------------------------------- */
enum {
    V_DAI, V_APRI, V_CHIUDI, V_RACCOGLI, V_GUARDA, V_SPINGI,
    V_USA, V_PARLA, V_TIRA, V_COUNT
};
#define V_VAI V_COUNT          /* nessun verbo: "Vai a" */

/* ---- oggetti cliccabili in scena (id >= H_FIRST) ------------------ */
enum {
    H_FIRST = 16,
    /* molo */
    H_PIUMA = H_FIRST, H_TABERNA, H_INSEGNA, H_BARBA, H_MOZZO, H_GABBIANO,
    H_BANCARELLA, H_VELIERO, H_PAESE, H_SENTIERO_D,
    /* taberna */
    H_OSTE, H_BANDIERA, H_QUADRO, H_LANTERNA, H_FINESTRA, H_BOTTIGLIE,
    H_BOCCALE, H_USCITA_T,
    /* spiaggia */
    H_CAPPELLO, H_CASSA, H_RELITTO, H_FARO, H_PALME, H_MARE, H_SCOGLIERA,
    H_SENTIERO_B,
    /* rovine (notte) */
    H_CUSTODE, H_LUNA, H_NAVE_F, H_BITTA, H_PALMA, H_SENTIERO_N,
    /* faro, esterno */
    H_FARO_E, H_CASA, H_PORTA_F, H_SCALA_F, H_GABBIANO_F, H_PALO, H_CARTELLO,
    H_SOLE, H_VELA, H_MARE_F, H_STACCIONATA, H_SENTIERO_F,
    /* faro, interno */
    H_LENTE, H_MECCANISMO, H_SCALA_I, H_PORTICINA, H_FINESTRE, H_LUMINO,
    H_TAVOLO, H_CARTA, H_LANTERNA_F,
    /* grotta, esterno */
    H_INGRESSO, H_OSSOBUCO, H_BARCA, H_CASSETTA, H_RUSCELLO, H_ROCCE, H_SENTIERO_G,
    /* grotta, interno */
    H_FORZIERE, H_TESCHIO, H_CASCATA, H_POZZA, H_STALATTITI, H_USCITA_G,
    /* nave fantasma, fuori e dentro */
    H_NAVE_E, H_LUNA_N, H_MARE_N, H_BARCA_N,
    H_BRODO, H_CANDELE, H_LANTERNA_N, H_CATENA, H_TIMONE, H_TAVOLA, H_OBLO, H_USCITA_N,
    H_LAST
};
#define OBJ_NONE (-1)
#define IS_ITEM(o) ((o) >= 0 && (o) < IT_COUNT)

typedef struct {
    short id;
    signed char room;
    const char *name;
    short x, y, w, h;          /* rettangolo cliccabile */
    short wx, wy;              /* punto dove andare (wx < 0: non camminare) */
    signed char face;          /* 1 destra, -1 sinistra, 0 invariato */
    signed char exit_to;       /* -1, oppure stanza / ROOM_MAP */
} Hotspot;

/* Zona calpestabile: unione di (al massimo 2) poligoni convessi, vertici
   in senso orario sullo schermo. Il protagonista non ne esce mai. */
#define WALK_MAX_PTS 6
typedef struct { signed char n; short x[WALK_MAX_PTS], y[WALK_MAX_PTS]; } WalkPoly;
typedef struct { signed char npoly; WalkPoly poly[2]; } WalkArea;

/* prospettiva: altezza del protagonista in funzione della y dei piedi,
   calibrata sugli oggetti e sui personaggi della stanza; tint = velatura
   (0 nessuna, 1 retino leggero per gli ambienti bui). h_far = 0: il
   protagonista e' gia' dipinto nella scena (in barca) e non si muove. */
typedef struct { short y_far, h_far, y_near, h_near; short tint; } RoomDepth;

/* ---- chi parla --------------------------------------------------- */
enum { SP_HERO, SP_OSTE, SP_BARBA, SP_MOZZO, SP_CUSTODE, SP_GABBIANO,
       SP_OSSOBUCO, SP_BRODO, SP_NARRATORE, SP_COUNT };

/* ---- flag della storia ------------------------------------------- */
enum {
    F_HAT_TAKEN, F_HAT_WORN, F_GOT_GROG, F_GOT_KEY, F_CRATE_OPEN,
    F_GULL_FED, F_FEATHER_TAKEN, F_MET_OSTE, F_KNOWS_CUSTODE, F_MET_CUSTODE,
    F_KNOWS_PROOFS, F_KNOWS_KEY, F_PROOF_HAT, F_PROOF_FEATHER, F_PROOF_BOOK,
    F_GAME_WON, F_MOZZO_1, F_MOZZO_2, F_SEEN_NIGHT,
    /* Parte II: la luce del Faro */
    F_PART2, F_GOT_COMPASS, F_FARO_KEY, F_FARO_OPEN, F_LANTERN_TAKEN, F_CHART_TAKEN,
    F_GEARS_OILED, F_LIGHT_ON, F_SEEN_FARO, F_SEEN_FARO_IN, F_KNOWS_GULL_KEY,
    /* Parte III: il vascello di Capitan Brodo */
    F_PART3, F_KNOWS_GROTTA, F_SKULL_TAKEN, F_CHEST2_OPEN, F_KNOWS_PROVERBS,
    F_FERRY, F_SEEN_GROTTA, F_SEEN_GROTTA_IN, F_SEEN_NAVE, F_MET_OSSO, F_MET_BRODO,
    F_DUEL_A, F_DUEL_B, F_BRODO_WON, F_KNOWS_TOLL
};

/* chiamate differite (eseguite dopo che i messaggi sono stati letti) */
enum { CALL_NONE, CALL_ENDING, CALL_ENTER_TAVERN, CALL_PART2, CALL_PART3, CALL_FERRY };

/* ---- servizi del motore per i copioni ---------------------------- */
void say(int speaker, const char *text);
int  has_item(int it);
void add_item(int it);
void remove_item(int it);
int  flag(int f);
void set_flag(int f);
void clear_flag(int f);
void go_room(int room);
void open_map(void);
void start_dialog(int npc);
void queue_call(int what);
void hero_face(int dir);
int  current_room(void);
void draw_prop(int item, int x, int y);
void draw_patch(int patch);
void show_chapter(int part);   /* cartello "Parte II/III" o il finale (0) */
int  hero_hidden(void);
void refresh_room(void);       /* ridisegna la stanza (oggetti e animazioni) */

/* ---- dati e copioni (script.c) ----------------------------------- */
extern const Hotspot hotspots[];
extern const int hotspot_count;
extern const WalkArea walk_areas[ROOM_COUNT];
extern const RoomDepth room_depth[ROOM_COUNT];
extern const char *const speaker_names[SP_COUNT];
extern const char *const verb_names[V_COUNT + 1];

int  hotspot_visible(int id);
const char *object_name(int obj);
const char *item_title(int it);
const char *room_title(int room);
int  room_background(int room);

void script_new_game(void);
void script_enter_room(int room);
int  script_action(int verb, int o1, int o2);
void script_default(int verb, int o1, int o2);
void script_draw_props(int room);

/* dialoghi a scelta multipla */
#define MAX_CHOICES 5
int  dialog_options(int npc, int ids[MAX_CHOICES], const char *texts[MAX_CHOICES]);
void dialog_choose(int npc, int id);   /* ritorna nel motore; id < 0 = fine */
void end_dialog(void);
void dialog_greeting(int npc);

/* mappa */
typedef struct { const char *name; short x, y, w, h; signed char room; } MapPlace;
extern const MapPlace map_places[];
extern const int map_place_count;
void script_map_place(int idx);
int  script_map_visible(int idx);
void script_draw_map(void);    /* segni aggiunti alla mappa (luoghi scoperti) */
const char *chapter_text(int part, int line);   /* righe del cartello */
int  chapter_room(int part);   /* dove si riprende dopo il cartello */

#endif
