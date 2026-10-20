# Forge of Wrath preset matcher: render candidates with RenderTest batch, compare with a reference kick
import os, sys, subprocess, numpy as np, glob, json, shutil
from scipy.io import wavfile
from scipy.signal import butter, sosfilt, resample_poly
SR = 48000
BIN = '/home/claude/doomsday/ForgeOfWrath/build/RenderTest_artefacts/Release/RenderTest'
SHM = '/dev/shm/fow'

def load(f):
    sr, raw = wavfile.read(f); x = raw.astype(np.float64)
    if raw.dtype.kind == 'i': x /= np.iinfo(raw.dtype).max
    if x.ndim > 1: x = x[:, :2].mean(1)
    if sr != SR: x = resample_poly(x, SR, sr)
    return x

# ---------------------------------------------------------------- parameter space
LOG, LIN, CAT = 0, 1, 2
SPACE = [  # id, kind, lo, hi
    ('tail_start', LOG, 1.0, 10), ('tail_hold', LIN, 0, 150), ('tail_drop', LOG, 8, 300), ('tail_curve', LIN, 0.5, 10), ('tail_wave', LIN, 0, 1),
    ('dist_type', CAT, 0, 4.999), ('drive', LIN, 0, 48), ('tone', LIN, -1, 1), ('low_cut', LOG, 10, 200), ('high_cut', LOG, 1500, 20000),
    ('body', LIN, -12, 12), ('grit', LIN, 0, 1), ('scream', LIN, 0, 1), ('scream_hz', LOG, 200, 4000), ('crunch', LIN, 0, 1), ('crunch_dec', LOG, 10, 500),
    ('punch_level', LIN, -36, 6), ('punch_start', LOG, 80, 2000), ('punch_end', LOG, 35, 250), ('punch_drop', LOG, 1, 60), ('punch_decay', LOG, 4, 200),
    ('chaos', LIN, 0, 1), ('chaos_dec', LOG, 5, 300), ('chaos_hz', LOG, 300, 10000), ('click_level', LIN, -60, -3), ('click_hz', LOG, 800, 16000), ('click_len', LOG, 0.5, 20),
    ('clip_drive', LIN, 0, 24), ('clip_soft', LIN, 0, 1), ('tail_level', LIN, -18, 6),
]
AMP_T = [0, 60, 150, 300, 500, 700, 850, 960]
DRV_T = [0, 80, 180, 300, 450, 600, 800, 1000]
D = len(SPACE) + len(AMP_T) + len(DRV_T) + 1

def decode(x, baseLen):
    x = np.clip(x, 0, 1); p = {}
    for i, (k, kind, lo, hi) in enumerate(SPACE):
        v = lo * (hi / lo) ** x[i] if kind == LOG else lo + (hi - lo) * x[i]
        p[k] = float(int(v)) if kind == CAT else float(v)
    o = len(SPACE)
    amp = [(t, float(x[o + j])) for j, t in enumerate(AMP_T)] + [(1000, 0.0)]
    o += len(AMP_T)
    drv = [(t, float(x[o + j])) for j, t in enumerate(DRV_T)]
    o += len(DRV_T)
    p['length'] = float(baseLen * (0.85 + 0.3 * x[o]))
    p['amp'] = ';'.join(f'{t:.1f}:{v:.4f}' for t, v in amp)
    p['drive_env'] = ';'.join(f'{t:.1f}:{v:.4f}' for t, v in drv)
    return p

def jobline(name, note, p, secs):
    toks = [name, str(note), 'sr=48000', f'secs={secs:.3f}']
    for k, v in p.items():
        if k == 'amp': toks.append('amp=' + v)
        elif k == 'drive_env': toks.append('denv=' + v)
        else: toks.append(f'{k}={v:.5f}')
    return ' '.join(toks)

