import sys, json, re, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kya_tex import *
from PIL import Image, ImageDraw
import os
ROOT = os.environ.get('KYA_CDEURO', 'bin/MAC/CDEURO')
j = json.load(open('out/survey.json'))
pat, out, n = sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 48
seen = set(); sel = []
for b in j['bindings']:
    if re.search(pat, b['bank'] + '|' + b['entry'], re.I):
        k = (b['img_hash'], b.get('pal_hash'))
        if k in seen: continue
        seen.add(k); sel.append(b)
        if len(sel) >= n: break
T = 128; cols = 8
sheet = Image.new('RGBA', (cols * T, ((len(sel) + cols - 1) // cols) * (T + 12)), (60, 60, 90, 255))
dr = ImageDraw.Draw(sheet)
for i, b in enumerate(sel):
    w, h, px, t0, xf = decode_rec(ROOT, b)
    im = Image.new('RGBA', (w, h)); im.putdata([(r, g, bb, min(255, a * 255 // 128)) for r, g, bb, a in px])
    im.thumbnail((T, T))
    bg = Image.new('RGBA', im.size, (255, 0, 255, 255)); bg.alpha_composite(im)
    x, y = (i % cols) * T, (i // cols) * (T + 12)
    sheet.paste(bg, (x, y))
    dr.text((x + 2, y + T), '%d %dx%d b%d' % (i, w, h, b['bpp']), fill=(255, 255, 255, 255))
    print(i, b['bank'], b['entry'].split('\\')[-1], b['mat'], b['layer'], w, h, b['bpp'], b['img_hash'])
sheet.save(out)
