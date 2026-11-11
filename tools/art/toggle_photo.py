"""
Inverseur a levier NINE HANDS | DSP : photos de reference fournies par l'utilisateur (nov. 2026), a utiliser sur toutes les pedales.
  assets/kit/toggle_photo_haut.png  levier en haut   (3840 x 2160, fond transparent)
  assets/kit/toggle_photo_bas.png   levier en bas    (meme cadrage, meme lumiere en haut a gauche)
  assets/kit/toggle_photo_milieu.png levier au milieu (dresse, vu dans l'axe), 1254 x 1254 : utilisee pour la position du milieu
Position du milieu (inverseur 3 positions, ou image intermediaire de bascule d'un inverseur 2 positions) :
composee a partir des deux photos, jamais par miroir : moitie haute de la photo « bas » + moitie basse de la photo
« haut » (le levier n'y passe pas), puis le bout arrondi du levier (pris sur la photo « haut ») pose au centre,
vu dans l'axe. Ombre portee (vers le bas a droite) et ombre de contact ajoutees d'apres le canal alpha.

frames(washer_r_units, size_units, AS) -> [haut, milieu, bas] en RGBA uint8, sprites carres de size_units * AS pixels,
centres sur le centre de la rondelle.
"""
import os
import numpy as np, cv2
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
KIT = os.path.join(os.path.dirname(HERE), 'assets', 'kit')


def _load(name):
    return np.asarray(Image.open(os.path.join(KIT, name)).convert('RGBA')).astype(np.float32)


def _washer(a):
    """centre et rayon de la rondelle : cercle ajuste sur le contour, hors levier (bandeau horizontal central)"""
    al = a[..., 3] > 128
    ys, xs = np.where(al)
    yc0 = 0.5 * (ys.min() + ys.max())
    pts = []
    for y in range(int(ys.min()), int(ys.max()) + 1, 4):
        row = np.where(al[y])[0]
        if len(row) == 0: continue
        pts.append((row.min(), y)); pts.append((row.max(), y))
    pts = np.array(pts, np.float64)
    # garde les points dont la largeur de ligne est grande (rondelle), pas ceux du levier
    widths = {}
    for x, y in pts: widths.setdefault(y, []).append(x)
    good = np.array([(x, y) for y, xx in widths.items() if max(xx) - min(xx) > 0.55 * (xs.max() - xs.min()) for x in xx])
    A = np.c_[2 * good[:, 0], 2 * good[:, 1], np.ones(len(good))]
    b = (good ** 2).sum(1)
    cx, cy, c = np.linalg.lstsq(A, b, rcond=None)[0]
    return cx, cy, np.sqrt(c + cx * cx + cy * cy)


def _shift(a, dx, dy):
    m = np.float32([[1, 0, dx], [0, 1, dy]])
    return cv2.warpAffine(a, m, (a.shape[1], a.shape[0]), flags=cv2.INTER_LINEAR, borderValue=0)


def _middle(up, dn, cu, cd):
    """compose la position du milieu dans le repere de la photo « haut »"""
    # recale la photo « bas » sur la rondelle de la photo « haut »
    dn2 = _shift(dn, cu[0] - cd[0], cu[1] - cd[1])
    hh, ww = up.shape[:2]
    yy, xx = np.mgrid[0:hh, 0:ww].astype(np.float32)
    r = cu[2]
    # fondu vertical autour du centre : au-dessus -> photo « bas » (levier en dessous), en dessous -> photo « haut »
    t = np.clip((yy - (cu[1] - 0.04 * r)) / (0.08 * r), 0, 1)[..., None]
    mid = dn2 * (1 - t) + up * t
    # les deux photos ont le pied du levier au centre : on le couvre avec le bout arrondi vu dans l'axe
    al = up[..., 3] > 128
    ys = np.where(al.any(1))[0]
    top = ys.min()
    ball_h = int(0.40 * r)                            # bout arrondi + debut de la tige
    cols = np.where(al[top:top + ball_h].any(0))[0]
    x0, x1 = cols.min() - 4, cols.max() + 5
    ball = up[top - 4:top + ball_h, x0:x1].copy()
    bh, bw = ball.shape[:2]
    # fond progressif en bas de la tige (elle s'enfonce dans le soufflet)
    fade = np.clip((bh - np.arange(bh)) / (0.30 * bh), 0, 1)[:, None]
    ball[..., 3] *= fade
    # ombre douce du levier dresse sur le soufflet (vers le bas a droite)
    cx, cy = int(cu[0]), int(cu[1])
    by0, bx0 = cy - bh // 2 - int(0.06 * r), cx - bw // 2
    sh = np.zeros((hh, ww), np.float32)
    sh[by0:by0 + bh, bx0:bx0 + bw] = ball[..., 3] / 255.0
    sh = cv2.GaussianBlur(_shift(sh, 0.10 * r, 0.14 * r), (0, 0), 0.05 * r) * 0.55
    mid[..., :3] *= (1 - sh * (mid[..., 3] / 255.0 > 0.5))[..., None]
    reg = mid[by0:by0 + bh, bx0:bx0 + bw]
    a = ball[..., 3:4] / 255.0
    reg[..., :3] = reg[..., :3] * (1 - a) + ball[..., :3] * a
    reg[..., 3:4] = np.maximum(reg[..., 3:4], ball[..., 3:4])
    return mid


