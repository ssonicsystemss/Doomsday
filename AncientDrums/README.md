# ANCIENT DRUMS v2
Generator kicków do hardstyle / rawstyle / uptempo / frenchcore. VST3 (instrument), działa w FL Studio.

## Instalacja (Windows)
Build robi się sam na GitHubie: zakładka **Actions** → ostatni build → **Artifacts** → `Ancient-Drums-VST3-Windows`.
Folder `Ancient Drums.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.

Każda nuta MIDI odpala kicka. **ROOT** (domyślnie C5) gra bez transpozycji.

## Tor sygnału
```
PUNCH / CRUNCH / KICK  (offset, obwiednia, reverse, pitch, filtry)
   warstwy z FORGE  -> FORGE --+
   reszta (np. punch) --------+--> + SUB -> RUMBLE -> PRE-GAIN -> CLIP -> LIMIT -> VOLUME

FORGE: FOCUS (podbicie środka) -> DRIVE (przester, 4x OS) -> GRIT (pogłos wpuszczony w przester)
       -> CRUNCH (podbicie góry + twardy clip, 4x OS) -> TONE (ciemniej / jaśniej) -> OTT (3 pasma)
```

## Ekran (środek)
- **PUNCH / CRUNCH / KICK** – wybór warstwy. Wybrana świeci, pozostałe są przygaszone w tle.
- **Przeciągnij waveform** w lewo/prawo – przesunięcie w czasie (Shift = precyzyjnie). Ujemne przesunięcie przycina początek sampla.
- **Biała linia z punktami** – głośność warstwy w czasie, jak automation clip:
  przeciągnij punkt, **dwuklik** dodaje / usuwa punkt, **prawy klik** na punkcie usuwa go.
- **0.5s / 1s / 2s** – zakres osi czasu.
- Upuść plik audio na ekran, żeby wczytać go do wybranej warstwy.

## Panele
| Panel | Co tam jest |
|---|---|
| Warstwa (lewo) | włącznik, sample z biblioteki / USER + LOAD, **REVERSE**, **FORGE** (czy warstwa idzie przez FORGE), KEY (punch), LEVEL, PITCH, OFFSET, LOW CUT, HIGH CUT, RESET ENV |
| FORGE (prawo) | DRIVE, FOCUS, GRIT, CRUNCH, TONE, OTT |
| SUB BASS | SINE / SAW / SQR, OCTAVE, LEVEL, LENGTH, TONE, DRIVE |
| RUMBLE | LEVEL, DELAY, DECAY, DRIVE, LOW PASS, DUCK |
| MASTER | ROOT, PRE-GAIN, CLIP, SOFT, LIMIT, VOLUME |

## Presety
Fabryczne: Ancient Raw, Temple Euphoric, Pharaoh Uptempo, Babylon Hardcore, Colossus Frenchcore, Monolith Rawstyle, Reverse Ritual, Punch Only, Sub & Rumble.
**SAVE** zapisuje do `Dokumenty\Ancient Drums\Presets\*.adp` razem z obwiedniami i ścieżkami Twoich sampli. **RANDOMIZE** losuje kicka w rozsądnych zakresach.

## Fonty
Cinzel, Michroma, Barlow Semi Condensed, JetBrains Mono – SIL Open Font License (licencje w `Resources/Fonts`).
