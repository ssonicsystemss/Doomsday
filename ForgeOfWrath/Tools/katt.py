import sys, os, json, numpy as np, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
sys.path.insert(0, os.path.dirname(__file__))
from kmatch import *
out = sys.argv[1]; names = sys.argv[2:]
lines = []; T = {}
for n in names:
    j = json.load(open(f'tune/{n}.json')); T[n] = (j, Target(j['ref']))
    lines.append(jobline(n, j['note'], j['params'], min(T[n][1].len + 0.05, 1.35)))
render(lines, SHM + '/at')
fig, ax = plt.subplots(len(names), 2, figsize=(18, 2.2 * len(names)), squeeze=False)
for i, n in enumerate(names):
    j, t = T[n]; y = load(f'{SHM}/at/{n}.wav'); y /= np.abs(y).max(); x = t.x / np.abs(t.x).max()
    m = int(0.1 * SR)
    ax[i][0].plot(np.arange(m) / SR * 1000, x[:m], lw=0.6); ax[i][0].set_title('REF ' + n, fontsize=8); ax[i][0].set_ylim(-1.05, 1.05)
    ax[i][1].plot(np.arange(m) / SR * 1000, y[:m], lw=0.6, color='C1'); ax[i][1].set_title('FORGE ' + n, fontsize=8); ax[i][1].set_ylim(-1.05, 1.05)
plt.tight_layout(); plt.savefig(out, dpi=55)
