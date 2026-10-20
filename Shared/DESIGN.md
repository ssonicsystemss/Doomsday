# Doomsday – visual direction

Owner's choice (2026-10-21): **C · Minimal / flat**, picked over Hardware / vintage and Neon / festival.
Mockups: Design canvas "Prophecy of Scream – graphic directions" (private artifact of the owner, boards A / B / C + pattern sheet).
Code: `Shared/Source/DoomsdayFlat.h/.cpp` (`doomsday::flat`, see Shared/README.md). First plugin rebuilt in this look: **Prophecy of Scream v3.1**
(2026-10-21) – use its `Source/PluginEditor.cpp` as the example (top bar, `ResponseGraph`, `OscCard` with tabs, bottom section cards).
Keep the brand badge (`doomsday::brand::drawBadge` in the 56 px top bar).

## Tokens
| Token | Value | Use |
|---|---|---|
| ground | `#16181c` | window background |
| surface | `#1b1e23` | panels, graph background |
| control | `#1d2025` / border `#2c3138` | buttons, knob track |
| divider | `#262a30` | grid lines, top bar border |
| text | `#e8eaed` | values, titles |
| text-dim | `#9aa1ab` | section titles, inactive segments |
| text-mute | `#7d848e` | parameter labels, axis labels |
| hot accent | `#ff5a36` | the one main colour (scream / drive / primary button) |
| cool accent | `#4ea1ff` | secondary (osc / filter / curves) |

Rule: one hot + one cool accent per plugin; no gradients, no glow, no textures.

## Type
Inter only: titles 16 px / 700, section titles 12 px / 700 letter-spacing 1.5, values 13–16 px / 600, labels 10–12 px.
(In JUCE: embed Inter as BinaryData, or fall back to the default sans.)

## Patterns
- **Knob**: thin 270° arc (track `#2c3138`, value arc in accent, 4–6 px, round caps), no knob body;
  numeric value under it (600 weight) and label under that (muted). Modulation ring = second thin arc outside.
- **Slider**: 8 px rounded track, accent fill, white 18 px dot, value text below.
- **Buttons**: 8 px radius; primary = accent fill with dark text; secondary = control colour + 1 px border.
- **Segmented control** for modes (LP / BP / HP, dist types) instead of combo boxes where ≤ 5 options.
- **Big graph first**: the main display (formant / EQ / pitch curve) takes ~half the height, with draggable white-ring nodes
  and a small value tag (`1.84 kHz · +9.2 dB · Q 4.0`) next to the active node; faint spectrum bars behind the curve.
- **Panels**: 12 px radius surface cards, 12 px gaps, title top-left, knobs in a 4-column grid.
- **Top bar**: 56 px, brand left, plugin name + version, preset `< name >` centred-right, one primary action.

## Colour roles in a plugin (as in PoS v3.1)
- Two parallel parts (OSC 1 / OSC 2) = hot / cool; the graph draws each part's curve in its colour.
- Global sections (voice, amp, fx, output) = neutral grey arcs `#b4bac3`; their on/off pills in the hot accent.
- Output meter: neutral fill, turns hot only near 0 dBFS.
- Spectrum bars behind curves: `#252a31` (barely visible, never compete with the curve).
