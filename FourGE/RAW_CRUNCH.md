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
