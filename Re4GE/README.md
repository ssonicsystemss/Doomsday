# RE4GE v0.12

## v0.12 (krok 6, część 2): SEKWENCER + GATE
**03 MODULATION → Sequencer** – 16 kroków (rozmiar kroku = STEP w Steps: 1/4, 1/8, 1/16):
- klik w numer = krok gra / nie gra, przeciągnij słupek = **głośność** kroku, przeciągnij dolne pole = **pitch** (−12…+12 półtonów),
  dwuklik = reset. **LENGTH** = ile kroków ma wzór (powtarza się), **SWING** = co drugi krok później (jego narastanie się ściska,
  więc nadal kończy się przed następnym krokiem). Przełącznik **SEQ** włącza wzór (wyłączony = stare STEPS 1–4).
- Kroki liczą się **po siatce FL**: nuta na 2. beacie zaczyna od właściwego kroku wzoru (np. 1/16 → krok 5), więc wzór zawsze
  siedzi tak samo w takcie, nawet gdy nuty są krótkie.
**03 MODULATION → Gate** – tnie gotowy bas w rytmie (nie wyzwala go): **RATE** 1/8 / 1/16 / 1/32 / 1/8T / 1/16T, **DEPTH**,
**LENGTH**, **SMOOTH** (ms), kształt **HARD / SOFT / TRANCE**, pasmo **FULL** albo **HIGHS** (tnie tylko powyżej ~250 Hz – dół gra dalej).

Randomizer (v0.11) po uwagach: EXPERIMENTAL bez „ciamkającego” filtra (tylko LP / OFF), więcej głębi (unison 4–7, stereo, smear,
otwarta góra), bez skrajnych skoków pitchu i głośnego szumu; we wszystkich stylach **trzeszczenie wchodzi później** (wolniejsze narastanie,
przester rośnie razem z basem).

## v0.11 (krok 6, część 1): RANDOMIZER W KIERUNKACH
**05 SYSTEM → Randomizer** (nowy przycisk obok Settings). Przycisk **Random** losuje z tymi ustawieniami.
- **MODE** – co się rusza: FULL (wszystko) / OSC (źródła, warstwy OSC 2–4, unison) / PITCH / DIST / FILTER (pre filtr + EQ) /
  MOD (1–3 nowe połączenia LFO → MATRIX) / REVERSE (tylko obwiednia, ruch pitchu i Shape).
- **STYLE** – w którą stronę: CLASSIC (umiarkowany drive, swell, mało warstw) / MODERN (warstwy, smear, szerokość, LAB SAT / DIODE) /
  RAW (mocny drive, asymetria, fold / crush, ogon kicka) / EXPERIMENTAL (szerokie zakresy, warp, PM, wavefold).
- **AMOUNT** – jak daleko od obecnego brzmienia: 0 % = nic, 30 % = wariacja, 100 % = nowy bas.
- **LOCK** – zablokowane moduły nigdy się nie zmieniają (np. zablokuj ENV i losuj resztę).
- Zasady zawsze: dół zostaje (LOW CUT wyłączony, SUB 60+ Hz), bas startuje **po ogonie kicka** (DELAY 20–45 %, zwykle 24–38 %),
  zawsze coś gra, SAMPLE tylko jeśli jest wczytany kick.
- **Głośność wyrównana**: każdy wylosowany bas jest zmierzony w tle i ustawiony na ok. −14 dB RMS (nie oceniasz „głośniejsze = lepsze”).
- **Jedno kliknięcie = jeden krok Undo** – nie podoba się, Ctrl+Z / Undo i wracasz.

## v0.10 (krok 5, część 2): RE4GE FX
**Druga wtyczka: RE4GE FX** (efekt na mikser, ten sam silnik i te same presety). Artifact `RE4GE-FX-VST3-Windows`, folder `RE4GE FX.vst3`.
- Wstaw na kanał z kickiem (albo na kanał basu i wyślij kick na **sidechain**). FX sam **znajduje kicki** w audio (lampka HITS) i każdy
  odpala reverse bass, jakbyś pisał nuty. **Threshold / Sensitivity / Holdoff / Input**, źródło detekcji **Input / Sidechain**.
