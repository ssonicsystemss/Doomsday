import sys, os, json, time, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from kmatch import *

def sep_cma(f, x0, sigma, gens, lam=20, seed=1, log=None):
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
        Z = rng.standard_normal((lam, n))
        Y = Z * np.sqrt(C)
        X = np.clip(m + sigma * Y, 0, 1)
        Y = (X - m) / sigma
        fit = f(X)
        order = np.argsort([v[0] for v in fit])
        if fit[order[0]][0] < best[0]: best = (fit[order[0]][0], X[order[0]].copy(), fit[order[0]][1])
        ysel = Y[order[:mu]]; yw = w @ ysel
        m = np.clip(m + sigma * yw, 0, 1)
        ps = (1 - cs) * ps + np.sqrt(cs * (2 - cs) * mueff) * yw / np.sqrt(C)
        hs = np.linalg.norm(ps) / np.sqrt(1 - (1 - cs) ** (2 * (g + 1))) < (1.4 + 2 / (n + 1)) * chiN
        pc = (1 - cc) * pc + hs * np.sqrt(cc * (2 - cc) * mueff) * yw
        C = (1 - c1 - cmu) * C + c1 * pc ** 2 + cmu * (w @ (ysel ** 2))
        sigma *= np.exp((cs / ds) * (np.linalg.norm(ps) / chiN - 1))
        sigma = min(sigma, 0.4)
        if log: log(g, best[0], best[2], sigma)
    return best

def x_of(name, val):
    for i, (k, kind, lo, hi) in enumerate(SPACE):
        if k == name: return np.log(val / lo) / np.log(hi / lo) if kind == LOG else (val - lo) / (hi - lo)

def initial(t):
    defaults = dict(tail_hold=0, tail_start=3.5, tail_drop=60, tail_curve=4, tail_wave=0.1, dist_type=1.5, drive=24, tone=0.1, low_cut=30, high_cut=10000, scream=0.1, scream_hz=900, body=0, grit=0.1,
                    crunch=0.15, crunch_dec=100, punch_level=-10, punch_start=400, punch_end=100, punch_drop=10, punch_decay=30, click_level=-30, chaos=0.4, chaos_dec=50, chaos_hz=2500, click_hz=4000, click_len=3,
                    clip_drive=6, clip_soft=0.3, tail_level=0)
    x = np.zeros(D)
    for i, (k, *_r) in enumerate(SPACE): x[i] = x_of(k, defaults[k])
    o = len(SPACE)
    env = 10 ** (t.env / 20); k = np.arange(t.nf) * HOP / SR * 1000
    L = t.l40
    ref = env[(k > 20) & (k < L * 0.7)].max() if np.any((k > 20) & (k < L * 0.7)) else env.max()
    for j, pm in enumerate(AMP_T):
        ms = max(15, pm * L / 1000); x[o + j] = np.clip(np.interp(ms, k, env) / ref, 0.05, 1)
    o += len(AMP_T)
    # drive envelope from the brightness of the reference over time
    br = t.bright; act = (k < L) & (env > env.max() * 0.03)
    lo_, hi_ = np.percentile(br[act], 5), np.percentile(br[act], 95)
    for j, pm in enumerate(DRV_T):
        ms = pm * L / 1000
        x[o + j] = np.clip(0.15 + 0.8 * (np.interp(ms, k, br) - lo_) / max(1e-3, hi_ - lo_), 0.05, 0.95)
    x[-1] = 0.5
    # pitch drop from the reference f0 track
    f0 = t.f0; tail = np.nanmedian(f0[(k > 150) & (k < max(200, L * 0.8))])
    if np.isfinite(tail):
        st = np.nanmedian(f0[(k > 4) & (k < 14)])
        if np.isfinite(st):
            x[x_of_idx('tail_start')] = np.clip(x_of('tail_start', np.clip(st / tail, 1.05, 9.5)), 0, 1)
            ex = (f0 - tail) / max(1, st - tail)
            te = k[np.argmax((k > 4) & (ex < 1 / np.e))] if np.any((k > 4) & (ex < 1 / np.e)) else 20
            x[x_of_idx('tail_curve')] = x_of('tail_curve', 4)
            x[x_of_idx('tail_drop')] = np.clip(x_of('tail_drop', np.clip(4 * te, 9, 290)), 0, 1)
    return x

def main():
    ref, outname, gens = sys.argv[1], sys.argv[2], int(sys.argv[3])
    note = int(sys.argv[4]); seed = int(sys.argv[5]) if len(sys.argv) > 5 else 1
    t = Target(ref)
    secs = min(t.len + 0.05, 1.95)
    work = f'{SHM}/{outname}'
    def f(X):
        lines = [jobline(f'c{i:03d}', note, decode(x, t.l40), secs) for i, x in enumerate(X)]
        render(lines, work)
        return [distance(t, load(f'{work}/c{i:03d}.wav')) for i in range(len(X))]
    t0 = time.time()
    def log(g, b, parts, s):
        if g % 5 == 0 or g == gens - 1: print(f'{outname} gen {g:3d} best {b:6.2f} spec {parts[0]:5.2f} env {parts[1]:5.2f} semis {parts[2]:5.2f} ltas {parts[3]:5.2f} flat {parts[4]:5.2f} sigma {s:.3f} {time.time()-t0:5.0f}s', flush=True)
    best = (1e9, None, None)
    for r, dt in enumerate([1, 2, 0]):
        x0 = initial(t); x0[x_of_idx('dist_type')] = (dt + 0.5) / 5
        b = sep_cma(f, x0, 0.2, gens, seed=seed * 10 + r, log=log)
        print(outname, 'restart', r, round(b[0], 2), flush=True)
        if b[0] < best[0]: best = b
    b = sep_cma(f, best[1], 0.07, gens // 2, seed=seed * 10 + 7, log=log)
    if b[0] < best[0]: best = b
    # categorical sweep of the distortion type around the best point
    xs = []
    for d in range(5):
        x = best[1].copy(); x[x_of_idx('dist_type')] = (d + 0.5) / 5; xs.append(x)
    fit = f(np.array(xs))
    i = int(np.argmin([v[0] for v in fit]))
    if fit[i][0] < best[0]: best = (fit[i][0], xs[i], fit[i][1])
    json.dump(dict(ref=ref, note=note, score=best[0], parts=best[2], x=list(map(float, best[1])), params=decode(best[1], t.l40)), open(f'{os.path.dirname(__file__)}/../tune/{outname}.json', 'w'), indent=1)
    print(outname, 'FINAL', round(best[0], 2), best[2])

def x_of_idx(name): return [k for k, *_ in SPACE].index(name)

if __name__ == '__main__': main()
