#!/usr/bin/env python3
"""Build histogram tables (markdown) from out/survey.json produced by kya_tex.py."""
import json, sys, collections as C

j = json.load(open(sys.argv[1] if len(sys.argv) > 1 else 'out/survey.json'))
R, B, P = j['records'], j['bindings'], j['pairstats']
FMT = {4: 'PSMT4 (4-bit CLUT)', 8: 'PSMT8 (8-bit CLUT)', 32: 'PSMCT32 (direct)', 16: 'PSMCT16', 24: 'PSMCT24'}

def tbl(title, counter, head=('key', 'count'), total=None, sort_key=None):
    out = ['', '### ' + title, '', '| %s | %s |' % head, '|---|---:|']
    items = sorted(counter.items(), key=sort_key) if sort_key else counter.most_common()
    for k, v in items:
        pct = ' (%.1f%%)' % (100.0 * v / total) if total else ''
        out.append('| %s | %d%s |' % (k, v, pct))
    return '\n'.join(out)

def bucket(w, h):
    return '%dx%d' % (w, h)

def sbucket(w, h):
    m = max(w, h)
    for lim in (8, 16, 32, 64, 128, 256, 512):
        if m <= lim:
            return '<=%d' % lim
    return '>512'

lines = []
# ---- instances: one per T2D. bitmap chunk in every G2D file on the disc
lines.append('## Totals\n')
uimg = {}
for b in B:
    uimg.setdefault(b['img_hash'], b)
pal_per_img = C.defaultdict(set)
for b in B:
    pal_per_img[b['img_hash']].add(b.get('pal_hash'))
lines.append('- G2D files parsed: %d (banks + loose)' % len({r['src'] for r in R}))
lines.append('- T2D bitmap instances (all G2D files): %d' % len(R))
lines.append('- material-layer bindings (bitmap+palette used by a material layer): %d' % len(B))
lines.append('- unique images (sha1 of level-0 texel data): %d' % len(uimg))
lines.append('- unique image+palette combinations: %d' % len(P))
tex_area = sum(b['w'] * b['h'] for b in uimg.values())
lines.append('- sum of level-0 texels over unique images: %.1f Mtexel (x4 upscale RGBA8 = %.0f MB, x2 = %.0f MB)'
             % (tex_area / 1e6, tex_area * 16 * 4 / 2**20, tex_area * 4 * 4 / 2**20))

U = list(uimg.values())
nu = len(U)
lines.append(tbl('Pixel format, unique images', C.Counter(FMT.get(b['bpp'], b['bpp']) for b in U), ('format', 'unique images'), nu))
lines.append(tbl('Pixel format, all bitmap instances', C.Counter(FMT.get(r['bpp'], r['bpp']) for r in R), ('format', 'instances'), len(R)))
lines.append(tbl('Size (WxH), unique images', C.Counter(bucket(b['w'], b['h']) for b in U), ('size', 'unique images'), nu))
lines.append(tbl('Max dimension bucket, unique images', C.Counter(sbucket(b['w'], b['h']) for b in U), ('max(w,h)', 'unique images'), nu,
                 sort_key=lambda kv: int(kv[0][2:]) if kv[0][2:].isdigit() else 9999))
lines.append(tbl('Square vs non-square, unique images', C.Counter('square' if b['w'] == b['h'] else 'non-square' for b in U), ('shape', 'unique images'), nu))
lines.append(tbl('Mip levels (incl. base), unique images', C.Counter(b['mips'] for b in U), ('mips', 'unique images'), nu, sort_key=lambda kv: kv[0]))
lines.append(tbl('Palettes stored per TEX (bHasPalette count), bindings', C.Counter(b['npal'] for b in B), ('palettes on TEX', 'bindings'), len(B), sort_key=lambda kv: kv[0]))
lines.append(tbl('Distinct palettes (by PA32 payload) seen with the same image, whole game', C.Counter(len(s - {None}) for s in pal_per_img.values()), ('distinct palettes', 'unique images'), nu, sort_key=lambda kv: kv[0]))
dec_per_img = C.defaultdict(set)
for b in B:
    st = P.get('%s|%s' % (b['img_hash'], b.get('pal_hash')))
    if st:
        dec_per_img[b['img_hash']].add(st['px_hash'])
