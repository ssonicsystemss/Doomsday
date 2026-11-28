#!/usr/bin/env python3
"""Fit a 4GE render to a reference using DOOMSCOPE's own comparison (Shared/Tools/doomscope) as the target.
score = rms of the 1/3-octave differences (40 Hz .. 12.7 kHz, after DOOMSCOPE's loudness match, clipped +-20 dB)
      + crest 0-15 ms, attack 10-90 %, percussive share 0-30 ms, centroid (cents) - each scaled to ~dB.
usage: ds_fit.py <ref.wav> <base params.txt> <midi> <bpm> <ms> <out name> <spec.json>   (spec = {"param": [min, max, start, step]})"""
import sys, os, re, json, subprocess, numpy as np, soundfile as sf
RT = os.environ.get('FGE_RT', '/home/user/doomsday/FourGE/build/RenderTest_artefacts/Release/RenderTest')
DS = os.environ.get('FGE_DS', '/home/user/doomsday/Shared/Tools/doomscope/doomscope.py')
TMP = os.environ.get('FGE_TMP', '/tmp/claude-0/dsfit'); os.makedirs(TMP, exist_ok=True)

def render(base, mid, bpm, ms, P, out):
    args = [RT, f'{TMP}/_r.wav', '--midi', '-', mid, str(bpm)] + open(base).read().split() + [f'{k}={v}' for k, v in P.items()]
    subprocess.run(args, check=True, capture_output=True)
    x, sr = sf.read(f'{TMP}/_r.wav', always_2d=True); m = x.mean(1); a = int(np.argmax(np.abs(m) > 0.003 * np.abs(m).max()))
    m = m[a:a + int(ms * sr / 1000)]; m = m / (np.abs(m).max() + 1e-12); sf.write(out, np.stack([m, m], 1), sr, subtype='PCM_24')

def compare(a, ref, bpm):
    subprocess.run(['python3', '-I', DS, 'compare', a, ref, '--bpm', str(bpm)], check=True, capture_output=True, cwd=TMP)
    d = os.path.splitext(a)[0] + '_vs_' + os.path.splitext(os.path.basename(ref))[0]
    t = open(os.path.join(d, 'compare.md')).read()
    bands = {float(k): float(v) for k, v in re.findall(r'(\d+):([+-]?\d+\.\d)', t.split('wszystkie pasma')[1].split('\n')[0])}
    def row(name):
        m = re.search(r'\| ' + re.escape(name) + r'[^|]*\| ([^|]+) \| ([^|]+) \|', t)
        try: return float(m.group(1)), float(m.group(2))
        except Exception: return None
    return bands, {k: row(k) for k in ['crest 0-15 ms dB', 'atak 10-90% ms', 'perkusyjny 0-30 ms %', 'centroid widma Hz (mediana)', 'crest dB']}

def score(bands, rows):
    b = np.array([np.clip(v, -20, 20) for f, v in bands.items() if 40 <= f <= 12700])
    s = list(b)
    def add(k, scale):
        r = rows.get(k)
        if r: s.append((r[1] - r[0]) / scale)
    add('crest 0-15 ms dB', 1.0); add('crest dB', 1.0); add('perkusyjny 0-30 ms %', 3.0)
    r = rows.get('atak 10-90% ms')
    if r: s.append(np.clip((np.log2(max(r[1], 0.5)) - np.log2(max(r[0], 0.5))) * 3, -20, 20))
    r = rows.get('centroid widma Hz (mediana)')
    if r: s.append(1200 * np.log2(r[1] / max(r[0], 1)) / 150)
    s = np.array(s); return float(np.sqrt(np.mean(s * s)))

if __name__ == '__main__':
    ref, base, mid, bpm, ms, name, spec = sys.argv[1:8]; bpm = int(bpm); ms = int(ms)
    S = json.load(open(spec)); P = {k: v[2] for k, v in S.items()}; st = {k: v[3] for k, v in S.items()}
    def ev(Q, keep=None):
        out = keep or f'{TMP}/_cand.wav'; render(base, mid, bpm, ms, Q, out); b, r = compare(out, ref, bpm); return score(b, r), b, r
    best, B, Rw = ev(P); print('start', round(best, 2), flush=True)
    for it in range(2):
        for k in S:
            lo, hi = S[k][0], S[k][1]
            for sg in (1, -1):
                while True:
                    Q = dict(P); Q[k] = round(min(hi, max(lo, P[k] + sg * st[k])), 4)
                    if Q[k] == P[k]: break
                    d, b, r = ev(Q)
                    if d < best - 0.02: best, P, B, Rw = d, Q, b, r
                    else: break
        print(it, round(best, 2), flush=True)
        for k in st: st[k] *= 0.5
    d, B, Rw = ev(P, f'{TMP}/{name}.wav')
    print('BEST', round(d, 2)); print(' '.join(f'{k}={v}' for k, v in P.items()))
    print('rows (4GE, REF):', {k: v for k, v in Rw.items()}); print('1/3 oct REF-4GE:', ' '.join(f'{int(f)}:{v:+.0f}' for f, v in B.items()))
