import sys, json, subprocess, numpy as np, matplotlib
matplotlib.use('Agg'); import matplotlib.pyplot as plt
sys.argv = ['x']; exec(open('tools/kmatch4.py').read().split('def main')[0])
out = 'cmp.png'; jobs = ['kick2d', 'kickg', 'k84']
fig, ax = plt.subplots(len(jobs), 4, figsize=(20, 4 * len(jobs)))
for r, n in enumerate(jobs):
    j = json.load(open(f'match2/{n}.json')); ref = load(j['ref']); y = render(j['params'], j['note'], 9)
    np.save(f'match2/{n}_y.npy', y)
    for c, (sig, lab) in enumerate(((ref, 'REF ' + n), (y, '4GE'))):
        s = sig[:int(0.8 * SR)]
        ax[r][c * 2].plot(np.arange(len(s)) / SR * 1000, s, lw=0.5); ax[r][c * 2].set_title(lab + ' wave'); ax[r][c * 2].set_xlim(0, 800)
        ax[r][c * 2 + 1].specgram(s + 1e-9, NFFT=1024, Fs=SR, noverlap=896, cmap='magma', vmin=-120); ax[r][c * 2 + 1].set_ylim(0, 8000); ax[r][c * 2 + 1].set_title(lab + ' spectrogram')
plt.tight_layout(); plt.savefig(out, dpi=55)
