# Shared – wspólne klocki pluginów DOOMSDAY

**Przed budową czegokolwiek przeczytaj też `LESSONS.md`** (co brzmiało źle i dlaczego).

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
| `UnisonBank` + `UnisonBank::Table::setup(voices≤16, wave, cents, stereo, pan, gain, sides = 1)` (sides = głośność rozstrojonych głosów wobec środkowego, MIX supersawa); `render(table, hz, 1/rate, l, r)`; `resetPhases(rng, start<0 = losowo)` | oscylator unison; fale 0 SINE 1 TRI 2 SAW 3 SQUARE 4 PULSE 5 HYBRID |
| `FxChain` + `FxParams` | CHORUS → DELAY (sync, ping-pong) → REVERB → PUMP (sync z hostem): `prepare`, `setup(params, bpm)`, `process(L, R, n, ppq, offset)` |
| `LevelFeed<N>` | min/max co ~4 ms do wizualizacji (audio → GUI) |
| `shape(x, 0 SOFT /1 HARD /2 TUBE /3 SCREAM)`, `shapeMakeup(drive, type)`, `DcBlocker` | przestery z Fortune of Seekers (po TUBE daj DcBlocker) |
| `Screamer` (`set(0..1)`, `process(x)`) | SCREAM na oscylator z Fortune of Seekers (drive + zawijanie) |
| `NoteStack` | mono / legato: stos trzymanych nut (ostatnia wygrywa) |
| `BiquadCoefs::make(Bell / LowShelf / HighShelf / Notch / BandPass / HighPass / LowPass, fs, hz, q, dB)`, `.magnitude(hz, fs)`, `Biquad`, `butterworthQs(stages)` (dowolny rząd) | filtry EQ (z Mirror of Souls) – ta sama funkcja liczy dźwięk i krzywą na ekranie |
| `AnalyzerTap<N>` (`push`, `latest`) + `Spectrum` (`feed`, `dbAt(hz)`) | analizator widma: audio wrzuca próbki, GUI liczy FFT 4096 z wygładzaniem |
| `XorNoise`, `PitchSweep` (`setTime`, `start`, `next (semis)`) | szum do „brudu” pod screechem; zjazd / podjazd wysokości po każdej nowej nucie (z Prophecy of Scream v3) |
| `Formant` (`process (l, r, hz, q, gainLin, amount, rate)`) | rezonansowy „głos” screecha – szczyt, który rusza obwiednia / LFO (PoS v3) |
| `Ott3` (`prepare`, `process (l, r, amount)`) | 3-pasmowy OTT: ciche pasma w górę, głośne w dół – gęsty hardstyle'owy dźwięk (PoS v3) |
| `StereoSpread` (`prepare`, `process (l, r, amount)`) | szerokie stereo: więcej boków + zdekorelowany środek powyżej ~220 Hz, mono-kompatybilne (PoS v3) |
| `DoomsdaySpace.h`: `space::EchoDelay` (STEREO / PING-PONG / WIDE, filtry w pętli, drive, wobble), `space::FdnReverb` (8 linii, pre-delay, size, decay RT60, damp, low cut, motion, SHIMMER, FREEZE, width), `space::Ducker`, `space::Shifter` (oktawa w górę), `space::MonoBass` (24 dB) | przestrzeń: delay + reverb + ducking (z Abyss of Echoes) |
| `DoomsdayDynamics.h`: `dyn::Crossover3` (LR4, sumuje się płasko), `dyn::BandComp` (ratio w dół + UPWARD, knee, curve() do rysowania), `dyn::saturate` (1:1 dla cichych), `dyn::TruePeak` (8x), `dyn::Limiter` (`prepare (sr, lookaheadMs, holdMs)`, true peak, latencySamples()), `dyn::Loudness` (LUFS M / S / I wg BS.1770), `dyn::Tilt` | dynamika i mastering (z Throne of Titans) |
| `DoomsdayKick.h` (v2): `kick::KickParams` (+ `cook(rate)` – współczynniki raz na blok), `kick::KickVoice` (TAIL: opadanie pitchu START / DROP / CURVE / HOLD albo rysowany PITCH, sine→tri→saw; CHAOS – szum w przester; ROOM przed / po przesterze; STAGE 1 (5 typów) → MID EQ → STAGE 2; LOW / HIGH CUT, BODY, SCREAM, CRUNCH, GRIT; PUNCH z DRIVE; CLICK), `kick::KickRoom` (mały pogłos mono), `kick::Shape` (rysowane obwiednie w promilach długości, max 24 kropki), `kick::masterClip` | synteza kicków (z Forge of Wrath) |
| `VoiceManager<Voice, N>` | polifonia z Fortune of Seekers: `noteOn(note, limit, fadeStep, sameNote)` (kradzież najstarszego z wyciszeniem), `noteOff`, `setPedal`, `allOff`. Voice musi mieć `active, held, sustained, note, age, kill, killStep` |

