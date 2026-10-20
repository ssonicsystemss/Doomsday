# render a PoS candidate and compare it with a reference
import sys, os, subprocess, numpy as np, pickle
sys.path.insert(0, os.path.dirname(__file__))
from feat import analyse
R = '/home/claude/doomsday/build/RenderTest_artefacts/Release/RenderTest'
OUT = os.path.join(os.path.dirname(__file__), '..', 'tune')
def render(name, note, params, hold=1.6):
    args = [R, OUT, 'held', str(note), 'prog=-1', 'name=' + name, f'hold={hold}'] + [f'{k}={v}' for k, v in params.items()]
    subprocess.run(args, check=True, capture_output=True)
    return os.path.join(OUT, name + '.wav')
def vec(r):
    return np.array(r['bands'] + [np.log2(r['cen']) * 10, r['flat'] * 40, r['corr'] * 10])
def report(r, ref=None):
    s = f"{r['name'][:28]:28s} cen {r['cen']:5.0f} flat {r['flat']:.2f} corr {r['corr']:5.2f} crest {r['crest']:4.1f} f0 {r['f0']:6.1f} | " + " ".join(f"{v:4.1f}" for v in r['bands'])
    if ref is not None: s += f"  | dist {np.linalg.norm(vec(r) - vec(ref)):.1f}"
    print(s)
