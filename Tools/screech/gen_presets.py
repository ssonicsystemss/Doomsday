# turns tuned JSON settings into C++ factory presets for PluginProcessor.cpp
import json, sys, os
D = os.path.join(os.path.dirname(__file__), '..', 'presets')
# (json, preset name, reference note) - octave shift puts the reference register around MIDI 60..72
LIST = [
 ('massive_wail', 'Wail of Souls'), ('house_howl', 'Howling Prophet'), ('serum_rise', 'Ascension'),
 ('sylenth_talk', 'Talking Dead'), ('dive_shot', 'Diving Blade'), ('comb_dive', 'Comb of Doom'),
 ('rise_scream', 'Rising Scream'), ('gate_wah', 'Wah of the Damned'), ('stutter_rise', 'Stutter Ritual'),
 ('raw_burst', 'Raw Burst'), ('tritone_fall', 'Tritone Fall FX'),
]
vol = json.load(open(os.path.join(D, 'volumes.json'))) if os.path.exists(os.path.join(D, 'volumes.json')) else {}
out = []
for k, name in LIST:
    d = json.load(open(os.path.join(D, k + '.json')))
    note = int(open(os.path.join(D, k + '.args')).read().split()[1])
    shift = round((note - 66) / 12)
    d = dict(d)
    if shift:
        d['o1_oct'] = d.get('o1_oct', 0) + shift
        if d.get('o2_on'): d['o2_oct'] = max(-3, min(3, d.get('o2_oct', 0) + shift))
    # musical finish: a little reverb unless the preset already has its own
    if not d.get('rev_on'): d.update(rev_on=1, rsize=0.6, rdamp=0.5, rmix=0.12)
    d.setdefault('dly_on', 0)
    if k in vol: d['volume'] = vol[k]
    items = ", ".join(f'{{ "{p}", {v}f }}' if isinstance(v, float) else f'{{ "{p}", {v} }}' for p, v in sorted(d.items()))
    out.append(f'            {{ "{name}", {{ {items} }} }},')
print("\n".join(out))
json.dump({k: [name, int(open(os.path.join(D, k + '.args')).read().split()[1]) - 12 * round((int(open(os.path.join(D, k + '.args')).read().split()[1]) - 66) / 12)] for k, name in LIST},
          open(os.path.join(D, 'play_notes.json'), 'w'))
