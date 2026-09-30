#!/usr/bin/env python3
"""Kya: Dark Lineage texture survey / exporter (stdlib + optional Pillow).

Walks every .BNK bank and loose .G2D on the extracted disc, parses G2D texture
files the same way the game/port does, emulates the GS local-memory upload
(BITBLTBUF/TRXPOS/TRXREG image transfers) and reads the texture back through
TEX0 (PSM/TBP0/TBW/CBP/CPSM/CSM) to get RGBA.

Format knowledge reused from the repo:
  - Bank layout:       src/EdenLib/edBank/sources/edBankFile.cpp (edCBankFileHeader,
                       TreeInfo_Recurse name tree, get_index, get_entry)
  - G2D chunk layout:  src/b-witch/ed3D.cpp ed3DPrepareG2DManageStruct / ed3DPreparePointer
                       (pointers are file-start-relative offsets, fixed up via REAL)
  - Material graph:    port/KyaTexture/src/Texture.cpp (MAT. -> LAY. -> TEX. -> T2D./palette)
  - Upload packets:    Texture.cpp ProcessUploadCommandList (REF tag = image data,
                       BITBLTBUF/TRXPOS/TRXREG A+D, mips first then palette)
  - Render regs:       Texture.cpp ProcessRenderCommandList (TEX0_1 from material cmd buffer)
  - Swizzle tables:    port/Windows/Renderer/Vulkan/src/pcsx2/TextureUpload/src/GSTables.cpp

Usage: kya_tex.py <CDEURO dir> <out dir> [--export-all]
"""
import os, sys, struct, hashlib, json, collections

# ---------------------------------------------------------------- GS swizzle
blockTable32 = [[0,1,4,5,16,17,20,21],[2,3,6,7,18,19,22,23],[8,9,12,13,24,25,28,29],[10,11,14,15,26,27,30,31]]
blockTable16 = [[0,2,8,10],[1,3,9,11],[4,6,12,14],[5,7,13,15],[16,18,24,26],[17,19,25,27],[20,22,28,30],[21,23,29,31]]
blockTable8 = blockTable32
blockTable4 = blockTable16
columnTable32 = [[0,1,4,5,8,9,12,13],[2,3,6,7,10,11,14,15],[16,17,20,21,24,25,28,29],[18,19,22,23,26,27,30,31],
                 [32,33,36,37,40,41,44,45],[34,35,38,39,42,43,46,47],[48,49,52,53,56,57,60,61],[50,51,54,55,58,59,62,63]]
columnTable16 = [
 [0,2,8,10,16,18,24,26,1,3,9,11,17,19,25,27],[4,6,12,14,20,22,28,30,5,7,13,15,21,23,29,31],
 [32,34,40,42,48,50,56,58,33,35,41,43,49,51,57,59],[36,38,44,46,52,54,60,62,37,39,45,47,53,55,61,63],
 [64,66,72,74,80,82,88,90,65,67,73,75,81,83,89,91],[68,70,76,78,84,86,92,94,69,71,77,79,85,87,93,95],
 [96,98,104,106,112,114,120,122,97,99,105,107,113,115,121,123],[100,102,108,110,116,118,124,126,101,103,109,111,117,119,125,127]]
columnTable8 = [
 [0,4,16,20,32,36,48,52,2,6,18,22,34,38,50,54],[8,12,24,28,40,44,56,60,10,14,26,30,42,46,58,62],
 [33,37,49,53,1,5,17,21,35,39,51,55,3,7,19,23],[41,45,57,61,9,13,25,29,43,47,59,63,11,15,27,31],
 [96,100,112,116,64,68,80,84,98,102,114,118,66,70,82,86],[104,108,120,124,72,76,88,92,106,110,122,126,74,78,90,94],
 [65,69,81,85,97,101,113,117,67,71,83,87,99,103,115,119],[73,77,89,93,105,109,121,125,75,79,91,95,107,111,123,127],
 [128,132,144,148,160,164,176,180,130,134,146,150,162,166,178,182],[136,140,152,156,168,172,184,188,138,142,154,158,170,174,186,190],
 [161,165,177,181,129,133,145,149,163,167,179,183,131,135,147,151],[169,173,185,189,137,141,153,157,171,175,187,191,139,143,155,159],
 [224,228,240,244,192,196,208,212,226,230,242,246,194,198,210,214],[232,236,248,252,200,204,216,220,234,238,250,254,202,206,218,222],
 [193,197,209,213,225,229,241,245,195,199,211,215,227,231,243,247],[201,205,217,221,233,237,249,253,203,207,219,223,235,239,251,255]]

