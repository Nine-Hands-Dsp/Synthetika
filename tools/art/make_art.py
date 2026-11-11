"""
Synthetika (NINE HANDS | DSP) - facade, format double plein cadre 2350 x 1800 px (repere 1175 x 900 unites, AS = 2 px / unite).
Disposition reprise du Soklo GS-5 (bandeau haut : presets + INPUT / MIX / OUTPUT ; six modules en colonnes ;
bandeau bas : nom + footswitch + logo), couleurs propres a Synthetika. Lumiere unique en haut a gauche a 45 deg.
Pieces : potards (calques de Fog Z), footswitch et jacks (kit photo NINE HANDS | DSP), touches et interrupteurs rendus ici.
Le plugin dessine textes, graduations, logo, ecrans et LED en vectoriel ; ce script produit l'apercu et le fond.
Usage : make_art.py [--fond]  (--fond : fond du plugin sans les pieces mobiles ni les textes)
"""
import os, json, sys
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE); A = os.path.join(ROOT, 'assets')
sys.path.insert(0, HERE)
import photo_controls as pc

FOND = '--fond' in sys.argv
AS = 2
FW, FH = 1175.0, 900.0
W, H = int(FW * AS), int(FH * AS)
yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)

# ------------------------------------------------------------------ geometrie (unites)
JACK_L = 124.0 * 0.8                # embases reduites de 20 % (demande de l'utilisateur)
JACK_P = JACK_L * 108 / 512
BX0, BX1 = JACK_P - 1.5, FW - JACK_P + 1.5
R_PL = 10.0                          # rayon des plaques
R_OUT = 17.0 + R_PL                  # boitier : coins concentriques aux plaques (cadre de 17)
IX0, IX1 = BX0 + 17, BX1 - 17
IY0, IY1 = 17.0, FH - 17
GAP = 6.0
TOP = (IX0, IY0, IX1, IY0 + 150)
MOD_Y0, MOD_Y1 = TOP[3] + GAP, TOP[3] + GAP + 562
BOT = (IX0, MOD_Y1 + GAP, IX1, IY1)
SYN_W = 318.0
COL_W = (IX1 - IX0 - SYN_W - 5 * GAP) / 5
MODX = [IX0]
for i in range(5): MODX.append(MODX[-1] + (SYN_W if i == 0 else COL_W) + GAP)
MODW = [SYN_W] + [COL_W] * 5
MODC = [x + w / 2 for x, w in zip(MODX, MODW)]
JACK_Y = (MOD_Y0 + MOD_Y1) / 2
def my(o): return MOD_Y0 + o
KR = 32.0      # decalage vertical dans un module (repere du GS-5)

PAL = dict(top=(50, 50, 56), syn=(126, 98, 186), dist=(214, 92, 120), mod=(64, 166, 170), ring=(236, 148, 58),
           crush=(140, 156, 182), verb=(156, 190, 92))
MODCOL = [PAL['syn'], PAL['dist'], PAL['mod'], PAL['ring'], PAL['crush'], PAL['verb']]
INK = (28, 26, 32); SILK = (236, 230, 216); AMBER = (255, 176, 64)
FS_PINK = (255, 84, 160)            # halo du footswitch BYPASS (rose)

# ------------------------------------------------------------------ outils
def sdf_rrect(x0, y0, x1, y1, r, X=None, Y=None):
    X = xx if X is None else X; Y = yy if Y is None else Y
    x0, y0, x1, y1, r = [v * AS for v in (x0, y0, x1, y1, r)]
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2; hx, hy = (x1 - x0) / 2 - r, (y1 - y0) / 2 - r
    qx = np.abs(X - cx) - hx; qy = np.abs(Y - cy) - hy
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r

def bandnoise(s1, s2, seed, shape=(H, W)):
    r = np.random.default_rng(seed).standard_normal(shape).astype(np.float32)
    a = cv2.GaussianBlur(r, (0, 0), s1) - cv2.GaussianBlur(r, (0, 0), s2)
    return a / a.std()

def shade(h, strength, lightK=1.6):
    gx = cv2.Sobel(h, cv2.CV_32F, 1, 0, ksize=3) / 8; gy = cv2.Sobel(h, cv2.CV_32F, 0, 1, ksize=3) / 8
    nx, ny, nz = -gx * strength, -gy * strength, np.ones_like(h)
    nn = np.sqrt(nx * nx + ny * ny + nz * nz); nx /= nn; ny /= nn; nz /= nn
    l = np.array([-1, -1, lightK], np.float32); l /= np.linalg.norm(l)
    diff = nx * l[0] + ny * l[1] + nz * l[2]
    hv = np.array([l[0], l[1], l[2] + 1]); hv /= np.linalg.norm(hv)
    spec = np.clip(nx * hv[0] + ny * hv[1] + nz * hv[2], 0, 1) ** 40
    return diff / l[2], spec

def edge_normal(d):
    gx = cv2.Sobel(d, cv2.CV_32F, 1, 0, ksize=3) / 8; gy = cv2.Sobel(d, cv2.CV_32F, 0, 1, ksize=3) / 8
    gn = np.sqrt(gx * gx + gy * gy) + 1e-6
    return gx / gn, gy / gn

def over(dst, spr, cx_u, cy_u, size_u=None):
    s = spr.astype(np.float32)
    if size_u is not None:
        n_ = int(round(size_u * AS))
        if s.shape[1] != n_:
            pm = s.copy(); pm[..., :3] *= pm[..., 3:4] / 255
            pm = cv2.resize(pm, (n_, int(round(n_ * s.shape[0] / s.shape[1]))), interpolation=cv2.INTER_AREA)
            a_ = pm[..., 3:4] / 255
            pm[..., :3] = np.where(a_ > 1e-4, pm[..., :3] / np.maximum(a_, 1e-4), 0)
            s = pm
    h_, w_ = s.shape[:2]
    x0 = int(round(cx_u * AS - w_ / 2)); y0 = int(round(cy_u * AS - h_ / 2))
    xa, ya, xb, yb = max(0, x0), max(0, y0), min(W, x0 + w_), min(H, y0 + h_)
    if xa >= xb or ya >= yb: return
    src = s[ya - y0:yb - y0, xa - x0:xb - x0]; d = dst[ya:yb, xa:xb]
    a_s = src[..., 3:4] / 255; a_d = d[..., 3:4] / 255
    a_o = a_s + a_d * (1 - a_s)
    d[..., :3] = np.where(a_o > 1e-4, (src[..., :3] * a_s + d[..., :3] * a_d * (1 - a_s)) / np.maximum(a_o, 1e-4), 0)
    d[..., 3:4] = a_o * 255

