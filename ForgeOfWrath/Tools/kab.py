# A/B audio: for every tuned kick -> 4 bars at 150 BPM: REF x4, FORGE x4 (peak-matched), all in one mp3
import sys, os, json, subprocess, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from kmatch import *
from scipy.io import wavfile
order = sys.argv[2:]; out = sys.argv[1]
beat = int(SR * 60 / 150)
lines = []; refs = []
for n in order:
    j = json.load(open(f'{os.path.dirname(__file__)}/../tune/{n}.json')); t = Target(j['ref'])
    lines.append(jobline(n, j['note'], j['params'], min(t.len + 0.05, 1.35))); refs.append(t.x)
render(lines, SHM + '/ab')
song = []
for n, r in zip(order, refs):
    y = load(f'{SHM}/ab/{n}.wav')
    for sig in (r / np.abs(r).max() * 0.9, y / np.abs(y).max() * 0.9):
        bar = np.zeros(beat * 4)
        for k in range(4):
            s = sig[:beat]; bar[k * beat:k * beat + len(s)] += s
        song.append(bar)
    song.append(np.zeros(beat * 2))
x = np.concatenate(song)
wavfile.write(SHM + '/ab.wav', SR, (x * 32767).astype(np.int16))
subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', SHM + '/ab.wav', '-b:a', '192k', out])
print('ok', len(x) / SR, 's')
