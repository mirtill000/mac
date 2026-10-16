#!/usr/bin/env python3
"""
Generatore degli asset grafici di Puerto Polli.

Prende il mockup (reference/mockup.png), ritaglia le scene, rimuove i
personaggi "stampati" negli sfondi che nel gioco sono sprite animati
(il protagonista) e i box di dialogo, riscala alle dimensioni del Mac
Classic e converte tutto in 1-bit con dithering Atkinson (lo stesso
usato da Bill Atkinson per MacPaint).

Genera:
  src/assets.c / src/assets.h   bitmap compresse (PackBits) + font + sprite
  build/preview/*.png           anteprime per controllo visivo

Uso:  python3 tools/make_assets.py
Richiede: Pillow, numpy, opencv-python-headless
"""
import os
import sys

import numpy as np
import cv2
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MOCKUP = os.path.join(ROOT, "reference", "mockup.png")
PREVIEW = os.path.join(ROOT, "build", "preview")

SCREEN_W, SCREEN_H = 512, 342
SCENE_H = 240

# ---------------------------------------------------------------------------
# Utilita' immagine
# ---------------------------------------------------------------------------

def load_gray():
    im = Image.open(MOCKUP).convert("L")
    return np.array(im).astype(np.float32)


def inpaint(img, rects, radius=4):
    """Inpainting (Navier-Stokes) delle zone indicate (coordinate locali)."""
    mask = np.zeros(img.shape, np.uint8)
    for (x0, y0, x1, y1) in rects:
        mask[y0:y1, x0:x1] = 255
    out = cv2.inpaint(np.clip(img, 0, 255).astype(np.uint8), mask, radius,
                      cv2.INPAINT_NS)
    return out.astype(np.float32)


def clone(img, rect, dx, dy=0, feather=3):
    """Copia nella zona rect il contenuto spostato di (dx,dy), con bordo
    sfumato per nascondere la cucitura."""
    x0, y0, x1, y1 = rect
    src = img[y0 + dy:y1 + dy, x0 + dx:x1 + dx].copy()
    h, w = src.shape
    a = np.ones((h, w), np.float32)
    for i in range(feather):
        f = (i + 1) / (feather + 1)
        a[i, :] = np.minimum(a[i, :], f)
        a[h - 1 - i, :] = np.minimum(a[h - 1 - i, :], f)
        a[:, i] = np.minimum(a[:, i], f)
        a[:, w - 1 - i] = np.minimum(a[:, w - 1 - i], f)
    img[y0:y1, x0:x1] = img[y0:y1, x0:x1] * (1 - a) + src * a


def levels(img, lo, hi, gamma=1.0):
    x = np.clip((img - lo) / float(hi - lo), 0, 1)
    x = x ** gamma
    return x * 255.0


def sharpen(img, amount=0.6):
    blur = cv2.GaussianBlur(img, (0, 0), 1.0)
    return np.clip(img + amount * (img - blur), 0, 255)


def atkinson(img):
    """Dithering di Atkinson. Ritorna array bool, True = nero."""
    a = img.astype(np.float32).copy()
    h, w = a.shape
    out = np.zeros((h, w), bool)
    for y in range(h):
        row = a[y]
        for x in range(w):
            old = row[x]
            new = 255.0 if old >= 128 else 0.0
            out[y, x] = new == 0.0
            err = (old - new) / 8.0
            if x + 1 < w: row[x + 1] += err
            if x + 2 < w: row[x + 2] += err
            if y + 1 < h:
                if x > 0: a[y + 1, x - 1] += err
                a[y + 1, x] += err
                if x + 1 < w: a[y + 1, x + 1] += err
            if y + 2 < h:
                a[y + 2, x] += err
    return out


def resize(img, w, h):
    return cv2.resize(img, (w, h), interpolation=cv2.INTER_AREA)


def save_preview(name, bits):
    os.makedirs(PREVIEW, exist_ok=True)
    Image.fromarray(np.where(bits, 0, 255).astype(np.uint8)).save(
        os.path.join(PREVIEW, name + ".png"))


# ---------------------------------------------------------------------------
# Scene
# ---------------------------------------------------------------------------

