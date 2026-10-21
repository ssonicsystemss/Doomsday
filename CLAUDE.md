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
| Forge of Wrath | `ForgeOfWrath/` | hardstyle kick synth (v2: dist chain, room, OTT, drawn pitch, sample layer); preset tuning tools in `Tools/` |

## Start every task here
1. `Shared/README.md` – catalogue of reusable DSP / UI / params blocks. Reuse before writing new code.
2. `Shared/LESSONS.md` – what sounded bad and why. Add an entry after every owner complaint.
   `Shared/RESEARCH.md` – analyses of other plugins (Kick 2, Pro-Q 4, Pro-L 2, Valhalla, Spire, Vital, ShaperBox, soothe2, OTT, Ozone,
   Saturn 2, Trash 2, Gross Beat, Maximus, Kickstart 2, Diva, Decapitator, Pro-C 2, Sytrus, Massive / Massive X, hardstyle kick technique (NI);
   Serum 2 / Sylenth1 in LESSONS) + **the idea backlog per Doomsday plugin at its end** – check it when the owner asks "what next".
   `Shared/DESIGN.md` – chosen GUI look (Minimal / flat): colours, fonts, knob / graph / button patterns.
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
- Never change existing parameter IDs (saved projects); new params get new IDs.
- After sound changes give the owner an A/B file (reference vs plugin) and ask what sounds wrong.

## Rules
- Git: push straight to `main` (owner's choice); no branches / PRs unless asked. Commit as `ssonicsystemss <tym222q@gmail.com>`.
- **ssonicsystemss/sonic-toolbox belongs to Damian: do not push / merge into it.** The owner allows using it to learn and to
  borrow ideas or code into our `Shared/` (note the source). Our shared code lives in `Shared/`. Lessons taken from it: `Shared/LESSONS.md`.
- Old repos (pepula-GSB/*) are backups only.
