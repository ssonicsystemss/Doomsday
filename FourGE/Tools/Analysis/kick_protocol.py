#!/usr/bin/env python3
"""4GE AUDIO ANALYSIS PROTOCOL - the measurement half (owner's protocol, 2026-11-25).
Measures FACTS only (no hypotheses): global character, a data-driven timeline (change points, not fixed 0-50 ms), pitch track,
9 bands over time, tonal vs noise share per band and time, TOK test, SUB test, asymmetry, clipping / artifacts, decay / space.
usage: kick_protocol.py <out.json> <wav> [<wav> ...]      (prints a short text summary per sample too)
Needs numpy, scipy, soundfile. The owner's WAVs are not in git."""
import sys, json, numpy as np, soundfile as sf
from scipy.signal import butter, sosfiltfilt

BANDS = [(20, 60), (60, 120), (120, 250), (250, 500), (500, 1000), (1000, 2000), (2000, 5000), (5000, 10000), (10000, 20000)]
BNAME = ['20-60', '60-120', '120-250', '250-500', '500-1k', '1-2k', '2-5k', '5-10k', '10k+']
HOP_MS = 5.0

def db(x): return 20 * np.log10(np.maximum(x, 1e-9))

def load(p):
    info = sf.info(p)
    x, sr = sf.read(p, always_2d=True)
    m = x.mean(1)
    a = int(np.argmax(np.abs(m) > 0.003 * np.abs(m).max()))
    return x[a:], m[a:], sr, info

def f0_track(m, sr):
    """NSDF pitch per 5 ms on a 40 ms window (25..1500 Hz) + confidence"""
    hop, win = int(sr * HOP_MS / 1000), int(sr * 0.04)
    out = []
    for i in range(0, max(1, len(m) - win), hop):
        s = m[i:i + win] - m[i:i + win].mean()
        n = len(s); ac = np.correlate(s, s, 'full')[n - 1:]
        sq = s * s; e = np.concatenate([[0], np.cumsum(sq)]); mm = (e[n] - e[np.arange(n)]) + e[n - np.arange(n)]
        nsdf = 2 * ac / (mm + 1e-12)
        lo, hi = int(sr / 1500), min(n // 2, int(sr / 25))
        best = nsdf[lo:hi].max() if hi > lo else 0
        f = 0.0; c = float(best)
        if best > 0.4:
            for k in range(lo + 1, hi - 1):
                if nsdf[k] > 0.85 * best and nsdf[k] >= nsdf[k - 1] and nsdf[k] >= nsdf[k + 1]:
                    a_, b_, c_ = nsdf[k - 1], nsdf[k], nsdf[k + 1]; den = a_ - 2 * b_ + c_
                    f = sr / (k + (0.5 * (a_ - c_) / den if abs(den) > 1e-9 else 0)); c = float(b_); break
        out.append((i / sr * 1000 + 20, f, c))
    return out

def band_track(m, sr):
    """energy per band every 5 ms (20 ms Hann frames), dB re the loudest frame-band"""
    hop, win = int(sr * HOP_MS / 1000), int(sr * 0.02)
    N = 4096; fr = np.fft.rfftfreq(N, 1 / sr); w = np.hanning(win)
    rows = []
    for i in range(0, max(1, len(m) - win), hop):
        sp = np.abs(np.fft.rfft(m[i:i + win] * w, N)) ** 2
        rows.append([sp[(fr >= a) & (fr < b)].sum() for a, b in BANDS])
    E = np.array(rows) + 1e-15
    return 10 * np.log10(E / E.max())

def tonal_share(seg, sr, f0):
    """share of energy ON the harmonics of f0 per band (1 = fully tonal)"""
    if f0 <= 0 or len(seg) < 256: return [None] * len(BANDS)
    N = 1 << 16; sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), N)) ** 2; fr = np.fft.rfftfreq(N, 1 / sr)
    harm = np.zeros_like(sp, bool); k = 1; tol = max(4.0, 0.025 * f0)
    while k * f0 < fr[-1]: harm |= np.abs(fr - k * f0) < tol; k += 1
    out = []
    for a, b in BANDS:
        sel = (fr >= a) & (fr < b); t = sp[sel].sum()
        out.append(float(sp[sel & harm].sum() / t) if t > 0 else None)
    return out

def harmonics(seg, sr, f0, n=16):
    N = 1 << 17; sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), N)); fr = np.fft.rfftfreq(N, 1 / sr); w = max(3, int(4 * N / sr))
    h = np.array([sp[max(0, np.argmin(np.abs(fr - k * f0)) - w):np.argmin(np.abs(fr - k * f0)) + w + 1].max() for k in range(1, n + 1)])
    return [float(v) for v in db(h / h[0])]

