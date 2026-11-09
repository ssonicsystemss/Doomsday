# the TOK -> TAIL seam: the biggest jump between neighbouring 6 ms frames in 60-260 ms (level dB and spectral distance dB)
import numpy as np, sys
from scipy.io import wavfile
def load(f):
    sr,x=wavfile.read(f); x=x.astype(float)/{np.dtype('int16'):32768.,np.dtype('int32'):2147483648.}.get(wavfile.read(f)[1].dtype,1.)
    x=x if x.ndim==1 else x.mean(1); i=np.argmax(np.abs(x)>0.003); return sr,x[i:]
def seam(f):
    sr,x=load(f); h=int(.006*sr); N=1024; fr=np.fft.rfftfreq(N,1/sr); edges=np.geomspace(60,12000,13)
    lv=[];sp=[]
    for k in range(int(.05*sr),int(.27*sr),h):
        s=x[k:k+N]; s=np.pad(s,(0,N-len(s)))
        p=np.abs(np.fft.rfft(s*np.hanning(N)))**2
        b=np.array([10*np.log10(p[(fr>=edges[i])&(fr<edges[i+1])].sum()+1e-12) for i in range(12)])
        lv.append(10*np.log10(np.mean(x[k:k+h]**2)+1e-12)); sp.append(b-b.max())
    lv=np.array(lv); sp=np.array(sp)
    dl=np.abs(np.diff(lv)); ds=np.mean(np.abs(np.diff(sp,axis=0)),axis=1)
    i=np.argmax(ds); return dl.max(), ds.max(), 50+i*6
print('%-36s %7s %7s %6s'%('file','dLvl','dSpec','at ms'))
for f in sys.argv[1:]:
    a,b,c=seam(f); print('%-36s %7.1f %7.1f %6d'%(f.split('/')[-1][-36:],a,b,c))
