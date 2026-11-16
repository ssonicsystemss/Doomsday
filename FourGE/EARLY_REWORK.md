# 4GE – EARLY REWORK (owner 2026-11-16) – build order: ONE good EARLY / CLASSIC PUNCH first, then fit the rest to it

EARLY is rebuilt from the CLASSIC PUNCH up – not fixed with EQ / distortion, and never "MODERN with the effects off": its own
construction profile. Goal: the old, hard, short hardstyle punch whose character comes from the CONSTRUCTION of the attack, not from a
modern "big punch + lots of saturation".

Structure: `TOK → CLASSIC PUNCH → BODY → TAIL → SUB`; the CLASSIC PUNCH is the hero of the first ~80–120 ms.

| layer | approach |
|---|---|
| TOK | short, characteristic |
| CLASSIC PUNCH | **the main element** |
| CLICK | very carefully / often OFF (if the punch makes its own transient, a global click only spoils it) → EARLY AUTO: global CLICK OFF |
| SUB PUNCH | short, controlled |
| BODY | less dominant |
| TAIL | may be aggressive, but must never take over the punch's character |
| SUB | a clean foundation |

1. **Source** – not a pure sine. A hybrid: a basic tonal source + a short transient component + a slightly resonant mid / low-mid +
   very controlled noise only as part of the attack. A "hit", never a click.
2. **Pitch envelope** – key: high start → a fast drop → stabilisation (`PITCH ↓↓↓ → FUNDAMENTAL → BODY`). Not the extreme drop of
   MODERN / RAW: short and clear, so the punch feels like a physical hit.
3. **Spectrum** – fundamental strong and clean; low-mid = the "meat"; mid = the character of the hit; top only enough for a readable
   transient. Not a wide, bright festival spectrum: EARLY = a more focused, aggressive mid.
4. **Distortion** – not `PUNCH → huge distortion → EQ` but `PUNCH SOURCE → pre-EQ → character saturation → waveshaper → post-EQ →
   transient control`. The distortion brings out the harmonics of the existing punch, it does not create its character.

Order of work: one reference EARLY / CLASSIC PUNCH → then TOK, SUB PUNCH, BODY, TAIL → then the whole EARLY.

## Status
- PUNCH TYPE **CLASSIC (EARLY)** built: `Source/DSP/Core/ClassicPunch.h`, params `cp_*`, punch tab CLASSIC; the FRONT profile: no global
  click in AUTO, a short global sub punch. The EARLY style does NOT switch to it yet (waiting for the owner to pick the reference punch).
- Measured on the owner's 20 EARLY references (kick_hardstyle_1-20, all G1, ~400 ms): pitch ~500-700 Hz at 2-5 ms → ~230 Hz at
  ~12 ms → ~100 Hz at ~60 ms → ~70 Hz at 100 ms → the note (49 Hz) by ~200 ms. Spectrum: 0-4 ms a transient peaking 2-4 kHz;
  4-15 ms the fundamental (200-320 Hz) + a flat harmonic plateau 640 Hz-4 kHz at -7..-12 dB; 15-40 ms peak 160-200 Hz, mids
  -15..-20; 40-100 ms peak ~100 Hz, rest under -18; 100-250 ms a clean sub (50-63 Hz), harmonics -20 (160 Hz) .. -50 (4 kHz).
- Defaults of `cp_*` = a fit of the prototype to those numbers (scratch tool `cp/fit.py`): start 1300 Hz, hit 240 Hz / 12 ms,
  glide 70 ms to note +13.5 st, a 2.3 kHz own transient, resonance 380 Hz, mid +8 dB at 2.9 kHz, saturation 32 dB (decay 113 ms)
  into a HARD shaper - the references are heavily clipped on the hit. Candidates P1-P5 sent to the owner to choose.
