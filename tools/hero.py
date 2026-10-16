"""Sprite del protagonista in pixel art 1-bit.

Personaggio originale nello stile dei protagonisti delle avventure
LucasArts dei primi anni '90: aspirante pirata allampanato, capelli biondi
raccolti in un codino, nasone, camicia bianca con le maniche a sbuffo,
cintura, pantaloni scuri e stivali.

Metodo (per avere linee nette e regolari a ogni misura):
  1. si disegnano le PARTI (capelli, viso, camicia...) a risoluzione 4x,
     senza contorni, in un sistema di coordinate "di progetto" alto 100;
  2. si riduce alla misura finale scegliendo per ogni pixel la parte
     prevalente nel blocco 4x4;
  3. si generano i contorni neri di 1 pixel: attorno alla sagoma e tra
     parti diverse (viso/capelli, camicia/pantaloni, ...);
  4. si aggiungono i dettagli da 1 pixel (occhio, bocca, fibbia);
  5. ogni parte ha un tono: bianco, nero o un retino fisso.
"""
import math
import numpy as np
from PIL import Image, ImageDraw

SS = 4                          # sovracampionamento del disegno
SX = 1.3                        # allargamento orizzontale del disegno
HEIGHTS = (92, 108, 124)        # altezze generate (prospettiva)

# toni finali
T, K, Wt, LG, DG, MG = 0, 1, 2, 3, 4, 5

# parti: id -> (tono, gruppo per i contorni)
P_NONE = 0
HAIR, TAIL, SKIN, SHIRT, SHADE, BACKARM, LEG, BACKLEG, BOOT, BELT, HAT, SLEEVE = range(1, 13)
PARTS = {
    HAIR: (LG, 1),      # capelli biondi
    TAIL: (LG, 1),      # codino
    SKIN: (Wt, 2),      # viso, collo, mani
    SHIRT: (Wt, 3),     # camicia
    SHADE: (LG, 3),     # ombra sulla camicia (stesso gruppo: nessun contorno)
    SLEEVE: (Wt, 10),   # manica anteriore a sbuffo (contornata sulla camicia)
    BACKARM: (LG, 4),   # braccio posteriore, in ombra
    LEG: (DG, 5),       # gamba anteriore
    BACKLEG: (MG, 6),   # gamba posteriore (piu' chiara: profondita')
    BOOT: (K, 7),       # stivali
    BELT: (K, 8),       # cintura
    HAT: (K, 9),        # tricorno
}


def size_for(h):
    """(larghezza in pixel multipla di 8, altezza) per un'altezza data."""
    s = h / 100.0
    w = int(math.ceil((46 * SX * s + 6) / 8.0)) * 8
    return w, h


class _Pen:
    """Porta le coordinate di progetto (alto 100, piedi a y=99, centro x=20)
    su una tela di w x h pixel."""

    def __init__(self, d, w, h):
        self.d, self.w, self.h, self.s = d, w, h, h / 100.0

    def x(self, x):
        return (x - 20) * SX * self.s + self.w / 2.0

    def y(self, y):
        return (y - 100) * self.s + self.h

    def pts(self, pts):
        return [(self.x(x), self.y(y)) for (x, y) in pts]

    def polygon(self, pts, part):
        self.d.polygon(self.pts(pts), fill=part)

    def ellipse(self, box, part):
        x0, y0, x1, y1 = box
        self.d.ellipse((self.x(x0), self.y(y0), self.x(x1), self.y(y1)), fill=part)

    def thick(self, a, b, wa, wb, part):
        """Segmento spesso (larghezze in unita' di progetto) con estremi tondi."""
        (ax, ay), (bx, by) = a, b
        dx, dy = bx - ax, by - ay
        n = math.hypot(dx, dy) or 1
        px, py = -dy / n, dx / n
        self.polygon([(ax + px * wa / 2, ay + py * wa / 2), (bx + px * wb / 2, by + py * wb / 2),
                      (bx - px * wb / 2, by - py * wb / 2), (ax - px * wa / 2, ay - py * wa / 2)], part)
        for (cx, cy, r) in ((ax, ay, wa / 2), (bx, by, wb / 2)):
            self.ellipse((cx - r, cy - r, cx + r, cy + r), part)


def _joint(x, y, a1, a2, l1, l2):
    a1, a2 = math.radians(a1), math.radians(a2)
    kx, ky = x + math.sin(a1) * l1, y + math.cos(a1) * l1
    return (kx, ky), (kx + math.sin(a2) * l2, ky + math.cos(a2) * l2)


def _boot(d, ank, big):
    x, y = ank
    e = 0.5 if big else 0
    d.polygon([(x - 3 - e, y - 7 - e), (x + 3 + e, y - 7 - e), (x + 3.5 + e, y - 1),
               (x + 7 + e, y + 0.5), (x + 7 + e, y + 3), (x - 3 - e, y + 3)], BOOT)


