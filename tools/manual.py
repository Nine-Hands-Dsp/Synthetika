# Notice Synthetika (NINE HANDS | DSP), francais uniquement, couverture noire « minimale » (gabarit commun a toutes les notices).
# Usage : python3 tools/manual.py <dossier des captures du plugin (n_main.png, n_bypass.png)> docs/Synthetika_Notice_FR_v<version>.pdf
import os, sys, shutil
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (BaseDocTemplate, PageTemplate, NextPageTemplate, Frame, Paragraph, Spacer, Image as RLImage,
                                Table, TableStyle, PageBreak, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = sys.argv[1]
OUT = sys.argv[2]
VERSION = "1.2.0"
TMP = os.path.join(os.path.dirname(os.path.abspath(OUT)), "_notice_tmp"); os.makedirs(TMP, exist_ok=True)
ASSETS = os.path.join(ROOT, "tools", "manual_assets")
FONTS = os.path.join(ASSETS, "fonts")
for n, f in (("Pop", "Poppins-Regular"), ("PopL", "Poppins-Light"), ("PopM", "Poppins-Medium"), ("PopB", "Poppins-Bold"),
             ("Title", "BarlowCondensed-ExtraBold")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(FONTS, f + ".ttf")))

ACC = colors.HexColor("#7E62BA"); ACC_L = colors.HexColor("#B9A6E6")       # violet du module SYNTH ENGINE
DARK = colors.HexColor("#1C1A20"); GREY = colors.HexColor("#5F5A68"); LIGHT = colors.HexColor("#EFEAF8")
LOGO_PNG = os.path.join(ASSETS, "nine_hands_logo_blanc.png")

# ---------------------------------------------------------------- illustrations (rendus reels du plugin)
def shot(name): return Image.open(os.path.join(SHOTS, name)).convert("RGB")
src = shot("n_main.png")
k = src.height / 900.0                                         # repere du plugin : 1175 x 900 unites (fond 2350 x 1800 / 2)
marks = [(1, 268, 72), (2, 496, 72), (3, 540, 72), (4, 712, 99), (5, 869, 99), (6, 1026, 99), (7, 728, 148), (8, 1042, 148),
         (9, 145, 300), (10, 50, 441), (11, 60, 545), (12, 150, 695), (13, 395, 300), (14, 375, 441), (15, 370, 615),
         (16, 553, 300), (17, 700, 207), (18, 712, 300), (19, 869, 300), (20, 1026, 300), (21, 560, 695), (22, 535, 823), (23, 70, 823)]
ann = src.copy(); dr = ImageDraw.Draw(ann)
fnt = ImageFont.truetype(os.path.join(FONTS, "Poppins-Bold.ttf"), int(15 * k))
for n, x, y in marks:
    cx, cy, r = x * k, y * k, 13 * k
    dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255), outline=(126, 98, 186), width=int(2.5 * k))
    dr.text((cx, cy + 1), str(n), font=fnt, fill=(28, 26, 32), anchor="mm")
ann.save(os.path.join(TMP, "annotated.png"))
def crop_u(im, box): return im.crop(tuple(int(v * k) for v in box))
crop_u(src, (36, 173, 360, 744)).save(os.path.join(TMP, "synth.png"))
crop_u(src, (360, 173, 517, 744)).save(os.path.join(TMP, "dist.png"))
byp = shot("n_bypass.png"); kb = byp.height / 900.0
on = crop_u(src, (36, 744, 760, 884))
off = byp.crop(tuple(int(v * kb) for v in (36, 744, 760, 884))).resize(on.size, Image.LANCZOS)
s = Image.new("RGB", (on.width, on.height * 2 + 12), (255, 255, 255)); s.paste(on, (0, 0)); s.paste(off, (0, on.height + 12))
s.save(os.path.join(TMP, "onoff.png"))

