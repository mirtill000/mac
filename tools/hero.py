"""Sprite del protagonista in pixel art 1-bit.

Testa, busto e braccia vengono dal protagonista del mockup (la versione
grande nella Taberna): si ritaglia la sagoma con un poligono tracciato a
mano, si riduce alla misura voluta, si normalizza la luce e si converte in
cinque toni resi con retini fissi (stabili durante l'animazione), con un
contorno nero attorno alla sagoma.

Gambe e stivali (che nel mockup sono coperti dal bancone) sono disegnati
qui per poter camminare: si disegnano a risoluzione 4x, si riducono
scegliendo per ogni pixel la parte prevalente e si contornano a 1 pixel.
"""
import math
import os
import numpy as np
import cv2
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MOCKUP = os.path.join(ROOT, "reference", "mockup.png")

HEIGHTS = (76, 92, 108, 172)        # altezze nominali: prospettiva all'aperto, mezzo busto in Taberna
UPPER_RATIO = 0.56              # testa+busto (fino alla cintura) / altezza

# toni finali
T, K, Wt, LG, DG, MG = 0, 1, 2, 3, 4, 5

# --- protagonista del mockup (Taberna) -------------------------------------
SRC_BOX = (1104, 116, 1184, 260)          # ritaglio nel mockup
SRC_TOP, SRC_BELT = 8, 104                # cima dei capelli, fondo della cintura
SRC_HIP_X, SRC_HIP_W = 41, 30             # centro e larghezza dei pantaloni
SRC_EYE = (56, 30, 59, 35)                # occhio (x0, y0, x1, y1)
SRC_MOUTH = (57, 44, 63, 45)              # bocca
SRC_HEAD = (36, 70)                       # testa: x sinistra, x destra
SILHOUETTE = [
    (36, 14), (40, 10), (48, 8), (56, 8), (63, 9), (68, 12), (70, 16), (70, 21),
    (66, 22), (66, 28), (69, 35), (69, 38), (64, 40), (64, 44), (62, 48), (58, 51), (53, 52), (53, 56),
    (57, 58), (61, 62), (63, 66), (65, 72), (65, 95), (66, 110), (68, 120), (67, 128), (62, 131),
    (58, 128), (57, 112), (56, 100), (56, 104), (29, 104), (24, 104), (23, 118), (22, 128), (19, 131),
    (13, 131), (10, 125), (10, 100), (8, 92), (8, 85), (10, 72), (13, 64), (18, 60),
    (26, 57), (26, 48), (30, 40), (33, 30), (34, 20)]
TONE_TH = (48, 95, 150, 200)              # soglie: nero, scuro, medio, chiaro, bianco

_src_cache = {}


def _source():
    if "g" not in _src_cache:
        g = np.array(Image.open(MOCKUP).convert("L")).astype(np.float32)
        x0, y0, x1, y1 = SRC_BOX
        src = g[y0:y1, x0:x1]
        S = 8
        m = Image.new("L", ((x1 - x0) * S, (y1 - y0) * S), 0)
        ImageDraw.Draw(m).polygon([(x * S, y * S) for x, y in SILHOUETTE], fill=255)
        m = np.array(m.resize((x1 - x0, y1 - y0), Image.BOX)).astype(np.float32) / 255
        _src_cache["g"], _src_cache["m"] = src, m
    return _src_cache["g"], _src_cache["m"]


def _scale_for(h):
    return UPPER_RATIO * h / float(SRC_BELT - SRC_TOP)


def _upper(h):
    """Testa, busto e braccia alla misura h: (toni, scala)."""
    src, m = _source()
    sc = _scale_for(h)
    sh, sw = src.shape
    w, hh = int(round(sw * sc)), int(round(sh * sc))
    gs = cv2.resize(src, (w, hh), interpolation=cv2.INTER_AREA)
    ms = cv2.resize(m, (w, hh), interpolation=cv2.INTER_AREA) > 0.5
    lo, hi = np.percentile(gs[ms], 4), np.percentile(gs[ms], 92)
    gs = np.clip((gs - lo) / (hi - lo), 0, 1) * 255
    t = np.full((hh, w), Wt, np.uint8)
    t[gs < TONE_TH[3]] = LG
    t[gs < TONE_TH[2]] = MG
    t[gs < TONE_TH[1]] = DG
    t[gs < TONE_TH[0]] = K
    t[~ms] = T
    # pulizia: un pixel diverso da tutti gli 8 vicini prende il tono prevalente
    for _ in range(2):
        tt = t.copy()
        for y in range(1, hh - 1):
            for x in range(1, w - 1):
                if not ms[y, x]:
                    continue
                nb = t[y - 1:y + 2, x - 1:x + 2].flatten().tolist()
                nb.pop(4)
                if t[y, x] not in nb:
                    vals = [v for v in nb if v]
                    if vals:
                        tt[y, x] = max(set(vals), key=vals.count)
        t = tt
    _outline(t)
    return t, sc