def load(name): return np.asarray(Image.open(os.path.join(A, name)).convert('RGBA')).astype(np.float32)
def shift(img, dx, dy): return cv2.warpAffine(img, np.float32([[1, 0, dx], [0, 1, dy]]), (img.shape[1], img.shape[0]), borderValue=0)
def rot(img, deg):
    h_, w_ = img.shape[:2]
    M = cv2.getRotationMatrix2D((w_ / 2, h_ / 2), -deg, 1.0)
    pm = img.copy(); pm[..., :3] *= pm[..., 3:4] / 255
    r = cv2.warpAffine(pm, M, (w_, h_), flags=cv2.INTER_CUBIC, borderValue=0)
    a_ = np.clip(r[..., 3:4], 0, 255) / 255
    r[..., :3] = np.where(a_ > 1e-4, r[..., :3] / np.maximum(a_, 1e-4), 0); r[..., 3:4] = a_ * 255
    return r

# ------------------------------------------------------------------ boitier + plaques
d_out = sdf_rrect(BX0, 0, BX1, FH, R_OUT)
u = (xx / W + yy / H) / 2
lf = (1.05 - 0.10 * u)[..., None]
h_pc = bandnoise(0.8, 1.8, 6) * 0.10 + bandnoise(4, 9, 7) * 0.18 + np.clip(1 - np.abs(bandnoise(3.6, 7, 8)) / 0.5, 0, 1) ** 2 * 0.22
dpc, spc = shade(h_pc, 3.0)
encl = np.array([30, 30, 33], np.float32) * lf * (0.92 + 0.08 * dpc[..., None]) + spc[..., None] * 8
t = np.clip(-d_out / 14.0, 0, 1); ex, ey = edge_normal(d_out)
slope = np.sqrt(np.clip(1 - t ** 2, 0, 1))
encl *= (1 + slope * 0.45 * np.clip((-ex - ey) / np.sqrt(2), -1, 1) - (1 - t) ** 3 * 0.30)[..., None]
# grain fin de la coque (peinture poudre) : bruit tres fin, un peu plus marque que sur les plaques
gr_f = np.random.default_rng(21).standard_normal((H, W)).astype(np.float32)
gr_f = 0.7 * cv2.GaussianBlur(gr_f, (0, 0), 0.6) + 0.3 * cv2.GaussianBlur(gr_f, (0, 0), 1.3)
gr_f /= gr_f.std()
encl *= (1 + 0.055 * gr_f)[..., None]
rgb = encl.copy()

# plaques peintes (texture poudre tres fine), legerement en relief sur le boitier
h_pl = bandnoise(0.7, 1.6, 11) * 0.10 + bandnoise(3, 7, 12) * 0.12
dpl, spl = shade(h_pl, 2.4)
plates = [(TOP, PAL['top'])] + [((MODX[i], MOD_Y0, MODX[i] + MODW[i], MOD_Y1), MODCOL[i]) for i in range(6)] + [(BOT, PAL['top'])]
amb = np.zeros((H, W), np.float32); inplate = np.zeros((H, W), np.float32)
for (x0, y0, x1, y1), col in plates:
    d = sdf_rrect(x0, y0, x1, y1, R_PL)
    a = np.clip(0.5 - d, 0, 1)
    sh = cv2.GaussianBlur(np.clip(0.5 - sdf_rrect(x0 + 1.6, y0 + 2.2, x1 + 1.6, y1 + 2.2, R_PL), 0, 1), (0, 0), 3.0)
    amb = np.maximum(amb, sh); inplate = np.maximum(inplate, a)
    base = np.array(col, np.float32) * lf * (0.94 + 0.06 * dpl[..., None]) + spl[..., None] * 6
    base *= (1 + 0.022 * gr_f)[..., None]          # grain fin, plus discret sur les plaques peintes
    tt = np.clip(-d / 5.0, 0, 1); nx_, ny_ = edge_normal(d)
    base *= (1 + np.sqrt(np.clip(1 - tt ** 2, 0, 1)) * 0.28 * np.clip((-nx_ - ny_) / np.sqrt(2), -1, 1))[..., None]
    rgb = rgb * (1 - a[..., None]) + base * a[..., None]
rgb *= (1 - amb * 0.55 * (1 - inplate))[..., None]
alpha = np.clip(0.5 - d_out, 0, 1)
canvas = np.dstack([np.clip(rgb, 0, 255), alpha * 255]).astype(np.float32)

# ------------------------------------------------------------------ couche vectorielle (textes, traits) : apercu
FB = os.path.join(A, 'Poppins-Bold.ttf'); FM = os.path.join(A, 'Poppins-Medium.ttf')
vec = Image.new('RGBA', (W, H), (0, 0, 0, 0)); vd = ImageDraw.Draw(vec)
texts = []
TXT_SY = 1.25          # polices etirees en hauteur seulement (demande de l'utilisateur) : le plugin dessine avec scale (1, 1.25)
def _glyphs(s, em, col, font, track):
    f = ImageFont.truetype(font, int(round(em * AS)))
    widths = [f.getlength(c) for c in s]; tr = track * em * AS
    total = sum(widths) + tr * (len(s) - 1)
    hh = int(em * AS * 2); im = Image.new('RGBA', (int(total) + 6, hh), (0, 0, 0, 0)); d = ImageDraw.Draw(im); cx_ = 3
    for c, w_ in zip(s, widths):
        d.text((cx_, hh / 2), c, font=f, fill=tuple(col) + (255,), anchor='lm'); cx_ += w_ + tr
    return im.resize((im.width, int(round(hh * TXT_SY))), Image.LANCZOS), total
TEXT_SY = 1.0           # pas d'etirement vertical
TEXT_K = 1.12           # textes de la partie centrale (modules) : police un peu plus grande
def text(s, em, x, y, col=INK, font=FB, track=0.04, anchor='mm', sy=None):
    if sy is None: sy = TEXT_SY
    if MOD_Y0 < y < MOD_Y1: em = em * TEXT_K
    f = ImageFont.truetype(font, int(round(em * AS)))
    widths = [f.getlength(c) for c in s]; tr = track * em * AS
    total = sum(widths) + tr * (len(s) - 1)
    hh = int(em * AS * 2); ww = int(total) + 8
    im = Image.new('RGBA', (ww, hh), (0, 0, 0, 0)); d = ImageDraw.Draw(im); x0 = 4
    for c, w_ in zip(s, widths):
        d.text((x0, hh / 2), c, font=f, fill=tuple(col) + (255,), anchor='lm'); x0 += w_ + tr
    if sy != 1.0: im = im.resize((ww, int(round(hh * sy))), Image.LANCZOS)
    left = x * AS - total / 2 if anchor == 'mm' else (x * AS if anchor == 'lm' else x * AS - total)
    vec.alpha_composite(im, (int(round(left - 4)), int(round(y * AS - im.height / 2))))
    texts.append(dict(s=s, em=round(em, 3), x=round(x, 2), y=round(y, 2), col=list(col), font=os.path.basename(font), track=track, anchor=anchor, mod=modtag(x, y)))
