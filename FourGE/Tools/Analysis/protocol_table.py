#!/usr/bin/env python3
"""Group comparison table (protocol section 18) from kick_protocol.py JSON. usage: protocol_table.py <group.json> [...]"""
import sys, json, numpy as np
def first_last_pitch(r):
    tr = [f for t, f in r['pitch_track'] if f > 0]
    return (tr[0] if tr else 0, r['tail_f0'])
for path in sys.argv[1:]:
    rows = json.load(open(path))
    print(f"\n### {path.split('proto_')[-1].replace('.json','')}\n")
    print("| sample | ms | sr/bit | tail Hz (MIDI) | pitch start → settle | TOK | punch: loudest band | rails (max) | clip runs | tail even−odd dB | tail h2/h3/h4 | noise share mids (tail) | SUB |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for r in rows:
        p0, pt = first_last_pitch(r)
        ph = r['phases']; first = ph[0] if ph else {}
        tailph = max(ph, key=lambda q: q['to_ms'] - q['from_ms']) if ph else {}
        ts = tailph.get('tonal_share', [None] * 9)
        mids = [v for v in ts[3:6] if v is not None]
        noise = f"{100 * (1 - np.mean(mids)):.0f} %" if mids else "-"
        h = r['tail_harmonics_db']; hs = f"{h[1]}/{h[2]}/{h[3]}" if len(h) > 3 else "-"
        tok = f"{r['tok']['hz']:.0f} Hz {r['tok']['len_ms']:.0f} ms" if r['tok'] else "NO"
        sub = "YES" if r['sub'] and r['sub']['below_note_db_re_fund'] > -6 else "NO"
        rail = max(q['at_rail'] for q in ph) if ph else 0
        print(f"| {r['file'][:34]} | {r['length_ms']} | {r['sr']//1000}k/{r['subtype'].replace('PCM_','')} | {r['tail_f0']} ({r['tail_note']}) | {p0:.0f} → {r['pitch_settle_ms'] and round(r['pitch_settle_ms'])} ms | {tok} | {first.get('loudest_band','-')} | {100*rail:.0f} % | {r['clip_runs']} | {r['tail_even_minus_odd_db']} | {hs} | {noise} | {sub} |")
