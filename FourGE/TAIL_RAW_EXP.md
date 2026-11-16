# 4GE – TAIL TYPE RAW & EXPERIMENTAL (owner's spec, 2026-11-16) – collected verbatim in English, build after the FRONT

RAW and EXPERIMENTAL are the two most flexible TAIL TYPEs, with completely different philosophies – NOT two presets of one
engine, but two separate TAIL generation profiles on a shared DSP foundation.

- **RAW** – aggressive, heavy, distorted, with a characteristic pitch movement and lots of harmonics. Must sound like
  professional Raw Hardstyle / Xtra Raw.
- **EXPERIMENTAL** – a sound laboratory: unusual modulation, non-standard sources, FM, ring modulation, formants, granular
  textures, non-standard pitch curves.

## 1. Shared TAIL architecture (both types use the same skeleton; they differ in HOW they use each module)

```
TAIL ENGINE
├── 01. SOURCE GENERATOR
├── 02. PITCH ENGINE
├── 03. BODY / TAIL TRANSITION
├── 04. DISTORTION STACK
├── 05. EQ & RESONANCE
├── 06. MODULATION ENGINE
├── 07. STEREO ENGINE
├── 08. SUB MANAGEMENT
└── 09. OUTPUT / CLIPPER
```

## 2. TAIL TYPE RAW – aggressive / distorted

Philosophy – three things:
1. A strong, clear MID – the characteristic growl.
2. Dense harmonics from CASCADED distortion.
3. Tonal MOVEMENT while it rings out – not a static, uniform distortion.

Based on the proven workflow: a Triangle / Sine source, amplitude + pitch envelopes, alternating EQ and distortion, a
resonant BAND-PASS SWEEP and an optional reverb. ~400–500 Hz is an important area for shaping a rawstyle tail (Screech House
tutorial) – a starting point for tuning, not a fixed frequency for every kick.

### A. RAW SOURCE GENERATOR
```
RAW SOURCE
├── OSCILLATOR 1   SINE / TRIANGLE / SAW / SQUARE      (default TRIANGLE)
├── OSCILLATOR 2   SINE / TRIANGLE / SAW / OFF         (default OFF)
└── NOISE          WHITE / PINK / OFF                  (default OFF)
```
The user can change the source without changing the architecture.

### B. RAW DISTORTION STACK – the heart of RAW
```
SOURCE → PRE-EQ → DISTORTION 1 → SHAPING EQ → DISTORTION 2 → BAND-PASS FILTER → DISTORTION 3 → POST-EQ → SOFT CLIPPER
```
Every distortion stage is a SEPARATE processor:

| stage | function | character |
|---|---|---|
| 1 | the main harmonic generation | aggressive |
| 2 | densifying the signal | strong |
| 3 | the final character | controlled |
| CLIPPER | peak control | soft / hard |

Important: the stages must NOT have the same drive. The first ones shape the character, the last ones refine it without
destroying the texture.

### C. RAW FILTER MOVEMENT – to be highlighted
RAW has an automatic BAND-PASS SWEEP:
```
FILTER ENVELOPE:  START FREQUENCY → SWEEP → PEAK / RESONANCE → RELEASE
```
Params: FILTER START, FILTER END, RESONANCE, SWEEP SPEED, SWEEP CURVE, FILTER MIX.
Movement shape: DOWN / UP / DOWN-UP / UP-DOWN / CUSTOM. The sweep is SYNCED to TAIL LENGTH.

### D. RAW CHARACTER – three macros (never plain volume knobs; each drives several DSP params)
- **GROWL** – more mid harmonics and resonance intensity.
- **CRUNCH** – more aggressive distortion and clipping.
- **MOVEMENT** – deeper sweeps and modulation.

### E. RAW REVERB – optional, internal
SIZE, DECAY, DAMPING, PRE-DELAY, MIX, LOW CUT. Subtle by default. Must never smear the attack or make an uncontrolled bass tail.

## 3. TAIL TYPE EXPERIMENTAL – creative sound design

Philosophy: not one characteristic sound. It must allow: metallic tails, unstable aggressive textures, FM bass tails,
formant sounds, unusual pitch movement, digital, almost glitchy tails. Inspiration: formant filtering, pitch shifting,
frequency shifting, ring modulation, stretching, resampling.

### A. EXPERIMENTAL SOURCE GENERATOR
```
EXPERIMENTAL SOURCE
├── OSCILLATOR       SINE / TRIANGLE / SAW / SQUARE / CUSTOM WAVETABLE
├── FM ENGINE        CARRIER, MODULATOR, FM AMOUNT, FM RATIO
├── NOISE ENGINE     WHITE / PINK / DIGITAL
└── RING MOD SOURCE
```
The FM ENGINE must be a REAL FM generator (phase modulation of a carrier by a modulator), not a distortion named FM.

