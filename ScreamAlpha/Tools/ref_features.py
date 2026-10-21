"""5CR34M 4LPH4 - reference screech analysis.

Usage: python3 ref_features.py <out.csv> <group>=<folder-or-glob> [...]
Per file: pitch movement, brightness and its movement, noisiness, harmonic shape (even/odd, richness, tilt),
harmonic-to-noise, stereo, amplitude rhythm, formant peaks. Prints group medians.
Reference WAVs are the owner's - they are not in git.
"""
import sys, glob, os, csv
import numpy as np
from scipy.io import wavfile
from scipy.signal import stft

def load(f):
    sr, x = wavfile.read(f)
    x = x.astype(np.float64)
    if x.ndim == 1: x = np.stack([x, x], 1)
    x = x[:, :2]
    return sr, x / max(1e-9, np.abs(x).max())

def yin(seg, sr, fmin=70, fmax=1800):
    n = len(seg) // 2
    taus = np.arange(int(sr / fmax), int(sr / fmin))
    a = seg[:n]
    d = np.array([np.sum((a - seg[t:t + n]) ** 2) for t in taus])
    c = d * np.arange(1, len(d) + 1) / np.maximum(np.cumsum(d), 1e-12)
    i = int(np.argmin(c))
    off = 0.0
    if 0 < i < len(c) - 1:
        p, q, r = c[i - 1], c[i], c[i + 1]
        off = 0.5 * (p - r) / (p - 2 * q + r + 1e-12)
    return sr / (taus[0] + i + off), 1 - c[i]

