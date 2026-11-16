# 4GE – OFFICIAL ARCHITECTURE (owner, 2026-11-14) – read this before touching 4GE

The owner's rule: **Kick → Punch → Tail → Processing.** New sound-design techniques go INTO a drawer below, never as one
more module on top. Nothing that exists is thrown away – it is moved into its drawer. DSP may keep its 12+ modules
underneath; the GUI stays simple.

## 1. Hard definitions (never mix them)

| term | what it is |
|---|---|
| **CLICK** | the "ts": the spark of the attack, 0–10 / 20 ms. A module of its own (`clk_*`, `DSP/Front/Click.h`) |
| **TOK** | the "tok": its own layer BEFORE the punch, 10–80 ms – longer than the click, shorter than the punch. Not a "small punch", not a macro (`tk_*`, `DSP/Front/Tok.h`) |
| **PUNCH** | the "THUMP": the first part of the kick, the attack, the character, the hit (5–100 ms) |
| **SUB PUNCH** | a short low / lower-mid impulse that gives the hit, 10–70 ms. **SUB PUNCH ≠ SUB**: it may have a pitch envelope but is gone before the real SUB dominates (`spk_*`, `DSP/Front/SubPunch.h`) |
| **BODY** | the character: the transition from the punch into the tail (its own generator / envelope), from ~20 ms |
| **TAIL** | the tonal tail: the main mass after the punch |
| **SUB** | the weight: the whole low end / tail |
| **SCREECH** | a separate layer of the higher-frequency character |

Kick timeline (owner 2026-11-16; the layers overlap in time):

```
TOK        0 – 40 / 70 ms
PUNCH      5 – 100 ms
CLICK      0 – 10 / 20 ms
SUB PUNCH 10 – 70 ms
BODY      20 – ...
TAIL / SUB ...
```

Rule: **CLICK < TOK < PUNCH** in length. Structure and routing:

```
TOK → PUNCH ENGINE (PUNCH TYPE + CLICK + SUB PUNCH) → BODY → TAIL → SUB

TOK + PUNCH ──(ZAAG SEND zg_send, 0 = beside it)──► ZAAG PROCESSING (the TAIL TYPE ZAAG cascade)
CLICK → CLICK PROCESSING (drive / mix) → PUNCH BUS          (never through ZAAG: ZAAG would smear the transient)
```

**ZAAG and the TOK (owner 2026-11-16):** no separate, mandatory TOK in ZAAG. ZAAG is the character of a processed TAIL, not
a kind of attack; a full TOK on top of the ZAAG punch risks a double transient, piled-up energy, a clash with the TOK TYPE and
losing control of the shared PUNCH ENGINE. Instead ZAAG SEND mixes the zaag into the existing TOK / PUNCH. The ZAAG style never
switches the TOK on.

The FRONT (TOK / CLICK / SUB PUNCH) has its own levels: not under the PUNCH fader / mute, not through the punch group's FX;
it goes into the master clip with the rest. Every module defaults OFF (LEVEL -60 dB) so approved presets stay identical.

**TOK** = ONE TOK CORE (pitch mode + pitch → pitch envelope START → fast drop → END → source → tone → amp envelope
ATTACK / DECAY / LENGTH → DRIVE → LEVEL) and six SOURCES: SINE (clean), TRI (soft), SQUARE (through a band, TONE hollow /
wooden ↔ full / harsh), PUP (a saw on the pitch envelope = "pup!"), 909 (a sine body + a band-passed noise "tik" only at its
start), WOOD (an impulse → band-pass → two resonant modes, PITCH moves the resonance). PITCH MODE: FIXED (default, G3 + PITCH,
±24 st) / FOLLOW MIDI / RELATIVE (note + PITCH). Per style: EARLY low / mid, 25–45 ms; MODERN mid, 35–70; RAW mid / higher,
30–65; EXPERIMENTAL anything, 10–80. Never 110–125 ms as a standard.