# couverture : pedale detouree avec le canal alpha du fond (boitier + jacks), reflet au sol, halo violet tres doux
bg_a = Image.open(os.path.join(ROOT, "assets", "background.png")).split()[3].resize(src.size, Image.LANCZOS)
cut = Image.new("RGBA", src.size, (0, 0, 0, 0)); cut.paste(src, (0, 0), bg_a)
cut = cut.crop(cut.getbbox()); cw, ch = cut.size
pad = int(cw * 0.16)
canvas = Image.new("RGBA", (cw + 2 * pad, int(ch * 1.30) + pad), (0, 0, 0, 255))
yy, xx = np.mgrid[0:canvas.height, 0:canvas.width].astype(np.float64)
gcx, gcy = canvas.width / 2, pad + ch * 0.45
glow = np.exp(-(((xx - gcx) / (cw * 0.62)) ** 2 + ((yy - gcy) / (ch * 0.48)) ** 2)) * 30
edge = np.clip(np.minimum.reduce([xx, canvas.width - 1 - xx, yy, canvas.height - 1 - yy]) / (pad * 0.9), 0, 1)
glow *= edge ** 2
halo = np.dstack([glow * 0.50, glow * 0.38, glow * 0.95, np.full_like(glow, 255)]).astype(np.uint8)
canvas = Image.alpha_composite(canvas, Image.fromarray(halo, "RGBA"))
refl = cut.transpose(Image.FLIP_TOP_BOTTOM); ra = np.array(refl).astype(np.float64)
fade = np.clip(1 - np.arange(refl.height) / (refl.height * 0.22), 0, 1) ** 1.6 * 0.22
ra[..., 3] *= fade[:, None]
canvas.alpha_composite(Image.fromarray(ra.astype(np.uint8), "RGBA"), (pad, pad + ch + 6))
canvas.alpha_composite(cut, (pad, pad))
canvas = canvas.crop((0, 0, canvas.width, pad + int(ch * 1.20)))
cover = os.path.join(TMP, "cover.png"); canvas.convert("RGB").save(cover)
annotated, synth_img, dist_img, onoff_img = [os.path.join(TMP, n) for n in ("annotated.png", "synth.png", "dist.png", "onoff.png")]

def img(path, width):
    im = Image.open(path); return RLImage(path, width=width, height=width * im.height / im.width)

