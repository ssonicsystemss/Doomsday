# 4TUNE
### Professional Hardstyle Melody Synthesizer

**Developer:** Sonic Systems  
**Instrument Type:** VST3 / AU Software Synthesizer  
**Primary Purpose:** Hardstyle melody generation, sound design and MIDI composition  
**Core Architecture:** 4 Oscillators / 2 Groups  
**Signature Macro:** FORTUNE

---

# 1. PRODUCT CONCEPT

**4TUNE** is a professional software synthesizer designed specifically for the creation of hardstyle melodic content, including plucks, leads, supersaws, euphoric melodies, festival leads and aggressive melodic sounds.

The instrument is built around **four independent oscillators divided into two oscillator groups**.

The central design philosophy of 4TUNE is:

> **Create a musically useful and pleasant sound at low FORTUNE values, then progressively transform it into a bright, wide and aggressive festival-ready sound as FORTUNE increases.**

The instrument must not behave like a simple preset generator. Every major parameter must remain independently controllable, while the **FORTUNE macro provides an intelligent macro-layer that simultaneously transforms multiple aspects of the sound**.

---

# 2. CORE OSCILLATOR ARCHITECTURE

4TUNE contains **four independent oscillators**.

The oscillators are divided into two groups:

### GROUP A — CORE

- OSC 1
- OSC 2

Purpose:

- fundamental tone
- main melody
- body
- initial pluck character
- primary harmonic content

### GROUP B — EXPANSION

- OSC 3
- OSC 4

Purpose:

- additional harmonics
- stereo width
- unison layers
- supersaw character
- high-frequency detail
- additional melodic character

The two groups must be independently controllable but capable of being routed together through a common signal path.

---

# 3. OSCILLATOR REQUIREMENTS

Each oscillator must provide independent control over:

### Pitch

- Octave
- Semitone
- Fine Tune
- Pitch modulation
- Transpose

### Waveform

The oscillator engine should support:

- Saw
- Sine
- Triangle
- Square
- Custom waveform / wavetable architecture where supported

The architecture should allow future expansion of the waveform system without redesigning the oscillator module.

### Level

Each oscillator must have an independent output level.

### Stereo

- Pan
- Stereo Spread

### Phase

- Phase Position
- Phase Randomization
- Free-running / retrigger behavior

### Unison

Each oscillator must have independent:

- Voice Count
- Detune
- Stereo Spread
- Unison Mix

This is important because OSC 1 may need to remain relatively focused while OSC 3 and OSC 4 provide the wide supersaw layer.

---

# 4. OSCILLATOR GROUPS

Each oscillator group must contain an independent mixer.

## GROUP A MIXER

Controls:

- OSC 1 Level
- OSC 2 Level
- Group Volume
- Group Pan
- Group Stereo Width

## GROUP B MIXER

Controls:

- OSC 3 Level
- OSC 4 Level
- Group Volume
- Group Pan
- Group Stereo Width

Each group must provide an independent **Group Output** that can be routed to the filter and effects architecture.

---

# 5. FILTER SYSTEM

4TUNE must contain a professional multimode filtering system.

At minimum:

- Low Pass 12 dB/oct
- Low Pass 24 dB/oct
- High Pass 12 dB/oct
- High Pass 24 dB/oct
- Band Pass
- Notch

Main parameters:

- Cutoff
- Resonance
- Drive
- Key Tracking
- Filter Envelope Amount

The filter must support smooth parameter interpolation to prevent audible clicks or zipper noise during automation.

---

# 6. FILTER ENVELOPE

The filter envelope is one of the most important components of the 4TUNE sound.

It must support:

- Attack
- Hold
- Decay
- Sustain
- Release
- Amount
- Envelope Curve

The primary purpose is to allow the creation of a controlled pluck transient:

**high initial cutoff → rapid decay → controlled sustained brightness**

The filter envelope must remain musically predictable across different note lengths.

---

# 7. AMPLITUDE ENVELOPE

4TUNE must provide a dedicated amplitude envelope.

Parameters:

- Attack
- Hold
- Decay
- Sustain
- Release
- Curve

The envelope must support both:

### Pluck Mode

Short attack and decay with controlled sustain.

### Lead Mode

Longer attack/release with increased sustain.

