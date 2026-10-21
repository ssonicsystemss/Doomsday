"""Fit the 5CR34M 4LPH4 prototype engine to one reference screech (CMA-ES, from ForgeOfWrath/Tools/ktune.py).

python3 amatch.py <AlphaRender> <ref.wav> <outdir> note=<midi> tuneSt=<st> scoopSt=.. scoopMs=.. [fixed=value ...] [gens=40]
Writes <outdir>/best.txt (params), best.wav, log.txt. Features: 1/3-octave shape of the steady part, noisiness (flatness),
harmonic-to-noise, how much the brightness moves, coarse loudness envelope.
"""
import sys, os, subprocess, tempfile, numpy as np
from multiprocessing import Pool
from scipy.io import wavfile
from scipy.signal import stft
sys.path.insert(0, os.path.dirname(__file__))
from ref_features import load

SPACE = [  # name, lo, hi, log
    ('coreLevel', 0.0, 1.5, False), ('coreShape', 0.0, 1.0, False), ('hashLevel', 0.0, 2.0, False),
    ('hashDecayMs', 30.0, 3000.0, True), ('hashSustain', 0.0, 1.0, False),
    ('detuneCents', 0.0, 80.0, False), ('carrierSaw', 0.0, 1.0, False), ('ratio', 0.5, 4.0, False),
    ('index', 0.0, 10.0, False), ('chaos', 0.0, 1.5, False), ('chaosHz', 60.0, 6000.0, True),
    ('hashHp', 100.0, 4000.0, True), ('drive1Db', 0.0, 40.0, False), ('combMix', 0.0, 1.0, False),
    ('combFb', -0.9, 0.9, False), ('combRatio', 0.5, 4.0, False), ('formHz', 300.0, 5000.0, True),
    ('formGainDb', -6.0, 24.0, False), ('formQ', 0.5, 8.0, True), ('drive2Db', 0.0, 30.0, False),
    ('toneHz', 2000.0, 18000.0, True), ('lowCut', 40.0, 400.0, True),
]
BANDS = 1000 * 2 ** (np.arange(-10, 14) / 3)        # ~100 Hz .. 20 kHz centres

def band_shape(m, sr, t0, t1):
    seg = m[int(t0 * sr):int(t1 * sr)]
    ff, tt, Z = stft(seg, sr, nperseg=4096, noverlap=3072)
    avg = (np.abs(Z) ** 2).mean(1)
    tot = avg[(ff > 80) & (ff < 16000)].sum()
    out = []
    for c in BANDS[BANDS < 16000]:
        sel = (ff >= c / 2 ** (1 / 6)) & (ff < c * 2 ** (1 / 6))
        out.append(10 * np.log10(avg[sel].sum() / tot + 1e-12))
    return np.array(out)

def features(path, f0, t0=0.25, t1=1.2):
    sr, x = load(path); m = x.mean(1)
    a, b = int(t0 * sr), int(min(t1, len(m) / sr - 0.05) * sr)
    seg = m[a:b]
    ff, tt, Z = stft(seg, sr, nperseg=4096, noverlap=3072)
    P = np.abs(Z) ** 2; avg = P.mean(1)
    end = min(t1, len(m) / sr - 0.05)
    bands = np.concatenate ([band_shape (m, sr, 0.03, 0.35), band_shape (m, sr, 0.5, end)])
    flat = np.median(np.exp(np.mean(np.log(P + 1e-14), 0)) / (np.mean(P, 0) + 1e-14))
    cen = (ff[:, None] * P).sum(0) / (P.sum(0) + 1e-12)
    cmove = np.std(np.log2(cen + 1))
    mask = np.zeros_like(ff, bool)
    for h in range(1, int(8000 / f0) + 1): mask |= np.abs(ff - f0 * h) < max(1.5 * (ff[1] - ff[0]), f0 / 8)
    band8 = ff < 8000
    hnr = 10 * np.log10(avg[band8 & mask].sum() / (avg[band8 & ~mask].sum() + 1e-14))
    hop = int(0.04 * sr); e = np.array([np.sqrt(np.mean(m[i:i + hop] ** 2)) for i in range(0, int(1.2 * sr), hop)])
    env = 20 * np.log10(e / (e.max() + 1e-12) + 1e-6)
    return dict(bands=np.array(bands), flat=10 * np.log10(flat + 1e-9), hnr=hnr, cmove=cmove, env=env)

def loss(c, r):
    parts = dict(bands=np.mean(np.abs(c['bands'] - r['bands'])), flat=0.3 * abs(c['flat'] - r['flat']),
                 hnr=0.3 * abs(c['hnr'] - r['hnr']), cmove=8 * abs(c['cmove'] - r['cmove']),
                 env=0.15 * np.mean(np.abs(np.maximum(c['env'], -40) - np.maximum(r['env'], -40))))
    return sum(parts.values()), parts