def _ct4():
    # columnTable4 rows as in GSTables.cpp (16 x 32); built from its 4-row pattern per column group
    base = [
     [0,8,32,40,64,72,96,104,2,10,34,42,66,74,98,106,4,12,36,44,68,76,100,108,6,14,38,46,70,78,102,110],
     [16,24,48,56,80,88,112,120,18,26,50,58,82,90,114,122,20,28,52,60,84,92,116,124,22,30,54,62,86,94,118,126],
     [65,73,97,105,1,9,33,41,67,75,99,107,3,11,35,43,69,77,101,109,5,13,37,45,71,79,103,111,7,15,39,47],
     [81,89,113,121,17,25,49,57,83,91,115,123,19,27,51,59,85,93,117,125,21,29,53,61,87,95,119,127,23,31,55,63],
     [192,200,224,232,128,136,160,168,194,202,226,234,130,138,162,170,196,204,228,236,132,140,164,172,198,206,230,238,134,142,166,174],
     [208,216,240,248,144,152,176,184,210,218,242,250,146,154,178,186,212,220,244,252,148,156,180,188,214,222,246,254,150,158,182,190],
     [129,137,161,169,193,201,225,233,131,139,163,171,195,203,227,235,133,141,165,173,197,205,229,237,135,143,167,175,199,207,231,239],
     [145,153,177,185,209,217,241,249,147,155,179,187,211,219,243,251,149,157,181,189,213,221,245,253,151,159,183,191,215,223,247,255]]
    # rows 8..15 = rows 0..7 + 256 (columns 2,3 repeat the pattern of columns 0,1)
    return base + [[v + 256 for v in r] for r in base]
columnTable4 = _ct4()

PSMCT32, PSMCT24, PSMCT16, PSMCT16S, PSMT8, PSMT4 = 0x00, 0x01, 0x02, 0x0a, 0x13, 0x14
PSM_NAMES = {0x00:'PSMCT32',0x01:'PSMCT24',0x02:'PSMCT16',0x0a:'PSMCT16S',0x13:'PSMT8',0x14:'PSMT4',
             0x1b:'PSMT8H',0x24:'PSMT4HL',0x2c:'PSMT4HH'}

def pa32(x, y, bp, bw):   # word address
    bn = bp + (y & ~0x1f) * bw + ((x >> 1) & ~0x1f) + blockTable32[(y >> 3) & 3][(x >> 3) & 7]
    return (bn << 6) + columnTable32[y & 7][x & 7]
def pa16(x, y, bp, bw):   # halfword address
    bn = bp + ((y >> 1) & ~0x1f) * bw + ((x >> 1) & ~0x1f) + blockTable16[(y >> 3) & 7][(x >> 4) & 3]
    return (bn << 7) + columnTable16[y & 7][x & 15]
def pa8(x, y, bp, bw):    # byte address
    bn = bp + ((y >> 1) & ~0x1f) * (bw >> 1) + ((x >> 2) & ~0x1f) + blockTable8[(y >> 4) & 3][(x >> 4) & 7]
    return (bn << 8) + columnTable8[y & 15][x & 15]
def pa4(x, y, bp, bw):    # nibble address
    bn = bp + ((y >> 2) & ~0x1f) * (bw >> 1) + ((x >> 2) & ~0x1f) + blockTable4[(y >> 4) & 7][(x >> 5) & 3]
    return (bn << 9) + columnTable4[y & 15][x & 31]