## Source/DoomsdayDrive.h  (`doomsday::drive`) – czyste przestery
| Klocek | Do czego |
|---|---|
| `Adaa1::process (x, curve)` + `plain (x, curve)`; krzywe `Soft, Hard, Tube, Fold, Cubic` | przester z anty-aliasingiem (ADAA): o ~7 dB mniej „cyfrowego piasku” niż zwykły, z 2x oversamplingiem o ~20 dB mniej; ciche sygnały przechodzą 1:1 |
| `OverSampler` (`prepare (ch, maxBlock)`, `setFactorIndex (0..3 = 1x..8x)`, `process (block, [] (ch, x) { ... })`, `latencySamples()`) | oversampling – wszystkie krotności przygotowane z góry, przełączanie bez alokacji |
| `DcBlock`, `makeup (curve, driveLin)` | po asymetrycznych krzywych / wyrównanie głośności przy podkręcaniu drive |
| `Bitcrusher` (`set (rateHz, bits, mix)`) | redukcja próbkowania + bitów („artefakt” na ogonie raw kicka) |
| `DrawnShaper` (`setShape (mod::Shape, symmetric)`, `identity()`) | waveshaper rysowany krzywą (jak Trash 2), tablica 1024 |
| `EnvFollower`, `DynamicDrive` (`process (x, amount -1..1)` → mnożnik drive) | przester zależny od transjentów (Saturn DYNAMICS): punch czysty, ogon brudny |
| `TransientShaper` (`process (x, attackDb, sustainDb)`) | podbicie / ścięcie uderzenia i wybrzmienia, niezależne od poziomu |
| `AutoGain` (`process (dry, wet)`) | wyjście tak głośne jak wejście – słychać charakter, nie „głośniej” |

## Source/DoomsdayFilter.h  (`doomsday::filter`) – filtr analogowy
| Klocek | Do czego |
|---|---|
| `Ladder` (`prepare`, `setCutoff` / `setG (prewarp (hz, rate))`, `setResonance (0..1.05)`, `setDrive (dB)`, `setMode (LP24, LP12, BP12, HP12, HP24)`, `setBassComp`, `process (x)`) | 4-stopniowy ladder (jak w Sylenth / Diva): nasycenie w pętli, ciche czyste, głośne ciepłe, rezonans do samooscylacji, stabilny przy szybkiej modulacji; jeden kanał na instancję |

## Source/DoomsdayMod.h  (`doomsday::mod`) – modulacja
| Klocek | Do czego |
|---|---|
| `Shape` (do 32 kropek `{czas, wartość 0..1, bend}`, `eval / at`, `insert / remove`, `toString / fromString`) | rysowane obwiednie i krzywe; `bend` wygina odcinek za kropką; `kick::Shape` to ten sam typ |
| `Lfo` (`SINE, TRIANGLE, SAW UP, SAW DOWN, SQUARE, S&H, SMOOTH, DRAWN`), `setRateHz`, `setSynced (beats, bpm)`, `syncTo (ppq)`, `retrigger`, `setPhaseOffset`, `setShape` | LFO wolne lub zsynchronizowane z FL; DRAWN = narysowany kształt jako jeden cykl (sidechain / volume shaper) |
| `syncDivisions() / syncBeats (i) / syncNames()` | wartości nut 1/64 .. 16 taktów (zwykłe, z kropką, triole) do parametru Choice |
| `Smoother`, `Matrix<źródła, cele, sloty>` (`setSlot (i, src, dst, amount, via)`, `setSource`, `compute`, `dest`) | wygładzanie parametrów; macierz modulacji |
| `Lfo` tryb `CHAOS` | atraktor Lorenza: płynny, nigdy się nie powtarza („żywe” screeche) |
| `Performer` (`setStep (seq, i, curve, height)`, `setLength`, `setMorph`, `at (ppq, stepBeats)`) | sekwencer 16 kroków z krzywą na krok, morfing sekwencji A / B (Massive) |
| `RandomPerNote` (`trigger()` na nutę, `next()` z glide) | losowa wartość na każde uderzenie (Massive Random Track) |
| `KeyScale` (`root`, `scale`, `contains`, `quantize`) | tonacja i skala (dur, moll, harmoniczna, frygijska, dorycka, pentatonika) |
| `Arp` (`noteOn / noteOff`, `setMode`, `setOctaves`, `setRateBeats`, `setStep`, `process (ppq, bpm, sr, n, emit)`) | arpeggiator ze step sequencerem (gate / velocity / transpozycja / on na krok) |

