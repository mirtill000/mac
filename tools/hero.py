"""Sprite del protagonista disegnato proceduralmente in pixel art 1-bit.

Il personaggio e' disegnato in un sistema di coordinate "di progetto"
(alto 100, piedi a y=99) e poi trasformato alla dimensione voluta: cosi'
ogni misura ha linee nette di 1 pixel invece di un'immagine riscalata.

Valori: 0 trasparente, 1 nero, 2 bianco, 3 grigio chiaro (25%),
4 grigio scuro (75%), 5 grigio medio (50%). I grigi diventano retini fissi,
come negli sfondi.
"""
import math
import numpy as np
from PIL import Image, ImageDraw

SX = 1.45                      # allargamento orizzontale del disegno
HEIGHTS = (88, 100, 112, 124)  # altezze generate (prospettiva)

T, K, Wt, LG, DG, MG = 0, 1, 2, 3, 4, 5


def size_for(h):
    """(larghezza in pixel multipla di 8, altezza) per un'altezza data."""
    s = h / 100.0
    w = int(math.ceil((44 * SX * s + 6) / 8.0)) * 8
    return w, h


class _Pen:
    """ImageDraw che porta le coordinate di progetto alla misura finale."""

    def __init__(self, d, w, h):
        self.d, self.w, self.h, self.s = d, w, h, h / 100.0

    def _x(self, x):
        return (x - 20) * SX * self.s + self.w / 2.0

    def _y(self, y):
        return (y - 100) * self.s + self.h

    def _pts(self, pts):
        return [(self._x(x), self._y(y)) for (x, y) in pts]

    def polygon(self, pts, **kw):
        self.d.polygon(self._pts(pts), **kw)

    def line(self, pts, **kw):
        self.d.line(self._pts(pts), **kw)

    def ellipse(self, box, **kw):
        x0, y0, x1, y1 = box
        self.d.ellipse((self._x(x0), self._y(y0), self._x(x1), self._y(y1)), **kw)

    def rectangle(self, box, **kw):
        x0, y0, x1, y1 = box
        self.d.rectangle((round(self._x(x0)), round(self._y(y0)),
                          round(self._x(x1)), round(self._y(y1))), **kw)


def _limb(d, x, y, ang1, ang2, l1, l2, w1, w2, fill, shade=None):
    """Arto a due segmenti; angoli in gradi da verticale (positivo = avanti)."""
    a1 = math.radians(ang1)
    a2 = math.radians(ang2)
    kx, ky = x + math.sin(a1) * l1, y + math.cos(a1) * l1
    ex, ey = kx + math.sin(a2) * l2, ky + math.cos(a2) * l2
    for (ax, ay, bx, by, wa, wb) in [(x, y, kx, ky, w1, w2), (kx, ky, ex, ey, w2, w2 - 1)]:
        dx, dy = bx - ax, by - ay
        n = math.hypot(dx, dy) or 1
        px, py = -dy / n, dx / n
        poly = [(ax + px * wa / 2, ay + py * wa / 2), (bx + px * wb / 2, by + py * wb / 2),
                (bx - px * wb / 2, by - py * wb / 2), (ax - px * wa / 2, ay - py * wa / 2)]
        d.polygon(poly, fill=fill, outline=K)
        if shade is not None:
            # ombra sul lato posteriore dell'arto
            sp = [(ax + px * wa / 2, ay + py * wa / 2), (bx + px * wb / 2, by + py * wb / 2),
                  (bx + px * wb / 6, by + py * wb / 6), (ax + px * wa / 6, ay + py * wa / 6)]
            d.polygon(sp, fill=shade, outline=K)
    return ex, ey, kx, ky


def _boot(d, x, y, fill, cuff):
    """Stivale: parte dalla caviglia (x,y), la punta guarda a destra."""
    pts = [(-4, -8), (4, -8), (4, -1), (8, 0), (8, 3), (-4, 3)]
    d.polygon([(x + px, y + py) for px, py in pts], fill=fill, outline=K)
    d.line([(x - 4, y - 8), (x + 4, y - 8)], fill=cuff)


def _hand(d, x, y, r=2.2):
    d.ellipse((x - r, y - r, x + r, y + r), fill=Wt, outline=K)