GS_BYTES = 4 * 1024 * 1024
_addr_cache = {}
def addr_list(psm, bp, bw, x0, y0, w, h):
    """Byte (or nibble for T4) addresses, row-major, masked to 4MB."""
    key = (psm, bp, bw, x0, y0, w, h)
    r = _addr_cache.get(key)
    if r is not None:
        return r
    if psm in (PSMCT32, PSMCT24):
        r = [(pa32(x, y, bp, bw) * 4) % GS_BYTES for y in range(y0, y0 + h) for x in range(x0, x0 + w)]
    elif psm in (PSMCT16, PSMCT16S):
        r = [(pa16(x, y, bp, bw) * 2) % GS_BYTES for y in range(y0, y0 + h) for x in range(x0, x0 + w)]
    elif psm == PSMT8:
        r = [pa8(x, y, bp, bw) % GS_BYTES for y in range(y0, y0 + h) for x in range(x0, x0 + w)]
    elif psm == PSMT4:
        r = [pa4(x, y, bp, bw) % (GS_BYTES * 2) for y in range(y0, y0 + h) for x in range(x0, x0 + w)]
    else:
        raise ValueError('unsupported psm %#x' % psm)
    if len(_addr_cache) > 4000:
        _addr_cache.clear()
    _addr_cache[key] = r
    return r

def gs_write(mem, src, bitblt, trxpos, trxreg):
    dbp = (bitblt >> 32) & 0x3fff
    dbw = (bitblt >> 48) & 0x3f
    dpsm = (bitblt >> 56) & 0x3f
    dsax = (trxpos >> 32) & 0x7ff
    dsay = (trxpos >> 48) & 0x7ff
    rrw = trxreg & 0xfff
    rrh = (trxreg >> 32) & 0xfff
    addrs = addr_list(dpsm, dbp, dbw, dsax, dsay, rrw, rrh)
    if dpsm == PSMCT32:
        for i, a in enumerate(addrs):
            mem[a:a + 4] = src[i * 4:i * 4 + 4]
    elif dpsm == PSMCT24:
        for i, a in enumerate(addrs):
            mem[a:a + 3] = src[i * 3:i * 3 + 3]
    elif dpsm in (PSMCT16, PSMCT16S):
        for i, a in enumerate(addrs):
            mem[a:a + 2] = src[i * 2:i * 2 + 2]
    elif dpsm == PSMT8:
        for i, a in enumerate(addrs):
            mem[a] = src[i]
    elif dpsm == PSMT4:
        for i, a in enumerate(addrs):
            b = src[i >> 1]
            v = (b >> 4) if (i & 1) else (b & 0xf)
            ba = a >> 1
            if a & 1:
                mem[ba] = (mem[ba] & 0x0f) | (v << 4)
            else:
                mem[ba] = (mem[ba] & 0xf0) | v
    else:
        raise ValueError('unsupported dpsm %#x' % dpsm)
    return dpsm, rrw, rrh

def c16_to_rgba(v):
    r = (v & 0x1f) << 3; g = ((v >> 5) & 0x1f) << 3; b = ((v >> 10) & 0x1f) << 3
    a = 0x80 if (v & 0x8000) else 0x00   # TEXA TA1=0x80/TA0=0 assumed (AEM=0)
    return (r, g, b, a)

def read_clut(mem, cbp, cpsm, csm, nent):
    """Return list of (r,g,b,a) in raw PS2 alpha (0..0x80)."""
    if csm != 0:
        raise ValueError('CSM2 not supported')
    if nent == 256:
        w, h = 16, 16
    else:
        w, h = 8, 2
    if cpsm == PSMCT32:
        addrs = addr_list(PSMCT32, cbp, 1, 0, 0, w, h)
        lin = [tuple(mem[a:a + 4]) for a in addrs]
    else:
        addrs = addr_list(PSMCT16 if cpsm == PSMCT16 else PSMCT16S, cbp, 1, 0, 0, w, h)
        lin = [c16_to_rgba(mem[a] | (mem[a + 1] << 8)) for a in addrs]
    if nent == 256:
        # CSM1 entry order swaps bits 3 and 4 of the index
        return [lin[(i & ~0x18) | ((i & 8) << 1) | ((i & 16) >> 1)] for i in range(256)]
    return lin