def scene_dock(g):
    x0, y0, x1, y1 = 537, 14, 999, 254
    img = g[y0:y1, x0:x1].copy()
    # Il protagonista e' "stampato" davanti alla bancarella (locali
    # x 200..237, y 122..233). Ricostruiamo la parete della bancarella
    # con le assi della parte destra e il molo con le assi vicine.
    clone(img, (212, 128, 240, 198), 30)       # parete bancarella
    clone(img, (210, 120, 238, 134), 28)       # bordo del tetto
    clone(img, (196, 118, 213, 198), -40)      # mare/molo a sinistra
    clone(img, (196, 204, 242, 236), 72)       # assi del molo in basso
    img = inpaint(img, [(196, 196, 242, 206)], 4)
    return img


def tile(img, rect, src):
    """Riempie rect ripetendo la texture presa da src (x0,y0,x1,y1)."""
    x0, y0, x1, y1 = rect
    sx0, sy0, sx1, sy1 = src
    patch = img[sy0:sy1, sx0:sx1].copy()
    ph, pw = patch.shape
    for y in range(y0, y1):
        for x in range(x0, x1):
            img[y, x] = patch[(y - y0) % ph, (x - x0) % pw]


def scene_tavern(g):
    x0, y0, x1, y1 = 1046, 14, 1511, 254
    img = g[y0:y1, x0:x1].copy()
    # Box di dialogo (locali x 78..425, y 16..104): lo sostituiamo con
    # il muro di assi verticali preso a sinistra della lanterna.
    tile(img, (76, 12, 428, 108), (138, 110, 162, 186))
    # trave orizzontale sotto il soffitto
    img[12:16, 76:428] = img[12:16, 60:76].mean()
    # mensola di raccordo per nascondere la cucitura
    img[104:108, 76:428] = 25
    img[103, 76:428] = 90
    # protagonista (locali x 64..134, y 108..240)
    tile(img, (64, 106, 136, 188), (138, 110, 162, 186))
    clone(img, (64, 186, 136, 240), 76, 0, 2)
    return img


def scene_beach(g):
    x0, y0, x1, y1 = 22, 375, 492, 606
    img = g[y0:y1, x0:x1].copy()
    # protagonista: assoluto x 205..236, y 497..590 -> locali 183..214, 122..215
    clone(img, (180, 140, 220, 220), 44)
    clone(img, (184, 118, 216, 142), 40)
    return img


def scene_night(g):
    x0, y0, x1, y1 = 534, 374, 999, 612
    img = g[y0:y1, x0:x1].copy()
    # box di dialogo: assoluto x 556..846, y 378..438 -> locali 22..312, 4..64
    sky = np.median(img[80:110, 300:330])
    img[2:68, 18:316] = sky
    rng = np.random.RandomState(7)
    for _ in range(9):
        sx, sy = rng.randint(24, 310), rng.randint(8, 62)
        img[sy, sx] = 220
        img[sy - 1, sx] = img[sy + 1, sx] = 170
        img[sy, sx - 1] = img[sy, sx + 1] = 170
    # protagonista: assoluto x 681..721, y 484..596 -> locali 147..187, 110..222
    clone(img, (144, 108, 192, 200), 50)
    clone(img, (144, 196, 192, 226), -46)
    return img


def scene_title(g):
    x0, y0, x1, y1 = 26, 19, 488, 322
    img = g[y0:y1, x0:x1].copy()
    # testo menu: assoluto x 200..310, y 242..316 -> locali 174..284, 223..297
    # il mare scuro e' uniforme: lo ricostruiamo con le righe del mare a
    # sinistra (stessa altezza, quindi stessi riflessi)
    clone(img, (172, 222, 288, 300), -150, 0, 4)
    return img


def scene_map(g):
    x0, y0, x1, y1 = 25, 731, 489, 983
    return g[y0:y1, x0:x1].copy()


# ---------------------------------------------------------------------------
# Icone (ritagliate dalla finestra "Inventario" del mockup)
# ---------------------------------------------------------------------------

ICON_NAMES = ["bottiglia", "moneta", "chiave", "mappa",
              "piuma", "calamaro", "cappello", "libro"]
ICON_SMALL = 32
ICON_BIG = 44


