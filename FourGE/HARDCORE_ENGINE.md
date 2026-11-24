# 4GE - HARDCORE KICK ENGINE (owner spec 2026-11-24)

GABBER / MILLENNIUM / HARDCORE / UPTEMPO = **profiles of ONE engine** (spec 52): TAIL TYPE **HARDCORE** (`tail_engine` 5,
`DSP/Tone/HardcoreKick.h`) makes the whole kick (like ZAAG: the punch / body / sub generators are silent in the profiles), and the
4 new STYLES (`Presets/StyleEngine.h`, appended after ZAAG) only set its parameters. Nothing of the other styles changed.

## Chain (`hc_*`)
SOURCE `hc_src` 909 / 909 HARD / BD ANALOG / SINE / SINE + 909 / HYBRID (`hc_src_mix`)
-> PITCH: END = the note (`hc_key_mode` FOLLOW MIDI) or a FIXED KEY (`hc_root_note`) + `hc_end` st + `hc_fine` ct; START = END + `hc_drop`
   (0-48 st) in `hc_time` (1-100 ms), `hc_curve` LINEAR / EXP / FAST DROP / S-CURVE / CUSTOM (`hc_curve_amt`)
-> PUNCH / BODY / TAIL relation: `hc_punch_len` / `hc_body_len` + `hc_punch_db` / `hc_body_db` / `hc_tail_db` = how hard each segment
   drives the chain (the relation, not a volume); the tail fades by `hc_tail_decay` over the length; `hc_attack`
-> PRE-EQ LOW / LOW-MID / MID / HIGH-MID / HIGH (`hc_eq_*_db` + `_hz`) -> DRIVE 1 `hc_d1` (tube / soft) -> EQ between (`hc_btw_hz` / `_db`)
-> [the tonal CRUNCH joins here: clipped together with the kick] -> DRIVE 2 `hc_d2` (hard) -> RESAMPLE FEEL `hc_resample`
   (internal normalisation + a second pass) -> ASYMMETRY `hc_asym` + `hc_bias` -> CLIP `hc_clip_mode` HARD / SOFT / FOLD / ASYMMETRIC /
   CUSTOM + `hc_clip` -> POST-EQ LOW CUT / BODY / MID / CRUNCH (2-6 kHz band, `hc_post_crunch_hz`) / HIGH.
`hc_stages` 1 = DRIVE 1 -> CLIP, 2 = + DRIVE 2, 3 = + ASYMMETRY (spec 54 test matrix).
CRUNCH (tonal, never noise): `hc_cr_on`, `hc_cr_src` 909 / BD, SINE, SINE + 909, SAW; `hc_cr_start` / `hc_cr_len`; pitch `hc_cr_pmode`
OWN (`hc_cr_pitch` + `hc_cr_pamt` falling in `hc_cr_ptime`) / FOLLOW PUNCH / FOLLOW BODY; `hc_cr_drive` (saturation), `hc_cr_asym`, `hc_cr_mix`.
TEXTURE (optional, max 30 %): `hc_tx_type` NOISE / CRUSHED NOISE / SHORT TRANSIENT, `hc_tx_mix`, `hc_tx_len`.
LENGTH: `hc_len_mode` KICK LENGTH or BPM GRID (`hc_grid` 1/16-1/2 x `hc_fill`): the kick never blocks the next one (uptempo).
BURST `hc_burst`: OFF / SHORT / PITCHED / HEAVY / BY VELOCITY (<0.4 normal, <0.6 short, <0.8 pitched, else heavy) - MIDI / automation, never random.
ANALYSIS line under the HARDCORE tabs (spec 51): style, source, punch / body / crunch / tail ms, pitch drop, key, stages, asymmetry, clip.

