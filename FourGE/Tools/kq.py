# quality report: density (spectral flatness 1-8 kHz in the tail), harsh (narrow peak 1.5-6 kHz over the smoothed spectrum),
# bass entry (ms when 30-90 Hz reaches -6 dB of its max), tok->tail contrast (dB, RMS 20-80 ms vs 150-300 ms), top (>2 kHz share)
import numpy as np, sys
from scipy.io import wavfile
from scipy.signal import butter, sosfilt
def load(f):
    sr,x=wavfile.read(f)
    if x.dtype==np.int16: x=x/32768.
    elif x.dtype==np.int32: x=x/2147483648.
    x=x.astype(float); x=x if x.ndim==1 else x.mean(1)
    i=np.argmax(np.abs(x)>0.003); return sr,x[i:]
def rep(f):
    sr,x=load(f)
    env=np.array([np.sqrt(np.mean(x[k:k+int(.005*sr)]**2)) for k in range(0,len(x)-int(.005*sr),int(.005*sr))])
    L=np.where(20*np.log10(env/env.max()+1e-9)>-40)[0][-1]*0.005
    seg=x[int(.15*sr):int(max(.16,min(L*0.8,.5))*sr)]
    N=1<<14; s=np.abs(np.fft.rfft(seg[:N]*np.hanning(min(N,len(seg))) if len(seg)>=N else np.pad(seg*np.hanning(len(seg)),(0,N-len(seg)))))+1e-12
    fr=np.fft.rfftfreq(N,1/sr); m=(fr>1000)&(fr<8000)
    flat=10*np.log10(np.exp(np.mean(np.log(s[m]**2)))/np.mean(s[m]**2))
    db=20*np.log10(s); k=int(200/(sr/N)); sm=np.convolve(db,np.ones(k)/k,'same'); m2=(fr>1500)&(fr<6000)
    harsh=(db-sm)[m2]; harsh=np.percentile(harsh,99.5)
    lo=sosfilt(butter(4,[30,90],'bandpass',fs=sr,output='sos'),x); le=np.array([np.sqrt(np.mean(lo[k:k+int(.005*sr)]**2)) for k in range(0,len(lo)-int(.005*sr),int(.005*sr))])
    entry=np.argmax(le>le.max()*0.5)*5
    r=lambda a,b: 20*np.log10(np.sqrt(np.mean(x[int(a*sr):int(b*sr)]**2))+1e-9)
    hi=sosfilt(butter(4,2000,'highpass',fs=sr,output='sos'),x[:int(L*sr)]); top=10*np.log10(np.mean(hi**2)/np.mean(x[:int(L*sr)]**2))
    return L*1000,flat,harsh,entry,r(.02,.08)-r(.15,.3),top
print('%-36s %6s %7s %6s %6s %7s %6s'%('file','len','dense','harsh','bass@','tokctr','top2k'))
for f in sys.argv[1:]:
    L,fl,h,e,c,t=rep(f); print('%-36s %6.0f %7.1f %6.1f %6.0f %7.1f %6.1f'%(f.split('/')[-1][-36:],L,fl,h,e,c,t))