## Source/DoomsdayOsc.h  (`doomsday::osc`) – oscylatory
| Klocek | Do czego |
|---|---|
| `IntensityOsc` (`setMode (SPECTRUM / BEND / FORMANT)`, `setIntensity`, `setShape`, `next (hz)`) | jedna gałka, trzy charaktery (Massive / Spire): harmoniczne, phase distortion, formant (samogłoska) stojący przy zmianie wysokości |
| `ModOsc` (`next (hz, ratio)`, `ring`, `phaseOffset`, `cutoffMul`) | oscylator modulujący: RING, PM / FM, FILTER FM (growl) |
| `FmOps<N>` (`setOp (i, ratio, level, feedback, heard)`, `setMod (from, to, amount)`, `next (hz, levelMul)`) | operatory FM z macierzą (Sytrus) – screeche FM |
| `ChordStack::semitones / ratio (chord, voice, n, amount)` | unison ↔ akord (UNISON, OCTAVES, FIFTH, POWER, MINOR, MAJOR, SUS4, MINOR 7) |
| `Slop` (`forVoice (v, amount)`, `advance (n)`) | analogowy rozrzut między głosami: strój, cutoff, czasy obwiedni + powolny dryf (Diva) |

## Source/DoomsdayShaper.h  (`doomsday::shaper`) – sidechain i efekty czasowe
| Klocek | Do czego |
|---|---|
| `OnsetDetector` (`setFocusHz`, `setThresholdDb`, `setMinGapMs`, `setSensitivity`, `process (x)` → true na uderzenie) | AUDIO TRIGGER od kicka (sidechain bez MIDI) |
| `presetCurve (0..11)` + `presetNames()` | 10 krzywych duckingu + 2 ucinania (Kickstart) |
| `VolumeShaper` (`setCurve`, `setDepth (low, mid, high)`, `setCrossovers`, `setSynced (on, beats)`, `trigger`, `process (L, R, n, ppq, bpm)`) | rysowany sidechain / volume shaper, z tempem albo od triggera, osobna głębokość na pasmo (np. tylko bas) |
| `TapeStop` (`engage (ms, curve)`, `release`), `Stutter` (`engage (samples)`, `release`), `Reverser` (`setChunk`) | efekty build-upów (Gross Beat): zatrzymanie taśmy, powtarzanie kawałka, granie wstecz |

## Source/DoomsdaySpectral.h  (`doomsday::spectral`) – widmo
| Klocek | Do czego |
|---|---|
| `ResonanceSuppressor` (`setDepth`, `setSharpness`, `setSelectivity`, `setTimes`, `setFocus`, `setMix`, `setDelta`, `cutDb()`, `latencySamples()` = 2047) | tłumik rezonansów jak soothe2: wycina tylko to, co wystaje (piszczące screeche, hihaty), DELTA = słyszysz co zabiera |

Testy klocków bez wtyczki: `Shared/Tests` (`SharedTests`: aliasing, poziom, latencja, Shape, LFO, sync, macierz, ladder, unison SIDES, bitcrusher, drawn shaper, transient shaper, dynamic drive, auto-gain, chaos LFO, performer, skala, arp, oscylatory INTENSITY / FM / akordy / slop, onset, volume shaper, tape stop, stutter, reverser, resonance suppressor).

## Source/DoomsdayParams.h  (`doomsday`)
- `fmt::` formatery tekstu (hz, ms, db, pct, bip, oct, semi, cents, lfoHz, pan) + parsery (`num, hzIn, msIn, pctIn, panIn`).
- `make::f / b / c / i / skew` – fabryki parametrów APVTS z formatami.
- `PresetIO { product, extension }` – `folder()`, `list()`, `apply(proc, apvts, values)`, `save(...)`, `load(...)`, `getState/setState` (uzupełnia parametry nieznane starym projektom), `hostTime(playhead, bpm, ppq)`.

## Source/DoomsdayUI.h / .cpp  (`doomsday::ui`)
- `Theme` (kolory, `accents`, `crown` dla dużego pokrętła) + `StageLookAndFeel(theme)` – fonty Michroma / Barlow.
- `Knob(apvts, id, label, accent, bipolar, big)`, `Switch(apvts, id, text, Power|Chip, accent)`, `Choice(apvts, id, labels, accent, vertical)`, `Meter`.
- `PresetBar(PresetHost&, withPlay, withRandom = true)` (v2) – < nazwa > / RANDOMIZE / SAVE / PLAY / UNDO / REDO; klik w nazwę = przeglądarka
  (szukanie, kategorie z prefiksu „KAT - Nazwa”, ulubione *, presety użytkownika), kółko myszy nad nazwą = następny / poprzedni,
  Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y. Procesor implementuje `PresetHost`; undo działa, gdy `undoManager()` zwraca `UndoManager`
  podany do APVTS (`apvts (*this, &undo, ...)`, `undo` zadeklarowany przed `apvts`). Ulubione: `AppData/Doomsday/<folder presetów> favourites.txt`.