def render(lines, outdir):
    if os.path.isdir(outdir): shutil.rmtree(outdir)
    os.makedirs(outdir)
    half = (len(lines) + 1) // 2
    procs = []
    for w, chunk in enumerate([lines[:half], lines[half:]]):
        if not chunk: continue
        jf = f'{outdir}/jobs{w}.txt'; open(jf, 'w').write('\n'.join(chunk) + '\n')
        procs.append(subprocess.Popen(['xvfb-run', '-a', BIN, 'batch', jf, outdir], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
    for pr in procs: pr.wait()

# ---------------------------------------------------------------- features
NB = 40
EDGES = np.geomspace(30, 16000, NB + 1)
WIN, HOP, NFFT = 1024, 240, 2048
FR = np.fft.rfftfreq(NFFT, 1 / SR)
BAND = [np.where((FR >= EDGES[i]) & (FR < EDGES[i + 1]))[0] for i in range(NB)]
BAND = [b if len(b) else np.array([np.argmin(np.abs(FR - EDGES[i]))]) for i, b in enumerate(BAND)]
LP = butter(4, 300, 'low', fs=SR, output='sos')
HANN = np.hanning(WIN)

def onset(x):
    a = np.abs(x); return max(0, int(np.argmax(a > 0.05 * a.max())) - int(0.001 * SR))

def feats(x, nframes):
    x = x / (np.abs(x).max() + 1e-12)
    pad = np.zeros(nframes * HOP + WIN); pad[:min(len(x), len(pad))] = x[:len(pad)]
    fr = np.lib.stride_tricks.sliding_window_view(pad, WIN)[::HOP][:nframes] * HANN
    S = np.abs(np.fft.rfft(fr, NFFT, axis=1)) ** 2
    B = np.stack([S[:, b].sum(1) for b in BAND], 1)
    B = 10 * np.log10(B + 1e-10); B = np.maximum(B, B.max() - 75)
    env = 10 * np.log10(np.mean(fr ** 2, 1) + 1e-10)
    env = np.maximum(env, env.max() - 60)
    # f0 from upward zero crossings of the low-passed signal
    y = sosfilt(LP, pad)
    zc = np.where((y[:-1] < 0) & (y[1:] >= 0))[0]
    f0 = np.full(nframes, np.nan)
    if len(zc) > 2:
        per = np.diff(zc); mid = (zc[:-1] + zc[1:]) / 2
        f0 = np.interp(np.arange(nframes) * HOP + WIN / 2, mid, SR / per)
    # spectral flatness per frame (200 Hz - 8 kHz): periodic square ~ low, dense chaotic impact ~ high
    sel = (FR > 100) & (FR < 2000)
    Ss = S[:, sel] + 1e-12
    fl = 10 * np.log10(np.exp(np.mean(np.log(Ss), 1)) / np.mean(Ss, 1))
    return B, env, f0, fl

OCT = np.geomspace(30, 16000, 55)
def ltas(x, a, b):
    seg = x[a:b]
    if len(seg) < 4096: seg = np.pad(seg, (0, 4096 - len(seg)))
    fr = np.lib.stride_tricks.sliding_window_view(seg, 4096)[::1024] * np.hanning(4096)
    P = (np.abs(np.fft.rfft(fr, axis=1)) ** 2).mean(0); f = np.fft.rfftfreq(4096, 1 / SR)
    out = np.array([P[(f >= OCT[i]) & (f < OCT[i + 1])].sum() + 1e-12 for i in range(len(OCT) - 1)])
    out = 10 * np.log10(out / out.sum()); return np.maximum(out, out.max() - 70)

class Target:
    def __init__(self, f):
        x = load(f); x = x[onset(x):]
        self.x = x; self.len = len(x) / SR
        self.nf = int(min(self.len + 0.03, 1.9) * SR / HOP)
        self.B, self.env, self.f0, self.fl = feats(x, self.nf)
        a = np.abs(x)
        k = int(0.005 * SR); r = np.array([np.sqrt(np.mean(x[i:i + k] ** 2)) for i in range(0, len(x) - k, k)])
        rdb = 20 * np.log10(r / r.max() + 1e-9)
        self.l40 = (len(rdb) - np.argmax(rdb[::-1] > -40)) * 5
        self.la, self.lb = int(0.06 * SR), int(max(0.1, min(self.l40 / 1000 - 0.02, 0.45)) * SR)
        S = np.abs(np.fft.rfft(np.lib.stride_tricks.sliding_window_view(np.pad(x, (0, self.nf * HOP + WIN)), WIN)[::HOP][:self.nf] * HANN, NFFT, axis=1)) ** 2
        self.bright = 10 * np.log10(S[:, FR > 600].sum(1) / (S.sum(1) + 1e-12) + 1e-6)
        self.lt = ltas(x / np.abs(x).max(), self.la, self.lb)

def distance(t, y):
    B, env, f0, fl = feats(y, t.nf)
    top = max(t.B.max(), B.max())
    w = np.clip((np.maximum(t.B, B) - top + 60) / 60, 0, 1)
    spec = np.sum(np.abs(t.B - B) * w) / np.sum(w)
    e = np.mean(np.abs(t.env - env))
    k = np.arange(t.nf) * HOP / SR
    sel = (k > 0.008) & (k < min(0.3, t.len - 0.02))
    a, b = t.f0[sel], f0[sel]
    ok = np.isfinite(a) & np.isfinite(b) & (a > 20) & (b > 20)
    semis = np.mean(np.abs(12 * np.log2(b[ok] / a[ok]))) if ok.sum() > 5 else 12
    semis = min(semis, 12)
    kk = np.arange(t.nf) * HOP / SR
    es = (kk < min(0.15, t.len)) & (t.env > t.env.max() - 30)
    flat = np.mean(np.abs(t.fl[es] - fl[es])) if es.sum() > 2 else 0.0
    yl = ltas(y / (np.abs(y).max() + 1e-12), t.la, t.lb)
    lt = np.mean(np.abs(yl - t.lt))
    return spec + 3.0 * e + 1.5 * semis + lt + 1.5 * flat, (spec, e, semis, lt, flat)
