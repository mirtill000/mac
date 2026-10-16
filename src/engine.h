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

/* ---- stanze ------------------------------------------------------ */
enum { ROOM_DOCK, ROOM_TAVERN, ROOM_BEACH, ROOM_NIGHT, ROOM_COUNT };
#define ROOM_MAP 99            /* destinazione speciale: la mappa */

/* ---- oggetti dell'inventario (stesso ordine delle icone) ---------- */
enum {
    IT_BOTTIGLIA, IT_MONETA, IT_CHIAVE, IT_MAPPA,
    IT_PIUMA, IT_CALAMARO, IT_CAPPELLO, IT_LIBRO, IT_COUNT
};

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

typedef struct { short x0, y0, x1, y1; } WalkArea;

/* ---- chi parla --------------------------------------------------- */
enum { SP_HERO, SP_OSTE, SP_BARBA, SP_MOZZO, SP_CUSTODE, SP_GABBIANO,
       SP_NARRATORE, SP_COUNT };

/* ---- flag della storia ------------------------------------------- */
enum {
    F_HAT_TAKEN, F_HAT_WORN, F_GOT_GROG, F_GOT_KEY, F_CRATE_OPEN,
    F_GULL_FED, F_FEATHER_TAKEN, F_MET_OSTE, F_KNOWS_CUSTODE, F_MET_CUSTODE,
    F_KNOWS_PROOFS, F_KNOWS_KEY, F_PROOF_HAT, F_PROOF_FEATHER, F_PROOF_BOOK,
    F_GAME_WON, F_MOZZO_1, F_MOZZO_2, F_SEEN_NIGHT
};

/* chiamate differite (eseguite dopo che i messaggi sono stati letti) */
enum { CALL_NONE, CALL_ENDING, CALL_ENTER_TAVERN };

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

/* ---- dati e copioni (script.c) ----------------------------------- */
extern const Hotspot hotspots[];
extern const int hotspot_count;
extern const WalkArea walk_areas[ROOM_COUNT];
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

#endif
