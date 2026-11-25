#!/usr/bin/env python3
"""Fit a 4GE render to a reference PHASE BY PHASE (the owner's analysis protocol, 2026-11-25).
The distance uses the REF's own timeline (its change points) and, per phase: level, 9-band shape, tonal share per band,
crest, rails (hard clip), pitch; for the tail: harmonics h2..h16 + the even / odd balance.
usage: protocol_fit.py <ref.wav> <base params.txt> <midi> <bpm> <ms> <out name> <spec.json>
  spec.json = {"param": [min, max, start, step], ...}   (coordinate descent, 3 rounds, steps halve)
Renders with RenderTest --midi (FGE_RT = the RenderTest binary). Prints the best k=v line + the per-phase report."""
import sys, os, json, subprocess, numpy as np, soundfile as sf
sys.path.insert(0, os.path.dirname(__file__))
import kick_protocol as kp

RT = os.environ.get('FGE_RT', '/home/user/doomsday/FourGE/build/RenderTest_artefacts/Release/RenderTest')
TMP = os.environ.get('FGE_TMP', '/tmp/claude-0/ab')

def load_norm(p, ms=None):
    x, sr = sf.read(p, always_2d=True); m = x.mean(1); a = int(np.argmax(np.abs(m) > 0.003 * np.abs(m).max())); m = m[a:]
    if ms: m = m[:int(ms * sr / 1000)]
    return m / (np.abs(m).max() + 1e-12), sr

def features(m, sr, edges):
    import scipy.signal as ss
    low = kp.f0_track(ss.sosfiltfilt(ss.butter(4, 320, fs=sr, output='sos'), m), sr)
    B = kp.band_track(m, sr)
    tot_rms = np.sqrt(np.mean(m ** 2)) + 1e-12
    out = []
    for a, b in zip(edges[:-1], edges[1:]):
        s = m[int(a * sr / 1000):int(b * sr / 1000)]
        if len(s) < 64: out.append(None); continue
        rows = B[int(a / kp.HOP_MS):max(int(a / kp.HOP_MS) + 1, int(b / kp.HOP_MS))]
        bands = 10 * np.log10((10 ** (rows / 10)).mean(0) + 1e-15); bands = np.maximum(bands - bands.max(), -40)
        fs_ = [f for t, f, c in low if a <= t - 20 < b and c > 0.4 and f > 0]
        f0 = float(np.median(fs_)) if fs_ else 0.0
        ts = kp.tonal_share(s, sr, f0) if f0 > 0 else [None] * 9
        out.append(dict(rms=20 * np.log10(np.sqrt(np.mean(s ** 2)) / tot_rms + 1e-12), bands=bands, f0=f0,
                        tonal=np.array([0.15 if v is None else v for v in ts]),
                        crest=20 * np.log10(np.abs(s).max() / (np.sqrt(np.mean(s ** 2)) + 1e-12)), rails=float(np.mean(np.abs(s) > 0.98))))
    return out

def tail_harm(m, sr, f0, a_ms, b_ms):
    s = m[int(a_ms * sr / 1000):int(b_ms * sr / 1000)]
    if f0 <= 0 or len(s) < 2048: return None
    h = np.array(kp.harmonics(s, sr, f0, 16)); return np.maximum(h, -60)

def distance(R, F, Rh, Fh):
    d = []; w = []
    for r, f in zip(R, F):
        if r is None or f is None: continue
        d += [(r['rms'] - f['rms']) / 2.0]; w += [1.0]
        d += list((r['bands'] - f['bands']) / 4.0); w += [0.4] * 9
        d += list((r['tonal'] - f['tonal']) * 6.0); w += [0.5] * 9
        d += [(r['crest'] - f['crest']) / 2.0, (r['rails'] - f['rails']) * 8.0]; w += [0.6, 1.0]
        if r['f0'] > 0 and f['f0'] > 0: d += [12 * np.log2(f['f0'] / r['f0'])]; w += [1.0]
    if Rh is not None and Fh is not None:
        d += list((Rh[1:] - Fh[1:]) / 4.0); w += [0.5] * 15
        ev = lambda h: 10 * np.log10((10 ** (h[1::2] / 10)).sum() / (10 ** (h[2::2] / 10)).sum())
        d += [(ev(Rh) - ev(Fh)) / 2.0]; w += [1.0]
    d = np.array(d); w = np.array(w); return float(np.sqrt(np.sum(w * d * d) / np.sum(w)))