st = {
    "h1": ParagraphStyle("h1", fontName="PopB", fontSize=20, leading=24, textColor=ACC, spaceAfter=8, spaceBefore=4, keepWithNext=1),
    "h2": ParagraphStyle("h2", fontName="PopM", fontSize=13, leading=17, textColor=DARK, spaceAfter=4, spaceBefore=10, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="Pop", fontSize=9.6, leading=14, textColor=DARK, spaceAfter=5),
    "tip": ParagraphStyle("tip", fontName="Pop", fontSize=9.0, leading=12.4, textColor=DARK, spaceAfter=3),
    "small": ParagraphStyle("small", fontName="Pop", fontSize=8.2, leading=11.5, textColor=GREY),
    "cell": ParagraphStyle("cell", fontName="Pop", fontSize=8.4, leading=11, textColor=DARK),
    "cellb": ParagraphStyle("cellb", fontName="PopM", fontSize=8.4, leading=11, textColor=DARK),
    "cells": ParagraphStyle("cells", fontName="Pop", fontSize=7.6, leading=9.6, textColor=DARK),
    "cellbs": ParagraphStyle("cellbs", fontName="PopM", fontSize=7.6, leading=9.6, textColor=DARK),
}
def P(t, s="p"): return Paragraph(t, st[s])
def bullets(items, s="p"): return [P("&bull;&nbsp;&nbsp;" + i, s) for i in items]
def table(rows, widths, header=True, small=False):
    data = [[P(c, ("cellbs" if small else "cellb") if (header and r == 0) else ("cells" if small else "cell")) for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    style = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#DDD5EC")),
             ("TOPPADDING", (0, 0), (-1, -1), 2 if small else 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 2 if small else 3)]
    if header: style += [("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("LINEBELOW", (0, 0), (-1, 0), 0.8, ACC)]
    t.setStyle(TableStyle(style)); return t

# ---------------------------------------------------------------- contenu (identique aux presets de PluginProcessor.cpp)
ENG = ["Deep", "Vintage", "Solo", "Strings", "Harmony"]; MODES = ["Soft", "Hard", "Rich", "Wavefold"]; POS = ["pré", "post", "pré + post"]
FACTORY = [
    ("Future Funk", "Deep", "Drive Soft 25 % (pré)", "Basse synthé funky et ronde, légèrement saturée : le son de démarrage."),
    ("Soklo Bass", "Deep", "aucun effet", "La basse carrée de l'extrait de Soklo, à l'état brut : idéale pour comparer les moteurs."),
    ("Fifths & Octave", "Harmony +7 / +12", "Modulation 35 / 60 %, Reverb 380 ms / 20 %", "Quinte et octave au-dessus de la note jouée, larges et en stéréo : le son du 2e extrait."),
    ("Analog Lead", "Solo", "Drive Rich 32 % (post), Modulation 18 / 40 %, Reverb 320 ms / 16 %", "Lead analogique qui chante et tranche."),
    ("Vintage Keys", "Vintage", "Synth Mix 90 %, Modulation 25 / 50 %", "Synthé chaud au filtre qui claque sur chaque note, un peu de guitare dessous."),
    ("String Machine", "Strings", "Modulation 45 / 65 %, Reverb 650 ms / 30 %", "Nappe de cordes électroniques à attaque lente."),
    ("Octave Stack", "Harmony −12 / +12", "Drive Soft 15 %", "Octave en dessous et au-dessus : énorme en notes seules."),
    ("Broken Radio", "Solo", "Drive Hard 48 %, Bitcrush 62 % / 6 bits", "Lead cassé et numérique, façon radio qui grésille."),
    ("Ring Bell", "Vintage", "Ring Mod 870 Hz / 60 %, Modulation 20 / 40 %, Reverb 450 ms / 22 %", "Timbre de cloche métallique (ordre Ring Mod > Bitcrush > Reverb)."),
    ("80s Gated Lead", "Solo", "Drive Soft 20 % (post), Modulation 28 / 55 %, Reverb 480 ms / 55 %", "Lead années 80 avec une grosse réverbe à porte."),
    ("Wavefold Bass", "Deep", "Drive Wavefold 45 % (pré)", "Basse creuse et métallique par repli d'onde."),
    ("Guitar + Synth", "Deep", "Synth Mix 50 %, Modulation 25 / 45 %", "La guitare doublée par la basse synthé, à parts égales."),
    ("Bit Choir", "Strings", "Bitcrush 40 % / 10 bits, Modulation 40 / 60 %, Reverb 700 ms / 35 %", "Chœur électronique granuleux (ordre Reverb > Bitcrush > Ring Mod)."),
    ("Dry Guitar FX", "synthé coupé", "Drive Rich 35 %, Ring Mod 120 Hz / 35 %, Reverb 300 ms / 18 %", "Les effets seuls sur la guitare, synthé éteint."),
]

W = A4[0] - 36 * mm
def story():
    f = [NextPageTemplate("p"), Spacer(1, 1), PageBreak()]
    f.append(P("1. Présentation", "h1"))
    f.append(P("<b>Synthetika</b> transforme la guitare en synthétiseur, sans micro MIDI : le plugin suit la note jouée et la fait chanter par l'un de ses <b>cinq moteurs</b> (Deep, Vintage, Solo, Strings, Harmony), puis la façonne avec une chaîne d'effets complète : Distortion, Modulation, Ring Mod, Bitcrush et Gated Reverb."))
    f.append(P("Il s'inspire du <b>Soklo GS-5</b> (Audio Hertz), développé avec le guitariste et producteur de Los Angeles Soklo : même disposition de façade, mêmes commandes, mêmes plages. Les timbres ont été calés sur des extraits de Soklo : la basse <b>Deep</b> reprend son onde quasi carrée (fondamentale forte, harmoniques impairs dominants, pas de sous-octave) et <b>Harmony</b> part de la quinte et de l'octave entendues dans ses sons."))
    f.append(P("Synthetika est <b>monophonique</b> : il suit une note à la fois. Jouez des notes seules, étouffez les cordes inutiles : les bends, slides et vibratos passent au synthé sans palier."))
    f.append(P("2. Installation", "h1"))
    for h, x in [("Windows", "Copiez <i>Synthetika.vst3</i> dans <i>C:\\Program Files\\Common Files\\VST3</i>."),
                 ("macOS", "Copiez <i>Synthetika.vst3</i> dans <i>~/Library/Audio/Plug-Ins/VST3</i>, puis une seule fois dans le Terminal : <i>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Synthetika.vst3</i> (retire la quarantaine du téléchargement)."),
                 ("Reaper", "Options &gt; Preferences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. Le plugin s'appelle <b>Synthetika (NINE HANDS | DSP)</b>. Placez-le sur la piste guitare, avec le monitoring d'entrée activé.")]:
        f.append(P(h, "h2")); f.append(P(x))
    f.append(P("Premiers pas", "h2"))
    f += bullets(["Réglez le gain de votre interface pour que les notes les plus fortes ne saturent pas, INPUT à 0 dB.",
                  "Chargez <b>Soklo Bass</b> (effets coupés, synthé seul) et jouez une phrase en notes seules ; essayez chaque moteur avec la même phrase.",
                  "Rallumez les effets un par un, puis dosez la guitare avec le MIX du synthé et l'ensemble avec MIX et OUTPUT."])
    f.append(PageBreak())
    f.append(P("3. Vue d'ensemble", "h1"))
    f.append(img(annotated, W * 0.86))
    rows = [["N°", "Commande", "Rôle"],
            ["1", "Barre de presets", "preset en cours, flèches précédent / suivant, clic = menu ; valeurs en direct pendant un réglage"],
            ["2", "Enregistrer", "enregistre le réglage comme preset utilisateur"], ["3", "Supprimer", "supprime le preset utilisateur en cours"],
            ["4", "INPUT", "−24 à +24 dB : niveau de la guitare qui entre"], ["5", "MIX", "0 à 100 % : guitare sèche / chaîne traitée"],
            ["6", "OUTPUT", "−24 à +24 dB : niveau final"], ["7", "LED d'entrée", "verte : signal reçu (plus elle brille, plus il est fort)"],
            ["8", "LED d'écrêtage", "rouge : la sortie dépasse 0 dBFS (baisser OUTPUT)"],
            ["9", "MIX du synthé", "0 à 100 % : guitare / synthé avant les effets"], ["10", "ENGINE SELECT", "les 5 moteurs (touche enfoncée = moteur choisi)"],
            ["11", "INTERVAL 1 / 2", "voix ajoutées du moteur Harmony, −24 à +24 demi-tons"], ["12", "ON (synthé)", "allume / coupe le synthé"],
            ["13", "GAIN", "quantité de distorsion"], ["14", "MODE", "Soft, Hard, Rich, Wavefold"], ["15", "PRE / POST", "place de la distorsion dans la chaîne"],
            ["16", "MODULATION", "DEPTH et MIX : voix désaccordées, stéréo"], ["17", "Poignées à six points", "glisser pour changer l'ordre Bitcrush / Ring Mod / Gated Reverb"],
            ["18", "BITCRUSH", "CRUSH (44,1 kHz &gt; 441 Hz) et RESOLUTION (24 &gt; 1 bit)"], ["19", "RING MOD", "FREQ (20 Hz – 5 kHz) et MIX"],
            ["20", "GATED REVERB", "TIME (0 – 1000 ms) et MIX"], ["21", "ON (modules)", "allume / coupe chaque module, réglages conservés"],
            ["22", "Footswitch BYPASS", "effet / bypass ; anneau rose allumé = effet actif"], ["23", "Arc-en-ciel", "s'allume quand la pédale est active, s'éteint en bypass"]]
    f.append(Spacer(1, 3 * mm)); f.append(table(rows, [10 * mm, 36 * mm, W - 46 * mm], small=True))
    f.append(PageBreak())
    f.append(P("4. Chaîne du signal", "h1"))
    f.append(P("Après INPUT, le signal se sépare en un <b>chemin sec</b> et un <b>chemin traité</b>, qui se rejoignent au MIX global ; OUTPUT s'applique en dernier :"))
    f.append(P("<b>INPUT</b> &gt; <b>Synth Engine</b> (+ Modulation) &gt; <b>Distortion PRE</b> &gt; <b>Bitcrush · Ring Mod · Gated Reverb</b> (dans l'ordre affiché, de gauche à droite) &gt; <b>Distortion POST</b> &gt; <b>MIX</b> &gt; <b>OUTPUT</b>."))
    f.append(P("Synthé coupé, la guitare passe quand même par Distortion, Bitcrush, Ring Mod et Gated Reverb. La Modulation, elle, n'agit que sur le synthé."))
    f.append(P("5. Synth Engine", "h1"))
    f.append(Table([[[P("<b>MIX</b> mélange la guitare et le synthé <i>avant</i> les effets : 0 % = guitare seule, 100 % = synthé seul. Utilisez-le pour doser le synthé sous la guitare, puis le MIX global pour l'équilibre sec / traité."),
                       P("ENGINE SELECT", "h2")] + bullets([
                       "<b>DEEP</b> : basse lourde et franche, onde quasi carrée renforcée sur la fondamentale, petit claquement de filtre à chaque note.",
                       "<b>VINTAGE</b> : deux dents de scie désaccordées + sous-octave dans un filtre 4 pôles qui s'ouvre à l'attaque puis se referme : le « wow » analogique, plus brillant quand vous attaquez fort.",
                       "<b>SOLO</b> : impulsion à largeur modulée + scie, filtre résonant et saturation, attaque immédiate : lead qui chante et tranche.",
                       "<b>STRINGS</b> : ensemble de trois scies au désaccord animé et une octave, aéré, attaque lente et relâchement long : nappe de cordes électronique.",
                       "<b>HARMONY</b> : la note jouée plus une ou deux voix (scie + carré) à des intervalles réglables."], "tip") + [
                       P("INTERVAL 1 / 2", "h2"),
                       P("Actifs en mode Harmony (grisés sinon). Chaque intervalle se règle de −24 à +24 demi-tons : flèches, molette, ou clic sur l'écran pour ouvrir le <b>tambour</b> (molette puis clic, Entrée pour valider, Échap pour annuler). +12 = une octave au-dessus, −12 = une octave en dessous. INTERVAL 2 peut être coupé (<i>OFF</i>) ; avec les deux voix, vous entendez trois voix de synthé.")],
                      img(synth_img, 52 * mm)]], colWidths=[W - 56 * mm, 56 * mm], style=[("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (0, 0), 0)]))
    f.append(P("Jouer Synthetika", "h2"))
    f += bullets(["Suivi de hauteur : de B0 (31 Hz, basse 5 cordes) et F#1 (8 cordes) jusqu'au-dessus de C6. Chaque attaque est détectée : la nouvelle note démarre directement à la bonne hauteur, sans glissement ni bavure (≈ 15 ms sur une guitare, environ deux périodes de la note dans le grave) ; la note précédente continue jusque-là, il n'y a jamais de trou.",
                  "Bends, vibratos, slides et legato (hammer-on, pull-off) sont suivis en continu, analyse toutes les 1,5 ms ; un saut d'octave parasite est filtré.",
                  "Signal trop faible : montez INPUT.",
                  "Arpèges laissés sonner : à chaque attaque, la note jouée est identifiée en comparant le son juste après et juste avant ; les cordes qui sonnent encore ne la troublent pas. Sur un accord plaqué, le synthé ne suit qu'une note.",
                  "Le volume suit votre jeu avec une compression douce, la fin de note s'éteint proprement et un limiteur doux garde la sortie sous 0 dBFS."], "tip")
    f.append(PageBreak())
    f.append(P("6. Les effets", "h1"))
    f.append(Table([[[P("Distortion", "h2"),
                       P("<b>GAIN</b> règle la saturation, <b>MODE</b> son caractère :")] + bullets([
                       "<b>SOFT</b> : saturation douce et ronde, façon lampe.", "<b>HARD</b> : écrêtage franc, plus brillant, proche de la fuzz.",
                       "<b>RICH</b> : plus épais, harmoniques paires, son dense.", "<b>WAVEFOLD</b> : l'onde se replie sur elle-même : creux, métallique, presque ring mod à fond."], "tip") + [
                       P("<b>PRE</b> distord avant le groupe Bitcrush / Ring Mod / Gated Reverb, <b>POST</b> après ; les deux peuvent être allumés ensemble (même GAIN et même MODE). L'un des deux reste toujours allumé : pour couper la distorsion, utilisez son interrupteur ON.")],
                      img(dist_img, 30 * mm)]], colWidths=[W - 34 * mm, 34 * mm], style=[("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (0, 0), 0)]))
    for h, x in [("Modulation", "Désaccorde les voix du synthé pour un chorus / épaississement : <b>DEPTH</b> écarte deux voix micro-désaccordées (une plus basse à gauche, une plus haute à droite, jusqu'à ±32 cents), <b>MIX</b> les mélange à la voix principale. À zéro, pas de désaccord. N'agit que si le synthé est allumé ; en mono, les voix sont additionnées."),
                 ("Ring Mod", "<b>FREQ</b> (20 Hz – 5 kHz) règle la porteuse ; <b>MIX</b> mélange le signal modulé au signal qui entre dans le module. Bas : trémolo granuleux ; haut : cloches et métal."),
                 ("Bitcrush", "<b>CRUSH</b> réduit la fréquence d'échantillonnage de 44,1 kHz (propre) à 441 Hz (repliement, grain) ; <b>RESOLUTION</b> réduit la précision de 24 bits (propre) à 1 bit. Dans une session à 48 ou 96 kHz, le plafond reste 44,1 kHz : CRUSH à fond à gauche réduit donc déjà légèrement la bande passante, comme sur l'original."),
                 ("Gated Reverb", "Réverbe non linéaire façon années 80 : les réflexions montent vite, restent pleines puis <b>s'arrêtent net</b>. <b>TIME</b> (0 – 1000 ms) règle la durée de la bouffée (à 0, une très courte bouffée reste audible), <b>MIX</b> la dose. Les motifs diffèrent à gauche et à droite (largeur stéréo), une légère modulation fait vivre les réflexions et des filtres gardent le son clair. Les queues des notes successives se chevauchent ; couper le module coupe la réverbe tout de suite en gardant les réglages."),
                 ("Ordre des effets", "Glissez la poignée à six points d'un titre (Bitcrush, Ring Mod, Gated Reverb) vers une autre place : les colonnes changent de place et le son suit l'ordre affiché, de gauche à droite. Aussi dans clic droit &gt; <b>Effect order</b>. Changer l'ordre change ce que chaque effet reçoit (une réverbe avant le bitcrush est elle-même écrasée, par exemple).")]:
        f.append(P(h, "h2")); f.append(P(x))
    f.append(P("7. Footswitch, MIDI et fenêtre", "h1"))
    f.append(KeepTogether([img(onoff_img, W * 0.62), P("Pédale active (anneau rose, arc-en-ciel allumé) puis en bypass.", "small")]))
    f += bullets(["<b>BYPASS</b> : clic sur le footswitch (fondu de 15 ms sans clic). L'anneau rose dans la gorge et l'arc-en-ciel s'allument quand Synthetika est actif ; leur halo se règle dans clic droit &gt; <b>Glow</b> (25, 50, 75 ou 100 %).",
                  "<b>MIDI Learn</b> : aucune commande n'a de CC par défaut. Clic droit sur un potard, une touche, un interrupteur ou le footswitch &gt; <b>MIDI Learn</b>, puis bougez la pédale d'expression ou appuyez sur le bouton MIDI ; la barre confirme (ex. <i>SYNTH MIX = CC 11</i>). <b>Clear MIDI</b> retire une assignation, clic droit &gt; MIDI &gt; <b>Clear all MIDI assignments</b> les retire toutes.",
                  "Pédale d'expression sans saccades (rampe entre deux messages, course de la pédale apprise, zones mortes aux butées, CC 14 bits accepté). Footswitch MIDI à accrochage (127 / 0) ou momentané : clic droit &gt; Settings.",
                  "<b>Program Change</b> 1 à 14 : charge les presets d'usine. Tous les paramètres sont automatisables dans le DAW.",
                  "<b>Fenêtre</b> : s'ouvre à « 100 % » (hauteur de Ring Think) et ne peut jamais être plus petite ; clic droit &gt; <b>Plugin size</b> : 100 / 125 / 150 / 200 %, <b>Default size (new windows)</b> pour la taille d'ouverture ; tirer le coin agrandit jusqu'à 200 % (proportions conservées).",
                  "<b>Ligne d'état</b> : dès qu'un réglage bouge, la pastille de presets affiche la valeur en direct (ex. <i>REVERB TIME 380 MS</i>) puis revient au preset 2,5 s plus tard ; un clic la fait revenir tout de suite."])
    f.append(P("8. Presets", "h1"))
    f.append(P("Flèches de la barre : preset précédent / suivant (presets d'usine puis presets utilisateur). Clic sur le nom : liste de tous les presets. <b>Enregistrer</b> ouvre une fenêtre pour nommer le preset ; un nom déjà pris demande confirmation, enregistrer sous le nom d'un preset d'usine crée votre copie et <i>Default</i> est réservé. <b>Supprimer</b> demande confirmation et ne touche jamais aux presets d'usine. Un astérisque après le nom signale un preset modifié."))
    f.append(P("Les presets utilisateur sont des fichiers <i>.preset</i> à partager ou à copier entre ordinateurs : <i>%APPDATA%\\Nine Hands DSP\\Synthetika\\User Presets</i> (Windows), <i>~/Library/Application Support/Nine Hands DSP/Synthetika/User Presets</i> (macOS). Ils gardent moteur et intervalles, ordre des effets, interrupteurs, PRE / POST et mode, tous les potards, MIX du synthé, INPUT, OUTPUT et MIX global."))
    rows = [["N°", "Preset", "Moteur", "Effets", "Description"]]
    for i, (n, e, fx, d) in enumerate(FACTORY): rows.append([str(i + 1), "<b>" + n + "</b>", e, fx, d])
    f.append(table(rows, [7 * mm, 27 * mm, 25 * mm, 55 * mm, W - 114 * mm], small=True))
    f.append(P("9. Conseils d'utilisation", "h1"))
    f += bullets(["Dans Reaper, placez Synthetika sur la piste guitare <b>avant</b> Ampire et Impulse ; pour un son de synthé pur, contournez l'ampli ou utilisez une capture propre.",
                  "Utilisez le buffer le plus bas qui reste stable : la détection prend environ 15 ms sur une guitare, la carte son et les autres plugins en ajoutent.",
                  "Sur une 7 ou 8 cordes ou une basse, Deep et Vintage donnent une basse énorme ; gardez une attaque franche sur les cordes graves.",
                  "Pour doubler la guitare : MIX du synthé vers 50 % (preset <i>Guitar + Synth</i>), puis un peu de Modulation pour l'élargir.",
                  "Les octaves d'Harmony (−12, +12) sonnent bien sur tout le manche ; la quinte (+7) est la signature de Soklo."], "tip")
    tech = [["Format", "VST3 (Windows 64 bits, macOS universel Apple Silicon + Intel)"], ["Canaux", "entrée mono ou stéréo, sortie stéréo (ou mono)"],
            ["Suivi", "monophonique, YIN décimé à fenêtre adaptée + affinage à pleine résolution, de 28 Hz à 1,6 kHz, analyse toutes les 1,5 ms ; détection d'attaque et identification de la note jouée parmi les cordes qui sonnent ; nouvelle note ≈ 15 ms (plus dans le grave)"],
            ["Moteurs", "Deep, Vintage, Solo, Strings, Harmony (2 intervalles −24 à +24, le 2e coupable)"],
            ["Effets", "Distortion 4 modes pré / post suréchantillonnée x4 · Modulation ±32 cents · Ring Mod 20 Hz – 5 kHz · Bitcrush 44,1 kHz – 441 Hz, 24 – 1 bit · Gated Reverb 0 – 1000 ms ; ordre réglable"],
            ["Presets", "14 d'usine, presets utilisateur en fichiers .preset"],
            ["MIDI", "MIDI Learn sur les 26 commandes (rien d'assigné par défaut), Program Change 1-14"],
            ["Fenêtre", "100 % (taille minimale) / 125 % / 150 % / 200 % ; Glow 25 à 100 % ; anti-gel v2 sous Windows"]]
    f.append(KeepTogether([P("10. Caractéristiques techniques", "h1"), table(tech, [26 * mm, W - 26 * mm], header=False, small=True)]))
    return f

def deco(c, doc):
    c.saveState()
    c.setFillColor(DARK); c.rect(0, A4[1] - 6 * mm, A4[0], 6 * mm, stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 6.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
    c.setFont("Pop", 7.5); c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, "Synthetika (NINE HANDS | DSP) · v" + VERSION)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()

# ---- Couverture « minimale » (gabarit fixe commun a toutes les notices)
PEDAL_H, PEDAL_MAXW, PEDAL_TOP = 82 * mm, 115 * mm, 84 * mm
LANG_TOP = 18 * mm; NAME_TOP, NAME_PT = 194 * mm, 30; SUB_TOP, SUB_PT = 208 * mm, 10
LOGO_W, LOGO_BOTTOM = 38 * mm, 22 * mm; BAND_H = 3 * mm
def cover_bg(c, doc):
    c.saveState(); c.setFillColor(colors.black); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, 0, A4[0], BAND_H, stroke=0, fill=1)
    def line(text, top, font, size, cs, col):
        w = pdfmetrics.stringWidth(text, font, size) + cs * (len(text) - 1)
        t = c.beginText((A4[0] - w) / 2, A4[1] - top - size * 0.8); t.setFont(font, size); t.setCharSpace(cs)
        t.setFillColor(col); t.textOut(text); c.drawText(t)
    line("FRANÇAIS", LANG_TOP, "PopM", 8.5, 0, ACC_L)
    ci = Image.open(cover); iw, ih = ci.size
    cw = iw / 1.32; ch = (ih - int(cw * 0.16)) / 1.2; pad = int(cw * 0.16)
    kk = min(PEDAL_H / ch, PEDAL_MAXW / cw)
    dw, dh = iw * kk, ih * kk
    y0 = A4[1] - (PEDAL_TOP + (PEDAL_H - ch * kk) / 2) + pad * kk
    c.drawImage(cover, (A4[0] - dw) / 2, y0 - dh, dw, dh)
    line("SYNTHETIKA", NAME_TOP - NAME_PT * 0.02, "Title", NAME_PT, NAME_PT * 0.5, colors.white)
    line("Synthé guitare", SUB_TOP, "Pop", SUB_PT, 1.5, colors.HexColor("#BDC4B8"))
    lw, lh = Image.open(LOGO_PNG).size
    c.drawImage(LOGO_PNG, (A4[0] - LOGO_W) / 2, LOGO_BOTTOM, LOGO_W, LOGO_W * lh / lw, mask="auto")
    c.restoreState()

doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                      title="SYNTHETIKA - Notice v" + VERSION, author="NINE HANDS | DSP")
doc.addPageTemplates([PageTemplate(id="cover", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="fc")], onPage=cover_bg),
                      PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")], onPage=deco)])
doc.build(story())
shutil.rmtree(TMP, ignore_errors=True)
print(OUT)
