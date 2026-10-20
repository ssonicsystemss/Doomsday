import sys, os, json, numpy as np, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
sys.path.insert(0, os.path.dirname(__file__))
from kmatch import *
names = sys.argv[2:]; out = sys.argv[1]
fig, ax = plt.subplots(len(names), 4, figsize=(20, 3.2 * len(names)), squeeze=False)
for r, n in enumerate(names):
    j = json.load(open(f'{os.path.dirname(__file__)}/../tune/{n}.json'))
    t = Target(j['ref']); secs = min(t.len + 0.05, 1.35)
    render([jobline('best', j['note'], j['params'], secs)], f'{SHM}/cmp')
    y = load(f'{SHM}/cmp/best.wav'); y = y / np.abs(y).max()
    for c, (sig, lab) in enumerate([(t.x / np.abs(t.x).max(), 'REF ' + os.path.basename(j['ref'])[:30]), (y, f'FORGE {j["score"]:.2f}')]):
        B, env, f0, _fl = feats(sig, t.nf)
        a = ax[r][c * 2]; a.imshow(B.T, origin='lower', aspect='auto', cmap='magma', extent=[0, t.nf * HOP / SR * 1000, 0, NB])
        a.set_yticks([0, 10, 20, 30, 40]); a.set_yticklabels([f'{EDGES[i]:.0f}' for i in [0, 10, 20, 30, 40]]); a.set_title(lab, fontsize=9)
        a2 = a.twinx(); a2.plot(np.arange(t.nf) * HOP / SR * 1000, f0, 'c', lw=0.8); a2.set_ylim(30, 400); a2.set_yscale('log')
        b = ax[r][c * 2 + 1]; tt = np.arange(len(sig)) / SR * 1000; b.plot(tt[:int(t.nf * HOP)], sig[:int(t.nf * HOP)], lw=0.4); b.set_ylim(-1.05, 1.05)
plt.tight_layout(); plt.savefig(out, dpi=60)
