"""
Darkbass (NINE HANDS | DSP) - commandes d'apres les photos fournies par l'utilisateur (nov. 2026) :
  assets/kit/knob_photo.png        potard noir en aluminium tourne (reflets coniques), repere blanc
  assets/kit/footswitch_photo.png  footswitch : ecrou hexagonal texture + bouton en metal tourne

Potard : le repere blanc de la photo est efface (interpolation angulaire a rayon constant, le long des stries
du metal tourne), pour que seul le repere dessine par le plugin tourne et que les reflets restent fixes.
Les deux sprites recoivent une ombre portee (vers le bas a droite) et une ombre de contact d'apres leur canal alpha
(lumiere en haut a gauche, comme le reste de la pedale).
"""
import os
import numpy as np, cv2
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
KIT = os.path.join(os.path.dirname(HERE), 'assets', 'kit')

KNOB_C, KNOB_RPX = (437.0, 419.5), 350.0          # centre et rayon exterieur du potard dans la photo (px)
IND_R0, IND_R1, IND_W = 0.45, 0.97, 0.034          # repere de la photo : rayons debut / fin et demi-largeur (x rayon)
FS_C, FS_FLATS_PX, FS_BTN_PX = (625.5, 618.0), 718.0, 272.0   # footswitch : centre, distance entre plats, rayon du bouton


def _load(name):
    return np.asarray(Image.open(os.path.join(KIT, name)).convert('RGBA')).astype(np.float32)


def _shift(a, dx, dy):
    return cv2.warpAffine(a, np.float32([[1, 0, dx], [0, 1, dy]]), (a.shape[1], a.shape[0]), borderValue=0)


def _place(src, c, k, n):
    """recadre / met a l'echelle (alpha premultiplie) : point c de la photo au centre d'un carre de n pixels"""
    pm = src.copy(); pm[..., :3] *= pm[..., 3:4] / 255.0
    # reduction en deux temps pour un rendu net sans crenelage
    k1 = min(1.0, k * 2.0)
    n1 = int(round(n * k1 / k))
    M1 = np.float32([[k1, 0, n1 / 2 - c[0] * k1], [0, k1, n1 / 2 - c[1] * k1]])
    big = cv2.warpAffine(pm, M1, (n1, n1), flags=cv2.INTER_AREA, borderValue=0)
    small = cv2.resize(big, (n, n), interpolation=cv2.INTER_AREA)
    a = np.clip(small[..., 3] / 255.0, 0, 1)
    rgb = np.where(a[..., None] > 1e-4, small[..., :3] / np.maximum(a, 1e-4)[..., None], 0)
    return rgb, a


def _with_shadow(rgb, a, size_ref_px, cast_amt=0.62):
    s = size_ref_px
    cast = cv2.GaussianBlur(_shift(a, 0.06 * s, 0.09 * s), (0, 0), 0.06 * s) * cast_amt
    contact = cv2.GaussianBlur(_shift(a, 0.010 * s, 0.016 * s), (0, 0), 0.016 * s) * 0.55
    sh = np.clip(cast + contact, 0, 0.86) * (1 - a)
    alpha = np.clip(a + sh, 0, 1)
    col = np.where(alpha[..., None] > 1e-4, rgb * (a / np.maximum(alpha, 1e-4))[..., None], 0)
    return np.dstack([np.clip(col, 0, 255), alpha * 255]).astype(np.float32)


def knob_clean():
    """photo du potard sans son repere blanc"""
    im = _load('knob_photo.png')
    h, w = im.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    dx, dy = xx - KNOB_C[0], yy - KNOB_C[1]
    r = np.hypot(dx, dy); th = np.arctan2(dx, -dy)           # angle depuis le haut, sens horaire
    R = KNOB_RPX
    # zone du repere : bande angulaire autour du haut, entre IND_R0 et IND_R1, un peu elargie
    half = (IND_W * R + 10.0) / np.maximum(r, 1.0)
    m = (r > IND_R0 * R - 22) & (r < IND_R1 * R + 4) & (np.abs(th) < half)
    out = im.copy()
    ys, xs = np.where(m)
    rr, tt, hh = r[ys, xs], th[ys, xs], half[ys, xs] + 2.0 / np.maximum(r[ys, xs], 1.0)
    def sample(ang):
        sx = KNOB_C[0] + rr * np.sin(ang); sy = KNOB_C[1] - rr * np.cos(ang)
        return np.stack([cv2.remap(im[..., ch], sx.reshape(-1, 1).astype(np.float32), sy.reshape(-1, 1).astype(np.float32),
                                   cv2.INTER_LINEAR).ravel() for ch in range(4)], -1)
    left, right = sample(-hh), sample(hh)
    t = np.clip((tt + hh) / (2 * hh), 0, 1)[:, None]
    fill = left * (1 - t) + right * t
    # grain fin du metal tourne (stries radiales) : bruit allonge dans le sens du rayon
    rng = np.random.default_rng(5)
    noise = rng.normal(0, 1.6, len(ys))[:, None]
    fill[:, :3] += noise
    out[ys, xs] = fill
    return out