def frame(h, legs=(0, 0, 0, 0), arms=(0, 0), bob=0, mouth=False, hat=False, blink=False):
    """legs = (coscia_dietro, stinco_dietro, coscia_avanti, stinco_avanti).

    Look: capelli corti scuri e mossi con le tempie brizzolate, occhiali dalla
    montatura scura, barba corta sale e pepe, sorriso, giacca blu (retino
    scuro) sopra la camicia bianca aperta, pantaloni e scarpe scuri."""
    w, h = size_for(h)
    im = Image.new("L", (w, h), T)
    d = _Pen(ImageDraw.Draw(im), w, h)
    oy = bob
    hipy = 57 + oy

    # gamba e braccio posteriori (in ombra)
    ex, ey, _, _ = _limb(d, 18, hipy, legs[0], legs[1], 18, 15, 8, 7, DG)
    _boot(d, ex, min(ey, 96), DG, DG)
    hx, hy, _, _ = _limb(d, 17, 31 + oy, -arms[0], -arms[0] * 0.6 - 8, 12, 11, 6, 5, K)
    _hand(d, hx, hy)

    # giacca blu (retino scuro) con la camicia bianca che si vede davanti
    d.polygon([(11, 29 + oy), (28, 29 + oy), (30, 44 + oy), (29, 60 + oy), (12, 60 + oy), (10, 44 + oy)],
              fill=DG, outline=K)
    d.polygon([(21, 29 + oy), (28, 29 + oy), (29, 37 + oy), (26, 50 + oy), (22, 40 + oy)],
              fill=Wt, outline=K)                                          # camicia
    d.line([(22, 29 + oy), (24, 37 + oy), (23, 44 + oy)], fill=K)          # bavero
    d.line([(21, 31 + oy), (21, 56 + oy)], fill=K)                         # profilo della giacca
    d.rectangle((24, 50 + oy, 25, 51 + oy), fill=K)                        # bottone
    d.line([(14, 44 + oy), (19, 44 + oy)], fill=K)                         # tasca

    # gamba anteriore (pantaloni scuri)
    ex, ey, kx, ky = _limb(d, 22, hipy, legs[2], legs[3], 18, 15, 8, 7, K)
    _boot(d, ex, min(ey, 96), K, K)

    # collo con il colletto aperto della camicia
    d.rectangle((18, 24 + oy, 23, 30 + oy), fill=Wt, outline=K)
    d.polygon([(18, 27 + oy), (22, 31 + oy), (25, 27 + oy), (24, 32 + oy), (19, 32 + oy)], fill=Wt, outline=K)

    # testa
    d.ellipse((13, 5 + oy, 30, 27 + oy), fill=Wt, outline=K)
    d.polygon([(29, 13 + oy), (32, 18 + oy), (29, 19 + oy)], fill=Wt, outline=K)        # naso
    d.ellipse((15, 12 + oy, 19, 18 + oy), fill=Wt, outline=K)                          # orecchio
    # barba corta sale e pepe: guancia, mascella e mento
    d.polygon([(15, 17 + oy), (19, 19 + oy), (23, 20 + oy), (27, 20 + oy), (30, 20 + oy),
               (30, 24 + oy), (27, 27 + oy), (21, 27 + oy), (16, 24 + oy)], fill=MG, outline=K)
    # sorriso (o bocca aperta quando parla)
    if mouth:
        d.polygon([(23, 20 + oy), (30, 20 + oy), (29, 25 + oy), (25, 25 + oy)], fill=K, outline=K)
        d.line([(24, 21 + oy), (29, 21 + oy)], fill=Wt)                                # denti
    else:
        # sorriso largo con i denti in vista
        d.polygon([(23, 20 + oy), (30, 20 + oy), (28, 23 + oy), (25, 23 + oy)], fill=Wt, outline=K)
    d.line([(23, 19 + oy), (30, 19 + oy)], fill=K)                                     # baffi
    # capelli corti, scuri e mossi, tempie grigie
    d.polygon([(13, 15 + oy), (12, 9 + oy), (14, 5 + oy), (18, 2 + oy), (23, 1 + oy), (28, 2 + oy),
               (30, 5 + oy), (29, 9 + oy), (27, 8 + oy), (24, 9 + oy), (20, 9 + oy), (18, 12 + oy),
               (16, 12 + oy), (15, 16 + oy)], fill=DG, outline=K)
    for cx, cy in ((16, 4), (21, 2), (26, 3), (29, 6), (14, 9)):
        d.ellipse((cx - 1.5, cy - 1 + oy, cx + 1.5, cy + 1.5 + oy), fill=K, outline=None)  # riccioli
    d.polygon([(14, 11 + oy), (16, 11 + oy), (16, 16 + oy), (14, 16 + oy)], fill=LG, outline=None)  # tempia
    # occhiali dalla montatura scura
    d.rectangle((23, 11 + oy, 29, 16 + oy), fill=Wt, outline=K)
    d.line([(23, 12 + oy), (18, 13 + oy)], fill=K)                                     # stanghetta
    if blink:
        d.line([(25, 14 + oy), (27, 14 + oy)], fill=K)
    else:
        d.rectangle((26, 13 + oy, 26, 14 + oy), fill=K)
    d.line([(23, 10 + oy), (28, 9 + oy)], fill=K)                                      # sopracciglio
    if hat:
        # cappello da pirata (tricorno)
        d.polygon([(14, 8 + oy), (15, 0 + oy), (21, -1 + oy), (28, 0 + oy), (29, 8 + oy)], fill=K, outline=K)
        d.polygon([(8, 6 + oy), (21, 10 + oy), (35, 6 + oy), (33, 11 + oy), (21, 13 + oy), (10, 11 + oy)],
                  fill=K, outline=K)
        d.line([(10, 8 + oy), (21, 11 + oy), (33, 8 + oy)], fill=Wt)
        d.rectangle((21, 3 + oy, 22, 4 + oy), fill=Wt)
    # braccio anteriore: manica della giacca e mano
    hx, hy, _, _ = _limb(d, 23, 32 + oy, arms[1], arms[1] * 0.6 + 6, 12, 11, 7, 6, DG)
    _hand(d, hx, hy)
    return np.array(im)