class Fitter:
    def __init__(self, ref, base, mid, bpm, ms, name):
        self.ref, self.base, self.mid, self.bpm, self.ms, self.name = ref, base, mid, bpm, ms, name
        m, self.sr = load_norm(ref, ms)
        r = kp.analyse(ref); self.edges = [0] + [c for c in r['change_points_ms'] if 3 < c < min(ms, r['length_ms']) - 10] + [int(min(ms, r['length_ms']))]
        self.R = features(m, self.sr, self.edges)
        longest = int(np.argmax(np.diff(self.edges))); self.ta, self.tb = self.edges[longest], self.edges[longest + 1]
        self.f0 = r['tail_f0']; self.Rh = tail_harm(m, self.sr, self.f0, self.ta, self.tb)
    def render(self, P, out):
        args = [RT, f'{TMP}/_pf.wav', '--midi', '-', self.mid, str(self.bpm)] + open(self.base).read().split() + [f'{k}={v}' for k, v in P.items()]
        subprocess.run(args, check=True, capture_output=True)
        x, sr = sf.read(f'{TMP}/_pf.wav', always_2d=True); m = x.mean(1); a = int(np.argmax(np.abs(m) > 0.003 * np.abs(m).max()))
        m = m[a:a + int(self.ms * sr / 1000)]; m = m / (np.abs(m).max() + 1e-12)
        sf.write(out, np.stack([m, m], 1), sr, subtype='PCM_24'); return m
    def score(self, P, keep=None):
        m = self.render(P, keep or f'{TMP}/_pf_n.wav')
        F = features(m, self.sr, self.edges); Fh = tail_harm(m, self.sr, self.f0, self.ta, self.tb)
        return distance(self.R, F, self.Rh, Fh), F

def report(fit, F):
    print(f"phase      | REF rms crest rails f0     | 4GE rms crest rails f0     | loudest band REF/4GE | tonal mids REF/4GE")
    for (a, b), r, f in zip(zip(fit.edges[:-1], fit.edges[1:]), fit.R, F):
        if r is None or f is None: continue
        lb = kp.BNAME
        print(f"{a:4d}-{b:<5d} | {r['rms']:5.1f} {r['crest']:5.1f} {r['rails']:4.2f} {r['f0']:6.1f} | {f['rms']:5.1f} {f['crest']:5.1f} {f['rails']:4.2f} {f['f0']:6.1f} | "
              f"{lb[int(np.argmax(r['bands']))]:7s}/{lb[int(np.argmax(f['bands']))]:7s} | {np.mean(r['tonal'][3:6]):.2f}/{np.mean(f['tonal'][3:6]):.2f}")

if __name__ == '__main__':
    ref, base, mid, bpm, ms, name, spec = sys.argv[1:8]
    fit = Fitter(ref, base, mid, int(bpm), int(ms), name)
    S = json.load(open(spec)); P = {k: v[2] for k, v in S.items()}; st = {k: v[3] for k, v in S.items()}
    best, F = fit.score(P); print('start', round(best, 3), flush=True)
    for it in range(3):
        for k in S:
            lo, hi = S[k][0], S[k][1]
            for sg in (1, -1):
                while True:
                    Q = dict(P); Q[k] = round(min(hi, max(lo, P[k] + sg * st[k])), 4)
                    if Q[k] == P[k]: break
                    d, FQ = fit.score(Q)
                    if d < best - 1e-3: best, P, F = d, Q, FQ
                    else: break
        print(it, round(best, 3), flush=True)
        for k in st: st[k] *= 0.5
    d, F = fit.score(P, f'{TMP}/{name}.wav')
    print('BEST', round(d, 3)); print(' '.join(f'{k}={v}' for k, v in P.items())); report(fit, F)
