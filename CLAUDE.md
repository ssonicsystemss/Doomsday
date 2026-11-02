# DOOMSDAY (read first)

Hardstyle plugins by ssonicsystemss (JUCE 8, VST3 for FL Studio on Windows). Owner: Polish hardstyle producer, not a
programmer – answer in Polish, simply; judge sound by his ears, not only by tests. User docs = Polish `README.md` per plugin.

| Plugin | Folder | What |
|---|---|---|
| Prophecy of Scream | repo root | screech synth – **frozen at v3.1** (last commit with PoS changes: `f745e5e`); a new screech synth will be written from scratch (see LESSONS) |
| Ancient Drums | `AncientDrums/` | kick generator (sample layers, WSOLA pitch) – own old code, keep as is |
| Fortune of Seekers | `FortuneOfSeekers/` | melody synth – own old code |
| Dark Serpent | `DarkSerpent/` | screamer / lead |
| Mirror of Souls | `MirrorOfSouls/` | 8-band EQ |
| Abyss of Echoes | `AbyssOfEchoes/` | reverb / delay / stereo |
| Throne of Titans | `ThroneOfTitans/` | multiband master + limiter + LUFS (LEARN) |
| 5CR34M 4LPH4 | `ScreamAlpha/` | new screech synth (successor of PoS) – **v0.4 test build** (engine `Source/AlphaEngine.h`, MOD 1/2 with ASSIGN + drawn shapes `Source/AlphaMod.h`, flat UI, owner presets 6/7/8); plan `PLAN.md`, offline prototype `Proto/`, reference analysis + matcher `Tools/` |
| Forge of Wrath | `ForgeOfWrath/` | hardstyle kick synth – **frozen at v2.1** (owner, 2026-10-23); its successor is 4GE |
| 4GE | `FourGE/` | new kick synth (successor of Forge) – **v0.4** (MAIN screen + ADVANCED tabs, UNISON / WIDTH: centre + L / R voices, lows mono, FULL presets): engine `Shared/Source/GeKick.h` (PUNCH types / filter / drive, TAIL osc A + B, pre filter + sweep, stage BIAS / MIX / AUTO GAIN, TEXTURE layer, SUB level, BUS dist, SOLO / MUTE), owner's Forge kicks ported as factory presets; reverse bass moved out to RE4GE |
| RE4GE | `Re4GE/` | reverse bass instrument (split from 4GE) – **v0.16** (step 7 part 2: REFERENCE page `Source/Analysis/RefAnalysis.h` (envelope / start / peak / 6 bands / hints, one beat) + `GUI/RefPanel.*` (drop a WAV, renders the plugin with `Exporter::render` on a background thread); MORPH choices / switches jump at 50 %; part 1: MORPH A / B – slots in the state child `MORPH` (A / B, normalized values by param ID), `gatherParams (…, MorphView*)` interpolates in 0..1 from a double buffer (`morphRebuild`), `GUI/MorphBar.*` in the top bar; step 6 part 4: EXPORT WAV `Source/Export/Exporter.h` on a background thread with copies of params + sample, `GUI/ExportPanel.*` (drag into FL), preset CATEGORIES = name prefix (`ReProcessor::presetCategory`); part 3: POST EQ 8 bands `DSP/Eq8.h` per chain BEFORE the sub (low cut never cuts the sub) + COMP, `GUI/EqView.*`, Dynamics page; part 2: 16-step SEQUENCER in `DSP/Trigger.h` on the host grid (`Engine::setHostPpq`, song step = ppq / step) + rhythmic GATE in `DSP/Engine.h`, `GUI/SeqView.*`; new params appended AFTER the FX block (FL automation by index); part 1: RANDOMIZER `Source/Randomizer/` – MODE / STYLE / AMOUNT / LOCK, musical ranges + rules, loudness matched by an offline render in a private engine, 1 click = 1 undo; page `GUI/RandomPanel.*`, settings = `rnd*` state properties) + **RE4GE FX** (the same code as an effect: `ReProcessor (fx = true)`, target `Re4GEFX` with `RE4GE_FX=1`, `DSP/HitDetector.h`, LIVE reverse of the previous kick; KEEP LENGTH pitch `DSP/GrainReader.h`); step 5 part 1: KICK-TO-REVERSE `DSP/KickAnalysis.h` + `GUI/KickView.*`, sample region, ORIGINAL / PROCESSED, sample embedded in the state (`ReProcessor::restoreSample`); `RenderTest <out> --analyse <wav>`; step 4: LFO 1–4 + 16-row MOD MATRIX `DSP/ModMatrix.h` applied per grid point on a copy of the settings, `GUI/MatrixView.*`; `ParamBridge` records the parameter pointers once (no lookups / allocation on the audio thread); step 3: DISTORTION LAB per stage `Shared/Source/DoomsdayLab.h` (ADAA curves + custom table) in `DSP/ToneChain.h`, `GUI/WaveshaperView.*`, curve files `Presets/ShapeFiles.h`, QUALITY 2–16x oversamplers made once in `DSP/Engine.h`; step 2: SHAPE DESIGNER `DSP/ShapeLanes.h` + `GUI/ShapeDesigner.*`, curves in the state via `ReProcessor::setShape` (undoable, ValueTree listener -> spin-locked copy for audio); step 1 part 2: SUB oct / harmonics / sat, CLICK, GLIDE, SMEAR diffuser `DSP/LowEnd.h`; part 1: OSC 2–4 `DSP/OscBank.h`, 8 macros `DSP/Macros.h`, stage 2, pre filter ENV / KEY, new GUI with NAV + MAIN + macros; `RenderTest <out> --render <settings> <wav>` for matching; PLAN step 0 done: components `Source/DSP/` (Engine, Trigger, Envelope, Oscillator, KickTail paced render, ToneChain, LowEnd), `Parameters/` (IDs FROZEN: `Tests/param_ids.txt`, legacy map), `Presets/`, `GUI/`; 32-sample grid; sound = v0.2) – stereo: centre + L / R chains with UNISON / WIDTH, sub on the note, tail ROOM; PATTERN / GATE trigger, SYNTH + PHASE WARP / kick TAIL backwards / SAMPLE, one envelope with templates, pitch modes, tone chain with 4GE stages, SUB crossover; **v0.3+ plan = `Re4GE/PLAN.md`** (owner's full spec, build order, 5 signature features; synthesis first, reversing is one source) |

## Start every task here
1. `Shared/README.md` – catalogue of reusable DSP / UI / params blocks. Reuse before writing new code.
2. `Shared/LESSONS.md` – what sounded bad and why. Add an entry after every owner complaint.
   `Shared/RESEARCH.md` – analyses of other plugins (Kick 2, Pro-Q 4, Pro-L 2, Valhalla, Spire, Vital, ShaperBox, soothe2, OTT, Ozone,
   Saturn 2, Trash 2, Gross Beat, Maximus, Kickstart 2, Diva, Decapitator, Pro-C 2, Sytrus, Massive / Massive X, hardstyle kick technique (NI), dkd (Damian: kick + reverse bass, host handling);
   Serum 2 / Sylenth1 in LESSONS) + **the idea backlog per Doomsday plugin at its end** – check it when the owner asks "what next".
   `Shared/DESIGN.md` – chosen GUI look (Minimal / flat): colours, fonts, knob / graph / button patterns.
   Damian's knowledge base (read only, clone `ssonicsystemss/sonic-toolbox`): `AVOID.md` (one rule per line) + `bugs/` (FL crashes,
   install, Windows test traps) + `INDEX.md` – check before releasing a build.