- **Dry**: kick przechodzi nietknięty (sprawdzone próbka po próbce), bas dochodzi do niego. Wyłączony = tylko bas.
  **Bass** = poziom basu, **Note** = tonacja numeru.
- **Live reverse** (ze źródłem SAMPLE): basem jest **ogon POPRZEDNIEGO kicka puszczony od tyłu**. Pierwszy kick po ciszy nie ma basu,
  bo nie ma jeszcze poprzedniego. **Live from** pomija klik / punch.
- Strona **04 SOURCE → Audio Input** (w RE4GE FX otwiera się od niej).


## v0.9 (krok 5, część 1): KICK-TO-REVERSE
**04 SOURCE → Kick > Reverse**: wrzuć (przeciągnij) swojego kicka, np. wyrenderowanego z 4GE.
- Wtyczka sama dzieli go na **TRANSIENT / PUNCH / BODY / TAIL** (kolorowe pasy pod waveformem, przy każdym pewność wykrycia w %)
  i znajduje jego **nutę** (ROOT, z odchyłką w centach).
- Po wrzuceniu od razu gra **TAIL od tyłu**. Przyciski **Tail / Body+Tail / Punch+ / All** wybierają, która część gra.
  Białe uchwyty na waveformie (albo gałki START / END) ustawiasz ręcznie; kolejne DETECT nadpisuje je dopiero po kliknięciu.
- **VARISPEED / KEEP LENGTH**: przy VARISPEED wyższa nuta gra szybciej i krócej (jak płyta). Przy **KEEP LENGTH** gra wyżej, ale tak samo długo
  (ziarna WSOLA jak w Ancient Drums, każde ziarno dopasowane do fali, bez fazowania).
- **ORIGINAL**: kick taki, jaki jest (bez przesterów RE4GE). **PROCESSED**: przez cały tor RE4GE (filtr, przestery, EQ, makra, matrix).
  HYBRID = dokręć OSC 2–4 i SUB, a grają razem z kickiem, nastrojone do nuty.
- Po Twoim odsłuchu (znów „za wcześnie”): Dense Rise i pochodne startują po **28 % kroku**, Late po 38 %. Gałka **DELAY** jest teraz na MAIN.
- **Kick zapisuje się W PROJEKCIE i W PRESECIE** (OWN SOUND). Jak plik zniknie z dysku, gra kopia ze środka (wtyczka to pokazuje).


## v0.8 (krok 4): LFO 1–4 + MATRIX MODULACJI
**03 MODULATION**:
- **LFO**: 4 LFO, a każde ma falę (SINE / TRI / SAW+ / SAW− / SQR / S&H / SMOOTH / CHAOS), **SYNC** (wartości nut w tempie FL) albo RATE w Hz, PHASE
  i **RETRIG** (włączony = każdy krok basu rusza LFO od początku, czyli każde uderzenie ma ten sam ruch).
- **Matrix**: 16 wierszy. Każdy ma ON, **SOURCE** (LFO 1–4, AMP ENV, linie z SHAPE, velocity, mod wheel, aftertouch, pitch bend, KEY, makra,
  RANDOM na krok ze stałym ziarnem, STEP), **DESTINATION** (28 celów: poziomy OSC, pitch, filtr, drive, stage 2, warp, PM, bell, high cut, sub,
  szerokość, smear, detune, output, hardness / tone LAB, click, swell), **AMOUNT** (±, 100 % = cały zakres opisany w nazwie celu) i **POLARITY**
  (AS IS / UNI / BI). Kilka wierszy na jeden cel się sumuje. Gałki się nie ruszają, więc automacja i presety zostają czyste.
- Makra dalej robią swoje (stałe cele) i są też źródłami w matrixie.
- Nowy preset **MOD - Dense Rise Talk 1/16**: LFO (trójkąt 1/16) gada filtrem, a obwiednia otwiera HARDNESS przesteru.


