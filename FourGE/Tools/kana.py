# kick feature report: f0 track, band shares, tail centroid, envelope
import sys, numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly
def load(p):
    sr, x = wavfile.read(p)
    x = x.astype(np.float64)
    if x.dtype.kind == 'i' or np.abs(x).max() > 2: x = x / (np.abs(x).max() + 1e-12)
    if x.ndim > 1: x = x.mean(1)
    if sr != 44100: x = resample_poly(x, 44100, sr)
    x = x / (np.abs(x).max() + 1e-12)
    # start at the first sample > 5 % of peak
    a = np.argmax(np.abs(x) > 0.05); return x[max(0, a - 10):]
def f0(x, t, w=0.025):
    a = int(t * 44100); n = int(w * 44100); seg = x[a:a + n + 2000]
    if len(seg) < n + 2000: return 0
    lo, hi = 44100 // 1200, 44100 // 30
    ac = np.array([np.dot(seg[:n], seg[l:l + n]) for l in range(lo, hi)])
    b = np.argmax(ac); best = ac[b]
    for i in range(1, b):
        if ac[i] > 0.9 * best and ac[i] >= ac[i - 1] and ac[i] >= ac[i + 1]: b = i; break
    return 44100 / (b + lo)
edges = [20, 60, 150, 400, 1500, 5000, 16000]
def bands(x, a, b):
    seg = x[int(a * 44100):int(b * 44100)]
    if len(seg) < 4096: return [0] * 6
    X = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2; f = np.fft.rfftfreq(len(seg), 1 / 44100)
    e = np.array([X[(f >= edges[k]) & (f < edges[k + 1])].sum() for k in range(6)]); return 10 * np.log10(e / e.sum() + 1e-12)
def centroid(x, a, b):
    seg = x[int(a * 44100):int(b * 44100)]; X = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))); f = np.fft.rfftfreq(len(seg), 1 / 44100)
    return (X * f).sum() / X.sum()
def length(x):
    env = np.abs(x); idx = np.where(env > 10 ** (-40 / 20))[0]; return idx[-1] / 44100 if len(idx) else 0
for p in sys.argv[1:]:
    x = load(p)
    fs = [f0(x, t) for t in (0.003, 0.015, 0.03, 0.06, 0.1, 0.2)]
    rm = [20 * np.log10(np.sqrt(np.mean(x[int(t * 44100):int((t + 0.02) * 44100)] ** 2)) + 1e-9) for t in (0, 0.05, 0.1, 0.2, 0.3, 0.4)]
    bd = bands(x, 0.08, 0.35)
    name = p.split('/')[-1][-34:]
    print(f"{name:34s} f0 " + " ".join(f"{v:5.0f}" for v in fs) + " | rms " + " ".join(f"{v:4.0f}" for v in rm) +
          " | bands(80-350ms) " + " ".join(f"{v:5.1f}" for v in bd) + f" | cent {centroid(x, 0.08, 0.35):5.0f} | len {length(x):.2f}")