Envelope timing must be independent of host BPM unless tempo synchronization is explicitly enabled.

---

# 8. MODULATION ENVELOPE

A third envelope must be available for sound-design modulation.

Possible destinations:

- Pitch
- Fine Tune
- Cutoff
- Resonance
- Waveform Position
- Warp
- Unison Detune
- Stereo Width
- SCREAM
- Delay
- Reverb

The modulation envelope must have independent timing parameters.

---

# 9. LFO SYSTEM

4TUNE should provide at least two independent LFOs.

Each LFO should support:

- Sine
- Triangle
- Saw
- Ramp
- Square
- Sample & Hold
- Random

Parameters:

- Rate
- Depth
- Phase
- Fade In
- Sync
- Retrigger

Tempo synchronization should support common musical divisions such as:

- 1/1
- 1/2
- 1/4
- 1/8
- 1/16
- 1/32
- dotted values
- triplet values

---

# 10. MODULATION MATRIX

4TUNE must include a flexible modulation routing system.

## Modulation Sources

- AMP ENV
- FILTER ENV
- MOD ENV
- LFO 1
- LFO 2
- Velocity
- Mod Wheel
- Aftertouch
- Key Tracking
- Random
- FORTUNE

## Modulation Destinations

- Oscillator Pitch
- Fine Tune
- Waveform Position
- Warp
- Cutoff
- Resonance
- Unison Detune
- Stereo Spread
- Pan
- Volume
- SCREAM
- Delay
- Reverb
- Other exposed synthesis parameters

Each modulation connection must have an adjustable amount.

The system should support both positive and negative modulation.

---

# 11. SCREAM SYSTEM

SCREAM is a dedicated nonlinear sound-processing module.

It must not function as a simple conventional distortion knob.

Its purpose is to introduce controlled harmonic excitation, saturation and aggressive high-frequency character while maintaining musical usability.

Primary parameters:

- SCREAM
- Drive
- Saturation
- Tone
- Bite
- Body
- Mix
- Output

Optional processing stages may include:

- Soft Clipping
- Waveshaping
- Wavefolding
- Harmonic Excitation
- High-Frequency Saturation

SCREAM should be capable of producing:

### LOW

Subtle harmonic enhancement.

### MEDIUM

Aggressive melodic character.

### HIGH

Strong distorted / screaming hardstyle lead character.

The module must include appropriate gain compensation so increasing SCREAM does not automatically produce an uncontrolled increase in perceived loudness.

---

# 12. SIGNATURE FORTUNE MACRO

## FORTUNE

FORTUNE is the signature macro control of 4TUNE.

It must transform the sound progressively from:

**clean / controlled / pleasant**

into:

**bright / wide / aggressive / festival-ready**

FORTUNE is not a simple volume or brightness control.

It is a **multi-parameter macro modulation system**.

---

# 13. FORTUNE — FILTER TRANSFORMATION

As FORTUNE increases:

**Filter Cutoff must progressively increase.**

At maximum FORTUNE:

- cutoff may reach up to approximately **+2 octaves relative to the configured base range**
- the perceived brightness of the melody increases significantly

The exact scaling should be non-linear and musically optimized.

The goal is:

**low FORTUNE → controlled pluck**

**medium FORTUNE → brighter melodic lead**

**high FORTUNE → extremely open and bright festival sound**

FORTUNE must not completely replace the user's manual cutoff setting.

It must operate as an additional modulation layer.

---

# 14. FORTUNE — UNISON TRANSFORMATION

FORTUNE progressively increases oscillator unison detune.

This creates a transition from:

**focused / narrow sound**

to:

**wide / supersaw-like sound**

The effect should primarily affect oscillators configured with multiple unison voices.

The amount of FORTUNE influence on unison detune must be controlled internally to prevent excessive pitch instability.

OSC 1 and OSC 2 may remain relatively focused while OSC 3 and OSC 4 can become significantly wider.

---

# 15. FORTUNE — SCREAM TRANSFORMATION

FORTUNE must automatically increase the SCREAM intensity.

The response must be non-linear.

If SCREAM is already active:

**FORTUNE increases it significantly.**

If SCREAM is near zero:

**FORTUNE introduces only a controlled amount of SCREAM.**

