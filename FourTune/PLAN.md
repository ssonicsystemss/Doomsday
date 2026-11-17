# 4TUNE – build plan

Owner spec: `SPEC.md` (2026-11-17, 31 sections). Successor of Fortune of Seekers (frozen at v1.0, old projects keep it).
Owner priorities (2026-11-17): **euphoric leads / supersaws** and **plucks / arps** sound best.
Melody test (owner 2026-11-17): `Tests/demo/no_time_to_waste.mid` (Wildstylez - No Time To Waste lead, 150 BPM, B1-E5, played at the
original pitch, solo without a kick): `RenderTest <out.wav> --midi <preset> Tests/demo/no_time_to_waste.mid 150 [id=value]`.

Rules (same as every Doomsday plugin): DSP / GUI / files never in one component, real-time safe audio thread,
parameter IDs frozen in `Tests/param_ids.txt` (APPEND ONLY - the order is the FL automation index), every visible control
must change the sound (RenderTest checks min vs max), English UI, Polish README.

## Signal flow
```
OSC 1 + OSC 2 -> GROUP A mixer (vol / pan / width) --+
                                                      +-> FILTER (route A+B / A / B) -> AMP ENV -> (voice sum)
OSC 3 + OSC 4 -> GROUP B mixer (vol / pan / width) --+
   -> SCREAM (oversampled, gain compensated) -> EQ -> DIST -> CHORUS -> DELAY -> REVERB -> STEREO -> LIMITER -> MASTER
FORTUNE = a modulation layer on top: FINAL = BASE + FORTUNE (cutoff +2 oct, detune, width, scream, reverb mix +15 / size +10)
```

## Steps
| # | What | Spec | Status |
|---|---|---|---|
| 0 | Plugin skeleton, parameter table (`Source/Params.h` X-macro -> APVTS + enum `P::`), frozen IDs, CI | 26 | v0.1 |
| 1 | SOUND ENGINE: 4 osc (SAW / SINE / TRI / SQUARE / PULSE / WAVETABLE morph + WARP, oct / semi / fine, pan, width, phase + random + retrig / free, unison voices / detune / spread / mix), group mixers, multimode filter (LP12 / LP24 / HP12 / HP24 / BAND / NOTCH, cutoff / reso / drive / key / env, route), 3 AHDSR + curve envelopes, poly / mono / legato / glide / bend / velocity | 2-8, 25 | v0.1 |
| 2 | SCREAM module (drive / saturation / tone / bite / body / mix / output, soft clip -> fold -> HF excite, auto gain, 1x / 2x / 4x) + FORTUNE macro (non-linear curve, per-destination amounts) | 11-19 | v0.1 |
| 3 | FX: EQ 4 band, DIST, CHORUS, DELAY (sync, ping-pong, filter), REVERB, STEREO (width / balance / mono bass), LIMITER | 20 | v0.1 |
| 4 | LFO 1 / 2 (7 shapes, sync incl. dotted / triplet, fade, phase, retrig) + 16-slot MOD MATRIX (sources / destinations from the spec, +/-) | 9-10 | v0.1 |
| 5 | GUI: SYNTH page (OSC -> FILTER -> SCREAM -> FX -> OUTPUT readable at a glance, dominant FORTUNE with CONTROLLED -> FESTIVAL scale), MOD page, FX page | 29 | v0.1 |
| 6 | Factory presets (plucks + euphoric leads / supersaws first) + PresetBar browser (search / categories / favourites) + RANDOM / SMART RANDOM | 27-28 | v0.1 |
| 7 | MELODY ENGINE: key / scale (incl. custom), phrase length, density, complexity, repetition, movement, octave range, rhythm density + 9 hardstyle modes with their own rules | 21-22 | next |
| 8 | PIANO ROLL (add / delete / move / pitch / length / velocity / copy / paste / duplicate / quantize / humanize / transpose / octave) + MIDI drag & drop into FL | 23-24 | next |
| 9 | Preset browser extras (rename / delete / author), HQ switches, CPU pass | 26-27 | later |

## Decisions taken without the owner (2026-11-17, can be changed)
- SCREAM sits after the voice sum (one oversampled stage for all notes, like Sylenth's distortion), not per voice - cheap and
  the classic hardstyle bus sound; chords through high SCREAM intermodulate (that is the hardstyle lead sound).
- WAVETABLE = a morph SINE -> TRI -> SAW -> SQUARE by POSITION (the architecture takes real tables later); WARP = phase
  distortion on every wave (PULSE: POSITION = pulse width).
- Group B gets the stronger FORTUNE detune (spec 14: OSC 1 / 2 stay focused).

## Step 7 notes (2026-11-17, before writing the generator)
- Owner: 4TUNE v0.1 on No Time To Waste "chyba git" -> go on with MELODY (step 7) + PIANO ROLL / MIDI drag (step 8).
- The reference MIDI (Wildstylez - No Time To Waste lead) = a **shuffled bounce**: every beat = a lead note 2/3 beat long on the beat +
  a 1/3 beat note on the triplet off-beat, which is mostly the **low root of the current chord** (B1 / G2 / A2 / F#2, one bar per chord)
  and sometimes a second melody note. Chords 1 per bar (Bm - G - A - F#), lead notes are chord tones on the beat, steps / small
  leaps in between, phrase answers repeat bars 1-2 with a new ending. Use this as the CLASSIC / EUPHORIC rhythm template
  ("ROOT BOUNCE" option: off-beats = low chord root), all velocities 100.
- Plan for the code: `Source/Melody/MelodyGen.h` (no DSP / GUI: settings -> notes, seeded), melody stored in the state child MELODY
  (ValueTree, undoable), audio-thread snapshot swapped from the message thread, PREVIEW synced to the host, `GUI/PianoRoll` + DRAG MIDI.
