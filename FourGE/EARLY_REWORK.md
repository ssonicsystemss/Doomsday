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
  click in AUTO, a short global sub punch. Owner 2026-11-16: "classic punche git" -> P1 (= the `cp_*` defaults) taken as the reference punch.
- Measured on the owner's 20 EARLY references (kick_hardstyle_1-20, all G1, ~400 ms): pitch ~500-700 Hz at 2-5 ms → ~230 Hz at
  ~12 ms → ~100 Hz at ~60 ms → ~70 Hz at 100 ms → the note (49 Hz) by ~200 ms. Spectrum: 0-4 ms a transient peaking 2-4 kHz;
  4-15 ms the fundamental (200-320 Hz) + a flat harmonic plateau 640 Hz-4 kHz at -7..-12 dB; 15-40 ms peak 160-200 Hz, mids
  -15..-20; 40-100 ms peak ~100 Hz, rest under -18; 100-250 ms a clean sub (50-63 Hz), harmonics -20 (160 Hz) .. -50 (4 kHz).
- Defaults of `cp_*` = a fit of the prototype to those numbers (scratch tool `cp/fit.py`): start 1300 Hz, hit 240 Hz / 12 ms,
  glide 70 ms to note +13.5 st, a 2.3 kHz own transient, resonance 380 Hz, mid +8 dB at 2.9 kHz, saturation 32 dB (decay 113 ms)
  into a HARD shaper - the references are heavily clipped on the hit. Candidates P1-P5 sent to the owner to choose.
- **EARLY style = the whole kick fitted (2026-11-16)**: `punch_mode` CLASSIC + TOK / SUB PUNCH / BODY / TAIL / master fitted to the 20
  refs (scratch tool `ef/fit.py`: RenderTest `--presetlab` batches of 16, third-octave spectra in 5 time windows + zero-crossing
  pitch track + window levels; score 25 -> 10 = spectrum 3.9 dB, pitch 2.3 st, envelope 2.2 dB). Results: the CLASSIC punch carries
  the distortion (SOFT 0.4 -> TUBE 0.55 on the PUNCH, the tail stages at 0), the tail is clean and low-passed (300 Hz, mid -6 dB at
  545), starts from the hit (`tail_start_mode` 1), its pitch falls slowly (127 -> note in 170 ms, curve 0.04); the SUB glides down
  with it (SLOW DROP 12 st / 120 ms - the refs sit at 90 / 72 / 63 / 56 Hz over 80-130 ms); body = a clean layer at 162 Hz; master
  clip 7. A separate TOK went to -40 dB in every round -> EARLY TOK AUTO = OFF (the classic punch is the tok); SUB PUNCH -19.7 dB / 50 ms.
  Remaining mismatch: the spectrum 15-40 ms (refs have more 300-600 Hz) - next candidate if the owner hears it.
- Owner 2026-11-16 "drapie przy punchu" (on the very start): the stacked distortion gave +5 dB of 8-20 kHz in the first 1.5 ms.
  His pick **D**: `cp_ws_type` SOFT, `cp_sat` 24, `cp_ws_drive` 12, `cp_high_cut` 7000, punch pool stages at 0, master clip 3 (score
  12.4 - by ear over the numbers). Also tried: a lower punch start 800 Hz (F) and a shorter 5 kHz transient (G) - closer onset
  numbers, not chosen.

## MODERN as a whole kick (2026-11-16, after the owner liked RAW: "nawet git a dawaj inne")
- MODERN fitted to the owner's OPS FREE KICK 1-4 (F; KICK5 ARSONY is G, left out) with the scratch tool `mf/fit.py` (2 passes):
  score 25.6 -> 12.6 (style 13.8). The refs hold a tonal ~700 Hz for the first ~40 ms -> a high 909 TOK (+22 st, 65 ms, loud),
  a loud global click (+3.5 dB, 24 ms), a long sub punch (110 ms), the tail sits low from the start (pe_start 60 Hz), tail attack 23 ms,
  the punch chain SOFT 0.8 -> DIGITAL 0.35, master clip 5.6.
