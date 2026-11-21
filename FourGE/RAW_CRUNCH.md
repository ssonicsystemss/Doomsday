# RAW CRUNCH - CRACK SOURCE REWORK (owner spec 2026-11-21, the current one)

A short TONAL layer, never noise / random impulses (`DSP/Core/RawPunch.h crunchPath`, its own bus after the master clip):
SOURCE `cn_src` SINE / SAW / 909-BD (default) / SINE + SAW (`cn_src_mix`), pitched from the PUNCH START x `cn_pitch_off` (-24..+12 st,
default -12), stable (no punch drop; 909 has its own short knock), `cn_phase` start phase, `cn_offset` = CRUNCH DELAY 0-80 ms (20)
-> ENVELOPE `cn_attack` -> hold to PUNCH END + BODY x `cn_follow` (LENGTH) -> `cn_decay`
-> PRE-SHAPE `cn_low_cut`, `cn_mid_boost` (1 kHz), `cn_high_cut`, `cn_pre_emph` -> TUBE `cn_tube` (0.3) -> ASYMMETRIC `cn_grit` (0.6,
the negative half more drive + offset) -> WAVESHAPER `cn_ws` SOFT / HARD / FOLD / CRUSH / CUSTOM -> HARD CLIP `cn_clip`
-> DISTORTION MIX `cn_dist_mix` (0.85) -> POST EQ `cn_tone` (LP 2-14 kHz) + BODY LIMIT `cn_body_limit` (low shelf) -> CRUNCH MIX `cn_crunch`
(level only). `cn_solo` SOLO CRUNCH. Retired: cn_density, cn_source, cn_pitch.
Why (owner's rawstyle-kick_G_major, fitted in 10 ms windows): the punch = ~600-700 Hz + a second, lower, steadier tone (e.g. 534 + 625,
713 + 409 + 158 Hz), clipped together -> irregular square cycles = the crack. One tone into a clip = "pup".

---

# RAW CRUNCH – rework #2 (owner 2026-11-21): a TONAL layer

**This replaces the CRACK IMPULSE ENGINE below (kept only as history).** After RAW kick tutorials (Teknovault: the crunch = its own
oscillator below the punch, own envelope, tube + asymmetric distortion; On Point Samples: an Analog BD sine, longer envelope, lower
pitch, shifted in time; the classic stack PUNCH + CRUNCH + PUNCH REVERB): **TONAL SOURCE + DISTORTION = CRUNCH, never NOISE + DISTORTION.**

Chain (`DSP/Core/RawPunch.h crunchPath`, its own bus after the master clip):
SINE (key: note x 2^((45 + PITCH) / 12), slight -3 st fall) -> ENVELOPE (hard first hit x2.2 drive, hold 55 % of the length, release)
-> TUBE (`cn_tube`) -> HP 150 + MID BELL +14 dB at TONE (350-1400 Hz) -> ASYMMETRIC (`cn_grit` = ASYM) -> HARD CLIP -> FINAL EQ
(HP 170 x2, LP by TONE, open for the first ~2 ms) -> post shape (hit, then a held dirty CZZ at ~0.72).
Length = PUNCH END + BODY x BODY FOLLOW (as before). OFFSET (`cn_offset`) = start after the kick.
`cn_density` / `cn_source` RETIRED (kept for old projects, no sound). New: `cn_pitch`, `cn_offset`, `cn_tube`.

Fitted to the owner's 4 references (OPS_FREE_RAWSTYLE_PUNCH 20_F / 15_D / 11 / 10_C): tonal 400-800 Hz, level held ~100-150 ms,
crest 2-4 dB (near square), energy 200-1500 Hz; the bright 0-5 ms there = a separate snap (our SNAP / CRACK layer).
4GE now: F ~390 -> 350, C ~580 -> 525, D ~650 -> 600 Hz; held level (hit - tail ~1 dB), flatness ~0 (a tone, not a noise).

---

# 4GE – RAW PUNCH: DYNAMIC DISTORTED CRUNCH (owner's spec 2026-11-21, after "kurwa no nie mamy tego dalej")

The target is not a "KRAK" (that is the CRACK layer, `RAW_CRACK.md`). It is a **held, dense, distorted grind**: "CZZZZ / GRRRR".
It runs from the punch into the body, with no extra bass and no "DUM".

