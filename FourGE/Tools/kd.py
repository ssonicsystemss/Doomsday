# how different kicks are: a log spectrogram of the first 400 ms (24 log bands x 16 frames, level-normalised) -> mean |dB| distance
import numpy as np, sys
from scipy.io import wavfile
def load(f):
    sr,x=wavfile.read(f); x=x.astype(float)/{np.dtype('int16'):32768.,np.dtype('int32'):2147483648.}.get(x.dtype if False else wavfile.read(f)[1].dtype,1.)
    x=x if x.ndim==1 else x.mean(1); i=np.argmax(np.abs(x)>0.003); return sr,x[i:]
def feat(f):
    sr,x=load(f); N=2048; hop=int(0.025*sr); edges=np.geomspace(30,16000,25); fr=np.fft.rfftfreq(N,1/sr); F=[]
    for k in range(16):
        s=x[k*hop:k*hop+N]; s=np.pad(s,(0,N-len(s))) if len(s)<N else s
        p=np.abs(np.fft.rfft(s*np.hanning(N)))**2
        F.append([10*np.log10(p[(fr>=edges[b])&(fr<edges[b+1])].sum()+1e-12) for b in range(24)])
    F=np.array(F); return np.maximum(F-F.max(),-60)
fs=sys.argv[1:]; X=[feat(f) for f in fs]
n=len(fs); D=np.zeros((n,n))
for i in range(n):
    for j in range(n): D[i,j]=np.mean(np.abs(X[i]-X[j]))
names=[f.split('/')[-1][:-4][-22:] for f in fs]
print('%-22s'%''+''.join('%6d'%i for i in range(n)))
for i in range(n): print('%-22s'%names[i]+''.join('%6.1f'%D[i,j] for j in range(n)))
off=D[np.triu_indices(n,1)]; print('min %.1f  mean %.1f'%(off.min(),off.mean()))