This prevents the macro from transforming a clean sound into an excessively distorted sound immediately.

Example behavior:

**SCREAM = 0%**

FORTUNE may introduce a subtle amount of harmonic aggression.

**SCREAM = 50%**

FORTUNE significantly increases the aggressive character.

**SCREAM = 100%**

FORTUNE drives the existing aggressive character toward its upper range.

---

# 16. FORTUNE — REVERB TRANSFORMATION

FORTUNE progressively increases spatial depth.

Maximum influence:

### REVERB MIX

**+15 percentage points**

### REVERB SIZE

**+10 percentage points**

The purpose is not to create an obviously wet sound.

Instead, the sound should gradually move toward a larger festival-scale spatial character.

---

# 17. FORTUNE — GLOBAL BEHAVIOR

FORTUNE must produce a coherent transformation.

The transformation should feel like a single sound-design process rather than several unrelated automation movements.

The desired perceptual progression is:

### 0–20%

Controlled melodic pluck.

### 20–40%

Brighter and more expressive.

### 40–60%

Increasing stereo width and harmonic energy.

### 60–80%

Wide, bright and aggressive.

### 80–100%

Large festival-style melodic lead.

The exact response curve should be configurable internally.

---

# 18. FORTUNE PRIORITY SYSTEM

Manual parameter values always remain valid.

FORTUNE operates as a secondary modulation layer.

Conceptually:

**FINAL PARAMETER = BASE PARAMETER + FORTUNE MODULATION**

The system must prevent parameter overflow and define safe maximum values.

Example:

If the user sets SCREAM to 40%:

FORTUNE does not replace 40% with a completely new value.

Instead:

**FORTUNE adds controlled additional SCREAM.**

This principle must apply to all FORTUNE-controlled parameters.

---

# 19. FORTUNE CUSTOMIZATION

Advanced users should optionally be able to configure which parameters are affected by FORTUNE.

Possible controls:

- Fortune → Cutoff
- Fortune → Detune
- Fortune → SCREAM
- Fortune → Reverb Mix
- Fortune → Reverb Size

Each destination should have an independent **Fortune Amount**.

This allows users to create alternative FORTUNE behaviors without removing the default signature behavior.

---

# 20. EFFECTS ARCHITECTURE

4TUNE should contain an integrated effects section.

Minimum modules:

### EQ

- Low
- Low Mid
- High Mid
- High

### Distortion / Saturation

- Drive
- Tone
- Mix
- Output

### Chorus

- Rate
- Depth
- Width
- Mix

### Delay

- Time
- Feedback
- Stereo / Ping Pong
- Filter
- Mix

### Reverb

- Pre-Delay
- Size
- Decay
- Damping
- Width
- Mix

### Stereo Processing

- Width
- Stereo Balance
- Mono Bass

### Limiter

- Input
- Ceiling
- Release

The effects architecture should support configurable routing where technically practical.

---

# 21. MELODY ENGINE

The melody-generation section is a core component of 4TUNE.

Its purpose is to help users generate musically coherent hardstyle melodies rather than random MIDI sequences.

The system must support:

### Scale Selection

- Major
- Minor
- Harmonic Minor
- Melodic Minor
- Dorian
- Phrygian
- Chromatic
- Custom Scale

### Musical Parameters

- Key
- Scale
- Tempo
- Phrase Length
- Note Density
- Complexity
- Repetition
- Movement
- Octave Range
- Rhythm Density

---

# 22. HARDSTYLE MELODY MODES

The generator should provide musical behavior presets such as:

- Classic Hardstyle
- Euphoric Hardstyle
- Rawphoric
- Melodic Raw
- Festival
- Emotional
- Anthem
- Dark
- Minimal

These modes should not simply select random notes.

They should modify the internal generation rules.

For example:

**Euphoric**

More melodic movement, larger intervals and stronger phrase resolution.

**Classic**

Simpler motifs, stronger repetition and more traditional rhythmic structures.

**Rawphoric**

More aggressive interval behavior and darker melodic structures.

---

# 23. PIANO ROLL

Generated melodies must be fully editable.

The integrated piano roll should allow:

- Add Note
- Delete Note
- Move Note
- Change Pitch
- Change Length
- Change Velocity
- Copy
- Paste
- Duplicate
- Quantize
- Humanize
- Transpose
- Change Octave

