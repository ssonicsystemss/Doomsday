# Shared – wspólne klocki pluginów DOOMSDAY

Spis dla kolejnych pluginów: przeczytaj ten plik, potem tylko potrzebne nagłówki.

## Jak użyć w nowym pluginie
- `CMakeLists.txt` pluginu: `include(../Shared/cmake/Doomsday.cmake)` + `doomsday_plugin(Target NAME "..." CODE Abcd CATEGORIES Instrument Synth SOURCES ... TEST Tests/RenderTest.cpp)`
  (JUCE 8.0.10, statyczny runtime MSVC, VST3 + Standalone, fonty w BinaryData, `Shared/Source` w include, `DoomsdayUI.cpp` dołączony).
- Workflow: skopiuj `Shared/templates/build-workflow.yml` do `.github/workflows/`, podmień nazwy, dopisz folder pluginu do `paths-ignore` w `build-windows.yml` (PoS).
- Lokalnie (Linux): symlink `JUCE` w folderze pluginu, `CXXFLAGS="-DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0 -DJUCE_USE_XRENDER=0" cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DPROPHECY_BUILD_TESTS=ON`, test pod `xvfb-run`.

## Source/DoomsdayDSP.h  (`doomsday::dsp`)
| Klocek | Do czego |
|---|---|
| `polyBlep, wrap01, fastSin01, fastTanh, timeCoef(ms, rate), softClip(x, knee), onePoleCoef(hz, rate)` | pomocnicze |
| `Env` + `Env::rates(a, d, s, r, rate)` | ADSR: `trigger(fromZero)`, `release()`, `next(rates)`, `idle()` |
| `SvfCoefs svfCoefs(hz, k, rate)`, `Svf` (`lp/bp/hp/peak`), `MultiFilter::process(x, type 0 LP12 /1 LP24 /2 BAND /3 HIGH, coefs)` | filtry TPT |
| `UnisonBank` + `UnisonBank::Table::setup(voices≤16, wave, cents, stereo, pan, gain)`; `render(table, hz, 1/rate, l, r)`; `resetPhases(rng, start<0 = losowo)` | oscylator unison; fale 0 SINE 1 TRI 2 SAW 3 SQUARE 4 PULSE 5 HYBRID |
| `FxChain` + `FxParams` | CHORUS → DELAY (sync, ping-pong) → REVERB → PUMP (sync z hostem): `prepare`, `setup(params, bpm)`, `process(L, R, n, ppq, offset)` |
| `LevelFeed<N>` | min/max co ~4 ms do wizualizacji (audio → GUI) |
| `shape(x, 0 SOFT /1 HARD /2 TUBE /3 SCREAM)`, `shapeMakeup(drive, type)`, `DcBlocker` | przestery z Fortune of Seekers (po TUBE daj DcBlocker) |
| `Screamer` (`set(0..1)`, `process(x)`) | SCREAM na oscylator z Fortune of Seekers (drive + zawijanie) |
| `NoteStack` | mono / legato: stos trzymanych nut (ostatnia wygrywa) |
| `VoiceManager<Voice, N>` | polifonia z Fortune of Seekers: `noteOn(note, limit, fadeStep, sameNote)` (kradzież najstarszego z wyciszeniem), `noteOff`, `setPedal`, `allOff`. Voice musi mieć `active, held, sustained, note, age, kill, killStep` |

## Source/DoomsdayParams.h  (`doomsday`)
- `fmt::` formatery tekstu (hz, ms, db, pct, bip, oct, semi, cents, lfoHz, pan) + parsery (`num, hzIn, msIn, pctIn, panIn`).
- `make::f / b / c / i / skew` – fabryki parametrów APVTS z formatami.
- `PresetIO { product, extension }` – `folder()`, `list()`, `apply(proc, apvts, values)`, `save(...)`, `load(...)`, `getState/setState` (uzupełnia parametry nieznane starym projektom), `hostTime(playhead, bpm, ppq)`.

## Source/DoomsdayUI.h / .cpp  (`doomsday::ui`)
- `Theme` (kolory, `accents`, `crown` dla dużego pokrętła) + `StageLookAndFeel(theme)` – fonty Michroma / Barlow.
- `Knob(apvts, id, label, accent, bipolar, big)`, `Switch(apvts, id, text, Power|Chip, accent)`, `Choice(apvts, id, labels, accent, vertical)`, `Meter`.
- `PresetBar(PresetHost&, withPlay)` – < preset > / RANDOMIZE / SAVE / PLAY; procesor implementuje `PresetHost`.
- `paint::glassPanel(g, lnf, rect, title, accent, spot)`, `paint::beam(...)`, `scaleCanvas(editor, canvas, W, H)`.
- `FxTabs(apvts, accent)` + `addTab(name, powerParamId, cells, side)` – zakładki efektów z ACTIVE (jak w Fortune of Seekers), `refresh()` z timera.
- `FlowLane(levelFeed, accent, label)` – dźwięk płynący z góry na dół (THE PATH z Fortune of Seekers), `tick()` z timera.

Pluginy używające Shared: **Dark Serpent**. Klocki z Fortune of Seekers (przestery, SCREAM, polifonia, zakładki FX, THE PATH) są tu skopiowane i sprawdzone testem Dark Serpent. Starsze (PoS, Ancient Drums, Fortune of Seekers) mają własny kod i nie są przerabiane.
