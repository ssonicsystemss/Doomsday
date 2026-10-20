import sys, numpy as np, wave
def load(p):
    w=wave.open(p); sr=w.getframerate(); n=w.getnframes(); ch=w.getnchannels()
    a=np.frombuffer(w.readframes(n),dtype=np.uint8).reshape(-1,3)
    x=(a[:,0].astype(np.int32)|(a[:,1].astype(np.int32)<<8)|(a[:,2].astype(np.int32)<<16)); x=np.where(x>=1<<23,x-(1<<24),x)/float(1<<23)
    return x.reshape(-1,ch)[:,0], sr
bands=[(20,100),(100,300),(300,800),(800,2500),(2500,8000),(8000,20000)]
for p in sys.argv[1:]:
    x,sr=load(p); beat=int(sr*60/150)
    hit=x[beat:2*beat]; tail=hit[int(0.06*sr):int(0.30*sr)]
    S=np.abs(np.fft.rfft(tail*np.hanning(len(tail))))**2; f=np.fft.rfftfreq(len(tail),1/sr)
    tot=S.sum(); r=[S[(f>=a)&(f<b)].sum()/tot for a,b in bands]; cen=(S*f).sum()/tot
    rms=np.sqrt(np.mean(hit[:int(0.35*sr)]**2))
    print(f"{p.split('/')[-1]:28s}"+" ".join(f"{v*100:5.1f}%" for v in r)+f"  cen {cen:6.0f}Hz  rms {20*np.log10(rms):5.1f}dB  pk {20*np.log10(np.max(np.abs(hit))):5.1f}")