def _draw_hat(d, oy):
    d.polygon([(14, 8 + oy), (15, 0 + oy), (21, -1 + oy), (27, 0 + oy), (28, 8 + oy)], HAT)
    d.polygon([(8, 7 + oy), (21, 10 + oy), (34, 7 + oy), (32, 12 + oy), (21, 13 + oy), (10, 12 + oy)], HAT)


def _parts(w, h, legs, arms, bob, hat):
    """Disegna le parti a risoluzione SS volte maggiore."""
    W, H = w * SS, h * SS
    im = Image.new("L", (W, H), P_NONE)
    d = _Pen(ImageDraw.Draw(im), W, H)
    oy = bob
    hip = (20, 55 + oy)

    # gamba e braccio posteriori
    hb = (hip[0] - 1.5, hip[1])
    knee, ank = _joint(hb[0], hb[1], legs[0], legs[1], 20, 18)
    d.thick(hb, knee, 7, 6, BACKLEG)
    d.thick(knee, ank, 6, 5, BACKLEG)
    _boot(d, (ank[0], min(ank[1], 96)), False)
    sh = (18, 31 + oy)
    elb, hand = _joint(sh[0], sh[1], -arms[0], -arms[0] * 0.6 - 10, 12, 11)
    d.thick(sh, elb, 6, 5, BACKARM)
    d.thick(elb, hand, 4.5, 3.5, BACKARM)
    d.ellipse((hand[0] - 2, hand[1] - 2, hand[0] + 2, hand[1] + 2), SKIN)

    # codino (dietro la testa e il collo)
    d.polygon([(13, 11 + oy), (16, 12 + oy), (14, 22 + oy), (11, 27 + oy), (9, 26 + oy), (11, 18 + oy)], TAIL)

    # busto: camicia bianca, ombra sulla schiena, cintura
    d.polygon([(13, 27 + oy), (26, 27 + oy), (28, 37 + oy), (26.5, 53 + oy), (14, 53 + oy), (12, 38 + oy)], SHIRT)
    d.polygon([(13, 29 + oy), (16, 29 + oy), (16.5, 52 + oy), (14, 52 + oy), (12.5, 38 + oy)], SHADE)
    d.polygon([(13.5, 51 + oy), (27, 51 + oy), (27, 55 + oy), (13.5, 55 + oy)], BELT)

    # gamba anteriore
    hf = (hip[0] + 2, hip[1])
    knee, ank = _joint(hf[0], hf[1], legs[2], legs[3], 20, 18)
    d.thick(hf, knee, 7.5, 6.5, LEG)
    d.thick(knee, ank, 6.5, 5.5, LEG)
    _boot(d, (ank[0], min(ank[1], 96)), True)

    # collo e testa: viso lungo, nasone, mento pronunciato
    d.polygon([(18, 22 + oy), (22, 22 + oy), (22.5, 28 + oy), (17.5, 28 + oy)], SKIN)
    d.ellipse((13.5, 3.5 + oy, 27.5, 25 + oy), SKIN)
    d.polygon([(25, 11 + oy), (31.5, 17 + oy), (30, 18.5 + oy), (25, 18 + oy)], SKIN)
    d.polygon([(19, 22 + oy), (25, 21 + oy), (24, 25.5 + oy), (19, 25 + oy)], SKIN)
    # capelli biondi pettinati all'indietro
    d.polygon([(12, 13 + oy), (12.5, 6 + oy), (16, 2 + oy), (21, 1 + oy), (26, 2.5 + oy), (28.5, 7 + oy),
               (26, 7.5 + oy), (22, 6.5 + oy), (19, 8 + oy), (17.5, 13 + oy), (16, 18 + oy), (13, 18 + oy)], HAIR)
    if hat:
        _draw_hat(d, oy)

    # braccio anteriore: manica a sbuffo e mano
    sh = (22, 31 + oy)
    elb, hand = _joint(sh[0], sh[1], arms[1], arms[1] * 0.6 + 8, 12, 11)
    d.thick(sh, elb, 8, 6.5, SLEEVE)
    d.thick(elb, hand, 6.5, 4, SLEEVE)
    d.ellipse((hand[0] - 2.2, hand[1] - 2.2, hand[0] + 2.2, hand[1] + 2.2), SKIN)
    return np.array(im)


def _downsample(P, w, h):
    """Per ogni blocco SSxSS sceglie la parte prevalente; le parti sottili
    vincono sul vuoto se coprono almeno 6 sottopixel su 16."""
    B = P.reshape(h, SS, w, SS).transpose(0, 2, 1, 3).reshape(h, w, SS * SS)
    counts = np.stack([(B == pid).sum(axis=2) for pid in range(13)], axis=2)
    nonT = SS * SS - counts[:, :, 0]
    best = counts[:, :, 1:].argmax(axis=2) + 1
    out = np.zeros((h, w), np.uint8)
    out[nonT >= 6] = best[nonT >= 6]
    return out


