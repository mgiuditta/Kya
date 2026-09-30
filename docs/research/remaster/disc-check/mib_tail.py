"""Measure trailing digital silence and end-flag position in cinematic MIB streams."""
import json, sys, statistics, collections
from pathlib import Path
root = Path(sys.argv[1]); recs = json.load(open(sys.argv[2]))['cine_mib']
tails, flags = [], collections.Counter()
for r in recs:
    d = (root / r['file']).read_bytes(); ch, blk = r['ch'], r['blk']
    nblk = len(d) // (ch * blk)
    # rebuild channel 0 stream
    c0 = b''.join(d[i*ch*blk:i*ch*blk+blk] for i in range(nblk))
    n = len(c0)//16; silent = 0
    for f in range(n-1, -1, -1):
        fr = c0[f*16:f*16+16]
        if any(fr[2:]): break
        silent += 1
    tails.append(silent*28/r['rate'])
    ends = [f for f in range(n) if c0[f*16+1] & 1]
    flags['has_end_flag' if ends else 'no_end_flag'] += 1
tails.sort()
print('files', len(tails), 'trailing digital-silence seconds: min %.3f p10 %.3f median %.3f p90 %.3f max %.3f' % (
    tails[0], tails[len(tails)//10], statistics.median(tails), tails[len(tails)*9//10], tails[-1]))
print(flags)
