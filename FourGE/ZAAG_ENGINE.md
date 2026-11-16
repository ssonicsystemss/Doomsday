# 4GE – ZAAG ENGINE (TAIL TYPE ZAAG) – owner's tutorial, 2026-11-15

Code: `Source/DSP/Tone/ZaagTail.h` (tail), `Source/DSP/Core/ZaagPunch.h` (PUNCH TYPE ZAAG: click, piep, trask, THUMP, punch
reverb). GUI: TAIL → TYPE ZAAG → tabs **ZAAG** / **ZAAG SWEEP / FX** / **ZAAG OUT**. Tests: `Tests/RenderTest.cpp` (ZAAG CASCADE,
ZAAG STEREO, ZAAG KICK-LENGTH SYNC, KICK END = NOTE, 14.11 every knob).

## The most important algorithm

**ZAAG is not a distortion preset. It is a chain of dependent nonlinear processes – every stage changes the signal for the next.**
Strictly serial, **no level matching between the stages** (a louder fundamental after EQ1 must hit DIST 2 harder); one kick envelope
and one pitch move everything together. The only level compensation is at the very end.

```
INPUT → EQ1 → DISTORTION 1 → EQ2 → DISTORTION 2 → EQ3 → UNIQUE FX → DISTORTION 3 → EQ4 → SOFT CLIP → STEREO
```

## Chain (sections 01–10)