## Profiles (starting points, spec 46-49)
| | GABBER | MILLENNIUM | HARDCORE | UPTEMPO |
|---|---|---|---|---|
| source | 909 | 909 | SINE + 909 | SINE + 909 (35 %) |
| pitch | -24 st / 30 ms EXP | -28 / 35 EXP | -32 / 22 FAST | -36 / 12 FAST |
| punch / body | 25 / 120 ms | 40 / 140 | 35 / 100 | 30 / 60 |
| crunch | 909, 5-155 ms, +12 FOLLOW BODY, dark | 909, 15-135, OWN +7, heavy | SINE+909, 8-98, FOLLOW PUNCH | SINE+909, 5-65, OWN +19, bright |
| drive 1 / 2 | 18 / 24 dB | 20 / 26 | 16 / 26 | 18 / 30 |
| asym / clip | 0.45, HARD 8 dB | 0.7, ASYMMETRIC 9 | 0.7, HARD 10 | 0.7, CUSTOM 12 |
| resample | 0.5 | 0.3 | 0.2 | 0.3 |
| length | 95 % of a beat | 85 % | 75 % | 55 % |
| post | highs -3, body +2 | dark (-4), body +3 | crunch +2 | crunch band +5 @ 4 kHz, highs +2 |
Presets: "GABBER - Crushed 909", "MILLENNIUM - Heavy Dark", "HARDCORE - Aggressive", "UPTEMPO - Forward", "UPTEMPO - Burst By Velocity".

## Not yet (next steps)
The 8 MASTER MACROS mapping per hardcore profile (spec 44), the KICK VARIATION style tables for the 4 new styles, a PITCH CURVE drawn
by hand (CUSTOM = an exponent today), the full spec 54 matrix (5 sources x 4 profiles x 4 asym x 3 stages) as listening files is
rendered as a subset (see the session). Next profiles (spec 56): FRENCHCORE / PEEP / BOUNCY UPTEMPO / ... = more profiles of this engine.

## 2026-11-24 (owner: "basic, nudny") - MID SCREAM + OUTPUT CLIP, profiles fitted to the owner's refs
- MID SCREAM `hc_ms_*`: a 2nd tone (FIXED HZ or a HARMONIC of the note; SINE / TRI / SAW / SQUARE; SWEEP st + time, START, FADE IN,
  MOTION +-1 st at RATE) added x the segment gain BEFORE the pre EQ - distorted together with the note.
- OUTPUT CLIP `hc_out_clip`: the last stage, after the post EQ (flat tops back).
- Profiles fitted (band energies / crest / centroid / level drop) to: HARDCORE = 10 OPS hardcore kicks, MILLENNIUM = 8 Tritone,
  UPTEMPO = 2 uptempo, GABBER = 2 gabber (refs are the owner's, not in git).
- LAYER ENERGY: `DSP/Analysis/LayerEnergy.h` (TOK / PUNCH / BODY / CRUNCH / SUB / TAIL / SCREAM, total + 5 bands),
  `RenderTest <out.txt> --energy <preset | factory:name> [note] [id=value]`.

## 2026-11-24 (later) - HARDCORE rebuilt from ONE ref (OPS HARDCORE KICK 31_G), owner: "w miare podobnie", variants 2 / 3 best
Measured on the ref (it is a hard-clipped square: only where the pre-clip wave changes sign matters):
- 0-8 ms a HELD ~2 kHz tone (a tok), then a straight drop (linear in semitones, ~1.9 st / ms) to the note G1 by ~40 ms = +64 st.
- the tail = the note squared to the rails + a 2nd tone at its ~12th harmonic (539 / 588 / 637 Hz sidebands = locked to the note,
  never a fixed "piep"), strong at 20-80 ms, almost gone at ~85-150 ms (a clean note square = the DUM), back from ~160 ms to the end.
  The same dip pattern is in all 10 OPS hardcore refs (zero crossings per 20 ms).
New params (append only, all off by default): `hc_hold` PITCH HOLD, `hc_drop_x` DROP + (beyond 48 st), `hc_ms_dip` / `_at` / `_len`.
Profile HARDCORE (StyleEngine) = this kick: SINE source, drop 48 + 16 st / 30 ms LINEAR, hold 8 ms, MID SCREAM HARMONIC 12 x 0.65 from
19.5 ms, DIP 0.6 at 85 ms for 70 ms, DRIVE 1 / 2 24 dB, 2 stages, SOFT clip 6 dB, low cut 10 Hz, length = 98 % of a beat (BPM grid),
MASTER SAFETY CLIP on + master +4 dB in the presets (the final squaring at the host rate: no oversampling overshoot on the edges).
Presets: "HARDCORE - OPS 31" (the rebuild) and "HARDCORE - Aggressive" (more 2nd tone 0.75, shallower dip 0.4).
Tools (throw-away, /tmp): zero-crossing pitch, crossings / rail share per 20 ms, 5 bands per window, a Python sketch before C++.
Still different by numbers: the ref has ~3 dB more 20-400 Hz and rounded humps after each switch (not a pure square).
