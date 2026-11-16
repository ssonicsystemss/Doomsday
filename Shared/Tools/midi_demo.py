# Mix a kick render and a melody render (both from RenderTest --midi) into one demo with a light sidechain.
# usage: python3 midi_demo.py kicks.wav melody.wav out.mp3   (owner's test MIDI, 2026-11-16: bass line = 4GE kicks, melody = Fortune of Seekers)
import sys,subprocess,numpy as np
from scipy.io import wavfile
def ld(f):
    sr,x=wavfile.read(f); x=x.astype(float)
    if x.ndim==1: x=np.stack([x,x],1)
    return sr,x/max(1e-9,np.abs(x).max())
sr,k=ld(sys.argv[1]); _,m=ld(sys.argv[2]); n=max(len(k),len(m))
K=np.zeros((n,2)); M=np.zeros((n,2)); K[:len(k)]=k[:,:2]; M[:len(m)]=m[:,:2]
env=np.abs(K.mean(1)); a=np.exp(-1/(sr*0.06)); e=np.zeros(n); v=0.0
for i in range(n): v=max(env[i],v*a); e[i]=v
y=K*0.85+M*0.45*(1-0.6*np.clip(e,0,1))[:,None]; y/=np.abs(y).max()*1.02
tmp=sys.argv[3].rsplit('.',1)[0]+'.wav'; wavfile.write(tmp,sr,(y*32767).astype(np.int16))
subprocess.run(['ffmpeg','-y','-loglevel','error','-i',tmp,'-b:a','192k',sys.argv[3]])
