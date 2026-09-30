"""Inventory sample rates / channels / durations of all audio on the Kya disc.

Stdlib only. Usage: python3 audio_rates.py <CDEURO dir> [json_out]

Categories:
  sfx_vag      VAGp samples embedded in *.BNK (edSoundPrepareSampleLoad, _edSoundPlay.cpp:201)
  music_wave   waves in IECS 'Vagi' chunks of music instrument banks (port/Audio/edMusicData.cpp)
  streamch_vag LEVEL/*/STREAMCH/*.VAG (SoundSampleEntry::LoadStreamCh, Audio.cpp:3147)
  cine_mib     LEVEL/*/STREAM/*.MIB, format from the MIH records in LEVELIOP.BNK
               (CAudioManager::LoadGlobalSoundFunc_00184a70, Audio.cpp:2124; GlobalSound_FileData)
  pss_audio    MOVIES/*.PSS SShd header in MPEG-PS private stream 1
"""
import collections, hashlib, json, re, struct, sys
from pathlib import Path

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else 'bin/MAC/CDEURO')
OUT = Path(sys.argv[2]) if len(sys.argv) > 2 else None


def bank_entries(d):
    """Yield (index, category, subtype, offset, size) of an uncompressed Eden KNAB bank."""
    if d[8:12] != b'KNAB':
        return
    count, = struct.unpack_from('<I', d, 40)
    table, types = struct.unpack_from('<II', d, 48)
    for i in range(count):
        sub, cat = struct.unpack_from('<HH', d, 8 + types + i * 4)
        off, size = struct.unpack_from('<II', d, 8 + table + i * 16)
        yield i, cat, sub, off, size


def adpcm_samples(data):
    """Samples up to (and including) the first block with the end flag (bit 0 of byte 1)."""
    n = 0
    for b in range(0, len(data) - 15, 16):
        n += 28
        if data[b + 1] & 1:
            break
    return n


records = collections.defaultdict(list)

# ---- BNK scan: embedded VAGp samples, music banks, MIH records --------------
seen_vag, seen_wave = set(), set()
mih = {}  # (level, NAME) -> (rate, ch, blk, cnt)
for bnk in sorted(ROOT.rglob('*.BNK')):
    d = bnk.read_bytes()
    level = bnk.relative_to(ROOT).parts[1] if bnk.parts[-3] == 'LEVEL' or 'LEVEL' in bnk.parts else ''
    for m in re.finditer(rb'VAGp', d):
        o = m.start()
        if o + 0x30 > len(d):
            continue
        size, rate = struct.unpack_from('>II', d, o + 12)
        if not (1000 <= rate <= 96000) or size == 0 or o + 0x30 + size > len(d):
            continue
        body = d[o + 0x30:o + 0x30 + size]
        h = hashlib.sha1(body).hexdigest()
        if h in seen_vag:
            continue
        seen_vag.add(h)
        n = adpcm_samples(body)
        records['sfx_vag'].append(dict(file=str(bnk.relative_to(ROOT)), off=o, rate=rate, ch=1,
                                       samples=n, dur=n / rate,
                                       name=d[o + 0x20:o + 0x30].split(b'\0')[0].decode('latin1')))
    ents = list(bank_entries(d))
    heads = [(i, off, size) for i, cat, sub, off, size in ents if d[off:off + 8] == b'IECSsreV' and d[off + 16:off + 24] == b'IECSdaeH']
    bodies = {i: (off, size) for i, cat, sub, off, size in ents}
    for i, off, size in heads:
        hdr = d[off:off + size]
        bodysize, = struct.unpack_from('<I', hdr, 32)
        vagi, = struct.unpack_from('<I', hdr, 48)
        assert hdr[vagi:vagi + 8] == b'IECSigaV'
        maximum, = struct.unpack_from('<I', hdr, vagi + 12)
        offs = [struct.unpack_from('<I', hdr, vagi + 16 + k * 4)[0] for k in range(maximum + 1)]
        waves = [(struct.unpack_from('<IH', hdr, vagi + e)) for e in offs if e != 0xFFFFFFFF]
        starts = sorted({w[0] for w in waves}) + [bodysize]
        # the body (BD) is the entry of matching size, normally the next one
        bd = next((bodies[j] for j in (i + 1, i - 1) if j in bodies and bodies[j][1] == bodysize), None)
        for start, rate in waves:
            end = next(s for s in starts if s > start)
            if bd:
                raw = d[bd[0] + start:bd[0] + end]
                n = adpcm_samples(raw)
                key = hashlib.sha1(raw).hexdigest()
            else:
                n = (end - start) // 16 * 28
                key = (bnk.name, start, rate)
            if key in seen_wave:
                continue
            seen_wave.add(key)
            records['music_wave'].append(dict(file=str(bnk.relative_to(ROOT)), rate=rate, ch=1,
                                              samples=n, dur=n / rate))
    for m in re.finditer(rb'([\x20-\x7e]{1,200})\.mib\0', d, re.I):
        o = m.end()
        o = (o + 3) & ~3
        rate, ch, blk, cnt = struct.unpack_from('<iiii', d, o)
        name = m.group(1).decode().replace('\\', '/').split('/')[-1].upper()
        mih[(level, name)] = (rate, ch, blk, cnt)