def knob_sprite(R_units, AS, pad=1.45):
    """sprite carre de cote 2 * R * pad unites, potard de rayon R_units, sans repere ; renvoie (rgba, cote en unites)"""
    S_ = 2 * R_units * pad
    n = int(round(S_ * AS))
    k = R_units * AS / KNOB_RPX
    rgb, a = _place(knob_clean(), KNOB_C, k, n)
    return _with_shadow(rgb, a, 2 * R_units * AS, 0.66), S_


def footswitch_sprite(flats_units, AS, pad=1.9):
    """sprite carre (cote = flats * pad unites) ; renvoie (rgba, reflet bleu rgba, cote en unites, rayon du bouton en unites)"""
    S_ = flats_units * pad
    n = int(round(S_ * AS))
    k = flats_units * AS / FS_FLATS_PX
    rgb, a = _place(_load('footswitch_photo.png'), FS_C, k, n)
    spr = _with_shadow(rgb, a, flats_units * AS, 0.62)
    # reflet bleu de la LED voisine (au-dessus) sur le flanc haut de l'ecrou : calque separe, en fondu avec la LED
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    dx_, dy_ = (xx - n / 2) / (flats_units * AS / 2), (yy - n / 2) / (flats_units * AS / 2)
    rr = np.hypot(dx_, dy_)
    facing = np.clip(-dy_ / np.maximum(rr, 1e-3), 0, 1) ** 2
    btn = FS_BTN_PX / (FS_FLATS_PX / 2)
    ring = np.clip((rr - btn - 0.02) / 0.05, 0, 1)                    # ecrou seulement (pas le bouton)
    lum = rgb.mean(2) / 255.0
    refl = a * ring * facing * (0.3 + 0.7 * lum)
    blue = np.array([60.0, 140.0, 255.0], np.float32)
    refl_rgba = np.dstack([np.broadcast_to(blue, (n, n, 3)), np.clip(refl * 0.45, 0, 1) * 255]).astype(np.uint8)
    return spr, refl_rgba, S_, FS_BTN_PX * k / AS


if __name__ == '__main__':
    ks, KS = knob_sprite(120, 2)
    fs, fr, FS, br = footswitch_sprite(200, 2)
    print(KS, FS, br)
    for name, s in (('knob', ks), ('fs', fs)):
        bg = np.full(s.shape[:2] + (3,), 20.0)
        a = s[..., 3:4] / 255.0
        Image.fromarray((bg * (1 - a) + s[..., :3] * a).astype(np.uint8)).save(f'/tmp/claude-0/{name}_sprite.png')


# ------------------------------------------------------------------ embase jack (photo fournie par l'utilisateur, nov. 2026)
# assets/kit/jack_photo.png : ecrou hexagonal + rondelle vus de profil, rondelle en haut ; le bord bas (coupe nette)
# est la face collee au boitier. Sur la pedale : IN a droite, OUT a gauche, rondelle vers l'exterieur.
JACK_BOX = (46, 558, 80, 188)                    # x0, x1, y0, y1 de l'objet dans la photo (px)