def read_indices(mem, psm, tbp, tbw, w, h):
    addrs = addr_list(psm, tbp, tbw, 0, 0, w, h)
    if psm == PSMT8:
        return bytes(mem[a] for a in addrs)
    # T4
    return bytes((mem[a >> 1] >> 4) if (a & 1) else (mem[a >> 1] & 0xf) for a in addrs)

def read_direct(mem, psm, tbp, tbw, w, h, tcc):
    addrs = addr_list(psm, tbp, tbw, 0, 0, w, h)
    if psm == PSMCT32:
        return [tuple(mem[a:a + 4]) for a in addrs]
    if psm == PSMCT24:
        return [tuple(mem[a:a + 3]) + (0x80,) for a in addrs]
    return [c16_to_rgba(mem[a] | (mem[a + 1] << 8)) for a in addrs]

# ---------------------------------------------------------------- BNK
def bank_entries(d):
    """Yield (name, bytes) for each entry of an edCBankFileHeader bank (unpacked only)."""
    if d[8:12] != b'KNAB':
        return
    flags = struct.unpack_from('<I', d, 0xc)[0]
    if flags & 1:
        raise ValueError('LZ77-packed bank not supported')
    count = struct.unpack_from('<I', d, 0x28)[0]
    fhdo = struct.unpack_from('<i', d, 0x30)[0]
    namo = struct.unpack_from('<i', d, 0x38)[0]
    idxo = struct.unpack_from('<i', d, 0x3c)[0]
    # name tree (TreeInfo_Recurse): DFS, leaves in index order
    names = []
    if namo:
        p = [namo + 8]
        def rec(prefix):
            c = struct.unpack_from('b', d, p[0])[0]; p[0] += 1
            if c < 0:
                n = -c
                if n == 0x80:
                    n += d[p[0]]; p[0] += 1
                cnt = 0; sh = 0
                while True:
                    b = d[p[0]]; p[0] += 1
                    cnt |= (b & 0x7f) << sh; sh += 7
                    if b < 0x80:
                        break
                nm = d[p[0]:p[0] + n].decode('latin1'); p[0] += n
                for _ in range(cnt):
                    rec(prefix + [nm])
            else:
                n = c
                if n == 0x7f:
                    n += d[p[0]]; p[0] += 1
                nm = d[p[0]:p[0] + n].decode('latin1'); p[0] += n
                parts = prefix + [nm]
                # un-optimise "dir\.ext\name" -> "dir\name.ext"
                out = []
                i = 0
                while i < len(parts):
                    if parts[i].startswith('.') and i + 1 < len(parts):
                        out.append(parts[i + 1] + parts[i]); i += 2
                    else:
                        out.append(parts[i]); i += 1
                names.append('\\'.join(out))
        while d[p[0]] != 0 and len(names) < count:
            rec([])
    idx_to_name = {}
    if idxo and names:
        for i, nm in enumerate(names):
            if count < 0x100:
                fi = d[idxo + 8 + i]
            elif count < 0x10000:
                fi = struct.unpack_from('<H', d, idxo + 8 + i * 2)[0]
            else:
                fi = struct.unpack_from('<I', d, idxo + 8 + i * 4)[0]
            idx_to_name[fi] = nm
    for fi in range(count):
        off, size = struct.unpack_from('<ii', d, fhdo + 8 + fi * 16)
        yield idx_to_name.get(fi, '#%d' % fi), d[off:off + size]

# ---------------------------------------------------------------- G2D
def chunks(d, s, e):
    p = s
    while p + 16 <= e:
        tag = d[p:p + 4]
        size = struct.unpack_from('<i', d, p + 8)[0]
        yield p, tag, size
        if size <= 0:
            break
        p += size