def icons(g):
    cols = [1070 + c / 2.0 for c in (127, 325, 517, 710)]
    rows = [395 + r / 2.0 for r in (150, 335)]
    small, big = [], []
    for i in range(8):
        cx, cy = cols[i % 4], rows[i // 4]
        x0, y0 = int(cx - 27), int(cy - 27)
        img = g[y0:y0 + 54, x0:x0 + 54].copy()
        img = levels(img, 40, 145)
        # bordo bianco per non ereditare le linee della griglia
        img[:3, :] = 255; img[-3:, :] = 255; img[:, :3] = 255; img[:, -3:] = 255
        for size, lst in ((ICON_SMALL, small), (ICON_BIG, big)):
            r = sharpen(resize(img, size, size), 0.4)
            lst.append(atkinson(r))
    return small, big


# ---------------------------------------------------------------------------
# Font bitmap (DejaVu Sans Mono 11, senza antialias) - Latin-1
# ---------------------------------------------------------------------------

FONT_W, FONT_H = 7, 14


def make_font(path):
    f = ImageFont.truetype(path, 11)
    glyphs = []
    for code in range(32, 256):
        im = Image.new("1", (FONT_W + 4, FONT_H), 1)
        d = ImageDraw.Draw(im)
        d.fontmode = "1"
        ch = bytes([code]).decode("latin-1")
        if code in range(127, 160):
            ch = " "
        if code == 0x85:
            ch = "\u2026"      # puntini di sospensione
        if code == 0x86:
            ch = "\u25bc"      # triangolo "continua"
        d.text((0, 0), ch, font=f, fill=0)
        a = ~np.array(im)[:, :FONT_W]
        glyphs.append(a)
    return glyphs


# ---------------------------------------------------------------------------
# Emissione C
# ---------------------------------------------------------------------------

def packbits(row):
    """PackBits (come UnpackBits del Toolbox) su una riga di byte."""
    out = bytearray()
    i, n = 0, len(row)
    while i < n:
        # cerca una ripetizione
        j = i
        while j + 1 < n and row[j + 1] == row[i] and j - i < 127:
            j += 1
        if j - i >= 2:
            out.append((257 - (j - i + 1)) & 0xFF)
            out.append(row[i])
            i = j + 1
            continue
        # letterali
        start = i
        while i < n and i - start < 128:
            if i + 2 < n and row[i] == row[i + 1] == row[i + 2]:
                break
            i += 1
        out.append(i - start - 1)
        out += bytes(row[start:i])
    return bytes(out)


def bits_to_bytes(bits):
    h, w = bits.shape
    rb = (w + 7) // 8
    pad = np.zeros((h, rb * 8), bool)
    pad[:, :w] = bits
    return np.packbits(pad, axis=1), rb


def c_array(name, data, f):
    f.write("static const unsigned char %s[%d] = {\n" % (name, len(data)))
    for i in range(0, len(data), 20):
        f.write("  " + ",".join(str(b) for b in data[i:i + 20]) + ",\n")
    f.write("};\n")


def main():
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import hero

    g = load_gray()
    scenes = {}
    for name, fn, w, h, lo, hi, gm in [
        ("dock", scene_dock, SCREEN_W, SCENE_H, 30, 215, 1.0),
        ("tavern", scene_tavern, SCREEN_W, SCENE_H, 10, 175, 0.75),
        ("beach", scene_beach, SCREEN_W, SCENE_H, 30, 220, 1.0),
        ("night", scene_night, SCREEN_W, SCENE_H, 10, 200, 1.0),
        ("title", scene_title, SCREEN_W, SCREEN_H, 20, 165, 1.0),
        ("map", scene_map, SCREEN_W, 282, 60, 150, 1.0),
    ]:
        img = fn(g)
        img = resize(img, w, h)
        img = sharpen(levels(img, lo, hi, gm))
        bits = atkinson(img)
        save_preview(name, bits)
        scenes[name] = bits

    small, big = icons(g)
    for i, n in enumerate(ICON_NAMES):
        save_preview("icon_" + n, big[i])

    font = make_font("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf")
    bold = make_font("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf")

    frames = hero.all_frames()

    src = os.path.join(ROOT, "src")
    with open(os.path.join(src, "assets.c"), "w") as f:
        f.write("/* GENERATO da tools/make_assets.py - non modificare a mano */\n")
        f.write('#include "assets.h"\n\n')
        total = 0
        # sfondi compressi PackBits riga per riga
        order = ["title", "map", "dock", "tavern", "beach", "night"]
        for n in order:
            by, rb = bits_to_bytes(scenes[n])
            data = b"".join(packbits(bytes(r)) for r in by)
            total += len(data)
            c_array("bg_" + n, data, f)
        f.write("\nconst PackedBitmap asset_bg[BG_COUNT] = {\n")
        for n in order:
            h, w = scenes[n].shape
            f.write("  { %d, %d, bg_%s },\n" % (w, h, n))
        f.write("};\n\n")
        # icone
        for kind, lst, sz in (("small", small, ICON_SMALL), ("big", big, ICON_BIG)):
            data = b""
            for b in lst:
                by, rb = bits_to_bytes(b)
                data += by.tobytes()
            c_array("icons_" + kind, data, f)
        # maschere delle icone piccole (per usarle come oggetti in scena):
        # trasparente = bianco collegato al bordo
        mdata = b""
        for b in small:
            white = (~b).astype(np.uint8)
            ff = white.copy() * 255
            h, w = ff.shape
            m2 = np.zeros((h + 2, w + 2), np.uint8)
            for (sx, sy) in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)):
                if ff[sy, sx] == 255:
                    cv2.floodFill(ff, m2, (sx, sy), 128)
            mask = ff != 128
            mask = cv2.dilate(mask.astype(np.uint8), np.ones((3, 3), np.uint8)) > 0
            mdata += bits_to_bytes(mask)[0].tobytes()
        c_array("icons_small_mask", mdata, f)
        f.write("const unsigned char *const asset_icon_mask = icons_small_mask;\n")
        f.write("const Bitmap asset_icon_small[ITEM_ICON_COUNT] = {\n")
        rbs = (ICON_SMALL + 7) // 8
        for i in range(8):
            f.write("  { %d, %d, %d, icons_small + %d },\n" % (ICON_SMALL, ICON_SMALL, rbs, i * rbs * ICON_SMALL))
        f.write("};\nconst Bitmap asset_icon_big[ITEM_ICON_COUNT] = {\n")
        rbb = (ICON_BIG + 7) // 8
        for i in range(8):
            f.write("  { %d, %d, %d, icons_big + %d },\n" % (ICON_BIG, ICON_BIG, rbb, i * rbb * ICON_BIG))
        f.write("};\n\n")
        # sprite protagonista: per ogni frame bitmap nero + maschera
        hdata = b""
        for fr in frames:
            b, m = hero.to_bits(fr)
            hdata += bits_to_bytes(b)[0].tobytes() + bits_to_bytes(m)[0].tobytes()
        c_array("hero_data", hdata, f)
        hrb = (hero.W + 7) // 8
        fsz = hrb * hero.H
        f.write("const Sprite asset_hero[HERO_FRAMES] = {\n")
        for i in range(len(frames)):
            f.write("  { %d, %d, %d, hero_data + %d, hero_data + %d },\n" %
                    (hero.W, hero.H, hrb, i * 2 * fsz, i * 2 * fsz + fsz))
        f.write("};\n\n")
        # font
        for fname, gl in (("font_regular", font), ("font_bold", bold)):
            data = bytearray()
            for a in gl:
                for row in a:
                    v = 0
                    for x in range(FONT_W):
                        if row[x]:
                            v |= 0x80 >> x
                    data.append(v)
            c_array(fname + "_data", bytes(data), f)
        f.write("const unsigned char *const asset_font[2] = { font_regular_data, font_bold_data };\n")
        print("sfondi compressi: %d byte" % total)

    with open(os.path.join(src, "assets.h"), "w") as f:
        f.write("""/* GENERATO da tools/make_assets.py - non modificare a mano */
#ifndef ASSETS_H
#define ASSETS_H

typedef struct { short w, h; const unsigned char *packed; } PackedBitmap;
typedef struct { short w, h, rowBytes; const unsigned char *bits; } Bitmap;
typedef struct { short w, h, rowBytes; const unsigned char *bits, *mask; } Sprite;

enum { BG_TITLE, BG_MAP, BG_DOCK, BG_TAVERN, BG_BEACH, BG_NIGHT, BG_COUNT };
#define ITEM_ICON_COUNT 8
#define ICON_SMALL %d
#define ICON_BIG %d
#define HERO_FRAMES %d
#define HERO_W %d
#define HERO_H %d
#define FONT_W %d
#define FONT_H %d
#define FONT_FIRST 32
#define GLYPH_ELLIPSIS 0x85
#define GLYPH_TRIANGLE 0x86

extern const PackedBitmap asset_bg[BG_COUNT];
extern const Bitmap asset_icon_small[ITEM_ICON_COUNT];
extern const Bitmap asset_icon_big[ITEM_ICON_COUNT];
extern const unsigned char *const asset_icon_mask; /* ICON_SMALL x ICON_SMALL ciascuna */
extern const Sprite asset_hero[HERO_FRAMES];
extern const unsigned char *const asset_font[2];

#endif
""" % (ICON_SMALL, ICON_BIG, len(frames), hero.W, hero.H, FONT_W, FONT_H))


if __name__ == "__main__":
    main()
