# 5CR34M 4LPH4 (ScreamAlpha) – plan

Successor of Prophecy of Scream (frozen at v3.1, see `Shared/LESSONS.md`). Written from scratch on `Shared/` blocks.
Name in the UI / FL: **5CR34M 4LPH4**; in code / folders / files: `ScreamAlpha`.

## What the owner's references say (2026-10-21)
55 references from the owner (Screech House short notes, FHSP hits, OPS one-shots, 25 OPS screech loops @150, 4 solos, 1 acid bass).
Features: `Tools/ref_features.py` → `Tools/ref_features.csv` (WAVs are the owner's, not in git). Compared with his own 5 PoS presets.

| | references (medians per group) | PoS presets | meaning |
|---|---|---|---|
| energy below 300 Hz | −21 … −35 dB | −11 dB | references are light below ~250 Hz; PoS drags low energy |
| brightness movement (std of centroid, oct) | 0.35 … 0.78 (loops 0.78) | 0.31 | references keep changing colour, note by note |
| main resonance (formant 1) | 690 … 900 Hz | 1.3 kHz | lower, open "aah" voice vs nasal |
| amplitude movement (env std/mean) | 0.7 … 1.9 | 0.4 | rhythmic chopping / gated notes |
| upper harmonics (spectrogram) | smeared, chaotic, never steady | clean straight lines | PoS too periodic – same lesson as Forge (CHAOS) |
| note start (spectrogram) | fast pitch scoop down into every note (≈ +5…+12 st, 40–400 ms) | only on retrigger; legato kills it | the "throw" of each note |

The owner's word for what PoS lacks: **"polot"**. A LIFT prototype (delayed vibrato, slow filter rise, octave-up layer, chorus)
was rejected ("nie o to chodzi") – so it is not slow modulation or width; it is the life of the source and the per-note gesture.

## Engine (first version)
Mono / legato voice, 4x oversampled core, everything per note:
1. **Source**: 2 oscillators (saw / pulse with PWM / `IntensityOsc` SPECTRUM+BEND+FORMANT modes from Massive), wide unison (`UnisonBank`),
   FM between them, **chaos** (noise-modulated phase + `Slop`) so the highs never sit still.
2. **Note gesture** (the main new idea): every note – also legato – gets a pitch scoop (amount, time, curve, up or down) and an
   optional dive at note end; drawn per-note shapes (`mod::Shape`).
3. **Scream chain**: HP 150–300 Hz (keeps it light) → drive 1 (`Adaa1`) → **comb / flanger with feedback** (new block) →
   formant pair (vowel A–O–E–I morph, 500 Hz–3 kHz) moved by the note gesture → drive 2 → tone.
4. **Movement**: tempo-synced LFO / step shapes on formant, comb and gate (`Performer`, `Lfo` with CHAOS), per-note random (`RandomPerNote`).
5. FX: OTT (`Ott3`), stereo, delay, reverb; output with level match.
6. UI: Minimal / flat (`doomsday::flat`), big graph = formant/vowel + comb; preset browser + undo (`PresetBar`).

## How we work
Step by step with A/B against the references (same note, level matched) before anything goes into the plugin.
First milestone: the source + note gesture + scream chain offline (render tool), A/B vs FHSP 107, Massive 023 and loop 9.