def parse_bitmap(d, off):
    """off = chunk (T2D./PA32/...) offset. Returns dict."""
    w, h, bpp, mips, ppsx2 = struct.unpack_from('<HHHHI', d, off + 16)
    return dict(off=off, tag=d[off:off + 4].decode('latin1'), w=w, h=h, bpp=bpp, mips=mips, ppsx2=ppsx2)

def upload_cmds(d, ppsx2):
    """Return list of transfers [dict(src, bitblt, trxpos, trxreg)] from first PSX2 header."""
    pkt, n = struct.unpack_from('<ii', d, ppsx2)
    xfers = []
    cur = None
    for i in range(n):
        q = pkt + i * 16
        u0, u1, u2, u3 = struct.unpack_from('<IIII', d, q)
        cmdA, cmdB = struct.unpack_from('<QQ', d, q)
        if u2 == 0x0e:  # SCE_GIF_PACKED_AD giftag (Texture.cpp ProcessUploadCommandList)
            nxt2 = struct.unpack_from('<I', d, q + 16 + 8)[0] if i + 1 < n else None
            if nxt2 != 0x3f:  # not the trailing TEXFLUSH
                cur = {}
                xfers.append(cur)
        if cur is None:
            continue
        if (u0 >> 28) == 3:
            cur['src'] = u1
            cur['qwc'] = u0 & 0xffff
        if cmdB == 0x50: cur['bitblt'] = cmdA
        elif cmdB == 0x51: cur['trxpos'] = cmdA
        elif cmdB == 0x52: cur['trxreg'] = cmdA
    return [x for x in xfers if 'src' in x and 'bitblt' in x]

def find_tex0(d, pbuf, n):
    regs = {}
    for i in range(n):
        q = pbuf + i * 16
        if q + 16 > len(d):
            break
        cmdA = struct.unpack_from('<Q', d, q)[0]
        reg = struct.unpack_from('<I', d, q + 8)[0]
        if reg == 0x06:
            regs.setdefault('tex0_list', []).append(cmdA)
        if reg in (0x06, 0x08, 0x42, 0x47, 0x14, 0x34, 0x36) and reg not in regs:
            regs[reg] = cmdA
    return regs

def layer_tex0(regs, layer):
    l = regs.get('tex0_list', [])
    if not l:
        return None
    return l[layer] if layer < len(l) else l[0]

def decode_tex0(v):
    return dict(TBP0=v & 0x3fff, TBW=(v >> 14) & 0x3f, PSM=(v >> 20) & 0x3f, TW=(v >> 26) & 0xf,
                TH=(v >> 30) & 0xf, TCC=(v >> 34) & 1, TFX=(v >> 35) & 3, CBP=(v >> 37) & 0x3fff,
                CPSM=(v >> 51) & 0xf, CSM=(v >> 55) & 1, CSA=(v >> 56) & 0x1f, CLD=(v >> 61) & 7)