### B. EXPERIMENTAL PROCESSING MATRIX – a reorderable chain, not a fixed one
Processors: FORMANT FILTER (vowels, resonances), FREQUENCY SHIFTER (metallic shifts), RING MOD (bell-like, inharmonic),
PITCH SHIFTER (pitch shift), WAVESHAPER (nonlinear shaping), BITCRUSHER (digital texture), PHASER (phase movement),
COMB FILTER (metallic resonances), STRETCH (stretching the texture), RESONATOR (frequency emphasis).
Every processor has ON / OFF, MIX, AMOUNT, its own characteristic params, and modulation of selected params.
The order MUST be changeable (freq shifter before distortion ≠ distortion before freq shifter).
**Max 8 active FX slots** in one chain, reorderable. (RAW instead = a FIXED optimised chain with stages switchable on / off.)

### C. EXPERIMENTAL MODULATION ENGINE
Every processor can get modulation from an independent source: ENVELOPE, LFO 1, LFO 2, RANDOM, KICK POSITION, MANUAL CURVE.
E.g. LFO 1 → FORMANT FREQUENCY, LFO 2 → FM AMOUNT, ENVELOPE → DISTORTION DRIVE, RANDOM → FREQUENCY SHIFT.
RANDOM must be deterministic per MIDI note / event in FIXED SEED mode – never a different sound on each render of the same kick.

### D. EXPERIMENTAL PITCH ENGINE
RAW: a characteristic, fairly controlled tonal movement. EXPERIMENTAL: much more freedom.
PITCH CURVE: LINEAR DOWN / LINEAR UP / EXPONENTIAL DOWN / EXPONENTIAL UP / MULTI-STEP / SINE MODULATION / RANDOM / CUSTOM.
Params: START PITCH, END PITCH, PITCH RANGE, PITCH TIME, CURVE, MODULATION RATE, MODULATION DEPTH.
Plus independent modulation of the FM frequency and the frequency shifter.

### E. STEREO
RAW: a stable centre, controlled width of the harmonics, mono lows.
EXPERIMENTAL: modulated width, stereo phaser, stereo frequency shifter, independent mid / side processing, width automation over time.
Mandatory protection: LOW → MONO, MID → USER CONTROLLED, HIGH → STEREO FX. Random phase modulation must never lose the fundamental.

## 4. KICK LENGTH sync (both types)
Every time-modulated parameter uses the one shared TAIL time axis. Two modes: **SYNC** (every envelope / modulation scales
to the TAIL length) and **FREE** (times set independently). Keep **SYNC TO BODY END** for processes that react to the end of
the BODY. Example over 200 ms (illustration, not DSP values): RAW FILTER SWEEP ~0–165 ms, DISTORTION ENVELOPE ~0–180,
RESONANCE MOVEMENT ~0–135, REVERB SEND ~0–110.

## 5. TAIL macros – one shared set, different mapping
| macro | RAW | EXPERIMENTAL |
|---|---|---|
| DRIVE | distortion aggression | nonlinearity intensity |
| TONE | brightness and growl | colour and spectrum |
| MOVEMENT | filter sweep | modulation |
| RESONANCE | growl / mid peak | formant / comb |
| DENSITY | harmonic density | texture complexity |
| SPACE | subtle reverb | spatial FX |
| CHAOS | distortion instability | random / FM / glitch |
| WIDTH | controlled stereo | creative stereo |
The existing 4GE macros stay compatible with the other modules.

## 6. RAW vs EXPERIMENTAL – implementation differences
| feature | RAW | EXPERIMENTAL |
|---|---|---|
| source | simple oscillators | oscillators + FM + noise |
| distortion | the main element | one of many processors |
| EQ | key | creative |
| filter sweep | mandatory engine | optional modulation |
| formant | optional | basic processor |
| ring mod | optional | basic processor |
| random | limited | extensive |
| stereo | controlled | creative |
| resampling | optional | an important sound-design element |

## 7. Technical requirements (mandatory)
Real-time oscillators; no allocation in the audio callback; smoothed automation (no clicks); oversampling for the nonlinear
processors; phase sync where needed; no uncontrolled DC; no uncontrolled level growth when cascading; every processor
bypassable; offline renders repeatable.

## 8. Acceptance tests
1. RAW keeps its character across MIDI notes. 2. RAW keeps its growl across kick lengths. 3. EXPERIMENTAL can make an
inharmonic, metallic tail. 4. The processor order really changes the sound. 5. Modulation makes no clicks / artefacts.
6. Stereo never destroys the mono low end. 7. Both engines work at different sample rates. 8. The same MIDI + preset =
the same render.

## 9. Final recommendation
- **RAW** – Aggressive – GROWL · CRUNCH · MOVEMENT: characteristic aggressive Rawstyle / Xtra Raw tails.
- **EXPERIMENTAL** – Creative – FM · FORMANT · CHAOS: sounds a classic distortion chain cannot easily make.
- **ZAAG stays its own engine**, never merged with RAW: RAW may use similar processors but does not inherit the ZAAG
  algorithm. Three really different TAIL engines, not three names for one distortion chain.