def change_points(B, f0t, n_max=6):
    """boundaries where the 9-band shape or the pitch changes most (novelty on a smoothed feature vector)"""
    F = B - B.max(1, keepdims=True)                     # spectral SHAPE (level removed)
    lvl = B.max(1)
    pf = np.array([np.log2(max(f, 1)) for _, f, _ in f0t][:len(F)] + [0] * max(0, len(F) - len(f0t)))
    X = np.column_stack([F / 6.0, lvl[:, None] / 6.0, pf[:len(F), None] * 2])
    k = 3; nov = np.zeros(len(X))
    for i in range(k, len(X) - k): nov[i] = np.linalg.norm(X[i:i + k].mean(0) - X[i - k:i].mean(0))
    idx = []
    for i in np.argsort(nov)[::-1]:
        if nov[i] <= 0: break
        if all(abs(i - j) >= 4 for j in idx): idx.append(i)
        if len(idx) >= n_max: break
    return sorted(int(i * HOP_MS) for i in idx)

def analyse(p):
    x, m, sr, info = load(p)
    pk = np.abs(m).max(); mn = m / pk
    last = np.nonzero(np.abs(mn) > 0.001)[0][-1]; length = 1000 * (last + 1) / sr
    main_len = 1000 * (np.nonzero(np.abs(mn) > 0.03)[0][-1] + 1) / sr          # the main sound (to -30 dB): tail statistics only here
    # pitch: the NOTE from a low-passed copy (a distorted kick's harmonics never win), the full signal only where the low
    # track has nothing (a high TOK / punch above 300 Hz)
    lowt = f0_track(sosfiltfilt(butter(4, 320, fs=sr, output='sos'), mn), sr)
    fullt = f0_track(mn, sr)
    f0t = [(t, fl, max(cl, 0.61)) if (cl > 0.4 and fl > 0) else (t, ff, cf) for (t, fl, cl), (_, ff, cf) in zip(lowt, fullt)]
    B = band_track(mn, sr)
    stable = [f for t, f, c in lowt if c > 0.4 and f > 0 and t > 0.4 * main_len and t < 0.9 * main_len]   # the NOTE: low track only
    f_tail = float(np.median(stable)) if stable else 0.0
    # spectral check (harmonic sum over h1..h8, 30-130 Hz): the note a hard-clipped square / 2-tone tail really sits on
    seg = mn[int(0.45 * main_len * sr / 1000):int(0.85 * main_len * sr / 1000)]
    if len(seg) > 4096:
        N = 1 << 17; sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), N)); fr = np.fft.rfftfreq(N, 1 / sr); ls = np.log(sp + 1e-9)
        cands = np.arange(30.0, 130.0, 0.1)
        score = [sum(ls[int(round(k * c * N / sr))] - ls[int(round((k - 0.5) * c * N / sr))] for k in range(1, 9)) for c in cands]   # (minus the gaps between)
        f_hps = float(cands[int(np.argmax(score))])
        if f_tail <= 0: f_tail = f_hps                                      # (only where the low track found nothing)
    # pitch summary: first confident, the settle time (within 3 % of the tail pitch and staying)
    conf = [(t, f) for t, f, c in f0t if c > 0.6 and f > 0]
    settle = None
    if f_tail > 0:
        for i, (t, f) in enumerate(conf):
            if all(abs(ff / f_tail - 1) < 0.03 for _, ff in conf[i:i + 8]): settle = t; break
    tok = None                                          # a HELD high pitch before the drop (>= 4 ms within 3 %)
    if len(conf) > 3 and f_tail > 0:
        t0, fa = conf[0]
        if fa > 2.5 * f_tail:
            held = [t for t, f in conf if abs(f / fa - 1) < 0.03 and t < t0 + 40]
            if len(held) >= 2: tok = {'start_ms': round(conf[0][0] - 20, 1), 'len_ms': round(held[-1] - conf[0][0] + HOP_MS, 1), 'hz': round(fa, 1)}
    # tonal share per band in phases
    cps = change_points(B, f0t)
    edges = [0] + [c for c in cps if 3 < c < length - 10] + [int(length)]
    phases = []
    for a_, b_ in zip(edges[:-1], edges[1:]):
        s = mn[int(a_ * sr / 1000):int(b_ * sr / 1000)]
        if len(s) < 64: continue
        fs_ = [f for t, f, c in f0t if a_ <= t - 20 < b_ and c > 0.6 and f > 0]
        f_here = float(np.median(fs_)) if fs_ else 0.0
        rows = B[int(a_ / HOP_MS):max(int(a_ / HOP_MS) + 1, int(b_ / HOP_MS))]
        bands = list(np.round(10 * np.log10((10 ** (rows / 10)).mean(0) + 1e-15), 1))
        ph = {'from_ms': a_, 'to_ms': b_, 'rms_db': round(float(db(np.sqrt(np.mean(s ** 2)))), 1),
              'crest_db': round(float(db(np.abs(s).max() / (np.sqrt(np.mean(s ** 2)) + 1e-12))), 1),
              'f0_start': round(fs_[0], 1) if fs_ else 0, 'f0_end': round(fs_[-1], 1) if fs_ else 0,
              'bands_db': bands, 'loudest_band': BNAME[int(np.argmax(bands))],
              'tonal_share': [None if v is None else round(v, 2) for v in tonal_share(s, sr, f_here if f_here > 0 else f_tail)],
              'pos_peak': round(float(s.max()), 2), 'neg_peak': round(float(s.min()), 2), 'dc': round(float(s.mean()), 3),
              'pos_share': round(float(np.mean(s > 0)), 2), 'at_rail': round(float(np.mean(np.abs(s) > 0.98)), 2)}
        phases.append(ph)
    # tail harmonics + odd / even balance
    tseg = mn[int(0.45 * main_len * sr / 1000):int(0.85 * main_len * sr / 1000)]
    harm = harmonics(tseg, sr, f_tail) if f_tail > 0 and len(tseg) > 2048 else []
    oe = None
    if harm:
        lin = 10 ** (np.array(harm) / 20); oe = round(float(10 * np.log10((lin[2::2] ** 2).sum() / (lin[1::2] ** 2).sum())), 1)
    # SUB test: is there a low component that is NOT the tail note (a separate sine below / around it)?
    sub = None
    if f_tail > 0:
        seg = mn[int(0.3 * main_len * sr / 1000):int(0.8 * main_len * sr / 1000)]
        N = 1 << 17; sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), N)); fr = np.fft.rfftfreq(N, 1 / sr)
        sel = (fr > 25) & (fr < 0.85 * f_tail)
        if sel.any():
            lowpk = sp[sel].max(); fund = sp[np.argmin(np.abs(fr - f_tail))]
            sub = {'below_note_peak_hz': round(float(fr[sel][np.argmax(sp[sel])]), 1), 'below_note_db_re_fund': round(float(db(lowpk / fund)), 1)}
    # artifacts
    clipped_runs = int(np.sum(np.diff((np.abs(m) >= 0.999 * pk).astype(int)) == 1))
    uniq = len(np.unique(np.round(m * 32768)))
    hf = B[:, 8]; nyq_energy = round(float(np.median(hf)), 1)
    # decay / space: after the loudest phase, how fast does it fall; diffuse (noise) share in the last 20 % of the sound
    env = [db(np.sqrt(np.mean(mn[i:i + int(0.01 * sr)] ** 2))) for i in range(0, len(mn) - int(0.01 * sr), int(0.005 * sr))]
    env = np.array(env); ipk = int(np.argmax(env))
    t60 = next((i for i in range(ipk, len(env)) if env[i] < env[ipk] - 30), None)
    rep = {'file': p.split('/')[-1], 'sr': sr, 'subtype': info.subtype, 'channels': info.channels, 'length_ms': round(length), 'main_ms': round(main_len),
           'peak_dbfs': round(float(db(pk)), 2), 'rms_db': round(float(db(np.sqrt(np.mean(mn[:last] ** 2)))), 1),
           'stereo_side_db': round(float(10 * np.log10(np.mean((x[:, 0] - x[:, -1]) ** 2) / (np.mean(m ** 2) + 1e-20) + 1e-12)), 1) if info.channels > 1 else None,
           'tail_f0': round(f_tail, 1), 'tail_note': (round(69 + 12 * np.log2(f_tail / 440), 2) if f_tail > 0 else None),
           'pitch_first': conf[0] if conf else None, 'pitch_settle_ms': settle, 'tok': tok,
           'pitch_track': [(round(t), round(f, 1)) for t, f, c in f0t if c > 0.6 and f > 0][::2],
           'change_points_ms': cps, 'phases': phases, 'tail_harmonics_db': [round(v) for v in harm], 'tail_even_minus_odd_db': oe,
           'sub': sub, 'clip_runs': clipped_runs, 'distinct_levels': uniq, 'hf_10k_median_db': nyq_energy,
           'decay_30db_ms': (t60 - ipk) * 5 if t60 else None}
    return rep

