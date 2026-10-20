import sys, os, numpy as np
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from scipy.signal import stft
sys.path.insert(0, os.path.dirname(__file__)); from feat import load
pairs = [a.split('=') for a in sys.argv[2:]]
fig, axs = plt.subplots(len(pairs), 2, figsize=(12, 2.6*len(pairs)), facecolor='black', squeeze=False)
for row, (a, b) in zip(axs, pairs):
    for ax, f in zip(row, (a, b)):
        sr, x = load(f); m = x.mean(1)[: int(sr*1.7)]
        fr, t, Z = stft(m, sr, nperseg=2048, noverlap=2048-256)
        P = 20*np.log10(np.abs(Z)+1e-9); P -= P.max()
        ax.pcolormesh(t, fr, P, vmin=-70, vmax=0, cmap='magma', shading='auto'); ax.set_yscale('symlog', linthresh=200); ax.set_ylim(50, 16000)
        ax.set_title(os.path.basename(f)[:40], color='w', fontsize=8); ax.tick_params(colors='gray', labelsize=6)
plt.tight_layout(); plt.savefig(sys.argv[1], dpi=65, facecolor='black')
