import sys, glob, os, numpy as np
from scipy.io import wavfile
from scipy.signal import stft

def load(f):
    sr, x = wavfile.read(f)
    x = x.astype(np.float64)
    if x.dtype.kind != 'f' and np.abs(x).max() > 2: x /= np.abs(x).max()
    if x.ndim == 1: x = np.stack([x, x], 1)
    return sr, x[:, :2] / max(1e-9, np.abs(x).max())

def yin_f0(m, sr, t0, win=0.04, fmin=60, fmax=1500):
    n = int(win*sr); a = int(t0*sr)
    if a+2*n >= len(m): return np.nan, 0
    seg = m[a:a+2*n]
    taus = np.arange(int(sr/fmax), int(sr/fmin))
    d = np.array([np.sum((seg[:n]-seg[t:t+n])**2) for t in taus])
    cmnd = d * np.arange(1, len(d)+1) / np.maximum(np.cumsum(d), 1e-12)
    i = np.argmin(cmnd)
    if 0 < i < len(cmnd)-1:
        a_, b_, c_ = cmnd[i-1], cmnd[i], cmnd[i+1]; off = 0.5*(a_-c_)/(a_-2*b_+c_+1e-12)
    else: off = 0
    return sr/(taus[0]+i+off), 1-cmnd[i]

def analyse(f):
    sr, x = load(f)
    m = x.mean(1); s = (x[:,0]-x[:,1])/2
    dur = len(m)/sr
    # amplitude envelope
    hop = int(0.01*sr)
    rms = np.array([np.sqrt(np.mean(m[i:i+hop]**2)+1e-12) for i in range(0, len(m)-hop, hop)])
    rdb = 20*np.log10(rms/rms.max())
    on = np.argmax(rdb > -30)*0.01
    end = (len(rdb) - np.argmax(rdb[::-1] > -30))*0.01
    att = (np.argmax(rdb > -3)*0.01) - on
    # f0 contour every 10 ms
    ts = np.arange(on, end-0.09, 0.01)
    f0 = np.array([yin_f0(m, sr, t)[0] for t in ts]); conf = np.array([yin_f0(m, sr, t)[1] for t in ts])
    good = conf > 0.6
    med = np.nanmedian(f0[good]) if good.any() else np.nan
    cents = 1200*np.log2(f0/med)
    # spectrum (sustain part)
    a, b = int((on+0.08)*sr), int(min(end, on+1.2)*sr)
    h = m[a:b]; w = np.hanning(len(h)); S = np.abs(np.fft.rfft(h*w))**2; fr = np.fft.rfftfreq(len(h), 1/sr)
    bands = [(20,200),(200,800),(800,2000),(2000,5000),(5000,10000),(10000,20000)]
    tot = S[fr>20].sum(); be = [S[(fr>=lo)&(fr<hi)].sum()/tot*100 for lo,hi in bands]
    cen = (S*fr).sum()/S.sum()
    mm = (fr>2000)&(fr<8000); flat = np.exp(np.mean(np.log(S[mm]+1e-20)))/np.mean(S[mm]+1e-20)
    # harmonic amplitudes relative to f0 (first 12)
    harm = []
    for k in range(1, 13):
        sel = (fr > med*k*0.97) & (fr < med*k*1.03)
        harm.append(10*np.log10(S[sel].max()+1e-20) if sel.any() else -200)
    harm = np.array(harm) - harm[0]
    # centroid over time -> modulation (filter / wah / talk)
    f_, t_, Z = stft(m, sr, nperseg=2048, noverlap=2048-441)
    P = np.abs(Z)**2
    cen_t = (P*f_[:,None]).sum(0)/(P.sum(0)+1e-20)
    sel = (t_ > on+0.05) & (t_ < end-0.05)
    ct = cen_t[sel]
    cen_range = (np.percentile(ct, 90)/max(1, np.percentile(ct, 10))) if len(ct) > 4 else 1
    # modulation rate of centroid
    rate = np.nan
    if len(ct) > 40:
        c = ct - ct.mean(); sp = np.abs(np.fft.rfft(c*np.hanning(len(c)))); fq = np.fft.rfftfreq(len(c), 0.01)
        k = (fq > 0.5) & (fq < 20)
        if k.any() and sp[k].max() > 3*np.median(sp[k]): rate = fq[k][np.argmax(sp[k])]
    # pitch movement
    cg = cents[good]
    first = cents[:8][good[:8]] if good[:8].any() else np.array([np.nan])
    vib = np.nanstd(cg[10:]) if len(cg) > 20 else np.nan
    # stereo
    corr = np.corrcoef(x[a:b,0], x[a:b,1])[0,1]
    sideR = np.sqrt(np.mean(s[a:b]**2))/np.sqrt(np.mean(m[a:b]**2)+1e-12)
    crest = 20*np.log10(np.abs(m[a:b]).max()/np.sqrt(np.mean(m[a:b]**2)))
    note = 69+12*np.log2(med/440)
    return dict(name=os.path.basename(f)[:46], sr=sr, dur=dur, on=on, att=att, sus=end-on, f0=med, note=note,
                start_c=np.nanmedian(first), pmin=np.nanpercentile(cg,5) if len(cg) else np.nan, pmax=np.nanpercentile(cg,95) if len(cg) else np.nan,
                vib=vib, cen=cen, cenR=cen_range, rate=rate, flat=flat, bands=be, harm=harm, corr=corr, side=sideR, crest=crest,
                env_db=rdb, cents=cents, ts=ts, good=good, cen_t=ct)

if __name__ == '__main__':
    import pickle
    res = [analyse(f) for f in sorted(glob.glob(sys.argv[1] + '/*.wav'))]
    names = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']
    print(f"{'file':46s} dur  att  f0     note  start  pmin  pmax  vib  centr  cenR rate  flat  corr side crest | <200 200-800 .8-2k 2-5k 5-10k 10k+ | h2..h6 dB")
    for r in res:
        n = int(round(r['note'])) if np.isfinite(r['note']) else 0
        print(f"{r['name']:46s} {r['sus']:.2f} {r['att']*1000:3.0f} {r['f0']:6.1f} {names[n%12]}{n//12-1:<2d} {r['start_c']:6.0f} {r['pmin']:5.0f} {r['pmax']:5.0f} {r['vib']:4.0f} {r['cen']:6.0f} {r['cenR']:4.1f} {r['rate']:4.1f} {r['flat']:.2f} {r['corr']:5.2f} {r['side']:.2f} {r['crest']:4.1f} | "
              + " ".join(f"{v:4.1f}" for v in r['bands']) + " | " + " ".join(f"{v:4.0f}" for v in r['harm'][1:6]))
    pickle.dump(res, open(sys.argv[2], 'wb'))
