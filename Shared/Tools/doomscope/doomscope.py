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

VERSION = "0.2.1"

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


def sub_tail_pitch(mono, sr, onset, end):
    """Nuta ogona mierzona TYLKO w pasmie subu (LP 130 Hz) dlugim oknem YIN (~190 ms) – odporne na przester i warstwy.
    Wynik: mediana, rozrzut (p10-p90 w centach), udzial dzwiecznych ramek, mediany w 3 oknach (dryf)."""
    L = end - onset
    a = onset + max(int(0.06 * sr), int(0.25 * L))
    b = onset + int(0.85 * L)
    if b - a < int(0.08 * sr):
        return None
    y = sos_filter(mono[a:b], sr, hi=130, order=6)
    frame = 1 << int(round(math.log2(0.19 * sr)))
    t, f, c = yin_track(y, sr, fmin=25, fmax=125, frame=frame, hop=max(64, int(sr * 0.004)))
    keep = (t >= frame / 2 / sr) & (t <= (len(y) - frame / 2) / sr)  # tylko pelne ramki
    f = f[keep]
    v = np.isfinite(f)
    if v.sum() < 5:
        return None
    fv = f[v]
    med = float(np.median(fv))
    cents = 1200 * np.log2(fv / med)
    thirds = [float(np.median(p)) for p in np.array_split(fv, 3) if len(p)]
    return {"f0_hz": med, "note": hz_to_note(med), "spread_cents": float(np.percentile(cents, 90) - np.percentile(cents, 10)),
            "voiced_pct": float(100 * v.mean()), "thirds_hz": thirds,
            "drift_cents": float(1200 * math.log2(thirds[-1] / thirds[0])) if len(thirds) > 1 else 0.0,
            "window_ms": [1000 * (a - onset) / sr, 1000 * (b - onset) / sr]}