def _outline(t):
    solid = t != T
    edge = np.zeros_like(solid)
    edge[1:, :] |= solid[1:, :] & ~solid[:-1, :]
    edge[:-1, :] |= solid[:-1, :] & ~solid[1:, :]
    edge[:, 1:] |= solid[:, 1:] & ~solid[:, :-1]
    edge[:, :-1] |= solid[:, :-1] & ~solid[:, 1:]
    t[edge] = K


def size_for(h):
    """(larghezza multipla di 8, altezza della tela) per l'altezza nominale h.
    La tela ha in cima un margine per il cappello."""
    sc = _scale_for(h)
    src_w = SRC_BOX[2] - SRC_BOX[0]
    w = int(math.ceil((src_w * sc + 18) / 8.0)) * 8
    return w, h + int(round(SRC_TOP * sc))



# --- gambe disegnate (camminata) --------------------------------------------
SS = 4
LEG, BACKLEG, BOOT = 1, 2, 3
LEG_TONES = {LEG: (DG, 1), BACKLEG: (K, 2), BOOT: (K, 3)}


def _thick(d, a, b, wa, wb, part):
    (ax, ay), (bx, by) = a, b
    dx, dy = bx - ax, by - ay
    n = math.hypot(dx, dy) or 1
    px, py = -dy / n, dx / n
    d.polygon([(ax + px * wa / 2, ay + py * wa / 2), (bx + px * wb / 2, by + py * wb / 2),
               (bx - px * wb / 2, by - py * wb / 2), (ax - px * wa / 2, ay - py * wa / 2)], fill=part)
    for (cx, cy, r) in ((ax, ay, wa / 2), (bx, by, wb / 2)):
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=part)


def _legs(w, H, hip_x, hip_y, sc, legs):
    """Gambe alla misura finale; legs = (coscia, stinco) dietro e avanti, in gradi."""
    im = Image.new("L", (w * SS, H * SS), 0)
    d = ImageDraw.Draw(im)
    tw = SRC_HIP_W * sc                      # larghezza dei pantaloni
    L = (H - 1 - hip_y)                      # dall'anca alla suola
    thigh, shin, boot_h = L * 0.48, L * 0.42, L * 0.10
    for part, dx, (a1, a2) in ((BACKLEG, -tw * 0.2, legs[0:2]), (LEG, tw * 0.2, legs[2:4])):
        hx, hy = (hip_x + dx) * SS, (hip_y - 2) * SS
        r1, r2 = math.radians(a1), math.radians(a2)
        kx, ky = hx + math.sin(r1) * thigh * SS, hy + math.cos(r1) * thigh * SS
        ax, ay = kx + math.sin(r2) * shin * SS, ky + math.cos(r2) * shin * SS
        ay = min(ay, (H - 1 - boot_h) * SS)
        _thick(d, (hx, hy), (kx, ky), tw * 0.5 * SS, tw * 0.42 * SS, part)
        _thick(d, (kx, ky), (ax, ay), tw * 0.42 * SS, tw * 0.36 * SS, part)
        # stivale con la punta in avanti
        bw = tw * 0.38 * SS
        d.polygon([(ax - bw / 2, ay - boot_h * 1.6 * SS), (ax + bw / 2, ay - boot_h * 1.6 * SS),
                   (ax + bw / 2, ay), (ax + bw * 1.15, ay + boot_h * 0.25 * SS),
                   (ax + bw * 1.15, (H - 0.5) * SS), (ax - bw / 2, (H - 0.5) * SS)], fill=BOOT)
    P = np.array(im)
    B = P.reshape(H, SS, w, SS).transpose(0, 2, 1, 3).reshape(H, w, SS * SS)
    counts = np.stack([(B == p).sum(axis=2) for p in range(4)], axis=2)
    best = counts[:, :, 1:].argmax(axis=2) + 1
    parts = np.where(SS * SS - counts[:, :, 0] >= 6, best, 0).astype(np.uint8)
    tone = np.zeros((H, w), np.uint8)
    group = np.zeros((H, w), np.uint8)
    for p, (tn, gr) in LEG_TONES.items():
        tone[parts == p] = tn
        group[parts == p] = gr
    _outline(tone)
    # contorno tra gamba anteriore (grigia) e il resto
    for dy, dx in ((0, 1), (1, 0)):
        a = group[:H - dy, :w - dx]
        b = group[dy:, dx:]
        diff = (a != b) & (a != 0) & (b != 0)
        ya, xa = np.nonzero(diff & (a == 1))
        tone[ya, xa] = K
        yb, xb = np.nonzero(diff & (b == 1))
        tone[yb + dy, xb + dx] = K
    # risvolto chiaro degli stivali
    return tone


