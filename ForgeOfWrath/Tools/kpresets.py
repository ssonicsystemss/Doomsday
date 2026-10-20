# writes ForgeOfWrath/Source/FactoryPresets.inc from the tuned json files + hand-made presets
import json, os, sys
T = os.path.join(os.path.dirname(__file__), '..', 'tune')
KEYS = {28: 'E', 29: 'F', 30: 'F#', 31: 'G', 32: 'G#', 33: 'A', 34: 'A#', 35: 'B', 36: 'C'}
TUNED = [  # json, name
    ('nc', 'RAW - Rusted Chainsaw'), ('ws', 'RAW - Unchained Beast'), ('zt', 'RAW - Tribal Fury'), ('vul', 'RAW - Volcanic Eruption'), ('gal', 'RAW - Galactic Roar'),
    ('f009', 'HARD - Forged Steel'), ('f019', 'HARD - Old Skool Pick'), ('f027', 'HARD - Old Skool Hammer'), ('hs04', 'HARD - Iron Fist'),
    ('k32', 'EUPHORIC - Classic Thump'), ('k55', 'EUPHORIC - Bright Thump'), ('k77', 'EUPHORIC - Deep Thump'), ('hk10', 'EUPHORIC - Clean Hammer'),
    ('h09', 'EUPHORIC - Short Tok'), ('s25', 'EUPHORIC - Soft Pillow'),
    ('rb02', 'REVERSE - Rising Wrath'), ('rb24', 'REVERSE - Undertow'), ('rb32', 'REVERSE - Backdraft'), ('rb40', 'REVERSE - Long Rise'),
    ('gt08', 'SPECIAL - Double Strike'),
    ('z9', 'ZAAG - Saw Blade'), ('z10', 'ZAAG - Chainsaw Massacre'), ('z15', 'ZAAG - Iron Saw'), ('z22', 'ZAAG - Buzz Cutter'),
    ('szp', 'RAW - Szpila'), ('mass', 'RAW - Mass Destruction'), ('rawp', 'RAW - Powerhouse'), ('raws', 'RAW - Slow Burn'), ('tric', 'RAW - Crunch Titan'),
    ('k73', 'HARD - Seventh Seal'), ('elec', 'HARD - Electro Blaze'), ('revc', 'HARD - Cathedral'), ('tri4', 'EUPHORIC - Short Strike'),
    ('psy5', 'PSY - Mind Bender'), ('psy8', 'PSY - Third Eye'), ('rbg', 'REVERSE - Midnight Rise'),
    ('upp', 'UPTEMPO - Piercing Slide'), ('upr', 'UPTEMPO - Raw Rocket'),
]
IDS_EXTRA = ['output']
IDS = ['tail_level', 'tail_start', 'tail_hold', 'chaos', 'chaos_dec', 'chaos_hz', 'tail_drop', 'tail_curve', 'tail_wave', 'dist_type', 'drive', 'tone', 'low_cut', 'high_cut', 'body', 'grit', 'scream', 'scream_hz',
       'crunch', 'crunch_dec', 'punch_level', 'punch_start', 'punch_end', 'punch_drop', 'punch_decay', 'click_level', 'click_hz', 'click_len', 'length', 'clip_drive', 'clip_soft']

def num(v):
    v = float(v); a = abs(v)
    d = 0 if a >= 100 else 1 if a >= 10 else 2 if a >= 1 else 3
    t = f'{round(v, d):.{d}f}' if d else f'{round(v):.0f}.'
    t = t.rstrip('0') if '.' in t else t
    return (t + '0' if t.endswith('.') else t) + 'f'

def entry(name, p, amp, drv):
    vals = ', '.join(f'{{ "{k}", {num(p[k])} }}' for k in IDS + IDS_EXTRA if k in p)
    return f'        L.push_back ({{ "{name}", {{ {vals} }},\n                        "{amp}", "{drv}" }});'

def rnd(s):  # shorten the shape strings
    return ';'.join(f'{float(a):g}:{round(float(b), 3):g}' for a, b in (t.split(':') for t in s.split(';')))

out = ['// Factory presets. TUNED ones were matched by ear-model (spectrogram + pitch + envelope) to reference kicks;',
       '// the key in [ ] is the note they were matched on (play that note for the original pitch).', 'namespace', '{',
       '    std::vector<KickPreset> buildPresets()', '    {', '        std::vector<KickPreset> L;',
       '        L.push_back ({ "INIT - Forge Default", {}, "", "" });']
sys.path.insert(0, os.path.dirname(__file__))
from kmatch import render, jobline, load, SHM
import numpy as np
jobs = {}
TUNED = [(k, n) for k, n in TUNED if os.path.exists(os.path.join(T, k + '.json'))]
CAT = ['RAW', 'HARD', 'EUPHORIC', 'REVERSE', 'ZAAG', 'PSY', 'UPTEMPO', 'SPECIAL']
TUNED.sort(key=lambda kn: (CAT.index(kn[1].split(' - ')[0]), kn[1]))
for key, name in TUNED:
    j = json.load(open(os.path.join(T, key + '.json'))); jobs[key] = j
render([jobline(k, j['note'], j['params'], 2.0) for k, j in jobs.items()], SHM + '/lvl')
for key, name in TUNED:
    j = jobs[key]; p = dict(j['params'])
    p['dist_type'] = int(p['dist_type'])
    pk = np.abs(load(f'{SHM}/lvl/{key}.wav')).max()
    p['output'] = float(np.clip(20 * np.log10(0.9 / pk), -24, 24))
    out.append(entry(f'{name} [{KEYS.get(j["note"], "?")}]', p, rnd(p['amp']), rnd(p['drive_env'])))
# hand-made tools
out.append(entry('TOOL - Pure Sub Tail', dict(tail_start=2.5, tail_drop=40, tail_curve=4, tail_wave=0, dist_type=0, drive=2, tone=0, low_cut=15, high_cut=3000, punch_level=-36, click_level=-60, length=450, clip_drive=0, clip_soft=1),
                 '0:1;850:0.9;970:0.3;1000:0', '0:1;1000:1'))
out.append(entry('TOOL - Punch + Click Only', dict(tail_level=-24, punch_level=0, punch_start=600, punch_end=70, punch_drop=8, punch_decay=60, click_level=-10, click_hz=6000, click_len=3, length=250, clip_drive=3),
                 '0:1;800:0.9;1000:0', '0:1;1000:1'))
out.append(entry('TOOL - Reverse Bass Builder', dict(tail_start=3, tail_drop=50, dist_type=2, drive=30, tone=0.3, high_cut=9000, grit=0.15, punch_level=-4, click_level=-24, length=450, clip_drive=6, clip_soft=0.2),
                 '0:1;150:0.55;300:0.75;900:1;970:0.6;1000:0', '0:0.25;250:0.1;600:1;1000:1'))
out += ['        return L;', '    }', '}', '']
dst = '/home/claude/doomsday/ForgeOfWrath/Source/FactoryPresets.inc'
open(dst, 'w').write('\n'.join(out))
print('wrote', len(out), 'lines')
