# 4GE – OFFICIAL ARCHITECTURE (owner, 2026-11-14) – read this before touching 4GE

The owner's rule: **Kick → Punch → Tail → Processing.** New sound-design techniques go INTO a drawer below, never as one
more module on top. Nothing that exists is thrown away – it is moved into its drawer. DSP may keep its 12+ modules
underneath; the GUI stays simple.

## 1. Hard definitions (never mix them)

| term | what it is |
|---|---|
| **CLICK** | the micro-transient at the very start (0–3 ms) |
| **TOK** | an optional tonal layer BEFORE the punch ("pup"); its own layer, not part of PUNCH |
| **PUNCH** | the first part of the kick: the attack, the character, the hit |
| **SUB PUNCH** | the delayed low end that belongs to the hit |
| **BODY** | the transition from the punch into the tail (its own generator / envelope) |
| **TAIL** | the main tonal mass after the punch |
| **SCREECH** | a separate layer of the higher-frequency character |

Kick timeline (every engine respects it):

```
0 ms ─ CLICK ─ TOK ─ PUNCH ─ SUB PUNCH ─ BODY ─ TAIL ─ DECAY
```

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
│   ├── CLICK            psh_click, psh_click_len, psh_click_tone          (shared)
│   └── SUB PUNCH        psh_sub, psh_sub_*                                (shared)
├── 03 TAIL ENGINE       TAIL TYPE (tail_engine)
│   ├── CLASSIC          tail_*, body_* (body = its own part)
│   ├── PVC              pt_*, gt_*
│   └── ZAAG             zg_*   (rebuild in progress: 2 saws -> pitch -> EQ/waveshape x3 -> final EQ + SQUASH -> soft clip -> filter movement -> shape)
├── 04 CHARACTER / MOVE  screech (scr_*, m06 filters), resonance, filter movement, modulation (module 09)
├── 05 MIX / SPACE       core mixer (mix_*), layers (module 04), stereo (spc_*), room / reverb, EQ (m07)
└── 06 MASTER            distortion pool (dist*, per location), dynamics (m08), clipper, limiter, output
```

## 4. Rules for new work

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
4. owner names on labels  5. ZAAG tail rebuilt per the owner's spec (memory / Shared/RESEARCH.md)  6. TOK as its own layer before PUNCH
