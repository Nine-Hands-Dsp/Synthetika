"""
Potard de Synthetika, remodele d'apres la photo fournie (assets/knob_src/potard_08.png, nov. 2026).
La photo n'est pas ronde (ellipse 687 x 707 px, chapeau decentre de 13 px par la perspective) : le potard est
reconstruit parfaitement rond et concentrique, avec les mesures de la photo :
  - chapeau alu tourne (stries concentriques) jusqu'a r = 0,80 R, biseau clair a 0,82, gorge noire a 0,84 ;
  - jupe en caoutchouc noir de 0,85 a 1,0 R, 16 bossages de prise sur le bord (amplitude ~2 % du rayon) ;
  - repere : rainure noire de 0,40 a 0,97 du rayon du chapeau ;
  - reflets coniques sur la diagonale haut gauche / bas droite, lobe haut gauche le plus clair (lumiere a 45 deg).
Calques nh::Knob, PNG 1024 x 1024 fond transparent, meme cadrage, axe au centre exact :
  knob_shadow (centree, sans decalage), knob_crown, knob_cap (sans reflets), knob_marker (vers le haut),
  knob_highlight (reflets + lisere, fixes).
"""
import os
import numpy as np, cv2
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE); OUT = os.path.join(ROOT, 'assets')
N = 1024; SS = 2; M = N * SS                 # rendu sur-echantillonne
R = 0.40 * M                                  # rayon exterieur de la jupe
yy, xx = np.mgrid[0:M, 0:M].astype(np.float32)
dx, dy = xx - M / 2 + 0.5, yy - M / 2 + 0.5
r = np.hypot(dx, dy) / R
th = np.arctan2(dy, dx)                       # 0 = droite, sens horaire (y vers le bas)
rng = np.random.default_rng(8)

def aa(d_px):                                  # couverture anti-crenelee d'une distance signee (px, <0 dedans)
    return np.clip(0.5 - d_px, 0, 1)

R_CAP, R_BEV, R_GAP0, R_CROWN0 = 0.800, 0.825, 0.838, 0.852
NB, AMP = 16, 0.013
# bord exterieur : bossages arrondis (profil en plateau), parfaitement reguliers
ph = (th * NB / (2 * np.pi)) % 1.0
bump = np.clip(np.cos((ph - 0.5) * 2 * np.pi) * 3.2 + 1.2, -1, 1) * 0.5 + 0.5
r_edge = 1.0 - AMP * (1 - bump)       # couronne crantee : 16 bossages reguliers, parfaitement concentriques
a_out = aa((r - r_edge) * R)

# ---------------------------------------------------------------- jupe (caoutchouc)
light = np.array([-1, -1, 1.25]); light /= np.linalg.norm(light)
def lam(nx, ny, nz):
    return np.clip(nx * light[0] + ny * light[1] + nz * light[2], 0, 1)
# profil : face superieure legerement bombee vers l'exterieur, arrondi sur le bord
t = np.clip((r - R_CROWN0) / (r_edge - R_CROWN0), 0, 1)
slope = np.where(t > 0.72, (t - 0.72) / 0.28, 0) ** 1.4 * 2.4 + 0.10
ux, uy = np.cos(th), np.sin(th)
nz = 1 / np.sqrt(1 + slope ** 2); nx, ny = ux * slope * nz, uy * slope * nz
diff = lam(nx, ny, nz) / light[2]
grain = cv2.GaussianBlur(rng.standard_normal((M, M)).astype(np.float32), (0, 0), 1.2)
crown_l = 30 + 16 * (diff - 0.8) + 3.0 * grain
# leger reflet satine sur l'arrondi exterieur cote lumiere
hv = light + np.array([0, 0, 1]); hv /= np.linalg.norm(hv)
spec = np.clip(nx * hv[0] + ny * hv[1] + nz * hv[2], 0, 1) ** 18
crown_l += 34 * spec
# flancs des bossages : un peu plus sombres dans les creux
crown_l *= 0.88 + 0.12 * bump
crown_rgb = np.dstack([crown_l * 0.98, crown_l, crown_l * 1.04])
a_crown = a_out * aa((R_GAP0 - r) * R)
# gorge noire entre jupe et chapeau
a_gap = a_out * aa((R_BEV - r) * R) * (1 - a_crown)
gap_rgb = np.dstack([np.full_like(r, 12.0)] * 3)

