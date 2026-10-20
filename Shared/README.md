# Shared – wspólne klocki pluginów DOOMSDAY

Spis dla kolejnych pluginów: przeczytaj ten plik, potem tylko potrzebne nagłówki.

## Jak użyć w nowym pluginie
- `CMakeLists.txt` pluginu: `include(../Shared/cmake/Doomsday.cmake)` + `doomsday_plugin(Target NAME "..." CODE Abcd CATEGORIES Instrument Synth SOURCES ... TEST Tests/RenderTest.cpp)`
  (JUCE 8.0.10, statyczny runtime MSVC, VST3 + Standalone, fonty w BinaryData, `Shared/Source` w include, `DoomsdayUI.cpp` dołączony).
  Efekt zamiast instrumentu: dopisz `EFFECT` (wejście + wyjście stereo, bez MIDI), np. `CATEGORIES Fx EQ`.
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
| `BiquadCoefs::make(Bell / LowShelf / HighShelf / Notch / BandPass / HighPass / LowPass, fs, hz, q, dB)`, `.magnitude(hz, fs)`, `Biquad`, `butterworthQs(stages)` | filtry EQ (z Mirror of Souls) – ta sama funkcja liczy dźwięk i krzywą na ekranie |
| `AnalyzerTap<N>` (`push`, `latest`) + `Spectrum` (`feed`, `dbAt(hz)`) | analizator widma: audio wrzuca próbki, GUI liczy FFT 4096 z wygładzaniem |
| `XorNoise`, `PitchSweep` (`setTime`, `start`, `next (semis)`) | szum do „brudu” pod screechem; zjazd / podjazd wysokości po każdej nowej nucie (z Prophecy of Scream v3) |
| `Formant` (`process (l, r, hz, q, gainLin, amount, rate)`) | rezonansowy „głos” screecha – szczyt, który rusza obwiednia / LFO (PoS v3) |
| `Ott3` (`prepare`, `process (l, r, amount)`) | 3-pasmowy OTT: ciche pasma w górę, głośne w dół – gęsty hardstyle'owy dźwięk (PoS v3) |
| `StereoSpread` (`prepare`, `process (l, r, amount)`) | szerokie stereo: więcej boków + zdekorelowany środek powyżej ~220 Hz, mono-kompatybilne (PoS v3) |
| `DoomsdaySpace.h`: `space::EchoDelay` (STEREO / PING-PONG / WIDE, filtry w pętli, drive, wobble), `space::FdnReverb` (8 linii, pre-delay, size, decay RT60, damp, low cut, motion, SHIMMER, FREEZE, width), `space::Ducker`, `space::Shifter` (oktawa w górę), `space::MonoBass` (24 dB) | przestrzeń: delay + reverb + ducking (z Abyss of Echoes) |
| `DoomsdayDynamics.h`: `dyn::Crossover3` (LR4, sumuje się płasko), `dyn::BandComp` (ratio w dół + UPWARD, knee, curve() do rysowania), `dyn::saturate`, `dyn::TruePeak` (8x), `dyn::Limiter` (lookahead, true peak, latencySamples()), `dyn::Loudness` (LUFS M / S / I wg BS.1770), `dyn::Tilt` | dynamika i mastering (z Throne of Titans) |
| `DoomsdayKick.h`: `kick::KickVoice` (TAIL z opadaniem pitchu sine→tri→saw, 5 typów przesteru, TONE, LOW / HIGH CUT, BODY, SCREAM, CRUNCH, GRIT, PUNCH, CLICK), `kick::Shape` (rysowane obwiednie AMP / DRIVE w promilach długości), `kick::masterClip` | synteza kicków (z Forge of Wrath) |
| `VoiceManager<Voice, N>` | polifonia z Fortune of Seekers: `noteOn(note, limit, fadeStep, sameNote)` (kradzież najstarszego z wyciszeniem), `noteOff`, `setPedal`, `allOff`. Voice musi mieć `active, held, sustained, note, age, kill, killStep` |

## Source/DoomsdayParams.h  (`doomsday`)
- `fmt::` formatery tekstu (hz, ms, db, pct, bip, oct, semi, cents, lfoHz, pan) + parsery (`num, hzIn, msIn, pctIn, panIn`).
- `make::f / b / c / i / skew` – fabryki parametrów APVTS z formatami.
- `PresetIO { product, extension }` – `folder()`, `list()`, `apply(proc, apvts, values)`, `save(...)`, `load(...)`, `getState/setState` (uzupełnia parametry nieznane starym projektom), `hostTime(playhead, bpm, ppq)`.

## Source/DoomsdayUI.h / .cpp  (`doomsday::ui`)
- `Theme` (kolory, `accents`, `crown` dla dużego pokrętła) + `StageLookAndFeel(theme)` – fonty Michroma / Barlow.
- `Knob(apvts, id, label, accent, bipolar, big)`, `Switch(apvts, id, text, Power|Chip, accent)`, `Choice(apvts, id, labels, accent, vertical)`, `Meter`.
- `PresetBar(PresetHost&, withPlay, withRandom = true)` – < preset > / RANDOMIZE / SAVE / PLAY; procesor implementuje `PresetHost`.
- `paint::glassPanel(g, lnf, rect, title, accent, spot)`, `paint::beam(...)`, `scaleCanvas(editor, canvas, W, H)`.
- `FxTabs(apvts, accent)` + `addTab(name, powerParamId, cells, side)` – zakładki efektów z ACTIVE (jak w Fortune of Seekers), `refresh()` z timera.
- `FlowLane(levelFeed, accent, label)` – dźwięk płynący z góry na dół (THE PATH z Fortune of Seekers), `tick()` z timera.

Pluginy używające Shared: **Dark Serpent**, **Mirror of Souls**, **Abyss of Echoes**, **Throne of Titans**, **Forge of Wrath**, **Prophecy of Scream** (od v3: NOISE / FORMANT / SWEEP / OTT / WIDTH, przez `../Shared/Source/DoomsdayDSP.h`). Klocki z Fortune of Seekers (przestery, SCREAM, polifonia, zakładki FX, THE PATH) są tu skopiowane i sprawdzone testem Dark Serpent. Starsze (Ancient Drums, Fortune of Seekers, stare części PoS) mają własny kod i nie są przerabiane.

## Znak firmowy (sonicsystems / DOOMSDAY)
`Source/DoomsdayBrand.h` – samodzielny nagłówek z wbudowanym logo (`Resources/sonicsystems_doomsday.png`).
Standard: `doomsday::brand::drawHeaderBadge (g, domyślnaSzerokośćOkna / (float) W, krycie [, kolorBieli])` – zawsze lewy
górny róg nagłówka, zawsze 70 x 36 px przy domyślnym rozmiarze okna (niezależnie od wielkości canvasu wtyczki);
linie wersji / opisu zaczynają się od `headerTextX (ta sama skala)`. Rysować raz, w cache'owanym tle. Na jasnych wtyczkach (Ancient Drums) biel jest przemalowana na ciemny kolor.
Używają go wszystkie wtyczki Doomsday (stare włączają go ścieżką względną `../../Shared/Source/DoomsdayBrand.h`).
Przy zmianie logo: podmień PNG i wygeneruj nagłówek od nowa; Prophecy of Scream przebudowuje się sam (od v3 jego workflow obserwuje też Shared/).
