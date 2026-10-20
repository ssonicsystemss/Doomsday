import sys, os, json, random, pickle, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from tune import render, vec, report
from feat import analyse
# usage: search.py <ref-substring> <note> <iters> <base.json> <space.json> <out.json> [hold]
refs = {r['name']: r for r in pickle.load(open(os.path.join(os.path.dirname(__file__), '..', 'ref_feats.pkl'), 'rb'))}
ref = [r for n, r in refs.items() if sys.argv[1] in n][0]
note, iters = int(sys.argv[2]), int(sys.argv[3])
base = json.load(open(sys.argv[4])); space = json.load(open(sys.argv[5])); hold = float(sys.argv[7]) if len(sys.argv) > 7 else 1.6
tag = os.path.basename(sys.argv[6]).replace('.json', '')
def score(r):
    d = vec(r) - vec(ref)
    s = np.linalg.norm(d)
    if np.isfinite(ref['f0']) and np.isfinite(r['f0']): s += 0.02 * abs(1200 * np.log2(r['f0'] / ref['f0']))
    return s
best = dict(base); r = analyse(render(tag, note, best, hold)); bs = score(r)
report(ref); report(r, ref)
rng = random.Random(1)
for it in range(iters):
    cand = dict(best)
    for k in rng.sample(list(space), k=min(len(space), rng.choice([1, 2, 3]))):
        lo, hi, kind = space[k]
        if kind == 'int': cand[k] = rng.randint(int(lo), int(hi))
        elif kind == 'log': cand[k] = round(float(np.exp(rng.uniform(np.log(lo), np.log(hi)))), 3)
        else: cand[k] = round(rng.uniform(lo, hi), 3)
    try: r = analyse(render(tag, note, cand, hold))
    except Exception as e: continue
    s = score(r)
    if s < bs: best, bs = cand, s; print(f'{it:3d} {s:6.2f}', {k: cand[k] for k in space}); report(r, ref)
json.dump(best, open(sys.argv[6], 'w'), indent=1)
r = analyse(render(tag, note, best, hold)); r['name'] = 'BEST ' + tag; report(r, ref)
