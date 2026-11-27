#!/usr/bin/env python3
"""
DOOMSCOPE — analizator sampli hardstyle (sonicsystems / Doomsday)

Zamienia dzwiek w liczby i wykresy, ktore Claude potrafi rzetelnie ocenic.
Wymaga tylko: numpy, scipy, matplotlib (+ ffmpeg dla formatow innych niz WAV).

Uzycie:
  python doomscope.py analyze kick.wav [--type auto|kick|screech|lead|rbass] [--bpm 150] [--key F#] [--out DIR]
  python doomscope.py compare moj.wav referencja.wav [--type ...] [--out DIR]
  python doomscope.py selftest

Wyniki: report.md (liczby jako tekst – glowne zrodlo prawdy), report.json, PNG-i (pomocniczo).
"""

import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile

import numpy as np
from scipy import optimize, signal
from scipy.io import wavfile
from scipy.ndimage import uniform_filter1d, median_filter

VERSION = "0.1.0"

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
BANDS = [("SUB", 20, 60), ("LOW", 60, 150), ("LOWMID", 150, 500),
         ("CRUNCH", 500, 2000), ("PRESENCE", 2000, 6000), ("AIR", 6000, 20000)]
STEREO_BANDS = [("<120", 0, 120), ("120-500", 120, 500), ("500-2k", 500, 2000),
                ("2k-8k", 2000, 8000), (">8k", 8000, None)]
TIME_POINTS_MS = [0, 2, 5, 10, 15, 20, 30, 40, 50, 75, 100, 150, 200, 300, 400, 600, 800, 1000, 1500, 2000, 3000]
EPS = 1e-12


# ----------------------------------------------------------------------------- utils

def db(x):
    return 20.0 * np.log10(np.maximum(np.abs(x), EPS))


def pdb(x):
    return 10.0 * np.log10(np.maximum(x, EPS))


def hz_to_note(f):
    if f is None or not np.isfinite(f) or f <= 0:
        return None
    midi = 69 + 12 * math.log2(f / 440.0)
    n = int(round(midi))
    return {"note": f"{NOTE_NAMES[n % 12]}{n // 12 - 1}", "cents": round((midi - n) * 100, 1), "midi": round(midi, 2)}


def note_str(f):
    n = hz_to_note(f)
    if not n:
        return "–"
    return f"{n['note']} {n['cents']:+.0f}c"


def key_to_pc(key):
    if not key:
        return None
    k = key.strip().upper().replace("♯", "#").replace("B", "b") if False else key.strip()
    k = k[0].upper() + k[1:]
    flats = {"Db": "C#", "Eb": "D#", "Gb": "F#", "Ab": "G#", "Bb": "A#"}
    k = flats.get(k[:2], k)
    name = k[:2] if len(k) > 1 and k[1] == "#" else k[:1]
    return NOTE_NAMES.index(name) if name in NOTE_NAMES else None


def sos_filter(x, sr, lo=None, hi=None, order=4):
    nyq = sr / 2
    if lo and hi and hi < nyq * 0.98:
        sos = signal.butter(order, [lo / nyq, hi / nyq], btype="band", output="sos")
    elif lo:
        sos = signal.butter(order, lo / nyq, btype="high", output="sos")
    elif hi and hi < nyq * 0.98:
        sos = signal.butter(order, hi / nyq, btype="low", output="sos")
    else:
        return x.copy()
    return signal.sosfiltfilt(sos, x, axis=0)


def rms_env(x, sr, win_ms):
    """x: 1-D sygnal; zwraca obwiednie RMS (centrowana)."""
    w = max(1, int(sr * win_ms / 1000))
    return np.sqrt(np.maximum(uniform_filter1d(x * x, w, mode="constant"), 0))


def first_cross_below(arr, start, level):
    idx = np.flatnonzero(arr[start:] < level)
    return start + idx[0] if idx.size else None


def r(x, n=2):
    if x is None:
        return None
    try:
        if not np.isfinite(x):
            return None
    except TypeError:
        return x
    return round(float(x), n)


