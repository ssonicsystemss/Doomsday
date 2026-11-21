# RAW CRACK REWORK (owner 2026-11-21): TRANSIENT CRACK + CRACK REVERB

`cr_model` TRANSIENT CRACK (default) / CLASSIC LAYER (the old layer below, kept for old projects; its knobs only show in that model).

**TRANSIENT CRACK** (`DSP/Front/Crack.h transient`): ONE short event, 2-25 ms (`cr_width`: 0 = 2.5, 50 % ~ 10, 100 % = 22 ms),
ATTACK 0 / DECAY = length / hard termination. SOURCE `cr_source` IMPULSE / CLICK / SHORT SAW / SHORT SQUARE / NOISE BURST / HYBRID
(default: 70 % impulse + saw, 30 % a very short burst). Own pitch = the punch's pitch x `cr_pitch_off` (+9 st) with a fast fall
`cr_pitch_env` (+12 -> 0 inside the length). PRE GAIN -> WAVESHAPER `cr_ws` SOFT / HARD / ASYMMETRIC (default) / FOLD / RECTIFY
with `cr_asym` -> BANDPASS `cr_bp_freq` / `cr_bp_width` (oct) / `cr_bp_res` (follows the pitch) -> HARD CLIP. `cr_cbody` = CRACK BODY
5-40 ms, lightly distorted. `cr_position`: 0 = the punch start, 1 = the body start. `cr_raw` RAW CRACK macro (non-linear: amplitude,
density, asymmetry, sharpness, a little length). The PUNCH level scales it (PUNCH off = no crack).

**HI EQ** `cr_hi_gain` / `cr_hi_freq`: a high shelf on the whole crack (dry + wet), owner 2026-11-21 ("boost hi eq" on PARALLEL).

**CRACK REVERB** (`DSP/Front/CrackReverb.h`, owner spec "RAW CRACK REVERB ENGINE"): a mono short room (pre-delay, 8 early taps by SIZE,
4 damped combs = DECAY RT60, 2 allpasses = DIFFUSION), then REVERB FILTER (HP / LP / RES) -> REVERB DRIVE (SOFT / HARD / ASYM / FOLD).
`cr_rv_route`: PRE-DISTORTION (source -> pre-EQ -> reverb -> the crack's distortion -> final envelope), POST-DISTORTION (the distorted
crack -> reverb), PARALLEL (default: dry crack untouched + wet). MIX 0 / OFF = the dry crack exactly. GATE cuts the wet at
pre-delay + 0.35 x DECAY (still loud, a gated room). Only the crack, never the tail / master.
**LISTEN** `cr_listen`: KICK / SOLO SOURCE / SOLO DRY / SOLO WET / SOLO REVERB (raw) / SOLO FINAL - the SOLO ones play only the crack.
Owner (2026-11-21): first the 3 routings + SOLO to HEAR whether reverb before distortion makes the crack; no preset tuning yet.
Not yet: stereo width (the crack bus is mono), REVERB ATTACK / RELEASE and envelope-driven MIX / DRIVE, RAW CRACK -> reverb send / drive,
the ROOM CRACK / RAW CRUNCH / METALLIC CRACK starting points.
The tonal CRUNCH (`cn_*`, RAW_CRUNCH.md) is now OFF by default (the owner: "piep", not an industrial crack).

---

# 4GE – RAW PUNCH ENGINE EXTENSION: CRACK + SNAP + BODY COUPLING (owner's specs 2026-11-21)

Owner, after the first RAW PUNCH demos: "trzasku za mało". The aim is the RAW 2015–2017 "KRAK!", reached with an independent harmonic
layer instead of more distortion.

This is an **extension of PUNCH TYPE RAW, not a second RAW engine.**
- It never replaces the punch and never changes BODY / TAIL or the other styles.
- `cr_on` OFF gives the RAW punch exactly as before.
- It never runs with other punch types.

Code: `Source/DSP/Front/Crack.h` (`CrackUnit`); params `cr_*` (enum names `ck*`: `cr*` was taken by CREST); punch tab **RAW CRACK**.

```
RAW PUNCH (existing source, untouched) ─┐
CRACK SOURCE: own saw/square tone ───────┤ CRACK PITCH = FOLLOW PUNCH (+ st against the RAW punch's own pitch) or FIXED, + PITCH MOD
   + a little PUNCH / TRANSIENT / NOISE  │ -> CRACK TONE (low / mid / high) -> CRACK DRIVE (+ type) + DENSITY folds + EDGE -> CRACK CLIP
                                         │ -> its own A / D / S / R
SNAP (parallel): a short hit with a low/mid BODY part, LENGTH 1-15 ms, TONE, EDGE
BODY COUPLING: the end is synced so the crack always reaches into the BODY's start, it decays into it, and the body's onset feeds
               its source (never a copy of the distorted body; the body's envelope is untouched)
-> CRACK LEVEL, a soft peak ceiling -> added AFTER the master clip (the kick's main DRIVE never touches the crack)
```

- **CLASSIC / CRACKING:** the RAW punch type (`rp_type`) sets the layer's character. CRACKING has a harder snap, more density, a
  stronger drive and clip, and a sharper edge.
- **Starting values:** the RAW PUNCH factory presets carry them.
- **RenderTest (spec tests A–G):**
  - OFF = as before; other punch types never get the layer;
  - A: SNAP alone is a hit with a low/mid part, not a bare click;
  - B: CRACK alone is harmonic (spectral flatness 0.07 vs white noise 0.59);
  - C: SNAP + CRACK together;
  - D: BODY COUPLING fills 20–60 ms;
  - E: high CRACK leaves the body unchanged (0.00 dB at 60–200 ms) and the crack has no sustain;
  - F: CLASSIC and CRACKING differ;
  - G: the master clip / tail drive never change the crack;
  - the peak ceiling holds;
  - every `cr_*` control changes the sound.