def decode(x):
    out = {}
    for v, (k, lo, hi, lg) in zip(x, SPACE):
        out[k] = lo * (hi / lo) ** v if lg else lo + v * (hi - lo)
    return out

def sep_cma(f, x0, sigma, gens, lam=20, seed=1, log=None):   # from ForgeOfWrath/Tools/ktune.py
    rng = np.random.default_rng(seed)
    n = len(x0); mu = lam // 2
    w = np.log(mu + 0.5) - np.log(np.arange(1, mu + 1)); w /= w.sum(); mueff = 1 / np.sum(w ** 2)
    cs = (mueff + 2) / (n + mueff + 5); ds = 1 + 2 * max(0, np.sqrt((mueff - 1) / (n + 1)) - 1) + cs
    cc = 4 / (n + 4); c1 = 2 / ((n + 1.3) ** 2 + mueff) * (n + 2) / 3
    cmu = min(1 - c1, 2 * (mueff - 2 + 1 / mueff) / ((n + 2) ** 2 + mueff) * (n + 2) / 3)
    chiN = np.sqrt(n) * (1 - 1 / (4 * n) + 1 / (21 * n * n))
    m = np.array(x0, float); C = np.ones(n); ps = np.zeros(n); pc = np.zeros(n)
    best = (1e9, m.copy(), None)
    for g in range(gens):
        Z = rng.standard_normal((lam, n)); Y = Z * np.sqrt(C)
        X = np.clip(m + sigma * Y, 0, 1); Y = (X - m) / sigma
        fit = f(X)
        order = np.argsort([v[0] for v in fit])
        if fit[order[0]][0] < best[0]: best = (fit[order[0]][0], X[order[0]].copy(), fit[order[0]][1])
        ysel = Y[order[:mu]]; yw = w @ ysel
        m = np.clip(m + sigma * yw, 0, 1)
        ps = (1 - cs) * ps + np.sqrt(cs * (2 - cs) * mueff) * yw / np.sqrt(C)
        hs = np.linalg.norm(ps) / np.sqrt(1 - (1 - cs) ** (2 * (g + 1))) < (1.4 + 2 / (n + 1)) * chiN
        pc = (1 - cc) * pc + hs * np.sqrt(cc * (2 - cc) * mueff) * yw
        C = (1 - c1 - cmu) * C + c1 * pc ** 2 + cmu * (w @ (ysel ** 2))
        sigma *= np.exp((cs / ds) * (np.linalg.norm(ps) / chiN - 1)); sigma = min(sigma, 0.4)
        if log: log(g, best[0], best[2], sigma)
    return best

G = {}
def evaluate(x):
    p = decode(x)
    fd, path = tempfile.mkstemp(suffix='.wav', dir=G['tmp']); os.close(fd)
    args = [G['render'], path] + [f'{k}={v}' for k, v in G['fixed'].items()] + [f'{k}={v:.5g}' for k, v in p.items()]
    r = subprocess.run(args, capture_output=True, text=True)
    try:
        if r.returncode != 0: return (1e6, None)
        return loss(features(path, G['f0']), G['ref'])
    finally:
        os.remove(path)

if __name__ == '__main__':
    render, ref, outdir = sys.argv[1:4]
    os.makedirs(outdir, exist_ok=True)
    fixed = dict(a.split('=', 1) for a in sys.argv[4:]); gens = int(fixed.pop('gens', 40))
    sr, x = load(ref)
    if 'pattern' not in fixed: fixed.setdefault('dur', f'{len(x) / sr - 0.05:.3f}')
    f0 = 440 * 2 ** ((float(fixed.get('note', 69)) + float(fixed.get('tuneSt', 0)) - 69) / 12)
    G.update(render=render, fixed=fixed, f0=f0, tmp=tempfile.mkdtemp(), ref=features(ref, f0))
    logf = open(os.path.join(outdir, 'log.txt'), 'w')
    def log(g, b, parts, s):
        line = f'gen {g:3d} best {b:6.2f} ' + (' '.join(f'{k}={v:.2f}' for k, v in parts.items()) if parts else '') + f' sigma {s:.3f}'
        print(line, flush=True); logf.write(line + '\n'); logf.flush()
    with Pool(min(16, os.cpu_count() or 4)) as pool:
        f = lambda X: pool.map(evaluate, list(X))
        best = sep_cma(f, np.full(len(SPACE), 0.5), 0.3, gens, lam=24, log=log)
    bp = decode(best[1])
    with open(os.path.join(outdir, 'best.txt'), 'w') as fh:
        for k, v in list(fixed.items()) + [(k, f'{v:.5g}') for k, v in bp.items()]: fh.write(f'{k}={v}\n')
    subprocess.run([render, os.path.join(outdir, 'best.wav')] + [f'{k}={v}' for k, v in fixed.items()] + [f'{k}={v:.5g}' for k, v in bp.items()])
    print('best', best[0], best[2])
