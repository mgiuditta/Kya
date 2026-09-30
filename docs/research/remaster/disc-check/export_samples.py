#!/usr/bin/env python3
"""Export the representative sample set to samples/ (+ index.csv). Picks were chosen by eye from contact sheets."""
import sys, os, json, csv, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kya_tex import *
import os
ROOT = os.environ.get('KYA_CDEURO', 'bin/MAC/CDEURO')
HERE = os.path.dirname(os.path.abspath(__file__))
j = json.load(open(os.path.join(HERE, 'out/survey.json')))
B = j['bindings']
FMT = {4: 'PSMT4', 8: 'PSMT8', 32: 'PSMCT32'}
# (img_hash, pal_hash or None=first seen, category, short name)
PICKS = [
 ('f6bcabb719ec71cc', None, 'character-kya-skin-clothes', 'kya_body_atlas'),
 ('0304dd8dceff198a', None, 'terrain-stone', 'kronos_stone_tiles'),
 ('d5cb85e3d994411c', None, 'terrain-rock', 'kronos_rock'),
 ('f08434c65e2efa03', None, 'effect-lava', 'kronos_lava_edge'),
 ('c0a093a176b87b26', None, 'ui-artwork-512', 'bonus_kya_art0'),
 ('b7ee89591a184a7c', None, 'effect-decal', 'kya_bounce'),
]
# (map file, sheet index, category, short name)
SHEET = [
 ('map_misc.txt', 11, 'character-native-atlas', 'aton_native_atlas'),
 ('map_misc.txt', 19, 'sky', 'sky_gradient_l1'),
 ('map_misc.txt', 27, 'sky', 'sky_sunset_l10'),
 ('map_misc.txt', 21, 'foliage-alpha', 'tree_canopy'),
 ('map_misc.txt', 1, 'ui-hud-digit', 'counter_digit0'),
 ('map_misc.txt', 16, 'terrain-bark', 'bark_tiling'),
 ('map_l1s1.txt', 5, 'sky-backdrop', 'l1_backdrop_sunset'),
 ('map_l1s1.txt', 6, 'foliage-alpha', 'l1_leaves_cluster_4bit'),
 ('map_l1s1.txt', 18, 'foliage-alpha', 'l1_star_leaf'),
 ('map_l1s1.txt', 21, 'terrain-wood', 'l1_wood_tiling'),
 ('map_l1s1.txt', 36, 'foliage-tiling', 'l1_leaves_tiling'),
 ('map_l1s1.txt', 32, 'foliage-alpha', 'l1_grass_edge'),
 ('map_l1s1.txt', 10, 'foliage-small', 'l1_flower_32'),
 ('map_l1s1.txt', 7, 'effect-flare', 'l1_sun_flare_ct32'),
 ('map_l1s1.txt', 34, 'terrain-moss', 'l1_moss_ground'),
 ('map_l7.txt', 0, 'terrain-wood', 'l7_wood_planks'),
 ('map_l7.txt', 1, 'terrain-rock', 'l7_rock_4bit'),
 ('map_l7.txt', 16, 'terrain-stone', 'l7_swirl_stone'),
 ('map_l7.txt', 33, 'foliage-alpha', 'l7_vines_cutout'),
 ('map_l7.txt', 25, 'tiny', 'l7_tiny_8x8'),
 ('map_fx.txt', 22, 'effect-particle', 'fx_lightning'),
 ('map_fx.txt', 32, 'effect-flipbook', 'fx_explosion_sheet'),
 ('map_fx.txt', 15, 'effect-particle', 'fx_glow_16'),
 ('map_fx.txt', 11, 'effect-particle', 'fx_blue_ring'),
 ('map_fx.txt', 41, 'effect-smoke', 'fx_smoke'),
 ('map_ui.txt', 1, 'ui-icon', 'ui_icon_64'),
 ('map_ui.txt', 7, 'ui-icon', 'ui_icon_32_4bit'),
 ('map_ui.txt', 45, 'ui-panel', 'ui_panel_256'),
 ('map_ui.txt', 46, 'ui-logo-glow', 'ui_glow_logo'),
]

def find(img, pal=None):
    for b in B:
        if b['img_hash'] == img and (pal is None or b.get('pal_hash') == pal):
            return b
    raise KeyError(img)

sel = []
for img, pal, cat, nm in PICKS:
    sel.append((find(img, pal), cat, nm))
for mf, idx, cat, nm in SHEET:
    line = open(os.path.join(HERE, mf)).read().splitlines()[idx].split()
    sel.append((find(line[-1]), cat, nm))
# font
sel.append((next(b for b in B if b['entry'].endswith('.fon')), 'ui-font', 'font_medium'))
# palette variants of one image (LVL03 switch arrow: 6 palettes that change the decoded pixels)
seen = []
for b in B:
    if b['img_hash'] != '9f430406e69e2915':
        continue
    ph = j['pairstats']['%s|%s' % (b['img_hash'], b.get('pal_hash'))]['px_hash']
    if ph not in seen:
        seen.append(ph)
        if len(seen) <= 3:
            sel.append((b, 'palette-variant', 'arrow_palvariant%d' % len(seen)))

out = os.path.join(HERE, 'samples')
os.makedirs(out, exist_ok=True)
for f in os.listdir(out):
    if f.endswith('.png'):
        os.remove(os.path.join(out, f))
stats = j['pairstats']
with open(os.path.join(out, 'index.csv'), 'w', newline='') as fh:
    w = csv.writer(fh)
    w.writerow(['filename', 'source_bank', 'g2d_entry', 'material_off', 'layer', 'format', 'clut', 'width', 'height', 'mips',
                'palettes_on_tex', 'alpha_min', 'alpha_max', 'category', 'img_hash', 'pal_hash'])
    for i, (b, cat, nm) in enumerate(sel):
        wd, ht, px, t0, xf = decode_rec(ROOT, b)
        fn = '%02d_%s_%s_%dx%d.png' % (i, nm, FMT[b['bpp']], wd, ht)
        to_png(wd, ht, px, os.path.join(out, fn))
        st = stats.get('%s|%s' % (b['img_hash'], b.get('pal_hash')), {})
        clut = ('%s %dx%d CT32' % (b.get('pal_tag'), b.get('pal_w'), b.get('pal_h'))) if b.get('pal_hash') else ''
        w.writerow([fn, b['bank'], b['entry'].split('\\')[-1], b['mat'], b['layer'], FMT[b['bpp']], clut, wd, ht, b['mips'],
                    b['npal'], st.get('amin'), st.get('amax'), cat, b['img_hash'], b.get('pal_hash') or ''])
        print(fn, cat)