def jsonable(o):
    if isinstance(o, dict):
        return {k: jsonable(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [jsonable(v) for v in o]
    if isinstance(o, (np.floating, float)):
        return None if not np.isfinite(o) else float(o)
    if isinstance(o, np.integer):
        return int(o)
    if isinstance(o, np.ndarray):
        return jsonable(o.tolist())
    return o


# ----------------------------------------------------------------------------- I/O

def load_audio(path):
    info = {"file": os.path.basename(path)}
    x = sr = None
    try:
        sr, data = wavfile.read(path)
        info["format"] = str(data.dtype)
        if data.dtype == np.uint8:
            x = (data.astype(np.float64) - 128) / 128
        elif data.dtype == np.int16:
            x = data.astype(np.float64) / 32768
        elif data.dtype == np.int32:
            x = data.astype(np.float64) / 2147483648
        else:
            x = data.astype(np.float64)
    except Exception:
        if not shutil.which("ffmpeg"):
            raise SystemExit(f"Nie moge wczytac {path} (brak ffmpeg dla formatow innych niz WAV).")
        with tempfile.TemporaryDirectory() as td:
            tmp = os.path.join(td, "x.wav")
            subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", path, "-acodec", "pcm_f32le", tmp], check=True)
            sr, data = wavfile.read(tmp)
            x = data.astype(np.float64)
            info["format"] = "via ffmpeg"
    if x.ndim == 1:
        x = x[:, None]
    if x.shape[1] > 2:
        x = x[:, :2]
    info["sr"] = int(sr)
    info["channels"] = int(x.shape[1])
    info["length_ms"] = round(1000 * len(x) / sr, 1)
    return x, int(sr), info


# ----------------------------------------------------------------------------- loudness (ITU-R BS.1770 / libebur128)

def k_weight_coeffs(fs):
    f0 = 1681.974450955533
    G = 3.999843853973347
    Q = 0.7071752369554196
    K = math.tan(math.pi * f0 / fs)
    Vh = 10 ** (G / 20)
    Vb = Vh ** 0.4996667741545416
    a0 = 1 + K / Q + K * K
    b1 = [(Vh + Vb * K / Q + K * K) / a0, 2 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0]
    a1 = [1.0, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0]
    f0 = 38.13547087602444
    Q = 0.5003270373238773
    K = math.tan(math.pi * f0 / fs)
    a0 = 1 + K / Q + K * K
    b2 = [1.0, -2.0, 1.0]
    a2 = [1.0, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0]
    return (b1, a1), (b2, a2)


def k_weight(x, sr):
    (b1, a1), (b2, a2) = k_weight_coeffs(sr)
    y = signal.lfilter(b1, a1, x, axis=0)
    return signal.lfilter(b2, a2, y, axis=0)


def block_loudness(y, sr, block_s, hop_s):
    """y: K-weighted (n, ch). Zwraca tablice glosnosci blokow (LUFS) i srednie kwadratow."""
    B = int(round(block_s * sr))
    H = int(round(hop_s * sr))
    if len(y) < B:
        return np.array([]), np.array([])
    p = y * y
    cs = np.vstack([np.zeros((1, y.shape[1])), np.cumsum(p, axis=0)])
    starts = np.arange(0, len(y) - B + 1, H)
    z = (cs[starts + B] - cs[starts]) / B  # (nblocks, ch)
    zs = z.sum(axis=1)
    return -0.691 + pdb(zs), zs


def integrated_lufs(x, sr):
    y = k_weight(x, sr)
    L, zs = block_loudness(y, sr, 0.4, 0.1)
    if L.size == 0:
        return None
    keep = L > -70
    if not keep.any():
        return None
    rel = -0.691 + pdb(zs[keep].mean()) - 10
    keep2 = keep & (L > rel)
    if not keep2.any():
        return None
    return -0.691 + pdb(zs[keep2].mean())


def loudness_range(x, sr):
    y = k_weight(x, sr)
    L, zs = block_loudness(y, sr, 3.0, 1.0)
    if L.size < 2:
        return None
    keep = L > -70
    if keep.sum() < 2:
        return None
    rel = -0.691 + pdb(zs[keep].mean()) - 20
    v = L[keep & (L > rel)]
    if v.size < 2:
        return None
    return float(np.percentile(v, 95) - np.percentile(v, 10))


def max_window_lufs(x, sr, win_s, hop_s=0.01):
    y = k_weight(np.vstack([x, np.zeros((int(win_s * sr), x.shape[1]))]), sr)
    L, _ = block_loudness(y, sr, win_s, hop_s)
    return float(L.max()) if L.size else None


def true_peak_db(x):
    up = signal.resample_poly(x, 4, 1, axis=0)
    return float(db(np.max(np.abs(up))))


# ----------------------------------------------------------------------------- pitch: kick (zero crossings + phase fit)

def _F_exp(t, fe, f0, tau):
    return fe * t + (f0 - fe) * tau * (1 - np.exp(-t / tau))


def _F_2exp(t, fe, a1, t1, a2, t2):
    return fe * t + a1 * t1 * (1 - np.exp(-t / t1)) + a2 * t2 * (1 - np.exp(-t / t2))


def _F_pow(t, fe, f0, tau, p):
    if abs(p - 1) < 1e-6:
        return fe * t + (f0 - fe) * tau * np.log1p(t / tau)
    return fe * t + (f0 - fe) * tau / (1 - p) * ((1 + t / tau) ** (1 - p) - 1)


def f_model(name, params, t):
    t = np.asarray(t, dtype=float)
    if name == "exp":
        fe, f0, tau = params
        return fe + (f0 - fe) * np.exp(-t / tau)
    if name == "2exp":
        fe, a1, t1, a2, t2 = params
        return fe + a1 * np.exp(-t / t1) + a2 * np.exp(-t / t2)
    fe, f0, tau, p = params
    return fe + (f0 - fe) / (1 + t / tau) ** p


def kick_pitch(mono, sr, onset, end_idx):
    """Obwiednia pitchu kicka: przejscia przez zero na LP sygnale + dopasowanie modelu w dziedzinie fazy."""
    y = mono[onset:end_idx]
    if len(y) < sr * 0.05:
        return None
    y = sos_filter(y, sr, lo=15, hi=min(4000, sr * 0.45), order=4)
    s = np.signbit(y)
    i = np.flatnonzero(s[:-1] & ~s[1:])  # rosnace przejscia: y[i]<0, y[i+1]>=0
    if i.size < 8:
        return None
    frac = y[i] / (y[i] - y[i + 1] + EPS)
    tc = (i + frac) / sr
    # usun przejscia w ciszy na koncu (ponizej -45 dB obwiedni)
    env = rms_env(y, sr, 20)
    lvl = env[np.minimum(i, len(env) - 1)]
    tc = tc[lvl > env.max() * 10 ** (-45 / 20)]
    if tc.size < 8:
        return None
    # czyszczenie: usun przejscia tworzace okres znacznie krotszy od sasiadow (szum/klik)
    for _ in range(3):
        per = np.diff(tc)
        med = median_filter(per, size=7, mode="nearest")
        bad = np.flatnonzero(per < 0.55 * med)
        if bad.size == 0:
            break
        tc = np.delete(tc, bad + 1)
    per = np.diff(tc)
    f_zc = 1 / per
    t_zc = (tc[:-1] + tc[1:]) / 2
    k = np.arange(tc.size, dtype=float)
    t = tc

    fe0 = float(np.median(f_zc[int(len(f_zc) * 0.6):]))
    f00 = float(max(np.max(f_zc[:3]), fe0 * 1.5))
    fits = {}

    def run(name, F, p0, lb, ub):
        def res(p):
            return F(t, *p[:-1]) + p[-1] - k
        try:
            sol = optimize.least_squares(res, p0, bounds=(lb, ub), max_nfev=4000)
            rss = float(np.sum(sol.fun ** 2))
            n = len(k)
            aic = n * math.log(max(rss, 1e-12) / n) + 2 * len(p0)
            fits[name] = {"params": sol.x[:-1].tolist(), "rms_cycles": math.sqrt(rss / n), "aic": aic}
        except Exception:
            pass

    run("exp", _F_exp, [fe0, f00, 0.03, 0.0], [15, 15, 2e-4, -5], [400, 20000, 2.0, 5])
    run("2exp", _F_2exp, [fe0, (f00 - fe0) * 0.7, 0.005, (f00 - fe0) * 0.3, 0.05, 0.0],
        [15, 0, 1e-4, 0, 1e-3, -5], [400, 20000, 0.5, 20000, 2.0, 5])
    run("pow", _F_pow, [fe0, f00, 0.01, 2.0, 0.0], [15, 15, 1e-4, 0.3, -5], [400, 20000, 2.0, 12, 5])
    if not fits:
        return None
    best = min(fits, key=lambda n: fits[n]["aic"])
    params = fits[best]["params"]
    fe = params[0]

    tt = np.linspace(0, t[-1], 4000)
    ff = f_model(best, params, tt)
    st_above = 12 * np.log2(np.maximum(ff, EPS) / fe)
    settle_1st = tt[np.flatnonzero(st_above > 1)[-1]] if (st_above > 1).any() else 0.0
    t_plus5 = tt[np.flatnonzero(ff > fe + 5)[-1]] if (ff > fe + 5).any() else 0.0
    f_start = float(f_model(best, params, 0.0))
    half = fe + (f_start - fe) / 2
    t_half = tt[np.flatnonzero(ff > half)[-1]] if (ff > half).any() else 0.0
    return {
        "model": best,
        "params": params,
        "fits": {n: {"rms_cycles": r(v["rms_cycles"], 3), "aic": r(v["aic"], 1)} for n, v in fits.items()},
        "f_end_model": fe,
        "f_start_model": f_start,
        "f_first_cycle": float(f_zc[0]),
        "drop_semitones": float(12 * math.log2(f_start / fe)) if f_start > fe else 0.0,
        "t_half_ms": 1000 * t_half,
        "settle_1st_ms": 1000 * settle_1st,
        "t_to_fend_plus5_ms": 1000 * t_plus5,
        "zc_t": t_zc, "zc_f": f_zc,
    }


def tail_tuning(mono, sr, onset, t0_ms, t1_ms, f_guess):
    """F0 ogona przez sygnal analityczny na LP ponizej 2. harmonicznej."""
    a = onset + int(sr * t0_ms / 1000)
    b = onset + int(sr * t1_ms / 1000)
    if b - a < sr * 0.03 or not f_guess:
        return None
    seg_full = mono[max(0, a - int(0.1 * sr)): b + int(0.05 * sr)]
    y = sos_filter(seg_full, sr, lo=max(10, f_guess * 0.4), hi=f_guess * 1.6, order=4)
    off = a - max(0, a - int(0.1 * sr))
    y = y[off: off + (b - a)]
    ph = np.unwrap(np.angle(signal.hilbert(y)))
    inst = np.diff(ph) * sr / (2 * math.pi)
    inst = inst[int(len(inst) * 0.1): int(len(inst) * 0.9)]
    if inst.size < 10:
        return None
    f = float(np.median(inst))
    # dryft: roznica (centy) miedzy pierwsza i ostatnia tercja
    n3 = max(1, inst.size // 3)
    drift = 1200 * math.log2(np.median(inst[-n3:]) / np.median(inst[:n3]))
    return {"f0": f, "drift_cents": drift, "window_ms": [t0_ms, t1_ms]}


# ----------------------------------------------------------------------------- pitch: tonal (YIN)

def yin_track(y, sr, fmin=40, fmax=4000, frame=2048, hop=256, thresh=0.15):
    W = frame // 2
    tau_min = max(2, int(sr / fmax))
    tau_max = min(W - 1, int(sr / fmin) + 1)
    pad = frame // 2
    yp = np.pad(y, (pad, pad))
    n_frames = 1 + (len(yp) - frame) // hop
    fr = np.lib.stride_tricks.sliding_window_view(yp, frame)[::hop][:n_frames]
    nfft = 1 << int(math.ceil(math.log2(frame + W)))
    f0 = np.full(n_frames, np.nan)
    conf = np.zeros(n_frames)
    rmsf = np.zeros(n_frames)
    taus = np.arange(tau_max + 1)
    for s in range(0, n_frames, 128):
        c = np.array(fr[s:s + 128], dtype=float)
        a = c[:, :W]
        conv = np.fft.irfft(np.fft.rfft(a[:, ::-1], nfft) * np.fft.rfft(c, nfft), nfft)
        acf = conv[:, W - 1: W - 1 + tau_max + 1]
        cs = np.concatenate([np.zeros((len(c), 1)), np.cumsum(c * c, 1)], 1)
        e0 = cs[:, W][:, None]
        et = cs[:, taus + W] - cs[:, taus]
        d = e0 + et - 2 * acf
        d[:, 0] = 0
        cm = np.cumsum(d[:, 1:], 1)
        dn = np.ones_like(d)
        dn[:, 1:] = d[:, 1:] * taus[1:] / np.maximum(cm, EPS)
        for i, row in enumerate(dn):
            seg = row[tau_min:tau_max]
            below = np.flatnonzero(seg < thresh)
            if below.size:
                j = below[0]
                while j + 1 < seg.size and seg[j + 1] < seg[j]:
                    j += 1
            else:
                j = int(np.argmin(seg))
            tau = j + tau_min
            shift = 0.0
            if 1 <= tau < tau_max:
                den = row[tau - 1] - 2 * row[tau] + row[tau + 1]
                if abs(den) > EPS:
                    shift = 0.5 * (row[tau - 1] - row[tau + 1]) / den
                    if abs(shift) > 1:
                        shift = 0.0
            f0[s + i] = sr / (tau + shift)
            conf[s + i] = 1 - row[tau]
            rmsf[s + i] = math.sqrt(e0[i, 0] / W)
    times = np.arange(n_frames) * hop / sr
    voiced = (conf > 0.5) & (rmsf > rmsf.max() * 10 ** (-45 / 20))
    f0[~voiced] = np.nan
    # lagodne wygladzenie (mediana 5) na czesciach dzwiecznych
    lf = np.log2(f0)
    sm = lf.copy()
    for i in range(len(lf)):
        w = lf[max(0, i - 2): i + 3]
        w = w[np.isfinite(w)]
        if np.isfinite(lf[i]) and w.size:
            sm[i] = np.median(w)
    return times, 2 ** sm, conf


def modulation(series, rate_hz, lo=0.5, hi=40.0, detrend_s=1.0):
    """Szuka dominujacej modulacji (LFO) w szeregu (np. pitch w centach albo obwiednia w dB)."""
    v = np.asarray(series, dtype=float)
    ok = np.isfinite(v)
    if ok.sum() < rate_hz * 0.3:
        return None
    idx = np.arange(v.size)
    v = np.interp(idx, idx[ok], v[ok])
    win = int(detrend_s * rate_hz) | 1
    if win >= 5 and win < v.size:
        trend = signal.savgol_filter(v, win, 2)
        d = v - trend
    else:
        d = v - v.mean()
    n = 1 << int(math.ceil(math.log2(v.size * 8)))
    spec = np.abs(np.fft.rfft(d * np.hanning(d.size), n))
    fr = np.fft.rfftfreq(n, 1 / rate_hz)
    band = (fr >= lo) & (fr <= min(hi, rate_hz / 2 * 0.9))
    if not band.any():
        return None
    k = np.flatnonzero(band)[np.argmax(spec[band])]
    depth = (np.percentile(d, 95) - np.percentile(d, 5)) / 2
    prom = spec[k] / (np.median(spec[band]) + EPS)
    return {"rate_hz": float(fr[k]), "depth": float(depth), "peak_prominence": float(prom)}


# ----------------------------------------------------------------------------- harmonics (order tracking)

def harmonic_analysis(y, sr, f_of_t, n_harm=16):
    """y: segment mono, f_of_t: przyblizona czestotliwosc podstawowa dla kazdej probki (Hz).
    Order tracking: faza z sygnalu analitycznego podstawowej -> przeprobkowanie po fazie ->
    harmoniczne dokladnie w binach (dziala tez przy dryfcie i vibrato)."""
    f_of_t = np.asarray(f_of_t, dtype=float)
    fmean = float(np.nanmedian(f_of_t))
    if not np.isfinite(fmean) or fmean <= 0:
        return None
    fund = sos_filter(y, sr, lo=fmean * 0.6, hi=fmean * 1.5, order=4)
    fund_share = np.sum(fund ** 2) / (np.sum(y ** 2) + EPS)
    phase_src = "analityczna faza podstawowej"
    if fund_share > 10 ** (-30 / 10):
        cyc = np.unwrap(np.angle(signal.hilbert(fund))) / (2 * math.pi)
        if np.any(np.diff(cyc) <= 0):
            cyc = np.maximum.accumulate(cyc + np.arange(cyc.size) * 1e-9)
    else:
        cyc = np.concatenate([[0], np.cumsum(np.nan_to_num(f_of_t, nan=fmean)[:-1]) / sr])
        phase_src = "scalkowany tor F0 (slaba podstawowa)"
    # odetnij brzegi (efekty filtra / hilberta)
    cut = int(min(0.1 * y.size, 3 * sr / fmean))
    cyc = cyc[cut: y.size - cut]
    idx0 = np.arange(cut, y.size - cut)
    S = 1 << int(math.floor(math.log2(max(64, sr / fmean))))
    S = min(S, 1024)
    y = sos_filter(y, sr, hi=min(0.45 * S * fmean, 0.45 * sr), order=8)
    c0 = math.ceil(cyc[0])
    ncyc = int(math.floor(cyc[-1] - c0))
    if ncyc < 6:
        return None
    grid = c0 + np.arange(ncyc * S) / S
    pos = np.interp(grid, cyc, idx0)
    yr = np.interp(pos, np.arange(y.size), y)
    w = np.hanning(yr.size)
    P = np.abs(np.fft.rfft(yr * w)) ** 2
    nb = P.size
    hmax = min(S // 2 - 1, int(16000 / fmean))
    Ph = []
    for h in range(1, hmax + 1):
        c = h * ncyc
        lo, hi = max(0, c - 2), min(nb, c + 3)
        Ph.append(P[lo:hi].sum())
    Ph = np.array(Ph)
    fund_lo = max(1, int(0.5 * ncyc))
    total = P[fund_lo: int(min(nb, (hmax + 0.5) * ncyc))].sum()
    inh = total - Ph.sum()
    A = np.sqrt(Ph)
    H1 = A[0] + EPS
    levels = [float(db(A[h] / H1)) for h in range(min(n_harm, A.size))]
    thd = math.sqrt(Ph[1:].sum() / (Ph[0] + EPS))
    even = Ph[1::2].sum()
    odd = Ph[2::2].sum()
    T1 = A[0] / (A.sum() + EPS)
    T2 = A[1:4].sum() / (A.sum() + EPS)
    T3 = A[4:].sum() / (A.sum() + EPS)
    # srodek ciezkosci harmonicznych (w numerze harmonicznej)
    hc = float((np.arange(1, A.size + 1) * Ph).sum() / (Ph.sum() + EPS))
    return {
        "f0_used": fmean, "cycles": ncyc, "phase_source": phase_src,
        "fundamental_share_db": float(pdb(fund_share)), "harmonics_measured": int(hmax),
        "levels_db_rel_h1": levels,
        "thd_pct": 100 * thd, "thd_db": float(db(thd)),
        "even_odd_db": float(pdb(even / (odd + EPS))),
        "tristimulus": [float(T1), float(T2), float(T3)],
        "harmonic_centroid": hc,
        "inharmonic_db": float(pdb(max(inh, EPS) / (Ph.sum() + EPS))),
    }


# ----------------------------------------------------------------------------- clipping / stereo / spectrum

def clipping_analysis(x, active):
    out = {}
    pk = np.max(np.abs(x))
    if pk <= 0:
        return {"verdict": "cisza"}
    runs_total = 0
    longest = 0
    near = 0
    for ch in range(x.shape[1]):
        m = np.abs(x[:, ch]) >= 0.999 * pk
        near += m.sum()
        if m.any():
            d = np.diff(np.concatenate([[0], m.astype(int), [0]]))
            st = np.flatnonzero(d == 1)
            en = np.flatnonzero(d == -1)
            L = en - st
            runs_total += int((L >= 3).sum())
            longest = max(longest, int(L.max()))
    a = np.abs(x[active]).ravel()
    hist, _ = np.histogram(a, bins=512, range=(0, pk))
    ref = hist[int(512 * 0.8): int(512 * 0.98)].mean() + EPS
    edge_ratio = hist[-1] / ref
    v = x[active].ravel()
    kurt = float(np.mean((v - v.mean()) ** 4) / (np.var(v) ** 2 + EPS))
    crest = float(db(pk / (np.sqrt(np.mean(v * v)) + EPS)))
    if runs_total >= 3 and edge_ratio > 3:
        verdict = "HARD CLIP (plaskie szczyty)"
    elif crest < 6 and edge_ratio > 1.5:
        verdict = "mocno nasycony / soft clip (gestosc pod sufitem, brak plaskich serii)"
    elif crest < 9:
        verdict = "skompresowany / nasycony"
    else:
        verdict = "bez wyraznego clippingu"
    out.update({"flat_runs_ge3": runs_total, "longest_run_samples": longest,
                "pct_samples_near_peak": 100 * near / x.size, "hist_edge_ratio": float(edge_ratio),
                "kurtosis": kurt, "crest_db": crest, "verdict": verdict})
    return out


def stereo_analysis(x, sr, active):
    if x.shape[1] < 2:
        return None
    L = x[active, 0]
    R = x[active, 1]
    res = {}

    def corr(a, b):
        return float(np.sum(a * b) / (math.sqrt(np.sum(a * a) * np.sum(b * b)) + EPS))
    M = (L + R) / 2
    S = (L - R) / 2
    if np.sum(S * S) < 1e-14 * (np.sum(M * M) + EPS):
        return {"identical_channels": True, "correlation": 1.0}
    res["correlation"] = corr(L, R)
    res["side_to_mid_db"] = float(pdb(np.sum(S * S) / (np.sum(M * M) + EPS)))
    res["mono_loss_db"] = float(pdb(2 * np.sum(M * M) / (np.sum(L * L) + np.sum(R * R) + EPS)))
    res["balance_LR_db"] = float(pdb(np.sum(L * L) / (np.sum(R * R) + EPS)))
    bands = []
    for name, lo, hi in STEREO_BANDS:
        Lb = sos_filter(x[:, 0], sr, lo=lo or None, hi=hi)[active]
        Rb = sos_filter(x[:, 1], sr, lo=lo or None, hi=hi)[active]
        Mb = (Lb + Rb) / 2
        Sb = (Lb - Rb) / 2
        bands.append({"band": name, "correlation": corr(Lb, Rb),
                      "side_to_mid_db": float(pdb(np.sum(Sb * Sb) / (np.sum(Mb * Mb) + EPS)))})
    res["bands"] = bands
    return res


def frac_octave_smooth(freqs, power, frac=6):
    out = np.empty_like(power)
    c = np.concatenate([[0], np.cumsum(power)])
    lo = freqs * 2 ** (-0.5 / frac)
    hi = freqs * 2 ** (0.5 / frac)
    il = np.searchsorted(freqs, lo)
    ih = np.searchsorted(freqs, hi, side="right")
    ih = np.maximum(ih, il + 1)
    out = (c[np.minimum(ih, len(power))] - c[il]) / (np.minimum(ih, len(power)) - il)
    return out


def average_spectrum(mono, sr):
    n = 1 << int(math.ceil(math.log2(max(len(mono), 65536))))
    w = np.hanning(len(mono))
    P = np.abs(np.fft.rfft(mono * w, n)) ** 2
    P /= (np.sum(w ** 2) * sr / 2 + EPS)
    f = np.fft.rfftfreq(n, 1 / sr)
    return f, P


def spectral_peaks(f, P, count=8):
    m = (f >= 20) & (f <= 18000)
    fs = f[m]
    sm = frac_octave_smooth(fs, P[m], 48)
    d = pdb(sm)
    pk, prop = signal.find_peaks(d, prominence=3)
    if pk.size == 0:
        return []
    order = pk[np.argsort(d[pk])[::-1]][:count]
    top = d.max()
    return [{"hz": round(float(fs[i]), 1), "note": note_str(fs[i]), "db_rel_max": round(float(d[i] - top), 1)}
            for i in sorted(order, key=lambda i: fs[i])]


def spectral_descriptors(mono, sr, onset, end):
    y = mono[onset:end]
    n = 1024 if sr <= 50000 else 2048
    hop = n // 4
    if len(y) < n:
        y = np.pad(y, (0, n - len(y)))
    f, t, Z = signal.stft(y, sr, window="hamming", nperseg=n, noverlap=n - hop, boundary=None, padded=True)
    P = np.abs(Z) ** 2
    e = P.sum(0)
    keep = e > e.max() * 10 ** (-40 / 10)
    P = P[:, keep]
    if P.shape[1] == 0:
        return {}
    Pn = P / (P.sum(0) + EPS)
    cen = (f[:, None] * Pn).sum(0)
    spread = np.sqrt(((f[:, None] - cen) ** 2 * Pn).sum(0))
    cum = np.cumsum(Pn, 0)
    roll = f[np.argmax(cum >= 0.95, axis=0)]
    band = (f >= 250) & (f <= 16000)
    Pb = P[band] + EPS
    flat = np.exp(np.mean(np.log(Pb), 0)) / np.mean(Pb, 0)
    A = np.sqrt(P)
    An = A / (np.sqrt((A * A).sum(0)) + EPS)
    flux = np.r_[0, np.sqrt((np.diff(An, axis=1).clip(min=0) ** 2).sum(0))]

    def mi(v):
        return {"median": float(np.median(v)), "iqr": float(np.percentile(v, 75) - np.percentile(v, 25))}
    return {"centroid_hz": mi(cen), "spread_hz": mi(spread), "rolloff95_hz": mi(roll),
            "flatness": mi(flat), "flux": mi(flux)}


# ----------------------------------------------------------------------------- main analysis

def analyze(x, sr, info, stype="auto", bpm=None, key=None):
    res = {"tool": f"DOOMSCOPE {VERSION}", "info": info}
    mono = x.mean(axis=1)
    energy = (x * x).mean(axis=1)
    peak = float(np.max(np.abs(x)))
    if peak <= 0:
        raise SystemExit("Plik jest cichy (same zera).")

    # --- onset / koniec
    thr = peak * 10 ** (-40 / 20)
    onset = int(np.flatnonzero(np.abs(x).max(axis=1) >= thr)[0])
    fast = np.sqrt(uniform_filter1d(energy, max(1, int(sr * 0.001)), mode="constant"))
    slow = np.sqrt(uniform_filter1d(energy, max(1, int(sr * 0.030)), mode="constant"))
    slow_db = db(slow / (slow.max() + EPS))
    fast_db = db(fast / (fast.max() + EPS))
    above = np.flatnonzero(slow_db > -60)
    end = int(above[-1]) + 1 if above.size else len(x)
    end = max(end, onset + int(0.01 * sr))
    active = slice(onset, end)
    res["timing"] = {"leading_silence_ms": 1000 * onset / sr,
                     "sound_length_ms": 1000 * (end - onset) / sr,
                     "trailing_silence_ms": 1000 * (len(x) - end) / sr}

    # --- obwiednia / atak
    seg = fast[onset:end]
    # transjent = maksimum szybkiej obwiedni w pierwszych 50 ms (globalne max bywa w ogonie przy mocnym nasyceniu)
    ipk = int(np.argmax(seg[: max(2, int(0.05 * sr))]))
    pk = seg[ipk]
    ths = np.arange(1, 11) / 10
    tth = np.array([np.flatnonzero(seg[: ipk + 1] >= th * pk)[0] for th in ths]) / sr
    eff = np.diff(tth)
    alpha = 3
    m_eff = eff.mean() if eff.size else 0
    i0 = 0
    while i0 < eff.size - 1 and eff[i0] > alpha * m_eff:
        i0 += 1
    i1 = eff.size - 1
    while i1 > 0 and eff[i1] > alpha * m_eff:
        i1 -= 1
    att_start, att_end = tth[i0], tth[min(i1 + 1, tth.size - 1)]
    lat = math.log10(max(att_end - att_start, 1 / sr))
    sseg = slow[onset:end]
    is_pk = int(np.argmax(sseg))
    sdb = db(sseg / (sseg.max() + EPS))
    decays = {}
    for lvl in (-6, -12, -20, -40, -60):
        j = first_cross_below(sdb, is_pk, lvl)
        decays[str(lvl)] = 1000 * j / sr if j is not None else None
    # regresja zaniku -3..-23 dB
    j3 = first_cross_below(sdb, is_pk, -3)
    j23 = first_cross_below(sdb, is_pk, -23)
    slope = None
    if j3 is not None and j23 is not None and j23 - j3 > 10:
        tt = np.arange(j3, j23) / sr
        slope = float(np.polyfit(tt, sdb[j3:j23], 1)[0])
    amp = sseg
    tc_mask = amp > 0.15 * amp.max()
    t_cent = float((np.arange(amp.size)[tc_mask] * amp[tc_mask]).sum() / (amp[tc_mask].sum() + EPS) / sr * 1000)
    eff_dur = float(1000 * (amp > 0.4 * amp.max()).sum() / sr)
    act = x[active]
    rms_all = math.sqrt(np.mean(act ** 2))
    n15 = max(1, int(0.015 * sr))
    head = x[onset:onset + n15]
    body = x[onset + n15:end]
    res["envelope"] = {
        "attack_10_90_ms": 1000 * (tth[8] - tth[0]),
        "attack_weakest_effort_ms": 1000 * (att_end - att_start),
        "log_attack_time": lat,
        "time_to_transient_peak_ms": 1000 * ipk / sr,
        "time_to_peak_slow_ms": 1000 * is_pk / sr,
        "decay_ms_from_slow_peak": decays,
        "decay_slope_db_per_s": slope,
        "t60_extrapolated_ms": (-60 / slope * 1000) if slope and slope < 0 else None,
        "temporal_centroid_ms": t_cent,
        "effective_duration_ms": eff_dur,
        "crest_db": float(db(np.max(np.abs(act)) / (rms_all + EPS))),
        "crest_head15_db": float(db(np.max(np.abs(head)) / (math.sqrt(np.mean(head ** 2)) + EPS))),
        "head15_vs_body_rms_db": float(pdb(np.mean(head ** 2) / (np.mean(body ** 2) + EPS))) if body.size else None,
    }
    # zaniki per pasmo
    band_dec = {}
    for name, lo, hi in (("sub<100", None, 100), ("body100-500", 100, 500), ("crunch>1k", 1000, None)):
        yb = sos_filter(mono, sr, lo=lo, hi=hi)[onset:end]
        eb = rms_env(yb, sr, 30 if hi and hi <= 100 else 10)
        if eb.max() <= 0:
            continue
        ebd = db(eb / eb.max())
        ip = int(np.argmax(eb))
        j = first_cross_below(ebd, ip, -20)
        band_dec[name] = {"peak_ms": 1000 * ip / sr, "to_-20dB_ms": 1000 * j / sr if j is not None else None,
                          "level_db_rel_total": float(pdb(np.mean(yb ** 2) / (np.mean(mono[active] ** 2) + EPS)))}
    res["band_decays"] = band_dec

    # --- metering
    lufs_int = integrated_lufs(x, sr) if len(x) >= 0.4 * sr else None
    k_act = k_weight(x, sr)[active]
    hit = -0.691 + pdb((k_act ** 2).mean(axis=0).sum())
    tp = true_peak_db(x)
    sp = float(db(peak))
    met = {"sample_peak_dbfs": sp, "true_peak_dbtp": tp, "intersample_over_db": tp - sp,
           "rms_dbfs_active": float(db(rms_all)),
           "max_momentary_lufs": max_window_lufs(x, sr, 0.4),
           "max_short_term_lufs": max_window_lufs(x, sr, 3.0) if len(x) > 0.5 * sr else None,
           "hit_loudness_lufs_nonstd": hit,
           "integrated_lufs": lufs_int,
           "lra_lu": loudness_range(x, sr) if len(x) >= 6 * sr else None}
    met["plr_db"] = tp - met["max_momentary_lufs"] if met["max_momentary_lufs"] is not None else None
    res["metering"] = met

    # --- sprawdzenia techniczne
    dc = [float(np.mean(x[:, c])) for c in range(x.shape[1])]
    last5 = x[max(0, len(x) - int(0.005 * sr)):]
    res["checks"] = {
        "dc_offset_dbfs": [float(db(v)) for v in dc],
        "first_sample_dbfs": float(db(np.max(np.abs(x[0])))),
        "last_sample_dbfs": float(db(np.max(np.abs(x[-1])))),
        "last_5ms_rms_db_rel_peak": float(db(math.sqrt(np.mean(last5 ** 2)) / peak)),
    }

    # --- pasma w czasie
    band_series = {}
    band_share = {}
    tot = 0
    sig_b = {}
    for name, lo, hi in BANDS:
        yb = sos_filter(mono, sr, lo=lo, hi=hi if hi < sr / 2 else None)
        sig_b[name] = yb
        e = np.sum(yb[active] ** 2)
        band_share[name] = e
        tot += e
    for name in band_share:
        band_share[name] = 100 * band_share[name] / (tot + EPS)
    ref_lvl = max(np.max(rms_env(sig_b[n], sr, 10)) for n in sig_b) + EPS
    for name, lo, hi in BANDS:
        env = rms_env(sig_b[name], sr, 30 if hi <= 150 else 10)
        band_series[name] = env / ref_lvl
    res["bands"] = {"share_pct": band_share}

    # --- widmo
    f, P = average_spectrum(mono[active], sr)
    res["spectrum"] = {"peaks": spectral_peaks(f, P), "descriptors": spectral_descriptors(mono, sr, onset, end)}

    # --- pitch
    kp = kick_pitch(mono, sr, onset, end)
    tonal_t = tonal_f = None
    if stype == "auto":
        if kp and kp["drop_semitones"] >= 12 and kp["f_end_model"] < 130 and kp["fits"][kp["model"]]["rms_cycles"] < 0.5:
            stype = "kick"
        else:
            stype = "tonal"
    res["type"] = stype
    pitch = {}
    f_track = None
    harm_seg = None
    if stype == "kick" and kp:
        fe = kp["f_end_model"]
        settle = kp["settle_1st_ms"]
        L_ms = res["timing"]["sound_length_ms"]
        t20 = decays["-20"] or L_ms
        tt_ = np.linspace(0, L_ms / 1000, 4000)
        cents_above = 1200 * np.log2(f_model(kp["model"], kp["params"], tt_) / fe)
        hi_ = np.flatnonzero(cents_above > 3)
        t_settle3 = 1000 * tt_[hi_[-1]] if hi_.size else 0.0
        t_b = max(t20, min(L_ms, settle + 60))
        still_gliding = t_settle3 > t_b - 40
        t_a = t_settle3 if not still_gliding else max(settle, t_b - max(40, 0.4 * (t_b - settle)))
        kp["settle_3c_ms"] = t_settle3
        kp["tail_still_gliding"] = bool(still_gliding)
        tun = tail_tuning(mono, sr, onset, t_a, min(t_b, L_ms), fe)
        pitch = {k: v for k, v in kp.items() if k not in ("zc_t", "zc_f")}
        pitch["curve_ms"] = {str(tp_): float(f_model(kp["model"], kp["params"], tp_ / 1000))
                             for tp_ in TIME_POINTS_MS if tp_ <= res["timing"]["sound_length_ms"]}
        if tun:
            pitch["tail"] = {"f0_hz": tun["f0"], "note": hz_to_note(tun["f0"]),
                             "still_gliding": kp["tail_still_gliding"],
                             "drift_cents": tun["drift_cents"], "window_ms": tun["window_ms"]}
            pc = key_to_pc(key)
            if pc is not None:
                midi = 69 + 12 * math.log2(tun["f0"] / 440)
                dist = ((midi - pc) % 12)
                dist = dist - 12 if dist > 6 else dist
                pitch["tail"]["vs_key_cents"] = 100 * dist
            # harmoniczne ogona
            a = onset + int(sr * t_a / 1000)
            b = onset + int(sr * min(t_b, res["timing"]["sound_length_ms"]) / 1000)
            if b - a > 6 * sr / tun["f0"]:
                tt = (np.arange(a, b) - onset) / sr
                harm_seg = (mono[a:b], f_model(kp["model"], kp["params"], tt), [t_a, t_b])
        f_track = (kp["zc_t"], kp["zc_f"])
    else:
        frame = 2048 if sr <= 50000 else 4096
        hop = max(64, int(sr * 0.003))
        tonal_t, tonal_f, conf = yin_track(mono[onset:end], sr, frame=frame, hop=hop)
        v = np.isfinite(tonal_f)
        pitch["voiced_pct"] = float(100 * v.mean())
        if v.sum() > 3:
            fmed = float(np.nanmedian(tonal_f))
            cents = 1200 * np.log2(tonal_f / fmed)
            pitch["median_f0_hz"] = fmed
            pitch["median_note"] = hz_to_note(fmed)
            pitch["range_cents_p5_p95"] = [float(np.nanpercentile(cents, 5)), float(np.nanpercentile(cents, 95))]
            pitch["vibrato"] = modulation(cents, sr / hop)
            pitch["curve_ms"] = {str(tp_): (float(np.interp(tp_ / 1000, tonal_t[v], tonal_f[v])) if v.any() else None)
                                 for tp_ in TIME_POINTS_MS if tp_ <= res["timing"]["sound_length_ms"]}
            # najdluzszy stabilny odcinek do harmonicznych
            fi = np.interp(np.arange(end - onset) / sr, tonal_t[v], tonal_f[v])
            harm_seg = (mono[onset:end], fi, [0, res["timing"]["sound_length_ms"]])
        f_track = (tonal_t, tonal_f)
        env_db = db(rms_env(mono[onset:end], sr, 5)[::hop] + EPS)
        pitch["amp_mod"] = modulation(env_db, sr / hop)
    res["pitch"] = pitch
    if harm_seg is not None:
        h = harmonic_analysis(harm_seg[0], sr, harm_seg[1])
        if h:
            h["window_ms"] = harm_seg[2]
        res["harmonics"] = h

    # --- clipping / stereo
    res["clipping"] = clipping_analysis(x, active)
    res["stereo"] = stereo_analysis(x, sr, active)

    # --- loop loudness (kick w petli na cwiercnutach, ucinany na kolejnym uderzeniu)
    if stype == "kick" or bpm:
        b_ = bpm or 150
        beat = int(round(sr * 60 / b_))
        one = x[onset:onset + beat]
        if len(one) < beat:
            one = np.vstack([one, np.zeros((beat - len(one), x.shape[1]))])
        loop = np.tile(one, (32, 1))
        res["loop"] = {"bpm": b_, "beat_ms": 1000 * beat / sr,
                       "integrated_lufs": integrated_lufs(loop, sr),
                       "max_short_term_lufs": max_window_lufs(loop, sr, 3.0),
                       "true_peak_dbtp": true_peak_db(loop),
                       "sound_vs_beat_pct": 100 * (end - onset) / beat}

    # --- serie czasowe (skwantyzowane, dla Claude'a)
    ts = {}
    for tp_ in TIME_POINTS_MS:
        if tp_ > res["timing"]["sound_length_ms"]:
            break
        i = onset + int(sr * tp_ / 1000)
        row = {"slow_db": float(slow_db[min(i, len(x) - 1)]),
               "fast_db": float(fast_db[min(i, len(x) - 1)]) if tp_ <= 50 else None}
        for name in band_series:
            row[name] = float(db(band_series[name][min(i, len(x) - 1)]))
        ts[str(tp_)] = row
    res["timeseries"] = ts
    res["flags"] = make_flags(res)

    # dane do wykresow (nie do JSON)
    res["_plot"] = {"x": x, "sr": sr, "onset": onset, "end": end, "fast_db": fast_db, "slow_db": slow_db,
                    "band_series": band_series, "spec_f": f, "spec_P": P, "kp": kp, "f_track": f_track,
                    "tonal": (tonal_t, tonal_f)}
    return res


def make_flags(res):
    fl = []
    c = res["checks"]
    m = res["metering"]
    if max(c["dc_offset_dbfs"]) > -50:
        fl.append(f"DC offset {max(c['dc_offset_dbfs']):.1f} dBFS (> -50) – warto HP ~10-20 Hz")
    if c["last_sample_dbfs"] > -60:
        fl.append(f"Ostatnia probka {c['last_sample_dbfs']:.1f} dBFS (> -60) – ryzyko kliku na koncu sampla, daj fade-out")
    if c["first_sample_dbfs"] > -40 and res["timing"]["leading_silence_ms"] < 0.05:
        fl.append(f"Pierwsza probka {c['first_sample_dbfs']:.1f} dBFS – start nie od zera (moze byc zamierzony klik)")
    if res["timing"]["leading_silence_ms"] > 1:
        fl.append(f"Cisza na poczatku {res['timing']['leading_silence_ms']:.1f} ms – sample spozni sie wzgledem siatki")
    if c["last_5ms_rms_db_rel_peak"] > -40:
        fl.append(f"Ostatnie 5 ms na {c['last_5ms_rms_db_rel_peak']:.1f} dB wzgl. szczytu – ogon nie wygasa w pliku")
    if m["true_peak_dbtp"] > 0:
        fl.append(f"True peak {m['true_peak_dbtp']:+.2f} dBTP (> 0)")
    if m["intersample_over_db"] > 0.5:
        fl.append(f"Intersample overs: TP o {m['intersample_over_db']:.2f} dB wyzej niz sample peak")
    t = res.get("pitch", {}).get("tail")
    if t:
        if abs(t["note"]["cents"]) > 15:
            fl.append(f"Ogon {t['f0_hz']:.2f} Hz = {t['note']['note']} {t['note']['cents']:+.0f} c – poza stroem o >15 centow (prog hipotetyczny)")
        if "vs_key_cents" in t and abs(t["vs_key_cents"]) > 15:
            fl.append(f"Ogon odchylony od tonacji o {t['vs_key_cents']:+.0f} c")
        if t.get("still_gliding"):
            fl.append("Pitch ogona wciaz zjezdza do konca dzwieku (zjazd za wolny wzgl. dlugosci) – slychac 'glide', nie stabilna nute")
        if abs(t["drift_cents"]) > 20:
            fl.append(f"Dryf pitchu w ogonie {t['drift_cents']:+.0f} c (ogon nie jest stabilny)")
    st = res.get("stereo")
    if st and st.get("bands"):
        b0 = st["bands"][0]
        if b0["correlation"] < 0.9:
            fl.append(f"Korelacja ponizej 120 Hz = {b0['correlation']:.2f} (< 0.9) – sub nie jest mono (prog hipotetyczny)")
    if "HARD" in res["clipping"].get("verdict", ""):
        fl.append(f"Hard clip: {res['clipping']['flat_runs_ge3']} serii plaskich szczytow (moze byc zamierzone)")
    return fl


# ----------------------------------------------------------------------------- report (markdown)

def fmt(v, n=1, unit=""):
    if v is None:
        return "–"
    if isinstance(v, float) and not np.isfinite(v):
        return "–"
    return f"{v:.{n}f}{unit}"


def report_md(res):
    L = []
    inf = res["info"]
    L.append(f"# DOOMSCOPE – {inf['file']}")
    L.append(f"{res['tool']} · typ: **{res['type']}** · {inf['sr']} Hz · {inf['channels']} kan. · {inf['length_ms']} ms · {inf.get('format', '')}")
    L.append("Czas 0 ms = onset (pierwsza probka powyzej -40 dB wzgl. szczytu).\n")
    L.append("## Uwagi")
    L += [f"- {f}" for f in res["flags"]] or ["- brak"]
    tm = res["timing"]
    L.append(f"\n## Czas\nonset po {fmt(tm['leading_silence_ms'], 2)} ms ciszy · dzwiek {fmt(tm['sound_length_ms'])} ms (do -60 dB) · cisza na koncu {fmt(tm['trailing_silence_ms'])} ms")
    m = res["metering"]
    L.append("\n## Poziomy")
    L.append("| metryka | wartosc |\n|---|---|")
    L.append(f"| sample peak | {fmt(m['sample_peak_dbfs'], 2)} dBFS |")
    L.append(f"| true peak (4x) | {fmt(m['true_peak_dbtp'], 2)} dBTP |")
    L.append(f"| RMS (aktywna czesc) | {fmt(m['rms_dbfs_active'], 2)} dBFS |")
    L.append(f"| max momentary (400 ms) | {fmt(m['max_momentary_lufs'], 2)} LUFS |")
    L.append(f"| hit loudness (niestandard.) | {fmt(m['hit_loudness_lufs_nonstd'], 2)} LUFS |")
    L.append(f"| integrated | {fmt(m['integrated_lufs'], 2)} LUFS |")
    L.append(f"| max short-term (3 s) | {fmt(m['max_short_term_lufs'], 2)} LUFS |")
    L.append(f"| PLR (TP − max momentary) | {fmt(m['plr_db'], 2)} dB |")
    if res.get("loop"):
        lp = res["loop"]
        L.append(f"| **petla {lp['bpm']} BPM** integrated | {fmt(lp['integrated_lufs'], 2)} LUFS |")
        L.append(f"| petla max short-term / TP | {fmt(lp['max_short_term_lufs'], 2)} LUFS / {fmt(lp['true_peak_dbtp'], 2)} dBTP |")
        L.append(f"| dlugosc dzwieku / cwiercnuta ({fmt(lp['beat_ms'], 0)} ms) | {fmt(lp['sound_vs_beat_pct'], 0)} % |")
    e = res["envelope"]
    L.append("\n## Obwiednia i transjent")
    L.append(f"atak 10-90% (do szczytu transjentu): {fmt(e['attack_10_90_ms'], 2)} ms · atak (weakest effort): {fmt(e['attack_weakest_effort_ms'], 2)} ms · LAT {fmt(e['log_attack_time'], 2)}")
    L.append(f"szczyt transjentu (obwiednia 1 ms, pierwsze 50 ms) @ {fmt(e['time_to_transient_peak_ms'], 2)} ms · szczyt wolnej obwiedni (30 ms) @ {fmt(e['time_to_peak_slow_ms'], 1)} ms")
    d = e["decay_ms_from_slow_peak"]
    L.append("zanik od szczytu: " + " · ".join(f"{k} dB @ {fmt(v, 0)} ms" for k, v in d.items()))
    L.append(f"nachylenie zaniku (-3..-23 dB): {fmt(e['decay_slope_db_per_s'], 0)} dB/s · T60 ekstrapol.: {fmt(e['t60_extrapolated_ms'], 0)} ms")
    L.append(f"centroid czasowy {fmt(e['temporal_centroid_ms'], 0)} ms · czas efektywny (>40%) {fmt(e['effective_duration_ms'], 0)} ms")
    L.append(f"crest calosc {fmt(e['crest_db'], 1)} dB · crest pierwszych 15 ms {fmt(e['crest_head15_db'], 1)} dB · RMS 0-15 ms vs reszta {fmt(e['head15_vs_body_rms_db'], 1)} dB")
    if res["band_decays"]:
        L.append("\n| pasmo | szczyt @ ms | do -20 dB @ ms | poziom wzgl. calosci |\n|---|---|---|---|")
        for k, v in res["band_decays"].items():
            L.append(f"| {k} | {fmt(v['peak_ms'], 1)} | {fmt(v['to_-20dB_ms'], 0)} | {fmt(v['level_db_rel_total'], 1)} dB |")
    p = res["pitch"]
    L.append("\n## Pitch")
    if res["type"] == "kick" and "model" in p:
        L.append(f"model zjazdu: **{p['model']}** · parametry {[round(v, 5) for v in p['params']]}")
        L.append("dopasowania (rms w cyklach / AIC): " + ", ".join(f"{k}: {v['rms_cycles']} / {v['aic']}" for k, v in p["fits"].items()))
        L.append(f"start (model, t=0): {fmt(p['f_start_model'], 0)} Hz ({note_str(p['f_start_model'])}) · pierwszy pelny cykl: {fmt(p['f_first_cycle'], 0)} Hz")
        L.append(f"koniec (model): {fmt(p['f_end_model'], 2)} Hz ({note_str(p['f_end_model'])}) · zjazd {fmt(p['drop_semitones'], 1)} polt.")
        L.append(f"polowa zjazdu @ {fmt(p['t_half_ms'], 1)} ms · do 1 poltonu od ogona @ {fmt(p['settle_1st_ms'], 1)} ms · do 3 centow @ {fmt(p.get('settle_3c_ms'), 1)} ms · do ogon+5 Hz @ {fmt(p['t_to_fend_plus5_ms'], 1)} ms")
        if p.get("tail"):
            t = p["tail"]
            L.append(f"**ogon: {t['f0_hz']:.2f} Hz = {t['note']['note']} {t['note']['cents']:+.1f} c** (okno {fmt(t['window_ms'][0], 0)}-{fmt(t['window_ms'][1], 0)} ms, dryf {t['drift_cents']:+.1f} c"
                     + (f", wzgl. tonacji {t['vs_key_cents']:+.0f} c" if "vs_key_cents" in t else "") + ")")
    elif p.get("median_f0_hz"):
        L.append(f"mediana F0 {p['median_f0_hz']:.2f} Hz = {note_str(p['median_f0_hz'])} · dzwieczne {p['voiced_pct']:.0f}% klatek")
        L.append(f"zakres pitchu (p5..p95) {p['range_cents_p5_p95'][0]:+.0f}..{p['range_cents_p5_p95'][1]:+.0f} c wzgl. mediany")
        v = p.get("vibrato")
        if v:
            L.append(f"modulacja pitchu: {v['rate_hz']:.2f} Hz, glebokosc ±{v['depth']:.0f} c (wyrazistosc szczytu {v['peak_prominence']:.1f}x)")
    else:
        L.append("brak wiarygodnego pitchu (szum / perkusja)")
    am = p.get("amp_mod")
    if am:
        L.append(f"modulacja glosnosci: {am['rate_hz']:.2f} Hz, glebokosc ±{am['depth']:.1f} dB (wyrazistosc {am['peak_prominence']:.1f}x)")
    if p.get("curve_ms"):
        L.append("\n| t [ms] | " + " | ".join(p["curve_ms"].keys()) + " |")
        L.append("|---|" + "---|" * len(p["curve_ms"]))
        L.append("| F0 [Hz] | " + " | ".join(fmt(v, 0) for v in p["curve_ms"].values()) + " |")
        L.append("| nuta | " + " | ".join(note_str(v) for v in p["curve_ms"].values()) + " |")
    h = res.get("harmonics")
    if h:
        L.append(f"\n## Harmoniczne (okno {fmt(h['window_ms'][0], 0)}-{fmt(h['window_ms'][1], 0)} ms, F0 {h['f0_used']:.1f} Hz, {h['cycles']} cykli, {h['harmonics_measured']} harm.)")
        L.append("H1..H16 [dB wzgl. H1]: " + ", ".join(f"{v:.0f}" for v in h["levels_db_rel_h1"]))
        L.append(f"THD {h['thd_pct']:.0f}% ({h['thd_db']:.1f} dB) · parzyste/nieparzyste {h['even_odd_db']:+.1f} dB · tristimulus {[round(v, 2) for v in h['tristimulus']]} · srodek harm. H{h['harmonic_centroid']:.1f}")
        L.append(f"energia nieharmoniczna (aliasing + szum) {h['inharmonic_db']:.1f} dB wzgl. harmonicznych")
    L.append("\n## Pasma")
    L.append("| " + " | ".join(f"{n} {lo}-{hi}" for n, lo, hi in BANDS) + " |")
    L.append("|" + "---|" * len(BANDS))
    L.append("| " + " | ".join(f"{res['bands']['share_pct'][n]:.1f}%" for n, _, _ in BANDS) + " |")
    L.append("\n## Przebieg w czasie (dB; obwiednie wzgl. wlasnego max, pasma wzgl. najglosniejszego pasma)")
    cols = ["slow_db", "fast_db"] + [b[0] for b in BANDS]
    L.append("| t [ms] | " + " | ".join(cols) + " |")
    L.append("|---|" + "---|" * len(cols))
    for t_, row in res["timeseries"].items():
        L.append(f"| {t_} | " + " | ".join(fmt(max(row[c], -99), 1) if row[c] is not None else "" for c in cols) + " |")
    s = res["spectrum"]
    L.append("\n## Widmo")
    L.append("(widmo usrednia caly dzwiek razem ze zjazdem – do strojenia uzywaj sekcji Pitch / ogon)")
    L.append("szczyty (wygladzone 1/48 okt.): " + "; ".join(f"{q['hz']} Hz {q['note']} {q['db_rel_max']} dB" for q in s["peaks"]))
    dsc = s["descriptors"]
    if dsc:
        L.append("deskryptory (mediana / IQR): " + " · ".join(
            f"{k} {fmt(v['median'], 3 if k in ('flatness', 'flux') else 0)}/{fmt(v['iqr'], 3 if k in ('flatness', 'flux') else 0)}" for k, v in dsc.items()))
    c = res["clipping"]
    L.append("\n## Clipping")
    L.append(f"{c['verdict']} · serie ≥3 probek przy szczycie: {c.get('flat_runs_ge3')} (najdl. {c.get('longest_run_samples')}) · "
             f"{fmt(c.get('pct_samples_near_peak'), 3)}% probek ≥ -0.01 dB · krawedz histogramu {fmt(c.get('hist_edge_ratio'), 1)}x · kurtoza {fmt(c.get('kurtosis'), 2)}")
    st = res["stereo"]
    L.append("\n## Stereo")
    if not st:
        L.append("mono (1 kanal)")
    elif st.get("identical_channels"):
        L.append("kanaly identyczne (dual mono)")
    else:
        L.append(f"korelacja {st['correlation']:.2f} · S/M {st['side_to_mid_db']:.1f} dB · strata po zsumowaniu do mono {st['mono_loss_db']:.2f} dB · balans L/R {st['balance_LR_db']:+.2f} dB")
        L.append("| pasmo | korelacja | S/M dB |\n|---|---|---|")
        for b in st["bands"]:
            L.append(f"| {b['band']} | {b['correlation']:.2f} | {b['side_to_mid_db']:.1f} |")
    ch = res["checks"]
    L.append("\n## Technikalia")
    L.append(f"DC: {', '.join(fmt(v, 1) for v in ch['dc_offset_dbfs'])} dBFS · pierwsza probka {fmt(ch['first_sample_dbfs'], 1)} dBFS · ostatnia {fmt(ch['last_sample_dbfs'], 1)} dBFS · ostatnie 5 ms {fmt(ch['last_5ms_rms_db_rel_peak'], 1)} dB wzgl. szczytu")
    return "\n".join(L) + "\n"


# ----------------------------------------------------------------------------- plots

def _plt():
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    plt.rcParams.update({"font.size": 11, "axes.grid": True, "grid.alpha": 0.35, "lines.linewidth": 1.6,
                         "figure.dpi": 100, "savefig.dpi": 100, "axes.titlesize": 12})
    return plt


FREQ_TICKS = [20, 30, 40, 50, 60, 80, 100, 150, 200, 300, 500, 1000, 2000, 5000, 10000, 20000]


def _freq_axis(ax, lo=20, hi=20000, axis="x"):
    lab = [f"{t // 1000}k" if t >= 1000 else str(t) for t in FREQ_TICKS]
    if axis == "x":
        ax.set_xscale("log")
        ax.set_xlim(lo, hi)
        ax.set_xticks(FREQ_TICKS, lab)
        ax.minorticks_off()
    else:
        ax.set_yscale("log")
        ax.set_ylim(lo, hi)
        ax.set_yticks(FREQ_TICKS, lab)
        ax.minorticks_off()


def stitched_spectrogram(ax, mono, sr, onset, end, t_max_ms, plt):
    y = mono[onset:end + int(0.02 * sr)]
    vmax = None
    for (n, fmin, fmax) in ((1 << int(round(math.log2(0.17 * sr))), 20, 500), (1024 if sr <= 50000 else 2048, 500, sr / 2)):
        hop = max(16, int(sr * 0.002))
        yy = np.pad(y, (n // 2, n // 2))
        f, t, Z = signal.stft(yy, sr, window="hann", nperseg=n, noverlap=n - hop, boundary=None, padded=False)
        t = (t - (n // 2) / sr) * 1000
        S = pdb(np.abs(Z) ** 2)
        if vmax is None:
            vmax = S.max()
        m = (f >= fmin) & (f <= fmax)
        mesh = ax.pcolormesh(t, f[m], S[m] - vmax, vmin=-90, vmax=0, cmap="magma", shading="auto", rasterized=True)
    _freq_axis(ax, 20, min(20000, sr / 2), axis="y")
    ax.set_xlim(0, t_max_ms)
    ax.set_ylabel("Hz")
    ax.set_xlabel("ms")
    cb = plt.colorbar(mesh, ax=ax, pad=0.01)
    cb.set_label("dB wzgl. max")
    ax.axhline(500, color="w", lw=0.6, ls=":", alpha=0.6)


def plot_report(res, out):
    plt = _plt()
    P = res["_plot"]
    x, sr, on, end = P["x"], P["sr"], P["onset"], P["end"]
    mono = x.mean(axis=1)
    L_ms = res["timing"]["sound_length_ms"]
    t_max = min(max(L_ms * 1.05, 50), 1000 * (len(x) - on) / sr)
    files = []
    pitch = res["pitch"]

    # ---- 1: przebieg + zoom + obwiednia
    fig, axs = plt.subplots(3, 1, figsize=(14, 9), gridspec_kw={"height_ratios": [1, 1, 1.2]})
    t = (np.arange(len(x)) - on) / sr * 1000
    for c in range(x.shape[1]):
        axs[0].plot(t, x[:, c], lw=0.8, label=["L", "R"][c] if x.shape[1] == 2 else "mono", alpha=0.85)
    axs[0].set_xlim(-2, t_max)
    axs[0].set_ylim(-1.05, 1.05)
    axs[0].set_title(f"Image 1 – {res['info']['file']}: przebieg (0 ms = onset)")
    axs[0].legend(loc="upper right")
    if res["type"] == "kick" and "settle_1st_ms" in pitch:
        for a in axs:
            a.axvline(pitch["settle_1st_ms"], color="tab:red", ls="--", lw=1)
        axs[0].text(pitch["settle_1st_ms"], 0.9, f" koniec zjazdu (1 polt.) {pitch['settle_1st_ms']:.0f} ms", color="tab:red")
    z = (t >= -1) & (t <= 50)
    axs[1].plot(t[z], mono[z], color="k", lw=1)
    axs[1].set_xlim(-1, 50)
    axs[1].set_title("Zoom 0–50 ms (mono)")
    if res["type"] == "kick" and pitch.get("curve_ms"):
        for tp in (5, 10, 20):
            if str(tp) in pitch["curve_ms"]:
                axs[1].annotate(f"{tp} ms: {pitch['curve_ms'][str(tp)]:.0f} Hz", (tp, 0.85), color="tab:blue")
                axs[1].axvline(tp, color="tab:blue", ls=":", lw=0.8)
    axs[2].plot(t, np.maximum(P["fast_db"], -90), lw=0.8, color="tab:gray", label="szybka (1 ms)")
    axs[2].plot(t, np.maximum(P["slow_db"], -90), lw=2, color="tab:blue", label="wolna (30 ms)")
    axs[2].set_xlim(-2, t_max)
    axs[2].set_ylim(-72, 3)
    axs[2].set_yticks(np.arange(-72, 1, 6))
    axs[2].set_ylabel("dB")
    axs[2].set_xlabel("ms")
    dec = res["envelope"]["decay_ms_from_slow_peak"]
    pk_ms = res["envelope"]["time_to_peak_slow_ms"]
    for lvl, v in dec.items():
        if v is not None:
            axs[2].plot(pk_ms + v, int(lvl), "o", color="tab:red")
            axs[2].annotate(f"{lvl} dB @ {pk_ms + v:.0f} ms", (pk_ms + v, int(lvl)), textcoords="offset points", xytext=(6, 4), color="tab:red")
    m = res["metering"]
    axs[2].text(0.99, 0.95, f"TP {m['true_peak_dbtp']:.2f} dBTP | max mom. {fmt(m['max_momentary_lufs'], 1)} LUFS\ncrest {res['envelope']['crest_db']:.1f} dB | atak 10-90% {res['envelope']['attack_10_90_ms']:.2f} ms",
                transform=axs[2].transAxes, ha="right", va="top", bbox=dict(fc="w", ec="0.6"))
    axs[2].legend(loc="lower left")
    axs[2].set_title("Obwiednia (dB wzgl. max)")
    fig.tight_layout()
    fp = os.path.join(out, "1_przebieg.png")
    fig.savefig(fp)
    plt.close(fig)
    files.append(fp)

    # ---- 2: spektrogram + pitch
    fig, axs = plt.subplots(2, 1, figsize=(14, 9))
    stitched_spectrogram(axs[0], mono, sr, on, end, t_max, plt)
    axs[0].set_title("Image 2 – spektrogram (log f; <500 Hz okno dlugie, >500 Hz krotkie; 0..-90 dB)")
    kp = P["kp"]
    if res["type"] == "kick" and kp:
        tt = np.linspace(0, t_max / 1000, 600)
        ff = f_model(kp["model"], kp["params"], tt)
        axs[0].plot(tt * 1000, ff, color="cyan", lw=1.2, ls="--", label="model F0")
        axs[0].legend(loc="upper right")
        axs[1].plot(kp["zc_t"] * 1000, kp["zc_f"], ".", ms=3, color="0.5", label="przejscia przez zero")
        axs[1].plot(tt * 1000, ff, color="tab:red", lw=2, label=f"model {kp['model']}")
        if pitch.get("tail"):
            tl = pitch["tail"]
            axs[1].axhline(tl["f0_hz"], color="tab:green", ls="--")
            axs[1].text(t_max * 0.6, tl["f0_hz"] * 1.08, f"ogon {tl['f0_hz']:.2f} Hz = {tl['note']['note']} {tl['note']['cents']:+.0f} c", color="tab:green", fontsize=12)
        for tp in (0, 10, 20, 50, 100):
            if str(tp) in pitch.get("curve_ms", {}):
                v = pitch["curve_ms"][str(tp)]
                axs[1].plot(tp, v, "o", color="tab:red")
                axs[1].annotate(f"{v:.0f} Hz", (tp, v), textcoords="offset points", xytext=(5, 5))
        axs[1].legend(loc="upper right")
    else:
        tt, ff = P["tonal"]
        if tt is not None:
            axs[1].plot(tt * 1000, ff, color="tab:red", lw=1.5, label="F0 (YIN)")
            axs[0].plot(tt * 1000, ff, color="cyan", lw=1, ls="--")
            if pitch.get("median_f0_hz"):
                axs[1].axhline(pitch["median_f0_hz"], color="tab:green", ls="--")
                axs[1].text(0.02, 0.9, f"mediana {pitch['median_f0_hz']:.1f} Hz = {note_str(pitch['median_f0_hz'])}", transform=axs[1].transAxes, color="tab:green")
    _freq_axis(axs[1], 20, min(20000, sr / 2), axis="y")
    lo = 20
    finite = None
    if res["type"] == "kick" and kp:
        finite = kp["zc_f"]
    elif P["tonal"][1] is not None:
        finite = P["tonal"][1][np.isfinite(P["tonal"][1])]
    if finite is not None and len(finite):
        axs[1].set_ylim(max(20, np.min(finite) / 1.5), min(sr / 2, np.max(finite) * 1.5))
    axs[1].set_xlim(0, t_max)
    axs[1].set_xlabel("ms")
    axs[1].set_ylabel("Hz (log)")
    axs[1].set_title("Pitch")
    fig.tight_layout()
    fp = os.path.join(out, "2_spektrogram_pitch.png")
    fig.savefig(fp)
    plt.close(fig)
    files.append(fp)

    # ---- 3: widmo + pasma w czasie
    fig, axs = plt.subplots(2, 1, figsize=(14, 9))
    f, Pp = P["spec_f"], P["spec_P"]
    m_ = (f >= 15) & (f <= sr / 2)
    fs = f[m_]
    sm = pdb(frac_octave_smooth(fs, Pp[m_], 6))
    tilt = 4.5 * np.log2(fs / 1000)
    sm_t = sm + tilt
    sm_t -= sm_t.max()
    sm0 = sm - sm.max()
    axs[0].plot(fs, sm0, color="0.6", lw=1.2, label="bez nachylenia")
    axs[0].plot(fs, sm_t, color="tab:blue", lw=2, label="+4.5 dB/okt (jak SPAN)")
    for name, lo_, hi_ in BANDS:
        axs[0].axvspan(lo_, hi_, alpha=0.06 if BANDS.index((name, lo_, hi_)) % 2 else 0.12, color="tab:orange")
        axs[0].text(math.sqrt(lo_ * hi_), -4, name, ha="center", fontsize=10, color="tab:orange")
    shown = []
    for q in sorted(res["spectrum"]["peaks"], key=lambda q: -q["db_rel_max"]):
        if q["db_rel_max"] < -30 or any(abs(math.log2(q["hz"] / s_)) < 1 / 3 for s_ in shown) or len(shown) >= 6:
            continue
        shown.append(q["hz"])
        i = np.argmin(np.abs(fs - q["hz"]))
        axs[0].annotate(f"{q['hz']:.0f} Hz\n{q['note']}", (fs[i], sm0[i]), textcoords="offset points", xytext=(0, 8), ha="center", fontsize=9)
    _freq_axis(axs[0], 20, min(20000, sr / 2))
    axs[0].set_ylim(-66, 3)
    axs[0].set_yticks(np.arange(-66, 1, 6))
    axs[0].set_ylabel("dB")
    axs[0].legend(loc="lower left")
    axs[0].set_title("Image 3 – srednie widmo aktywnej czesci (wygladzone 1/6 okt.)")
    tt = (np.arange(len(x)) - on) / sr * 1000
    for name in P["band_series"]:
        axs[1].plot(tt, np.maximum(db(P["band_series"][name]), -80), label=f"{name} {res['bands']['share_pct'][name]:.0f}%")
    axs[1].set_xlim(0, t_max)
    axs[1].set_ylim(-60, 3)
    axs[1].set_yticks(np.arange(-60, 1, 6))
    axs[1].set_xlabel("ms")
    axs[1].set_ylabel("dB")
    axs[1].legend(loc="upper right", ncol=3)
    axs[1].set_title("Energia w pasmach w czasie (wzgl. najglosniejszego pasma; % = udzial energii)")
    fig.tight_layout()
    fp = os.path.join(out, "3_widmo_pasma.png")
    fig.savefig(fp)
    plt.close(fig)
    files.append(fp)
    return files


# ----------------------------------------------------------------------------- compare

def compare(resA, resB, out):
    plt = _plt()
    PA, PB = resA["_plot"], resB["_plot"]
    sr = PA["sr"]
    if PB["sr"] != sr:
        raise SystemExit("Rozne sample rate – przekonwertuj pliki do tego samego sr.")
    mA = PA["x"].mean(1)[PA["onset"]:]
    mB = PB["x"].mean(1)[PB["onset"]:]
    # dokladne wyrownanie korelacja w +-5 ms na pierwszych 100 ms
    n = int(0.1 * sr)
    lag_max = int(0.005 * sr)
    a = mA[:n]
    b = mB[:n + lag_max]
    best, lag = -1, 0
    for l_ in range(-lag_max, lag_max + 1):
        if l_ >= 0:
            bb = mB[l_:l_ + len(a)]
            aa = a[:len(bb)]
        else:
            aa = mA[-l_:-l_ + n]
            bb = mB[:len(aa)]
        if len(aa) < 10:
            continue
        c = np.sum(aa * bb) / (np.sqrt(np.sum(aa * aa) * np.sum(bb * bb)) + EPS)
        if c > best:
            best, lag = c, l_
    if best < 0.5:  # rozne dzwieki – korelacja niewiarygodna, zostaje wyrownanie po onsecie
        lag = 0
    if lag >= 0:
        mB_al = mB[lag:]
        mA_al = mA
    else:
        mA_al = mA[-lag:]
        mB_al = mB
    gain_db = resA["metering"]["hit_loudness_lufs_nonstd"] - resB["metering"]["hit_loudness_lufs_nonstd"]
    g = 10 ** (gain_db / 20)
    N = min(len(mA_al), len(mB_al))
    A_ = mA_al[:N]
    B_ = mB_al[:N] * g
    gls = np.sum(A_ * mB_al[:N]) / (np.sum(mB_al[:N] ** 2) + EPS)
    resid = A_ - gls * mB_al[:N]
    null_db = float(pdb(np.sum(resid ** 2) / (np.sum(A_ ** 2) + EPS)))

    rows = []

    def add(name, va, vb, unit="", n=1, kind="diff"):
        if va is None or vb is None:
            rows.append((name, va, vb, None, unit, n))
            return
        dv = (vb - va) if kind == "diff" else (1200 * math.log2(vb / va) if va > 0 and vb > 0 else None)
        rows.append((name, va, vb, dv, unit if kind == "diff" else "c", n))
    ma, mb = resA["metering"], resB["metering"]
    add("true peak dBTP", ma["true_peak_dbtp"], mb["true_peak_dbtp"], " dB", 2)
    add("max momentary LUFS", ma["max_momentary_lufs"], mb["max_momentary_lufs"], " LU", 2)
    add("hit loudness LUFS", ma["hit_loudness_lufs_nonstd"], mb["hit_loudness_lufs_nonstd"], " LU", 2)
    if resA.get("loop") and resB.get("loop"):
        add("petla integrated LUFS", resA["loop"]["integrated_lufs"], resB["loop"]["integrated_lufs"], " LU", 2)
    ea, eb = resA["envelope"], resB["envelope"]
    add("crest dB", ea["crest_db"], eb["crest_db"], " dB")
    add("crest 0-15 ms dB", ea["crest_head15_db"], eb["crest_head15_db"], " dB")
    add("RMS 0-15 ms vs reszta dB", ea["head15_vs_body_rms_db"], eb["head15_vs_body_rms_db"], " dB")
    add("atak 10-90% ms", ea["attack_10_90_ms"], eb["attack_10_90_ms"], " ms", 2)
    for lvl in ("-6", "-20", "-40"):
        add(f"zanik do {lvl} dB ms", ea["decay_ms_from_slow_peak"][lvl], eb["decay_ms_from_slow_peak"][lvl], " ms", 0)
    add("dlugosc dzwieku ms", resA["timing"]["sound_length_ms"], resB["timing"]["sound_length_ms"], " ms", 0)
    pa, pb = resA["pitch"], resB["pitch"]
    if resA["type"] == "kick" and resB["type"] == "kick" and "model" in pa and "model" in pb:
        add("pitch start (model) Hz", pa["f_start_model"], pb["f_start_model"], kind="cents", n=0)
        add("polowa zjazdu ms", pa["t_half_ms"], pb["t_half_ms"], " ms", 1)
        add("zjazd do 1 polt. ms", pa["settle_1st_ms"], pb["settle_1st_ms"], " ms", 1)
        if pa.get("tail") and pb.get("tail"):
            add("ogon Hz", pa["tail"]["f0_hz"], pb["tail"]["f0_hz"], kind="cents", n=2)
        for tp in ("5", "10", "20", "50", "100"):
            if tp in pa["curve_ms"] and tp in pb["curve_ms"]:
                add(f"pitch @ {tp} ms Hz", pa["curve_ms"][tp], pb["curve_ms"][tp], kind="cents", n=0)
    ha, hb = resA.get("harmonics"), resB.get("harmonics")
    if ha and hb:
        add("THD %", ha["thd_pct"], hb["thd_pct"], " pp", 0)
        add("parzyste/nieparzyste dB", ha["even_odd_db"], hb["even_odd_db"], " dB")
        add("energia nieharmoniczna dB", ha["inharmonic_db"], hb["inharmonic_db"], " dB")
    for name, _, _ in BANDS:
        add(f"udzial {name} %", resA["bands"]["share_pct"][name], resB["bands"]["share_pct"][name], " pp")
    da, dbb = resA["spectrum"]["descriptors"], resB["spectrum"]["descriptors"]
    if da and dbb:
        add("centroid widma Hz (mediana)", da["centroid_hz"]["median"], dbb["centroid_hz"]["median"], kind="cents", n=0)
        add("plaskosc widma (mediana)", da["flatness"]["median"], dbb["flatness"]["median"], "", 3)
    sa, sb = resA.get("stereo"), resB.get("stereo")
    if sa and sb and sa.get("bands") and sb.get("bands"):
        add("korelacja <120 Hz", sa["bands"][0]["correlation"], sb["bands"][0]["correlation"], "", 2)
        add("korelacja calosc", sa["correlation"], sb["correlation"], "", 2)

    # roznica widm (po wyrownaniu glosnosci)
    fA, PA_ = average_spectrum(A_, sr)
    fB, PB_ = average_spectrum(B_, sr)
    m_ = (fA >= 20) & (fA <= min(20000, sr / 2))
    fs = fA[m_]
    sA = pdb(frac_octave_smooth(fs, PA_[m_], 6))
    sB = pdb(frac_octave_smooth(fs, PB_[m_], 6))
    diff = sB - sA
    floor = max(sA.max(), sB.max()) - 90
    diff[(sA < floor) & (sB < floor)] = np.nan  # oba sygnaly pod podloga – roznica bez sensu
    third = []
    centers = 1000 * 2 ** (np.arange(-17, 14) / 3)
    for c in centers:
        if c > min(20000, sr / 2):
            break
        mm = (fs >= c * 2 ** (-1 / 6)) & (fs < c * 2 ** (1 / 6))
        if mm.any() and np.isfinite(diff[mm]).any():
            third.append((c, float(np.nanmean(diff[mm]))))
    big = sorted(third, key=lambda q: -abs(q[1]))[:6]

    L = [f"# DOOMSCOPE – porownanie\n**A:** {resA['info']['file']}  ·  **B:** {resB['info']['file']}"]
    L.append(f"wyrownanie: po onsecie (prog -40 dB) + korekta korelacja {1000 * lag / sr:+.2f} ms (max korelacja {best:.3f}; korekta tylko gdy > 0.5) · B przeskalowane o {gain_db:+.2f} dB (hit loudness) do widm i wykresow")
    L.append(f"null test (A − g·B, g dopasowane LS): residuum {null_db:.1f} dB wzgl. A (0 dB = zupelnie inne, < -40 dB = prawie identyczne)\n")
    L.append("| metryka | A | B | Δ (B−A) |\n|---|---|---|---|")
    for name, va, vb, dv, unit, n in rows:
        L.append(f"| {name} | {fmt(va, n)} | {fmt(vb, n)} | {('–' if dv is None else f'{dv:+.{n}f}{unit}')} |")
    L.append("\n## Najwieksze roznice widma (1/3 okt., B−A po wyrownaniu glosnosci)")
    L.append(", ".join(f"{c:.0f} Hz: {d:+.1f} dB" for c, d in sorted(big)))
    L.append("\nwszystkie pasma 1/3 okt.: " + ", ".join(f"{c:.0f}:{d:+.1f}" for c, d in third))
    with open(os.path.join(out, "compare.md"), "w") as fh:
        fh.write("\n".join(L) + "\n")

    # wykres 1: obwiednie, zoom, pitch
    fig, axs = plt.subplots(3, 1, figsize=(14, 9))
    t = np.arange(N) / sr * 1000
    tmax = min(N / sr * 1000, max(resA["timing"]["sound_length_ms"], resB["timing"]["sound_length_ms"]) * 1.05)
    eA = db(rms_env(A_, sr, 30))
    eB = db(rms_env(B_, sr, 30))
    ref = max(eA.max(), eB.max())
    axs[0].plot(t, np.maximum(eA - ref, -90), color="tab:blue", lw=2, label=f"A: {resA['info']['file']}")
    axs[0].plot(t, np.maximum(eB - ref, -90), color="tab:red", lw=2, ls="--", label=f"B: {resB['info']['file']} ({gain_db:+.1f} dB)")
    axs[0].set_xlim(0, tmax)
    axs[0].set_ylim(-66, 3)
    axs[0].set_yticks(np.arange(-66, 1, 6))
    axs[0].legend(loc="upper right")
    axs[0].set_title("Image 1 – porownanie: obwiednie RMS 30 ms (dB)")
    z = t <= 50
    axs[1].plot(t[z], A_[z], color="tab:blue", lw=1.2, label="A")
    axs[1].plot(t[z], B_[z], color="tab:red", lw=1.2, ls="--", label="B")
    axs[1].set_title("Zoom 0–50 ms")
    axs[1].legend(loc="upper right")
    if resA["type"] == "kick" and resB["type"] == "kick" and PA["kp"] and PB["kp"]:
        tt = np.linspace(0, tmax / 1000, 600)
        fa = f_model(PA["kp"]["model"], PA["kp"]["params"], tt)
        fb = f_model(PB["kp"]["model"], PB["kp"]["params"], tt + lag / sr)
        axs[2].plot(tt * 1000, fa, color="tab:blue", lw=2, label="A")
        axs[2].plot(tt * 1000, fb, color="tab:red", lw=2, ls="--", label="B")
        _freq_axis(axs[2], max(20, min(fa.min(), fb.min()) / 1.3), min(sr / 2, max(fa.max(), fb.max()) * 1.3), axis="y")
        axs[2].set_title("Pitch (model)")
        axs[2].legend(loc="upper right")
    else:
        for P_, col, ls, lab in ((PA, "tab:blue", "-", "A"), (PB, "tab:red", "--", "B")):
            tt, ff = P_["tonal"]
            if tt is not None:
                axs[2].plot(tt * 1000, ff, color=col, ls=ls, label=lab)
        axs[2].set_yscale("log")
        axs[2].set_title("Pitch (YIN)")
        axs[2].legend(loc="upper right")
    axs[2].set_xlim(0, tmax)
    axs[2].set_xlabel("ms")
    fig.tight_layout()
    fig.savefig(os.path.join(out, "compare_1_czas.png"))
    plt.close(fig)

    fig, axs = plt.subplots(2, 1, figsize=(14, 9))
    top = max(sA.max(), sB.max())
    axs[0].plot(fs, sA - top, color="tab:blue", lw=2, label="A")
    axs[0].plot(fs, sB - top, color="tab:red", lw=2, ls="--", label=f"B ({gain_db:+.1f} dB)")
    _freq_axis(axs[0], 20, min(20000, sr / 2))
    axs[0].set_ylim(-66, 3)
    axs[0].set_yticks(np.arange(-66, 1, 6))
    axs[0].legend(loc="lower left")
    axs[0].set_title("Image 2 – widma (1/6 okt., po wyrownaniu glosnosci)")
    axs[1].plot(fs, diff, color="k", lw=2)
    axs[1].axhline(0, color="0.3")
    axs[1].axhspan(-3, 3, color="tab:green", alpha=0.12)
    for c, d in big:
        axs[1].annotate(f"{c:.0f} Hz\n{d:+.1f} dB", (c, d), textcoords="offset points", xytext=(0, 8 if d > 0 else -26), ha="center", fontsize=9)
    _freq_axis(axs[1], 20, min(20000, sr / 2))
    lim = max(6, min(30, math.ceil(np.nanmax(np.abs(diff)) / 3) * 3 + 3))
    axs[1].set_ylim(-lim, lim)
    axs[1].set_ylabel("B − A [dB]")
    axs[1].set_title("Roznica B − A (zielony pas = ±3 dB)")
    fig.tight_layout()
    fig.savefig(os.path.join(out, "compare_2_widmo.png"))
    plt.close(fig)


# ----------------------------------------------------------------------------- selftest (syntetyczne sample o znanych parametrach)

def synth_kick(sr=48000, f0=2000.0, fe=49.0, tau=0.02, drive=4.0, length=0.6, click=True, stereo_top=False):
    t = np.arange(int(sr * length)) / sr
    ph = 2 * np.pi * (fe * t + (f0 - fe) * tau * (1 - np.exp(-t / tau)))
    y = np.sin(ph)
    amp = np.minimum(1, t / 0.0005) * np.where(t < length * 0.7, 1.0, np.cos((t - length * 0.7) / (length * 0.3) * np.pi / 2) ** 2)
    y = np.tanh(drive * y * amp) / np.tanh(drive)
    if click:
        rng = np.random.default_rng(1)
        n = rng.standard_normal(len(t)) * np.exp(-t / 0.002) * 0.3
        y = y + sos_filter(n, sr, lo=2000)
    y *= 0.89
    if stereo_top:
        hi = sos_filter(y, sr, lo=2000)
        return np.stack([y + 0.2 * hi, y - 0.2 * hi], 1)
    return np.stack([y, y], 1)


def selftest():
    ok = True

    def check(name, cond, detail):
        nonlocal ok
        ok &= bool(cond)
        print(f"[{'OK ' if cond else 'BLAD'}] {name}: {detail}")

    sr = 48000
    (b1, a1), (b2, a2) = k_weight_coeffs(48000)
    check("filtr K @48k (shelf b0)", abs(b1[0] - 1.53512485958697) < 1e-6, f"{b1[0]:.11f}")
    check("filtr K @48k (HP a1)", abs(a2[1] + 1.99004745483398) < 1e-6, f"{a2[1]:.11f}")
    t = np.arange(sr * 5) / sr
    s = np.sin(2 * np.pi * 997 * t)[:, None]
    L = integrated_lufs(s, sr)
    check("BS.1770: sinus 997 Hz 0 dBFS (1 kanal) = -3.01 LUFS", abs(L + 3.01) < 0.05, f"{L:.3f}")
    s2 = np.sin(2 * np.pi * sr / 4 * t + np.pi / 4)[:, None]
    tp = true_peak_db(s2)
    check("true peak: sinus fs/4 faza 45° (sample peak -3 dB) -> 0 dBTP", abs(tp) < 0.3, f"TP {tp:.2f} dBTP, SP {db(np.max(np.abs(s2))):.2f}")

    cases = [(2000, 49.0, 0.020, 4), (1200, 46.25, 0.035, 8), (3000, 55.0, 0.010, 2), (800, 43.65, 0.060, 6)]
    for f0, fe, tau, drv in cases:
        x = synth_kick(sr, f0, fe, tau, drv)
        res = analyze(x, sr, {"file": "synth", "sr": sr, "channels": 2, "length_ms": 600}, "auto")
        p = res["pitch"]
        tl = p.get("tail", {})
        tail = tl.get("f0_hz")
        if tail:
            w0, w1 = tl["window_ms"]
            tw = np.linspace(w0, w1, 200) / 1000
            f_true = float(np.median(fe + (f0 - fe) * np.exp(-tw / tau)))
        else:
            f_true = fe
        good = res["type"] == "kick" and tail and abs(1200 * math.log2(tail / f_true)) < 5 \
            and abs(1200 * math.log2(p["f_end_model"] / fe)) < 10
        check(f"kick f0={f0} fe={fe} tau={tau * 1000:.0f}ms drive={drv}: typ+strojenie ogona",
              good, f"typ={res['type']} ogon={fmt(tail, 2)} Hz (prawda w oknie {f_true:.2f}) {note_str(tail) if tail else ''}"
              f" · asymptota {p.get('f_end_model', 0):.2f} (prawda {fe})" + (" · OGON WCIAZ ZJEZDZA" if tl.get("still_gliding") else ""))
        if p.get("model"):
            ftrue = {tp_: fe + (f0 - fe) * math.exp(-tp_ / 1000 / tau) for tp_ in (5, 10, 20, 50)}
            errs = [1200 * math.log2(p["curve_ms"][str(k)] / v) for k, v in ftrue.items()]
            check("   krzywa pitchu @5/10/20/50 ms", max(abs(e) for e in errs) < 50,
                  "bledy [c]: " + ", ".join(f"{e:+.0f}" for e in errs) + f" · model {p['model']}")
    # screech z vibrato
    tt = np.arange(int(sr * 2)) / sr
    fm = 440 * 2 ** (50 / 1200 * np.sin(2 * np.pi * 6 * tt))
    ph = 2 * np.pi * np.cumsum(fm) / sr
    saw = 2 * ((ph / (2 * np.pi)) % 1) - 1
    saw = sos_filter(saw, sr, hi=12000) * 0.7
    res = analyze(np.stack([saw, saw], 1), sr, {"file": "screech", "sr": sr, "channels": 2, "length_ms": 2000}, "auto")
    p = res["pitch"]
    v = p.get("vibrato") or {}
    check("lead 440 Hz vibrato 6 Hz ±50 c", res["type"] == "tonal" and abs(p.get("median_f0_hz", 0) - 440) < 3
          and abs(v.get("rate_hz", 0) - 6) < 0.5 and 35 < v.get("depth", 0) < 65,
          f"F0 {fmt(p.get('median_f0_hz'), 1)} Hz, vibrato {fmt(v.get('rate_hz'), 2)} Hz ±{fmt(v.get('depth'), 0)} c")
    h = res.get("harmonics") or {}
    lv = h.get("levels_db_rel_h1", [0, 0, 0])
    check("pila: H2≈-6 dB, H3≈-9.5 dB", abs(lv[1] + 6.02) < 1.5 and abs(lv[2] + 9.54) < 1.5,
          f"H2 {lv[1]:.1f}, H3 {lv[2]:.1f}, parzyste/nieparzyste {fmt(h.get('even_odd_db'), 1)} dB")
    print("\nWYNIK:", "wszystko OK" if ok else "SA BLEDY")
    return ok


# ----------------------------------------------------------------------------- CLI

def run_analyze(path, args, out):
    x, sr, info = load_audio(path)
    res = analyze(x, sr, info, args.type, args.bpm, args.key)
    os.makedirs(out, exist_ok=True)
    md = report_md(res)
    with open(os.path.join(out, "report.md"), "w") as fh:
        fh.write(md)
    files = [] if args.no_plots else plot_report(res, out)
    with open(os.path.join(out, "report.json"), "w") as fh:
        json.dump(jsonable({k: v for k, v in res.items() if not k.startswith("_")}), fh, indent=1, ensure_ascii=False)
    return res, md, files


def main():
    ap = argparse.ArgumentParser(description="DOOMSCOPE – analizator sampli hardstyle")
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("analyze", "compare"):
        p = sub.add_parser(name)
        p.add_argument("files", nargs=1 if name == "analyze" else 2)
        p.add_argument("--type", default="auto", choices=["auto", "kick", "screech", "lead", "rbass", "tonal"])
        p.add_argument("--bpm", type=float, default=None)
        p.add_argument("--key", default=None, help="tonacja/nuta docelowa ogona, np. F#, G, Ab")
        p.add_argument("--out", default=None)
        p.add_argument("--no-plots", action="store_true")
        p.add_argument("--quiet", action="store_true")
    sub.add_parser("selftest")
    args = ap.parse_args()
    if args.cmd == "selftest":
        sys.exit(0 if selftest() else 1)
    if args.type in ("screech", "lead", "rbass"):
        args.type = "tonal"
    if args.cmd == "analyze":
        f = args.files[0]
        out = args.out or os.path.splitext(f)[0] + "_doomscope"
        res, md, files = run_analyze(f, args, out)
        if not args.quiet:
            print(md)
        print(f"-> {out}/report.md, report.json" + ("".join(f", {os.path.basename(p)}" for p in files)))
    else:
        a, b = args.files
        out = args.out or os.path.splitext(a)[0] + "_vs_" + os.path.splitext(os.path.basename(b))[0]
        resA, _, _ = run_analyze(a, args, os.path.join(out, "A"))
        resB, _, _ = run_analyze(b, args, os.path.join(out, "B"))
        compare(resA, resB, out)
        if not args.quiet:
            print(open(os.path.join(out, "compare.md")).read())
        print(f"-> {out}/compare.md, compare_1_czas.png, compare_2_widmo.png, A/, B/")


if __name__ == "__main__":
    main()
