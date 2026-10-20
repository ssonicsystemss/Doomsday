import sys, glob, numpy as np
from scipy.io import wavfile
from scipy.signal import get_window
def load(p):
    sr,x=wavfile.read(p); x=x.astype(float)
    if x.dtype!=float or np.abs(x).max()>2: x/=np.abs(x).max()
    return sr,x
def f0_track(m,sr,hop=441,win=4096,fmin=80,fmax=1200):
    out=[]
    for s in range(0,len(m)-win,hop):
        f=m[s:s+win]*np.hanning(win)
        ac=np.fft.irfft(np.abs(np.fft.rfft(f,2*win))**2)[:win]
        ac/=ac[0]+1e-12
        lo,hi=int(sr/fmax),int(sr/fmin)
        k=lo+np.argmax(ac[lo:hi])
        # parabolic interp
        a,b,c=ac[k-1],ac[k],ac[k+1]; d=0.5*(a-c)/(a-2*b+c+1e-12)
        out.append((s/sr, sr/(k+d), ac[k]))
    return np.array(out)
for p in sorted(glob.glob(sys.argv[1]+'/*.wav')):
    sr,x=load(p); L,R=x[:,0],x[:,1]; m=(L+R)/2; sd=(L-R)/2
    name=p.split('/')[-1][:-4]
    # envelope
    hop=int(sr*0.005); rms=np.array([np.sqrt(np.mean(m[i:i+hop]**2)) for i in range(0,len(m)-hop,hop)])
    pk=rms.max(); t_on=np.argmax(rms>pk*0.1)*5; t_pk=np.argmax(rms>pk*0.9)*5
    last=len(rms)-1-np.argmax(rms[::-1]>pk*0.1); dur=last*5
    # f0
    tr=f0_track(m,sr)
    good=tr[(tr[:,2]>0.5)]
    f0=np.median(good[:,1]) if len(good) else 0
    midi=69+12*np.log2(f0/440) if f0>0 else 0
    early=good[good[:,0]<0.06]; mid=good[(good[:,0]>0.15)&(good[:,0]<0.8)]
    bend = 12*np.log2(np.median(early[:,1])/f0) if len(early) else 0
    vib = 1200*np.std(np.log2(mid[:,1]/f0)) if len(mid) else 0
    # spectrum of steady part
    a=int(0.2*sr); b=min(len(m),int(0.9*sr)); seg=m[a:b]*np.hanning(b-a)
    S=np.abs(np.fft.rfft(seg))**2; f=np.fft.rfftfreq(b-a,1/sr)
    tot=S[f>20].sum()
    bands=[(20,250),(250,800),(800,2000),(2000,5000),(5000,10000),(10000,20000)]
    br=[S[(f>=lo)&(f<hi)].sum()/tot*100 for lo,hi in bands]
    cen=(S*f).sum()/S.sum()
    msk=(f>1000)&(f<8000); flat=np.exp(np.mean(np.log(S[msk]+1e-20)))/np.mean(S[msk])
    # harmonics
    H=[]
    for h in range(1,31):
        fh=h*f0; w=(f>fh-f0*0.25)&(f<fh+f0*0.25)
        H.append(10*np.log10(S[w].sum()+1e-20))
    H=np.array(H)-max(H)
    even=np.mean(H[1::2][:10]); odd=np.mean(H[2::2][:10])
    # harmonic vs inharmonic energy (noise/detune)
    harm=0
    for h in range(1,int(15000/f0)):
        w=(f>h*f0-f0*0.12)&(f<h*f0+f0*0.12); harm+=S[w].sum()
    hfrac=harm/S[(f>20)&(f<15000)].sum()
    # envelope of formant: smoothed spectrum peaks (1/3 octave smoothing)
    fc=np.geomspace(200,10000,60); env=[]
    for c in fc:
        w=(f>c/2**(1/6))&(f<c*2**(1/6)); env.append(10*np.log10(S[w].mean()+1e-20))
    env=np.array(env); env-=env.max()
    peaks=[fc[i] for i in range(1,len(env)-1) if env[i]>env[i-1] and env[i]>env[i+1] and env[i]>-25]
    # stereo
    side=np.sqrt(np.mean(sd[a:b]**2))/np.sqrt(np.mean(m[a:b]**2)+1e-12)
    corr=np.corrcoef(L[a:b],R[a:b])[0,1]
    print(f"== {name}")
    print(f"  f0 {f0:.1f} Hz (midi {midi:.2f})  onset bend {bend:+.2f} st  pitch wobble {vib:.1f} ct  attack {t_pk-t_on} ms  len {dur} ms")
    print(f"  bands% " + " ".join(f"{v:5.1f}" for v in br) + f"  centroid {cen:.0f} Hz  flat1-8k {flat:.3f}  harmonic {hfrac*100:.0f}%")
    print(f"  harmonics dB 1-16: " + " ".join(f"{v:.0f}" for v in H[:16]) + f"   even/odd(2-21) {even:.1f}/{odd:.1f}")
    print(f"  spectral peaks (Hz): " + ", ".join(f"{p:.0f}" for p in peaks))
    print(f"  stereo side/mid {side:.2f}  L/R corr {corr:.2f}")