def kick_pitch_yin(mono, sr, onset, end, f_tail):
    """Zapasowa krzywa pitchu kicka, gdy przejscia przez zero zawioda (mocny przester, warstwy):
    YIN w 5 rozdzielczosciach na sygnale LP 3 kHz, wymuszony spadek (kick nie rosnie), dopasowanie modelu exp do log f."""
    y = sos_filter(mono[onset:end], sr, hi=min(3000, 0.45 * sr), order=4)
    hop = max(16, int(sr * 0.001))
    sizes = [1 << int(round(math.log2(s * sr / 44100))) for s in (4096, 2048, 1024, 512, 256)]
    tracks = []
    for N in sizes:
        _, ff, _ = yin_track(y, sr, fmin=max(20, 2 * sr / N), fmax=min(6000, sr / 4), frame=N, hop=hop, thresh=0.2)
        tracks.append(np.where(ff >= 2.5 * sr / N, ff, np.nan))
    Lf = min(len(t_) for t_ in tracks)
    out = np.full(Lf, np.nan)
    for i in range(Lf):
        for ff in reversed(tracks):
            if np.isfinite(ff[i]):
                out[i] = ff[i]
                break
    times = np.arange(Lf) * hop / sr
    # bledy oktawowe w ogonie i ponizej ogona -> odrzuc; potem wymus spadek od poczatku
    out[(out < f_tail * 0.8)] = np.nan
    late = times > 0.15
    out[late & (np.abs(1200 * np.log2(np.where(np.isfinite(out), out, f_tail) / f_tail)) > 300)] = np.nan
    v = np.isfinite(out)
    if v.sum() < 10:
        return None
    mono_f = np.fmin.accumulate(np.where(v, out, np.inf))
    mono_f[~np.isfinite(mono_f)] = np.nan
    tv, fv = times[v], mono_f[v]
    head = tv <= 0.25
    if head.sum() < 5:
        return None
    f0_guess = float(np.nanmax(fv[head]))

    def res_(p):
        f0, tau = p
        return np.log(f_tail + (f0 - f_tail) * np.exp(-tv[head] / tau)) - np.log(fv[head])
    try:
        sol = optimize.least_squares(res_, [max(f0_guess, f_tail * 1.5), 0.02], bounds=([f_tail * 1.01, 1e-4], [20000, 1.0]))
        f0, tau = sol.x
        rms_c = float(1200 * np.sqrt(np.mean(sol.fun ** 2)) / math.log(2))
    except Exception:
        return None
    params = [f_tail, float(f0), float(tau)]
    tt = np.linspace(0, (end - onset) / sr, 4000)
    ff_ = f_model("exp", params, tt)
    st_above = 12 * np.log2(ff_ / f_tail)
    settle = tt[np.flatnonzero(st_above > 1)[-1]] if (st_above > 1).any() else 0.0
    t5 = tt[np.flatnonzero(ff_ > f_tail + 5)[-1]] if (ff_ > f_tail + 5).any() else 0.0
    half = f_tail + (f0 - f_tail) / 2
    t_half = tt[np.flatnonzero(ff_ > half)[-1]] if (ff_ > half).any() else 0.0
    return {"model": "exp", "params": params, "source": "YIN (przejscia przez zero zawiodly)",
            "fits": {"exp(YIN)": {"rms_cycles": None, "aic": None, "rms_cents": rms_c}},
            "f_end_model": f_tail, "f_start_model": float(f0), "f_first_cycle": f0_guess,
            "drop_semitones": float(12 * math.log2(f0 / f_tail)), "t_half_ms": 1000 * t_half,
            "settle_1st_ms": 1000 * settle, "t_to_fend_plus5_ms": 1000 * t5, "zc_t": tv, "zc_f": fv}


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
    # okno calkowania YIN = pierwsze W probek ramki -> centrujemy je na czasie ramki (i*hop)
    yp = np.pad(y, (W // 2, frame))
    n_frames = len(y) // hop + 1
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


# ----------------------------------------------------------------------------- P1: reassignment, HPSS, YIN check, sharpness

def reassigned_spectrogram(y, sr, n=None, hop=None, t_bins=None, f_bins=None):
    """Spektrogram reasygnowany (Auger-Flandrin): 3 FFT (h, t*h, dh/dt). Zwraca (t_edges_s, f_edges_hz, power2d).
    Znaki skalibrowane w selftest (sinus -> f, impuls -> t)."""
    n = n or (1 << int(round(math.log2(0.0107 * sr))))  # ~10.7 ms
    hop = hop or max(4, n // 32)
    win = np.hanning(n)
    tw = np.arange(n) - (n - 1) / 2
    dwin = np.gradient(win)
    pad = n // 2
    yp = np.pad(y, (pad, pad))
    fr = np.lib.stride_tricks.sliding_window_view(yp, n)[::hop]
    centers = np.arange(fr.shape[0]) * hop  # probka srodka ramki w y (po odjeciu pad)
    Xh = np.fft.rfft(fr * win, axis=1)
    Xt = np.fft.rfft(fr * (win * tw), axis=1)
    Xd = np.fft.rfft(fr * dwin, axis=1)
    mag2 = np.abs(Xh) ** 2
    ok = mag2 > mag2.max() * 1e-9
    den = np.where(ok, mag2, 1)
    fk = np.fft.rfftfreq(n, 1 / sr)[None, :]
    f_hat = fk - (sr / (2 * np.pi)) * np.imag(Xd * np.conj(Xh) / den)
    t_hat = (centers[:, None] + np.real(Xt * np.conj(Xh) / den)) / sr
    T = t_hat[ok]
    F = f_hat[ok]
    W = mag2[ok]
    t_bins = t_bins if t_bins is not None else np.linspace(0, len(y) / sr, 400)
    f_bins = f_bins if f_bins is not None else np.geomspace(20, sr / 2, 300)
    H, _, _ = np.histogram2d(T, F, bins=[t_bins, f_bins], weights=W)
    return t_bins, f_bins, H


def reassign_point_check(sr=48000):
    """Kalibracja: sinus 1 kHz -> f_hat, impuls -> t_hat."""
    n = 512
    t = np.arange(sr // 10) / sr
    tb = np.linspace(0, 0.1, 801)
    fb = np.linspace(900, 1100, 401)
    _, _, H = reassigned_spectrogram(np.sin(2 * np.pi * 1000 * t), sr, n=n, t_bins=tb, f_bins=fb)
    f_est = float(fb[:-1][np.argmax(H.sum(0))] + (fb[1] - fb[0]) / 2)
    imp = np.zeros(sr // 10)
    imp[int(0.05 * sr)] = 1
    tb2 = np.linspace(0.04, 0.06, 801)
    fb2 = np.geomspace(100, sr / 2, 50)
    _, _, H2 = reassigned_spectrogram(imp, sr, n=n, t_bins=tb2, f_bins=fb2)
    t_est = float(tb2[:-1][np.argmax(H2.sum(1))] + (tb2[1] - tb2[0]) / 2)
    return f_est, t_est


def hpss_share(y, sr):
    """Separacja harmoniczno-perkusyjna (FitzGerald, mediany). Zwraca udzial perkusyjny calosci i w 0-30 ms."""
    n = 1 << int(round(math.log2(0.023 * sr)))
    hop = n // 8
    if len(y) < n * 2:
        y = np.pad(y, (0, n * 2 - len(y)))
    f, t, Z = signal.stft(y, sr, nperseg=n, noverlap=n - hop, boundary="zeros", padded=True)
    S = np.abs(Z) ** 2
    lh = max(3, int(0.2 * sr / hop) | 1)
    lp = max(3, int(500 / (sr / n)) | 1)
    Hm = median_filter(S, size=(1, lh), mode="nearest")
    Pm = median_filter(S, size=(lp, 1), mode="nearest")
    den = Hm ** 2 + Pm ** 2
    mask_p = np.divide(Pm ** 2, den, out=np.zeros_like(den), where=den > (S.max() ** 2) * 1e-24)
    P = S * mask_p
    tot = S.sum() + EPS
    head = t <= 0.03
    return {"percussive_pct": float(100 * P.sum() / tot),
            "percussive_pct_0_30ms": float(100 * P[:, head].sum() / (S[:, head].sum() + EPS)),
            "percussive_pct_after_30ms": float(100 * P[:, ~head].sum() / (S[:, ~head].sum() + EPS)) if (~head).any() else None}


def yin_multires_check(mono, sr, onset, end, model, params):
    """Niezalezna kontrola krzywej pitchu kicka: YIN w 5 rozdzielczosciach, w kazdej ramce najkrotsze wazne okno."""
    y = mono[onset:end]
    hop = max(16, int(sr * 0.001))
    base = 44100
    sizes = [int(4096 * sr / base), int(2048 * sr / base), int(1024 * sr / base), int(512 * sr / base), int(256 * sr / base)]
    sizes = [1 << int(round(math.log2(s))) for s in sizes]
    tracks = []
    for N in sizes:
        tt, ff, cf = yin_track(y, sr, fmin=max(20, 2 * sr / N), fmax=min(6000, sr / 4), frame=N, hop=hop, thresh=0.2)
        valid_min = 2.5 * sr / N
        ff = np.where(ff >= valid_min, ff, np.nan)
        tracks.append(ff)
    L = min(len(t_) for t_ in tracks)
    out = np.full(L, np.nan)
    for i in range(L):
        for ff in reversed(tracks):  # od najkrotszego okna
            if np.isfinite(ff[i]):
                out[i] = ff[i]
                break
    times = np.arange(L) * hop / sr
    fm = f_model(model, params, times)
    v = np.isfinite(out)
    if v.sum() < 5:
        return None
    dev = 1200 * np.log2(out[v] / fm[v])
    octave_err = np.mean(np.abs(dev) > 600) * 100
    good = np.abs(dev) <= 600
    return {"frames": int(v.sum()), "median_abs_cents": float(np.median(np.abs(dev[good]))) if good.any() else None,
            "p90_abs_cents": float(np.percentile(np.abs(dev[good]), 90)) if good.any() else None,
            "octave_error_pct": float(octave_err), "_t": times, "_f": out}


def bark(f):
    return 13 * np.arctan(0.00076 * f) + 3.5 * np.arctan((f / 7500) ** 2)


def sharpness_approx(y, sr, cal_db=94.0):
    """Przyblizona ostrosc (acum) wg DIN 45692 na uproszczonym modelu Zwickera:
    energia w pasmach 0.5 Bark -> specyficzna glosnosc N'(z) ~ E^0.23 -> S = 0.11*sum(N' g z dz)/N.
    Kalibracja: sinus 0 dBFS = cal_db dB SPL. Tylko do porownan miedzy samplami (nie ISO 532-1)."""
    n = 1 << int(round(math.log2(0.046 * sr)))
    hop = n // 2
    if len(y) < n:
        y = np.pad(y, (0, n - len(y)))
    f, t, Z = signal.stft(y, sr, window="hann", nperseg=n, noverlap=n - hop, boundary=None, padded=True)
    P = (np.abs(Z) ** 2) * 2  # scipy.stft skaluje przez 1/sum(okno): moc sinusa 0 dBFS -> 0.5
    zb = bark(f)
    edges = np.arange(0, 24.01, 0.5)
    zc = (edges[:-1] + edges[1:]) / 2
    idx = np.digitize(zb, edges) - 1
    E = np.zeros((len(zc), P.shape[1]))
    for k in range(len(zc)):
        m = idx == k
        if m.any():
            E[k] = P[m].sum(0)
    L = 10 * np.log10(E / 0.5 + EPS) + cal_db  # dB SPL
    LTQ = 3.64 * (np.maximum(bark_inv(zc), 20) / 1000) ** -0.8 - 6.5 * np.exp(-0.6 * (bark_inv(zc) / 1000 - 3.3) ** 2) + 1e-3 * (bark_inv(zc) / 1000) ** 4
    Etq = 10 ** (LTQ / 10)[:, None]
    Ee = 10 ** (L / 10)
    Ns = 0.08 * (Etq ** 0.23) * ((0.5 + 0.5 * Ee / Etq) ** 0.23 - 1)
    Ns = np.maximum(Ns, 0)
    # rozmycie w gore (maskowanie), uproszczone: -10 dB/Bark
    for k in range(1, len(zc)):
        Ns[k] = np.maximum(Ns[k], Ns[k - 1] * 10 ** (-10 * 0.5 / 20))
    N = Ns.sum(0) * 0.5
    g = np.where(zc <= 15.8, 1.0, 0.15 * np.exp(0.42 * (zc - 15.8)) + 0.85)[:, None]
    S = 0.11 * (Ns * g * zc[:, None]).sum(0) * 0.5 / (N + EPS)
    act = N > N.max() * 0.1
    if not act.any():
        return None
    return {"sharpness_acum_median": float(np.median(S[act])), "sharpness_acum_max": float(np.max(S[act])),
            "loudness_sone_max_approx": float(N.max())}


def bark_inv(z):
    # przyblizenie odwrotne (Traunmuller)
    return 1960 * (z + 0.53) / (26.28 - z)


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
    st_sub = sub_tail_pitch(mono, sr, onset, end)
    def _model_vs_sub(k_, s_):
        # model (przejscia przez zero) w TYM SAMYM oknie co pomiar subu -> zgodny? (dziala tez gdy ogon wciaz zjezdza)
        tw = np.linspace(s_["window_ms"][0], s_["window_ms"][1], 200) / 1000
        return abs(1200 * math.log2(float(np.median(f_model(k_["model"], k_["params"], tw))) / s_["f0_hz"]))
    zc_ok = bool(kp and kp["drop_semitones"] >= 12 and kp["f_end_model"] < 130 and kp["fits"][kp["model"]]["rms_cycles"] < 0.5
                 and (st_sub is None or _model_vs_sub(kp, st_sub) < 60))
    sub_kickish = bool(st_sub and st_sub["voiced_pct"] > 50 and st_sub["spread_cents"] < 120 and 25 < st_sub["f0_hz"] < 125
                       and res["envelope"]["attack_10_90_ms"] < 10 and res["timing"]["sound_length_ms"] < 1500)
    tonal_t = tonal_f = None
    if stype == "auto":
        stype = "kick" if (zc_ok or sub_kickish) else "tonal"
    if stype == "kick" and not zc_ok and st_sub:
        kp_y = kick_pitch_yin(mono, sr, onset, end, st_sub["f0_hz"])
        if kp_y:
            kp = kp_y
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
        if st_sub:
            pitch["tail_sub"] = st_sub
        pitch["curve_ms"] = {str(tp_): float(f_model(kp["model"], kp["params"], tp_ / 1000))
                             for tp_ in TIME_POINTS_MS if tp_ <= res["timing"]["sound_length_ms"]}
        if st_sub and (not tun or kp.get("source")):  # model zweryfikowany wzgl. subu -> zostaje pomiar analityczny (dokladniejszy przy zjezdzie)
            tun = {"f0": st_sub["f0_hz"], "drift_cents": st_sub["drift_cents"], "window_ms": st_sub["window_ms"], "src": "pasmo subu (LP 130 Hz, YIN 190 ms)"}
        if tun:
            pitch["tail"] = {"f0_hz": tun["f0"], "note": hz_to_note(tun["f0"]),
                             "still_gliding": kp["tail_still_gliding"] and not tun.get("src"),
                             "drift_cents": tun["drift_cents"], "window_ms": tun["window_ms"],
                             "source": tun.get("src", "sygnal analityczny podstawowej")}
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
        if np.isfinite(tonal_f).sum() > 3 and np.nanmedian(tonal_f) < 3.0 * sr / frame:  # niski ton: okno za krotkie (min ~2 okresy)
            frame *= 2
            tonal_t, tonal_f, conf = yin_track(mono[onset:end], sr, fmin=25, frame=frame, hop=hop)
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

    # --- clipping / stereo / transjent vs ton / ostrosc
    res["clipping"] = clipping_analysis(x, active)
    res["stereo"] = stereo_analysis(x, sr, active)
    res["percussive"] = hpss_share(mono[active], sr)
    res["sharpness"] = sharpness_approx(mono[active], sr)
    yin_chk = None
    if stype == "kick" and kp:
        yin_chk = yin_multires_check(mono, sr, onset, end, kp["model"], kp["params"])
        if yin_chk:
            res["pitch"]["yin_check"] = {k: v for k, v in yin_chk.items() if not k.startswith("_")}

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
                    "tonal": (tonal_t, tonal_f), "yin_chk": yin_chk}
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


# ----------------------------------------------------------------------------- profil referencji (kalibracja na kickach wlasciciela)

# (klucz, opis, jednostka, czy porownywac w skali log (Hz))
METRICS = [
    ("true_peak_dbtp", "true peak", "dBTP", False),
    ("max_momentary_lufs", "max momentary", "LUFS", False),
    ("hit_lufs", "hit loudness", "LUFS", False),
    ("loop_lufs", "petla integrated", "LUFS", False),
    ("crest_db", "crest calosc", "dB", False),
    ("crest_head15_db", "crest 0-15 ms", "dB", False),
    ("head15_vs_body_db", "RMS 0-15 ms vs reszta", "dB", False),
    ("attack_ms", "atak 10-90%", "ms", False),
    ("decay6_ms", "zanik do -6 dB", "ms", False),
    ("decay20_ms", "zanik do -20 dB", "ms", False),
    ("sound_ms", "dlugosc dzwieku", "ms", False),
    ("temporal_centroid_ms", "centroid czasowy", "ms", False),
    ("f_start_hz", "pitch start (model)", "Hz", True),
    ("pitch5_hz", "pitch @5 ms", "Hz", True),
    ("pitch10_hz", "pitch @10 ms", "Hz", True),
    ("pitch20_hz", "pitch @20 ms", "Hz", True),
    ("pitch50_hz", "pitch @50 ms", "Hz", True),
    ("pitch100_hz", "pitch @100 ms", "Hz", True),
    ("t_half_ms", "polowa zjazdu", "ms", False),
    ("settle1_ms", "zjazd do 1 poltonu", "ms", False),
    ("tail_hz", "ogon", "Hz", True),
    ("drop_st", "zjazd", "polt.", False),
    ("thd_pct", "THD ogona", "%", False),
    ("even_odd_db", "harm. parzyste/nieparzyste", "dB", False),
    ("inharmonic_db", "energia nieharmoniczna", "dB", False),
    ("share_SUB", "udzial SUB", "%", False),
    ("share_LOW", "udzial LOW", "%", False),
    ("share_LOWMID", "udzial LOWMID", "%", False),
    ("share_CRUNCH", "udzial CRUNCH", "%", False),
    ("share_PRESENCE", "udzial PRESENCE", "%", False),
    ("share_AIR", "udzial AIR", "%", False),
    ("centroid_hz", "centroid widma", "Hz", True),
    ("flatness", "plaskosc widma", "", False),
    ("percussive_pct", "udzial perkusyjny", "%", False),
    ("percussive_head_pct", "perkusyjny 0-30 ms", "%", False),
    ("sharpness_acum", "ostrosc (mediana)", "acum", False),
    ("sharpness_max_acum", "ostrosc (max)", "acum", False),
    ("corr_sub", "korelacja <120 Hz", "", False),
    ("side_mid_db", "S/M calosc", "dB", False),
]


def flat_metrics(res):
    g = lambda *ks: _dig(res, ks)
    m = {
        "true_peak_dbtp": g("metering", "true_peak_dbtp"),
        "max_momentary_lufs": g("metering", "max_momentary_lufs"),
        "hit_lufs": g("metering", "hit_loudness_lufs_nonstd"),
        "loop_lufs": g("loop", "integrated_lufs"),
        "crest_db": g("envelope", "crest_db"),
        "crest_head15_db": g("envelope", "crest_head15_db"),
        "head15_vs_body_db": g("envelope", "head15_vs_body_rms_db"),
        "attack_ms": g("envelope", "attack_10_90_ms"),
        "decay6_ms": g("envelope", "decay_ms_from_slow_peak", "-6"),
        "decay20_ms": g("envelope", "decay_ms_from_slow_peak", "-20"),
        "sound_ms": g("timing", "sound_length_ms"),
        "temporal_centroid_ms": g("envelope", "temporal_centroid_ms"),
        "f_start_hz": g("pitch", "f_start_model"),
        "t_half_ms": g("pitch", "t_half_ms"),
        "settle1_ms": g("pitch", "settle_1st_ms"),
        "tail_hz": g("pitch", "tail", "f0_hz") or g("pitch", "median_f0_hz"),
        "drop_st": g("pitch", "drop_semitones"),
        "thd_pct": g("harmonics", "thd_pct"),
        "even_odd_db": g("harmonics", "even_odd_db"),
        "inharmonic_db": g("harmonics", "inharmonic_db"),
        "centroid_hz": g("spectrum", "descriptors", "centroid_hz", "median"),
        "flatness": g("spectrum", "descriptors", "flatness", "median"),
        "percussive_pct": g("percussive", "percussive_pct"),
        "percussive_head_pct": g("percussive", "percussive_pct_0_30ms"),
        "sharpness_acum": g("sharpness", "sharpness_acum_median"),
        "sharpness_max_acum": g("sharpness", "sharpness_acum_max"),
        "corr_sub": (res.get("stereo") or {}).get("bands", [{}])[0].get("correlation") if (res.get("stereo") or {}).get("bands") else None,
        "side_mid_db": g("stereo", "side_to_mid_db"),
    }
    for tp in ("5", "10", "20", "50", "100"):
        m[f"pitch{tp}_hz"] = g("pitch", "curve_ms", tp) if res.get("type") == "kick" else None
    for name, _, _ in BANDS:
        m[f"share_{name}"] = g("bands", "share_pct", name)
    return {k: (float(v) if v is not None and np.isfinite(v) else None) for k, v in m.items()}


def _dig(d, ks):
    for k in ks:
        if not isinstance(d, dict) or k not in d or d[k] is None:
            return None
        d = d[k]
    return d


def build_profile(rows, names):
    prof = {"n": len(rows), "files": names, "metrics": {}}
    for key, label, unit, logscale in METRICS:
        v = np.array([r[key] for r in rows if r.get(key) is not None], dtype=float)
        if v.size == 0:
            continue
        w = np.log2(v) if logscale and (v > 0).all() else v
        q = lambda p: float(2 ** np.percentile(w, p)) if logscale and (v > 0).all() else float(np.percentile(w, p))
        prof["metrics"][key] = {"label": label, "unit": unit, "log": logscale, "n": int(v.size),
                                "p10": q(10), "median": q(50), "p90": q(90), "min": float(v.min()), "max": float(v.max())}
    return prof


def against_profile(flat, prof):
    """Gdzie sample wypada na tle referencji: ponizej p10 / w zakresie / powyzej p90 + odleglosc w szerokosciach IQR-like."""
    out = []
    for key, label, unit, logscale in METRICS:
        if key == "tail_hz":
            continue  # nuta ogona zalezy od tonacji tracku, nie od jakosci kicka
        pm = prof["metrics"].get(key)
        v = flat.get(key)
        if not pm or v is None:
            continue
        if logscale and v > 0:
            tr = lambda a: math.log2(a) * 1200  # centy
        else:
            tr = lambda a: a
        lo, med, hi = tr(pm["p10"]), tr(pm["median"]), tr(pm["p90"])
        # minimalna tolerancja (rozrzut referencji bywa ~0): w jednostkach metryki
        min_sp = {"dB": 1.0, "dBTP": 1.0, "LUFS": 1.0, "%": 2.0, "acum": 0.1, "polt.": 1.0, "": 0.05}.get(unit, 0)
        if logscale:
            min_sp = 50.0  # centy
        elif unit == "ms":
            min_sp = max(5.0, 0.1 * abs(pm["median"]))
        lo, hi = min(lo, med - min_sp / 2), max(hi, med + min_sp / 2)
        spread = max(hi - lo, 1e-9)
        z = (tr(v) - med) / (spread / 2)
        pos = "PONIZEJ" if tr(v) < lo else ("POWYZEJ" if tr(v) > hi else "w zakresie")
        out.append({"key": key, "label": label, "unit": unit, "value": v, "p10": pm["p10"], "median": pm["median"],
                    "p90": pm["p90"], "pos": pos, "dist": float(z),
                    "delta_vs_median": (1200 * math.log2(v / pm["median"]) if logscale and v > 0 else v - pm["median"]),
                    "delta_unit": "c" if logscale else unit})
    return out


def profile_md(ap, prof):
    L = [f"\n## Na tle referencji ({prof['n']} plikow: {', '.join(prof['files'][:6])}{' …' if len(prof['files']) > 6 else ''})",
         "Pozycja: p10–p90 referencji (min. tolerancja: 1 dB, 50 c, 2 pp, 5 ms / 10%) = 'w zakresie'. Odleglosc: 0 = mediana, ±1 = granica. "
         "Najpierw najwieksze odchylenia. Nuta ogona pominieta (zalezy od tonacji).",
         "| metryka | ten sample | referencje p10 / mediana / p90 | Δ vs mediana | pozycja |", "|---|---|---|---|---|"]
    for a in sorted(ap, key=lambda a: -abs(a["dist"])):
        n = 2 if a["unit"] in ("", "acum") else (0 if a["unit"] in ("Hz", "ms") else 1)
        mark = " **!**" if abs(a["dist"]) > 1.5 else ""
        L.append(f"| {a['label']} | {a['value']:.{n}f} {a['unit']} | {a['p10']:.{n}f} / {a['median']:.{n}f} / {a['p90']:.{n}f} | "
                 f"{a['delta_vs_median']:+.{max(n, 0)}f} {a['delta_unit']} | {a['pos']} ({a['dist']:+.1f}){mark} |")
    return "\n".join(L) + "\n"


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
        if p.get("source"):
            L.append(f"**krzywa z zapasowej metody: {p['source']}** – mniej dokladna w pierwszych ~10 ms; model exp dopasowany do YIN (rms {p['fits']['exp(YIN)']['rms_cents']:.0f} c)")
        else:
            L.append(f"model zjazdu: **{p['model']}** · parametry {[round(v, 5) for v in p['params']]}")
            L.append("dopasowania (rms w cyklach / AIC): " + ", ".join(f"{k}: {v['rms_cycles']} / {v['aic']}" for k, v in p["fits"].items()))
        L.append(f"start (model, t=0): {fmt(p['f_start_model'], 0)} Hz ({note_str(p['f_start_model'])}) · pierwszy pelny cykl: {fmt(p['f_first_cycle'], 0)} Hz")
        L.append(f"koniec (model): {fmt(p['f_end_model'], 2)} Hz ({note_str(p['f_end_model'])}) · zjazd {fmt(p['drop_semitones'], 1)} polt.")
        L.append(f"polowa zjazdu @ {fmt(p['t_half_ms'], 1)} ms · do 1 poltonu od ogona @ {fmt(p['settle_1st_ms'], 1)} ms · do 3 centow @ {fmt(p.get('settle_3c_ms'), 1)} ms · do ogon+5 Hz @ {fmt(p['t_to_fend_plus5_ms'], 1)} ms")
        if p.get("tail"):
            t = p["tail"]
            L.append(f"**ogon: {t['f0_hz']:.2f} Hz = {t['note']['note']} {t['note']['cents']:+.1f} c** ({t.get('source', '')}; okno {fmt(t['window_ms'][0], 0)}-{fmt(t['window_ms'][1], 0)} ms, dryf {t['drift_cents']:+.1f} c"
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
    ts_ = p.get("tail_sub")
    if ts_:
        L.append(f"ogon w pasmie subu: {ts_['f0_hz']:.2f} Hz = {note_str(ts_['f0_hz'])} · rozrzut p10-p90 {ts_['spread_cents']:.0f} c · tercje {', '.join(f'{v:.2f}' for v in ts_['thirds_hz'])} Hz")
    yc = p.get("yin_check")
    if yc:
        L.append(f"kontrola niezalezna (YIN 5 rozdzielczosci, {yc['frames']} ramek): odchylka od modelu mediana {fmt(yc['median_abs_cents'], 0)} c, p90 {fmt(yc['p90_abs_cents'], 0)} c, bledy oktawowe {yc['octave_error_pct']:.0f}%"
                 + (" – ZGODNE" if (yc['median_abs_cents'] or 999) < 30 else " – ROZBIEZNE: krzywa pitchu moze byc niepewna (warstwy / szum)"))
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
    pc_ = res.get("percussive")
    sh = res.get("sharpness")
    L.append("\n## Transjent vs ton, ostrosc")
    if pc_:
        L.append(f"udzial perkusyjny (HPSS): calosc {pc_['percussive_pct']:.0f}% · 0-30 ms {pc_['percussive_pct_0_30ms']:.0f}% · po 30 ms {fmt(pc_['percussive_pct_after_30ms'], 0)}% "
                 "(przy kicku szybki zjazd pitchu tez liczy sie jako 'perkusyjny')")
    if sh:
        L.append(f"ostrosc (przybl. DIN 45692, 0 dBFS = 94 dB SPL): mediana {sh['sharpness_acum_median']:.2f} acum, max {sh['sharpness_acum_max']:.2f} acum "
                 "(1 acum = waskopasmowy szum 1 kHz; wiecej = jasniej / ostrzej)")
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

    # ---- 4: punch pod lupa (spektrogram reasygnowany 0-80 ms + kontrola pitchu)
    zoom = min(80.0, max(20.0, L_ms))
    seg = mono[on: on + int(sr * (zoom + 15) / 1000)]
    fig, axs = plt.subplots(2, 1, figsize=(14, 9))
    tb = np.linspace(0, zoom / 1000, 321)
    fb = np.geomspace(20, min(20000, sr / 2), 241)
    _, _, H = reassigned_spectrogram(seg, sr, t_bins=tb, f_bins=fb)
    Hd = pdb(H / (H.max() + EPS))
    mesh = axs[0].pcolormesh(tb * 1000, fb, Hd.T, vmin=-80, vmax=0, cmap="magma", shading="flat", rasterized=True)
    plt.colorbar(mesh, ax=axs[0], pad=0.01).set_label("dB wzgl. max")
    _freq_axis(axs[0], 20, min(20000, sr / 2), axis="y")
    axs[0].set_xlim(0, zoom)
    axs[0].set_xticks(np.arange(0, zoom + 0.1, 5))
    axs[0].set_ylabel("Hz")
    axs[0].set_title(f"Image 4 – punch pod lupa: spektrogram reasygnowany 0–{zoom:.0f} ms (ostre linie = dokladne czestotliwosci)")
    if res["type"] == "kick" and kp:
        tt = np.linspace(0, zoom / 1000, 300)
        axs[0].plot(tt * 1000, f_model(kp["model"], kp["params"], tt), color="cyan", ls="--", lw=1.2, label="model F0")
        axs[0].legend(loc="upper right")
        tmx = min(L_ms, 200)
        tt2 = np.linspace(0, tmx / 1000, 400)
        axs[1].plot(tt2 * 1000, f_model(kp["model"], kp["params"], tt2), color="tab:red", lw=2.5, label=f"model {kp['model']} (przejscia przez zero)")
        axs[1].plot(kp["zc_t"] * 1000, kp["zc_f"], ".", color="0.45", ms=4, label="pojedyncze okresy")
        yc = P.get("yin_chk")
        if yc:
            axs[1].plot(yc["_t"] * 1000, yc["_f"], "x", color="tab:green", ms=4, label="YIN 5 rozdzielczosci (kontrola)")
        _freq_axis(axs[1], max(20, kp["f_end_model"] / 1.5), min(sr / 2, max(kp["f_start_model"], np.max(kp["zc_f"][:5])) * 1.5), axis="y")
        axs[1].set_xlim(0, tmx)
        axs[1].legend(loc="upper right")
        pyc = res["pitch"].get("yin_check")
        axs[1].set_title("Pitch: model vs niezalezna kontrola" + (f" (mediana odchylki {pyc['median_abs_cents']:.0f} c)" if pyc and pyc.get("median_abs_cents") is not None else ""))
    else:
        tz = np.arange(len(seg)) / sr * 1000
        axs[1].plot(tz, seg, color="k", lw=1)
        axs[1].set_xlim(0, zoom)
        axs[1].set_title("Przebieg")
    axs[1].set_xlabel("ms")
    fig.tight_layout()
    fp = os.path.join(out, "4_punch_lupa.png")
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
    if resA.get("percussive") and resB.get("percussive"):
        add("udzial perkusyjny %", resA["percussive"]["percussive_pct"], resB["percussive"]["percussive_pct"], " pp", 0)
        add("perkusyjny 0-30 ms %", resA["percussive"]["percussive_pct_0_30ms"], resB["percussive"]["percussive_pct_0_30ms"], " pp", 0)
    if resA.get("sharpness") and resB.get("sharpness"):
        add("ostrosc mediana acum", resA["sharpness"]["sharpness_acum_median"], resB["sharpness"]["sharpness_acum_median"], "", 2)
        add("ostrosc max acum", resA["sharpness"]["sharpness_acum_max"], resB["sharpness"]["sharpness_acum_max"], "", 2)
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


# ----------------------------------------------------------------------------- testy wtyczek offline (jak Plugin Doctor)

STEP_LEVELS = list(range(-36, 1, 3))


def test_layout(sr):
    """Deterministyczny sygnal testowy: lista segmentow (nazwa, start, dlugosc, parametry) + sygnal mono."""
    segs = []
    parts = []
    pos = [int(0.5 * sr)]
    parts.append(np.zeros(pos[0]))

    def add(name, y, gap=1.0, **kw):
        segs.append({"name": name, "start": pos[0], "len": len(y), **kw})
        parts.append(y)
        g = np.zeros(int(gap * sr))
        parts.append(g)
        pos[0] += len(y) + len(g)

    def fade(y, ms=5):
        n = int(sr * ms / 1000)
        w = np.ones(len(y))
        r = 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, n))
        w[:n] = r
        w[-n:] = r[::-1]
        return y * w

    imp = np.zeros(int(0.01 * sr))
    imp[0] = 0.5
    add("impulse", imp, gap=1.0, amp=0.5)
    T = 8.0
    f1, f2 = 20.0, min(20000.0, 0.45 * sr)
    t = np.arange(int(T * sr)) / sr
    R = math.log(f2 / f1)
    sw = 0.5 * np.sin(2 * np.pi * f1 * T / R * (np.exp(t * R / T) - 1))
    add("sweep", fade(sw, 2), gap=1.5, f1=f1, f2=f2, T=T, amp=0.5)
    for f in (100.0, 1000.0):
        for lv in STEP_LEVELS:
            tt = np.arange(int(0.4 * sr)) / sr
            add(f"step_{int(f)}_{lv}", fade(10 ** (lv / 20) * np.sin(2 * np.pi * f * tt)), gap=0.15, freq=f, level=lv)
    for f in (5000.0, 7000.0, 11000.0):
        tt = np.arange(int(0.5 * sr)) / sr
        add(f"hf_{int(f)}", fade(0.5 * np.sin(2 * np.pi * f * tt)), gap=0.3, freq=f, level=-6)
    T2 = 3.0
    t = np.arange(int(T2 * sr)) / sr
    R2 = math.log(f2 / 5000)
    add("hf_sweep", fade(0.5 * np.sin(2 * np.pi * 5000 * T2 / R2 * (np.exp(t * R2 / T2) - 1))), gap=1.0, f1=5000, f2=f2, T=T2)
    t = np.arange(int(1.0 * sr)) / sr
    add("imd", fade(0.4 * np.sin(2 * np.pi * 60 * t) + 0.1 * np.sin(2 * np.pi * 7000 * t)), gap=1.0)
    t = np.arange(int(6.0 * sr)) / sr
    lvl = -60 + 60 * t / 6.0
    add("ramp", fade(10 ** (lvl / 20) * np.sin(2 * np.pi * 1000 * t)), gap=1.0, f=1000, lo=-60, hi=0)
    t = np.arange(int(3.5 * sr)) / sr
    env = np.where(t < 1.0, -30, np.where(t < 2.0, -10, -30))
    add("bursts", fade(10 ** (env / 20) * np.sin(2 * np.pi * 1000 * t)), gap=1.0, t_up=1.0, t_down=2.0)
    rng = np.random.default_rng(12345)
    wn = rng.standard_normal(int(4.0 * sr))
    pk = np.fft.rfft(wn)
    fr = np.fft.rfftfreq(len(wn), 1 / sr)
    pk[1:] /= np.sqrt(fr[1:])
    pk[0] = 0
    pn = np.fft.irfft(pk, len(wn))
    pn *= 10 ** (-18 / 20) / np.std(pn)
    add("pink", fade(pn, 20), gap=1.5)
    return segs, np.concatenate(parts)


def gen_tests(out, sr):
    os.makedirs(out, exist_ok=True)
    segs, y = test_layout(sr)
    stereo = np.stack([y, y], 1).astype(np.float32)
    name = f"doomscope_test_{sr}.wav"
    wavfile.write(os.path.join(out, name), sr, stereo)
    json.dump({"version": VERSION, "sr": sr, "length_s": len(y) / sr, "segments": segs}, open(os.path.join(out, "manifest.json"), "w"), indent=1)
    txt = f"""DOOMSCOPE – test wtyczki ({len(y) / sr:.0f} s, {sr} Hz, float 32-bit stereo)

1. FL Studio: projekt w {sr} Hz. Wrzuc {name} na Playlist DOKLADNIE od poczatku taktu 1 (bez przesuniecia).
2. Kanal miksera z tym klipem: SAMA testowana wtyczka (efekt), nic wiecej. Master bez limitera / efektow.
3. Export -> WAV, 32-bit float (albo 24-bit), ta sama czestotliwosc, "Leave remainder" / tail = cut albo wait – obojetne.
4. Wyslij wynik (np. moja_wtyczka_out.wav). Analiza: python doomscope.py plugin-test moja_wtyczka_out.wav
   (latencja wtyczki jest wykrywana z impulsu; PDC w FL moze ja juz skompensowac – oba przypadki dzialaja)
Mozna testowac rozne ustawienia (np. DRIVE min / max) – osobny eksport na kazde ustawienie.
Tylko dla EFEKTOW (EQ, przester, kompresor, reverb...). Instrumentow (4GE, RE4GE) tak nie testujemy.
"""
    open(os.path.join(out, "JAK_TESTOWAC.txt"), "w").write(txt)
    print(txt)
    print(f"-> {out}/{name}, manifest.json, JAK_TESTOWAC.txt")


def _bh_spectrum(seg, sr):
    w = signal.windows.blackmanharris(len(seg))
    X = np.fft.rfft(seg * w)
    P = (np.abs(X) ** 2) * 2 / (len(seg) * np.sum(w ** 2))  # Parseval: suma binow skladowej = A^2/2
    f = np.fft.rfftfreq(len(seg), 1 / sr)
    return f, P


def tone_metrics(seg, sr, f0, n_h=20):
    f, P = _bh_spectrum(seg, sr)
    df = f[1]
    bw = 4  # biny wokol skladowej (okno Blackman-Harris ma glowny listek +-4 biny)
    used = np.zeros(P.size, bool)
    used[: int(10 / df) + bw] = True  # DC / infrasound
    harm = []
    for h in range(1, n_h + 1):
        fh = h * f0
        if fh >= sr / 2 - 2 * df * bw:
            break
        k = int(round(fh / df))
        lo, hi = max(0, k - bw), min(P.size, k + bw + 1)
        harm.append(P[lo:hi].sum())
        used[lo:hi] = True
    harm = np.array(harm)
    rest = P[~used].sum()
    spur = np.where(~used)[0]
    top = []
    if spur.size:
        pk, _ = signal.find_peaks(np.where(used, 0, P))
        for i in pk[np.argsort(P[pk])[::-1][:5]]:
            top.append({"hz": float(f[i]), "db_rel_fund": float(pdb(P[i] / (harm[0] + EPS)))})
    return {"fund_dbfs": float(pdb(harm[0] * 2)), "harm_db_rel": [float(pdb(h / (harm[0] + EPS))) for h in harm[1:10]],
            "thd_pct": float(100 * math.sqrt(harm[1:].sum() / (harm[0] + EPS))),
            "even_odd_db": float(pdb(harm[1::2].sum() / (harm[2::2].sum() + EPS))) if harm.size > 2 else None,
            "noise_alias_db_rel": float(pdb(rest / (harm[0] + EPS))), "spurs": top}


def plugin_test(path, out):
    plt = _plt()
    y, sr, info = load_audio(path)
    segs, x = test_layout(sr)
    ych = y
    y = y.mean(axis=1)
    os.makedirs(out, exist_ok=True)
    rep = {"file": info["file"], "sr": sr}
    # latencja z impulsu
    s0 = segs[0]["start"]
    win = y[max(0, s0 - int(0.2 * sr)): s0 + int(0.5 * sr)]
    i_pk = int(np.argmax(np.abs(win)))
    lat = i_pk - (s0 - max(0, s0 - int(0.2 * sr)))
    rep["latency_samples"] = lat
    rep["latency_ms"] = 1000 * lat / sr
    rep["polarity"] = "odwrocona" if win[i_pk] < 0 else "normalna"
    if len(y) < len(x) + lat:
        y = np.pad(y, (0, len(x) + lat - len(y) + 1))
    ya = y[lat: lat + len(x)] if lat >= 0 else np.pad(y, (-lat, 0))[: len(x)]
    S = {s["name"]: s for s in segs}
    seg = lambda n, a=0.0, b=None: (x[S[n]["start"] + int(a * sr): S[n]["start"] + (int(b * sr) if b is not None else S[n]["len"])],
                                    ya[S[n]["start"] + int(a * sr): S[n]["start"] + (int(b * sr) if b is not None else S[n]["len"])])
    # odpowiedz liniowa ze sweepu (dekonwolucja Farina przez dzielenie widm), kalibrowana na sygnale wejsciowym
    sw = S["sweep"]
    a0, a1 = sw["start"], sw["start"] + sw["len"] + int(1.5 * sr)
    xin, yout = x[a0:a1], ya[a0:a1]
    n = 1 << int(math.ceil(math.log2(len(xin) * 2)))
    X = np.fft.rfft(xin, n)
    fr = np.fft.rfftfreq(n, 1 / sr)
    # miekkie krawedzie pasma (1/2 okt. cosinus) -> zwarta odpowiedz impulsowa, bez dzwonienia
    taper = np.zeros_like(fr)
    lo_a, lo_b = sw["f1"], sw["f1"] * 2 ** 0.5
    hi_b, hi_a = sw["f2"] / 2 ** 0.5, sw["f2"]
    mid = (fr >= lo_b) & (fr <= hi_b)
    taper[mid] = 1
    m1 = (fr > lo_a) & (fr < lo_b)
    taper[m1] = 0.5 - 0.5 * np.cos(np.pi * np.log(fr[m1] / lo_a) / np.log(lo_b / lo_a))
    m2 = (fr > hi_b) & (fr < hi_a)
    taper[m2] = 0.5 + 0.5 * np.cos(np.pi * np.log(fr[m2] / hi_b) / np.log(hi_a / hi_b))
    Xinv = np.conj(X) / (np.abs(X) ** 2 + 1e-10 * np.max(np.abs(X) ** 2)) * taper
    pre = int(0.005 * sr)
    post = int(0.3 * sr)
    tl = int(0.05 * sr)

    def lin_resp(sig):
        ir_ = np.fft.irfft(np.fft.rfft(sig, n) * Xinv, n)
        lin_ = np.concatenate([ir_[-pre:], ir_[:post]])
        neg_ = ir_[-int(sw["T"] * sr * 0.9): -pre]
        w_ = np.ones(post)
        w_[-tl:] = np.hanning(2 * tl)[tl:]
        buf = np.zeros(1 << 16)
        buf[:post] = ir_[:post] * w_
        buf[-pre:] = ir_[-pre:] * np.hanning(2 * pre)[:pre]  # ujemny czas na KONCU bufora (kolowo)
        H_ = np.fft.rfft(buf)
        return H_, float(np.sum(neg_ ** 2) / (np.sum(lin_ ** 2) + EPS))
    Hy, neg_y = lin_resp(yout)
    Hx, neg_x = lin_resp(xin)
    fl = np.fft.rfftfreq(1 << 16, 1 / sr)
    msk = (fl >= 25) & (fl <= sw["f2"] / 1.5)
    Hl = np.where(msk, Hy / (Hx + EPS), 0)
    rep["sweep_distortion_energy_db"] = float(pdb(max(neg_y - neg_x, 1e-12)))
    mag_s = pdb(frac_octave_smooth(fl[msk], np.abs(Hl[msk]) ** 2, 12))
    fm = fl[msk]
    fgrid = np.geomspace(fm[0], fm[-1], 600)
    Hc = Hl[msk]
    re_s = np.interp(fgrid, fm, frac_octave_smooth(fm, Hc.real, 24))
    im_s = np.interp(fgrid, fm, frac_octave_smooth(fm, Hc.imag, 24))
    ph_g = np.unwrap(np.angle(re_s + 1j * im_s))
    gd_g = -np.gradient(ph_g, 2 * np.pi * fgrid)
    phase = np.interp(fm, fgrid, ph_g)
    pts = [25, 30, 50, 80, 100, 150, 200, 300, 500, 800, 1000, 2000, 3000, 5000, 8000, 10000, 12000]
    rep["freq_response_db"] = {str(p): float(np.interp(p, fm, mag_s)) for p in pts if fm[0] <= p <= fm[-1]}
    rep["group_delay_ms"] = {str(p): float(1000 * np.interp(p, fgrid, gd_g)) for p in (50, 100, 1000, 10000) if fm[0] <= p <= fm[-1]}
    # stopnie sinusow
    steps = {}
    for f in (100, 1000):
        rows = []
        for lv in STEP_LEVELS:
            xi, yo = seg(f"step_{f}_{lv}", 0.1, 0.35)
            tm = tone_metrics(yo, sr, f)
            tm["in_dbfs"] = lv
            tm["gain_db"] = tm["fund_dbfs"] - lv
            rows.append(tm)
        steps[str(f)] = rows
    rep["steps"] = steps
    g0 = steps["1000"][0]["gain_db"]
    sat = next((r["in_dbfs"] for r in steps["1000"] if r["thd_pct"] > 1.0), None)
    comp = next((r["in_dbfs"] for r in steps["1000"] if r["gain_db"] < g0 - 1.0), None)
    rep["summary_levels"] = {"gain_low_level_db": g0, "thd_over_1pct_from_dbfs": sat, "gain_drop_1db_from_dbfs": comp}
    # aliasing
    hf = {}
    for f in (5000, 7000, 11000):
        _, yo = seg(f"hf_{f}", 0.1, 0.45)
        hf[str(f)] = tone_metrics(yo, sr, f)
    rep["aliasing"] = hf
    # IMD SMPTE
    _, yo = seg("imd", 0.1, 0.9)
    fI, PI = _bh_spectrum(yo, sr)
    df = fI[1]

    def pw(fq):
        k = int(round(fq / df))
        return PI[max(0, k - 4): k + 5].sum()
    side = sum(pw(7000 + s * k * 60) for k in (1, 2, 3) for s in (-1, 1))
    rep["imd_smpte_pct"] = float(100 * math.sqrt(side / (pw(7000) + EPS)))
    # ramp -> krzywa statyczna
    xi, yo = seg("ramp")
    hop = int(0.02 * sr)
    nfr = len(xi) // hop
    lv_in = -60 + 60 * (np.arange(nfr) + 0.5) * hop / len(xi)
    rin = np.array([math.sqrt(np.mean(xi[i * hop:(i + 1) * hop] ** 2)) for i in range(nfr)])
    rout = np.array([math.sqrt(np.mean(yo[i * hop:(i + 1) * hop] ** 2)) for i in range(nfr)])
    curve_gain = db(rout / (rin + EPS))
    rep["static_curve"] = {str(l): float(np.interp(l, lv_in, db(rout) + 3.01)) for l in range(-60, 1, 6)}
    # bursty -> atak / release
    xi, yo = seg("bursts")
    e_in = rms_env(xi, sr, 2)
    e_out = rms_env(yo, sr, 2)
    gdb = db(e_out / (e_in + EPS))
    tb = np.arange(len(gdb)) / sr
    up, dn = int(1.0 * sr), int(2.0 * sr)
    g_hi = np.median(gdb[dn - int(0.3 * sr): dn - int(0.02 * sr)])
    g_lo = np.median(gdb[up - int(0.3 * sr): up - int(0.02 * sr)])
    g_lo2 = np.median(gdb[-int(0.4 * sr): -int(0.05 * sr)])

    def settle(start, stop, target):
        seg_ = np.abs(gdb[start:stop] - target) > 1.0
        idx = np.flatnonzero(seg_)
        return 1000 * (idx[-1] + 1) / sr if idx.size else 0.0
    rep["dynamics"] = {"gain_at_-30_db": float(g_lo), "gain_at_-10_db": float(g_hi), "gain_reduction_db": float(g_lo - g_hi),
                       "attack_ms_to_1db": settle(up + int(0.003 * sr), dn - int(0.05 * sr), g_hi) if abs(g_lo - g_hi) > 1.5 else None,
                       "release_ms_to_1db": settle(dn + int(0.003 * sr), len(gdb) - int(0.05 * sr), g_lo2) if abs(g_lo - g_hi) > 1.5 else None}
    # szum rozowy: transfer Welch + koherencja
    xi, yo = seg("pink", 0.1, 3.9)
    nper = 1 << int(round(math.log2(0.1 * sr)))
    fw, Pxy = signal.csd(xi, yo, sr, nperseg=nper)
    _, Pxx = signal.welch(xi, sr, nperseg=nper)
    _, Coh = signal.coherence(xi, yo, sr, nperseg=nper)
    Hn = db(np.abs(Pxy) / (Pxx + EPS))
    mm = (fw >= 50) & (fw <= min(16000, sr / 2 * 0.9))
    rep["noise_coherence_median"] = float(np.median(Coh[mm]))
    rep["noise_coherence_min_band"] = float(fw[mm][np.argmin(Coh[mm])])
    # stereo wyjscia
    if ych.shape[1] == 2:
        Ls, Rs = ych[:, 0], ych[:, 1]
        rep["output_side_to_mid_db"] = float(pdb(np.sum((Ls - Rs) ** 2) / (np.sum((Ls + Rs) ** 2) + EPS)))

    # ---- raport
    L = [f"# DOOMSCOPE – test wtyczki: {info['file']}", f"{sr} Hz · latencja {lat} probek ({rep['latency_ms']:.2f} ms) · polaryzacja {rep['polarity']}"
         + (f" · stereo S/M {rep['output_side_to_mid_db']:.1f} dB" if "output_side_to_mid_db" in rep else "")]
    sl = rep["summary_levels"]
    L.append("\n## Najwazniejsze")
    L.append(f"- wzmocnienie przy cichym sygnale (1 kHz, -36 dBFS): {sl['gain_low_level_db']:+.2f} dB")
    L.append(f"- przester (THD > 1%) zaczyna sie od wejscia: {fmt(sl['thd_over_1pct_from_dbfs'], 0)} dBFS · spadek wzmocnienia o 1 dB od: {fmt(sl['gain_drop_1db_from_dbfs'], 0)} dBFS")
    L.append(f"- aliasing+szum (ton 11 kHz, -6 dBFS): {hf['11000']['noise_alias_db_rel']:.1f} dB wzgl. tonu · 5 kHz: {hf['5000']['noise_alias_db_rel']:.1f} dB · IMD SMPTE {rep['imd_smpte_pct']:.2f}%")
    d_ = rep["dynamics"]
    L.append(f"- dynamika: wzmocnienie przy -30 dBFS {d_['gain_at_-30_db']:+.1f} dB, przy -10 dBFS {d_['gain_at_-10_db']:+.1f} dB (redukcja {d_['gain_reduction_db']:.1f} dB) · "
             f"atak {fmt(d_['attack_ms_to_1db'], 1)} ms · release {fmt(d_['release_ms_to_1db'], 0)} ms (do 1 dB od wartosci koncowej)")
    L.append(f"- liniowosc / stalosc w czasie: koherencja szumu mediana {rep['noise_coherence_median']:.3f} (1 = liniowa i stala; nizej = przester / modulacja / reverb)"
             f" · energia znieksztalcen w sweepie {rep['sweep_distortion_energy_db']:.1f} dB wzgl. liniowej")
    L.append("\n## Charakterystyka czestotliwosciowa (sweep, wygladzona 1/12 okt., dB)")
    L.append("| Hz | " + " | ".join(rep["freq_response_db"].keys()) + " |")
    L.append("|---|" + "---|" * len(rep["freq_response_db"]))
    L.append("| dB | " + " | ".join(f"{v:+.1f}" for v in rep["freq_response_db"].values()) + " |")
    L.append("opoznienie grupowe [ms]: " + ", ".join(f"{k} Hz: {v:.2f}" for k, v in rep["group_delay_ms"].items()))
    for f in ("100", "1000"):
        L.append(f"\n## Sinus {f} Hz – poziom wejscia vs wynik")
        L.append("| wejscie dBFS | wyjscie dBFS | wzmocn. dB | THD % | H2 | H3 | parz./nieparz. dB | szum+alias dB |")
        L.append("|---|---|---|---|---|---|---|---|")
        for r_ in steps[f]:
            h = r_["harm_db_rel"] + [None, None]
            L.append(f"| {r_['in_dbfs']} | {r_['fund_dbfs']:.1f} | {r_['gain_db']:+.2f} | {r_['thd_pct']:.2f} | {fmt(h[0], 0)} | {fmt(h[1], 0)} | {fmt(r_['even_odd_db'], 1)} | {r_['noise_alias_db_rel']:.0f} |")
    L.append("\n## Aliasing (tony -6 dBFS; wszystko poza harmonicznymi = aliasing lub szum)")
    for f, m_ in hf.items():
        L.append(f"- {f} Hz: szum+alias {m_['noise_alias_db_rel']:.1f} dB, THD {m_['thd_pct']:.2f}% · najwieksze prazki: "
                 + ", ".join(f"{s_['hz']:.0f} Hz {s_['db_rel_fund']:.0f} dB" for s_ in m_["spurs"][:4]))
    L.append("\n## Krzywa statyczna (rampa 1 kHz; wejscie dBFS -> wyjscie dBFS sinusa)")
    L.append(", ".join(f"{k}->{v:.1f}" for k, v in rep["static_curve"].items()))
    open(os.path.join(out, "plugin_test.md"), "w").write("\n".join(L) + "\n")
    json.dump(jsonable(rep), open(os.path.join(out, "plugin_test.json"), "w"), indent=1)

    # ---- wykresy
    fig, axs = plt.subplots(2, 1, figsize=(14, 9))
    axs[0].plot(fm, mag_s, lw=2, label="sweep (1/12 okt.)")
    axs[0].plot(fw[mm], Hn[mm], lw=1, alpha=0.6, label="szum rozowy (Welch)")
    _freq_axis(axs[0], 20, min(20000, sr / 2))
    lo_ = math.floor(min(mag_s.min(), -6) / 3) * 3
    hi_ = math.ceil(max(mag_s.max(), 6) / 3) * 3
    axs[0].set_ylim(max(lo_, -48), min(hi_, 30))
    axs[0].set_ylabel("dB")
    axs[0].legend(loc="lower left")
    axs[0].set_title(f"Image 1 – {info['file']}: charakterystyka czestotliwosciowa (latencja {lat} probek)")
    axs[1].plot(fm, np.degrees(np.angle(np.exp(1j * phase))), lw=1.2, color="tab:purple", label="faza [°]")
    ax2 = axs[1].twinx()
    ax2.plot(fw[mm], Coh[mm], color="tab:green", lw=1, alpha=0.8, label="koherencja")
    ax2.set_ylim(0, 1.05)
    ax2.set_ylabel("koherencja")
    _freq_axis(axs[1], 20, min(20000, sr / 2))
    axs[1].set_ylim(-190, 190)
    axs[1].set_yticks(range(-180, 181, 45))
    axs[1].set_title("Faza (po usunieciu latencji) i koherencja szumu (1 = liniowo)")
    fig.tight_layout()
    fig.savefig(os.path.join(out, "pt1_charakterystyka.png"))
    plt.close(fig)

    fig, axs = plt.subplots(2, 1, figsize=(14, 9))
    for f, c in (("100", "tab:blue"), ("1000", "tab:red")):
        lv = [r_["in_dbfs"] for r_ in steps[f]]
        axs[0].plot(lv, [max(r_["thd_pct"], 1e-3) for r_ in steps[f]], "o-", color=c, label=f"THD {f} Hz")
        axs[1].plot(lv, [r_["gain_db"] for r_ in steps[f]], "o-", color=c, label=f"wzmocnienie {f} Hz")
    axs[0].set_yscale("log")
    axs[0].set_ylabel("THD %")
    axs[0].legend()
    axs[0].set_title("Image 2 – znieksztalcenia vs poziom wejscia")
    axs[1].plot(lv_in, curve_gain, color="0.5", lw=1, label="rampa 1 kHz")
    axs[1].set_xlabel("wejscie dBFS")
    axs[1].set_ylabel("dB")
    axs[1].legend()
    axs[1].set_title("Wzmocnienie vs poziom (spadek = kompresja / nasycenie)")
    for a in axs:
        a.set_xticks(range(-60, 1, 6))
    fig.tight_layout()
    fig.savefig(os.path.join(out, "pt2_poziomy.png"))
    plt.close(fig)

    fig, axs = plt.subplots(2, 1, figsize=(14, 9))
    _, yo = seg("hf_sweep")
    nfft = 2048
    fS, tS, ZS = signal.stft(yo, sr, nperseg=nfft, noverlap=nfft - 256)
    SS = pdb(np.abs(ZS) ** 2)
    m = axs[0].pcolormesh(tS, fS, SS - SS.max(), vmin=-100, vmax=0, cmap="magma", shading="auto", rasterized=True)
    plt.colorbar(m, ax=axs[0], pad=0.01).set_label("dB")
    axs[0].set_ylabel("Hz (liniowo)")
    axs[0].set_xlabel("s")
    axs[0].set_title("Image 3 – sweep 5 kHz→20 kHz przez wtyczke: linie opadajace / lustrzane = ALIASING")
    _, yo = seg("hf_11000", 0.1, 0.45)
    fA, PA_ = _bh_spectrum(yo, sr)
    axs[1].plot(fA, pdb(PA_ / PA_.max()), lw=1)
    axs[1].set_xlim(0, sr / 2)
    axs[1].set_ylim(-140, 3)
    axs[1].set_yticks(range(-140, 1, 20))
    for k in range(1, int(sr / 2 / 11000) + 1):
        axs[1].axvline(11000 * k, color="tab:green", ls=":", lw=1)
    axs[1].set_title("Widmo tonu 11 kHz (zielone = harmoniczne; reszta prazkow = aliasing)")
    axs[1].set_xlabel("Hz")
    fig.tight_layout()
    fig.savefig(os.path.join(out, "pt3_aliasing.png"))
    plt.close(fig)

    fig, axs = plt.subplots(2, 1, figsize=(14, 9))
    axs[0].plot(lv_in, db(rout) + 3.01, lw=2, label="wyjscie")
    axs[0].plot(lv_in, lv_in, ls="--", color="0.5", label="1:1")
    axs[0].set_xlabel("wejscie dBFS (sinus 1 kHz)")
    axs[0].set_ylabel("wyjscie dBFS")
    axs[0].set_xticks(range(-60, 1, 6))
    axs[0].legend()
    axs[0].set_title("Image 4 – krzywa statyczna")
    axs[1].plot(tb * 1000, gdb, lw=1.2)
    axs[1].set_xlim(800, 2600)
    axs[1].axvline(1000, color="tab:red", ls=":")
    axs[1].axvline(2000, color="tab:red", ls=":")
    axs[1].set_xlabel("ms (skok -30 -> -10 dBFS @1000, powrot @2000)")
    axs[1].set_ylabel("wzmocnienie dB")
    axs[1].set_title("Atak / release (wzmocnienie w czasie, obwiednia 2 ms)")
    fig.tight_layout()
    fig.savefig(os.path.join(out, "pt4_dynamika.png"))
    plt.close(fig)
    return rep


def null_test(pa, pb, out):
    plt = _plt()
    a, sr, ia = load_audio(pa)
    b, srb, ib = load_audio(pb)
    if sr != srb:
        raise SystemExit("Rozne sample rate.")
    am, bm = a.mean(1), b.mean(1)
    n = 1 << int(math.ceil(math.log2(len(am) + len(bm))))
    c = np.fft.irfft(np.fft.rfft(am, n) * np.conj(np.fft.rfft(bm, n)), n)
    lag = int(np.argmax(np.abs(c)))
    if lag > n // 2:
        lag -= n
    if lag >= 0:
        A, B = a[lag:], b
    else:
        A, B = a, b[-lag:]
    N = min(len(A), len(B))
    A, B = A[:N], B[:N]
    g = np.sum(A * B) / (np.sum(B * B) + EPS)
    res_ = A - g * B
    out_db = float(pdb(np.sum(res_ ** 2) / (np.sum(A ** 2) + EPS)))
    os.makedirs(out, exist_ok=True)
    L = [f"# DOOMSCOPE – null test", f"A: {ia['file']} · B: {ib['file']}",
         f"przesuniecie B: {lag} probek ({1000 * lag / sr:.2f} ms) · wzmocnienie B: {db(g):+.3f} dB · polaryzacja {'odwrocona' if g < 0 else 'ta sama'}",
         f"**residuum: {out_db:.1f} dB wzgl. A** (< -90 = identyczne bitowo prawie; < -60 = niesłyszalne roznice; > -30 = wyraznie inne)",
         f"residuum peak: {db(np.max(np.abs(res_))):.1f} dBFS", "", "| pasmo | residuum dB wzgl. A w pasmie |", "|---|---|"]
    rm = res_.mean(1)
    Am = A.mean(1)
    for name, lo, hi in BANDS:
        rb = sos_filter(rm, sr, lo=lo, hi=hi if hi < sr / 2 else None)
        ab = sos_filter(Am, sr, lo=lo, hi=hi if hi < sr / 2 else None)
        L.append(f"| {name} {lo}-{hi} | {pdb(np.sum(rb ** 2) / (np.sum(ab ** 2) + EPS)):.1f} |")
    hop = int(0.05 * sr)
    seg_db = [pdb(np.mean(rm[i:i + hop] ** 2) / (np.mean(Am ** 2) + EPS)) for i in range(0, N - hop, hop)]
    if seg_db:
        worst = int(np.argmax(seg_db))
        L.append(f"\nnajglosniejsze residuum w {worst * 50}-{worst * 50 + 50} ms ({seg_db[worst]:.1f} dB wzgl. sredniej A)")
    open(os.path.join(out, "null.md"), "w").write("\n".join(L) + "\n")
    fig, ax = plt.subplots(2, 1, figsize=(14, 9))
    t = np.arange(N) / sr
    ax[0].plot(t, Am, lw=0.6, label="A")
    ax[0].plot(t, rm, lw=0.6, color="tab:red", label="residuum A − g·B")
    ax[0].legend()
    ax[0].set_title(f"Image 1 – null test: residuum {out_db:.1f} dB")
    f1, P1 = average_spectrum(Am, sr)
    f2, P2 = average_spectrum(rm, sr)
    mm = (f1 >= 20) & (f1 <= sr / 2)
    ax[1].plot(f1[mm], pdb(frac_octave_smooth(f1[mm], P1[mm], 6)), label="A")
    ax[1].plot(f2[mm], pdb(frac_octave_smooth(f2[mm], P2[mm], 6)), color="tab:red", label="residuum")
    _freq_axis(ax[1], 20, min(20000, sr / 2))
    ax[1].legend()
    ax[1].set_title("Widmo A i residuum")
    fig.tight_layout()
    fig.savefig(os.path.join(out, "null.png"))
    plt.close(fig)
    print("\n".join(L))
    return out_db


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
    # --- P1: reasygnacja, HPSS, ostrosc
    fe_, te_ = reassign_point_check(48000)
    check("spektrogram reasygnowany: sinus 1 kHz / impuls @50 ms", abs(fe_ - 1000) < 1.5 and abs(te_ - 0.05) < 0.0002, f"{fe_:.2f} Hz, {te_ * 1000:.3f} ms")
    rng = np.random.default_rng(7)
    tt = np.arange(sr) / sr
    clicks = np.zeros(sr)
    clicks[sr // 16:: sr // 8] = 1.0
    clicks = sos_filter(clicks, sr, lo=500)
    hp_c = hpss_share(clicks, sr)["percussive_pct"]
    hp_s = hpss_share(np.sin(2 * np.pi * 220 * tt), sr)["percussive_pct"]
    check("HPSS: kliki perkusyjne, sinus harmoniczny", hp_c > 80 and hp_s < 10, f"kliki {hp_c:.0f}%, sinus {hp_s:.0f}%")
    nb = sos_filter(rng.standard_normal(2 * sr), sr, lo=920, hi=1080)
    nb *= 10 ** (-34 / 20) / np.std(nb)
    shv = sharpness_approx(nb, sr)["sharpness_acum_median"]
    check("ostrosc: szum waskopasmowy 1 kHz 60 dB = 1 acum (DIN 45692)", abs(shv - 1) < 0.1, f"{shv:.2f} acum")

    # --- testy wtyczek na syntetycznych 'wtyczkach' o znanych parametrach
    with tempfile.TemporaryDirectory() as td:
        srt = 44100
        segs, xt = test_layout(srt)
        X2 = np.stack([xt, xt], 1)

        def run_fake(name, yv):
            p_ = os.path.join(td, name + ".wav")
            wavfile.write(p_, srt, yv.astype(np.float32))
            return plugin_test(p_, os.path.join(td, name))
        r_id = run_fake("ident", X2)
        fr_ = r_id["freq_response_db"]
        check("plugin-test: wtyczka przezroczysta", r_id["latency_samples"] == 0 and max(abs(v) for v in fr_.values()) < 0.1
              and r_id["summary_levels"]["thd_over_1pct_from_dbfs"] is None,
              f"latencja {r_id['latency_samples']}, charakterystyka max odchylka {max(abs(v) for v in fr_.values()):.2f} dB")
        A_ = 10 ** (6 / 40)
        w0 = 2 * np.pi * 1000 / srt
        al = np.sin(w0) / 2
        b_ = [1 + al * A_, -2 * np.cos(w0), 1 - al * A_]
        a_ = [1 + al / A_, -2 * np.cos(w0), 1 - al / A_]
        r_eq = run_fake("eq", signal.lfilter(b_, a_, X2, axis=0))
        _, htrue = signal.freqz(b_, a_, [500.0, 1000.0, 2000.0], fs=srt)
        errs = [r_eq["freq_response_db"][k] - v for k, v in zip(("500", "1000", "2000"), db(np.abs(htrue)))]
        check("plugin-test: EQ peak +6 dB @1 kHz (sweep vs prawda)", max(abs(e) for e in errs) < 0.3, "bledy " + ", ".join(f"{e:+.2f}" for e in errs) + " dB")
        yv = np.tanh(2 * X2) / np.tanh(2)
        bb, aa = signal.butter(1, 8000 / (srt / 2))
        yv = np.vstack([np.zeros((37, 2)), signal.lfilter(bb, aa, yv, axis=0)])
        r_f = run_fake("tanh", yv)
        sl = r_f["summary_levels"]
        check("plugin-test: tanh(2x) + LP 8k + 37 probek opoznienia", abs(r_f["latency_samples"] - 37) <= 2 and abs(sl["gain_low_level_db"] - 6.33) < 0.15
              and sl["thd_over_1pct_from_dbfs"] in (-15, -12) and r_f["aliasing"]["11000"]["noise_alias_db_rel"] > -40,
              f"latencja {r_f['latency_samples']}, wzmocn. {sl['gain_low_level_db']:+.2f} dB (teoria +6.33), THD>1% od {sl['thd_over_1pct_from_dbfs']} dBFS, "
              f"aliasing 11k {r_f['aliasing']['11000']['noise_alias_db_rel']:.0f} dB")
        # kompresor: wzmocnienie zalezne od obwiedni (atak 5 ms, release 100 ms, prog -20 dB, 4:1)
        env = np.zeros(len(xt))
        aA, aR = math.exp(-1 / (0.005 * srt)), math.exp(-1 / (0.1 * srt))
        e = 0.0
        ax = np.abs(xt)
        for i in range(len(xt)):
            c_ = aA if ax[i] > e else aR
            e = c_ * e + (1 - c_) * ax[i]
            env[i] = e
        lvl = db(env)
        gr = np.where(lvl > -20, -(lvl + 20) * (1 - 1 / 4), 0)
        r_c = run_fake("comp", X2 * (10 ** (gr / 20))[:, None])
        dy = r_c["dynamics"]
        check("plugin-test: kompresor 4:1 (wykrycie redukcji, ataku i release)", dy["gain_reduction_db"] > 4 and dy["attack_ms_to_1db"] is not None
              and dy["release_ms_to_1db"] is not None and dy["release_ms_to_1db"] > dy["attack_ms_to_1db"],
              f"redukcja {dy['gain_reduction_db']:.1f} dB, atak {fmt(dy['attack_ms_to_1db'], 1)} ms, release {fmt(dy['release_ms_to_1db'], 0)} ms")
        # null test
        pa_ = os.path.join(td, "na.wav")
        pb_ = os.path.join(td, "nb.wav")
        k_ = synth_kick(srt)
        wavfile.write(pa_, srt, k_.astype(np.float32))
        wavfile.write(pb_, srt, (np.vstack([np.zeros((100, 2)), k_ * 0.5])).astype(np.float32))
        import contextlib
        import io
        with contextlib.redirect_stdout(io.StringIO()):
            nd = null_test(pa_, pb_, os.path.join(td, "null"))
        check("null test: ta sama probka przesunieta o 100 probek i -6 dB", nd < -100, f"residuum {nd:.1f} dB")
    print("\nWYNIK:", "wszystko OK" if ok else "SA BLEDY")
    return ok


# ----------------------------------------------------------------------------- CLI

def run_analyze(path, args, out):
    x, sr, info = load_audio(path)
    res = analyze(x, sr, info, args.type, args.bpm, args.key)
    os.makedirs(out, exist_ok=True)
    md = report_md(res)
    if getattr(args, "ref", None):
        prof = json.load(open(args.ref))
        ap = against_profile(flat_metrics(res), prof)
        res["vs_reference"] = ap
        md += profile_md(ap, prof)
    with open(os.path.join(out, "report.md"), "w") as fh:
        fh.write(md)
    files = [] if args.no_plots else plot_report(res, out)
    with open(os.path.join(out, "report.json"), "w") as fh:
        json.dump(jsonable({k: v for k, v in res.items() if not k.startswith("_")}), fh, indent=1, ensure_ascii=False)
    return res, md, files


AUDIO_EXT = (".wav", ".mp3", ".flac", ".aif", ".aiff", ".ogg", ".m4a")


def expand_files(items):
    out = []
    for it in items:
        if os.path.isdir(it):
            for root, _, fs in os.walk(it):
                out += [os.path.join(root, f) for f in sorted(fs) if f.lower().endswith(AUDIO_EXT)]
        else:
            out.append(it)
    return out


def run_profile(args):
    files = expand_files(args.files)
    if not files:
        raise SystemExit("Brak plikow audio.")
    os.makedirs(args.out, exist_ok=True)
    rows, names = [], []
    for f in files:
        try:
            x, sr, info = load_audio(f)
            res = analyze(x, sr, info, args.type, args.bpm, None)
        except (Exception, SystemExit) as e:
            print(f"pomijam {f}: {e}")
            continue
        fm = flat_metrics(res)
        fm["_type"] = res["type"]
        rows.append(fm)
        names.append(os.path.basename(f))
        print(f"ok  {os.path.basename(f)}  typ={res['type']}  ogon={fmt(fm.get('tail_hz'), 2)} Hz  petla={fmt(fm.get('loop_lufs'), 1)} LUFS")
    prof = build_profile(rows, names)
    json.dump(prof, open(os.path.join(args.out, "profile.json"), "w"), indent=1, ensure_ascii=False)
    keys = [k for k, *_ in METRICS]
    with open(os.path.join(args.out, "metrics.csv"), "w") as fh:
        fh.write("file,type," + ",".join(keys) + "\n")
        for n, r in zip(names, rows):
            fh.write(f"{n},{r['_type']}," + ",".join("" if r.get(k) is None else f"{r[k]:.4g}" for k in keys) + "\n")
    L = [f"# DOOMSCOPE – profil referencji ({prof['n']} plikow)", "| metryka | p10 | mediana | p90 | min | max | n |", "|---|---|---|---|---|---|---|"]
    for k, pm in prof["metrics"].items():
        n = 2 if pm["unit"] in ("", "acum") else (0 if pm["unit"] in ("Hz", "ms") else 1)
        L.append(f"| {pm['label']} [{pm['unit']}] | {pm['p10']:.{n}f} | {pm['median']:.{n}f} | {pm['p90']:.{n}f} | {pm['min']:.{n}f} | {pm['max']:.{n}f} | {pm['n']} |")
    open(os.path.join(args.out, "profile.md"), "w").write("\n".join(L) + "\n")
    print("\n".join(L))
    print(f"-> {args.out}/profile.json, profile.md, metrics.csv  (uzycie: analyze plik.wav --ref {args.out}/profile.json)")


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
        p.add_argument("--ref", default=None, help="profil referencji (profile.json z komendy profile)")
    p = sub.add_parser("profile", help="folder / lista WAV-ow referencyjnych -> profile.json + metrics.csv")
    p.add_argument("files", nargs="+")
    p.add_argument("--type", default="auto", choices=["auto", "kick", "screech", "lead", "rbass", "tonal"])
    p.add_argument("--bpm", type=float, default=None)
    p.add_argument("--out", default="doomscope_profile")
    p = sub.add_parser("gen-tests", help="sygnal testowy dla efektow (do FL)")
    p.add_argument("--sr", type=int, default=44100)
    p.add_argument("--out", default="doomscope_tests")
    p = sub.add_parser("plugin-test", help="analiza eksportu sygnalu testowego przez wtyczke")
    p.add_argument("file")
    p.add_argument("--out", default=None)
    p = sub.add_parser("null", help="null test dwoch renderow (regresja wersji wtyczki)")
    p.add_argument("files", nargs=2)
    p.add_argument("--out", default=None)
    sub.add_parser("selftest")
    args = ap.parse_args()
    if args.cmd == "selftest":
        sys.exit(0 if selftest() else 1)
    if getattr(args, "type", None) in ("screech", "lead", "rbass"):
        args.type = "tonal"
    if args.cmd == "gen-tests":
        gen_tests(args.out, args.sr)
        return
    if args.cmd == "plugin-test":
        out = args.out or os.path.splitext(args.file)[0] + "_plugintest"
        plugin_test(args.file, out)
        print(open(os.path.join(out, "plugin_test.md")).read())
        print(f"-> {out}/plugin_test.md, plugin_test.json, pt1_charakterystyka.png, pt2_poziomy.png, pt3_aliasing.png, pt4_dynamika.png")
        return
    if args.cmd == "null":
        out = args.out or os.path.splitext(args.files[0])[0] + "_null"
        null_test(args.files[0], args.files[1], out)
        print(f"-> {out}/null.md, null.png")
        return
    if args.cmd == "profile":
        run_profile(args)
        return
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
