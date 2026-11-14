# punch after Eternize "Hardstyle Kick Punches Explained" (2026): TONAL layer from a BROADBAND source (snare / white noise, not a sine)
# -> EQ boost in the mids -> distortion (drives them into the ceiling) -> tape-stop pitch-down at the end -> volume shaper
# (transient + body) -> corrective EQ (lows out) ; + CLICK layer ; + SUB layer, mono, energy slightly DELAYED ; + a little short reverb
import numpy as np; from scipy.io import wavfile; from scipy.signal import butter,sosfilt,lfilter,fftconvolve
SR=44100; n=int(0.40*SR); t=np.arange(n)/SR*1000; rng=np.random.default_rng(11)
f2=lambda x,k,fc,o=2: sosfilt(butter(o,fc,k,fs=SR,output='sos'),x,axis=0)
def bell(x,fc,g,q=1.0):
    A=10**(g/40); w=2*np.pi*fc/SR; al=np.sin(w)/(2*q); c=np.cos(w); return lfilter([1+al*A,-2*c,1-al*A],[1+al/A,-2*c,1-al/A],x)
def asym(x,db,neg=0.5): g=10**(db/20); return np.where(x>=0,np.tanh(g*x),np.tanh(g*neg*x))
def snare(st=14):                                   # a synth "snare": noise + a short 190 Hz body, transposed up by st
    r=2**(st/12); m=int(0.3*SR); tt=np.arange(m)/SR
    s=rng.standard_normal(m)*np.exp(-tt/0.12)+0.8*np.sin(2*np.pi*190*tt)*np.exp(-tt/0.05)
    s=f2(s,'high',150); idx=np.arange(0,m,r); return np.interp(idx,np.arange(m),s)
def tapestop(x,depth=0.5,start=0.35):               # slow-down towards the end: playback rate falls from 1 to 1-depth after `start`
    L=len(x); u=np.arange(L)/L; rate=np.where(u<start,1.0,1-depth*((u-start)/(1-start))**1.5); pos=np.cumsum(rate); pos=pos[pos<L-1]
    return np.interp(pos,np.arange(L),x)
def shaper(L,att_ms=1,spike_ms=6,body=0.55,body_ms=70,end_ms=140):   # VOLUME SHAPER: transient spike, then the body, then out
    tt=np.arange(L)/SR*1000
    return np.interp(tt,[0,att_ms,spike_ms,spike_ms+4,body_ms,end_ms],[0,1,1,body,body*0.8,0])
def punch(mid=1000,gain=12,drive=24,tape=0.5,click=0.5,sub=0.9,sub_delay=25,verb=0.12):
    src=snare()
    x=bell(src,mid,gain,1.0); x=asym(x/np.abs(x).max(),drive)                     # EQ -> distortion = the character
    x=tapestop(x,tape)                                                             # pitch down towards the end
    L=min(len(x),n); y=np.zeros(n); y[:L]=x[:L]*shaper(L)                          # dynamic shape
    y=f2(y,'high',250,3)                                                           # corrective EQ: lows out
    # CLICK: a laser (8 kHz -> 300 Hz in 4 ms) distorted, ~6 ms
    f=8000*(300/8000)**np.clip(t/4,0,1); c=asym(np.sin(2*np.pi*np.cumsum(f)/SR),18)*np.exp(-t/2.0); c=f2(c,'high',2000)
    # SUB: mono sine, energy delayed
    fs=np.interp(t,[0,40,120],[110,60,55]); s=np.sin(2*np.pi*np.cumsum(fs)/SR)*np.interp(t,[0,sub_delay,sub_delay+15,220,300],[0,0.2,1,0.8,0])
    mono=y/np.abs(y).max()+click*c/np.abs(c).max()+sub*s
    ir=rng.standard_normal((int(0.12*SR),2))*np.exp(-np.arange(int(0.12*SR))/(0.025*SR))[:,None]; ir=f2(ir,'high',400)
    w=np.stack([fftconvolve(y,ir[:,0])[:n],fftconvolve(y,ir[:,1])[:n]],1); w/=np.abs(w).max()+1e-9
    out=np.stack([mono,mono],1)+verb*w; return out/np.abs(out).max()*0.9
V={'P1 punch wg Eternize (srodek 1 kHz)':punch(),
   'P2 srodek 700 Hz':punch(mid=700),
   'P3 srodek 1.6 kHz':punch(mid=1600),
   'P4 bez tape-stop':punch(tape=0.0),
   'P5 sam tonalny layer (bez click i sub)':punch(click=0,sub=0)}
for k,y in V.items(): wavfile.write(k+'.wav',SR,y.astype(np.float32))