lines.append(tbl('Distinct decoded RGBA results per image (palette variants that actually change pixels)', C.Counter(len(s) for s in dec_per_img.values()), ('distinct decoded variants', 'unique images'), nu, sort_key=lambda kv: kv[0]))
lines.append('\nUnique decoded RGBA textures (img+palette, deduped by output pixels): %d' % len({s['px_hash'] for s in P.values()}))
clut = C.Counter()
for b in B:
    if b.get('pal_hash'):
        t0 = b['tex0']
        clut['%s %dx%d, CPSM=%s, CSM%d' % (b.get('pal_tag'), b.get('pal_w') or 0, b.get('pal_h') or 0,
                                           {0: 'CT32', 2: 'CT16', 10: 'CT16S'}.get(t0['CPSM'], t0['CPSM']), t0['CSM'] + 1)] += 1
lines.append(tbl('CLUT format, bindings with palette', clut, ('CLUT', 'bindings'), sum(clut.values())))
up = C.Counter()
for b in B:
    fmt = FMT.get(b['bpp'], b['bpp']).split()[0]
    d = b['dpsm'][0] if b['dpsm'] else None
    up['%s uploaded as %s' % (fmt, {0: 'PSMCT32', 19: 'PSMT8', 20: 'PSMT4'}.get(d, d))] += 1
lines.append(tbl('Upload DPSM for level 0 (GS swizzle needed when differs), bindings', up, ('texture / upload', 'bindings'), len(B)))
lines.append(tbl('TEX0.TCC (1 = texture alpha used), bindings', C.Counter(b['tex0']['TCC'] for b in B), ('TCC', 'bindings'), len(B)))
lines.append(tbl('UV-animated (ANIM ST) textures, bindings', C.Counter(b['animst'] for b in B), ('animST', 'bindings'), len(B)))

# ---- alpha
ac = C.Counter()
rng = C.Counter()
for k, s in P.items():
    if s['amin'] == 0x80 and s['amax'] == 0x80:
        ac['fully opaque (all A=0x80)'] += 1
    elif s['ndistinct'] == 2 and s['amin'] == 0 and s['amax'] == 0x80:
        ac['1-bit cutout (A in {0,0x80})'] += 1
    elif s['amax'] <= 0x80:
        ac['graded alpha within 0..0x80'] += 1
    else:
        ac['has A > 0x80'] += 1
    rng['max A: ' + ('<=0x80' if s['amax'] <= 0x80 else '>0x80 (up to 0x%02x)' % s['amax'])] += 1
lines.append(tbl('Alpha usage, unique image+palette combos (decoded)', ac, ('alpha class', 'combos'), len(P)))
lines.append(tbl('Alpha max range', rng, ('range', 'combos'), len(P)))
over = [s for s in P.values() if s['amax'] > 0x80]
lines.append('\nCombos with any A>0x80: %d; of those mean fraction of texels >0x80: %.3f; max A seen 0x%02x' %
             (len(over), (sum(s['frac_over'] for s in over) / len(over)) if over else 0, max((s['amax'] for s in P.values()), default=0)))
lines.append('Combos with some A=0 texels: %d' % sum(1 for s in P.values() if s['amin'] == 0))
lines.append('Combos whose alpha is all 0 (colour-only / additive?): %d' % sum(1 for s in P.values() if s['amax'] == 0))

# ---- per folder
fold = C.defaultdict(lambda: [0, set(), 0, C.Counter()])
for r in R:
    fold[r['folder']][0] += 1
for b in B:
    f = fold[b['folder']]
    f[1].add(b['img_hash'])
    f[3][b['bpp']] += 1
out = ['', '### Per top-level folder', '', '| folder | bitmap instances | unique images (in folder) | 4-bit / 8-bit / 32-bit bindings |', '|---|---:|---:|---|']
def fkey(k):
    import re
    m = re.match(r'LEVEL/LEVEL_(\d+)', k)
    return (0, int(m.group(1)), '') if m else (1, 0, k)
for k in sorted(fold, key=fkey):
    f = fold[k]
    out.append('| %s | %d | %d | %d / %d / %d |' % (k, f[0], len(f[1]), f[3][4], f[3][8], f[3][32]))
lines.append('\n'.join(out))
# unique images shared across folders
imgfold = C.defaultdict(set)
for b in B:
    imgfold[b['img_hash']].add(b['folder'])
lines.append('\nUnique images appearing in >1 top-level folder: %d; in >5: %d' %
             (sum(1 for s in imgfold.values() if len(s) > 1), sum(1 for s in imgfold.values() if len(s) > 5)))
lines.append('\nDecode errors: %s' % (j['errors'] or 'none'))
lines.append('Non-G2D bank entries containing a T2DA chunk (not surveyed): %s' % j['nonG2D_with_T2D'])
lines.append('Bank entry types seen: %s' % dict(C.Counter(j['entry_ext']).most_common()))
print('\n'.join(lines))
