import sys, glob, os, numpy as np
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from scipy.signal import stft
sys.path.insert(0, os.path.dirname(__file__)); from kfeat import load, SR
files = sys.argv[2:] if len(sys.argv) > 2 else sorted(glob.glob(sys.argv[1] + '/*.wav'))
if len(sys.argv) > 2: out = sys.argv[1]
else: out = 'kspec.png'
cols = 4; rows = (len(files) + cols - 1) // cols
fig, axs = plt.subplots(rows * 2, cols, figsize=(cols * 4.2, rows * 3.6), facecolor='black', gridspec_kw={'height_ratios': [2.2, 1] * rows})
for i, f in enumerate(files):
    r, c = (i // cols) * 2, i % cols
    x = load(f); m = x.mean(1)[: int(SR * 0.7)]
    fr, t, Z = stft(m, SR, nperseg=2048, noverlap=2048 - 128)
    P = 20 * np.log10(np.abs(Z) + 1e-9); P -= P.max()
    ax = axs[r][c]; ax.pcolormesh(t, fr, P, vmin=-70, vmax=0, cmap='inferno', shading='auto'); ax.set_yscale('log'); ax.set_ylim(30, 12000)
    ax.set_title(os.path.basename(f)[:34], color='w', fontsize=8); ax.tick_params(colors='gray', labelsize=6)
    ax2 = axs[r + 1][c]; ax2.plot(np.arange(len(m)) / SR, m, lw=0.3, color='orange'); ax2.set_xlim(0, 0.7); ax2.set_ylim(-1.05, 1.05); ax2.tick_params(colors='gray', labelsize=6); ax2.set_facecolor('black')
for j in range(len(files), rows * cols):
    axs[(j // cols) * 2][j % cols].axis('off'); axs[(j // cols) * 2 + 1][j % cols].axis('off')
plt.tight_layout(); plt.savefig(out, dpi=62, facecolor='black')
