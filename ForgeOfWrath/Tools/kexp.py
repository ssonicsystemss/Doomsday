import sys, os, json, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from kmatch import *
j = json.load(open('tune/' + sys.argv[1] + '.json')); t = Target(j['ref'])
variants = {'base': {}}
for a in sys.argv[2:]:
    name, kv = a.split('/', 1)
    variants[name] = dict(x.split('=', 1) for x in kv.split(','))
lines = []
for n, ch in variants.items():
    p = dict(j['params'])
    for k, v in ch.items(): p[k] = v if k in ('amp', 'drive_env') else float(v)
    lines.append(jobline(n, j['note'], p, min(t.len + 0.05, 1.35)))
render(lines, SHM + '/exp')
for n in variants: print(n, [round(float(v), 2) for v in distance(t, load(f'{SHM}/exp/{n}.wav'))[1]], round(distance(t, load(f'{SHM}/exp/{n}.wav'))[0], 2))
print(json.dumps({k: (round(v, 2) if isinstance(v, float) else v) for k, v in j['params'].items()}))
