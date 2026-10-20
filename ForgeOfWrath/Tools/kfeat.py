import sys, glob, os, warnings, numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly
warnings.filterwarnings('ignore')
SR = 48000
def load(f):
    sr, raw = wavfile.read(f); x = raw.astype(np.float64)
    if raw.dtype.kind == 'i': x /= np.iinfo(raw.dtype).max
    if x.ndim == 1: x = np.stack([x, x], 1)
    x = x[:, :2]
    if sr != SR: x = resample_poly(x, SR, sr, axis=0)
    return x
def f0_track(m, hop=0.005, win=0.03, fmin=30, fmax=500):
    n = int(win * SR); out = []
    taus = np.arange(int(SR / fmax), int(SR / fmin))
    span = n + int(taus[-1]) + 1
    for a in range(0, len(m) - span, int(hop * SR)):
        seg = m[a:a + span]
        if np.sqrt(np.mean(seg[:n] ** 2)) < 1e-3: out.append(np.nan); continue
        d = np.array([np.sum((seg[:n] - seg[t:t + n]) ** 2) for t in taus])
        c = d * np.arange(1, len(d) + 1) / np.maximum(np.cumsum(d), 1e-12)
        i = np.argmin(c)
        out.append(SR / taus[i] if c[i] < 0.35 else np.nan)
    return np.array(out)
def analyse(f, quick=False):
    x = load(f); m = x.mean(1)
    hop = int(0.005 * SR)
    rms = np.array([np.sqrt(np.mean(m[i:i + hop] ** 2) + 1e-12) for i in range(0, len(m) - hop, hop)])
    rdb = 20 * np.log10(rms / rms.max())
    on = int(np.argmax(rdb > -30))
    t = lambda thr: (len(rdb) - np.argmax(rdb[::-1] > thr) - on) * 5
    a = int((on * 5 + 120) * SR / 1000); b = int((on * 5 + max(160, t(-20))) * SR / 1000)
    tail = m[a:b] if b > a + 2048 else m[a:a + 4096]
    S = np.abs(np.fft.rfft(tail * np.hanning(len(tail)))) ** 2; fr = np.fft.rfftfreq(len(tail), 1 / SR)
    tot = S[fr > 20].sum()
    sub = S[(fr > 20) & (fr < 120)].sum() / tot * 100; mid = S[(fr >= 120) & (fr < 1000)].sum() / tot * 100; hi = S[fr >= 1000].sum() / tot * 100
    cen = (S * fr).sum() / S.sum()
    p = m[on * hop: on * hop + int(0.025 * SR)]
    P = np.abs(np.fft.rfft(p * np.hanning(len(p)))) ** 2; pf = np.fft.rfftfreq(len(p), 1 / SR)
    click = P[pf > 2000].sum() / P[pf > 20].sum() * 100; pcen = (P * pf).sum() / P.sum()
    f0 = f0_track(m[on * hop:]) if not quick else np.array([np.nan])
    tt = np.arange(len(f0)) * 5
    def at(ms):
        k = (tt >= ms - 7) & (tt <= ms + 7); v = f0[k]; v = v[np.isfinite(v)]
        return np.median(v) if len(v) else np.nan
    sel = (tt > 150) & (tt < 350)
    tailf = np.nanmedian(f0[sel]) if np.any(np.isfinite(f0[sel])) else np.nan
    rmsAll = 20 * np.log10(np.sqrt(np.mean(m[on * hop: on * hop + int(0.3 * SR)] ** 2)))
    corr = np.corrcoef(x[:, 0], x[:, 1])[0, 1] if np.std(x[:, 0] - x[:, 1]) > 1e-6 else 1.0
    crest = 20 * np.log10(np.abs(tail).max() / (np.sqrt(np.mean(tail ** 2)) + 1e-12))
    env = {ms: rdb[min(len(rdb) - 1, on + ms // 5)] for ms in (10, 50, 100, 200, 300)}
    return dict(name=os.path.basename(f)[:44], len=len(m) / SR, pk=20 * np.log10(np.abs(x).max()), rms=rmsAll, l20=t(-20), l40=t(-40), sub=sub, mid=mid, hi=hi, cen=cen,
                click=click, pcen=pcen, f20=at(20), f50=at(50), f100=at(100), tailf=tailf, corr=corr, crest=crest, env=env, f0=f0, rdb=rdb[on:], spec=(fr, S))
def header():
    return f"{'kick':44s} len  rms300 l-20 l-40 | f@20 f@50 f@100 tail | sub  mid  hi  cen  | click pcen | crest corr | env 50/100/200/300"
def row(r):
    e = r['env']
    return (f"{r['name']:44s} {r['len']:.2f} {r['rms']:6.1f} {r['l20']:4.0f} {r['l40']:4.0f} | {r['f20']:4.0f} {r['f50']:4.0f} {r['f100']:4.0f} {r['tailf']:4.0f} | "
            f"{r['sub']:4.0f} {r['mid']:4.0f} {r['hi']:4.0f} {r['cen']:5.0f} | {r['click']:4.1f} {r['pcen']:5.0f} | {r['crest']:4.1f} {r['corr']:4.2f} | {e[50]:5.1f} {e[100]:5.1f} {e[200]:5.1f} {e[300]:5.1f}")
if __name__ == '__main__':
    import pickle
    res = [analyse(f) for f in sorted(glob.glob(sys.argv[1] + '/*.wav'))]
    print(header())
    for r in res: print(row(r))
    pickle.dump(res, open(sys.argv[2], 'wb'))
