# FAST DIST STAGE (owner spec 2026-11-25)

A functional counterpart of Fruity Fast Dist (its DSP is not public: the architecture and the behaviour, not a bit copy).
`DSP/Dist/FastDist.h` (pure DSP) - used twice in TAIL TYPE HARDCORE right after the PRE-EQ (`hc_fd1_*`, `hc_fd2_*`).

INPUT -> PRE -> THRESHOLD CORE (A / B) -> [ASYMMETRY] -> DC BLOCK -> MIX -> POST
- PRE 0..100 % (50 % = 1:1, 100 % = +24 dB): how much goes INTO the distortion. POST (dB): the level after, never the curve.
- THRES: where and how hard the nonlinear region starts (lower = earlier, more harmonics, more aggressive; below it the wave
  stays linear; the output is normalised to the curve's ceiling, so a low THRES also lifts the quiet parts).
- A = dense / saturating (an exponential knee), B = harder / clipping-like (a near-hard knee). A != B.
- MIX = dry * (1 - mix) + wet * mix.
- Advanced (stage 1): ASYMMETRY + DRIVE / - DRIVE (dB) / BIAS, a 5 Hz DC blocker after (only with asymmetry).
- Runs at the engine's 4x oversampled rate; deterministic (no random anything).
Rules from the spec: FAST DIST != tanh drive, != HARD CLIP, != TUBE; PRE != POST; never compensate the level from PRE.
The owner's gabber tutorial: an FPC kick -> Fast Dist x 2, PRE 100 % each.
