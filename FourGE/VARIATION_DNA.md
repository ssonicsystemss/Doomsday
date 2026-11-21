# 4GE – KICK VARIATION: the DNA algorithm (draft, 2026-11-21)

The owner's logic (2026-11-21):

| Control | Meaning |
|---|---|
| DNA | WHAT may change and WHICH WAY (9 parts, KEEP / EVOLVE + direction / FREE) |
| STYLE | the sound world we work in (EARLY / MODERN / RAW / EXPERIMENTAL / EUPHORIC / PVC / ZAAG) |
| VARIATION | how far the versions may go from the BASE |
| HUMANIZE | how far from an identical, sterile result. It never changes the character: the engine varies each hit (pitch ±12 ct, level ±0.8 dB) |
| MUTATE | variations of MY kick (preserve the identity of BASE) |
| NEW KICK IN STYLE | new kicks from scratch within the STYLE |
| MORPH | glide from the BASE to the chosen version |
| SET AS BASE | that version becomes the new starting point |

"The DNA decides whether this system makes good hardstyle kicks or only looks good." So a version is **not** "random knobs". Every version is
**proposed → rendered → measured → checked → chosen**.

## 1. Pipeline of one MUTATE

1. **Propose** 24 candidates (frames, cheap) from the BASE by the DNA rules (§3).
2. **Render** each one offline on the preview thread, level-matched to the BASE (LUFS, not peak).
3. **Measure** each one with module 16 `KickAnalysis` (pitch start / end / drop time, attack, transient, length, 6 bands, sub share, low-end
   correlation, crest, clipping, harmonics of the tail).
4. **Check** (§2 hard rules + §4 KEEP anchors + §5 direction check). A candidate that fails one check is thrown away.
5. **Score** each survivor:
   - fit to the STYLE profile: bands, crest and drop time vs the style. Where the owner's reference analyses exist (EARLY: 20 refs) they are
     used; otherwise the template's own render.
   - closeness of its distance-from-BASE to the VARIATION target.
6. **Choose 7 that differ from each other**. Farthest-point pick among the better half, so we never get 7 near-copies.
   If fewer than 7 survive, propose more (max 3 rounds), then show what we have.

## 2. Hard rules (every version, every mode – "a hardstyle kick stays a hardstyle kick")

- **The note never moves.** The settled tail pitch stays within ±25 cents of the BASE, because the kick is the bass line.
- **The low end stays mono.** Low correlation (< 150 Hz) ≥ 0.9.
- **The sub is there.** Sub share is not below the STYLE minimum.
- **No broken output.** No DC, no clipped samples after the master stage, not silent, the transient is present (transient dB > style minimum).
- **The length follows the BASE** (±3 %) unless LENGTH is not KEEP. KICK END / length sync are never touched.
- **A dirt budget.** The sum of the drives in one chain (punch / body / tail / master) has a STYLE maximum, so stacking stages never turns
  the tail into noise. Crest factor stays inside the style's range.
- **Resonance stays below self-oscillation** (tail / filter resonance ceilings per engine).

## 3. What each mode may do

| | Parameters | Types (punch type, tail type, engines, modes) | Step size |
|---|---|---|---|
| **KEEP** | frozen | frozen | 0 – plus the measured anchor (§4) |
| **EVOLVE** | continuous ones only | never | ≤ 0.4 × VARIATION over the knob's travel, each version a different share of it (45–100 %) |
| **FREE** | all of the part's parameters, inside the STYLE's ranges | may switch, only to what the STYLE allows | up to VARIATION × 1.5 |

- **Coupled parameters move together**, so a step never breaks a working balance:
  - tail drive ↑ comes with tail level ↓ (the same loudness into the next stage);
  - tone ↑ comes with resonance ↓ a little (no harsh peak);
  - punch level ↑ comes with punch drive ↓ a little;
  - pitch start ↑ comes with drop time ↓ a little (the same "speed" of the drop).
- **A type switch in FREE loads the new type's STYLE template first**, then rolls inside its ranges. Never "ZAAG tail with the CLASSIC tail's
  leftover numbers".
