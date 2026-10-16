"""
Scene della Parte II e III, dal secondo mockup (reference/mockup2.png).

Ogni pannello del mockup e' in 4:3 e contiene un box di testo e il
protagonista "stampato". Per ogni scena:
  1. si ritaglia la zona utile (spesso togliendo la fascia del box);
  2. si cancella il protagonista clonando lo sfondo vicino;
  3. gli oggetti raccoglibili si cancellano in una seconda copia: nel
     gioco vengono ridisegnati come "toppe" finche' non si raccolgono;
  4. se serve, si allarga fino al 2:1 dello schermo con seams.py;
  5. si riscala a 512x240 e si converte in 1-bit.

Tutte le coordinate sono locali al pannello (vedi PANELS).
"""
import os

import numpy as np
import cv2
from PIL import Image

import seams

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MOCKUP2 = os.path.join(ROOT, "reference", "mockup2.png")

SCREEN_W, SCENE_H = 512, 240
RATIO = 1.877            # larghezza/altezza prima del riscalamento (verticale -12%)

# pannelli del mockup: x0, y0, x1, y1 (assoluti)
PANELS = {
    "faro":       (20, 25, 493, 400),
    "faro_in":    (525, 26, 1009, 400),
    "grotta":     (1042, 26, 1513, 400),
    "grotta_in":  (20, 526, 493, 870),
    "nave":       (525, 526, 1009, 870),
    "nave_in":    (1042, 527, 1513, 872),
}


def load():
    return np.array(Image.open(MOCKUP2).convert("L")).astype(np.float32)


def panel(g, name):
    x0, y0, x1, y1 = PANELS[name]
    return g[y0:y1, x0:x1].copy()


def clone(img, rect, dx, dy=0, feather=3):
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


def inpaint(img, rects, radius=4):
    mask = np.zeros(img.shape, np.uint8)
    for (x0, y0, x1, y1) in rects:
        mask[y0:y1, x0:x1] = 255
    out = cv2.inpaint(np.clip(img, 0, 255).astype(np.uint8), mask, radius, cv2.INPAINT_TELEA)
    return out.astype(np.float32)


def sky_fill(img, rect, src_cols, seed=1):
    """Riempie rect con il cielo: il tono sfuma dall'alto (righe appena sopra)
    al basso (righe appena sotto), con un po' di grana presa dalle colonne
    src_cols, per non lasciare una macchia liscia."""
    x0, y0, x1, y1 = rect
    top = np.median(img[max(y0 - 6, 0):y0, x0:x1], axis=0) if y0 > 0 else None
    bot = np.median(img[y1:y1 + 6, x0:x1], axis=0)
    top = cv2.GaussianBlur(top.reshape(1, -1), (0, 0), 8).ravel() if top is not None else bot
    bot = cv2.GaussianBlur(bot.reshape(1, -1), (0, 0), 8).ravel()
    t = np.linspace(0, 1, y1 - y0)[:, None]
    ref = img[y0:y1, src_cols[0]:src_cols[1]]
    std = float(np.std(ref - np.median(ref, axis=1)[:, None])) * 0.4
    rng = np.random.RandomState(seed)
    noise = cv2.GaussianBlur(rng.randn(y1 - y0, x1 - x0).astype(np.float32), (0, 0), 1.0)
    noise *= std / max(noise.std(), 1e-3)
    fill = top[None, :] * (1 - t) + bot[None, :] * t + noise
    a = np.ones_like(fill)
    for i in range(4):
        f = (i + 1) / 5.0
        a[:, i] = np.minimum(a[:, i], f)
        a[:, -1 - i] = np.minimum(a[:, -1 - i], f)
        a[i, :] = np.minimum(a[i, :], f)
        a[-1 - i, :] = np.minimum(a[-1 - i, :], f)
    img[y0:y1, x0:x1] = img[y0:y1, x0:x1] * (1 - a) + fill * a


