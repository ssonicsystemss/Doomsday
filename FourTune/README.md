# 4TUNE v0.2 (wersja testowa)
Syntezator do hardstyle'owych melodii: plucki, leady, supersawy, melodie euphoric, festiwalowe leady. VST3 (instrument) do FL Studio.
Następca Fortune of Seekers (FoS zostaje jak był – stare projekty działają dalej).

## Instalacja (Windows)
Build robi się sam na GitHubie: zakładka **Actions** → *Build 4TUNE VST3* → **Artifacts** → `4TUNE-VST3-Windows`.
Folder `4TUNE.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.

## Tor sygnału (pasek na górze, kliknięcie otwiera stronę)
```
OSC 1 + OSC 2 -> GRUPA A (głośność / pan / szerokość) --+
                                                         +-> FILTR -> OBWIEDNIA AMP -> SCREAM -> EQ -> DIST -> CHORUS -> DELAY -> REVERB -> STEREO -> LIMITER
OSC 3 + OSC 4 -> GRUPA B (głośność / pan / szerokość) --+
```

## FORTUNE (duża złota gałka)
Jedna gałka, która przerabia dźwięk: **0 %** czysty, przyjemny pluck → **50 %** jaśniejszy, szerszy → **100 %** szeroki, jasny, agresywny festiwalowy lead.
Naraz: filtr w górę (do +2 oktaw), detune unisona (grupa B mocniej, grupa A zostaje skupiona), szerokość stereo, SCREAM
(przy SCREAM 0 dokłada tylko trochę, przy włączonym dużo), pogłos (mix do +15, rozmiar do +10 punktów).
**FORTUNE nigdy nie zastępuje Twoich gałek – dokłada do nich** (KOŃCOWA WARTOŚĆ = GAŁKA + FORTUNE). Pod gałką widać na żywo, ile dokłada.
Na stronie MOD → **FORTUNE TARGETS** ustawiasz, ile FORTUNE rusza każdą rzecz (np. FORTUNE bez pogłosu).

## Strona SYNTH
- **OSC 1–4:** kształt (SAW / SINE / TRIANGLE / SQUARE / PULSE / WAVETABLE), OCT / SEMI / FINE, LEVEL, PAN, SPREAD (stereo oscylatora),
  WT POS (WAVETABLE: sinus → trójkąt → piła → kwadrat; PULSE: szerokość impulsu), WARP (zgięcie fali), VOICES (unison 1–16), DETUNE,
  UNI SPREAD (rozłożenie głosów w stereo), UNI MIX (głośność bocznych głosów – „supersaw mix”), PHASE, RANDOM (losowa faza), RETRIG
  (włączone = każda nuta zaczyna od tej samej fazy; wyłączone = fala biegnie dalej).
- **GRUPA A (CORE)** = podstawa melodii, **GRUPA B (EXPANSION)** = szerokość / supersaw / góra. Każda grupa: VOLUME, PAN, WIDTH.
- **FILTER:** LP 12 / LP 24 / HP 12 / HP 24 / BAND / NOTCH, CUTOFF, RESONANCE, DRIVE, KEY TRACK, ENV AMOUNT; ROUTE (A + B / tylko A / tylko B przez filtr).
- **FILTER ENVELOPE / AMP ENVELOPE:** ATTACK, HOLD, DECAY, SUSTAIN, RELEASE, CURVE. Pluck = krótki decay i mały sustain, lead = duży sustain.
- **SCREAM:** krzyczący przester z kontrolą głośności (więcej SCREAM = inny charakter, nie głośniej). DRIVE, SATURATION, TONE, BITE (ostrość 2–3 kHz),
  BODY (ile dołu idzie przez przester), MIX, OUTPUT; 1x / 2x / 4x = jakość (oversampling).
- **PLAY:** POLY / MONO / LEGATO, liczba głosów, GLIDE, zakres pitch bend, TRANSPOSE, MASTER.

## Strona MOD
LFO 1 / 2 (SINE, TRIANGLE, SAW, RAMP, SQUARE, S&H, RANDOM; SYNC z nutami, też z kropką i triolami; PHASE, FADE IN, RETRIG),
MOD ENVELOPE, **MOD MATRIX** 16 slotów: źródło (obwiednie, LFO, velocity, mod wheel, aftertouch, key track, random, FORTUNE) →
cel (pitch, fine, wavetable, warp, cutoff, rezonans, detune, stereo, pan, głośność, poziomy oscylatorów i grup, drive filtra, SCREAM, delay, reverb), ilość ±.

## Strona FX
EQ (4 pasma), DISTORTION (z jakością 1x / 2x / 4x), CHORUS, DELAY (z tempem, ping-pong, filtr), REVERB (pre-delay, rozmiar, decay,
tłumienie, szerokość), STEREO (szerokość, balans, mono bas), LIMITER (input, ceiling, release), MASTER.
**FX ORDER** = kolejność efektów: EQ > DIST > CHORUS > DELAY > REVERB (domyślnie), DIST > EQ > …, … > REVERB > CHORUS (chorus na pogłosie),
EQ > CHORUS > DIST > … (przester na chorusie), DIST > EQ > DELAY > CHORUS > REVERB. STEREO i LIMITER są zawsze na końcu.

## Strona MELODY (generator melodii + piano roll)
**Generator:** KEY (tonacja), SCALE (MAJOR / MINOR / HARMONIC MINOR / MELODIC MINOR / DORIAN / PHRYGIAN / CHROMATIC / CUSTOM –
przy CUSTOM klikasz nuty skali), MODE (CLASSIC, EUPHORIC, RAWPHORIC, MELODIC RAW, FESTIVAL, EMOTIONAL, ANTHEM, DARK, MINIMAL),
długość frazy (1 / 2 / 4 / 8 taktów), zakres oktaw, ROOT BOUNCE (niski bas akordu na słabej części bitu – jak w „No Time To Waste”),
gałki DENSITY (ile nut), COMPLEXITY (synkopy, nuty przejściowe), REPETITION (ile motyw wraca), MOVEMENT (wielkość skoków),
RHYTHM (1/4 → 1/8 → shuffle 1/8T → 1/16).
Tryby zmieniają **zasady**, nie tylko nuty. CLASSIC, EUPHORIC i RAWPHORIC są zrobione na wzór Twoich plików MIDI (Headhunterz,
D-Block & S-te-Fan, Brooklyn Bounce, Nasty Boys / Ran-D, Wildstylez, Audiotricz, The Vision & Omegatypez / Sub Zero Project):
- **CLASSIC** = lead na każdy bit nad bujającym basem, stoi na nucie akordu albo idzie po akordzie, ruch (obrót 2–3 nut) jest na 4. bicie
  i prowadzi do następnego akordu;
- **EUPHORIC** = nowa nuta prawie na każdy bit, na „raz” i „trzy” nuty akordu, między nimi przejścia; RHYTHM w środku = bujanie,
  na skraju = proste ósemki z przytrzymanymi nutami i szesnastką przed taktem (jak „Inception”);
- **RAWPHORIC** = długa nuta, potem bieg w górę na 3.–4. bicie do następnego taktu (jak „Maze of Memories”).
Budowa jak w utworach: akordy w molu (i, VI, VII, iv, III i durowe V), jeden na takt; 4 takty = motyw, odpowiedź, motyw na nowym akordzie,
nowe zakończenie; 8 taktów = te 4 jeszcze raz z nowym ostatnim taktem. Ostatni takt prowadzi z powrotem do pierwszego (pętla się klei).
Pozostałe tryby: MELODIC RAW / DARK = ciemniej, chromatyczne przejścia, FESTIVAL = długie nuty i skoki o oktawę, MINIMAL = mało nut.
**SHIFT** (0 / +1 / +2 / +3 bity) = pierwsze bity frazy idą na jej koniec – to, co robiłeś ręcznie w FL („2 pierwsze beaty na koniec”).
Każda zmiana gałki od razu tworzy melodię od nowa (Ctrl+Z cofa). **GENERATE** = od nowa z tych ustawień, **NEW IDEA** = nowa melodia.
**PREVIEW** = 4TUNE gra melodię sam (gdy FL gra – w rytm projektu, gdy stoi – na własnym zegarze).

**Piano roll:** dwuklik = nowa nuta, przeciąganie = przesuwanie (czas + wysokość, Alt = bez siatki), prawa krawędź = długość,
prawy klik = usuń, przeciąganie po pustym = zaznaczanie, dolny pasek = velocity. Klawisze: Delete, Ctrl+A / C / V / D, strzałki
góra/dół = półton (z Shift = oktawa), Q = kwantyzacja, Ctrl+Z / Ctrl+Y. Przyciski: QUANTIZE, HUMANIZE, -1 / +1, OCT - / OCT +,
COPY, PASTE, DUPLICATE, DELETE, CLEAR, UNDO, REDO, GRID (1/4 … 1/32, też triola).
**DRAG MIDI >> FL:** złap i upuść w playliście / piano rollu FL – melodia ląduje jako plik MIDI (wysokość, długość, czas, velocity).
Melodia zapisuje się w projekcie i nie znika przy zmianie presetu brzmienia.

## Wygląd
Liquid glass w fiolecie i limonce (jak Fortune of Seekers): grupa A limonka, grupa B fiolet, FORTUNE najjaśniejsza limonka.

## Presety (strona PRESETS albo klik w nazwę presetu)
30 presetów fabrycznych: najwięcej plucków, leadów i supersawów, do tego po kilka w pozostałych kategoriach.

Po lewej kategorie: ALL, FAVOURITES (ulubione), PLUCKS, LEADS, SUPERSAWS, PADS, BASS, KEYS, EXPERIMENTAL, FESTIVAL, EUPHORIC, RAW, CLASSIC, USER.
Na górze listy **szukanie** po nazwie, kategorii i autorze. Dwuklik = wczytaj. **FAVOURITE** (ta sama gwiazdka co na pasku presetów),
**RENAME** i **DELETE** (do kosza) dla Twoich presetów, **DUPLICATE** = kopia. **SAVE AS:** nazwa + kategoria + autor
(autor zapamiętuje się na następny raz) → `Dokumenty\4TUNE\Presets\*.4tune`. Preset zapisuje samo brzmienie – melodia zostaje w projekcie.
**RANDOMIZE** = SMART RANDOM (muzyczne zakresy, bez przesady z detune / rezonansem / przesterem / pogłosem / głośnością),
**FULL RANDOM** = wszystko w całym zakresie.
W AMP ENVELOPE przyciski **PLUCK** / **LEAD** ustawiają od razu obwiednię plucka albo leada.
