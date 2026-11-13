# CRUNCH spec v2 (owner 2026-11-14, final DSP spec): parallel CLEAN / CRUNCH paths, internal BLEND crossfade inside the crunch envelope,
# drive = internal pre-gain + makeup, 0 % = bypass, AUTO LEVEL COMPENSATION (100 % is not louder), moving bandpass tracks the note.
import numpy as np; from scipy.io import wavfile; from scipy.signal import butter,sosfilt,lfilter
SR=44100; n=int(0.40*SR); t=np.arange(n)/SR*1000
def seg(pts):
    f=np.full(n,pts[-1][1])
    for (a,fa),(b,fb) in zip(pts[:-1],pts[1:]):
        m=(t>=a)&(t<b); f[m]=fa*(fb/fa)**((t[m]-a)/(b-a))
    return f
osc=lambda pts: np.sin(2*np.pi*np.cumsum(seg(pts))/SR)
fade=np.clip((400-t)/80,0,1)
def svf_bp(x,fc,q):
    y=np.zeros_like(x); ic1=ic2=0.0
    for i in range(len(x)):
        g=np.tan(np.pi*min(fc[i],0.45*SR)/SR); k=1/q; a1=1/(1+g*(g+k)); a2=g*a1; a3=g*a2
        v3=x[i]-ic2; v1=a1*ic1+a2*v3; v2=ic2+a2*ic1+a3*v3; ic1=2*v1-ic1; ic2=2*v2-ic2; y[i]=v1
    return y
def env(peak,end): e=np.where(t<peak,t/peak,np.clip(1-(t-peak)/(end-peak),0,1)); return e*e*(3-2*e)
def asymr(x,db,neg): g=10**(db/20); return np.where(x>=0,np.tanh(g*x),np.tanh(g*neg*x))
def bell(x,fc,gdb,q=0.9):
    A=10**(gdb/40); w=2*np.pi*fc/SR; al=np.sin(w)/(2*q); c=np.cos(w); return lfilter([1+al*A,-2*c,1-al*A],[1+al/A,-2*c,1-al/A],x)
hp=lambda y: sosfilt(butter(2,25,'high',fs=SR,output='sos'),y)
rms=lambda y,a,b: np.sqrt(np.mean(y[int(a*SR/1000):int(b*SR/1000)]**2))+1e-12
def kick(x,note=49.0,track=1.0):
    r=note/49.0
    src=(osc([(0,650*r),(20,620*r),(45,450*r),(80,note)])+0.8*osc([(0,160*r),(40,55*r),(80,note)]))*fade
    clean=asymr(src,14,0.45)                                        # CLEAN PATH (the punch / body as it is)
    if x<=0: return clean
    drive=18+24*x; neg=0.7-0.45*x; q=1.5-0.8*x; peak,end=6+4*x,25+35*x
    k=r**track                                                      # the band follows the note
    f0,f1=(2000+2500*x)*k,(700+300*x)*k
    c=asymr(src,drive,neg)                                          # internal pre-gain into the nonlinearity
    c=svf_bp(c,f0*(f1/f0)**np.clip(t/end,0,1),q)                     # MOVING bandpass
    c=bell(c,2500*k,6*x)                                            # harmonic shape (upper mid)
    c*=rms(clean,0,end)/rms(c,0,end)                                # makeup: the crunch path as loud as the clean one
    e=env(peak,end)*min(1,1.2*x)                                     # CRUNCH envelope x internal BLEND
    return (1-e)*clean+e*c                                          # CROSSFADE inside the crunch window
ref=None
for v in (0,25,50,75,100):
    y=hp(kick(v/100))
    if ref is None: ref=rms(y,0,150); g=0.5/np.abs(y).max()
    y*=ref/rms(y,0,150)*g                                           # AUTO LEVEL: same loudness for every setting
    wavfile.write('L%03d CRUNCH %d.wav'%(v,v),SR,np.stack([y,y],1).astype(np.float32)); print(v,'peak',round(float(np.abs(y).max()),2))
