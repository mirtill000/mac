"""
Ridimensionamento "sensibile al contenuto" (seam carving, Avidan & Shamir).

I mockup delle scene nuove sono in 4:3, l'area di gioco del Mac e' 512x240
(circa 2:1). Invece di deformare tutto o tagliare via il faro, si
aggiungono colonne (o si tolgono righe) lungo i percorsi di minima energia:
cielo, mare, roccia uniforme. Le zone protette (personaggi, oggetti
importanti) non vengono mai attraversate.

Le funzioni lavorano su una lista di immagini: i percorsi si calcolano
sulla prima e si applicano identici a tutte (per esempio la scena con e
senza un oggetto raccoglibile, che devono restare allineate).
"""
import numpy as np
import cv2

BIG = 1e9


def energy(img):
    g = cv2.GaussianBlur(img.astype(np.float32), (0, 0), 1.2)
    gx = cv2.Sobel(g, cv2.CV_32F, 1, 0, ksize=3)
    gy = cv2.Sobel(g, cv2.CV_32F, 0, 1, ksize=3)
    return np.abs(gx) + np.abs(gy)


def _seam(E):
    """Percorso verticale (una x per riga) di energia minima."""
    h, w = E.shape
    M = E.astype(np.float64).copy()
    back = np.zeros((h, w), np.int8)
    for y in range(1, h):
        prev = M[y - 1]
        left = np.concatenate(([np.inf], prev[:-1]))
        right = np.concatenate((prev[1:], [np.inf]))
        stack = np.vstack((left, prev, right))
        idx = np.argmin(stack, axis=0)
        back[y] = idx - 1
        M[y] += stack[idx, np.arange(w)]
    xs = np.zeros(h, int)
    xs[-1] = int(np.argmin(M[-1]))
    for y in range(h - 1, 0, -1):
        xs[y - 1] = xs[y] + back[y, xs[y]]
    return xs


def _insert(a, xs, kind):
    """Inserisce una colonna dopo xs[y] in ogni riga."""
    h, w = a.shape
    out = np.zeros((h, w + 1), a.dtype)
    for y in range(h):
        x = xs[y]
        r = a[y]
        if kind == "avg":
            v = (r[x] + r[min(x + 1, w - 1)]) / 2.0
        else:
            v = r[x]
        out[y, :x + 1] = r[:x + 1]
        out[y, x + 1] = v
        out[y, x + 2:] = r[x + 1:]
    return out


def widen(imgs, n, protect=None, spread=40.0):
    """Aggiunge n colonne duplicando i percorsi di minima energia."""
    imgs = [i.astype(np.float32).copy() for i in imgs]
    h, w = imgs[0].shape
    prot = np.zeros((h, w), np.float32) if protect is None else protect.astype(np.float32) * BIG
    used = np.zeros((h, w), np.float32)
    base_e = energy(imgs[0])
    for _ in range(n):
        xs = _seam(base_e + prot + used)
        imgs = [_insert(i, xs, "avg") for i in imgs]
        prot = _insert(prot, xs, "dup")
        base_e = _insert(base_e, xs, "dup")
        used = _insert(used, xs, "dup")
        used[np.arange(h), xs] += spread
        used[np.arange(h), xs + 1] += spread
    return imgs, prot > 0


def shrink_h(imgs, n, protect=None):
    """Toglie n righe lungo i percorsi orizzontali di minima energia."""
    ts = [i.T.astype(np.float32).copy() for i in imgs]
    pt = None if protect is None else protect.T.copy()
    for _ in range(n):
        E = energy(ts[0])
        if pt is not None:
            E = E + pt * BIG
        xs = _seam(E)
        h, w = ts[0].shape
        keep = np.ones((h, w), bool)
        keep[np.arange(h), xs] = False
        ts = [t[keep].reshape(h, w - 1) for t in ts]
        if pt is not None:
            pt = pt[keep].reshape(h, w - 1)
    return [t.T for t in ts], (None if pt is None else pt.T)