def parse_g2d(d):
    """Parse a G2D blob. Returns dict with bitmaps, palettes, and material layer bindings."""
    assert d[12:16] == b'.G2D', 'not a G2D'
    L = len(d)
    res = dict(bitmaps={}, palettes={}, bindings=[], nmat=0, anim=False)
    for p, tag, size in chunks(d, 16, L):
        if tag == b'T2DA':
            for q, t2, s2 in chunks(d, p + 16, p + size):
                if t2 != b'HASH':
                    res['bitmaps'][q] = parse_bitmap(d, q)
        elif tag == b'PALL':
            for q, t2, s2 in chunks(d, p + 16, p + size):
                if t2 != b'HASH':
                    res['palettes'][q] = parse_bitmap(d, q)
        elif tag == b'ANMA':
            res['anim'] = True
        elif tag == b'*2D*':
            for q, t2, s2 in chunks(d, p + 16, p + size):
                if t2 != b'MATA':
                    continue
                for r, t3, s3 in chunks(d, q + 16, q + s2):
                    if t3 != b'MAT.':
                        continue
                    res['nmat'] += 1
                    nb, _, mflags, pdma, pcmd, ncmd = struct.unpack_from('<BBHIIi', d, r + 16)
                    lays = struct.unpack_from('<4i', d, r + 32)
                    regs = find_tex0(d, pcmd, ncmd) if pcmd and pcmd + 16 <= L else {}
                    for li in range(min(nb, 4)):
                        lay = lays[li]
                        if lay <= 0 or lay + 0x30 > L:
                            continue
                        lf0, lf4 = struct.unpack_from('<II', d, lay + 16)
                        bhastex, palid, ptex = struct.unpack_from('<hHI', d, lay + 16 + 0x1c)
                        if not bhastex:
                            res['bindings'].append(dict(mat=r, layer=li, bitmap=None))
                            continue
                        t = ptex + 16
                        tex_hash_pdata = struct.unpack_from('<I', d, t + 8)[0]
                        npal, panim = struct.unpack_from('<iI', d, t + 0x10)
                        bmp = None
                        if tex_hash_pdata:
                            bmp = struct.unpack_from('<I', d, tex_hash_pdata + 8)[0]
                        pal = None
                        pal_all = []
                        for k in range(npal):
                            hc = struct.unpack_from('<I', d, t + 0x20 + k * 16 + 8)[0]
                            pal_all.append(struct.unpack_from('<I', d, hc + 8)[0])
                        if npal:
                            pal = pal_all[palid] if palid < npal else None
                        res['bindings'].append(dict(mat=r, layer=li, bitmap=bmp, palette=pal, pal_all=pal_all,
                                                    npal=npal, palid=palid, regs=regs, tex=ptex,
                                                    animst=bool(panim), mflags=mflags, lflags=(lf0, lf4)))
    return res