# ---- STREAMCH VAG files ------------------------------------------------------
seen = set()
for p in sorted(ROOT.rglob('*.VAG')):
    d = p.read_bytes()
    h = hashlib.sha1(d).hexdigest()
    if h in seen:
        continue
    seen.add(h)
    size, rate = struct.unpack_from('>II', d, 12)
    n = adpcm_samples(d[0x30:0x30 + size])
    records['streamch_vag'].append(dict(file=str(p.relative_to(ROOT)), rate=rate, ch=1, samples=n,
                                        dur=n / rate, name=d[0x20:0x30].split(b'\0')[0].decode('latin1')))

# ---- MIB cinematic streams ---------------------------------------------------
missing = []
for p in sorted(ROOT.rglob('*.MIB')):
    level = p.parts[-3]
    key = (level, p.stem.upper())
    if key not in mih:
        missing.append(str(p.relative_to(ROOT)))
        continue
    rate, ch, blk, cnt = mih[key]
    size = p.stat().st_size
    n = size // (ch * 16) * 28
    records['cine_mib'].append(dict(file=str(p.relative_to(ROOT)), rate=rate, ch=ch, blk=blk, cnt=cnt,
                                    size_ok=(size == ch * blk * cnt), samples=n, dur=n / rate))

# ---- PSS movies --------------------------------------------------------------
for p in sorted(ROOT.rglob('*.PSS')):
    with p.open('rb') as f:
        d = f.read(1 << 20)
    o = d.find(b'SShd')
    rec = dict(file=str(p.relative_to(ROOT)))
    if o >= 0:
        hsize, codec, rate, ch, inter = struct.unpack_from('<IiIII', d, o + 4)
        b = d.find(b'SSbd', o)
        bdsize, = struct.unpack_from('<I', d, b + 4)
        rec.update(codec={1: 'PCM16LE', 0x10: 'PS-ADPCM'}.get(codec, hex(codec)), rate=rate, ch=ch,
                   interleave=inter, ssbd_bytes=bdsize)
        if codec == 1:
            rec['dur'] = bdsize / (rate * ch * 2)
        # private_stream_1 (0x1BD) payload starts 'ff a0 00 00' then SShd; 0xA0 = audio track id
        rec['substream'] = hex(d[o - 3])
    s = d.find(b'\x00\x00\x01\xb3')
    if s >= 0:
        w = (d[s + 4] << 4) | (d[s + 5] >> 4)
        h = ((d[s + 5] & 15) << 8) | d[s + 6]
        fr = {1: 23.976, 2: 24, 3: 25, 4: 29.97, 5: 30}.get(d[s + 7] & 15)
        rec.update(video=f'{w}x{h}@{fr}')
    records['pss_audio'].append(rec)


def summary(cat):
    rs = [r for r in records[cat] if 'rate' in r]
    hist = collections.Counter((r['rate'], r['ch']) for r in rs)
    dur = collections.defaultdict(float)
    for r in rs:
        dur[(r['rate'], r['ch'])] += r.get('dur', 0)
    print(f'\n## {cat}: {len(rs)} unique items, total {sum(dur.values()) / 60:.1f} min')
    print('| rate Hz | ch | count | total min |')
    print('|---|---|---|---|')
    for k, c in sorted(hist.items(), key=lambda kv: -kv[1]):
        print(f'| {k[0]} | {k[1]} | {c} | {dur[k] / 60:.2f} |')


for cat in ('sfx_vag', 'music_wave', 'streamch_vag', 'cine_mib', 'pss_audio'):
    summary(cat)
if missing:
    print(f'\nMIB files without MIH record: {len(missing)}', missing[:10])
mibs = records['cine_mib']
print('\nMIB size == ch*blk*cnt for', sum(r['size_ok'] for r in mibs), '/', len(mibs))
print('MIB interleave block sizes:', collections.Counter(r['blk'] for r in mibs))
durs = sorted(r['dur'] for r in mibs)
if durs:
    print(f'MIB durations: min {durs[0]:.2f}s median {durs[len(durs) // 2]:.2f}s max {durs[-1]:.2f}s')
print('\nPSS:')
for r in records['pss_audio']:
    print(r)
if OUT:
    OUT.write_text(json.dumps(records, indent=1))