def row_fill(img, rect, src, seed=2):
    """Riempie rect riga per riga con il tono mediano delle colonne src
    (x0, x1) alla stessa altezza, piu' grana: per superfici come un tavolo."""
    x0, y0, x1, y1 = rect
    ref = img[y0:y1, src[0]:src[1]]
    prof = np.median(ref, axis=1)
    std = float(np.std(ref - prof[:, None])) * 0.6
    rng = np.random.RandomState(seed)
    n = cv2.GaussianBlur(rng.randn(y1 - y0, x1 - x0).astype(np.float32), (0, 0), sigmaX=2.0, sigmaY=0.6)
    n *= std / max(n.std(), 1e-3)
    fill = prof[:, None] + n
    a = np.ones_like(fill)
    for i in range(3):
        f = (i + 1) / 4.0
        a[:, i] = np.minimum(a[:, i], f)
        a[:, -1 - i] = np.minimum(a[:, -1 - i], f)
        a[i, :] = np.minimum(a[i, :], f)
        a[-1 - i, :] = np.minimum(a[-1 - i, :], f)
    img[y0:y1, x0:x1] = img[y0:y1, x0:x1] * (1 - a) + fill * a


def rmask(shape, rects):
    m = np.zeros(shape, bool)
    for (x0, y0, x1, y1) in rects:
        m[y0:y1, x0:x1] = True
    return m


# ---------------------------------------------------------------------------
# Le scene. Ognuna ritorna (immagine, {oggetto: immagine con l'oggetto},
# maschera protetta, righe da togliere)
# ---------------------------------------------------------------------------

def scene_faro(g):
    img = panel(g, "faro")[10:375]          # da qui in poi y locale - 10
    Y = lambda y: y - 10
    # angoli arrotondati dello schermo del mockup
    img = inpaint(img, [(0, 0, 22, Y(26)), (452, 0, 473, Y(26))], 3)
    # box di testo nel cielo
    sky_fill(img, (14, Y(24), 316, Y(104)), (392, 470), seed=3)
    # protagonista: busto davanti alla roccia, gambe sul sentiero
    clone(img, (230, Y(236), 282, Y(300)), -58)
    clone(img, (230, Y(296), 282, Y(358)), -58)
    prot = rmask(img.shape, [(312, 0, 386, Y(240)), (238, Y(140), 336, Y(215)),
                             (430, Y(205), 473, Y(300)), (95, Y(268), 182, Y(362)),
                             (102, Y(198), 148, Y(240)), (44, Y(172), 102, Y(214)),
                             (336, Y(205), 375, Y(250))])
    return img, {}, prot, 40


def scene_faro_in(g):
    full = panel(g, "faro_in")
    img = full[92:374].copy()                # niente box: la lente tocca il bordo
    Y = lambda y: y - 92
    clone(img, (116, Y(200), 160, Y(312)), 46)
    # oggetti raccoglibili: la lanterna sul pavimento e la carta sul tavolo
    with_obj = img.copy()
    clone(img, (364, Y(272), 414, Y(322)), 46)          # lanterna: pavimento
    row_fill(img, (364, Y(318), 414, Y(344)), (414, 440))  # ...e tavolino
    row_fill(img, (192, Y(304), 302, Y(354)), (304, 330))  # carta: piano del tavolo
    with_obj_lant = img.copy()
    with_obj_lant[Y(266):Y(348), 362:414] = with_obj[Y(266):Y(348), 362:414]
    with_obj_carta = img.copy()
    with_obj_carta[Y(298):Y(356), 190:304] = with_obj[Y(298):Y(356), 190:304]
    prot = rmask(img.shape, [(244, 0, 376, Y(305)), (36, 0, 128, Y(330))])
    return img, {"lanterna": with_obj_lant, "carta": with_obj_carta}, prot, 0


def scene_grotta(g):
    img = panel(g, "grotta")[100:372].copy()
    Y = lambda y: y - 100
    # protagonista: a sinistra roccia, a destra il buio della grotta
    clone(img, (100, Y(212), 128, Y(300)), -44)
    clone(img, (126, Y(212), 152, Y(300)), 40)
    clone(img, (100, Y(296), 152, Y(336)), 70)
    prot = rmask(img.shape, [(312, Y(258), 362, Y(320)), (352, Y(268), 471, Y(352)),
                             (62, Y(302), 106, Y(345))])
    return img, {}, prot, 0


def scene_grotta_in(g):
    full = panel(g, "grotta_in")
    img = full[92:342].copy()
    Y = lambda y: y - 92
    clone(img, (128, Y(186), 172, Y(302)), 48)
    with_obj = img.copy()
    clone(img, (54, Y(242), 92, Y(278)), 40)              # teschio: rocce vicine
    with_teschio = img.copy()
    with_teschio[Y(238):Y(282), 50:96] = with_obj[Y(238):Y(282), 50:96]
    return img, {"teschio": with_teschio}, None, 0