## v0.7 (krok 3): DISTORTION LAB
**02 PROCESS → Distortion**: obok każdego stopnia (STAGE 1 / 2) jest **LAB**:
- **CLASSIC**: typy z 4GE (Soft … Crush), jak dotąd.
- **SAT**: tanh ↔ twardy clip. **DIODE**: asymetryczny (parzyste harmoniczne, ciasny dół). **FOLD**: wavefolding.
  **RECT**: prostownik (HARDNESS 0 = zwykły, 50 % = półokresowy, 100 % = pełny, czyli oktawa w górę).
  **CRUSH**: prawdziwy bitcrusher (mniej bitów i niższa częstotliwość, aliasuje celowo). **CUSTOM**: Twoja narysowana krzywa.
- Wszystkie oprócz CRUSH mają **ADAA**, czyli dużo mniej cyfrowego „piasku” przy mocnym przesterze (test: od −5,7 do kilkunastu dB mniej aliasingu).
- Gałki: **HARDNESS** (charakter krzywej), **TONE** (pochylenie przy 800 Hz, **PRE** = zmienia to, co się przesterowuje, **POST** = tylko kolor), **OUTPUT**.
  STAGE 2 dostał też ASYM.
- **02 PROCESS → Waveshaper**: rysujesz krzywą CUSTOM dla stopnia 1 / 2 (X = wejście, Y = wyjście). **Biała kropka** pokazuje, jak głęboko bas
  wchodzi teraz w krzywą. SYMMETRIC (rysujesz prawą połówkę), Save / Load do `Dokumenty\RE4GE\Shapes`, Reset, Undo. Krzywe zapisują się też w presecie.
- **SYSTEM → Settings**: **QUALITY** (oversampling przy graniu 2× / **4×** / 8× / 16×, osobno przy eksporcie z FL, domyślnie **8×**), **AUTO GAIN**.
- Nowy preset **LAB - Diode Fold Rise**: Dense Rise z DIODE na stopniu 1 i WAVEFOLD na stopniu 2.
- Po Twoim odsłuchu („wchodzi trochę za szybko”): Dense Rise / LAB / Drawn startują teraz po **20 % kroku** (między wariantem B a C) z późniejszym
  narastaniem. Wariant D jest osobno jako **CLASSIC - Dense Rise Late** (start po 33 %).


## v0.6 (krok 2): REVERSE SHAPE DESIGNER
**01 SOUND → Shape**: jeden ekran, 4 rysowane linie na osi jednego kroku (od kicka do kicka). Czerwona kreska = koniec basu (GATE).
- **AMP**: głośność. DEPTH: ile rysunek zastępuje obwiednię z gałek (0 = gałki, 100 % = tylko rysunek). **From knobs** przerysowuje obecną obwiednię.
- **PITCH**: środek = na nucie, góra = +RANGE, dół = −RANGE (do 24 półtonów). Domyślnie podjazd w kicka.
- **HARMONIC**: jak bardzo bas się „otwiera”: skaluje DRIVE i, gdy filtr PRE jest włączony, przesuwa go w dół (do 4 oktaw).
- **PHASE**: skaluje PHASE WARP i PM w OSC 2–4.

