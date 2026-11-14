# CRUNCH PATH with the owner's EQ spec (2026-11-14): PRE-EQ (HPF 300 + bell 1.8k) -> waveshaper -> POST-EQ (HPF 600, presence bell
# moving with CRUNCH, DYNAMIC control 3.5-4.5k, LPF moving DOWN with CRUNCH) -> crunch envelope; 3 macros AMOUNT / COLOR / AIR
import numpy as np; from scipy.io import wavfile; from scipy.signal import butter,sosfilt
exec(open('macro2.py').read().split('ref=None')[0])
def filt(x,kind,fc,order=2): return sosfilt(butter(order,fc,kind,fs=SR,output='sos'),x)
def envf(x,ms): a=np.exp(-1/(ms*0.001*SR)); return sosfilt(np.array([[1-a,0,0,1,-a,0]]),np.abs(x))
def crunch_path(src,amount,color=0.5,air=0.5):
    x=filt(src,'high',300,3)                                         # PRE: HPF 300 Hz, 18 dB/oct (no sub / low body into the drive)
    x=bell(x,1800,2+2*amount,1.0)                                   # pre-emphasis +2..+4 dB @ 1.8 kHz
    x=asymr(x,18+24*amount,0.7-0.45*amount)                          # WAVESHAPER: drive + asymmetry from AMOUNT
    x=filt(x,'high',600,2)                                           # POST: HPF ~600 (leave room for BODY)
    pres=(2000+2500*amount)*(0.8+0.4*color)                          # PRESENCE: 2 -> 4.5 kHz with AMOUNT, COLOR shifts it
    x=bell(x,pres,2+4*amount,0.9)                                    # +2..+6 dB, wide
    b=bell(x,4000,0,1)*0; bp=filt(filt(x,'high',3500,2),'low',4500,2)   # DYNAMIC: the 3.5-4.5k band only when it is too strong
    r=np.clip(1-0.35*envf(x,5)/(envf(bp,5)+1e-9),0,0.7); x=x-r*bp
    lpf=(6500-1250*amount)*(0.75+0.5*air)                            # LPF moves DOWN with AMOUNT (density, not hiss); AIR opens it
    return filt(x,'low',lpf,2)
def kick(amount,color=0.5,air=0.5):
    mid=osc([(0,650),(20,620),(45,450),(80,49)])*fade; bass=osc([(0,160),(40,55),(80,49)])*fade
    src=mid+0.8*bass; clean=asymr(src,14,0.45)
    if amount<=0: return clean
    peak,end=6+4*amount,25+35*amount
    c=crunch_path(src,amount,color,air); c*=rms(clean,0,end)/rms(c,0,end)
    e=env(peak,end)*min(1,1.2*amount); return (1-e)*clean+e*c
V=[('E000 AMOUNT 0',kick(0)),('E025 AMOUNT 25',kick(.25)),('E050 AMOUNT 50',kick(.5)),('E075 AMOUNT 75',kick(.75)),('E100 AMOUNT 100',kick(1)),
   ('E5C0 AMOUNT 50 COLOR nisko',kick(.5,0)),('E5C1 AMOUNT 50 COLOR wysoko',kick(.5,1)),('E5A0 AMOUNT 50 AIR 0',kick(.5,.5,0)),('E5A1 AMOUNT 50 AIR 100',kick(.5,.5,1))]
ref=None
for k,y in V:
    y=hp(y)
    if ref is None: ref=rms(y,0,150); g=0.5/np.abs(y).max()
    y*=ref/rms(y,0,150)*g; wavfile.write(k+'.wav',SR,np.stack([y,y],1).astype(np.float32))
sr,r=wavfile.read('/mnt/user-data/attachments/58b64e7f-1af8-5704-b9cc-8cf07f46953c/6bd3d6bc-FHSP_Hardstyle_Kicks_009_-_Note_G1.wav'); r=r.astype(float); r=r if r.ndim>1 else np.stack([r,r],1)
i0=np.argmax(np.abs(r).max(1)>0.02*np.abs(r).max()); r=r[i0:]; r*=ref*g/np.sqrt(np.mean(r[:int(0.15*sr)]**2)); wavfile.write('EREF FHSP 009.wav',sr,r.astype(np.float32))
