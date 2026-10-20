import sys, os, json, numpy as np, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
sys.path.insert(0, os.path.dirname(__file__))
from kmatch import *
def harm(x, f0, a_s=0.15, b_s=0.32, N=30):
    a, b = int(a_s * SR), int(min(len(x) / SR - 0.03, b_s) * SR)
    seg = x[a:b] * np.hanning(b - a); n = 1 << 18
    P = np.abs(np.fft.rfft(seg, n)) ** 2; f = np.fft.rfftfreq(n, 1 / SR)
    out = []
    for k in range(1, N + 1):
        sel = (f > f0 * (k - 0.3)) & (f < f0 * (k + 0.3)); out.append(P[sel].max())
    out = 10 * np.log10(np.array(out) + 1e-20); return out - out.max()
out = sys.argv[1]; names = sys.argv[2:]
lines = []; T = {}
for n in names:
    j = json.load(open(f'tune/{n}.json')); T[n] = (j, Target(j['ref']))
    lines.append(jobline(n, j['note'], j['params'], min(T[n][1].len + 0.05, 1.35)))
render(lines, SHM + '/hm')
fig, ax = plt.subplots(len(names), 1, figsize=(10, 2.2 * len(names)), squeeze=False)
for i, n in enumerate(names):
    j, t = T[n]; f0 = np.nanmedian(t.f0[(np.arange(t.nf) * HOP / SR > 0.15) & (np.arange(t.nf) * HOP / SR < 0.3)])
    y = load(f'{SHM}/hm/{n}.wav')
    ax[i][0].plot(range(1, 31), harm(t.x, f0), 'o-', label='ref'); ax[i][0].plot(range(1, 31), harm(y, f0), 's-', label='forge')
    ax[i][0].set_title(f'{n} f0 {f0:.1f}', fontsize=9); ax[i][0].set_ylim(-60, 3); ax[i][0].legend(fontsize=7)
plt.tight_layout(); plt.savefig(out, dpi=60)