The generated melody must remain editable after generation.

---

# 24. MIDI OUTPUT

4TUNE must provide MIDI drag-and-drop functionality.

The user should be able to generate a melody and drag it directly from the 4TUNE interface into the DAW.

The MIDI output must preserve:

- Note Pitch
- Note Length
- Timing
- Velocity
- Musical Phrase

This allows 4TUNE to function both as an instrument and as a melody-composition tool.

---

# 25. HARDSTYLE-SPECIFIC PERFORMANCE FEATURES

4TUNE should support:

- Polyphonic operation
- Monophonic operation
- Legato
- Portamento / Glide
- Pitch Bend
- Mod Wheel
- Velocity
- Aftertouch where supported

Portamento should provide configurable glide time.

---

# 26. PROFESSIONAL DSP REQUIREMENTS

The audio engine must be designed for professional production use.

Required:

### Anti-Aliasing

Oscillator generation should minimize aliasing, particularly at high frequencies.

### Oversampling

Oversampling should be available for nonlinear processing such as:

- SCREAM
- Distortion
- Saturation
- Wavefolding

### Parameter Smoothing

Critical parameters must use smoothing where necessary:

- Cutoff
- Resonance
- Volume
- Pan
- Detune
- Mix
- Drive

This prevents zipper noise and clicks during automation.

### Gain Staging

Each major processing stage must maintain appropriate headroom.

### CPU Optimization

Voice processing should be optimized for real-time DAW use.

### Preset Recall

All parameters must be correctly restored when a DAW project is reopened.

---

# 27. PRESET SYSTEM

4TUNE should include a professional preset browser.

Categories:

- Plucks
- Leads
- Supersaws
- Pads
- Bass
- Keys
- Experimental
- Festival
- Euphoric
- Raw
- Classic
- User

Presets should support:

- Save
- Save As
- Rename
- Delete
- Favorite
- Search
- Category
- Author

---

# 28. RANDOMIZATION

4TUNE should provide two randomization modes.

### RANDOM

Fully randomized sound-design parameters.

### SMART RANDOM

Randomizes parameters while maintaining musically usable relationships between them.

Smart Random must avoid destructive combinations such as:

- excessive oscillator detune
- excessive resonance
- excessive distortion
- uncontrolled stereo width
- excessive reverb
- excessive output gain

---

# 29. USER INTERFACE PRINCIPLE

The interface must communicate the architecture clearly.

The user should immediately understand:

**OSCILLATORS → FILTER → SCREAM → FX → OUTPUT**

while the melody system operates as an integrated composition layer.

The FORTUNE control should have a visually dominant position because it is the signature function of 4TUNE.

The interface should communicate the transformation:

**CONTROLLED → BRIGHT → WIDE → AGGRESSIVE → FESTIVAL**

without requiring the user to understand the underlying DSP architecture.

---

# 30. CORE PRODUCT IDENTITY

4TUNE should not attempt to replace every synthesizer.

Its specialization is deliberate:

> **4TUNE is a hardstyle-focused melodic synthesizer and melody-generation instrument designed to turn simple musical ideas into polished, expressive and festival-ready hardstyle sounds.**

The defining workflow is:

**CREATE → SHAPE → PLAY → FORTUNE → EXPORT**

A user should be able to start with a simple pluck, construct a melody, increase FORTUNE and transform the sound into a wide, bright and aggressive hardstyle lead without manually adjusting multiple unrelated parameters.

---

# 31. SIGNATURE FEATURE SUMMARY

The defining feature of 4TUNE is **FORTUNE**.

FORTUNE simultaneously controls:

- Filter brightness
- Unison detune
- Stereo width
- SCREAM intensity
- Reverb Mix
- Reverb Size

while respecting the user's underlying sound design.

The result should be perceived as a **continuous musical transformation**, not as a collection of automated parameter changes.

### FORTUNE 0%

Controlled, pleasant, focused pluck.

### FORTUNE 50%

Brighter, wider and more expressive melodic sound.

### FORTUNE 100%

Wide, bright, harmonically rich, aggressive and festival-ready hardstyle lead.

This transformation is the primary sonic identity of **4TUNE**.