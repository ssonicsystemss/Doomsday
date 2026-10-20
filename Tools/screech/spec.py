import sys, glob, os, numpy as np
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from scipy.signal import stft
sys.path.insert(0, os.path.dirname(__file__)); from feat import load
files = sorted(glob.glob(sys.argv[1] + '/*.wav'))
cols = 4; rows = (len(files)+cols-1)//cols
fig, axs = plt.subplots(rows, cols, figsize=(cols*5, rows*3.0), facecolor='black')
for ax, f in zip(axs.flat, files):
    sr, x = load(f); m = x.mean(1)[: int(sr*1.7)]
    fr, t, Z = stft(m, sr, nperseg=2048, noverlap=2048-256)
    P = 20*np.log10(np.abs(Z)+1e-9); P -= P.max()
    ax.pcolormesh(t, fr, P, vmin=-80, vmax=0, cmap='magma', shading='auto')
    ax.set_yscale('symlog', linthresh=200); ax.set_ylim(50, 16000)
    ax.set_title(os.path.basename(f).replace('FHSP_Hardstyle_Sounds_','FHSP ').replace('SCREECH_HOUSE_-_','SH ')[:34], color='w', fontsize=9)
    ax.tick_params(colors='gray', labelsize=6)
for ax in axs.flat[len(files):]: ax.axis('off')
plt.tight_layout(); plt.savefig(sys.argv[2], dpi=70, facecolor='black')