def scene_nave(g):
    full = panel(g, "nave")
    img = full[92:342].copy()
    # la luna era dietro il box: la riportiamo in un cielo libero
    moon = full[20:82, 378:442].copy()
    a = np.zeros(moon.shape, np.float32)
    yy, xx = np.mgrid[0:moon.shape[0], 0:moon.shape[1]]
    d = np.hypot(xx - 31.5, yy - 30.5)
    a = np.clip((31 - d) / 4.0, 0, 1)
    y0, x0 = 4, 384
    img[y0:y0 + moon.shape[0], x0:x0 + moon.shape[1]] = \
        img[y0:y0 + moon.shape[0], x0:x0 + moon.shape[1]] * (1 - a) + moon * a
    return img, {}, None, 0


def scene_nave_in(g):
    full = panel(g, "nave_in")
    img = full[92:342].copy()
    Y = lambda y: y - 92
    clone(img, (82, Y(194), 128, Y(258)), -46)
    clone(img, (82, Y(254), 128, Y(316)), -46)
    return img, {}, None, 0


SCENES = [
    ("faro", scene_faro, 10, 245, 1.0),
    ("faro_in", scene_faro_in, 10, 240, 1.0),
    ("grotta", scene_grotta, 10, 235, 0.95),
    ("grotta_in", scene_grotta_in, 10, 230, 0.9),
    ("nave", scene_nave, 10, 235, 1.0),
    ("nave_in", scene_nave_in, 10, 225, 0.85),
]


