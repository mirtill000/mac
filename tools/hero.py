"""Sprite del protagonista disegnato proceduralmente in pixel art 1-bit.

Valori: 0 trasparente, 1 nero, 2 bianco, 3 grigio chiaro (25%),
4 grigio scuro (75%). I grigi vengono poi resi con retini fissi.
"""
import math
import numpy as np
from PIL import Image, ImageDraw

W, H = 56, 100
SX = 1.45          # allargamento orizzontale del disegno

T, K, Wt, LG, DG, MG = 0, 1, 2, 3, 4, 5


def _limb(d, x, y, ang1, ang2, l1, l2, w1, w2, fill):
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
        d.ellipse((bx - wb / 2, by - wb / 2, bx + wb / 2, by + wb / 2), fill=fill, outline=None)
    return ex, ey, kx, ky


def _boot(d, x, y, ang, fill, cuff):
    # stivale: parte dalla caviglia (x,y), punta verso destra
    a = math.radians(ang)
    pts = [(-4, -8), (4, -8), (4, -1), (8, 0), (8, 3), (-4, 3)]
    ca, sa = math.cos(a), math.sin(a)
    poly = [(x + px * ca + py * sa * 0.3, y + py) for px, py in pts]
    d.polygon(poly, fill=fill, outline=K)
    d.line([(x - 4, y - 8), (x + 4, y - 8)], fill=cuff)


class _Wide:
    """ImageDraw che allarga le coordinate x attorno al centro (x=20)."""

    def __init__(self, d):
        self.d = d

    @staticmethod
    def _x(x):
        return (x - 20) * SX + W / 2

    def _pts(self, pts):
        return [(self._x(x), y) for (x, y) in pts]

    def polygon(self, pts, **kw):
        self.d.polygon(self._pts(pts), **kw)

    def line(self, pts, **kw):
        self.d.line(self._pts(pts), **kw)

    def ellipse(self, box, **kw):
        x0, y0, x1, y1 = box
        self.d.ellipse((self._x(x0), y0, self._x(x1), y1), **kw)

    def rectangle(self, box, **kw):
        x0, y0, x1, y1 = box
        self.d.rectangle((round(self._x(x0)), y0, round(self._x(x1)), y1), **kw)