# --- fotogrammi --------------------------------------------------------------

def _paste(dst, src, x, y):
    h, w = src.shape
    H, W = dst.shape
    x0, y0 = max(0, x), max(0, y)
    x1, y1 = min(W, x + w), min(H, y + h)
    if x0 >= x1 or y0 >= y1:
        return
    s = src[y0 - y:y1 - y, x0 - x:x1 - x]
    d = dst[y0:y1, x0:x1]
    d[s != T] = s[s != T]


def frame(h, legs=(0, 0, 0, 0), bob=0, mouth=False, blink=False, **_):
    w, H = size_for(h)
    up, sc = _upper(h)
    top = 0                                   # la sorgente parte gia' dal margine
    ux = int(round(w / 2.0 - SRC_HIP_X * sc))
    hip_y = int(round(SRC_BELT * sc)) + bob
    hip_x = ux + SRC_HIP_X * sc
    out = _legs(w, H, hip_x, hip_y, sc, legs)
    upc = up.copy()
    # dettagli del viso
    if blink or mouth:
        def box(b):
            return [int(round(v * sc)) for v in b]
        if blink:
            x0, y0, x1, y1 = box(SRC_EYE)
            region = upc[y0:y1 + 1, x0:x1 + 1]
            region[region != T] = Wt
            upc[(y0 + y1) // 2 + 1, x0:x1 + 1] = K
        if mouth:
            x0, y0, x1, y1 = box(SRC_MOUTH)
            upc[y1 + 1, x0 + 1:x1] = K
            upc[y1 + 2, x0 + 2:x1 - 1] = K
    _paste(out, upc, ux, top + bob)
    return out


# ciclo di camminata a 6 fotogrammi: (coscia, stinco) dietro e avanti
WALK = [
    dict(legs=(-22, -6, 22, 4), bob=1),
    dict(legs=(-11, 6, 11, 24), bob=0),
    dict(legs=(0, 14, 0, 0), bob=0),
    dict(legs=(22, 4, -22, -6), bob=1),
    dict(legs=(11, 24, -11, 6), bob=0),
    dict(legs=(0, 0, 0, 14), bob=0),
]
STAND = dict(legs=(-3, -1, 4, 1), bob=0)


def all_frames(h, hat=False):
    """9 fotogrammi: 0 fermo, 1 parla, 2..7 cammina, 8 occhi chiusi."""
    fr = [frame(h, **STAND), frame(h, mouth=True, **STAND)]
    fr += [frame(h, **w) for w in WALK]
    fr += [frame(h, blink=True, **STAND)]
    if hat:
        hl = hat_layers(h)
        fr = [np.where(hl[HEAD_DOWN[i]] != T, hl[HEAD_DOWN[i]], f) for i, f in enumerate(fr)]
    return fr


# fotogrammi in cui la testa e' abbassata di 1 (per posare il cappello)
HEAD_DOWN = [0, 0, 1, 0, 0, 1, 0, 0, 0]


def hat_layers(h):
    """Il tricorno da solo, posato sulla testa: [testa normale, testa abbassata]."""
    w, H = size_for(h)
    sc = _scale_for(h)
    ux = w / 2.0 - SRC_HIP_X * sc
    hx0, hx1 = SRC_HEAD
    cx = ux + (hx0 + hx1) / 2.0 * sc + 1 * sc
    hw = (hx1 - hx0) * sc
    out = []
    for oy in (0, 1):
        im = Image.new("L", (w * SS, H * SS), 0)
        d = ImageDraw.Draw(im)
        by = (SRC_TOP + 10) * sc + oy            # tesa del cappello
        pts_crown = [(cx - hw * 0.40, by), (cx - hw * 0.36, by - 12 * sc), (cx - hw * 0.1, by - 15 * sc),
                     (cx + hw * 0.1, by - 15 * sc), (cx + hw * 0.36, by - 12 * sc), (cx + hw * 0.40, by)]
        pts_brim = [(cx - hw * 0.80, by - 6 * sc), (cx - hw * 0.4, by - 1 * sc), (cx, by), (cx + hw * 0.4, by - 1 * sc),
                    (cx + hw * 0.80, by - 6 * sc), (cx + hw * 0.66, by + 3 * sc), (cx, by + 5 * sc),
                    (cx - hw * 0.66, by + 3 * sc)]
        for pts in (pts_crown, pts_brim):
            d.polygon([(x * SS, y * SS) for x, y in pts], fill=1)
        P = np.array(im)
        B = P.reshape(H, SS, w, SS).transpose(0, 2, 1, 3).reshape(H, w, SS * SS)
        t = np.where((B == 1).sum(axis=2) >= 6, K, T).astype(np.uint8)
        # profilo chiaro della tesa e teschietto
        ys, xs = np.nonzero(t == K)
        if len(ys):
            yb = int(round(by + 2.5 * sc))
            row = np.nonzero(t[yb] == K)[0] if 0 <= yb < H else []
            for x in row[2:-2]:
                t[yb, x] = Wt
            sy, sx = int(round(by - 8 * sc)), int(round(cx))
            if 0 <= sy < H:
                t[sy, sx] = Wt
                t[sy, sx + 1] = Wt
        out.append(t)
    return out


def to_bits(a):
    """(nero, maschera) applicando i retini ai grigi."""
    h, w = a.shape
    yy, xx = np.mgrid[0:h, 0:w]
    light = ((xx % 2 == 0) & (yy % 2 == 0))          # 25% nero
    dark = ~((xx % 2 == 1) & (yy % 2 == 1))          # 75% nero
    mid = (xx + yy) % 2 == 0                         # 50% nero
    black = (a == K) | ((a == LG) & light) | ((a == DG) & dark) | ((a == MG) & mid)
    return black, a != T


# gabbiano in volo: due fotogrammi (ali su / ali giu')
GULL = [
    ["#.........#",
     ".#.......#.",
     "..##.#.##..",
     "....###....",
     ".....#....."],
    ["...........",
     "....###....",
     ".##..#..##.",
     "#.........#",
     "..........."],
]


def gull_bits():
    out = []
    for f in GULL:
        a = np.array([[c == "#" for c in row] for row in f])
        pad = np.zeros((a.shape[0], 16), bool)
        pad[:, :a.shape[1]] = a
        out.append(pad)
    return out


if __name__ == "__main__":
    import sys
    tiles = []
    for h in HEIGHTS:
        fr = all_frames(h)
        hats = hat_layers(h)
        fr.append(np.where(hats[0] != T, hats[0], fr[0]))
        for f in fr:
            b, m = to_bits(f)
            tiles.append(np.where(m, np.where(b, 0, 255), 160).astype(np.uint8))
    n = 10
    W = max(t.shape[1] for t in tiles) + 4
    H = max(t.shape[0] for t in tiles) + 4
    out = np.full((H * len(HEIGHTS), W * n), 200, np.uint8)
    for i, t in enumerate(tiles):
        r, c = divmod(i, n)
        out[r * H + H - 2 - t.shape[0]:r * H + H - 2, c * W + 2:c * W + 2 + t.shape[1]] = t
    Image.fromarray(out).resize((out.shape[1] * 2, out.shape[0] * 2), Image.NEAREST).save(sys.argv[1])