def process(g, ma, name, fn, lo, hi, gm):
    """Ritorna (bits dello sfondo, {oggetto: (x, y, bits)}, grigio finale)."""
    img, objs, prot, shrink = fn(g)
    keys = list(objs.keys())
    stack = [img] + [objs[k] for k in keys]
    if shrink:
        stack, prot = seams.shrink_h(stack, shrink, prot)
    h, w = stack[0].shape
    want = int(round(RATIO * h))
    if want > w:
        stack, prot = seams.widen(stack, want - w, prot)
    out = []
    # contrasto locale (CLAHE): in 1-bit le tessiture di roccia, legno e
    # mare restano leggibili invece di diventare macchie nere o grigie.
    # La curva si calcola sulla prima immagine e si applica uguale alle altre.
    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(4, 4))
    rs = [np.clip(ma.resize(s, SCREEN_W, SCENE_H), 0, 255) for s in stack]
    c0 = clahe.apply(rs[0].astype(np.uint8)).astype(np.float32)
    for r in rs:
        r = np.clip(c0 + (r - rs[0]), 0, 255)
        out.append(ma.sharpen(ma.levels(r, lo, hi, gm)))
    base = out[0]
    patches = {}
    # lo sfondo si retina con gli oggetti presenti; dentro il rettangolo di
    # ogni oggetto si usa la versione senza. La toppa e' il rettangolo con
    # l'oggetto: cosi' i bordi combaciano esattamente.
    if keys:
        full = base.copy()
        for i, k in enumerate(keys):
            full += out[1 + i] - base
        with_bits = ma.atkinson(full)
        without = ma.atkinson(base)
        bits = with_bits.copy()
        for i, k in enumerate(keys):
            diff = np.abs(out[1 + i] - base) > 6
            ys, xs = np.nonzero(diff)
            y0, y1 = max(ys.min() - 2, 0), min(ys.max() + 3, SCENE_H)
            x0 = max((xs.min() - 2) // 8 * 8, 0)
            x1 = min(((xs.max() + 3) + 7) // 8 * 8, SCREEN_W)
            bits[y0:y1, x0:x1] = without[y0:y1, x0:x1]
            patches[k] = (x0, y0, with_bits[y0:y1, x0:x1].copy())
    else:
        bits = ma.atkinson(base)
    return bits, patches, base


# ---------------------------------------------------------------------------
# Icone dei nuovi oggetti
# ---------------------------------------------------------------------------

def _sticker(crop, mask, size, invert=False):
    """Oggetto ritagliato su fondo bianco, con contorno nero."""
    img = 255.0 - crop if invert else crop.copy()
    m = cv2.resize(mask.astype(np.uint8) * 255, (size, size), interpolation=cv2.INTER_AREA) > 100
    r = cv2.resize(img, (size, size), interpolation=cv2.INTER_AREA)
    return r, m


def _poly_src(g, pts):
    """Ritaglio quadrato attorno al poligono (coordinate assolute) e maschera."""
    pts = np.array(pts, np.int32)
    x0, y0 = pts.min(axis=0) - 2
    x1, y1 = pts.max(axis=0) + 3
    side = max(x1 - x0, y1 - y0)
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    X0, Y0 = cx - side // 2, cy - side // 2
    crop = g[Y0:Y0 + side, X0:X0 + side].copy()
    m = np.zeros(crop.shape, np.uint8)
    cv2.fillPoly(m, [pts - [X0, Y0]], 1)
    return crop, m > 0


def icon_sources(g):
    """[(nome, immagine grigia quadrata, maschera)]: ritagli dal mockup."""
    out = []
    # bussola: dall'inventario del mockup (chiara su nero -> invertita)
    c = g[414:448, 347:381].copy()
    out.append(("bussola", 255 - c, None))
    # lanterna: dal pavimento del faro
    lp = [(40, 12), (46, 12), (47, 22), (57, 30), (56, 35), (54, 35), (54, 60), (58, 64),
          (58, 72), (22, 72), (23, 64), (26, 60), (26, 35), (23, 33), (28, 24), (38, 22)]
    out.append(("lanterna",) + _poly_src(g, [(x + 875, y + 290) for x, y in lp]))
    # carta nautica: dal tavolo del faro
    cp = [(198, 307), (288, 304), (298, 346), (203, 352)]
    out.append(("carta",) + _poly_src(g, [(x + 525, y + 26) for x, y in cp]))
    # teschio: dalla grotta
    sp = [(12, 22), (16, 13), (24, 11), (31, 15), (33, 24), (32, 32), (27, 37), (18, 37), (11, 30)]
    out.append(("teschio",) + _poly_src(g, [(x + 70, y + 762) for x, y in sp]))
    return out


def make_icon(ma, src, mask, size):
    r = cv2.resize(src, (size - 4, size - 4), interpolation=cv2.INTER_AREA)
    if mask is not None:
        m = cv2.resize(mask.astype(np.uint8) * 255, (size - 4, size - 4),
                       interpolation=cv2.INTER_AREA) > 110
        # tieni solo la componente piu' grande e riempi i buchi
        n, lab, st, _ = cv2.connectedComponentsWithStats(m.astype(np.uint8), connectivity=8)
        if n > 1:
            big = 1 + int(np.argmax(st[1:, cv2.CC_STAT_AREA]))
            m = lab == big
        ff = (~m).astype(np.uint8)
        h, w = ff.shape
        mm = np.zeros((h + 2, w + 2), np.uint8)
        cv2.floodFill(ff, mm, (0, 0), 2)
        m = ff != 2
        r = levels_local(r, m)
        r = np.where(m, r, 255.0)
        edge = cv2.dilate(m.astype(np.uint8), np.ones((3, 3), np.uint8)) > 0
        r = np.where(edge & ~m, 0.0, r)
    else:
        r = levels_local(r, np.ones(r.shape, bool))
    full = np.full((size, size), 255.0, np.float32)
    full[2:-2, 2:-2] = r
    return ma.atkinson(ma.sharpen(full, 0.4))


def levels_local(r, m):
    v = r[m]
    if v.size == 0:
        return r
    lo, hi = np.percentile(v, 3), np.percentile(v, 97)
    return np.clip((r - lo) / max(hi - lo, 1) * 255.0, 0, 255)


def flicker_frames(ma, gray_scene, rect, center, levels, radius):
    """Varianti di luce di una zona (lanterna, candela, lente del faro)."""
    x0, y0, x1, y1 = rect
    yy, xx = np.mgrid[0:SCENE_H, 0:SCREEN_W]
    fall = np.clip(1.0 - np.hypot(xx - center[0], (yy - center[1]) * 1.2) / radius, 0, 1)
    frames = []
    for d in levels:
        v = ma.atkinson(np.clip(gray_scene + d * fall, 0, 255))
        frames.append(v[y0:y1, x0:x1])
    return frames