def frame(legs=(0, 0, 0, 0), arms=(0, 0), bob=0, mouth=False, hat=False):
    """legs = (coscia_dietro, stinco_dietro, coscia_avanti, stinco_avanti)."""
    im = Image.new("L", (W, H), T)
    d = _Wide(ImageDraw.Draw(im))
    oy = bob
    hipy = 56 + oy
    # gamba posteriore (grigio scuro)
    ex, ey, _, _ = _limb(d, 18, hipy, legs[0], legs[1], 18, 15, 8, 7, DG)
    _boot(d, ex, min(ey, 96), 0, DG, LG)
    # braccio posteriore
    _limb(d, 17, 31 + oy, -arms[0], -arms[0] * 0.6 - 8, 12, 11, 6, 5, LG)
    # busto
    d.polygon([(12, 29 + oy), (27, 29 + oy), (29, 43 + oy), (27, 56 + oy), (13, 56 + oy), (11, 43 + oy)],
              fill=Wt, outline=K)
    d.line([(17, 29 + oy), (20, 36 + oy), (23, 29 + oy)], fill=K)
    d.line([(14, 40 + oy), (15, 50 + oy)], fill=LG)
    # cintura
    d.rectangle((12, 53 + oy, 27, 57 + oy), fill=K)
    d.rectangle((20, 54 + oy, 22, 56 + oy), fill=Wt)
    # gamba anteriore (nera)
    ex, ey, _, _ = _limb(d, 22, hipy, legs[2], legs[3], 18, 15, 8, 7, K)
    _boot(d, ex, min(ey, 96), 0, K, Wt)
    # collo e testa
    d.rectangle((18, 24 + oy, 22, 30 + oy), fill=Wt, outline=K)
    d.ellipse((13, 6 + oy, 29, 27 + oy), fill=Wt, outline=K)
    d.polygon([(28, 14 + oy), (31, 19 + oy), (28, 20 + oy)], fill=Wt, outline=K)  # naso
    # capelli lunghi
    d.polygon([(12, 13 + oy), (14, 5 + oy), (20, 2 + oy), (27, 3 + oy), (30, 8 + oy),
               (29, 10 + oy), (25, 8 + oy), (21, 10 + oy), (18, 16 + oy), (18, 26 + oy),
               (16, 34 + oy), (11, 35 + oy), (9, 27 + oy)], fill=MG, outline=K)
    d.line([(15, 9 + oy), (13, 26 + oy)], fill=K)
    # occhio, sopracciglio, bocca
    d.rectangle((25, 13 + oy, 25, 15 + oy), fill=K)
    d.line([(24, 11 + oy), (27, 11 + oy)], fill=K)
    if mouth:
        d.rectangle((25, 21 + oy, 27, 23 + oy), fill=K)
    else:
        d.line([(25, 22 + oy), (27, 22 + oy)], fill=K)
    if hat:
        # cappello da pirata (tricorno)
        d.polygon([(14, 9 + oy), (15, 1 + oy), (21, 0 + oy), (28, 1 + oy), (29, 9 + oy)], fill=K, outline=K)
        d.polygon([(8, 7 + oy), (21, 11 + oy), (35, 7 + oy), (33, 12 + oy), (21, 14 + oy), (10, 12 + oy)],
                  fill=K, outline=K)
        d.line([(10, 9 + oy), (21, 12 + oy), (33, 9 + oy)], fill=Wt)
        d.rectangle((21, 4 + oy, 22, 5 + oy), fill=Wt)
    # braccio anteriore (manica bianca)
    hx, hy, _, _ = _limb(d, 23, 32 + oy, arms[1], arms[1] * 0.6 + 6, 12, 11, 7, 6, Wt)
    d.line([(23 + math.sin(math.radians(arms[1])) * 9 - 3, 32 + oy + 9),
            (23 + math.sin(math.radians(arms[1])) * 9 + 3, 32 + oy + 9)], fill=K)
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


def all_frames():
    """16 fotogrammi: 0 fermo, 1 parla, 2..7 cammina; +8 = con cappello."""
    fr = []
    for hat in (False, True):
        fr += [frame(hat=hat, **STAND), frame(mouth=True, hat=hat, **STAND)]
        fr += [frame(hat=hat, **w) for w in WALK]
    return fr


def to_bits(a):
    """(nero, maschera) applicando i retini ai grigi."""
    h, w = a.shape
    yy, xx = np.mgrid[0:h, 0:w]
    light = ((xx % 2 == 0) & (yy % 2 == 0))          # 25% nero
    dark = ~((xx % 2 == 1) & (yy % 2 == 1))          # 75% nero
    mid = (xx + yy) % 2 == 0                         # 50% nero
    black = (a == K) | ((a == LG) & light) | ((a == DG) & dark) | ((a == MG) & mid)
    mask = a != T
    # alone bianco di 1 pixel: stacca il personaggio dagli sfondi retinati
    halo = mask.copy()
    halo[1:, :] |= mask[:-1, :]
    halo[:-1, :] |= mask[1:, :]
    halo[:, 1:] |= mask[:, :-1]
    halo[:, :-1] |= mask[:, 1:]
    return black, halo


if __name__ == "__main__":
    import sys
    fr = all_frames()
    out = np.full((H + 4, (W + 4) * len(fr)), 200, np.uint8)
    for i, f in enumerate(fr):
        b, m = to_bits(f)
        tile = np.where(m, np.where(b, 0, 255), 160)
        out[2:2 + H, i * (W + 4) + 2:i * (W + 4) + 2 + W] = tile
    Image.fromarray(out).resize((out.shape[1] * 3, out.shape[0] * 3), Image.NEAREST).save(sys.argv[1])