- `paint::glassPanel(g, lnf, rect, title, accent, spot)`, `paint::beam(...)`, `scaleCanvas(editor, canvas, W, H)`.
- `FxTabs(apvts, accent)` + `addTab(name, powerParamId, cells, side)` – zakładki efektów z ACTIVE (jak w Fortune of Seekers), `refresh()` z timera.
- `FlowLane(levelFeed, accent, label)` – dźwięk płynący z góry na dół (THE PATH z Fortune of Seekers), `tick()` z timera.
- `GainScope(accent, rangeDb)` + `push (peak, grDb)` z timera – przewijana fala z linią redukcji (jak Pro-L 2 / Pro-C 2), MAX z pamięcią.
- `Knob::setModulation (depth, nowPos01)` – pierścień modulacji wokół gałki (zakres + kropka „teraz”), jak w Serum / Vital.

## Source/DoomsdayFlat.h / .cpp  (`doomsday::flat`) – wygląd Minimal / flat (wybrany przez właściciela, `Shared/DESIGN.md`)
- `tokens()` – kolory (tło, powierzchnia, kontrolki, tekst, jeden gorący `hot` + jeden zimny `cool` akcent).
- `FlatLookAndFeel` – font Inter (Medium / SemiBold / Bold), cienkie łuki pokręteł, płaskie przyciski / listy / menu / okna / dymki.
  `FlatLookAndFeel::makePrimary (button)` = przycisk wypełniony akcentem (np. SAVE).
- `Knob(apvts, id, label, accentColour, bipolar, diameter = 44)` – łuk + wartość (klik = wpisz) + etykieta; `setModulation (depth, now)` = pierścień modulacji;
  `Knob::heightFor (d)` = potrzebna wysokość.
- `Toggle(apvts, id, text, Pill|Chip, accent)`, `Segmented(apvts, id, labels, accent)` (poziomy), `TabStrip(names, accent)` (zakładki z kropką ON),
  `card(g, lnf, rect, title, dotColour)` – płaski panel z tytułem.
- Wpięcie: `doomsday_plugin(... FLAT)` dodaje fonty Inter (`Shared/Resources/Fonts/Inter-*.ttf`, OFL) i `DoomsdayFlat.cpp`.
  Prophecy of Scream (v3.1) ma własny CMake – dodane ręcznie. Pierwszy plugin w tym stylu: PoS (duży wykres z punktami do przeciągania – `ResponseGraph` w `Source/PluginEditor.cpp`, do skopiowania np. do Mirror of Souls).

Pluginy używające Shared: **Dark Serpent**, **Mirror of Souls**, **Abyss of Echoes**, **Throne of Titans**, **Forge of Wrath**, **Prophecy of Scream** (od v3: NOISE / FORMANT / SWEEP / OTT / WIDTH, przez `../Shared/Source/DoomsdayDSP.h`). Klocki z Fortune of Seekers (przestery, SCREAM, polifonia, zakładki FX, THE PATH) są tu skopiowane i sprawdzone testem Dark Serpent. Starsze (Ancient Drums, Fortune of Seekers, stare części PoS) mają własny kod i nie są przerabiane.

## Znak firmowy (sonicsystems / DOOMSDAY)
`Source/DoomsdayBrand.h` – samodzielny nagłówek z wbudowanym logo (`Resources/sonicsystems_doomsday.png`).
Standard: `doomsday::brand::drawHeaderBadge (g, domyślnaSzerokośćOkna / (float) W, krycie [, kolorBieli])` – zawsze lewy
górny róg nagłówka, zawsze 70 x 36 px przy domyślnym rozmiarze okna (niezależnie od wielkości canvasu wtyczki);
linie wersji / opisu zaczynają się od `headerTextX (ta sama skala)`. Rysować raz, w cache'owanym tle. Na jasnych wtyczkach (Ancient Drums) biel jest przemalowana na ciemny kolor.
Używają go wszystkie wtyczki Doomsday (stare włączają go ścieżką względną `../../Shared/Source/DoomsdayBrand.h`).
Przy zmianie logo: podmień PNG i wygeneruj nagłówek od nowa; Prophecy of Scream przebudowuje się sam (od v3 jego workflow obserwuje też Shared/).
