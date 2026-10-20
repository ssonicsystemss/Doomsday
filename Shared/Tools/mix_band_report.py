import sys, numpy as np
from scipy.io import wavfile
names = sys.argv[2].split(',') if len(sys.argv) > 2 else None
sr, x = wavfile.read(sys.argv[1]); x = x[:,0].astype(float)
beat = int(sr*60/150); seg = beat*4
bands = [(20,150),(150,600),(600,2000),(2000,5000),(5000,10000),(10000,20000)]
print(f"{'preset':20s} " + " ".join(f"{a}-{b}".rjust(9) for a,b in bands) + "  centroid  flat2-8k")
for i in range(len(x)//seg):
    h = x[i*seg + int(0.06*sr): i*seg + int(0.40*sr)]
    w = np.hanning(len(h)); S = np.abs(np.fft.rfft(h*w))**2; f = np.fft.rfftfreq(len(h), 1/sr)
    tot = S[(f>20)].sum()
    r = [S[(f>=a)&(f<b)].sum()/tot*100 for a,b in bands]
    cen = (S*f).sum()/S.sum()
    m = (f>2000)&(f<8000); sm = S[m]+1e-20
    flat = np.exp(np.mean(np.log(sm)))/np.mean(sm)
    n = names[i] if names and i < len(names) else str(i)
    print(f"{n:20s} " + " ".join(f"{v:8.2f}%" for v in r) + f"  {cen:7.0f}  {flat:.3f}")
