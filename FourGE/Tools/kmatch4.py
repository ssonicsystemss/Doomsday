# 4GE (rebuild) matcher: tunes a few module-01 parameters so a rendered kick matches a reference kick
#   python3 kmatch4.py <ref.wav> <midi note> <fine ct> <out.json>
import sys, os, json, subprocess, tempfile, numpy as np, random
from scipy.io import wavfile
from scipy.signal import resample_poly, butter, sosfilt
BIN = '/home/claude/doomsday/FourGE/build/RenderTest_artefacts/Release/RenderTest'
TMP = tempfile.mkdtemp()
SR = 44100
def load(p):
    sr, x = wavfile.read(p); x = x.astype(np.float64)
    if x.ndim > 1: x = x.mean(1)
    if sr != SR: x = resample_poly(x, SR, sr)
    x = x / (np.abs(x).max() + 1e-12)
    a = np.argmax(np.abs(x) > 0.05); return x[max(0, a - 5):]
edges = 30 * 2 ** (np.arange(0, 29) / 3)      # 1/3 oct 30 Hz .. 16 kHz
sosLP = butter(4, 500, fs=SR, output='sos')
def feats(x):
    seg = x[int(0.06 * SR):int(0.35 * SR)]
    seg = np.pad(seg, (0, max(0, int(0.29 * SR) - len(seg))))
    X = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2; f = np.fft.rfftfreq(len(seg), 1 / SR)
    e = np.array([X[(f >= edges[k]) & (f < edges[k + 1])].sum() for k in range(len(edges) - 1)])
    lt = 10 * np.log10(e / e.sum() + 1e-9)
    env = []
    for t in np.arange(0, 0.7, 0.01):
        s = x[int(t * SR):int((t + 0.01) * SR)]
        env.append(20 * np.log10(np.sqrt(np.mean(s ** 2)) + 1e-5) if len(s) else -100)
    lp = sosfilt(sosLP, x)
    f0 = []
    for t in (0.004, 0.01, 0.018, 0.03, 0.045, 0.065, 0.09, 0.13, 0.2):
        a = int(t * SR); n = int(0.012 * SR); s = lp[a:a + n + 1600]
        if len(s) < n + 1600: f0.append(0); continue
        lo, hi = SR // 800, SR // 28
        ac = np.array([np.dot(s[:n], s[l:l + n]) for l in range(lo, hi)]); b = np.argmax(ac)
        for i in range(1, b):
            if ac[i] > 0.85 * ac[b] and ac[i] >= ac[i - 1] and ac[i] >= ac[i + 1]: b = i; break
        f0.append(SR / (b + lo))
    return lt, np.array(env), np.log2(np.maximum(np.array(f0), 20))
def dist(a, b):
    lt = np.mean(np.abs(a[0] - b[0]))
    m = (a[1] > -45) | (b[1] > -45)
    env = np.mean(np.abs(np.maximum(a[1], -50) - np.maximum(b[1], -50))[m]) if m.any() else 0
    f0 = np.mean(np.abs(a[2] - b[2])) * 12
    return lt + 0.5 * env + 0.6 * f0
SPACE = {  # id: (lo, hi, log)
    'pe_start': (90, 600, 1), 'pe_time': (15, 400, 1), 'pe_curve': (0, 0.8, 0), 'tail_drive': (0.2, 1, 0), 'tail_tone': (1500, 20000, 1),
    'tail_resonance': (0, 0.6, 0), 'tail_decay': (100, 2500, 1), 'tail_sustain': (0.3, 1, 0), 'tail_length': (150, 1500, 1),
    'body_level': (-30, -3, 0), 'body_pitch': (60, 400, 1), 'sub_level': (-30, -3, 0), 'punch_level': (-30, 0, 0), 'punch_pitch': (120, 1200, 1),
    'punch_drive': (0, 1, 0), 'tail_level': (-8, 0, 0)}
def render(params, note, idx):
    t = f'{TMP}/p{idx}.txt'; w = f'{TMP}/p{idx}.wav'
    with open(t, 'w') as fh:
        fh.write(f'note {note}\nseconds 1.0\ncore_auto_length 0\ncore_length 2000\n')
        for k, v in params.items(): fh.write(f'{k} {v}\n')
    subprocess.run([BIN, TMP, '--render', t, w], stdout=subprocess.DEVNULL)
    return load(w)
def clip(k, v): lo, hi, _ = SPACE[k]; return min(hi, max(lo, v))
def rnd(k):
    lo, hi, lg = SPACE[k]; return float(np.exp(random.uniform(np.log(lo), np.log(hi)))) if lg else random.uniform(lo, hi)
def main():
    ref, note, fine, out = sys.argv[1], int(sys.argv[2]), float(sys.argv[3]), sys.argv[4]
    R = feats(load(ref)); random.seed(1); idx = [0]
    def cost(p):
        idx[0] += 1; return dist(feats(render(p, note, idx[0] % 4)), R)
    best = None
    for char in (1, 2, 3, 5):
        for src in (0, 2):
            base = {'tail_character': char, 'tail_source': src, 'pe_fine': fine}
            cand = None
            for _ in range(40):
                p = dict(base); p.update({k: rnd(k) for k in SPACE}); c = cost(p)
                if cand is None or c < cand[0]: cand = (c, p)
            c, p = cand
            for rnd_i in range(3):
                step = 0.35 / (rnd_i + 1)
                for k in SPACE:
                    lo, hi, lg = SPACE[k]
                    for sgn in (1, -1):
                        q = dict(p)
                        q[k] = clip(k, p[k] * (np.exp(sgn * step) if lg else 1) + (0 if lg else sgn * step * (hi - lo)))
                        cq = cost(q)
                        if cq < c: c, p = cq, q
            print(f'char {char} src {src}: {c:.2f}', flush=True)
            if best is None or c < best[0]: best = (c, p)
    json.dump({'ref': ref, 'note': note, 'cost': best[0], 'params': best[1]}, open(out, 'w'), indent=1)
    print('best', best[0], best[1])
main()
