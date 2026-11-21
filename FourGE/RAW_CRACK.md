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