def vtext(s, em, x, y, rot_deg, col=SILK, track=0.08):
    im, total = _glyphs(s, em, col, FB, track)
    im = im.rotate(rot_deg, expand=True, resample=Image.BICUBIC)
    im = im.crop(im.getbbox())          # centrage sur l'encre reelle (et non sur la boite de la police)
    vec.alpha_composite(im, (int(round(x * AS - im.width / 2)), int(round(y * AS - im.height / 2))))
    texts.append(dict(s=s, em=em, x=x, y=y, col=list(col), font='Poppins-Bold.ttf', track=track, anchor='mm', rot=rot_deg, sy=TXT_SY, mod=-1, ink=True))
PRIM = dict(lines=[], dots=[], polys=[], frames=[])
def modtag(x, y):
    if MOD_Y0 < y < MOD_Y1:
        for i in (3, 4, 5):
            if MODX[i] <= x <= MODX[i] + MODW[i]: return i
    return -1
def line(x0, y0, x1, y1, w=1.2, col=INK, a=255):
    PRIM['lines'].append(dict(x0=round(x0, 3), y0=round(y0, 3), x1=round(x1, 3), y1=round(y1, 3), w=w, col=list(col), mod=modtag((x0 + x1) / 2, (y0 + y1) / 2)))
    vd.line([(x0 * AS, y0 * AS), (x1 * AS, y1 * AS)], fill=tuple(col) + (a,), width=max(1, int(round(w * AS))))
def scale(cx_u, cy_u, R_u, lo, hi, col=INK, n_=11, ly=None, lx=None):
    for k in range(n_):
        th = np.radians(-135 + 270 * k / (n_ - 1))
        r0, r1 = R_u + 4, R_u + (11 if k in (0, n_ - 1) else 8)
        line(cx_u + r0 * np.sin(th), cy_u - r0 * np.cos(th), cx_u + r1 * np.sin(th), cy_u - r1 * np.cos(th), 1.7, col)
    em = 11.5
    for s, sg in ((lo, -1), (hi, 1)):
        wt = ImageFont.truetype(FB, int(em * (TEXT_K if MOD_Y0 < cy_u < MOD_Y1 else 1) * AS)).getlength(s) / AS * 1.03 + 3
        x = cx_u + sg * max(lx if lx else R_u * 0.78, wt / 2 + 2)
        text(s, em, x, ly if ly else cy_u + R_u + 15, col, track=0.03)

KL = {k: load(f'knob_{k}.png') for k in ('shadow', 'crown', 'cap', 'marker', 'highlight')}
CROWN_R = 409.6                     # rayon de la jupe dans les calques 1024 px (tools/make_knob.py)
knobs = []
def knob(name, cx_u, cy_u, R_u, deg):
    knobs.append(dict(id=name, x=round(cx_u, 2), y=round(cy_u, 2), r=R_u, size=round(1024 / CROWN_R * R_u, 2), deg=deg))

def draw_knob(cx_u, cy_u, R_u, deg):
    size = 1024 / CROWN_R * R_u
    sh = shift(rot(KL['shadow'], deg), 0.025 * 1024, 0.025 * 1024)
    stack = np.zeros_like(KL['cap'])
    for k in ('crown', 'cap', 'marker'):
        l = rot(KL[k], deg); a_ = l[..., 3:4] / 255
        stack[..., :3] = l[..., :3] * a_ + stack[..., :3] * (1 - a_); stack[..., 3:4] = (a_ + stack[..., 3:4] / 255 * (1 - a_)) * 255
    over(canvas, sh, cx_u, cy_u, size); over(canvas, stack, cx_u, cy_u, size); over(canvas, KL['highlight'], cx_u, cy_u, size)

# ------------------------------------------------------------------ touches, interrupteurs (rendu procedural, lumiere haut gauche)
def keycap(w_u, h_u, col, lit=False, pressed=False, bezel=4.0):
    """touche rectangulaire dans un cadre sombre (lumiere en haut a gauche).
    Relevee : face haute, biseau clair en haut a gauche, ombre portee de la touche dans le puits en bas a droite.
    Enfoncee (selectionnee) : face descendue au fond du puits, biseau inverse (haut gauche dans l'ombre),
    ombre interne du bord haut gauche du cadre sur la face, face retro-eclairee."""
    pw, ph = w_u + 2 * bezel + 10, h_u + 2 * bezel + 10
    n_w, n_h = int(pw * AS), int(ph * AS)
    Y, X = np.mgrid[0:n_h, 0:n_w].astype(np.float32)
    ox, oy = 5, 5
    d_bz = sdf_rrect(ox, oy, ox + w_u + 2 * bezel, oy + h_u + 2 * bezel, 4.0, X, Y)
    inset = 1.6 if pressed else 0.3
    d_k = sdf_rrect(ox + bezel + inset, oy + bezel + inset, ox + bezel + w_u - inset, oy + bezel + h_u - inset, 2.5, X, Y)
    d_well = sdf_rrect(ox + bezel - 0.6, oy + bezel - 0.6, ox + bezel + w_u + 0.6, oy + bezel + h_u + 0.6, 3.0, X, Y)
    a_bz = np.clip(0.5 - d_bz, 0, 1)
    sh = cv2.GaussianBlur(shift(a_bz, 2.2 * AS, 3.0 * AS), (0, 0), 2.4 * AS) * 0.55
    tb = np.clip(-d_bz / (1.6 * AS), 0, 1); nx_, ny_ = edge_normal(d_bz)
    bz = np.array([16, 15, 18], np.float32) * np.ones((n_h, n_w, 1), np.float32)
    bz = bz * (1 + np.sqrt(np.clip(1 - tb ** 2, 0, 1)) * 1.6 * np.clip((-nx_ - ny_) / np.sqrt(2), -1, 1))[..., None]
    well = np.array([6, 6, 7], np.float32)
    a_well = np.clip(0.5 - d_well, 0, 1)
    body = bz * (1 - a_well[..., None]) + well * a_well[..., None]
    a_k = np.clip(0.5 - d_k, 0, 1)
    if not pressed:
        # ombre portee de la touche relevee dans le puits (bas droite)
        ksh = cv2.GaussianBlur(shift(a_k, 1.4 * AS, 1.9 * AS), (0, 0), 1.2 * AS) * (1 - a_k) * a_well
        body = body * (1 - 0.6 * ksh[..., None])
    c = np.array(col, np.float32)
    if not lit: c = c * 0.80
    tk = np.clip(-d_k / (2.2 * AS), 0, 1); nkx, nky = edge_normal(d_k)
    facing = np.clip((-nkx - nky) / np.sqrt(2), -1, 1)
    bevel = np.sqrt(np.clip(1 - tk ** 2, 0, 1))
    if pressed:
        face = c * (1 - bevel * 0.30 * facing)[..., None] * 0.86          # biseau inverse, face plus basse
        # ombre interne : le bord haut gauche du cadre masque la lumiere sur la face
        occl = np.clip(0.5 - sdf_rrect(ox + bezel + inset + 2.6, oy + bezel + inset + 3.2, ox + bezel + w_u + 6, oy + bezel + h_u + 6, 2.5, X, Y), 0, 1)
        inner = 1 - cv2.GaussianBlur(occl, (0, 0), 1.8 * AS)
        face = face * (1 - 0.42 * inner * a_k)[..., None]
    else:
        face = c * (1 + bevel * 0.38 * facing)[..., None]
        face *= (1.04 - 0.10 * (Y / n_h - 0.5))[..., None]
    if lit:
        cen = np.exp(-((X - n_w / 2 - (0.6 if pressed else 0) * AS) ** 2 / (0.45 * w_u * AS) ** 2 + (Y - n_h / 2 - (0.6 if pressed else 0) * AS) ** 2 / (0.7 * h_u * AS) ** 2))
        face = face * (1.08 + 0.16 * cen[..., None]) + 16 * cen[..., None]
    rgbk = face * a_k[..., None] + body * (1 - a_k[..., None])
    if pressed:
        # lueur de la touche retro-eclairee dans l'interstice
        glow = cv2.GaussianBlur(a_k, (0, 0), 1.6 * AS) * (1 - a_k) * a_well
        rgbk = rgbk + glow[..., None] * c * 0.45
    alpha_ = np.clip(a_bz + sh * (1 - a_bz), 0, 1)
    out = np.zeros((n_h, n_w, 4), np.float32)
    out[..., :3] = np.where(alpha_[..., None] > 1e-4, np.clip(rgbk, 0, 255) * (a_bz / np.maximum(alpha_, 1e-4))[..., None], 0)
    out[..., 3] = alpha_ * 255
    return out