Każda linia ma ON, LENGTH (% kroku), ANCHOR (**START** = od początku kroku, **END** = kończy się dokładnie tam, gdzie bas, czyli po reverse'owemu),
ONE SHOT / LOOP, DEPTH / RANGE i Reset.
Na wykresie: klik dodaje punkt, przeciąganie przesuwa, **Alt albo prawy przycisk + ciągnięcie** wygina odcinek, a podwójny klik usuwa punkt. Każda zmiana ma **Undo**,
a krzywe zapisują się w presecie i projekcie.
- Nowy preset **SHAPE - Drawn Rise Talk**: Dense Rise + rysowana głośność, podjazd pitchu, harmoniczne i warp otwierające się pod kick.
- TRIGGER / STEPS przeszły do **03 MODULATION → Steps**.


## v0.5 (krok 1, część 2)
- **SUB**: OCTAVE (0 / −1), HARMONICS (czyste 2. i 3. harmoniczne, żeby sub był słyszalny na małych głośnikach), SATURATION (domyślnie 0).
- **TRANSIENT → CLICK**: krótki tick z szumu z własnym HP, poza obwiednią i przesterem, na START basu albo w jego szczycie (PEAK).
- **PITCH → GLIDE**: pitch dojeżdża z poprzedniej nuty, kiedy bas jeszcze gra.
- **STEREO → SMEAR**: krótki dyfuzor na środku i górze, który wypełnia przerwy między harmonicznymi, jak pogłos w odwróconych kickach z paczek.
  Dół zostaje czysty. CLASSIC - Dense Rise ma go teraz na 40 %.


## v0.4 (krok 1 planu, część 1): RDZEŃ BRZMIENIA + NOWY EKRAN
- **Nowy ekran** według makiety. Po lewej pasek z blokami **01 SOUND / 02 PROCESS / 03 MODULATION / 04 SOURCE** (klik rozwija listę modułów,
  moduł zajmuje prawą stronę), na dole oddzielony **05 SYSTEM** (Save, Folder, Random, Play). **Makra i MASTER zawsze na dole**, preset i BPM u góry.
  Czerwona kropka przy module = moduł coś teraz robi. Na liście są tylko moduły, które już działają (MODULATION dojdzie w kroku 4).
- **MAIN**: podgląd beatu + SOURCE (poziomy OSC 1–4, Tune), ENVELOPE, TONE (Drive, Stage 2, Filter, High cut), LOW END (Sub Hz, Sub, Width).
- **OSC 2 / 3 / 4** obok OSC 1 (SYNTH / TAIL / SAMPLE): SINE / TRI / SAW / SQR / PULSE / NOISE, LEVEL (OFF), OCT, SEMI, FINE, PHASE, WIDTH
  (PULSE: szerokość, NOISE: jasność), **SYNC** (twardy do nuty), **PM**. Wszystkie warstwy idą razem przez przester, stąd gęstość.
- **PHASE**: RESET (każdy krok startuje tak samo) / FREE.
- **FILTER**: Env (filtr otwiera się z narastaniem basu) i Key (wyższe nuty jaśniej).
- **DISTORTION**: Stage 2 po EQ (z podbitej prezencji robi gęstą górę).
- **8 MAKR**: BODY (220 Hz), WEIGHT (sub), REVERSE (późniejsze narastanie + podjazd + późne otwarcie), PITCH (odbicie z góry), GRIT (przester),
  BITE (2 kHz + góra), MOVEMENT (unison), SPACE (szerokość). 0 = to, co ustawione w modułach.
- Preset **CLASSIC - Dense Rise**: synteza dopasowana pomiarem do Twoich Bass_15 / 16.


**Reverse bass** jako osobny instrument VST3 (FL Studio). Wydzielony z 4GE i zbudowany według analizy architektury reverse bassu
(`Shared/RESEARCH.md`, „Reverse bass”). Przester, filtry i ogon kicka są te same co w 4GE (wspólny silnik `Shared/Source/GeKick.h`).

## v0.3 (krok 0 planu): PRZEBUDOWA W ŚRODKU, BRZMIENIE BEZ ZMIAN
Fundament pod dalszą budowę (`PLAN.md`). Twoje presety i ustawienia grają tak samo jak w v0.2, sprawdzone próbka po próbce (różnica najwyżej −140 dB).
- **Silnik podzielony na klocki** (`Source/DSP/`: trigger, obwiednia, oscylator, ogon kicka, ton, sub / stereo). Osobno parametry (`Parameters/`),
  presety i pliki (`Presets/`) i GUI (`GUI/`). Dźwięk, wygląd i pliki nigdy nie są w jednym miejscu.
- **Wynik nie zależy od bufora FL**: silnik liczy w kawałkach po 32 próbki. Bufor 1, 64, 512, 4096 albo zmienny daje ten sam dźwięk.
- **Ogon kicka bez skoków CPU**: ogon liczy się po kawałku w ciszy przed basem, a nie cały naraz w chwili nuty.
  **Gałki KICK TAIL działają na żywo**: przy BEATS 2–8 zmiana słychać od następnego kroku (wcześniej dopiero od nowej nuty).
- **Nowe nazwy parametrów** z przedrostkami modułów (w FL na liście automacji: ENV Swell, DIST Drive, EQ High Cut…). Od teraz są **zamrożone na zawsze**
  (lista w `Tests/param_ids.txt`, test pilnuje). Twoje zapisane presety `.re4ge` z v0.1 / v0.2 dalej się wczytują.
- Nowe testy: każda gałka zmienia dźwięk (78 z 78), bufor bez wpływu na wynik, ID zamrożone, ogon na żywo, brak skoku CPU.

## v0.2: PEŁNIEJSZY BAS
- **STEREO**: środek gra czysty (dół i SUB mono, stabilne), a lewo i prawo dostają rozstrojone, „pływające” kopie (SYNTH: dodatkowe oscylatory,
  TAIL / SAMPLE: dodatkowe głowice czytające). Karta **UNISON / WIDTH** (zakładka Tone): Voices (domyślnie 5), Detune, Drift, Mix, Width, From.
- **SUB zostaje na nucie**: przy TUNE +12 bas jest oktawę wyżej, a sub dalej gra na granej nucie (wcześniej szedł w górę i filtr go wycinał).
- **ROOM / SIZE w KICK TAIL**: pogłos na ogonie **przed** odwróceniem. Od tyłu narasta w bas, jak w odwróconych kickach z paczek.
- Presety z ogonami mają otwartą górę (HIGH CUT 9 kHz), bo Twoje sample basów są jasne i gęste aż do 8 kHz.

## Instalacja (Windows)
GitHub → **Actions** → *Build RE4GE VST3* → **Artifacts** → `RE4GE-VST3-Windows`.
Folder `RE4GE.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, a w FL: *Options → Manage plugins → Find installed plugins*.
Wstaw jako **instrument** na osobny kanał obok kicka (4GE albo dowolnego).

## Jak tego używać (2 sposoby)
- **PATTERN** (domyślnie): w piano rollu RE4GE **piszesz te same nuty co kick**, czyli nuta na każdym kicku. Bas sam gra kroki
  (1/4, 1/8 albo 1/16) między kickami, na wysokości nuty. **STEPS** wybiera, które kroki grają (np. tylko offbeaty), **OFFSET** je przesuwa,
  a **BEATS** mówi, ile beatów basu gra jedna nuta.
- **GATE**: każda nuta to jedna nuta basu. Długość nuty w piano rollu = długość basu, a po puszczeniu jest **FADE**. Tak piszesz zwykłą linię basową.

**TIMING**:
- **SYNC**: DELAY / HOLD / DECAY / GATE w % kroku, więc bas sam pasuje do tempa;
- **MS**: te same czasy w milisekundach.

**SWELL** jest zawsze w ms.

## Ekran
Na górze presety, pod spodem **jeden beat** w tempie FL z podziałką w ms. Pomarańczowe = kick (Twoja nuta), fioletowe = bas,
przerywana linia = pitch. **MASTER**:
- **LEVEL**;
- **CLIP**: miękkie wyrównanie szczytów;
- **SUB Hz** + **SUB LEVEL**: poniżej SUB Hz bas jest czystym sinusem na nucie. Nigdy nie jest ucinany ani przesterowany, to jest to „pierdolnięcie”;
- **VELOCITY**.

### SOURCE
- **SOURCE**:
  - **SYNTH**: piła ↔ pulse (SAW/PULSE), WIDTH, SINE na fundament;
  - **TAIL**: klasyka, czyli ogon kicka puszczony **od tyłu**. Jego zjazd pitchu staje się podjazdem przed kickiem;
  - **SAMPLE**: Twój WAV, np. kick wyrenderowany z 4GE. Przeciągnij go na wtyczkę, a SAMPLE włączy się sam.
- **PHASE WARP** (tylko SYNTH): zagina fazę oscylatora, co daje inne harmoniczne niż drive.
  - **BEND**: sinus → piła, „sprężysty” bas;
  - **SQUEEZE**: nosowy, prawie kwadrat;
  - **SYNC**: gadający, jak hard sync;
  - **MIRROR**: metaliczny, parzyste harmoniczne.

  **AMOUNT** to siła, **ENV** sprawia, że warp otwiera się razem z narastaniem basu.
- **KICK TAIL** (dla TAIL): START, DROP, CURVE, WAVE, typ przesteru, DRIVE, TONE, HIGH CUT, SCREAM, SCREAM Hz. W trybie GATE dochodzi **LENGTH**: ile ogona grać od tyłu.
- **SAMPLE**: LOAD / x, **REVERSE** (od tyłu, kończy się na końcu gate'u), **ROOT** (nuta WAV-a), START.

### SHAPE
- **TRIGGER**: PATTERN / GATE, SYNC / MS, krok 1/4 / 1/8 / 1/16, OFFSET, BEATS.
- **STEPS**: 1–4, które kroki beatu grają (1/8: 1 + 2, 1/16: 1–4).
- **PITCH**:
  - **STABLE**: na nucie;
  - **DROP**: z góry na nutę na początku;
  - **RISE**: podjazd na końcu, klasyczny reverse;
  - **BOUNCE**: oba.

  Gałki: AMOUNT (półtony), TIME, TUNE, FINE, **KEY TRACK** (wyłączony = zawsze na NOTE, nuty dają tylko rytm).
- **ENVELOPE**: jedna obwiednia na wszystkie style:
  - **DUCK**: poziom, zanim bas wejdzie (0 = cisza pod kickiem);
  - **INITIAL**: od jakiego poziomu bas rusza;
  - **SWELL**: czas narastania (Twoje wzorce FHSP: 50–80 ms);
  - **CURVE**: + = szybko, potem ląduje (jak sidechain); − = wolno, później;
  - **SUSTAIN** + **DEC CURVE**: dokąd i jak opada;
  - **FADE**: cięcie przed kickiem.

  Do tego **TIMES**: DELAY, HOLD, DECAY, GATE.
  Szablony jednym klikiem (zmieniają tylko obwiednię, barwa zostaje):
  - **PUMP**: jak FHSP, szybki powrót, półka, cięcie;
  - **SWELL**: oddychający, szczyt wcześnie, długi ogon;
  - **TIGHT**: krótki, zwarty;
  - **ROLL**: rozlewający się;
  - **PULSE**: 1/16, agresywny.

### TONE
- **PRE FILTER** (przed drive'em): OFF / LP / HP / BP / NOTCH, CUTOFF, RESO.
- **DRIVE**: typ (SOFT, TUBE, HARD, FOLD, RAW, CRUSH), DRIVE, **ENV** (czysty na starcie, najbrudniejszy w szczycie), **ASYM**, **MIX**.
- **POST EQ**: **LOW CUT** (domyślnie OFF, tnie tylko przesterowaną część, nigdy SUB), HIGH CUT, BELL (Hz, gain, Q).

**Zasada:** domyślnie dół zostaje nietknięty. Zabiera go tylko świadomie ustawiony LOW CUT.

## Presety
- **INIT - Pump Tail**.
- Twoje ogony z Forge z basem jak w presetach BASS:
  - **PUMP - QL Tail**;
  - **PUMP - Volcanic Tail [F]**;
  - **PUMP - Undertow Tail [G]**.
- Na każdy szablon:
  - **SWELL - QL Tail**;
  - **SWELL - Synth Bend**;
  - **TIGHT - Synth Squeeze 1/8**;
  - **ROLL - Synth Mirror**;
  - **PULSE - Synth Sync Offbeats**.
- **RISE - QL Tail Pitch Up**.
- **GATE - Bass Line (notes)**: do pisania linii basu nutami.

## Plan (v0.2)
- Rysowane linie w okienku: AMP / DRIVE-WARP / PITCH.
- Opcjonalne wejście sidechain od kicka, żeby GATE wyciszał bas, kiedy wpada kick.
- Podział klawiatury.
