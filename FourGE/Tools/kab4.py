# A/B: for each pair -> REF x4, 4GE x4 at 150 BPM (peak-matched, cut on the beat with a 6 ms fade), one mp3
import sys, json, subprocess, numpy as np
from scipy.io import wavfile
sys.argv = [sys.argv[0]] + sys.argv[1:]
exec(open('tools/kmatch4.py').read().split('def main')[0])
SRC = 'match2'
out = 'ab_4ge_m01.mp3'
beat = int(SR * 60 / 150); fade = int(0.006 * SR)
song = []
for n in ['kick2d', 'kickg', 'k84']:
    j = json.load(open(f'{SRC}/{n}.json'))
    ref = load(j['ref']); y = np.load(f'{SRC}/{n}_y.npy')
    for sig in (ref, y):
        s = sig[:beat].copy(); s = np.pad(s, (0, beat - len(s)))
        s[-fade:] *= np.linspace(1, 0, fade)
        s = s / np.abs(s).max() * 0.9
        song.append(np.tile(s, 4))
    song.append(np.zeros(beat * 2))
x = np.concatenate(song)
wavfile.write('ab.wav', SR, (x * 32767).astype(np.int16))
subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', 'ab.wav', '-b:a', '192k', out])
print('ok', len(x) / SR)