- **Mixed parts:** a parameter shared by two parts (e.g. the master clipper: DIRT and every part's loudness) belongs to one part only
  (DIRT). The other parts are protected by their measured anchor.

## 4. KEEP = measured, not only frozen knobs

Freezing a part's knobs is not enough: the master clipper, the EQ or a shared drive can still change how that part *sounds*. So every KEEP
part also has a measured anchor. A candidate that moves it beyond the tolerance is thrown away.

| Part | Anchor (vs the BASE) |
|---|---|
| PITCH | start Hz ±3 %, drop time ±10 % |
| PUNCH | the first 30 ms: envelope ±1 dB, presence band ±1.5 dB, attack ±1 ms |
| BODY | the BODY band (150–400 Hz) ±1.5 dB |
| TAIL | tail harmonics 1–10 ±1.5 dB on average, MID band ±1.5 dB |
| WEIGHT | SUB band ±1 dB, sub share ±3 % |
| DIRT | crest ±1 dB, PRESENCE + AIR ±1.5 dB |
| LENGTH | length ±3 % |
| SPACE | side level ±2 dB, correlation ±0.05 |
| SCREECH | screech on / off the same, its band ±2 dB |

## 5. EVOLVE direction – and how ANY picks one

Each direction is a **perceptual axis**: several weighted parameters, *and* a measurement that proves it worked.

| Part | − / + | Moves (weighted) | Must measure |
|---|---|---|---|
| PITCH | LOWER / HIGHER | pitch start (all engines' start) | start Hz down / up ≥ 3 % |
| PUNCH | SOFTER / HARDER | punch level, transient, punch drive, click, attack (−) | transient dB −/+ ≥ 0.7 dB |
| BODY | LESS / MORE | body level, body resonance | BODY band −/+ ≥ 1 dB |
| TAIL | DARKER / BRIGHTER | tail tone, colour, mid gain, engine brightness | MID + PRESENCE vs LOW −/+ ≥ 1 dB |
| WEIGHT | LIGHTER / HEAVIER | sub level, sub drive, engine sub | SUB band −/+ ≥ 1 dB |
| DIRT | CLEANER / DIRTIER | every drive + clipper | harmonics 3–10 −/+ ≥ 1 dB, crest +/− |
| LENGTH | SHORTER / LONGER | kick length, decays | length −/+ ≥ 4 % |
| SPACE | NARROWER / WIDER | stereo widths, room | side level −/+ ≥ 2 dB |
| SCREECH | LESS / MORE | screech level, feedback, metal | screech band −/+ ≥ 1.5 dB |

A version that went the wrong way (e.g. BRIGHTER, but the drive made the low end louder) is thrown away. "Brighter" on screen means
brighter to the ear.

**ANY is not "random knobs".** For each EVOLVE part on ANY:
1. **Spread over the 7 versions.** Directions are dealt so the set covers both ways: about 3 versions −, 3 versions +, 1 neutral. They are
   shuffled separately per part, so the combinations differ (V1 brighter + heavier, V2 darker + lighter …). This is Latin-hypercube style,
   never 7 times "a bit brighter".
2. **Pulled toward the STYLE.** If the part is already outside what the STYLE usually has (e.g. a tail much darker than EUPHORIC), more
   versions go back toward the style: p(+) = 0.5 + 0.35 × (how far outside, −1..1).
3. **Then the same measured check** as a fixed direction: every ANY version gets a real direction label (shown on the tile, e.g.
   "brighter, heavier").

## 6. NEW KICK IN STYLE

- Start from the STYLE template (generators, pitch drop, distortion chain, FRONT profile).
- Roll every part as FREE, except LENGTH (it stays yours).
- Then the same pipeline: hard rules, style-fit score with a higher weight, 7 different picks.
- The BASE is not used, except for the note and the length.

## 7. Open points (owner)

- Is the **note lock** (§2) right for every case? Or should PITCH FREE be allowed to move the settled note (e.g. for an off-key effect)?
- MUTATE with render + measure takes about 1–3 s in the background (the tiles fill in as they are ready). OK?
- Style profiles: EARLY has 20 refs. Do we want more reference kicks per style (5–10 each) so "fit to the STYLE" is real for all 7?
