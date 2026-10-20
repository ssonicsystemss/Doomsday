# DOOMSDAY – pluginy hardstyle

| Plugin | Folder | Build (zakładka Actions) |
|---|---|---|
| **Prophecy of Scream** – syntezator screechy | katalog główny | *Build Prophecy of Scream VST3* → `Prophecy-of-Scream-VST3-Windows` |
| **Ancient Drums** – generator kicków | `AncientDrums/` | *Build Ancient Drums VST3* → `Ancient-Drums-VST3-Windows` |
| **Fortune of Seekers** – syntezator melodii | `FortuneOfSeekers/` | *Build Fortune of Seekers VST3* → `Fortune-of-Seekers-VST3-Windows` |
| **Dark Serpent** – screamer / lead do dropów | `DarkSerpent/` | *Build Dark Serpent VST3* → `Dark-Serpent-VST3-Windows` |
| **Mirror of Souls** – equalizer (efekt) | `MirrorOfSouls/` | *Build Mirror of Souls VST3* → `Mirror-of-Souls-VST3-Windows` |
| **Abyss of Echoes** – reverb / delay / stereo (efekt) | `AbyssOfEchoes/` | *Build Abyss of Echoes VST3* → `Abyss-of-Echoes-VST3-Windows` |
| **Throne of Titans** – mastering / limiter / LUFS (efekt) | `ThroneOfTitans/` | *Build Throne of Titans VST3* → `Throne-of-Titans-VST3-Windows` |

Każdy plugin buduje się osobno – zmiana w jednym nie odpala builda drugiego. Opisy: `AncientDrums/README.md`, `FortuneOfSeekers/README.md`, `DarkSerpent/README.md`, `MirrorOfSouls/README.md`, `AbyssOfEchoes/README.md`, `ThroneOfTitans/README.md`. Wspólne klocki dla nowych pluginów: `Shared/README.md`.

---

# PROPHECY OF SCREAM v3
Syntezator screechy do hardstyle / rawstyle / uptempo / frenchcore. VST3 (instrument), działa w FL Studio.

**Nowe w v3** (strojone pod prawdziwe screeche: gęsty „brud”, ruchomy szczyt, szerokie stereo, zjazdy):
- **NOISE** (zakładka DIST) – szum przed przesterem: daje gęsty, szumiący dywan pod screechem.
- **FORMANT** (nowa zakładka) – rezonansowy szczyt, czyli „głos” screecha: FREQ, GAIN, Q, ENV + DECAY (szczyt zjeżdża / podjeżdża
  po każdej nucie), KEY (100% + FREQ 262 Hz = szczyt idzie za grana nutą), LFO (gadanie / wah).
- **> AMP** (zakładka LFO) – LFO bramkuje dźwięk: kwadrat = rytmiczne cięcie / stutter.
- **SWEEP + S.TIME** (VOICE) – po każdej nowej nucie wysokość zjeżdża lub podjeżdża o zadane półtony w zadanym czasie (do 10 s – FX-y).
- **OTT** i **WIDTH** (OUTPUT) – zagęszczenie jak OTT i szerokie stereo (bas zostaje w środku).
- Nowe presety strojone pod referencje (sekcja Presety). Stare presety i projekty brzmią tak samo – nowe rzeczy domyślnie są wyłączone.

## Jak zdobyć plik .vst3 (Windows)
Build robi się sam na GitHubie: zakładka **Actions** → ostatni build → **Artifacts** → `Prophecy-of-Scream-VST3-Windows`.
Folder `Prophecy of Scream.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.

Projekty zrobione na v1.0 otwierają się z tym samym brzmieniem (OSC 2 wyłączony, wszystkie efekty włączone).

## Tor sygnału
```
OSC 1 (unison + FM + NOISE) -> DIST -> FILTER -> FORMANT --+
                                                           +-> AMP -> OTT -> WIDTH -> DELAY -> REVERB -> OUTPUT
OSC 2 (unison + FM + NOISE) -> DIST -> FILTER -> FORMANT --+
```
Każdy oscylator ma własne obwiednie pitchu i filtra oraz własne LFO. Wszystko przed DELAY liczone jest z 4× oversamplingiem.

## Panele
| Panel | Co tam jest |
|---|---|
| OSC 1 / OSC 2 | włącznik, WAVE (piła → kwadrat), UNISON, DETUNE, WIDTH, FM, RATIO, OCTAVE, SEMI, FINE, P.ENV, P.DECAY, LEVEL |
| Zakładki pod oscylatorem | **DIST** (HARD / TUBE / FOLD / SCREAM, DRIVE, BITE, NOISE, MIX), **FILTER** (LP12 / LP24 / BAND / HIGH, CUTOFF, RESO, ENV, DECAY, KEY), **LFO** (kształt, SYNC, RATE, > CUTOFF, > PITCH, > AMP), **FORMANT** (FREQ, GAIN, Q, ENV, DECAY, KEY, LFO). Każda zakładka ma włącznik **ACTIVE**, a dioda przy nazwie pokazuje, czy efekt gra |
| THE SCREAM | makro: dokłada FM, drive i rezonans obu oscylatorom naraz. Mod wheel też go podkręca. Pod nim podgląd fali |
| VOICE | GLIDE, BEND (zakres pitch-wheela), LEGATO, SWEEP, S.TIME |
| AMP | ATTACK, DECAY, SUSTAIN, RELEASE (wspólne dla obu oscylatorów) |
| DELAY | włącznik, TIME (zsynchronizowany z tempem), FEEDBACK, MIX, P.PONG |
| REVERB | włącznik, SIZE, DAMP, MIX |
| OUTPUT | OTT, WIDTH, VOLUME + miernik |

Dwuklik na pokrętle = wartość domyślna. Okno można skalować za róg.

## Presety
Fabryczne: **Dutchie** (domyślny), Zaagtand '04, Oldskool Gekte '05, Twin Prophecy, Blippy Blippy, Brain Fryer, Cardio, Devil's Scream, Farting Grandpa, Horn of Fallen, Mad Machine, Sexy Nightmare, Sonar, Zephyr.

Nowe w v3 (strojone pomiarami pod referencyjne screeche, grane najlepiej w okolicy C5–C6 w FL):
**Wail of Souls** (szeroki wyjący screech z podciągnięciem), **Howling Prophet** (zjazd na starcie + powolne opadanie),
**Ascension** (jasny, powoli podjeżdża), **Talking Dead** (gadający formant), **Diving Blade** (krótki strzał ze zjeżdżającym szczytem),
**Comb of Doom** (gęsty grzebień FM ze zjazdem), **Rising Scream**, **Wah of the Damned** (rytmiczne wah), **Stutter Ritual**
(bramkowany podjazd o oktawę), **Raw Burst** (krótki, brudny), **Tritone Fall FX** (7-sekundowy zjazd z szumem – przytrzymaj nutę).

Presety z `Resources/Presets` są wbudowane w plugin – żeby dodać nowy fabryczny preset, wrzuć tam plik `.pos`, dopisz go w `CMakeLists.txt` i w `embeddedPresets` w `PluginProcessor.cpp`.

- **< >** – poprzedni / następny preset.
- **SAVE** – zapis do `Dokumenty\Prophecy of Scream\Presets\*.pos`. Twoje presety są na liście w sekcji USER.
- **RANDOMIZE** – losuje screecha w rozsądnych zakresach, czasem dokłada OSC 2 (głośność zostaje).

## Fonty
Interfejs używa fontów Michroma i Barlow Semi Condensed (SIL Open Font License, pliki licencji w `Resources/Fonts`).
