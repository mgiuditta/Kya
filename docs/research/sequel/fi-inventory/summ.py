import sys, collections
D = '/private/tmp/claude-501/-Users-matteo-dev-kya/682bc2f7-7a84-4467-ab12-be38ca6b7cbc/scratchpad/'
exec(open(D + 'classmap.py').read())
rows = [l.rstrip('\n').split('\t') for l in open(D + sys.argv[1])]
C = [(int(r[1], 16), int(r[2])) for r in rows if r[0] == 'C']
A = [r for r in rows if r[0] == 'A']
print("SceneCfg classes:", len(C), "sum", sum(c for _, c in C), "actors", len(A))
by = collections.defaultdict(list)
for r in A:
    by[int(r[2], 16)].append(r)
full = len(sys.argv) > 2
for cid, cnt in C:
    rs = by[cid]
    sec = collections.Counter(int(r[4]) for r in rs)
    names = sorted(set(r[3] for r in rs))
    lim = 1000 if full else 10
    nm = ', '.join(names[:lim]) + (' ...(+%d)' % (len(names) - lim) if len(names) > lim else '')
    print(f"0x{cid:02x} {CLS.get(cid,'?'):26s} cfg={cnt:3d} n={len(rs):3d} sect={dict(sorted(sec.items()))} | {nm}")
