# DOOMSDAY (read first)

Hardstyle plugins by ssonicsystemss (JUCE, VST3 for FL Studio on Windows): **Prophecy of Scream** (screech synth,
repo root), **Ancient Drums** (`AncientDrums/`, kick generator), **Fortune of Seekers** (`FortuneOfSeekers/`,
melody synth). Each builds separately via its own GitHub Actions workflow; user docs are the Polish `README.md` files.

## Shared toolbox (check first)
Reusable pieces for all ssonicsystemss plugins live in **ssonicsystemss/sonic-toolbox**
(https://github.com/ssonicsystemss/sonic-toolbox). Before building a screen, display, helper or DSP block, read its
`CLAUDE.md` and reuse what is there (e.g. `screens/`: 20 plugin display screens + `ScreenKit.h`). Attach or clone
it when needed. If you write something other plugins could use, suggest adding it to the toolbox.
