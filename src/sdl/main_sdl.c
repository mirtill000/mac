/*
 * main_sdl.c - versione per PC (Linux/Windows/macOS moderni) basata su SDL2.
 * Serve per sviluppare e provare il gioco senza un Macintosh: la grafica
 * e' identica (stesso framebuffer 1-bit), ingrandita 2x.
 *
 * Tasti: Esc menu, Tab inventario, Spazio/Invio avanza, Ctrl+S/L/Q.
 */
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../game.h"
#include "../gfx.h"
#include "../platform.h"

#define SCALE 2

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static Uint32 pixels[FB_W * FB_H];

void plat_present(int x0, int y0, int x1, int y1)
{
    int x, y;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            int bit = gfx_fb[y * FB_RB + (x >> 3)] & (0x80 >> (x & 7));
            /* bianco leggermente caldo, come un fosforo del Mac Classic */
            pixels[y * FB_W + x] = bit ? 0xFF000000u : 0xFFF2F2EAu;
        }
}

static void slot_path(int slot, char *buf, size_t n)
{
    snprintf(buf, n, "puertopolli_%d.sav", slot + 1);
}

int plat_save(int slot, const unsigned char *data, int len)
{
    char p[64];
    FILE *f;
    slot_path(slot, p, sizeof(p));
    f = fopen(p, "wb");
    if (!f) return -1;
    if (fwrite(data, 1, (size_t)len, f) != (size_t)len) { fclose(f); return -1; }
    fclose(f);
    return 0;
}

int plat_load(int slot, unsigned char *data, int maxlen)
{
    char p[64];
    FILE *f;
    size_t n;
    slot_path(slot, p, sizeof(p));
    f = fopen(p, "rb");
    if (!f) return -1;
    n = fread(data, 1, (size_t)maxlen, f);
    fclose(f);
    return (int)n;
}

void plat_datetime(int *d, int *mo, int *y, int *h, int *mi)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    *d = tm->tm_mday; *mo = tm->tm_mon + 1; *y = tm->tm_year + 1900;
    *h = tm->tm_hour; *mi = tm->tm_min;
}

void plat_beep(void) { printf("\a"); fflush(stdout); }

unsigned long plat_ticks(void) { return (unsigned long)SDL_GetTicks() * 60UL / 1000UL; }

int main(int argc, char **argv)
{
    Uint32 last;
    int running = 1;
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    win = SDL_CreateWindow("Puerto Polli", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           FB_W * SCALE, FB_H * SCALE, SDL_WINDOW_RESIZABLE);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ren, FB_W, FB_H);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, FB_W, FB_H);
    SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR));

    game_init();
    last = SDL_GetTicks();
    while (running && !game_wants_quit()) {
        SDL_Event e;
        Uint32 now;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running = 0; break;
            case SDL_MOUSEMOTION: game_mouse(e.motion.x, e.motion.y); break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) game_click(e.button.x, e.button.y);
                break;
            case SDL_KEYDOWN: {
                SDL_Keycode k = e.key.keysym.sym;
                int cmd = (e.key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) != 0;
                if (k == SDLK_ESCAPE) game_key(KEY_ESC, 0);
                else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) game_key(KEY_RETURN, 0);
                else if (k == SDLK_TAB) game_key(KEY_TAB, 0);
                else if (k >= 32 && k < 127) game_key((int)k, cmd);
                break;
            }
            }
        }
        now = SDL_GetTicks();
        while (now - last >= 1000 / GAME_FPS) {
            game_tick();
            last += 1000 / GAME_FPS;
        }
        game_render();
        SDL_UpdateTexture(tex, NULL, pixels, FB_W * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
        SDL_Delay(5);
    }
    SDL_Quit();
    return 0;
}