def analyse(f):
    sr, x = load(f)
    m = x.mean(1); s = (x[:, 0] - x[:, 1]) / 2
    N, H = 2048, 1024
    ff, tt, Z = stft(m, sr, nperseg=N, noverlap=N - H)
    P = np.abs(Z) ** 2
    e = P.sum(0)
    act = e > e.max() * 10 ** (-30 / 10)
    out = {'file': os.path.basename(f), 'dur': len(m) / sr}
    # pitch track
    f0, conf = [], []
    for j in np.where(act)[0]:
        a = j * H
        seg = m[a:a + 2 * N]
        if len(seg) < 2 * N: f0.append(np.nan); conf.append(0); continue
        v, c = yin(seg[::2] if sr > 60000 else seg, sr // 2 if sr > 60000 else sr)
        f0.append(v); conf.append(c)
    f0 = np.array(f0); conf = np.array(conf)
    good = (conf > 0.75) & np.isfinite(f0)
    out['voiced'] = good.mean() if len(good) else 0
    fg = f0[good]
    if len(fg) > 4:
        st = 12 * np.log2(fg / np.median(fg))
        d = np.abs(np.diff(st))
        out['pitch_range_st'] = np.percentile(st, 95) - np.percentile(st, 5)
        out['glide_frac'] = np.mean((d > 0.25) & (d < 3))        # pitch moving continuously
        out['jump_frac'] = np.mean(d >= 3)                        # note changes / octave jumps
        out['f0_med'] = np.median(fg)
    else:
        out.update(pitch_range_st=np.nan, glide_frac=np.nan, jump_frac=np.nan, f0_med=np.nan)
    Pa = P[:, act]
    cen = (ff[:, None] * Pa).sum(0) / (Pa.sum(0) + 1e-12)
    out['centroid'] = np.median(cen)
    out['centroid_move_oct'] = np.std(np.log2(cen + 1))
    out['flatness'] = np.median(np.exp(np.mean(np.log(Pa + 1e-14), 0)) / (np.mean(Pa, 0) + 1e-14))
    out['hi6k_db'] = 10 * np.log10(Pa[ff > 6000].sum() / Pa.sum())
    out['lo300_db'] = 10 * np.log10(Pa[ff < 300].sum() / Pa.sum())
    # harmonic shape on voiced frames
    ev, rich, tilt, hnr = [], [], [], []
    idx = np.where(act)[0]
    df = ff[1] - ff[0]
    for k, j in enumerate(idx):
        if not good[k]: continue
        F = f0[k]
        if F * 2 > sr / 2: continue
        spec = P[:, j]
        hs = []
        for h in range(1, 21):
            fh = F * h
            if fh > min(12000, sr / 2 - 100): break
            b = int(round(fh / df))
            hs.append(spec[max(0, b - 1):b + 2].max())
        hs = np.array(hs)
        if len(hs) < 6: continue
        hd = 10 * np.log10(hs + 1e-14)
        even = hs[1::2].sum(); odd = hs[0::2].sum()
        ev.append(10 * np.log10(even / (odd + 1e-14)))
        rich.append(np.sum(hd > hd.max() - 20))
        kk = np.arange(1, len(hd) + 1)
        tilt.append(np.polyfit(np.log2(kk), hd, 1)[0])
        band = ff < min(8000, sr / 2)
        mask = np.zeros_like(ff, bool)
        for h in range(1, int(8000 / F) + 1):
            mask |= np.abs(ff - F * h) < max(df * 1.5, F / 8)
        hnr.append(10 * np.log10(spec[band & mask].sum() / (spec[band & ~mask].sum() + 1e-14)))
    out['even_odd_db'] = np.median(ev) if ev else np.nan
    out['harm_rich'] = np.median(rich) if rich else np.nan
    out['harm_tilt_db_oct'] = np.median(tilt) if tilt else np.nan
    out['hnr_db'] = np.median(hnr) if hnr else np.nan
    # stereo
    out['side_mid_db'] = 20 * np.log10(np.sqrt((s ** 2).mean()) / (np.sqrt((m ** 2).mean()) + 1e-12) + 1e-9)
    # amplitude rhythm / modulation
    hop = int(0.005 * sr)
    env = np.sqrt(np.convolve(m ** 2, np.ones(hop) / hop, 'same'))[::hop]
    envc = env - env.mean()
    sp = np.abs(np.fft.rfft(envc * np.hanning(len(envc))))
    fr = np.fft.rfftfreq(len(envc), 0.005)
    band = (fr > 0.8) & (fr < 25)
    out['am_rate_hz'] = fr[band][np.argmax(sp[band])] if band.any() else np.nan
    out['am_depth'] = env.std() / (env.mean() + 1e-12)
    out['crest_db'] = 20 * np.log10(np.abs(m).max() / (np.sqrt((m ** 2).mean()) + 1e-12))
    # formant peaks of the long-term spectral envelope (1/3-octave-ish smoothing)
    avg = Pa.mean(1)
    sm = np.convolve(avg, np.ones(31) / 31, 'same')
    sel = np.where((ff > 350) & (ff < 6000))[0]
    pk = [i for i in sel[1:-1] if sm[i] > sm[i - 1] and sm[i] > sm[i + 1]]
    pk = sorted(pk, key=lambda i: -sm[i])[:2]
    out['formant1'] = ff[pk[0]] if pk else np.nan
    out['formant2'] = ff[pk[1]] if len(pk) > 1 else np.nan
    return out

if __name__ == '__main__':
    outcsv = sys.argv[1]
    rows = []
    for g in sys.argv[2:]:
        name, pat = g.split('=', 1)
        files = sorted(glob.glob(os.path.join(pat, '*.wav')) if os.path.isdir(pat) else glob.glob(pat))
        for f in files:
            try:
                r = analyse(f); r['group'] = name; rows.append(r)
            except Exception as ex:
                print('skip', f, ex, file=sys.stderr)
    keys = ['group', 'file'] + [k for k in rows[0] if k not in ('group', 'file')]
    with open(outcsv, 'w', newline='') as fh:
        w = csv.DictWriter(fh, keys); w.writeheader(); [w.writerow(r) for r in rows]
    num = [k for k in keys if k not in ('group', 'file')]
    print('group'.ljust(8), ' '.join(k[:11].rjust(11) for k in num))
    for gname in dict.fromkeys(r['group'] for r in rows):
        sub = [r for r in rows if r['group'] == gname]
        print(gname[:8].ljust(8), ' '.join(f"{np.nanmedian([r[k] for r in sub]):11.3g}" for k in num))