def _sprite(src, c, washer_r_units, size_units, AS):
    """recadre la photo sur la rondelle, met a l'echelle, ajoute ombre portee + ombre de contact (lumiere en haut a gauche)"""
    n = int(round(size_units * AS))
    k = washer_r_units * AS / c[2]
    M = np.float32([[k, 0, n / 2 - c[0] * k], [0, k, n / 2 - c[1] * k]])
    # alpha premultiplie pour un redimensionnement propre
    pm = src.copy(); pm[..., :3] *= pm[..., 3:4] / 255.0
    small = cv2.warpAffine(pm, M, (n, n), flags=cv2.INTER_AREA if k < 1 else cv2.INTER_LANCZOS4, borderValue=0)
    if k < 0.5:   # forte reduction : passe par une image intermediaire pour eviter le crenelage
        big_n = int(n / k * 0.25)
        k1 = 0.25
        M1 = np.float32([[k1, 0, big_n / 2 - c[0] * k1], [0, k1, big_n / 2 - c[1] * k1]])
        mid = cv2.warpAffine(pm, M1, (big_n, big_n), flags=cv2.INTER_AREA, borderValue=0)
        small = cv2.resize(cv2.GaussianBlur(mid, (0, 0), 0.5 * (1.0 / (k / k1))), (n, n), interpolation=cv2.INTER_AREA)
    a = np.clip(small[..., 3] / 255.0, 0, 1)
    rgb = np.where(a[..., None] > 1e-4, small[..., :3] / np.maximum(a, 1e-4)[..., None], 0)
    cast = cv2.GaussianBlur(_shift(a, 0.16 * washer_r_units * AS, 0.24 * washer_r_units * AS), (0, 0), 0.16 * washer_r_units * AS) * 0.62
    contact = cv2.GaussianBlur(_shift(a, 0.03 * washer_r_units * AS, 0.05 * washer_r_units * AS), (0, 0), 0.05 * washer_r_units * AS) * 0.55
    sh = np.clip(cast + contact, 0, 0.85) * (1 - a)
    alpha = np.clip(a + sh, 0, 1)
    col = np.where(alpha[..., None] > 1e-4, rgb * (a / np.maximum(alpha, 1e-4))[..., None], 0)
    return np.dstack([np.clip(col, 0, 255), alpha * 255]).astype(np.uint8)


def frames(washer_r_units=22.0, size_units=100.0, AS=2):
    up, dn = _load('toggle_photo_haut.png'), _load('toggle_photo_bas.png')
    cu, cd = _washer(up), _washer(dn)
    if os.path.exists(os.path.join(KIT, 'toggle_photo_milieu.png')):
        # position du milieu : photo fournie par l'utilisateur (levier dresse, vu dans l'axe)
        mid = _load('toggle_photo_milieu.png'); cm = _washer(mid)
    else:
        mid = _middle(up, dn, cu, cd); cm = cu
    return [_sprite(up, cu, washer_r_units, size_units, AS),
            _sprite(mid, cm, washer_r_units, size_units, AS),
            _sprite(dn, cd, washer_r_units, size_units, AS)]


if __name__ == '__main__':
    fr = frames(60.0, 260.0, 2)
    out = np.concatenate([np.dstack([np.full(f.shape[:2] + (3,), 24, np.uint8), np.full(f.shape[:2], 255, np.uint8)]) for f in fr], 1)
    bg = out.astype(np.float32)
    x = 0
    for f in fr:
        a = f[..., 3:4] / 255.0
        bg[:, x:x + f.shape[1], :3] = bg[:, x:x + f.shape[1], :3] * (1 - a) + f[..., :3] * a
        x += f.shape[1]
    Image.fromarray(bg.astype(np.uint8)).save('/tmp/claude-0/toggle_frames.png')
    print('ok', [f.shape for f in fr])
