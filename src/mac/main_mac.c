/*
 * main_mac.c - versione Macintosh (68000, System 6.0.x / System 7).
 *
 * Il gioco disegna in un framebuffer 1-bit (gfx_fb) che ha esattamente il
 * formato di una BitMap QuickDraw: per mostrarlo basta CopyBits.
 *
 * Su uno schermo da 512x342 (Mac Plus, SE, Classic) la barra dei menu
 * viene nascosta e il gioco occupa tutto lo schermo, come nel mockup.
 * Su schermi piu' grandi la finestra viene centrata sotto la barra dei menu.
 */
#include <Quickdraw.h>
#include <Windows.h>
#include <Menus.h>
#include <Fonts.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Events.h>
#include <OSUtils.h>
#include <Files.h>
#include <ToolUtils.h>
#include <LowMem.h>
#include <Sound.h>
#include <Devices.h>

#include "../game.h"
#include "../gfx.h"
#include "../platform.h"

static WindowPtr win;
static BitMap off_bits;
static int menubar_hidden;
static short saved_mbar_height;
static RgnHandle saved_gray;
static MenuHandle apple_menu, file_menu;
static int has_wne;

enum { M_APPLE = 128, M_FILE = 129 };
enum { I_ABOUT = 1 };
enum { I_SAVE = 1, I_LOAD = 2, I_SEP = 3, I_QUIT = 4 };

/* ------------------------------------------------------------------ */
/* Piattaforma                                                        */
/* ------------------------------------------------------------------ */

void plat_present(int x0, int y0, int x1, int y1)
{
    Rect r;
    SetRect(&r, x0, y0, x1, y1);
    SetPort(win);
    CopyBits(&off_bits, &win->portBits, &r, &r, srcCopy, NULL);
}

static void save_name(int slot, Str255 name)
{
    static const char base[] = "Puerto Polli Salvataggio ";
    int i, n = 0;
    for (i = 0; base[i]; i++) name[++n] = (unsigned char)base[i];
    name[++n] = (unsigned char)('1' + slot);
    name[0] = (unsigned char)n;
}

int plat_save(int slot, const unsigned char *data, int len)
{
    Str255 name;
    short ref;
    long count = len;
    OSErr err;

    save_name(slot, name);
    err = Create(name, 0, 'PPol', 'PPsv');
    if (err != noErr && err != dupFNErr) return -1;
    if (FSOpen(name, 0, &ref) != noErr) return -1;
    err = SetEOF(ref, 0);
    if (err == noErr) err = FSWrite(ref, &count, data);
    FSClose(ref);
    FlushVol(NULL, 0);
    return (err == noErr && count == len) ? 0 : -1;
}

int plat_load(int slot, unsigned char *data, int len)
{
    Str255 name;
    short ref;
    long count = len;
    OSErr err;

    save_name(slot, name);
    if (FSOpen(name, 0, &ref) != noErr) return -1;
    err = FSRead(ref, &count, data);
    FSClose(ref);
    return (err == noErr && count == len) ? 0 : -1;
}

void plat_datetime(int *d, int *mo, int *y, int *h, int *mi)
{
    unsigned long secs;
    DateTimeRec dt;
    GetDateTime(&secs);
    SecondsToDate(secs, &dt);
    *d = dt.day; *mo = dt.month; *y = dt.year;
    *h = dt.hour; *mi = dt.minute;
}

void plat_beep(void) { SysBeep(10); }

/* ------------------------------------------------------------------ */
/* Barra dei menu                                                     */
/* ------------------------------------------------------------------ */

static void hide_menu_bar(void)
{
    RgnHandle mb = NewRgn();
    saved_mbar_height = LMGetMBarHeight();
    saved_gray = NewRgn();
    CopyRgn(LMGetGrayRgn(), saved_gray);
    SetRectRgn(mb, qd.screenBits.bounds.left, qd.screenBits.bounds.top,
               qd.screenBits.bounds.right, qd.screenBits.bounds.top + saved_mbar_height);
    UnionRgn(LMGetGrayRgn(), mb, LMGetGrayRgn());
    DisposeRgn(mb);
    LMSetMBarHeight(0);
    menubar_hidden = 1;
}

static void restore_menu_bar(void)
{
    if (!menubar_hidden) return;
    LMSetMBarHeight(saved_mbar_height);
    CopyRgn(saved_gray, LMGetGrayRgn());
    DisposeRgn(saved_gray);
    menubar_hidden = 0;
    DrawMenuBar();
}

static void setup_menus(void)
{
    apple_menu = NewMenu(M_APPLE, "\p\024");
    AppendMenu(apple_menu, "\pInformazioni su Puerto Polli...");
    AppendMenu(apple_menu, "\p(-");
    AppendResMenu(apple_menu, 'DRVR');
    InsertMenu(apple_menu, 0);
    file_menu = NewMenu(M_FILE, "\pArchivio");
    AppendMenu(file_menu, "\pSalva partita.../S;Carica partita.../L;(-;Esci/Q");
    InsertMenu(file_menu, 0);
    DrawMenuBar();
}

static void about_box(void)
{
    Alert(128, NULL);
    plat_present(0, 0, FB_W, FB_H);
}

