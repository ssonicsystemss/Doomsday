import sys, os, json, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from kmatch import *
def flat(x, lo, hi):
    a, b = int(0.08 * SR), int(min(len(x) / SR - 0.03, 0.35) * SR)
    seg = x[a:b]; fr = np.lib.stride_tricks.sliding_window_view(seg, 4096)[::1024] * np.hanning(4096)
    P = (np.abs(np.fft.rfft(fr, axis=1)) ** 2).mean(0); f = np.fft.rfftfreq(4096, 1 / SR)
    s = P[(f > lo) & (f < hi)] + 1e-20
    return 10 * np.log10(np.exp(np.mean(np.log(s))) / np.mean(s))
names = sys.argv[1:]
lines = []; T = {}
for n in names:
    j = json.load(open(f'tune/{n}.json')); T[n] = (j, Target(j['ref']))
    lines.append(jobline(n, j['note'], j['params'], min(T[n][1].len + 0.05, 1.35)))
render(lines, SHM + '/fl')
for n in names:
    y = load(f'{SHM}/fl/{n}.wav'); x = T[n][1].x
    print(f'{n:6s} ref  mid {flat(x,300,1500):6.1f} hf {flat(x,1500,8000):6.1f}   forge mid {flat(y,300,1500):6.1f} hf {flat(y,1500,8000):6.1f}')