def decode_binding(d, g, b):
    """Decode level 0 of a bound (bitmap, palette) pair. Returns (w, h, rgba_list_rawalpha, info)."""
    bm = g['bitmaps'][b['bitmap']]
    pal = g['palettes'].get(b['palette']) if b.get('palette') else None
    src = pal if pal else bm
    if not src['ppsx2']:
        raise ValueError('no PSX2 upload list')
    xfers = upload_cmds(d, src['ppsx2'])
    v = layer_tex0(b['regs'], b['layer'])
    if v is None:
        raise ValueError('no TEX0 in material command buffer')
    t0 = decode_tex0(v)
    want = {4: PSMT4, 8: PSMT8, 32: PSMCT32, 24: PSMCT24, 16: PSMCT16}.get(bm['bpp'])
    if want is not None and t0['PSM'] != want and xfers:
        # Only layer-0 TEX0 is present in the material command buffer; rebuild TEX0 for this
        # layer from its own upload (TBP0 = DBP of level-0 transfer, CBP = DBP of palette transfer).
        x0 = xfers[0]
        dbw = (x0['bitblt'] >> 48) & 0x3f
        dpsm = (x0['bitblt'] >> 56) & 0x3f
        t0 = dict(t0, PSM=want, TBP0=(x0['bitblt'] >> 32) & 0x3fff, synth=True)
        if dpsm == want:
            t0['TBW'] = dbw
        else:
            t0['TBW'] = max(2, (bm['w'] + 63) // 64)
        if pal and len(xfers) > bm['mips']:
            t0['CBP'] = (xfers[bm['mips']]['bitblt'] >> 32) & 0x3fff
            t0['CPSM'] = PSMCT32 if pal['bpp'] == 32 else PSMCT16
    mem = bytearray(GS_BYTES)
    for x in xfers:
        gs_write(mem, d[x['src']:x['src'] + x['qwc'] * 16], x['bitblt'], x.get('trxpos', 0), x['trxreg'])
    w, h = bm['w'], bm['h']
    psm = t0['PSM']
    if psm in (PSMT8, PSMT4):
        idx = read_indices(mem, psm, t0['TBP0'], t0['TBW'], w, h)
        clut = read_clut(mem, t0['CBP'], t0['CPSM'], t0['CSM'], 256 if psm == PSMT8 else 16)
        px = [clut[i] for i in idx]
    else:
        px = read_direct(mem, psm, t0['TBP0'], t0['TBW'], w, h, t0['TCC'])
    return w, h, px, t0, xfers

# ---------------------------------------------------------------- walk
def iter_g2d_sources(root):
    for dp, dn, fn in os.walk(root):
        dn.sort()
        for f in sorted(fn):
            path = os.path.join(dp, f)
            rel = os.path.relpath(path, root)
            ext = f.upper().rsplit('.', 1)[-1]
            if ext == 'BNK':
                d = open(path, 'rb').read()
                for name, blob in bank_entries(d):
                    if blob[12:16] != b'.G2D':
                        # e.g. .fon fonts embed a full G2D file after their glyph table
                        k = blob.find(b'.G2D*2D*')
                        if k >= 12 and (k - 12) % 4 == 0:
                            ln = struct.unpack_from('<I', blob, k - 4)[0]
                            blob = blob[k - 12:k - 12 + ln]
                    yield rel, name, blob
            elif ext == 'G2D':
                yield rel, f, open(path, 'rb').read()

def top_folder(rel):
    parts = rel.replace('\\', '/').split('/')
    if parts[0] == 'LEVEL' and len(parts) > 2:
        return 'LEVEL/' + parts[1]
    return parts[0]

def main():
    root, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    records = []      # one per T2D bitmap instance
    bindrecs = []     # one per material layer binding
    errors = collections.Counter()
    nonG2D_with_T2D = collections.Counter()
    entry_ext = collections.Counter()
    for rel, name, blob in iter_g2d_sources(root):
        ext = name.rsplit('.', 1)[-1].lower() if '.' in name else '?'
        entry_ext[ext] += 1
        if len(blob) < 16 or blob[12:16] != b'.G2D':
            if b'T2DA' in blob:  # G2D-like data we could not locate
                nonG2D_with_T2D[ext] += 1
            continue
        try:
            g = parse_g2d(blob)
        except Exception as e:
            errors['parse: %s' % type(e).__name__] += 1
            continue
        src_id = rel + '|' + name
        # bitmap instances
        for off, bm in g['bitmaps'].items():
            lvl0 = None
            records.append(dict(src=src_id, folder=top_folder(rel), bank=rel, entry=name, off=off,
                                w=bm['w'], h=bm['h'], bpp=bm['bpp'], mips=bm['mips'], has_psx2=bool(bm['ppsx2'])))
        for b in g['bindings']:
            if b.get('bitmap') is None:
                continue
            rec = dict(src=src_id, folder=top_folder(rel), bank=rel, entry=name, bitmap=b['bitmap'],
                       palette=b.get('palette'), npal=b['npal'], animst=b['animst'], mat=b['mat'], layer=b['layer'])
            bm = g['bitmaps'].get(b['bitmap'])
            if bm is None:
                errors['binding->missing bitmap'] += 1
                continue
            rec.update(w=bm['w'], h=bm['h'], bpp=bm['bpp'], mips=bm['mips'])
            if b.get('palette'):
                pb = g['palettes'].get(b['palette'])
                rec['pal_tag'] = pb['tag'] if pb else None
                rec['pal_w'] = pb['w'] if pb else None
                rec['pal_h'] = pb['h'] if pb else None
            v = layer_tex0(b['regs'], b['layer'])
            if v is not None:
                rec['tex0'] = decode_tex0(v)
                rec['ntex0'] = len(b['regs']['tex0_list'])
            # content hashes: image = level-0 source data of bitmap; palette data
            try:
                src = g['palettes'][b['palette']] if b.get('palette') else bm
                xf = upload_cmds(blob, src['ppsx2'])
                nimg = bm['mips']
                imgx = xf[:nimg]
                palx = xf[nimg:nimg + 1] if b.get('palette') else []
                h1 = hashlib.sha1()
                for x in imgx[:1]:
                    h1.update(blob[x['src']:x['src'] + x['qwc'] * 16])
                rec['img_hash'] = h1.hexdigest()[:16]
                if palx:
                    x = palx[0]
                    # only the PA32 payload (w*h entries) is meaningful; the transfer is a full 16x16/8x2
                    # CLUT block and runs past the palette into unrelated file bytes
                    pb = g['palettes'][b['palette']]
                    nbytes = pb['w'] * pb['h'] * pb['bpp'] // 8
                    rec['pal_hash'] = hashlib.sha1(blob[x['src']:x['src'] + nbytes]).hexdigest()[:16]
                rec['dpsm'] = [(x['bitblt'] >> 56) & 0x3f for x in xf]
                rec['nxfer'] = len(xf)
                rec['img_xfers'] = len(imgx)
            except Exception as e:
                errors['upload: %s' % e] += 1
            bindrecs.append(rec)
    # decode unique (img, pal) pairs for alpha stats
    print('bitmaps', len(records), 'bindings', len(bindrecs), file=sys.stderr)
    blobs = {}
    for rel, name, blob in iter_g2d_sources(root):
        if len(blob) >= 16 and blob[12:16] == b'.G2D':
            blobs[rel + '|' + name] = blob
    pairs = {}
    for r in bindrecs:
        k = (r.get('img_hash'), r.get('pal_hash'))
        if k not in pairs:
            pairs[k] = r
    pairstats = {}
    gcache = {}
    import time
    t = time.time()
    for n, (k, r) in enumerate(pairs.items()):
        blob = blobs[r['src']]
        g = gcache.get(r['src'])
        if g is None:
            gcache.clear()
            g = gcache[r['src']] = parse_g2d(blob)
        b = next(b for b in g['bindings'] if b['mat'] == r['mat'] and b['layer'] == r['layer'])
        try:
            w, h, px, t0, xf = decode_binding(blob, g, b)
        except Exception as e:
            errors['decode: %s' % e] += 1
            continue
        al = collections.Counter(p[3] for p in px)
        pairstats['%s|%s' % k] = dict(px_hash=hashlib.sha1(bytes(c for p in px for c in p)).hexdigest()[:16], amin=min(al), amax=max(al), ndistinct=len(al),
                                      frac_opaque=al.get(0x80, 0) / len(px), frac_zero=al.get(0, 0) / len(px),
                                      frac_over=sum(v for a, v in al.items() if a > 0x80) / len(px),
                                      synth=bool(t0.get('synth')))
        if n % 500 == 0:
            print(n, len(pairs), '%.1fs' % (time.time() - t), file=sys.stderr)
    json.dump(dict(records=records, bindings=bindrecs, pairstats=pairstats, errors=errors, nonG2D_with_T2D=nonG2D_with_T2D,
                   entry_ext=entry_ext), open(os.path.join(out, 'survey.json'), 'w'), default=str)



def to_png(w, h, px, path):
    from PIL import Image
    im = Image.new('RGBA', (w, h))
    # PS2 alpha 0..0x80 -> 0..255
    im.putdata([(r, g, b, min(255, a * 255 // 128)) for r, g, b, a in px])
    im.save(path)
    return im

def get_blob(root, bank, entry):
    path = os.path.join(root, bank)
    if bank.upper().endswith('.BNK'):
        for name, blob in bank_entries(open(path, 'rb').read()):
            if name == entry:
                if blob[12:16] != b'.G2D':
                    k = blob.find(b'.G2D*2D*')
                    ln = struct.unpack_from('<I', blob, k - 4)[0]
                    blob = blob[k - 12:k - 12 + ln]
                return blob
        raise KeyError(entry)
    return open(path, 'rb').read()

def decode_rec(root, rec):
    blob = get_blob(root, rec['bank'], rec['entry'])
    g = parse_g2d(blob)
    b = next(b for b in g['bindings'] if b['mat'] == rec['mat'] and b['layer'] == rec['layer'])
    return decode_binding(blob, g, b)

if __name__ == '__main__':
    main()