static void do_menu(long choice)
{
    short menu = (short)(choice >> 16), item = (short)(choice & 0xFFFF);
    if (menu == M_APPLE) {
        if (item == I_ABOUT) {
            about_box();
        } else {
            Str255 name;
            GetMenuItemText(apple_menu, item, name);
            OpenDeskAcc(name);
        }
    } else if (menu == M_FILE) {
        if (item == I_SAVE) game_key('s', 1);
        else if (item == I_LOAD) game_key('l', 1);
        else if (item == I_QUIT) game_key('q', 1);
    }
    HiliteMenu(0);
}

/* ------------------------------------------------------------------ */
/* Avvio                                                              */
/* ------------------------------------------------------------------ */

#define TRAP_WAITNEXTEVENT  0xA860
#define TRAP_UNIMPLEMENTED  0xA89F

static int trap_available(unsigned short trap)
{
    return NGetTrapAddress(trap, kToolboxTrapType) !=
           NGetTrapAddress(TRAP_UNIMPLEMENTED, kToolboxTrapType);
}

static void init_toolbox(void)
{
    MaxApplZone();
    MoreMasters();
    MoreMasters();
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    FlushEvents(everyEvent, 0);
    InitCursor();
    has_wne = trap_available(TRAP_WAITNEXTEVENT);
}

static void make_window(void)
{
    Rect sb = qd.screenBits.bounds, wr;
    int sw = sb.right - sb.left, sh = sb.bottom - sb.top;
    int top;

    if (sh - LMGetMBarHeight() < FB_H) {
        hide_menu_bar();
        top = sb.top;
    } else {
        setup_menus();
        top = sb.top + LMGetMBarHeight() + (sh - LMGetMBarHeight() - FB_H) / 2;
    }
    wr.left = (short)(sb.left + (sw - FB_W) / 2);
    wr.top = (short)top;
    wr.right = (short)(wr.left + FB_W);
    wr.bottom = (short)(wr.top + FB_H);
    win = NewWindow(NULL, &wr, "\pPuerto Polli", true, plainDBox, (WindowPtr)-1, false, 0);
    SetPort(win);

    off_bits.baseAddr = (Ptr)gfx_fb;
    off_bits.rowBytes = FB_RB;
    SetRect(&off_bits.bounds, 0, 0, FB_W, FB_H);

    {
        CursHandle ch = GetCursor(crossCursor);
        if (ch) SetCursor(*ch);
    }
}

static void handle_key(EventRecord *ev)
{
    char c = (char)(ev->message & charCodeMask);
    int cmd = (ev->modifiers & cmdKey) != 0;
    if (cmd && !menubar_hidden) {
        long m = MenuKey(c);
        if (m) { do_menu(m); return; }
    }
    if (cmd && c == '.') { game_key(KEY_ESC, 0); return; }
    if (c == 0x1B) game_key(KEY_ESC, 0);
    else if (c == 0x0D || c == 0x03) game_key(KEY_RETURN, 0);
    else if (c == 0x09) game_key(KEY_TAB, 0);
    else game_key((unsigned char)c, cmd);
}

int main(void)
{
    unsigned long last;
    Point mouse;

    init_toolbox();
    make_window();
    game_init();
    gfx_dirty_all();
    last = TickCount();

    while (!game_wants_quit()) {
        EventRecord ev;
        Boolean got;
        unsigned long now;
        int steps;

        if (has_wne) {
            got = WaitNextEvent(everyEvent, &ev, 0, NULL);
        } else {
            SystemTask();
            got = GetNextEvent(everyEvent, &ev);
        }
        if (got) {
            switch (ev.what) {
            case mouseDown: {
                WindowPtr w;
                short part = FindWindow(ev.where, &w);
                if (part == inMenuBar && !menubar_hidden) {
                    do_menu(MenuSelect(ev.where));
                } else if (part == inSysWindow) {
                    SystemClick(&ev, w);
                } else if (w == win || menubar_hidden) {
                    Point p = ev.where;
                    if (w != FrontWindow()) SelectWindow(win);
                    SetPort(win);
                    GlobalToLocal(&p);
                    game_click(p.h, p.v);
                }
                break;
            }
            case keyDown:
            case autoKey:
                handle_key(&ev);
                break;
            case updateEvt:
                if ((WindowPtr)ev.message == win) {
                    BeginUpdate(win);
                    plat_present(0, 0, FB_W, FB_H);
                    EndUpdate(win);
                }
                break;
            case osEvt:
                /* ritorno da un'altra applicazione (MultiFinder) */
                if (((ev.message >> 24) & 0xFF) == 0x01 && (ev.message & 1))
                    plat_present(0, 0, FB_W, FB_H);
                break;
            }
        }

        SetPort(win);
        GetMouse(&mouse);
        game_mouse(mouse.h, mouse.v);

        now = TickCount();
        steps = 0;
        while (now - last >= TICKS_PER_FRAME && steps < 4) {
            game_tick();
            last += TICKS_PER_FRAME;
            steps++;
        }
        if (now - last >= TICKS_PER_FRAME) last = now;   /* Mac troppo lento: niente recuperi */
        game_render();
    }

    restore_menu_bar();
    DisposeWindow(win);
    return 0;
}