**CLICK and SUB PUNCH are shared by every PUNCH TYPE as a system**: each PUNCH TYPE has its own PROFILE for them
(`DSP/Front/FrontProfiles.h`: offsets on top of the user's knobs – level, length, tone, drive). Example balances
(TOK / PUNCH / CLICK / SUB): EARLY ~75 / 100 / 45 / 85, MODERN ~50 / 100 / 60 / 60, RAW ~55 / 100 / 65 / 35, EXPERIMENTAL all
by the preset.

**INTERNAL vs GLOBAL (owner 2026-11-16).** INTERNAL click / sub punch = built into a PUNCH model (EUPHORIC `psh_click` / `psh_sub`,
PVC, ZAAG click / pup / thump) – part of its sound. GLOBAL CLICK / GLOBAL SUB PUNCH = the shared FRONT modules (`clk_*` / `spk_*`),
available with any PUNCH TYPE. In AUTO the engine asks the PUNCH TYPE profile (`FrontProfiles.h` `globalInAuto`) whether the global
layers are needed: EUPHORIC / PVC / ZAAG no (they have their own), the classic PUNCH / TOK yes. ON always adds them (even on top of the
model's own), OFF never. Character over symmetry.

**AUTO logic (owner 2026-11-16).** Every FRONT layer has AUTO / ON / OFF (`tk_mode` / `clk_mode` / `spk_mode`, default AUTO).
AUTO = the STYLE / preset / subtype decides (`tk_auto` / `clk_auto` / `spk_auto` + the layer's profile); ON / OFF = the user forces it,
and a style change never touches a forced layer. STYLE ENGINE → TOK PROFILE + CLICK PROFILE + SUB PUNCH PROFILE (no generators of
its own). Styles: EARLY / MODERN / RAW all three on (EARLY round tok, clear click, stronger short sub punch; MODERN precise tok, short
selective click, controlled sub punch; RAW harder tok, aggressive click, strong sub punch); EXPERIMENTAL by the preset; ZAAG optional
(off); EUPHORIC / PVC off (their models have their own click + sub). PUNCH SUBTYPES (EUPHORIC CLASSIC / 2010–2012 / FESTIVAL,
PVC CLASSIC / KLOENK / MODERN): global TOK AUTO by the profile, global CLICK OFF, global SUB PUNCH OFF – OFF = the extra global
layer only, never the click / sub built into the model. Other PUNCH TYPEs: by the profile.

**PUNCH SUBTYPES** (owner 2026-11-16) are NOT separate generators: they are sound PROFILES of the one PUNCH / TOK engine
that change several parameters at once (EUPHORIC CLASSIC / 2010–2012 / FESTIVAL, PVC CLASSIC / KLOENK / MODERN ...).
EUPHORIC CLASSIC = clean euphoric: attack medium, body strong, click moderate, sub punch strong, harmonics medium,
distortion light / medium, transient soft – "TOK → THUMP → a tonal, clean punch", never metallic.
2010–2012 = the older, harder hardstyle: attack fast, body strong, click strong, sub punch medium, harmonics medium-high,
distortion medium, transient clear – less smooth, raw and mechanical; a clear TOK → CLICK → PUNCH, not one packed mass.
FESTIVAL = big, wide, very readable for big systems: attack very clear, body very strong, click bright, sub punch strong,
harmonics high, distortion medium, more stereo in the highs. Size without more level: a strong transient, wide harmonics,
a controlled low end, stereo only above ~200–300 Hz.
PVC is a different family. PVC CLASSIC = short, dry, plastic-percussive: attack very fast, body short, click clear, sub punch
medium, harmonics medium-high, distortion medium, decay short. No big soft body: "TOK → K", not "TOK → THUUUMP".
KLOENK = the most characteristic profile: a resonant, hard, metal-plastic impact. Attack fast, body medium, click strong, sub
punch medium, RESONANCE high, distortion medium / high, decay medium. One concrete narrow resonant peak in the upper low-mid /
midrange (not broadband distortion) – that peak IS the "kloenk".
PVC MODERN = modern, very polished: big, aggressive and clean at once. Attack very fast, body strong, click very controlled,
sub punch strong, harmonics very high, distortion high, transient very precise, stereo controlled. NOT "more distortion":
CLEAN ATTACK → CONTROLLED PUNCH → DENSE HARMONICS → CLEAN LOW END (lots of harmonics, never a muddy low).

## 2. STYLE ≠ PUNCH TYPE ≠ TAIL TYPE

- **STYLE** = the character of the whole kick (EARLY / MODERN / RAW / EXPERIMENTAL). EUPHORIC / PVC / ZAAG stay in the
  STYLE list as ready RECIPES: a style only sets a PUNCH TYPE + TAIL TYPE + values; it never locks anything.
- **PUNCH TYPE** = the mechanism that makes the punch: EUPHORIC / PVC / RAW / MODERN / EXPERIMENTAL (+ later SUBTYPES,
  e.g. EUPHORIC: CLASSIC / 2010–2012 / FESTIVAL / CUSTOM; PVC: CLASSIC / KLOENK / MODERN).
- **TAIL TYPE** = the mechanism that makes the tail: CLASSIC / PVC / ZAAG / RAW / EXPERIMENTAL.
- Any combination is legal, e.g. STYLE MODERN + PUNCH EUPHORIC + TAIL ZAAG.
- After a TYPE is picked the GUI shows ONLY that type's controls (not 20 greyed knobs). SOURCE only where it makes sense.

## 3. Drawers and what is in them today

```
4GE
├── 01 KICK SETUP        style, length (core_length, len_*), key / MIDI (core_note*), level (core_master)
├── 02 PUNCH ENGINE      PUNCH TYPE (punch_mode)
│   ├── CLASSIC / TOK    punch_*                       (the old PUNCH / TOK generator)
│   ├── EUPHORIC         psh_* (source, mid, drive, asym, tape, shape, low cut)
│   ├── PVC              pvc_* (model KLOENK / TUBE)
│   ├── CLASSIC (EARLY)  cp_*  PUNCH TYPE CLASSIC (EARLY REWORK, EARLY_REWORK.md, DSP/Core/ClassicPunch.h): hybrid source + own transient, 3-stage pitch, pre-EQ > saturation > waveshaper > post-EQ > transient control; defaults fitted to the owner's 20 EARLY references
│   ├── (EUPHORIC)       psh_click*, psh_sub* = the EUPHORIC model's own click / sub (older, stays)
│   ├── FRONT: TOK       tk_*   (one TOK CORE, 6 sources, pitch mode)        (shared, its own layer before the punch)
│   ├── FRONT: CLICK     clk_*  (6 types, drive / mix, never through ZAAG)   (shared + per-type profile)
│   ├── FRONT: SUB PUNCH spk_*  (pitch env start -> end, gone before the SUB) (shared + per-type profile)
│   └── ZAAG SEND        zg_send (TOK + PUNCH into the ZAAG tail cascade)
├── 03 TAIL ENGINE       TAIL TYPE (tail_engine)
│   ├── CLASSIC          tail_*, body_* (body = its own part)
│   ├── PVC              pt_*, gt_*
│   ├── RAW              rw_* + the shared TAIL macros tl_*   TAIL TYPE RAW (owner 2026-11-16, TAIL_RAW_EXP.md): OSC 1 / 2 / NOISE, fixed stack PRE-EQ > DIST 1 > GROWL EQ > DIST 2 > BAND-PASS SWEEP > DIST 3 > POST-EQ > CLIP, GROWL / CRUNCH, reverb, sub, width (DSP/Tone/RawTail.h)
│   ├── EXPERIMENTAL     ex_* + tl_*   TAIL TYPE EXPERIMENTAL (TAIL_RAW_EXP.md, DSP/Tone/ExpTail.h): osc / wavetable, real FM, noise, ring source, 8 pitch curves, 8 re-orderable FX slots (10 processors), ENV / LFO 1 / LFO 2 / RANDOM (fixed seed) / KICK POS / CURVE, M/S stereo with mono lows
│   └── ZAAG             zg_*   ZAAG ENGINE (owner's tutorial, FourGE/ZAAG_ENGINE.md): VARIANT CLASSIC / RESONANT / BOUNCE / AGGRESSIVE, 8 ZAAG macros
├── 04 CHARACTER / MOVE  screech (scr_*, m06 filters), resonance, filter movement, modulation (module 09)
├── 05 MIX / SPACE       core mixer (mix_*), layers (module 04), stereo (spc_*), room / reverb, EQ (m07)
└── 06 MASTER            distortion pool (dist*, per location), dynamics (m08), clipper, limiter, output
```

## 4. Rules for new work

0. **ENGINES NEVER MIX (owner 2026-11-15).** Every PUNCH TYPE and TAIL TYPE is its own DSP file with its own parameters;
   no engine reads another engine's parameters or sound, no engine includes another engine's header (shared helpers like
   the biquad live in a neutral file). Punch things (click, piep, trask, punch reverb) live ONLY in PUNCH TYPEs, tail things
   ONLY in TAIL TYPEs. Switching a TYPE turns the previous engine off completely. A STYLE only picks a PUNCH TYPE + TAIL TYPE.

0b. **First separate PUNCH and TAIL engines built on real sound-design methods, then the common interface** (owner 2026-11-15).
   A TYPE may have VARIANTS (e.g. TAIL ZAAG → CLASSIC / RESONANT / BOUNCE / AGGRESSIVE) instead of 30 universal knobs.
0c. **ZAAG = a chain of dependent nonlinear processes, not a distortion preset**: every stage changes the signal for the next, no level
   matching in between; every movement in fractions of KICK LENGTH, never in ms; LOW → mono, HIGH → width (ZAAG_ENGINE.md).

1. A new technique = a new TYPE (or SUBTYPE) inside PUNCH or TAIL, or a block inside CHARACTER – never a free-floating module.
2. Parameter names carry their OWNER in the GUI label / name: PUNCH DRIVE, TAIL DRIVE, SCREECH DRIVE, MASTER DRIVE –
   never five knobs called "DRIVE". (Parameter IDs are frozen – only labels / names change.)
3. A preset stores everything: style, punch type + its parameters, tail type + its parameters, screech, mix, master
   (it does: the whole APVTS state + the `style` property).
4. RANDOMIZE stays inside the style; it may add effects; EXPERIMENTAL extras (fold, bit reduction) are never randomized on.
5. DSP, GUI and files never in one component (CLAUDE.md); every visible control changes the sound (Tests 14.11).

## 5. Target GUI (owner's sketch)

```
┌──────────────────────────────────────────────┐
│ 4GE          PRESET: 2012 EUPHORIC     SAVE  │
├──────────────────────────────────────────────┤
│ STYLE                                        │
├──────────────────────┬───────────────────────┤
│ PUNCH  [TYPE ▼]      │ TAIL  [TYPE ▼]        │
│ [its parameters]     │ [its parameters]      │
├──────────────────────┴───────────────────────┤
│ CLICK │ SUB │ BODY │ SCREECH │ MIX           │
├──────────────────────────────────────────────┤
│                   MASTER                     │
└──────────────────────────────────────────────┘
```

## 6. Build order (2026-11-14)

1. this file  2. PUNCH TYPE / TAIL TYPE drop-downs that SHOW only their type's controls  3. the main screen per the sketch
4. owner names on labels  5. ZAAG ENGINE per the owner's tutorial (done 2026-11-15, ZAAG_ENGINE.md)  6. TOK as its own layer before PUNCH
7. KICK LENGTH = a slider in the top bar (any length, automatable) + KICK END next to it (NOTE = a shorter note cuts, default)