# ciclo di camminata a 6 fotogrammi
WALK = [
    dict(legs=(-22, -10, 22, 10), arms=(18, -18), bob=1),
    dict(legs=(-12, 0, 12, 22), arms=(10, -10), bob=0),
    dict(legs=(0, 10, 0, 0), arms=(0, 0), bob=0),
    dict(legs=(22, 10, -22, -10), arms=(-18, 18), bob=1),
    dict(legs=(12, 22, -12, 0), arms=(-10, 10), bob=0),
    dict(legs=(0, 0, 0, 10), arms=(0, 0), bob=0),
]
STAND = dict(legs=(-4, -2, 5, 2), arms=(4, -4), bob=0)
FRAMES_PER_HAT = 9       # fermo, parla, 6 camminata, occhi chiusi


def all_frames(h):
    """18 fotogrammi: 0 fermo, 1 parla, 2..7 cammina, 8 occhi chiusi;
    +9 = con il cappello in testa."""
    fr = []
    for hat in (False, True):
        fr += [frame(h, hat=hat, **STAND), frame(h, mouth=True, hat=hat, **STAND)]
        fr += [frame(h, hat=hat, **w) for w in WALK]
        fr += [frame(h, blink=True, hat=hat, **STAND)]
    return fr


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
        for f in all_frames(h)[:9]:
            b, m = to_bits(f)
            tiles.append(np.where(m, np.where(b, 0, 255), 160).astype(np.uint8))
    W = max(t.shape[1] for t in tiles) + 4
    H = max(t.shape[0] for t in tiles) + 4
    out = np.full((H * len(HEIGHTS), W * 9), 200, np.uint8)
    for i, t in enumerate(tiles):
        r, c = divmod(i, 9)
        out[r * H + H - 2 - t.shape[0]:r * H + H - 2, c * W + 2:c * W + 2 + t.shape[1]] = t
    Image.fromarray(out).resize((out.shape[1] * 2, out.shape[0] * 2), Image.NEAREST).save(sys.argv[1])