A parallel path of the existing RAW punch source. Code: `DSP/Core/RawPunch.h` `crunchPath`; params `cn_*`; the 3rd row of the punch tab RAW.
- **Source:** the punch's own tone + the SNARE / NOISE / HYBRID broadband source (the EUPHORIC punch's recipe, held).
- **Chain:** PRE-SHAPING (HP 260 Hz) → ASYMMETRIC WAVESHAPING (GRIT) → MULTISTAGE SATURATION (2–5 stages by DENSITY, an HP 300 Hz
  between the stages) → CONTROLLED HARD CLIP → TONE (bell 900 Hz – 3.3 kHz + a lowpass 5–13 kHz, no hiss).
- **Envelope from the real phases:**
  - attack 1 ms;
  - held until PUNCH END (= the RAW punch DECAY);
  - END = PUNCH END + BODY DURATION (body delay … delay + length) × BODY FOLLOW.
- **HYBRID coupling:** inside the body it loses its edge (the lowpass closes 60 %) and gets grainier (a sample-and-hold by GRIT).
  There is no level step at the punch end (tested < 3 dB).
- **User controls:** CRUNCH, GRIT, DENSITY, TONE, BODY FOLLOW (+ SOURCE, ON/OFF). The clip threshold, the number of stages and the shaper
  stay internal.
- **Level:** in the punch bus. On by default with PUNCH TYPE RAW; OFF = the RAW punch as before.
- **Measured in "RAW PUNCH - 2016 Standard"** (crunch ON vs OFF, 1–6 kHz):

  | Window | Change |
  |---|---|
  | 5–40 ms | +2.2 dB |
  | 40–120 ms | +5.1 dB |
  | 120–250 ms | +6.7 dB |
  | the lows | -0.3 dB |

## Rework 2026-11-21: CRACK IMPULSE ENGINE

Owner: "przypomina ciągły szum, a nie trzask". The crunch is now **a series of distorted impulses, never a continuous noise**
("CZ-KR-CZ-KRR").
- **The series:**
  - it fires on the punch's own cycles, so it stays rhythmic with the punch;
  - the gaps are irregular but repeatable (a fixed hash, not a random generator); DENSITY sets them at ~9 → ~2.5 ms apart;
  - the first impulse is the strongest and the later ones get weaker;
  - each impulse decays within 30 % of the gap, so they never smear into each other.
- **Each impulse:**
  - a short tonal burst: saw / square + the punch's tone; SNARE / NOISE / HYBRID only colour it;
  - → HP 350 → ASYMMETRIC shaper (GRIT) → 2–4 saturation stages → HARD CLIP → TONE lowpass;
  - → its OWN envelope applied **after** the distortion, so it stays an impulse;
  - → an HP 300 Hz ×2 after the sequence mix: the impulse rate itself must not make a low "thump".
- **BODY TRANSITION:** in the body the impulses get softer, darker and lower; the series fades to its end, with no level step.
- **Tests:**
  - spiky envelope: 1–6 kHz in 0.5 ms frames, peak vs median ~30 dB;
  - the first impulse is the strongest (+2 dB);
  - no bass;
  - BODY FOLLOW follows the real phases;
  - no step at the punch / body boundary.
- **In "2016 Standard"** (ON vs OFF, 1–6 kHz):

  | Window | Change |
  |---|---|
  | 5–40 ms | +2.9 dB |
  | 40–120 ms | +2.6 dB |
  | 120–250 ms | +3.5 dB |
  | lows | +0.2 dB |

## 2026-11-21: "crunch musi być wyraźniejszy" + the first SOLO "zjebane"

- The CRUNCH now goes on the **attack bus after the master clip**, together with the CRACK. It is its own level, never squashed by the
  punch FX / the master clip.
- The SOLO was "zjebane" because it was a difference of two renders through the distortions (it held changes of everything). On the
  attack bus the difference is now exactly the layer.
- Denser (7 → 2 ms apart, each impulse up to 45 % of the gap) and louder (gain ×9, a soft ceiling at 0.9 → 1.2). The RAW PUNCH presets
  got `core_master` -11 dB for headroom.
- In "2016 Standard" (1–6 kHz vs OFF, peak 0.94, no clip):

  | CRUNCH | 0–5 ms | 5–40 ms | 40–120 ms | lows |
  |---|---|---|---|---|
  | default | +2.9 dB | +5.8 dB | +7.5 dB | ±0 |
  | 100 % | – | +7.4 dB | +9.5 dB | – |
