# ANCIENT DRUMS v2.2
Generator kicków do hardstyle / rawstyle / uptempo / frenchcore. VST3 (instrument), działa w FL Studio.

## Instalacja (Windows)
Build robi się sam na GitHubie: zakładka **Actions** → ostatni build → **Artifacts** → `Ancient-Drums-VST3-Windows`.
Folder `Ancient Drums.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.

Każda nuta MIDI odpala kicka. **ROOT** (domyślnie C5) gra bez transpozycji.

## Tor sygnału
```
PUNCH / CRUNCH / KICK  (offset, obwiednia, reverse, pitch, filtry)
   warstwy z FORGE  -> FORGE --+
   reszta (np. punch) --------+--> PRE-GAIN -> CLIP -> LIMIT -> VOLUME

FORGE: FOCUS -> DRIVE (przester, 4x OS, bez „piasku” w górze) -> GRIT (krótki pogłos wpuszczony w przester)
       -> CRUNCH (podbicie ~2.4 kHz + miękko-twardy clip, 4x OS) -> TONE (ciemniej / jaśniej)
       -> OTT (3 pasma) -> SMOOTH (zdejmuje ostrość) -> MIX (równolegle z czystym sygnałem)
```
FORGE zmienia barwę, a nie głośność – wyjście jest automatycznie wyrównywane do poziomu wejścia, więc master clipper nie dostaje po uszach.

## Ekran (środek)
- **PUNCH / CRUNCH / KICK** – wybór warstwy. Wybrana świeci, pozostałe są przygaszone w tle.
- **Przeciągnij waveform** w lewo/prawo – przesunięcie w czasie (Shift = precyzyjnie), zakres ±2 s, nic nie blokuje.
  Ujemne przesunięcie przycina początek sampla. Gdy sampel ucieknie poza ekran, pojawia się strzałka z jego pozycją,
  a **RESET SAMPLE POSITION** (świeci na żółto) wraca do 0 ms.
- **Biała linia z punktami** – głośność warstwy w czasie, jak automation clip:
  - pierwszy punkt (romb) stoi na sztywno na początku warstwy – rusza się tylko góra/dół,
  - pozostałe punkty chodzą tylko między sąsiadami i nie wyjdą poza ekran, sąsiedzi się nie przestawiają,
  - **dwuklik** dodaje punkt (trzymaj i przesuwaj, żeby go od razu ustawić) / usuwa punkt, **prawy klik** na punkcie usuwa go.
- **0.5s / 1s / 2s** – zakres osi czasu.
- Upuść plik audio na ekran, żeby wczytać go do wybranej warstwy.

## Panele
| Panel | Co tam jest |
|---|---|
| Warstwa (lewo) | włącznik, sample z biblioteki / USER + LOAD, **REVERSE**, **FORGE** (czy warstwa idzie przez FORGE), KEY (punch), LEVEL, PITCH (od v2.2 zmienia tylko wysokość – długość sampla zostaje), OFFSET, LOW CUT, HIGH CUT, RESET ENV |
| MASTER (prawo) | ROOT, PRE-GAIN, CLIP, SOFT, LIMIT, VOLUME |
| FORGE (dół) | DRIVE, FOCUS, GRIT, CRUNCH, TONE, OTT, **SMOOTH**, **MIX** – w kolejności toru |

## Presety
Fabryczne: Ancient Raw, Temple Euphoric, Pharaoh Uptempo, Babylon Hardcore, Colossus Frenchcore, Monolith Rawstyle, Reverse Ritual, Clean Tail, Punch Only.
**SAVE** zapisuje do `Dokumenty\Ancient Drums\Presets\*.adp` razem z obwiedniami i ścieżkami Twoich sampli. **RANDOMIZE** losuje kicka w rozsądnych zakresach.

## Zmiany w v2.1
- usunięte SUB BASS i RUMBLE,
- FORGE przestrojony: mniej szumu i „piasku”, wyrównanie głośności, nowe SMOOTH i MIX,
- ekran: przypięty pierwszy punkt, punkty nie wychodzą poza ekran, przesuwanie sampla bez limitu + RESET SAMPLE POSITION.

Projekty z v2.0 otwierają się, ale FORGE brzmi łagodniej (inne strojenie), a ustawienia SUB/RUMBLE są pomijane.

## Fonty
Cinzel, Michroma, Barlow Semi Condensed, JetBrains Mono – SIL Open Font License (licencje w `Resources/Fonts`).
