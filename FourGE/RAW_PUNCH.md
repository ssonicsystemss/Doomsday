# 4GE – PUNCH TYPE RAW (owner's spec 2026-11-21)

Two independent RAW punch architectures. They are **not** one engine with different values. Code: `Source/DSP/Core/RawPunch.h`.
Parameters: `rp_type` (which one), `rc_*` (RAW CLASSIC), `rk_*` (RAW 2015-2017 CRACKING). Each type shows only its own knobs. GUI: punch tab RAW.

| | RAW CLASSIC | RAW 2015-2017 / CRACKING |
|---|---|---|
| Structure | TOK / TRANSIENT → PUNCH → CRACK LAYER → BODY → TAIL | SNAP → CRACK GENERATOR → HARMONIC SHAPER → BODY COUPLING → BODY → TAIL |
| Character | KRAK → DUM: punch + a controlled crack | KRAK! → BRUTAL BODY: snap + crack + harmonic density |
| Crack | SOURCE → PRE-SHAPE → WAVESHAPER (sine fold) → HARD CLIP → BAND SHAPE → CRACK ENVELOPE | dark/bright tilt on the crack source → DENSITY folds (1–4) → controlled asymmetric distortion → EDGE (upper harmonics) → DYNAMIC response; level held, so DENSITY never raises the punch level |
| Body coupling | 0 = the crack is gated off before the body; 100 = the crack band slides down and melts into a low-mid bridge | a distorted bridge carries the crack's energy into the body (no hole) |
| User knobs | SNAP, CRACK, CRACK DECAY, BODY COUPLING (+ START / DROP / END / DECAY / DENSITY / CRACK FREQ / PUNCH DRIVE) | SNAP, CRACK, DENSITY, EDGE, DARK/BRIGHT, CRACK DECAY, BODY COUPLING (+ START / DROP / END / DECAY / DYNAMIC / BRIDGE DRIVE) |

- SNAP = how hard the start hits. CRACK = how harmonically aggressive it is. They are independent; RenderTest checks this.
- CRACKING: the crack decay follows the kick length.
- The FRONT profile: RAW has its own SNAP (no global click in AUTO) and a short, firm global sub punch.
- Acceptance presets (spec 12): `RAW PUNCH - Classic Clean / Classic Crack / 2016 Standard / 2016 Extreme`. Each is the RAW style kick
  with the RAW punch; the RAW tail comes in after the crack (`rw_attack` 28 ms).
- RenderTest checks:
  - SNAP vs CRACK independence;
  - DENSITY keeps the level;
  - BODY COUPLING fills the gap (40–90 ms);
  - in the 4 presets the crack adds presence at 5–40 ms and nothing at 150–350 ms, so it is a short event, never a continuous noise;
  - the note stays readable.