def _render(parts, extra):
    """Parti -> toni con i contorni; extra = dettagli da 1 pixel [(x, y, tono)]."""
    h, w = parts.shape
    tone = np.zeros((h, w), np.uint8)
    group = np.zeros((h, w), np.uint8)
    for pid, (tn, gr) in PARTS.items():
        tone[parts == pid] = tn
        group[parts == pid] = gr
    solid = parts != 0
    out = tone.copy()
    # contorno esterno: pixel della sagoma che toccano il vuoto
    edge = np.zeros_like(solid)
    edge[1:, :] |= solid[1:, :] & ~solid[:-1, :]
    edge[:-1, :] |= solid[:-1, :] & ~solid[1:, :]
    edge[:, 1:] |= solid[:, 1:] & ~solid[:, :-1]
    edge[:, :-1] |= solid[:, :-1] & ~solid[:, 1:]
    out[edge] = K
    # contorni interni tra gruppi diversi: si scurisce il pixel del gruppo
    # con il numero maggiore (cosi' la linea resta di 1 pixel)
    for dy, dx in ((0, 1), (1, 0)):
        a = group[:h - dy, :w - dx]
        b = group[dy:, dx:]
        diff = (a != b) & (a != 0) & (b != 0)
        ya, xa = np.nonzero(diff & (a > b))
        out[ya, xa] = K
        yb, xb = np.nonzero(diff & (b > a))
        out[yb + dy, xb + dx] = K
    for (x, y, tn) in extra:
        if 0 <= x < w and 0 <= y < h and solid[y, x]:
            out[y, x] = tn
    return out


def _px(w, h, x, y):
    s = h / 100.0
    return (int(round((x - 20) * SX * s + w / 2.0 - 0.5)), int(round((y - 100) * s + h - 0.5)))


def frame(h, legs=(0, 0, 0, 0), arms=(0, 0), bob=0, mouth=False, hat=False, blink=False):
    """legs = (coscia_dietro, stinco_dietro, coscia_avanti, stinco_avanti)."""
    w, h = size_for(h)
    parts = _downsample(_parts(w, h, legs, arms, bob, hat), w, h)
    oy = bob
    extra = []
    ex, ey = _px(w, h, 23.5, 12.5 + oy)                           # occhio
    extra += [(ex - 1, ey, K), (ex, ey, K)] if blink else [(ex, ey, K), (ex, ey + 1, K)]
    bx, by = _px(w, h, 22.5, 10.5 + oy)                           # sopracciglio
    extra += [(bx, by, K), (bx + 1, by, K)]
    mx, my = _px(w, h, 24, 20.5 + oy)                             # bocca
    extra += [(mx, my, K), (mx + 1, my, K)]
    if mouth:
        extra += [(mx, my + 1, K), (mx + 1, my + 1, K)]
    ex_, ey_ = _px(w, h, 16.5, 14 + oy)                           # orecchio
    extra += [(ex_, ey_, K), (ex_, ey_ + 1, K)]
    cx, cy = _px(w, h, 22, 28 + oy)                               # scollo della camicia
    extra += [(cx, cy, K), (cx + 1, cy + 1, K), (cx + 2, cy, K)]
    fx, fy = _px(w, h, 22, 53 + oy)                               # fibbia
    extra += [(fx, fy, Wt), (fx + 1, fy, Wt)]
    return _render(parts, extra)


# ciclo di camminata a 6 fotogrammi
WALK = [
    dict(legs=(-24, -8, 24, 6), arms=(20, -20), bob=1),
    dict(legs=(-12, 4, 12, 26), arms=(10, -10), bob=0),
    dict(legs=(0, 14, 0, 0), arms=(0, 0), bob=0),
    dict(legs=(24, 6, -24, -8), arms=(-20, 20), bob=1),
    dict(legs=(12, 26, -12, 4), arms=(-10, 10), bob=0),
    dict(legs=(0, 0, 0, 14), arms=(0, 0), bob=0),
]
STAND = dict(legs=(-3, -1, 4, 1), arms=(4, -4), bob=0)


def all_frames(h, hat=False):
    """9 fotogrammi: 0 fermo, 1 parla, 2..7 cammina, 8 occhi chiusi.
    Nel gioco il cappello e' uno strato a parte (hat_layers)."""
    fr = [frame(h, hat=hat, **STAND), frame(h, mouth=True, hat=hat, **STAND)]
    fr += [frame(h, hat=hat, **w) for w in WALK]
    fr += [frame(h, blink=True, hat=hat, **STAND)]
    return fr


# fotogrammi in cui la testa e' abbassata di 1 (per posare il cappello)
HEAD_DOWN = [0, 0, 1, 0, 0, 1, 0, 0, 0]


def hat_layers(h):
    """Il tricorno da solo: [testa normale, testa abbassata di 1]."""
    out = []
    w, hh = size_for(h)
    for oy in (0, 1):
        P = Image.new("L", (w * SS, hh * SS), P_NONE)
        _draw_hat(_Pen(ImageDraw.Draw(P), w * SS, hh * SS), oy)
        parts = _downsample(np.array(P), w, hh)
        tx, ty = _px(w, hh, 21, 4 + oy)
        out.append(_render(parts, [(tx, ty, Wt), (tx + 1, ty, Wt)]))
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