# ---------------------------------------------------------------- chapeau alu tourne (matiere seule, sans reflets)
# stries concentriques : bruit tres fin en fonction du rayon, presque constant le long du cercle
rad_n = rng.standard_normal(8192).astype(np.float32)
rad_n = np.convolve(rad_n, np.ones(3) / 3, 'same')
ri = np.clip((r * R / (R_CAP * R)) * 4000, 0, 8190).astype(np.int32)
streak = rad_n[ri] * 0.65 + cv2.GaussianBlur(rng.standard_normal((M, M)).astype(np.float32), (0, 0), 0.7) * 0.35
cap_l = 164 + 10 * streak
cap_rgb = np.dstack([cap_l * 0.99, cap_l, cap_l * 1.02])
a_cap = aa((r - R_CAP) * R)
# biseau du chapeau (anneau 0,800 - 0,825) : matiere plus claire, eclairee par le reflet fixe
a_bev = aa((r - R_BEV) * R) * (1 - a_cap)
bev_rgb = np.dstack([np.full_like(r, 150.0)] * 3)

# ---------------------------------------------------------------- reflets fixes (calque highlight)
# reflet conique de l'alu tourne : deux lobes sur la diagonale de la lumiere, haut gauche plus fort
d_l = th - np.radians(-135)
lobe = np.cos(d_l) ** 2
lobe = lobe ** 2.2
side = np.where(np.cos(d_l) > 0, 1.0, 0.72)
anis = np.clip(1 - np.abs(streak) * 0.25, 0.5, 1)
hl_a = (0.50 * lobe * side * anis) * (r < R_CAP)
# assombrissement des secteurs perpendiculaires (noir transparent)
dark_a = (0.22 * (1 - np.cos(d_l) ** 2) ** 2) * (r < R_CAP)
# lisere : arete du biseau eclairee en haut a gauche, ombree en bas a droite
face = np.cos(d_l)
rim = np.exp(-((r - (R_CAP + R_BEV) / 2) / 0.010) ** 2)
rim_hi = rim * np.clip(face, 0, 1) * 0.85
rim_lo = rim * np.clip(-face, 0, 1) * 0.45
# reflet doux sur la jupe (haut gauche)
crown_hi = np.exp(-((r - 0.93) / 0.05) ** 2) * np.clip(face, 0, 1) ** 3 * 0.12 * a_crown
w_a = np.clip(hl_a + rim_hi + crown_hi, 0, 1)
k_a = np.clip(dark_a + rim_lo, 0, 1) * (1 - w_a)
hl_alpha = np.clip(w_a + k_a, 0, 1)
hl_rgb = np.where(hl_alpha[..., None] > 1e-4, (np.array([255.0, 255, 255]) * w_a[..., None]) / np.maximum(hl_alpha, 1e-4)[..., None], 0)

# ---------------------------------------------------------------- repere (rainure gravee vers le haut)
rc = R_CAP * R
mx0, mx1, mw = 0.40 * rc, 0.97 * rc, 0.026 * rc
u_ = -dy                                     # vers le haut
dseg = np.where(u_ < mx0, np.hypot(dx, u_ - mx0), np.where(u_ > mx1, np.hypot(dx, u_ - mx1), np.abs(dx))) - mw
a_mk = aa(dseg)
# paroi cote lumiere (gauche) dans l'ombre, paroi droite eclairee : liseré clair a droite
wall = np.clip((dx / mw), -1, 1)
mk_l = 14 + 26 * np.clip(wall, 0, 1) ** 3 * (np.abs(dx) > mw * 0.55)
mk_rgb = np.dstack([mk_l] * 3)

# ---------------------------------------------------------------- ombre (centree, sans decalage)
sh = cv2.GaussianBlur(a_out.astype(np.float32), (0, 0), 0.035 * R) * 0.75

def save(name, rgb, a):
    img = np.dstack([np.clip(rgb, 0, 255), np.clip(a, 0, 1) * 255]).astype(np.float32)
    pm = img.copy(); pm[..., :3] *= pm[..., 3:4] / 255
    small = cv2.resize(pm, (N, N), interpolation=cv2.INTER_AREA)
    al = small[..., 3:4] / 255
    small[..., :3] = np.where(al > 1e-4, small[..., :3] / np.maximum(al, 1e-4), 0)
    Image.fromarray(np.clip(small, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(OUT, f'knob_{name}.png'))

# couronne = jupe + gorge + biseau (tout ce qui tourne hors chapeau)
cr_a = np.clip(a_crown + a_gap + a_bev, 0, 1)
cr_rgb = (crown_rgb * a_crown[..., None] + gap_rgb * a_gap[..., None] + bev_rgb * a_bev[..., None]) / np.maximum(cr_a, 1e-4)[..., None]
save('shadow', np.zeros((M, M, 3)), sh)
save('crown', cr_rgb, cr_a)
save('cap', cap_rgb, a_cap)
save('marker', mk_rgb, a_mk)
save('highlight', hl_rgb, hl_alpha)
print('ok', N, 'R crown =', 0.40 * N, 'px')
