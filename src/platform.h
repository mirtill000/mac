/*
 * platform.h - quello che il gioco chiede al sistema ospite.
 * Implementazioni: src/mac/main_mac.c (Macintosh, Toolbox),
 *                  src/sdl/main_sdl.c (PC, per sviluppo),
 *                  src/test/main_test.c (test automatici senza video).
 */
#ifndef PLATFORM_H
#define PLATFORM_H

/* copia a video il rettangolo [x0,x1) x [y0,y1) di gfx_fb */
void plat_present(int x0, int y0, int x1, int y1);

/* salvataggi: ritornano 0 se ok */
int  plat_save(int slot, const unsigned char *data, int len);
int  plat_load(int slot, unsigned char *data, int len);

/* data e ora locali */
void plat_datetime(int *day, int *month, int *year, int *hour, int *minute);

void plat_beep(void);

#endif