if __name__ == '__main__':
    out = []
    for p in sys.argv[2:]:
        r = analyse(p); out.append(r)
        print(f"== {r['file']}  {r['length_ms']} ms  sr {r['sr']} {r['subtype']}  peak {r['peak_dbfs']} dBFS  tail f0 {r['tail_f0']} (MIDI {r['tail_note']})")
        print(f"   pitch first {r['pitch_first']} settle {r['pitch_settle_ms']} ms  TOK {r['tok']}  change points {r['change_points_ms']}")
        for ph in r['phases']:
            ts = ' '.join('-' if v is None else f"{v:.2f}" for v in ph['tonal_share'])
            print(f"   {ph['from_ms']:4d}-{ph['to_ms']:<4d} rms {ph['rms_db']:6.1f} crest {ph['crest_db']:4.1f} f0 {ph['f0_start']}->{ph['f0_end']}  loudest {ph['loudest_band']:7s} "
                  f"+{ph['pos_peak']} {ph['neg_peak']} pos {ph['pos_share']} rail {ph['at_rail']}  tonal/band {ts}")
        print(f"   tail harmonics {r['tail_harmonics_db']}  even-odd {r['tail_even_minus_odd_db']} dB  sub {r['sub']}  clip runs {r['clip_runs']}  levels {r['distinct_levels']}  decay30 {r['decay_30db_ms']} ms")
    json.dump(out, open(sys.argv[1], 'w'), indent=1, default=float)