def jack_sprite(length_units, AS, side):
    """side = +1 (tranche droite, IN) ou -1 (tranche gauche, OUT). Renvoie (rgba float, largeur, hauteur en unites) ;
    le bord colle au boitier est a gauche (side +1) ou a droite (side -1) du sprite."""
    im = _load('jack_photo.png')
    x0, x1, y0, y1 = JACK_BOX
    crop = im[y0:y1, x0:x1]
    # rotation d'un quart de tour (jamais de miroir) : face boitier (bas de la photo) vers le boitier
    rot = np.rot90(crop, k=-1 if side > 0 else 1).copy()       # k=-1 : horaire (bas -> gauche) ; k=1 : anti-horaire (bas -> droite)
    h, w = rot.shape[:2]
    k = length_units * AS / h
    n_w, n_h = max(1, int(round(w * k))), int(round(h * k))
    pm = rot.copy(); pm[..., :3] *= pm[..., 3:4] / 255.0
    small = cv2.resize(pm, (n_w, n_h), interpolation=cv2.INTER_AREA)
    a = np.clip(small[..., 3] / 255.0, 0, 1)
    rgb = np.where(a[..., None] > 1e-4, small[..., :3] / np.maximum(a, 1e-4)[..., None], 0)
    # lumiere de la pedale (haut gauche) : le haut de l'ecrou un peu plus clair, le bas un peu plus sombre
    g = np.linspace(1.08, 0.86, n_h)[:, None, None]
    rgb = np.clip(rgb * g, 0, 255)
    return np.dstack([rgb, a * 255]).astype(np.float32), n_w / AS, n_h / AS


# ------------------------------------------------------------------ LED (photo fournie par l'utilisateur, nov. 2026)
# assets/kit/led_photo.png : LED rouge a lentille facettee dans une bague metal. On ne garde que la lentille
# (et son joint sombre), sans la bague metal, recoloree en bleu ; l'etat eteint garde les facettes, lentille sombre.
LED_C, LED_RPX = (264.5, 281.5), 139.0

def led_sprites(R_units, AS, pad=1.25):
    """renvoie (eteinte rgba, allumee rgba, cote en unites) ; R_units = rayon de la lentille"""
    im = _load('led_photo.png')
    h, w = im.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.hypot(xx - LED_C[0], yy - LED_C[1]) / LED_RPX
    rgb = im[..., :3]
    R_, G_ = rgb[..., 0], rgb[..., 1]
    blue = np.dstack([G_, 0.60 * G_ + 0.35 * R_, R_])                      # rouge -> bleu, coeur presque blanc
    # eteinte : facettes (detail fin) sur un fond bleu nuit, reflet de la lumiere en haut a gauche
    lum = rgb.mean(2)
    detail = lum - cv2.GaussianBlur(lum, (0, 0), 6)
    hl = np.exp(-(((xx - LED_C[0] + 0.38 * LED_RPX) / (0.22 * LED_RPX)) ** 2 + ((yy - LED_C[1] + 0.42 * LED_RPX) / (0.16 * LED_RPX)) ** 2))
    off = np.dstack([np.full_like(lum, 10.0), np.full_like(lum, 16.0), np.full_like(lum, 30.0)]) \
          + (detail * 0.30)[..., None] * np.array([0.6, 0.8, 1.2]) + (hl * 70)[..., None] * np.array([0.85, 0.9, 1.0])
    off = off * (0.85 + 0.15 * (1 - r))[..., None]
    # joint sombre autour de la lentille (sans la bague metal)
    seat = (r > 1.0) & (r <= 1.07)
    a_lens = np.clip((1.0 - r) * LED_RPX, 0, 1)
    a_seat = np.clip((1.07 - r) * LED_RPX, 0, 1)
    def build(col, with_seat):
        c = col.copy()
        if with_seat: c[seat] = 8.0
        a = (a_seat if with_seat else a_lens) * 255
        return np.dstack([np.clip(c, 0, 255), a])
    S_ = 2 * R_units * pad
    n = int(round(S_ * AS)); k = R_units * AS / LED_RPX
    out = []
    for src, seat_on, shadow in ((build(off, True), True, True), (build(blue, False), False, False)):
        rgb_s, a_s = _place(src, LED_C, k, n)
        if shadow:
            # leger creux : ombre interne en haut a gauche + ombre de contact
            spr = _with_shadow(rgb_s, a_s, 2 * R_units * AS, 0.25)
        else:
            spr = np.dstack([rgb_s, a_s * 255]).astype(np.float32)
        out.append(spr)
    return out[0], out[1], S_
