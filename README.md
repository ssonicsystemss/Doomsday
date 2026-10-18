# DOOMSDAY – pluginy hardstyle

| Plugin | Folder | Build (zakładka Actions) |
|---|---|---|
| **Prophecy of Scream** – syntezator screechy | katalog główny | *Build Prophecy of Scream VST3* → `Prophecy-of-Scream-VST3-Windows` |
| **Ancient Drums** – generator kicków | `AncientDrums/` | *Build Ancient Drums VST3* → `Ancient-Drums-VST3-Windows` |

Każdy plugin buduje się osobno – zmiana w jednym nie odpala builda drugiego. Opis Ancient Drums: `AncientDrums/README.md`.

---

# PROPHECY OF SCREAM v2
Syntezator screechy do hardstyle / rawstyle / uptempo / frenchcore. VST3 (instrument), działa w FL Studio.

## Jak zdobyć plik .vst3 (Windows)
Build robi się sam na GitHubie: zakładka **Actions** → ostatni build → **Artifacts** → `Prophecy-of-Scream-VST3-Windows`.
Folder `Prophecy of Scream.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.

Projekty zrobione na v1.0 otwierają się z tym samym brzmieniem (OSC 2 wyłączony, wszystkie efekty włączone).

## Tor sygnału
```
OSC 1 (unison + FM) -> DIST -> FILTER  --+
                                         +-> AMP -> DELAY -> REVERB -> OUTPUT
OSC 2 (unison + FM) -> DIST -> FILTER  --+
```
Każdy oscylator ma własne obwiednie pitchu i filtra oraz własne LFO. Wszystko przed DELAY liczone jest z 4× oversamplingiem.

## Panele
| Panel | Co tam jest |
|---|---|
| OSC 1 / OSC 2 | włącznik, WAVE (piła → kwadrat), UNISON, DETUNE, WIDTH, FM, RATIO, OCTAVE, SEMI, FINE, P.ENV, P.DECAY, LEVEL |
| Zakładki pod oscylatorem | **DIST** (HARD / TUBE / FOLD / SCREAM, DRIVE, BITE, MIX), **FILTER** (LP12 / LP24 / BAND / HIGH, CUTOFF, RESO, ENV, DECAY, KEY), **LFO** (kształt, SYNC, RATE, > CUTOFF, > PITCH). Każda zakładka ma włącznik **ACTIVE**, a dioda przy nazwie pokazuje, czy efekt gra |
| THE SCREAM | makro: dokłada FM, drive i rezonans obu oscylatorom naraz. Mod wheel też go podkręca |
| VOICE | GLIDE, BEND (zakres pitch-wheela), LEGATO |
| AMP | ATTACK, DECAY, SUSTAIN, RELEASE (wspólne dla obu oscylatorów) |
| DELAY | włącznik, TIME (zsynchronizowany z tempem), FEEDBACK, MIX, P.PONG |
| REVERB | włącznik, SIZE, DAMP, MIX |
| OUTPUT | VOLUME + miernik |

Dwuklik na pokrętle = wartość domyślna. Okno można skalować za róg.

## Presety
Fabryczne: **Dutchie** (domyślny), Zaagtand '04, Oldskool Gekte '05, Twin Prophecy, Blippy Blippy, Brain Fryer, Cardio, Devil's Scream, Farting Grandpa, Horn of Fallen, Mad Machine, Sexy Nightmare, Sonar, Zephyr.

Presety z `Resources/Presets` są wbudowane w plugin – żeby dodać nowy fabryczny preset, wrzuć tam plik `.pos`, dopisz go w `CMakeLists.txt` i w `embeddedPresets` w `PluginProcessor.cpp`.

- **< >** – poprzedni / następny preset.
- **SAVE** – zapis do `Dokumenty\Prophecy of Scream\Presets\*.pos`. Twoje presety są na liście w sekcji USER.
- **RANDOMIZE** – losuje screecha w rozsądnych zakresach, czasem dokłada OSC 2 (głośność zostaje).

## Fonty
Interfejs używa fontów Michroma i Barlow Semi Condensed (SIL Open Font License, pliki licencji w `Resources/Fonts`).