def lightpanel(w_u, h_u, lit, kind='on'):
    """interrupteur ON (pastille noire avec fenetre lumineuse a gauche) ou bascule PRE / POST (fenetre pleine largeur)"""
    pw, ph = w_u + 12, h_u + 12
    n_w, n_h = int(pw * AS), int(ph * AS)
    Y, X = np.mgrid[0:n_h, 0:n_w].astype(np.float32)
    d = sdf_rrect(5, 5, 5 + w_u, 5 + h_u, 3.5, X, Y)
    a = np.clip(0.5 - d, 0, 1)
    sh = cv2.GaussianBlur(shift(a, 2.0 * AS, 2.8 * AS), (0, 0), 2.2 * AS) * 0.55
    tb = np.clip(-d / (1.8 * AS), 0, 1); nx_, ny_ = edge_normal(d)
    body = np.array([22, 21, 25], np.float32) * (1 + np.sqrt(np.clip(1 - tb ** 2, 0, 1)) * 1.4 * np.clip((-nx_ - ny_) / np.sqrt(2), -1, 1))[..., None]
    body *= (1.08 - 0.16 * (Y / n_h))[..., None]
    wx0, wx1 = (5 + 4, 5 + 4 + 0.16 * w_u) if kind == 'on' else (5 + 4, 5 + w_u - 4)
    dw = sdf_rrect(wx0, 5 + 4, wx1, 5 + h_u - 4, 1.5, X, Y)
    aw = np.clip(0.5 - dw, 0, 1)
    if lit:
        win = np.array([255, 178, 70], np.float32) * (0.85 + 0.15 * np.exp(-((Y - n_h / 2) / (h_u * 0.3 * AS)) ** 2))[..., None]
        if kind != 'on':
            stripes = (np.sin(X / (2.2 * AS) * np.pi) > 0.2).astype(np.float32)
            win = win * (0.55 + 0.45 * stripes[..., None])
    else:
        win = np.array([34, 30, 26], np.float32) * np.ones_like(body)
        if kind != 'on':
            win = win + 40 * np.exp(-((X - n_w / 2) / (0.12 * w_u * AS)) ** 2)[..., None]
    rgb_ = body * (1 - aw[..., None]) + win * aw[..., None]
    alpha_ = np.clip(a + sh * (1 - a), 0, 1)
    out = np.zeros((n_h, n_w, 4), np.float32)
    out[..., :3] = np.where(alpha_[..., None] > 1e-4, rgb_ * (a / np.maximum(alpha_, 1e-4))[..., None], 0)
    out[..., 3] = alpha_ * 255
    if lit:
        g_ = cv2.GaussianBlur(aw, (0, 0), 4 * AS) * 0.35
        out[..., :3] = np.clip(out[..., :3] + g_[..., None] * np.array([255, 170, 60]), 0, 255)
    return out

def window(w_u, h_u):
    """ecran noir encastre : le texte est dessine par le plugin"""
    pw, ph = w_u + 12, h_u + 12
    n_w, n_h = int(pw * AS), int(ph * AS)
    Y, X = np.mgrid[0:n_h, 0:n_w].astype(np.float32)
    d = sdf_rrect(6, 6, 6 + w_u, 6 + h_u, 3.0, X, Y)
    a = np.clip(0.5 - d, 0, 1)
    rgb_ = np.array([6, 6, 8], np.float32) * np.ones((n_h, n_w, 1), np.float32)
    nx_, ny_ = edge_normal(d)
    rim = np.exp(-np.clip(-d, 0, None) / (1.0 * AS)) * a
    rgb_ = rgb_ + (rim * np.clip((nx_ + ny_) / np.sqrt(2), 0, 1) * 70)[..., None]
    return np.dstack([rgb_, a * 255]).astype(np.float32)

# ------------------------------------------------------------------ bandeau haut : presets + master
tx0, ty0, tx1, ty1 = TOP
PRE_BOX = (tx0 + 16, ty0 + 37, tx0 + 16 + 520, ty0 + 111)
PRESET = (PRE_BOX[0] + 16, PRE_BOX[1] + 18, 400.0, 38.0)
pb = PRE_BOX
_fr = Image.new('RGBA', (W, H), (0, 0, 0, 0)); _fd = ImageDraw.Draw(_fr)
_fd.rounded_rectangle([pb[0] * AS, pb[1] * AS, pb[2] * AS, pb[3] * AS], radius=6 * AS, outline=SILK + (255,), width=int(round(1.4 * AS)))
PRIM['frames'].append(dict(r=list(pb), rad=6, w=1.4, col=list(SILK), gap=[pb[0] + 12, pb[1] - 3, 68, 6]))
_fd.rectangle([(pb[0] + 12) * AS, (pb[1] - 3) * AS, (pb[0] + 80) * AS, (pb[1] + 3) * AS], fill=(0, 0, 0, 0))   # passage du mot PRESET
vec.alpha_composite(_fr)
text('PRESET', 14, pb[0] + 46, pb[1], SILK, track=0.08)
SAVE_C = (PRESET[0] + 400 + 28, PRESET[1] + 19); DEL_C = (SAVE_C[0] + 44, SAVE_C[1])
MX = [MODC[3], MODC[4], MODC[5]]
MKY = ty0 + 74; MKR = 27.0
for x, s, lo, hi in zip(MX, ['INPUT', 'MIX', 'OUTPUT'], ['-24', '0', '-24'], ['+24', '100%', '+24']):
    text(s, 15, x, ty0 + 22, SILK, track=0.06); scale(x, MKY, MKR, lo, hi, SILK)
