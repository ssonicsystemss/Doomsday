#!/usr/bin/env python3
# Reference MIDI analysis (4TUNE melody rules, 2026-11-18): onset grid per bar, top voice + bass (lower case).
# usage: python3 midi_grid.py <file.mid> [bars]   (no dependencies)
import struct,sys
def vlq(d,i):
    v=0
    while True:
        b=d[i];i+=1;v=(v<<7)|(b&0x7f)
        if b<0x80:return v,i
def parse(path):
    d=open(path,'rb').read()
    assert d[:4]==b'MThd'
    fmt,ntr,div=struct.unpack('>HHH',d[8:14]);i=14
    tracks=[];tempo=None
    while i<len(d):
        tag=d[i:i+4];ln=struct.unpack('>I',d[i+4:i+8])[0];i+=8
        if tag!=b'MTrk':i+=ln;continue
        t=d[i:i+ln];i+=ln;j=0;tick=0;rs=0;name='';on={};notes=[]
        while j<len(t):
            dt,j=vlq(t,j);tick+=dt;b=t[j]
            if b==0xff:
                ty=t[j+1];l,j2=vlq(t,j+2);data=t[j2:j2+l];j=j2+l
                if ty==3:name=data.decode('latin1')
                if ty==0x51 and tempo is None:tempo=60e6/int.from_bytes(data,'big')
                continue
            if b in(0xf0,0xf7):l,j2=vlq(t,j+1);j=j2+l;continue
            if b&0x80:rs=b;j+=1
            st=rs&0xf0
            if st in(0xc0,0xd0):j+=1;continue
            a,v=t[j],t[j+1];j+=2
            if st==0x90 and v>0:on.setdefault(a,[]).append(tick)
            elif st in(0x80,0x90):
                if on.get(a):s=on[a].pop(0);notes.append((s/div,(tick-s)/div,a))
        notes.sort();tracks.append((name,notes))
    return div,tempo,tracks
import struct,sys
from collections import defaultdict
NN='C C# D D# E F F# G G# A A# B'.split()
f=sys.argv[1];nb=int(sys.argv[2]);div,tempo,tr=parse(f)
ns=sorted(n for _,x in tr for n in x)
low=min(p for _,_,p in ns)
on=defaultdict(list)
for s,d,p in ns:on[round(s*12)].append(p)
for b in range(nb):
    out=[]
    for k in range(48*b//1, 48*(b+1)):pass
    toks=[]
    for t in range(b*48,(b+1)*48):
        if t in on:
            ps=sorted(on[t]);top=ps[-1]
            lab=NN[top%12]+str(top//12-1)
            if top<low+12: lab=lab.lower()
            toks.append(f"{(t-b*48)/12:.2f}:{lab}")
    print(f"bar{b+1:2d} "+' '.join(toks))