| # | section | what | parameters |
|---|---|---|---|
| 01 | SOURCE | LOW SAW on the note + HIGH SAW 3 octaves up (the tutorial's −2 / +1 oct), quiet; fixed, not exposed | `zg_sawb_level` = HIGH SAW LEVEL |
| 02 | FUNDAMENTAL | the current pitch: START st → ROOT in DROP ms (the hit – the only thing in ms) | `zg_start`, `zg_drop`, `zg_curve`, `zg_root` |
| 03 | DISTORTION STACK | DIST 1 maximally aggressive (+ mix), DIST 2, DIST 3 = ZAAG CHARACTER (SOFT / HARD / WAVE / FOLD / ASYMMETRIC / DIGITAL / ZERO-SQUARE, preamp, asym, mix); the DISTORTION ENVELOPE lowers all drives over the kick | `zg_s1_drive / _curve / _asym / _mix`, `zg_s2_drive`, `zg_drive`, `zg_s3_type / _preamp / _asym / _mix`, `zg_dist_env`, `zg_dirt`, `zg_dirt_tone` |
| 04 | MOVING EQ | EQ1 = a dynamic bell ON the current fundamental + a light mid boost; EQ2 = MOVING LOW SHELF: **0 → 100 % over the punch, 100 → 50 % over the tail** | `zg_low_ctrl`, `zg_s1_fund`, `zg_s1_mid_hz`, `zg_s1_mid`, `zg_s1_low_cut`, `zg_eqm_shelf / _depth / _time` |
| 05 | RESONANT SWEEP | EQ3 = a narrow peak, a **dynamic resonant frequency sweep locked to the kick envelope** (not "boost the fundamental"): high → plateau after the punch → second fall, optional REOPEN; in multiples of the fundamental | `zg_peak`, `zg_peak_q`, `zg_sw_start / _plateau / _end / _hold / _reopen` |
| 06 | UNIQUE FX | ZAAG RESONATOR: PHASER / RESONATOR / COMB (x fundamental, follows the sweep), stereo width L≠R, low / moderate MIX ("adds character, does not replace the kick"), POSITION in the cascade | `zg_rsn_mode / _freq / _reso / _width / _mix / _pos` |
| 07 | STEREO ENGINE | ZAAG STEREO: M/S → SIDE HIGH-PASS (48 dB/oct) → SIDE BOOST (a band on the side only) → WIDTH → HIGH REVERB (top band). **LOW → mono, HIGH → width** | `zg_side_cut`, `zg_side_boost_hz`, `zg_side_boost`, `zg_width`, `zg_hverb` |
| 08 | BASS MANAGEMENT | EQ4 = SUB PUSH (low shelf into the clipper = the squeeze), LOW-MID CUT, HIGH ENHANCE; the processed lows replaced by a clean mono SUB added **after** the tail shape (own envelope) | `zg_squash`, `zg_lm_cut`, `zg_final_tone`, `zg_sub_src`, `zg_sub`, `zg_sub_freq`, `zg_sub_decay` |
| 09 | FINAL CLIP | SOFT CLIP (ZAAG CRUSH: soft & round → hard & square). The MASTER waveshaper to 0 dB lives in MASTER (the ZAAG style sets `master_clip_drive`), oversampled, and is not the only level control | `zg_press`, `zg_knee` |
| 10 | KICK-LENGTH SYNC | every movement (shelf, sweep, resonator, distortion envelope) is read at **pos = ms / KICK LENGTH** – `START = KickLength × 0.18`, never "START = 20 ms"; 120 → 220 ms rescales everything | `zg_eqm_time`, `zg_sw_hold` (fractions) |

Tail shape: `zg_attack`, `zg_body`, `zg_decay`, `zg_fade`; `zg_len_follow` / `zg_length`.

## Macros (0.5 = neutral)

ZAAG (`zg_zaag`: everything at once) · ZAAG DRIVE (`zg_grit`) · ZAAG RESONANCE · ZAAG MOVEMENT · ZAAG BASS (sub ↔ mids) ·
ZAAG STEREO (only the safe part) · ZAAG CRUSH · ZAAG CHARACTER (`zg_character`: SOFT blended into the DISTORTION TYPE).

## Variants (`zg_variant`)

CLASSIC (the tutorial chain) · RESONANT (+6 dB / narrower sweep, reopens) · BOUNCE (the shelf and the sub breathe 3x over the
tail, a sharper pitch hit) · AGGRESSIVE (+6 dB drive, character pushed towards the type, harder crush). Recipe constants in
`ZaagTail.h::cook`. Later (listening round 2026-11-16): LONG, CLEAN, INDUSTRIAL. FAKEDROP dropped (owner 2026-11-16).

## PUNCH TYPE ZAAG (ZaagPunch.h)

CLICK (2 ms) · PIEP (a pip falling to half) · TRASK (falling saw x3, 700 Hz – TRASK TONE) · **PUP** (owner 2026-11-15: the raw-style
click – a driven saw diving PUP DIVE octaves over PUP LENGTH, "pup" at 1 kHz; `zg_pup / _hz / _ms / _oct / _drive`) · THUMP (a
driven low hit falling to the note) · PUNCH REVERB on the click / piep / trask / pup (send, pre-delay, gate, brightness, low cut).
Everything goes through the MASTER clip (unlike the TRANSIENT module on the MASTER BUS, which bypasses it).

## Factory presets (owner's picks, 2026-11-15, 155 BPM rounds)

ZAAG - Resonant Laser (= X7, his favourite = the ZAAG style default) · ZAAG - Hard Saw (A1) · ZAAG - Zero Square (A2) ·
ZAAG - Laser (A5) · ZAAG - Classic Wave (W5) · ZAAG - Resonant Phaser (N2 between "dirtier" and "more movement").
Level: MASTER CLIP + SAFETY CLIP (not LIMITER 1: a loud PUP makes the limiter duck the whole hit and kill the low start).

## Rules

- Not FL names (Destructor / Hammer Classic / Wide Cabinet) – own names.
- Lows always mono (test: side < −40 dB under 60 Hz with every stereo control at max).
- FOLD / DIGITAL are never picked by the randomizer by itself.
- Retired (kept for old projects, no sound, not on any page): `zg_saw`, `zg_sawb_oct`, `zg_sawb_detune`, `zg_s2_low / _low_mid /
  _mid / _hi_mid`, `zg_bits`, `zg_fold`, `zg_tone`, `zg_focus`, `zg_flt_*` (the old filter movement – the resonant sweep replaces it).
- KICK END = NOTE (default since 2026-11-15): a shorter note in the piano roll **cuts** the kick (5 ms), it never shortens it –
  KICK LENGTH stays and the ZAAG movements still scale with KICK LENGTH, not with the note.
