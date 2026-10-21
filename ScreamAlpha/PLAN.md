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

## Owner's ideas – next steps (2026-10-22, after testing v0.3)
1. **Preset 8** from the owner (coming) → factory preset; tune RANDOM "owner taste" ranges on presets 6 / 7 / 8.
2. **Bigger automation window**: the cutoff-automation preview much larger (full card width).
3. **Custom shape**: besides the ready shapes (rise / fall / up-down / wobble / steps) a CUSTOM shape drawn with points
   (click = add, drag = move, bend the curve between points – like the drawn pitch in Forge of Wrath). Picking a ready
   shape loads it into the editor, so it can be edited, not drawn from zero. Length 1/16 … 1 bar, tempo-synced as now.
4. **MOD 1 / MOD 2** (owner's own design – NOT Serum drag & drop):
   - one window with two tabs of graphs, MOD 1 and MOD 2, each its own colour (different from each other);
   - per MOD a button **ASSIGN**: press it, then click any knob → that knob takes the MOD colour and the button text changes
     to the knob name (e.g. CUTOFF); right-click / small "x" removes the assignment;
   - **one knob per MOD** for now (more targets later if needed);
   - under it **one range slider + a [+ / −] switch**. The slider sets the OTHER END of the range as an absolute knob value:
     - **+**: the MOD moves from the knob's position UP to the slider value (knob 30 %, slider 50 % → 30 % … 50 %);
     - **−**: from the knob's position DOWN to the slider value (knob 30 %, slider 10 % → 10 % … 30 %);
     - in + the slider can't go below the knob, in − not above; turning the knob past the slider pushes the slider along;
   - the knob shows the range as a thin ring in the MOD colour (`flat::Knob::setModulation` already exists);
   - today's cutoff automation becomes MOD 1 → CUTOFF; old projects / presets must load the same (new param IDs only).
