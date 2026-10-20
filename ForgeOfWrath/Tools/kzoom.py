import sys, os, numpy as np, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
sys.path.insert(0, os.path.dirname(__file__))
from kmatch import *
out = sys.argv[1]; pairs = sys.argv[2:]
fig, ax = plt.subplots(len(pairs) // 2, 3, figsize=(20, 3.2 * len(pairs) // 2), squeeze=False)
for r in range(len(pairs) // 2):
    for k, f in enumerate(pairs[r * 2: r * 2 + 2]):
        x = load(f); x = x[onset(x):]; x /= np.abs(x).max()
        a, b = int(0.15 * SR), int(0.35 * SR)
        seg = x[a:b]; S = 20 * np.log10(np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) + 1e-9); fr = np.fft.rfftfreq(len(seg), 1 / SR)
        S -= S.max()
        # smooth
        sm = np.convolve(S, np.ones(15) / 15, 'same')
        ax[r][0].semilogx(fr[1:], sm[1:], lw=0.8, label=os.path.basename(f)[:25]); ax[r][0].set_xlim(30, 20000); ax[r][0].set_ylim(-90, 0); ax[r][0].legend(fontsize=7)
        z = x[int(0.2 * SR): int(0.245 * SR)]
        ax[r][1 + k].plot(np.arange(len(z)) / SR * 1000, z, lw=0.6); ax[r][1 + k].set_title(os.path.basename(f)[:30], fontsize=8)
plt.tight_layout(); plt.savefig(out, dpi=60)