for x, a, nm in zip(MX, [0, 90, -10], ['input', 'mix', 'output']): knob(nm, x, MKY, MKR, a)
LED_IN = (MX[0], ty0 + 131); LED_OUT = (MX[2], ty0 + 131)

# ------------------------------------------------------------------ modules
TITLES = ['SYNTH ENGINE', 'DISTORTION', 'MODULATION', 'RING MOD', 'BITCRUSH', 'GATED REVERB']
for i in range(6):
    em = 23 if i == 0 else (13.5 if len(TITLES[i]) > 10 else (15.5 if len(TITLES[i]) == 10 else 17.5))
    text(TITLES[i], em, MODC[i] + (7 if i >= 3 else 0), my(34), INK, track=0.05)
    if i >= 3:      # poignee de deplacement (modules reordonnables)
        tw = ImageFont.truetype(FB, int(em * TEXT_K * AS)).getlength(TITLES[i]) / AS * 1.05 / 2
        gx_ = MODC[i] + 7 - tw - 13
        for ddx in (0, 5):
            for ddy in (-5, 0, 5):
                vd.ellipse([(gx_ + ddx - 1.5) * AS, (my(34) + ddy - 1.5) * AS, (gx_ + ddx + 1.5) * AS, (my(34) + ddy + 1.5) * AS], fill=INK + (255,))
                PRIM['dots'].append(dict(x=gx_ + ddx, y=my(34) + ddy, r=1.5, col=list(INK), mod=i))

sc = MODC[0]
text('MIX', 15, sc, my(86)); knob('synthMix', sc, my(152), 42.0, 60); scale(sc, my(152), 42.0, '0', '100%', ly=my(152) + KR + 15, lx=50.0)
text('ENGINE SELECT', 14, sc, my(232), track=0.06)
line(MODX[0] + 14, my(232), sc - 76, my(232), 1.6); line(sc + 76, my(232), MODX[0] + SYN_W - 14, my(232), 1.6)
ENG = ['DEEP', 'VINTAGE', 'SOLO', 'STRINGS', 'HARMONY']
ENG_COL = [(214, 92, 120), (232, 226, 212), (240, 168, 70), (90, 182, 186), (164, 172, 190)]
KW, KH = 88.0, 34.0
ENG_POS = [(sc - 96, my(268)), (sc, my(268)), (sc + 96, my(268)), (sc - 48, my(336)), (sc + 48, my(336))]   # rangees alignees sur SOFT et RICH
SEL = 4
ib = (MODX[0] + 16, my(372), MODX[0] + SYN_W - 16, my(440))
vd.rounded_rectangle([ib[0] * AS, ib[1] * AS, ib[2] * AS, ib[3] * AS], radius=6 * AS, outline=INK + (255,), width=int(1.6 * AS))
PRIM['frames'].append(dict(r=list(ib), rad=6, w=1.6, col=list(INK), gap=None))
INT_ROWS = [my(390), my(422)]
INT_DISP = (MODX[0] + 140, MODX[0] + SYN_W - 52)
for k, y in enumerate(INT_ROWS): text(f'INTERVAL {k + 1}', 12.3, MODX[0] + 26, y, anchor='lm', track=0.03)
ON_W, ON_H, ON_Y = 66.0, 28.0, my(522)

dc = MODC[1]
text('GAIN', 15, dc, my(86)); knob('gain', dc, my(152), KR, 40); scale(dc, my(152), KR, '0', '100%')
text('MODE', 15, dc, my(232))
MODES = ['SOFT', 'HARD', 'RICH', 'WAVEFOLD']
MB_W, MB_H = 96.0, 27.0
MODE_Y = [my(268 + 34 * k) for k in range(4)]
text('PRE', 14, dc - 32, my(422)); text('POST', 14, dc + 32, my(422))   # alignes sur INTERVAL 2
PP_W, PP_H, PP_Y = 46.0, 22.0, my(454)
LBL = [None, None, ('DEPTH', 'MIX'), ('FREQ', 'MIX'), ('CRUSH', 'RESOLUTION'), ('TIME', 'MIX')]
RANGES = [None, None, (('0', '100%'), ('0', '100%')), (('20Hz', '5kHz'), ('0', '100%')), (('44.1kHz', '441Hz'), ('1', '24Bits')),
          (('0', '1000ms'), ('0', '100%'))]
IDS = [None, None, ('modDepth', 'modMix'), ('ringFreq', 'ringMix'), ('crush', 'bits'), ('verbTime', 'verbMix')]
ANG = [None, None, (-30, 45), (10, -70), (-135, 135), (-20, 30)]
for i in range(2, 6):
    c = MODC[i]
    for k in range(2):
        yk = my(152 if k == 0 else 361)
        text(LBL[i][k], 15 if len(LBL[i][k]) < 8 else 13.5, c, my(86 if k == 0 else 296))
        knob(IDS[i][k], c, yk, KR, ANG[i][k]); scale(c, yk, KR, *RANGES[i][k])

# ------------------------------------------------------------------ bandeau bas : nom + footswitch + logo
bx0, by0, bx1, by1 = BOT
BCY = (by0 + by1) / 2
for k, col in enumerate([MODCOL[0], MODCOL[1], MODCOL[3], MODCOL[2]]):
    x = bx0 + 16 + k * 17
    PRIM['polys'].append(dict(p=[x + 24, BCY - 30, x + 36, BCY - 30, x + 12, BCY + 30, x, BCY + 30], col=list(col)))
