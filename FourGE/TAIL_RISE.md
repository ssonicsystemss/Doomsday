# 4GE – TAIL RISE (owner's spec 2026-11-21, BPM decision the same day)

The end of the TAIL grows instead of only fading. Code: `Source/DSP/Tone/TailRise.h`, params `rs_*`, tail tab RISE (with every TAIL TYPE).

**Decision (owner 2026-11-21):** no sync to the MIDI note length. A plugin learns a note's length only at NOTE OFF, so it cannot place a
rise at "75 % of the note". RISE is synced to the **BPM and a musical grid** instead:
- the schedule is known at NOTE ON;
- it is the same in playback and in the FL render;
- NOTE OFF stays fully independent (KICK END as before).

- **SYNC:**
  - BPM: start = GRID × START, end = GRID; GRID is 1/16, 1/8, 1/4 = one beat, 1/2 or 1 BAR; a tempo change moves the rise by itself;
  - TAIL: start and end are parts of the kick length (one-shot).
- **CURVE:** LINEAR / EXPONENTIAL / S-CURVE / FAST.
- **END:** FADE 30 ms / HARD STOP 3 ms (no click) / TAIL FOLLOW (holds until the kick ends). The rise never extends the kick.
- **MODE** sets the layers; AMOUNT scales them:

  | Mode | Layers |
  |---|---|
  | POWER | gain + a little filter |
  | RAW | drive + harmonics + a little gain and noise |
  | PITCH | pitch + a little gain |
  | HARMONIC | harmonics + filter |
  | HYBRID | the user's own amounts for GAIN / PITCH / DRIVE / HARMONICS / FILTER / NOISE |
- **Layers:** separate, all driven by one envelope.
  - GAIN: up to +12 dB, with a soft knee at 1.4 × the tail's own peak from before the rise (peak control).
  - PITCH: moves the tail generators and the tail engines themselves, up to PITCH RANGE. It is not a pitch shifter.
  - DRIVE: compensated, so it adds aggression, not level.
  - HARMONICS: an exciter on the highs.
  - FILTER: an opening high shelf.
  - NOISE: follows the tail's level.
- **Style starting values (spec 9):**

  | Style | Rise |
  |---|---|
  | EARLY | off |
  | MODERN | POWER 35 % |
  | RAW | RAW 55 % |
  | EXPERIMENTAL | HYBRID 65 % |
  | EUPHORIC | HYBRID (gain + pitch) 30 % |
  | PVC | POWER 25 % |
  | ZAAG | HARMONIC 45 % |
- RenderTest checks:
  - the window follows the tempo (150 vs 200 BPM);
  - nothing changes before the window;
  - peaks stay controlled;
  - PITCH goes up;
  - the style defaults are set.