3. The plugin's `README.md` (features, presets) and its `Tests/RenderTest.cpp` (what is already checked).
4. Analysis / matching tools (Python): `ForgeOfWrath/Tools` (kick matching vs references, A/B mp3, reference feature CSVs),
   `Tools/screech` (Prophecy of Scream screech matching), `Shared/Tools` (small band / f0 reports). Reference WAVs are the owner's – not in git.

## Build / test (Linux container)
- JUCE: `ln -sfn /home/claude/AncientDrums/JUCE <Plugin>/JUCE` (or clone JUCE 8.0.10); **remove the symlink before committing**.
- `cd <Plugin> && CXXFLAGS="-DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0 -DJUCE_USE_XRENDER=0" cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPROPHECY_BUILD_TESTS=ON`
- `ninja -C build RenderTest && xvfb-run -a build/RenderTest_artefacts/Release/RenderTest <outdir>` → checks + GUI snapshot PNG (look at it).
- Shared blocks have their own tests: `cd Shared/Tests` (same JUCE symlink + cmake flags) → `ninja -C build && build/SharedTests_artefacts/Release/SharedTests`.
- CI: every plugin has `.github/workflows/build-<name>.yml` (template `Shared/templates/build-workflow.yml`); new plugin folders
  go into `paths-ignore` of `build-windows.yml`. Status: `gh run list -R ssonicsystemss/Doomsday`. Download = run page → Artifacts.

## Conventions
- New plugin: `doomsday_plugin(...)` from `Shared/cmake/Doomsday.cmake`, fixed-size canvas scaled by the editor, logo via
  `doomsday::brand::drawHeaderBadge (g, defaultW / W, 0.55f)` top-left, version text at `headerTextX`, `PresetBar`, English UI text + tooltips.
- Real-time safe audio thread (no alloc / locks / IO); coefficients per block, not per sample.
- **DSP, GUI and file / preset management never in one component** (owner, 2026-11-01): DSP knows no GUI or files, GUI does no DSP
  (previews in a background thread), files never on the audio thread. Every visible control must change the sound (test min vs max).
- Never change existing parameter IDs (saved projects); new params get new IDs.
- After sound changes give the owner an A/B file (reference vs plugin) and ask what sounds wrong.

## Rules
- Git: push straight to `main` (owner's choice); no branches / PRs unless asked. Commit as `ssonicsystemss <tym222q@gmail.com>`.
- **ssonicsystemss/sonic-toolbox belongs to Damian: do not push / merge into it.** The owner allows using it to learn and to
  borrow ideas or code into our `Shared/` (note the source). Our shared code lives in `Shared/`. Lessons taken from it: `Shared/LESSONS.md`.
- Old repos (pepula-GSB/*) are backups only.