def draw_emblem(lit=1.0):
    # embleme arc-en-ciel en vectoriel (rendu x4 puis reduit : bords nets) ; allume = couleurs vives + halo, eteint = sombre
    SS = 4; x0e, y0e = bx0 + 4, BCY - 44; we, he = 110, 88
    im = Image.new('RGBA', (we * AS * SS, he * AS * SS), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    for k, col in enumerate([MODCOL[0], MODCOL[1], MODCOL[3], MODCOL[2]]):
        x = bx0 + 16 + k * 17
        pts = [(x + 24, BCY - 30), (x + 36, BCY - 30), (x + 12, BCY + 30), (x, BCY + 30)]
        c = tuple(int(v * (0.30 + 0.70 * lit) + 40 * (1 - lit)) for v in col)
        d.polygon([((px - x0e) * AS * SS, (py - y0e) * AS * SS) for px, py in pts], fill=c + (255,))
    small = im.resize((we * AS, he * AS), Image.LANCZOS)
    if lit > 0:
        a = np.asarray(small).astype(np.float32)
        glow = cv2.GaussianBlur(a, (0, 0), 6 * AS); glow[..., 3] *= 0.95 * lit
        over(canvas, glow, x0e + we / 2, y0e + he / 2)
    over(canvas, np.asarray(small).astype(np.float32), x0e + we / 2, y0e + he / 2)
text('SYNTHETIKA', 38, bx0 + 116, BCY - 9, SILK, anchor='lm', track=0.06)
text('GUITAR SYNTH', 14.5, bx0 + 118, BCY + 25, SILK, anchor='lm', track=0.14)
FS = ((bx0 + bx1) / 2, BCY + 11)
text('BYPASS', 13, FS[0], by0 + 17, SILK, track=0.12)
LOGO_W = 210.0 * 0.9; LOGO_C = (bx1 - 16 - LOGO_W / 2, by1 - 20)
vtext('OUTPUT', 8, (BX0 + IX0) / 2, JACK_Y, 90); vtext('INPUT', 8, (BX1 + IX1) / 2, JACK_Y, -90)   # centres entre bord du boitier et plaques, a hauteur des jacks


# ------------------------------------------------------------------ occlusion ambiante douce autour des pieces (lumiere haut gauche)
occ = Image.new('L', (W, H), 0); od = ImageDraw.Draw(occ)
def o_circle(x, y, r): od.ellipse([(x - r) * AS, (y - r) * AS, (x + r) * AS, (y + r) * AS], fill=255)
def o_rect(x, y, w, h, r=4): od.rounded_rectangle([(x - w / 2) * AS, (y - h / 2) * AS, (x + w / 2) * AS, (y + h / 2) * AS], radius=r * AS, fill=255)
for k in knobs: o_circle(k['x'], k['y'], k['r'] * 1.02)
for pos in ENG_POS: o_rect(pos[0], pos[1], KW + 8, KH + 8)
o_rect(dc, (MODE_Y[0] + MODE_Y[-1]) / 2, MB_W + 12, MODE_Y[-1] - MODE_Y[0] + MB_H + 12)
for xo in (dc - 32, dc + 32): o_rect(xo, PP_Y, PP_W, PP_H, 3.5)
for i in range(6): o_rect(MODC[i], ON_Y, ON_W, ON_H, 3.5)
for c_ in (SAVE_C, DEL_C): o_rect(c_[0], c_[1], 34, 34)
for y in INT_ROWS:
    o_rect((INT_DISP[0] + INT_DISP[1]) / 2, y, INT_DISP[1] - INT_DISP[0], 22, 3)
    for xa in (INT_DISP[0] - 16, INT_DISP[1] + 16): o_rect(xa, y, 23, 21)
o_rect(PRESET[0] + 200, PRESET[1] + 19, 404, 42, 21)
for (x, y) in (LED_IN, LED_OUT): o_circle(x, y, 7)
o_circle(FS[0], FS[1], 84.0 * 0.56)
om = np.asarray(occ).astype(np.float32) / 255
om_s = shift(om, 1.2 * AS, 1.6 * AS)
ao_map = 1 - 0.40 * cv2.GaussianBlur(om_s, (0, 0), 2.4 * AS) - 0.26 * cv2.GaussianBlur(om_s, (0, 0), 8 * AS)
ao_map = np.where(om > 0.5, 1.0, ao_map)
# contact des plaques avec le boitier : coins et bords legerement plus sombres sur le boitier, et pied des plaques
ao_map *= 1 - 0.22 * cv2.GaussianBlur(inplate, (0, 0), 5 * AS) * (1 - inplate)
# bords des plaques : legere occlusion dans les interstices (cote ombre plus marque)
edge_in = (1 - cv2.GaussianBlur(inplate, (0, 0), 3.5 * AS)) * inplate
ao_map *= 1 - 0.30 * np.clip(edge_in * 2, 0, 1)
# jacks : ombre douce sur la tranche
for xj in (BX0, BX1):
    jm = np.zeros((H, W), np.float32)
    cv2.rectangle(jm, (int((xj - 6) * AS), int((JACK_Y - JACK_L / 2) * AS)), (int((xj + 6) * AS), int((JACK_Y + JACK_L / 2) * AS)), 1.0, -1)
    ao_map *= 1 - 0.25 * cv2.GaussianBlur(jm, (0, 0), 5 * AS) * (1 - inplate) * (alpha > 0.5)
canvas[..., :3] *= np.clip(ao_map, 0.4, 1)[..., None]


# ------------------------------------------------------------------ source lumineuse a 45 deg (haut gauche), regle NINE HANDS | DSP
# source placee sur la diagonale exacte du coin haut gauche (meme decalage en x et en y) : l'eclairement decroit
# a 45 deg vers le bas a droite ; leger reflet satine perpendiculaire a la lumiere (bande x + y = cte).
SRC = (-260.0 * AS, -260.0 * AS)
dist = np.hypot(xx - SRC[0], yy - SRC[1]) / (FH * AS)
lamb = 1.0 / (1.0 + (dist / 0.95) ** 2)                            # decroissance douce (type 1 / d2 adoucie)
lamb = (lamb - lamb.min()) / (lamb.max() - lamb.min())
diag = (xx + yy) / (W + H)                                          # 0 en haut a gauche, 1 en bas a droite
sheen = np.exp(-((diag - 0.30) / 0.10) ** 2)
gain = 0.86 + 0.24 * lamb
canvas[..., :3] = canvas[..., :3] * gain[..., None] + (sheen * 12.0 * (inplate * 0.85 + 0.15))[..., None]

# ------------------------------------------------------------------ pieces fixes du fond
for c_ in (SAVE_C, DEL_C): over(canvas, keycap(28, 28, (54, 54, 60), lit=False, bezel=3.0), *c_)
for y in INT_ROWS:
    over(canvas, window(INT_DISP[1] - INT_DISP[0], 22), (INT_DISP[0] + INT_DISP[1]) / 2, y)
    for xa in (INT_DISP[0] - 16, INT_DISP[1] + 16): over(canvas, keycap(18, 16, (226, 222, 214), lit=True, bezel=2.5), xa, y)
mb = (dc - MB_W / 2 - 6, MODE_Y[0] - MB_H / 2 - 6, dc + MB_W / 2 + 6, MODE_Y[-1] + MB_H / 2 + 6)
d_mb = sdf_rrect(*mb, 4.0); a_mb = np.clip(0.5 - d_mb, 0, 1)
sh_mb = cv2.GaussianBlur(shift(a_mb, 2.2 * AS, 3 * AS), (0, 0), 2.4 * AS) * 0.5 * (1 - a_mb)
canvas[..., :3] = canvas[..., :3] * (1 - sh_mb[..., None]) * (1 - a_mb[..., None]) + np.array([16, 15, 18]) * a_mb[..., None]
jr, jw, jh = pc.jack_sprite(JACK_L, AS, +1); over(canvas, jr, FW - jw / 2, JACK_Y)
jl, jw, jh = pc.jack_sprite(JACK_L, AS, -1); over(canvas, jl, jw / 2, JACK_Y)
fs_spr, _refl_up, FSS, FS_BTNR = pc.footswitch_sprite(84.0, AS)
# reflet de l'anneau lumineux sur l'ecrou : la source est l'anneau dans la gorge, donc le reflet entoure la gorge
# (et non plus seulement le flanc haut, prevu pour une LED placee au-dessus) ; decroit en s'eloignant de l'anneau.
_n = fs_spr.shape[0]; _yy, _xx = np.mgrid[0:_n, 0:_n].astype(np.float32)
_r = np.hypot(_xx - _n / 2, _yy - _n / 2) / AS
_lum = fs_spr[..., :3].mean(2) / 255.0
_ring = np.exp(-np.clip(_r - FS_BTNR * 1.12, 0, None) / (0.16 * FS_BTNR)) * (_r > FS_BTNR * 1.12)
fs_refl = np.dstack([np.zeros((_n, _n, 3), np.float32), (_ring * (fs_spr[..., 3] / 255.0) * (0.25 + 0.75 * _lum) * 0.55 * 255)])
over(canvas, fs_spr, *FS)

# ------------------------------------------------------------------ pieces mobiles (apercu seulement)
if not FOND:
    for k, (pos, col) in enumerate(zip(ENG_POS, ENG_COL)):
        over(canvas, keycap(KW, KH, col, lit=(k == SEL), pressed=(k == SEL)), *pos)
    for k, y in enumerate(MODE_Y):
        over(canvas, keycap(MB_W, MB_H, (90, 182, 186) if k == 1 else (226, 222, 214), lit=(k == 1), pressed=(k == 1), bezel=1.5), dc, y)
    over(canvas, lightpanel(PP_W, PP_H, True, 'pp'), dc - 32, PP_Y)
    over(canvas, lightpanel(PP_W, PP_H, False, 'pp'), dc + 32, PP_Y)
    on_state = [True, True, True, False, False, True]
    for i in range(6): over(canvas, lightpanel(ON_W, ON_H, on_state[i], 'on'), MODC[i], ON_Y)
    for k in knobs: draw_knob(k['x'], k['y'], k['r'], k['deg'])
    for (x, y), colr, lit in ((LED_IN, (80, 230, 90), True), (LED_OUT, (255, 60, 50), False)):
        r_ = 5.5
        vd.ellipse([(x - r_ - 1.5) * AS, (y - r_ - 1.5) * AS, (x + r_ + 1.5) * AS, (y + r_ + 1.5) * AS], fill=(12, 12, 14, 255))
        c2 = colr if lit else tuple(int(v * 0.28) for v in colr)
        vd.ellipse([(x - r_) * AS, (y - r_) * AS, (x + r_) * AS, (y + r_) * AS], fill=c2 + (255,))
        vd.ellipse([(x - r_ * 0.55) * AS, (y - r_ * 0.65) * AS, (x - r_ * 0.05) * AS, (y - r_ * 0.25) * AS], fill=(255, 255, 255, 120))
    for k, (pos, s) in enumerate(zip(ENG_POS, ENG)):
        text(s, 13, pos[0] + (1.0 if k == SEL else 0), pos[1] + (1.2 if k == SEL else 0), INK, track=0.03)
    for k, (y, s) in enumerate(zip(MODE_Y, MODES)): text(s, 13, dc + (1.0 if k == 1 else 0), y + (1.2 if k == 1 else 0), INK, track=0.03)
    for i in range(6): text('ON', 12.5, MODC[i] + 7, ON_Y, (200, 196, 188), track=0.06)

scr = Image.new('RGBA', (W, H), (0, 0, 0, 0)); g = ImageDraw.Draw(scr)
x0, y0, w0, h0 = PRESET
if not FOND: g.rounded_rectangle([x0 * AS - 3, y0 * AS - 3, (x0 + w0) * AS + 3, (y0 + h0) * AS + 3], radius=h0 / 2 * AS + 3, fill=(14, 12, 18, 255))
if not FOND: g.rounded_rectangle([x0 * AS, y0 * AS, (x0 + w0) * AS, (y0 + h0) * AS], radius=h0 / 2 * AS, fill=(4, 4, 6, 255), outline=(150, 145, 160, 160), width=2)
if not FOND:
    f = ImageFont.truetype(FM, int(23 * AS))
    g.text(((x0 + w0 / 2) * AS, (y0 + h0 / 2) * AS + 1), 'Future Funk', font=f, fill=(235, 235, 240, 255), anchor='mm')
    for sx, dd in ((x0 + 17, -1), (x0 + w0 - 17, 1)):
        g.polygon([((sx + dd * 5.5) * AS, (y0 + h0 / 2) * AS), ((sx - dd * 5.5) * AS, (y0 + h0 / 2 - 6) * AS), ((sx - dd * 5.5) * AS, (y0 + h0 / 2 + 6) * AS)], fill=(150, 150, 155, 255))
    f2 = ImageFont.truetype(FM, int(15 * AS))
    for y, s in zip(INT_ROWS, ['+7', '+12']):
        g.text((((INT_DISP[0] + INT_DISP[1]) / 2) * AS, y * AS + 1), s, font=f2, fill=AMBER + (255,), anchor='mm')
    for y in INT_ROWS:
        for xa, dd in ((INT_DISP[0] - 16, -1), (INT_DISP[1] + 16, 1)):
            g.polygon([((xa + dd * 4.5) * AS, y * AS), ((xa - dd * 4.5) * AS, (y - 5) * AS), ((xa - dd * 4.5) * AS, (y + 5) * AS)], fill=(30, 28, 34, 255))
    sx_, sy_ = SAVE_C
    g.rounded_rectangle([(sx_ - 8) * AS, (sy_ - 8) * AS, (sx_ + 8) * AS, (sy_ + 8) * AS], radius=2 * AS, fill=(220, 218, 212, 255))
    g.rectangle([(sx_ - 4.5) * AS, (sy_ - 8) * AS, (sx_ + 4.5) * AS, (sy_ - 3) * AS], fill=(54, 54, 60, 255))
    g.ellipse([(sx_ - 3.2) * AS, (sy_ + 0.5) * AS, (sx_ + 3.2) * AS, (sy_ + 6.5) * AS], fill=(54, 54, 60, 255))
    tx_, ty_ = DEL_C
    g.rectangle([(tx_ - 6) * AS, (ty_ - 4) * AS, (tx_ + 6) * AS, (ty_ + 9) * AS], fill=(220, 218, 212, 255))
    g.rectangle([(tx_ - 8) * AS, (ty_ - 7.5) * AS, (tx_ + 8) * AS, (ty_ - 5.5) * AS], fill=(220, 218, 212, 255))
    g.rectangle([(tx_ - 2.5) * AS, (ty_ - 9.5) * AS, (tx_ + 2.5) * AS, (ty_ - 7.5) * AS], fill=(220, 218, 212, 255))
    for ddx in (-3, 0, 3): g.rectangle([(tx_ + ddx - 0.6) * AS, (ty_ - 2) * AS, (tx_ + ddx + 0.6) * AS, (ty_ + 7) * AS], fill=(54, 54, 60, 255))
over(canvas, np.asarray(scr).astype(np.float32), FW / 2, FH / 2)

if not FOND:
    ring = Image.new('RGBA', (W, H), (0, 0, 0, 0)); gr = ImageDraw.Draw(ring)
    Rr = FS_BTNR * 1.0775; Wr = FS_BTNR * 0.0674; cxr, cyr = FS[0] * AS, FS[1] * AS
    box = [cxr - Rr * AS, cyr - Rr * AS, cxr + Rr * AS, cyr + Rr * AS]
    for wdt in np.arange(20, 3, -1.5): gr.ellipse(box, outline=FS_PINK + (int(255 * 0.075 * 0.75),), width=max(1, int(wdt * Rr / 33.2 * AS)))
    gr.ellipse(box, outline=FS_PINK + (217,), width=max(1, int(Wr * AS)))
    gr.ellipse(box, outline=(255, 200, 226, 230), width=max(1, int(Wr * 0.42 * AS)))
    over(canvas, cv2.GaussianBlur(np.asarray(ring).astype(np.float32), (0, 0), 0.6), FW / 2, FH / 2)
    # leger reflet rose de l'anneau sur l'ecrou (calque fs_refl, 0,35 a 0,5 x niveau)
    rf = fs_refl.astype(np.float32).copy(); rf[..., :3] = np.array(FS_PINK, np.float32); rf[..., 3] *= 0.45
    over(canvas, rf, *FS)
    draw_emblem(1.0)
    over(canvas, load('logo_white_840.png'), *LOGO_C, LOGO_W)
    over(canvas, np.asarray(vec).astype(np.float32), FW / 2, FH / 2)

out = np.clip(canvas, 0, 255).astype(np.uint8)
name = 'fond_synthetika_2350x1800.png' if FOND else 'apercu_synthetika_2350x1800.png'
Image.fromarray(out, 'RGBA').save(os.path.join(ROOT, 'design', name))
if not FOND:
    prev = Image.fromarray(out, 'RGBA'); bgp = Image.new('RGBA', prev.size, (24, 24, 26, 255)); bgp.alpha_composite(prev)
    bgp.convert('RGB').resize((1175, 900), Image.LANCZOS).save(os.path.join(ROOT, 'design', 'apercu_synthetika_60.png'))
json.dump(dict(frame=[FW, FH], modX=MODX, modW=MODW, modC=MODC, knobs=knobs, texts=texts), open(os.path.join(ROOT, 'design', 'layout_preview.json'), 'w'), indent=1)
print('ok', W, H, 'box', round(BX0, 2), round(BX1, 2), 'mods', [round(v, 1) for v in MODX], round(COL_W, 1))

if FOND:
    OUTD = os.path.join(ROOT, 'export'); os.makedirs(OUTD, exist_ok=True)
    def save_rgba(arr, name): Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(OUTD, name), optimize=True)
    save_rgba(canvas, 'background.png')
    # colonnes reordonnables : decoupe de chaque plaque (avec la moitie de l'interstice de chaque cote)
    crops = []
    for m in (3, 4, 5):
        x0c, y0c = int(round((MODX[m] - GAP / 2) * AS)), int(round((MOD_Y0 - GAP / 2) * AS))
        x1c, y1c = int(round((MODX[m] + MODW[m] + GAP / 2) * AS)), int(round((MOD_Y1 + GAP / 2) * AS))
        save_rgba(canvas[y0c:y1c, x0c:x1c], f'column{m}.png')
        crops.append([x0c / AS, y0c / AS, (x1c - x0c) / AS, (y1c - y0c) / AS])
    for k, col in enumerate(ENG_COL):
        save_rgba(keycap(KW, KH, col, lit=False, pressed=False), f'key_eng{k}_up.png')
        save_rgba(keycap(KW, KH, col, lit=True, pressed=True), f'key_eng{k}_dn.png')
    save_rgba(keycap(MB_W, MB_H, (226, 222, 214), lit=False, pressed=False, bezel=1.5), 'key_mode_up.png')
    save_rgba(keycap(MB_W, MB_H, (90, 182, 186), lit=True, pressed=True, bezel=1.5), 'key_mode_dn.png')
    save_rgba(lightpanel(ON_W, ON_H, False, 'on'), 'sw_on_off.png'); save_rgba(lightpanel(ON_W, ON_H, True, 'on'), 'sw_on_on.png')
    save_rgba(lightpanel(PP_W, PP_H, False, 'pp'), 'sw_pp_off.png'); save_rgba(lightpanel(PP_W, PP_H, True, 'pp'), 'sw_pp_on.png')
    rf = fs_refl.astype(np.float32).copy(); rf[..., :3] = np.array(FS_PINK, np.float32); save_rgba(rf, 'fs_refl.png')
    for k in ('shadow', 'crown', 'cap', 'marker', 'highlight'):
        Image.open(os.path.join(A, f'knob_{k}.png')).resize((512, 512), Image.LANCZOS).save(os.path.join(OUTD, f'knob_{k}.png'), optimize=True)
    spr = dict(keyEng=[KW + 2 * 4.0 + 10, KH + 2 * 4.0 + 10], keyMode=[MB_W + 3 + 10, MB_H + 3 + 10], swOn=[ON_W + 12, ON_H + 12], swPP=[PP_W + 12, PP_H + 12],
               fsRefl=fs_refl.shape[1] / AS)
    pos = dict(engPos=ENG_POS, engNames=ENG, engCol=ENG_COL, keyW=KW, keyH=KH, modeX=dc, modeY=MODE_Y, modeNames=MODES, modeW=MB_W, modeH=MB_H,
               ppPos=[[dc - 32, PP_Y], [dc + 32, PP_Y]], ppW=PP_W, ppH=PP_H, onY=ON_Y, onW=ON_W, onH=ON_H, intRows=INT_ROWS, intDisp=list(INT_DISP),
               saveC=SAVE_C, delC=DEL_C, preset=list(PRESET), master=MX, masterY=MKY, masterR=MKR, ledIn=LED_IN, ledOut=LED_OUT,
               fs=list(FS), fsBtnR=FS_BTNR, logoC=list(LOGO_C), logoW=LOGO_W, modY=[MOD_Y0, MOD_Y1], gap=GAP, crops=crops, sprites=spr,
               titleY=my(34), handleX=[0, 0, 0], fsPink=FS_PINK)
    json.dump(dict(frame=[FW, FH], modX=MODX, modW=MODW, modC=MODC, knobs=knobs, texts=texts, prim=PRIM, pos=pos),
              open(os.path.join(OUTD, 'layout.json'), 'w'), indent=1)
    print('export', OUTD)
