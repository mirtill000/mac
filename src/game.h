/*
 * game.h - interfaccia del motore di gioco verso la piattaforma.
 */
#ifndef GAME_H
#define GAME_H

#define GAME_FPS 20          /* aggiornamenti logici al secondo */
#define TICKS_PER_FRAME 3    /* tick da 1/60 s per aggiornamento */

/* tasti speciali passati a game_key */
#define KEY_ESC    27
#define KEY_RETURN 13
#define KEY_TAB    9
#define KEY_UP     30
#define KEY_DOWN   31

void game_init(void);
void game_mouse(int x, int y);        /* posizione del puntatore */
void game_click(int x, int y);        /* pressione del pulsante */
void game_key(int ch, int command);   /* tasto (command = tasto Mela/Ctrl) */
void game_tick(void);                 /* un passo logico (1/20 s) */
void game_render(void);               /* ridisegna e chiama plat_present */
int  game_wants_quit(void);

/* introspezione per i test automatici */
enum { DBG_BUSY, DBG_FLAG, DBG_HAS, DBG_MODE, DBG_ROOM, DBG_SLOT, DBG_MODAL, DBG_DIALOG, DBG_REDRAW };
int  game_debug(int what, int arg);

#endif
