# 4GE v0.6 – Hardstyle Kick Designer (przebudowa od zera)

Syntezator kicków hardstyle (instrument VST3, FL Studio). **Nowy 4GE zbudowany od zera** według Twojej specyfikacji
17 modułów (`SPEC.md`, `MODULE01.md`…`MODULE12.md`, `MODULES13-17_overview.md`, zasady nadrzędne w `RULES.md`, plan w `PLAN.md`).
**v0.6 = wszystkie 17 modułów** – od CORE KICK ENGINE (moduł 01) po docelowy interfejs (moduł 17).

## INTERFEJS (moduł 17)
- **Lewa strona = nawigacja w 5 blokach**: 01 KICK (MAIN, ENVELOPES) · 02 SYNTHESIS (OSCILLATORS, LAYERS, SAMPLES) ·
  03 PROCESSING (DISTORTION, SCREECH, EQ, DYNAMICS, STEREO) · 04 MODULATION (MOD MATRIX) · 05 SYSTEM (PRESETS, ANALYZE, EXPORT, SETTINGS).
  Szare numerki = numery modułów (wszystkie 17 mają swoje miejsce). **Czerwona kropka** = ta część coś teraz robi z dźwiękiem
  (np. włączony oscylator, stopień distortion, sampel w slocie, modulacja).
- **Góra**: STYLE (EARLY / MODERN / RAW / EXP – klik = zastosuj z zachowaniem Twoich zmian, prawy klik = APPLY ALL / PREVIEW / FORGET),
  LENGTH (SHORT / MEDIUM / LONG / XL – jeden klik = jeden UNDO; przycisk z ms = panel długości), presety, RANDOMIZE / SAVE / PLAY / UNDO / REDO.
- **Linia INFO** pod nimi: najedź na cokolwiek, a zobaczysz nazwę, wartość, zakres, wartość domyślną, czy jest modulowane, czy to
  „Twoja” zmiana (STYLE jej nie ruszy) i co robi z dźwiękiem. Bez myszki nad kontrolką = gdzie jesteś i co jest na tej stronie.
- **Dół – zawsze widoczny**: 8 MASTER MACROS (strzałkę **>** przeciągnij na dowolną gałkę = modulacja) i GLOBAL: LENGTH, MASTER, VELOCITY,
  CHOKE / POLY, KICK END (LENGTH / NOTE / +FX), PHASE RESET, AUTO LENGTH, miernik OUT. PITCH TRACKING jest teraz w PITCH ENGINE.
- **Prawy klik na każdej gałce** (każda strona): RESET TO DEFAULT, TYPE A VALUE, COPY / PASTE VALUE, MODULATE WITH…, usuwanie modulacji.
- **Upuść plik gdziekolwiek**: WAV / AIFF / FLAC → menu: do następnego wolnego slotu SAMPLES albo jako referencja ANALYZE; `.4gp` → wczytuje preset.
- **Skróty**: Ctrl+Z / Ctrl+Y = cofnij / ponów, Ctrl+← / → = poprzednia / następna strona (gdy okno 4GE ma fokus – FL czasem zabiera skróty).
- **SETTINGS**: rozmiar okna (FIT = największy, który mieści się na ekranie, albo 70–150 %), dymki z podpowiedziami ON / OFF, linia INFO ON / OFF,
  od której strony 4GE się otwiera (np. ostatnio używana), lista wszystkich skrótów myszki / klawiatury. Rozmiar możesz też zmienić,
  ciągnąc róg okna – zapamięta się. Te ustawienia są Twoje (plik w `AppData\Roaming\4GE`), a nie projektu.

> **Ważne przy instalacji:** nowy 4GE ma wszystkie parametry nowe, więc ma też **nowy identyfikator wtyczki**.
> Jeśli masz projekty FL ze starym 4GE v0.4, **nie kasuj starego pliku**: zmień jego nazwę na `4GE v0.4.vst3`
> i zostaw obok nowego `4GE.vst3`. Stare projekty otworzą się ze starą wtyczką i będą brzmieć jak dawniej, a nowe kicki robisz w nowej.
> (Na liście wtyczek FL zobaczysz dwa „4GE” – nowy ma w okienku napis v0.5 lub nowszy.)
> Stare presety `.4ge` nie wczytują się do nowego 4GE (świadomie – nowy silnik). Nowe presety zapisujesz w `Dokumenty\4GE\Presets` (`.4gp`).

## Instalacja (Windows)
GitHub → zakładka **Actions** → *Build 4GE VST3* → **Artifacts** → `4GE-VST3-Windows`.
Folder `4GE.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, a w FL: *Options → Manage plugins → Find installed plugins*.
Wstaw jako **instrument** (Channel Rack) i graj nutą (np. G1 = 49 Hz).

## Jak działa kick (moduł 01)
Kick składa się z **czterech niezależnych części**. Każda ma własne źródło, obwiednię, długość, ton, drive i fazę.
Spotykają się dopiero w **CORE MIXER**.

| Część | Kolor | Co robi |
|---|---|---|
| **PUNCH** | czerwony | pierwsze uderzenie: źródło SINE / TRI / SAW / SQUARE / NOISE / CLICK, własny PITCH (Hz), który w GLIDE ms spada do pitchu kicka |
| **BODY** | bursztyn | środek kicka: własny PITCH, który spada do pitchu kicka (dłuższy GLIDE), TONE + RESONANCE, DRIVE |
| **TAIL** | fiolet | ogon: gra dokładnie pitch kicka (START → nuta), źródło SINE / TRI / SAW / SQUARE / PULSE / NOISE, **CHARACTER** |
| **SUB** | niebieski | czysty dół: sinus / trójkąt **na granej nucie**, mono, bez przesteru (DRIVE domyślnie 0) |

**TAIL CHARACTER** – siedem różnych algorytmów nasycenia (nie zmiana nazwy presetu):
CLEAN (prawie czysty) · TONAL (lampa, parzyste harmoniczne) · DIRTY (asymetryczny twardy clip) · SCREAM (rezonansowe pasmo w pętli) ·
METAL (modulacja pierścieniowa nieharmoniczną nośną + fold) · BUZZ (kwadratowy, nieparzyste harmoniczne) · CRUSH (bity + zmniejszanie sample rate).
Hardstyle'owy ogon potrzebuje dużo DRIVE – dopiero wtedy ma ostre krawędzie, czyli górę.

**Obwiednia** każdej części: ATTACK → DECAY (do SUSTAIN) → trzyma → RELEASE do końca jej LENGTH.
Najkrótsze możliwe wejście to 0,15 ms, a wyjście 3 ms, więc nie ma klików nawet przy ATTACK / RELEASE = 0.

## Ekran
- **Góra**: LENGTH (100–2500 ms), MASTER, VELOCITY; przełączniki **AUTO LENGTH**, **PHASE RESET**, **PITCH TRACKING**; **CHOKE / POLY**;
  presety + PLAY / RANDOMIZE / SAVE / UNDO / REDO.
- **PITCH ENGINE**: tryb **CLASSIC / FAST / SLOW / DOUBLE / RISE / STABLE**, START, END, TIME, CURVE (nisko = spada szybko i wcześnie,
  wysoko = trzyma się wysoko i spada późno), AMOUNT (dodatkowe półtony na uderzeniu), FINE,
  **HOLD** (pitch stoi na START przez tyle ms, potem spada – tonalny „tok”, to daje „pup pup pup”) i **TOK LEVEL**
  (głośność ogona w czasie HOLD – ciszej = wejście basu po toku mocniej uderza).
- **PUNCH / TOK → BODY + TAIL** (dwa etapy kicka): w trybie **TOK** część PUNCH to tok („pup pup”) = **TRANSIENT** (bardzo krótki
  klik: TRANSIENT, TR LENGTH, TR TONE) + **TOK CORE** (źródło, nuta, długość, przester). Tok trzyma jedną nutę przez swoją LENGTH
  (bez zjazdu), a **BODY i TAIL fizycznie zaczynają się, gdy tok się kończy** – to nie jest HOLD ogona. Ogon zaczyna zjazd **z nuty
  toku**, w tej samej fazie, a **OVERLAP** (domyślnie 20 ms) to płynne przejście równej mocy – bez słyszalnego szwu; krócej = twardsze cięcie.
  W stylach tok dostaje też ten sam łańcuch przesteru co ogon (barwa się nie łamie).
  **FIXED** = tok gra zawsze nutę z **TOK NOTE** (np. G „choćby ogon chodził w MIDI po różnych nutach”), **FOLLOW MIDI** = idzie za nutą.
  **OFFSET** = tok gra nutę z MIDI + **NOTE OFFSET** półtonów (np. +12 = oktawa nad ogonem).
  **TOK LENGTH** (gałka LENGTH na karcie PUNCH / TOK w trybie TOK) – w stylach: EARLY ~15–50 ms, MODERN ~80–140, RAW ~80–150,
  EXPERIMENTAL 5–150 (zakresy RANDOMIZE). Tok to pełna tonalna faza początku kicka, nie tylko klik.
  **TOK DECAY** (dB) – mocny początek toka i kontrolowane opadanie do końca, gdzie ogon przejmuje energię (MODERN -4, RAW -5).
  **BEND** + GLIDE – własna obwiednia pitchu toka (zaczyna BEND półtonów obok i dojeżdża do swojej nuty w GLIDE ms).
  **TAIL START MODE** (karta TAIL): **AFTER TOK** (domyślnie) = body + tail startują po toku; **INDEPENDENT** = startują w **TAIL START**
  ms – np. tok 125 ms, tail od 90 ms = świadome nakładanie (tok gra do swojego końca).
  **SUB START** (gałka START na karcie SUB = opóźnienie obwiedni suba) – sub ma własny start i może wejść pod tok (MODERN 75 ms,
  RAW 55 ms), nie czeka na koniec toka.
  BODY i TAIL mają dalej własne obwiednie i charakter. Tryb **PUNCH** = dawny krótki punch (zjeżdża w pitch kicka, ogon od uderzenia).
- **BODY bez „łup”**: BODY nie ma już własnego zjazdu pitchu – idzie za ogonem w stałym interwale (FOLLOW → TAIL, 0 / +7 / +12 st),
  w MODERN / RAW to szum (gęstość, zero pitchu). RANDOMIZE losuje interwał, źródło, przester, barwę i poziom BODY w każdym stylu.
- **START (PITCH ENGINE): FIXED / FOLLOW MIDI** – początek zjazdu ogona stały albo z nutą; START / END / PUNCH PITCH pokazują nutę,
  można wpisać nutę („G4”). Presety MODERN / RAW / EXPERIMENTAL: tok FIXED na czystej nucie, w słyszalnym paśmie (300 Hz – 6 kHz) najwyżej ~3 dB nad ogonem (głośniejszy przykrywał tail). **MODERN = czysty** (wg przykładów OPS FREE KICK: bas pod tokiem – SUB START 25 ms, jasny tail SAW bez ciężkiego przesteru; RANDOMIZE w MODERN nie bierze już cech RAW). **EARLY = tryb PUNCH** (krótkie uderzenie wpadające w kick, ogon od uderzenia – bez tonalnego „pup”).
- **Środek**: duży **waveform** – to prawdziwy wyrenderowany kick, każda część w swoim kolorze.
  **Złap go myszką i przeciągnij do FL** – dostaniesz gotowy plik WAV.
- **Cztery karty**: LEVEL, PITCH, LENGTH, DRIVE, MUTE, SOLO i pasek poziomu. **Kliknij kartę**, a na dole pojawią się jej szczegóły
  (źródło, ATTACK / DECAY / SUSTAIN / RELEASE, TONE, RESONANCE, MIX, GLIDE, PULSE W, PHASE).
- **CORE MIXER**: FADER, PAN, PHASE Ø (odwrócenie fazy) i miernik dla każdej części.
- Wyszarzone gałki w danej chwili nic nie robią: SUB PITCH przy włączonym trackingu (sub siedzi na nucie), PULSE W bez źródła PULSE,
  START / TIME / CURVE / AMOUNT w trybie STABLE, TONE puncha przy CLICK.
- Każdą wartość możesz kliknąć i wpisać ręcznie.

**PITCH TRACKING** włączony: kick projektujesz przy END PITCH, a grana nuta przesuwa go tak, że END = nuta (START przesuwa się razem z nim).
Wyłączony: START i END w hercach, niezależnie od nuty.
**AUTO LENGTH** włączony: LENGTH skraca / wydłuża BODY (trochę), TAIL i SUB (w pełni), a PUNCH zostaje krótki.
Wyłączony: każda część ma swoją długość, a LENGTH tylko ucina kick na końcu (z miękkim wyciszeniem).
**CHOKE**: nowy kick wycisza poprzedni w 6 ms (czysty dół). **POLY**: kicki nachodzą na siebie.

## Strona ENVELOPES (moduł 02)
Przycisk **ENVELOPES** u góry (obok **MAIN**). Po lewej wybierasz **część** (PUNCH / BODY / TAIL / SUB) i **widok**
(PITCH / AMPLITUDE / FILTER / DISTORTION / RESONANCE) – u góry edytora zawsze widać, co edytujesz.
- **Edytor**: do 64 punktów. Dwuklik w puste miejsce = nowy punkt, dwuklik na punkt = usuń, przeciągnij = przesuń
  (kolejność w czasie zostaje), SHIFT = zaznacz kilka, przeciągnij po pustym = zaznacz prostokątem, **ALT + przeciągnij** na odcinku = napięcie
  (TENSION), **prawy klik** = typ krzywej odcinka (LINEAR, EXP, LOG, EASE IN / OUT / IN-OUT, BEZIER z uchwytami, STEP), dokładna wartość, usuń.
  Kółko = zoom w poziomie, SHIFT + kółko = w pionie. Oś czasu w ms, pion w półtonach (PITCH) albo w % (reszta).
- **Rysowanie zawsze gra**: jeśli obwiednia była w trybie ADSR (albo pitch w trybie CLASSIC itp.), pierwsze rysowanie przełącza ją
  na DRAWN / CUSTOM – jedno UNDO cofa oba.
- **COPY / PASTE** (także między częściami i widokami), **RESET**, **FIT**, **SNAP** (przyciąganie do siatki), **SAVE / LOAD**
  pojedynczej obwiedni jako plik (`Dokumenty\4GE\Envelopes`, `.4genv`).
- Pod edytorem prawdziwe parametry tej obwiedni: tryb, LINK (INDEPENDENT / FOLLOW / LINKED – np. SUB może iść za ruchem TAIL),
  czasy, DELAY, CURVE, AMOUNT, TRIGGER (ONE SHOT / RETRIGGER / LEGATO), SYNC. Po lewej dla wszystkich: TIME SCALE, TIME OFFSET,
  **SYNC** (czasy zsynchronizowanych obwiedni idą za BPM z FL; bez BPM z hosta – LOCAL BPM).
- Narysowane obwiednie zapisują się w projekcie FL i w presetach użytkownika.

## Strona OSCILLATORS (moduł 03)
Przycisk **OSCILLATORS** u góry. **Cztery dodatkowe oscylatory** – każdy wpada DO wybranej części (PUNCH / BODY / TAIL / SUB) przed jej
filtrem i przesterem, więc oscylator wysłany do TAIL przechodzi przez CHARACTER, obwiednię i cięcia ogona. Domyślnie wyłączone (dźwięk się nie zmienia).
- **Sekcja oscylatora**: ON, MUTE (cichy, ale dalej moduluje FM / ring / sync), SOLO, RESET, PRESET (kategorie: CLEAN SINE, CLASSIC HARDSTYLE,
  AGGRESSIVE SAW, METALLIC, FM TONES, SCREAM SOURCES, EXPERIMENTAL, SUB SOURCES + zapis / odczyt), WAVEFORM, PITCH FROM (wysokość z PUNCH / BODY /
  TAIL / SUB albo stałe Hz), LEVEL, OCT, SEMI, FINE, PHASE, UNISON, wysyłki **> PUNCH / > BODY / > TAIL / > SUB** (każda z własnym poziomem), PAN.
- **Przebiegi**: SINE, TRIANGLE, SAW, SQUARE, PULSE (szerokość 1–99 %), NOISE (WHITE / PINK / BROWN / METALLIC / DIGITAL),
  **CUSTOM** (rysujesz jeden cykl albo IMPORT WAV – z pliku brany jest jeden okres, to źródło przebiegu, nie sampel kicka),
  **WAVETABLE** (BASIC, HARDSTYLE, FORMANT, METAL, PWM SWEEP albo USER = Twój WAV pocięty na klatki; POSITION, MORPH, skan STATIC / LINEAR /
  ENVELOPE / LFO / RANDOM z seedem), **HARMONIC** (64 harmoniczne do narysowania + AMOUNT, ODD, EVEN, ROLL-OFF, TILT, SPREAD, PHASE, RANDOM).
- **Zakładki** wybranego oscylatora: WAVEFORM, WAVETABLE, HARMONICS, UNISON (1–16 głosów, DETUNE, SPREAD, BLEND, tryby, fazy głosów),
  FM MATRIX (wiersz = modulator, kolumna = nośna, przekątna = FEEDBACK; RATIO, FM ENV, DECAY), RING MOD (mnożenie przez inny oscylator
  albo sinus), PHASE (RESET / RANDOM / FREE RUN / SYNCED do innego oscylatora, INVERT), HARD SYNC (HARD / SOFT, AMOUNT).
  Każdą zakładkę można zapisać osobno (SAVE / LOAD), cały oscylator (SAVE OSC) albo wszystkie cztery (SAVE ALL 4) – `Dokumenty\4GE\Oscillators`.
- **Podgląd po prawej**: prawdziwy przebieg i prawdziwe widmo (harmoniczne 1–64) wybranego oscylatora albo SUMY, przed / po FM, ringu i syncu.
- Makra oscylatorów (OSC MIX, HARMONICS, DETUNE…) przyjdą jako lokalne kontrolery z mapowaniem w module 09 (zasada 8 MASTER MACROS).

## Strona LAYERS (moduł 04)
Przycisk **LAYERS** u góry, w środku trzy widoki:
- **NOISE**: dwa generatory szumu (NOISE A / B). Źródła WHITE, PINK, BROWN, BLUE, VIOLET, METALLIC (rezonatory: FREQ, Q, AMOUNT, METAL),
  DIGITAL (BITS, SR REDUCE, QUANT, GRAIN) i CUSTOM (rysowane widmo). Do tego COLOR, filtr LP / HP / BP / NOTCH, RESO, DRIVE, MIX, WIDTH,
  obwiednia (własna ATTACK / DECAY / LENGTH, z części PUNCH / BODY / TAIL / SUB albo rysowana), PITCH TRACK, seed z trybem FIXED (ten sam szum
  przy każdym kicku) / PER TRIGGER / FREE RUN. Szum wysyłasz do części (> PUNCH / > BODY / > TAIL / > SUB – przed ich przesterem)
  i na magistralę TEXTURE. **MOD ENV** rusza poziomem, cutoffem, rezonansem, drive, kolorem, szerokością i grainem w czasie.
- **TRANSIENT**: dodatkowe stuknięcie (SINE CLICK, NOISE CLICK, IMPULSE, SHORT PULSE, METALLIC CLICK, OSC SNAP, CUSTOM rysowany) z poziomem,
  długością, wysokością, TONE, DRIVE, MIX, WIDTH i POSITION liczonym od początku kicka / puncha / body / taila (np. stuk w momencie wejścia
  taila po toku). **ATTACK LEVEL / TIME / DECAY + LOW / HIGH TRANSIENT** podbijają atak całego kicka (osobno dół i góra),
  **TRANSIENT SHAPER** (ATTACK / SUSTAIN / SENSITIVITY / TIME / MIX) działa na prawdziwym detektorze obwiedni, przed MASTER CLIP.
- **LAYERS**: 8 niezależnych warstw w liście (nazwa, ON, GRUPA, ŹRÓDŁO, LEVEL, PITCH, LENGTH, PAN, PHASE, filtr, DRIVE, M, S, ROUTING
  i mini-waveform warstwy). Źródło warstwy to jej własna kopia: SINE / TRI / SAW / SQUARE, NOISE A / B, TRANSIENT albo OSC 1–4.
  Pod listą: **LAYER** (obwiednia ADSR / ONE SHOT / MULTIPOINT rysowana, START, WIDTH, filtr, LINK do innej warstwy – poziom / pitch / długość /
  obwiednia osobno, PRE-FADER, wysyłki do 7 magistral w trybie MULTI), **GROUPS** (6 grup: ON, LEVEL, PAN, WIDTH, M, S, OUTPUT; magistrale
  TEXTURE / CUSTOM; SOLO PROCESSED / SOURCE), **TOOLS** (RANDOMIZER z trybami, INTENSITY, SEED, PRESERVE GROUPS / ROUTING – zablokowane
  warstwy LOCK zostają nietknięte; PHASE ALIGN – liczy korelację dwóch warstw dla każdej fazy, zmienia fazę dopiero po APPLY).
  COPY / PASTE / DUPLICATE / DELETE / MOVE UP / DOWN (tylko kolejność na liście) / RENAME / LOCK. Wszystko z jednym UNDO.
- PRESETS / SAVE / LOAD w każdym widoku (`Dokumenty\4GE\Layers`): szumy (WHITE IMPACT, DARK NOISE…), transienty (CLASSIC CLICK, HARD IMPACT…)
  i zestawy warstw (CLASSIC PUNCH STACK, MODERN BODY STACK, RAW TAIL TEXTURE…).

## Strona DISTORTION (moduł 05)
Przycisk **DISTORTION** u góry. 16 stopni przesteru dla całego kicka – każdy stawiasz w jednym miejscu (u góry):
**GLOBAL** (cały kick), **PUNCH / BODY / TAIL / SUB** (ta część, przed jej suwakiem), **TEXTURE** (magistrala TEXTURE) albo **L1–L8** (jedna warstwa).
- Po lewej **CHAIN**: stopnie tego miejsca w kolejności grania (góra → dół). ADD / DUPLICATE / COPY / PASTE / UP / DOWN / DELETE –
  kolejność zmienia dźwięk. Pod spodem PRESETS / SAVE / LOAD, crossovery multibandu, GLOBAL POSITION (globalny przester przed master clipem
  = sub czysty, albo po nim = z subem) i OFFLINE RENDER (eksport o stopień lepszy oversampling).
- **STAGE**: TYPE – 11 algorytmów, każdy z własną krzywą: SOFT CLIP, HARD CLIP, TUBE (osobno EVEN / ODD harmoniczne), DIODE, WAVEFOLD
  (prawdziwe zawijanie, STAGES), BITCRUSH (BITS, SR REDUCE, DITHER), RECTIFIER, ASYMMETRIC (osobny drive / próg dla dołu), FOLDBACK
  (odbicie od progu), DIGITAL CLIP (HARDNESS), CUSTOM (rysujesz własną krzywą w oknie TRANSFER CURVE: ciągnij punkty, dwuklik = dodaj / usuń).
  Widać tylko gałki, których dany typ naprawdę używa. MORPH + MORPH TO = płynne przejście między dwoma algorytmami (grają oba).
  PATH: SERIAL albo PARALLEL A / B / C (gałęzie z tego samego sygnału, sumowane). BAND: cały sygnał albo tylko LOW / MID / HIGH
  (multiband, pasma sumują się płasko). DRIVE + DRIVE CURVE, INPUT / OUTPUT, MIX, BIAS, AUTO GAIN, OVERSAMPLING 8x / 16x.
- **PRE FILTER** (co idzie w przester) i **POST FILTER** (co z niego wychodzi) – LP / HP / BP / NOTCH / LADDER, 6–48 dB, RESO, DRIVE.
- **DYNAMIC DISTORTION**: przester w czasie – źródło: własna obwiednia (ATTACK / DECAY), rysowana, obwiednia pitchu, poziom wejścia albo
  velocity → DRIVE, BIAS, THRESHOLD, FOLD, MIX, OUTPUT, POST CUTOFF.
- Na dole prawdziwe widoki (liczone w tle): **TRANSFER CURVE** (krzywa z tych samych tablic co dźwięk), **WAVEFORM** przed (linia) / po
  (wypełnienie) i **SPECTRUM** przed / po (FREEZE, zakres, kanał MID / L / R / SIDE, odsłuch jednego pasma multibandu).
- Presety (zastępują cały przester): EARLY – CLASSIC SATURATION, SOFT PUNCH, VINTAGE CLIP; MODERN – MODERN HARD CLIP, CLEAN AGGRESSION,
  DENSE DISTORTION; RAWSTYLE – RAW ASYMMETRIC, BRUTAL FOLD, HARD DIGITAL, CRUSHED TAIL; EXPERIMENTAL – METALLIC FOLD, FM CRUSH,
  DIGITAL CHAOS, COMPLEX HARMONICS; UTILITY – SUB CLEANUP, PUNCH CONTROL, HIGH TEXTURE, MASTER CLIP.
  SAVE zapisuje (`Dokumenty\4GE\Distortion`): jeden stopień, cały łańcuch, blok multiband, blok parallel albo pełny przester.
- Makra przesteru (GRIT, HARDNESS…) dojdą w module 09 jako przypisania do 8 głównych makr.

## Strona SCREECH (moduł 06)
Przycisk **SCREECH** u góry, trzy widoki:
- **FILTERS**: 8 filtrów (F1–F8). Każdy: TYPE – LOW / HIGH / BAND PASS, NOTCH, LADDER, DIODE LADDER, STATE VARIABLE, COMB (metaliczny
  grzebień), FORMANT (samogłoski), ALL PASS – każdy ma swój algorytm. LOCATION (cały kick, część, TEXTURE, warstwa albo screech),
  POSITION przed / po przesterze z modułu 05, PATH (szeregowo / równolegle) i SPLIT BAND (tylko dół / środek / góra). CUTOFF, RESO, DRIVE,
  MIX, SLOPE, KEY TRACK, VELOCITY, STEREO / MONO / MID / SIDE i własna obwiednia ADSR (→ cutoff, reso, drive; INVERT, SYNC LEN).
  Widać tylko to, co dany typ naprawdę obsługuje. Na dole prawdziwa odpowiedź filtra (na starcie kicka i później) i jego obwiednia.
- **RESONANCE**: 2 rezonatory – tryby CLEAN, METALLIC, SCREECH, RUBBER, FORMANT, CUSTOM; wzbudzenie sygnałem albo własnym SINE / SAW /
  SQUARE / NOISE / IMPULSE / oscylatorami screecha. FREQ, Q, DECAY, AMOUNT, DAMPING, EXCITE, FEEDBACK (do 95 %, zabezpieczony), PITCH TRACK,
  MIX i obwiednia wysokości rezonansu (np. 8 kHz → 1,8 kHz).
- **SCREECH**: screech z samej syntezy, bez sampli. 8 trybów, każdy inną drogą: TONAL, METALLIC, FORMANT, FM SCREECH, NOISE SCREECH,
  FEEDBACK, HYBRID, CUSTOM. 4 oscylatory (fala, poziom, pitch, fine, faza, detune, FM / RING z innego oscylatora, pan), 2 filtry screecha,
  formanty A-E-I-O-U z MORPH, pętla feedbacku z STABILITY. Obwiednie AMP / MOD / PITCH. TIMING: START MODE – MANUAL, BODY END
  (screech zaczyna się dokładnie z końcem BODY i przesuwa się razem z nim), BODY END + OFFSET, TAIL START, KICK START, HOST SYNC;
  LENGTH / SYNC TO TAIL / AUTO, FADE IN / OUT, PRE-TRIGGER (ciche rozkręcenie przed wejściem), QUANTIZE. OUTPUT: do TAIL / TEXTURE / MASTER…
- PRESETS (EARLY – CLASSIC RESONANCE, OLDSCHOOL SCREECH; MODERN – WIDE SCREECH, MELODIC TAIL; RAWSTYLE – AGGRESSIVE METAL, FEEDBACK TERROR;
  EXPERIMENTAL – FORMANT MACHINE, BROKEN RESONATOR), SAVE / LOAD, A / B, COPY / PASTE, RANDOMIZE z LOCK dla każdego silnika – wszystko z UNDO.
- Makra (SCREAM, RESONANCE, METALLIC…) i wspólna matryca modulacji dojdą w module 09.

## Strona EQ (moduł 07)
Przycisk **EQ** u góry, pięć zakładek. Każdy procesor stawiasz w jednym miejscu (cały kick, PUNCH / BODY / TAIL / SUB, TEXTURE, warstwa
albo screech), przed albo po przesterze z modułu 05.
- **PARAMETRIC**: 16 pasm (1–16 u góry). Typy: BELL, LOW / HIGH SHELF, LOW / HIGH CUT (6–48 dB), BAND PASS, NOTCH, TILT, RESONANT PEAK,
  tryb STEREO / LEFT / RIGHT / MID / SIDE, MIX. Na wykresie prawdziwa krzywa miejsca (czerwona) + analizator PRE (szary) / POST
  (kolor) z PEAK HOLD, uśrednianiem, rozdzielczością FFT, skalą LOG / LIN i częstotliwością dominującą. Punkty ciągniesz myszką
  (częstotliwość / gain), kółko = Q, dwuklik = nowe pasmo, prawy klik = usuń. COPY / PASTE / DUPLICATE / DELETE.
- **DYNAMIC**: 4 pasma dynamiczne – bell, którego gain idzie za prawdziwym detektorem (PEAK / RMS z oknem): DYNAMIC CUT / BOOST
  (powyżej progu), UPWARD / DOWNWARD (poniżej progu); detektor z tego pasma, całego sygnału, dołu albo góry. THRESHOLD, RANGE (0–24 dB),
  RATIO, KNEE, ATTACK, RELEASE, MIX.
- **MULTIBAND**: 4 pasma SUB / LOW / MID / HIGH (zwrotnice Linkwitz-Riley 12 / 24 / 48 – suma płaska, albo Butterworth / Bessel),
  w każdym GAIN, WIDTH, INV, M, S i dynamika: COMPRESSOR / EXPANDER / GATE / LIMITER / TRANSIENT (atak / sustain osobno).
  LISTEN = przetworzone albo pasma przed obróbką. Na dole poziomy pasm.
- **SPECTRAL**: TILT (ciemniej / jaśniej wokół PIVOT), HARMONIC SHAPER (nowe parzyste / nieparzyste harmoniczne z dołu i z góry),
  EXCITER (6 krzywych, nowe harmoniczne tylko w swoim zakresie), SPECTRAL COMPRESSION (24 pasma – ścina tylko to, co wystaje),
  SPECTRAL FREEZE (łapie widmo w chwili CAPTURE AT i trzyma tę teksturę). ORDER = kolejność EQ / DYN / MB / SPECTRAL w jednym miejscu.
- **STEREO**: WIDTH ogólny i dla dołu / środka / góry, MONO BELOW (poniżej zawsze mono – WIDTH nigdy nie poszerza subu), gainy
  mid / side w 3 pasmach, MONO LISTEN, korelacja (ogólna i w pasmach).
- PRESETS (EARLY – CLASSIC WARMTH, OLDSCHOOL PUNCH; MODERN – MODERN BALANCE, BRIGHT FESTIVAL; RAWSTYLE – RAW AGGRESSION, HEAVY SUB CONTROL;
  EXPERIMENTAL – SPECTRAL CHAOS, METALLIC FREQUENCY), SAVE / LOAD (pasmo .4geq albo cały moduł .4gm07), A / B, RANDOMIZE z LOCK – z UNDO.
- Bez latencji (EQ minimum phase, kompresja widmowa na pasmach zamiast FFT). Linear phase i makra modułu – później (makra w module 09).

## Strona DYN (moduł 08)
Przycisk **DYN** u góry, sześć zakładek. Każdy procesor stawiasz w jednym miejscu (cały kick, PUNCH / BODY / TAIL / SUB, TEXTURE, warstwa,
screech albo **MASTER OUTPUT** = na samym końcu), przed albo po przesterze z modułu 05.
- **COMPRESSOR** (4): typy VCA, FET (szybki, z kolorem), OPTO (gładki), VARI-MU (łagodny – ratio rośnie z poziomem), DIGITAL, UPWARD
  (podnosi ciche), PARALLEL (dry + skompresowany bez skoku głośności), SIDECHAIN (ducking od innej części). DETECTION PEAK / RMS / HYBRID /
  TRUE PEAK, THRESHOLD, RATIO, ATTACK, RELEASE (albo AUTO RELEASE), KNEE, INPUT / OUTPUT, MIX, DRY / WET (tylko PARALLEL), STEREO LINK,
  SIDECHAIN SOURCE (np. PUNCH ścisza TAIL, gdy gra punch) z filtrem HPF / LPF, LOOKAHEAD (tylko na MASTER OUTPUT – dodaje latencję, FL ją zna).
- **MULTIBAND**: dynamika 4 pasm z modułu 07 + **BAND LINK** (połączone pasma mają wspólny detektor – barwa się nie przesuwa).
- **TRANSIENT** (5, domyślnie po jednym na część): ATTACK + / − (więcej / mniej uderzenia), SUSTAIN + / − (więcej / mniej tego, co po nim),
  tryby PUNCH / SUSTAIN / BODY / TAIL, SENSITIVITY, TIME, RELEASE.
- **CLIPPER** (2): SOFT, HARD, ASYMMETRIC, SINE, CUBIC, TANH i **CUSTOM CURVE** (rysujesz krzywą po prawej), CEILING (nigdy nie przekroczony),
  DRIVE, SYMMETRY, SOFTNESS, OVERSAMPLING (mniej „piasku” z twardego clipa).
- **LIMITER** (2, pierwszy domyślnie na MASTER OUTPUT): TRANSPARENT / AGGRESSIVE / PUNCH / LOUD / TRUE PEAK, CEILING, LOOKAHEAD, RELEASE,
  ATTACK, KNEE, STEREO LINK, TRUE PEAK (też szczyty między próbkami).
- **CREST & LEVEL**: kontrola crest factor (szczyt / RMS: nisko = gęsto i głośno, wysoko = kolczasto i punchowo) z TARGET i osobno
  PUNCH / BODY / TAIL, analizator (szczyt, RMS, crest – z dynamiką i bez). AUTO GAIN (PEAK / RMS / LOUDNESS – procesor nie zmienia
  głośności, słyszysz kształt), LEVEL MATCH (słyszysz kick BEZ obróbki na tym samym poziomie – uczciwe A / B), ORDER (kolejność w jednym miejscu).
- Przy każdym procesorze: prawdziwa krzywa (wejście → wyjście), historia redukcji z renderu kicka (pierwsze 600 ms), mierniki na żywo
  IN / OUT / GR (+ MAX / AVG, RESET MAX) i ostrzeżenia (clip, za mocna kompresja, limiter pracuje za ciężko, skok głośności, za gęsto).
- PRESETS (EARLY – CLASSIC PUNCH, SOFT DYNAMICS; MODERN – TIGHT IMPACT, FESTIVAL LOUD; RAWSTYLE – EXTREME CRUSH, HEAVY PUNCH;
  EXPERIMENTAL – BROKEN DYNAMICS, MAXIMUM DENSITY – cały łańcuch razem z miejscami), SAVE / LOAD (procesor .4gdyn albo moduł .4gm08),
  A / B, COPY / PASTE / RESET – z UNDO. Makra modułu (PUNCH / LOUDNESS / GLUE) dojdą w module 09.

## MASTER MACROS i strona MOD (moduł 09)
- **8 MASTER MACROS** na głównym ekranie (pod waveformem): IMPACT, BODY, TAIL, PITCH, GRIT, SCREAM, WEIGHT, CHAOS. Makro niczego
  nie nadpisuje – dodaje swój ruch na wierzchu gałek (FINAL = BAZA + modulacja); przy 0 % kick brzmi dokładnie tak samo.
  Domyślne przypisania (MACRO MAPPING): IMPACT → poziom / drive punchu, transient · BODY → poziom / decay / drive body ·
  TAIL → długość / decay / release taila · PITCH → start / czas / krzywa pitchu · GRIT → drive taila, stage 2, drive body,
  distortion 1 · SCREAM → rezonans taila, poziom screecha, mid bell · WEIGHT → poziom / drive / długość suba ·
  CHAOS → dryf / rozstrojenie unisonu, pulse width.
- Przycisk **MOD** (prawy górny róg) – **MODULATION MATRIX**: 64 połączenia (widok po 16), w każdym SOURCE, DESTINATION (dowolny
  modulowalny parametr 4GE – menu wg modułów), DEPTH, OFFSET, POLARITY, CURVE, MODE (ADD / MULTIPLY / MAX / MIN / REPLACE),
  SMOOTH, LAG, MUTE, SOLO, LABEL i pasek BAZA | FINAL na żywo. **SHOW: <makro>** = jego MACRO MAPPING (dodawanie / zmiana celów),
  CLEAR ALL, DEFAULT MACROS. Wszystko z UNDO, zapisuje się w projekcie i presecie.
- Źródła: makra (8 + 4 USER), **LFO 1–8** (8 kształtów, RATE albo HOST SYNC z podziałem, PHASE, RETRIGGER OFF / ON NOTE /
  ON KICK TRIGGER / ON BAR, LOOP / ONE SHOT, FADE IN / OUT, SMOOTH, PULSE WIDTH), **STEP SEQUENCER** (do 64 kroków rysowanych myszką:
  wartość, PROBABILITY, RATCHET, CURVE, TIE, ACCENT, MUTE; kierunki, SWING, HUMANIZE, SEED, COPY / PASTE / REVERSE / SHIFT / RANDOM),
  **RANDOM 1–2** (FIXED SEED, PER TRIGGER, FREE RUN, ONE SHOT, CONTROLLED; ten sam SEED = ten sam wynik), VELOCITY, KEY, PITCH ENV,
  obwiednie głośności PUNCH / BODY / TAIL / SUB / KICK, FILTER ENV. **MOVEMENT** skaluje naraz wszystkie LFO / sekwencer / random.
- **Modulacja z każdej gałki:** prawy klik na gałce → MODULATE … WITH (źródło) albo REMOVE; albo przeciągnij źródło (strzałka „>” przy
  makrze na MAIN albo wiersz w SOURCE MONITOR) na gałkę. Modulowana gałka ma pierścień: jak daleko sięga i gdzie jest teraz.
- **CUSTOM LFO**: kształt WAVE = CUSTOM, rysujesz do 16 punktów (klik = dodaj, przeciągnij, prawy klik = usuń), LINEAR albo CUBIC.
- **MORPH / MIDI / FOLLOW**: MACRO MORPH (STORE A, STORE B, gałka MORPH = płynne przejście wszystkich makr), 4 źródła MIDI CC z LEARN,
  MIDI LEARN makra (kręcisz gałką kontrolera – makro w 4GE idzie za nią, też w automatyce), AUDIO FOLLOWER (głośność KICK / PUNCH / BODY /
  TAIL / SUB / TEXTURE jako źródło: ATTACK, RELEASE, GAIN, THRESHOLD, RANGE, GATE, SMOOTH).
- **VARIANT RANDOMIZER** (też przycisk RANDOMIZE na górze): STYLE EARLY (czysty, tonalny ogon, mało przesteru) / MODERN / RAW (SCREAM /
  METAL / BUZZ / CRUSH, dużo drive) / EXPERIMENTAL, SCOPE (cały kick albo jedna część), AMOUNT (100 % = nowy kick, mniej = wariacja),
  SEED (ten sam = ten sam wynik), LOCK części, ROUTING (kilka połączeń LFO / SEQ / RANDOM). Głośność wyrównana po każdym losowaniu,
  REGENERATE = ten sam start i seed z nowymi ustawieniami. Jedno kliknięcie = jeden UNDO.
- **DESTINATION MONITOR** (klik w numer wiersza): BAZA / MODULACJA / FINAL wybranego celu w jego jednostkach.
- **SOURCE MONITOR** pokazuje każde źródło na żywo. Bezpieczeństwo: MASTER i sufit limitera modulacja może tylko obniżyć, LOOKAHEAD
  nie jest celem (latencja), wartości zawsze w zakresie. Eksport = to samo co w FL (ten sam seed, LFO na kicku).

## Strona SMP – sample (moduł 10)
- Przycisk **SMP**. **8 slotów** – każdy to jeszcze jedna warstwa kicka (warstwy 9–16): ma GROUP (PUNCH / BODY / TAIL / SUB /
  TEXTURE …), więc działa na nim poziom / mute / solo grupy, moduły 05–08 tej części i cała modulacja (MOD). Łączy się z syntezą.
- **Ładowanie:** LOAD, przeciągnięcie pliku na waveform albo przeglądarka. WAV / AIFF / FLAC (też OGG / MP3), mono i stereo, dowolne
  sample rate (przeliczane wysokiej jakości filtrem), do 60 s. Do 20 s plik zapisuje się **w projekcie / presecie** – otworzysz go
  na innym komputerze. Dłuższe: tylko ścieżka; brak pliku = slot **MISSING** (nic się nie wywala), **RELINK** = wskaż folder, slot sam
  znajdzie plik po nazwie. **COLLECT** kopiuje użyte pliki do wybranego folderu (tylko gdy klikniesz).
- **Edytor waveformu:** znaczniki **S / E** (START / END) i **L1 / L2** (pętla w LOOP / PING-PONG) – przeciągasz; z **SHIFT** przyklejają
  się do transientów (kreski na dole). Kółko = zoom, przeciąganie tła = przewijanie, podwójny klik / FIT = cały sample. Biała kreska =
  gdzie sample gra teraz. **PREVIEW REGION** = posłuchaj START..END bez kicka. Ruchy znaczników mają UNDO.
- **Odtwarzanie:** MODE ONE SHOT / GATE (gra póki trzymasz nutę) / LOOP (z crossfade – bez dziury na szwie) / PING-PONG, REVERSE
  (plik się nie zmienia), RATE (szybkość), PITCH / FINE, ROOT + KEY TRACK (sample idzie za nutą MIDI), FADE IN / OUT (start w środku
  dźwięku zawsze dostaje min. 1 ms fade – bez klika), OFFSET, PHASE, a do tego filtr, drive, width, obwiednia, routing i LINK jak
  w warstwach. Podmiana sampla w trakcie grania = 3 ms crossfade.
- **Narzędzia** (na kopii roboczej, oryginał na dysku nietknięty, z UNDO): NORMALIZE, DC (usuń DC offset), TRIM (utnij ciszę).
- **Przeglądarka:** FOLDER (3 poziomy podfolderów), szukanie po nazwie, FAVORITES, RECENT, PREVIEW (odsłuch bez ładowania),
  LOAD TO SLOT / podwójny klik. Ulubione i ostatnie trzymane w ustawieniach 4GE, nie w presecie.
- Ładowanie idzie w tle – audio nigdy nie czeka. Ten sam preset = ten sam render (FL = eksport).

## Strona STEREO (moduł 11)
- Przycisk **STEREO** (górny rząd). **7 jednostek stereo**: grupy PUNCH / BODY / TAIL / SUB / TEXTURE / CUSTOM (każda po swoich
  filtrach / EQ / przesterze / dynamice) i **MASTER** (cały kick, przed limiterem wyjścia). Każda ma ON (OFF = dźwięk nietknięty).
- **PAN** przesuwa środek dźwięku (stała moc), **BALANCE** tylko ścisza przeciwny kanał – to dwie różne rzeczy. **WIDTH** 0–200 %
  (0 % = mono, 100 % = jak jest) z algorytmem: **M/S WIDTH** (przewidywalny), **DECORRELATION** (szerokość z góry pasma, bezpieczna
  w mono), **HAAS** (jeden kanał później – efekt kreatywny, strona ostrzega: sprawdź mono), DISABLED. Szerokość nigdy nie podbija
  głośności (wyrównanie energii). **MIX**, **OUTPUT**.
- **CHANNEL SWAP** (L ↔ R), **POLARITY** (lewy / prawy / oba; pojedyncza warstwa ma swój INVERT na LAYERS / SMP), **CH DELAY**
  (±10 ms: minus = lewy później, plus = prawy).
- **LOW MONO** + CROSSOVER 20–300 Hz: wszystko poniżej idzie na środek (środek kicka nietknięty – punch zostaje w kształcie).
- **M/S ENGINE**: MID / SIDE level, M/S WIDTH, MID SOLO / SIDE SOLO.
- **SUB MONO LOCK** (domyślnie ON): grupa SUB zawsze mono i na środku, a MASTER trzyma doły (min. 120 Hz) w mono – silniejsze niż WIDTH.
- **MONO AUDITION**: słuchasz w mono, preset się nie zmienia (nie zapisuje się).
- Po prawej: **VECTOR SCOPE**, **CORRELATION** (-1…+1; ujemna = problem fazy, nic się samo nie zmienia), mierniki MID / SIDE / L / R,
  STEREO FIELD. Pod gałkami tabela **ALL UNITS** (klik = otwórz). Wszystko modulowalne z MOD (pan, width, mid, side, crossover, delay…),
  bez klików.

## ANALIZA kicka (moduł 16)
- Przycisk **ANALYZE** w nagłówku (obok STEREO). **LOAD REFERENCE** albo przeciągnij WAV / AIFF / FLAC / MP3 na stronę.
  Referencja jest analizowana w tle, a 4GE jest renderowane tak jak przy eksporcie (cały kick z masterem) **na tej samej nucie**
  co referencja (pole NOTE możesz zmienić). ANALYZE 4GE = przeanalizuj jeszcze raz aktualne brzmienie.
- Widać: **waveform**, **krzywą pitchu** (z nutą ogona, np. „G1 +40 ct”), **spektrum**, **energię w 6 pasmach**
  (SUB / LOW / BODY / MID / PRESENCE / AIR), tabelę **REFERENCJA | 4GE | RÓŻNICA**: długość, nuta i pitch ogona, start pitchu,
  czas spadku, trzymany TOK, attack, transient, % suba, peak, RMS, LUFS, crest (gęstość), korelacja, korelacja basu, side.
  CHARACTER = cechy słowami (TONAL TOK, SUB-HEAVY, BRIGHT / SCREECHY, STEREO, DC OFFSET, CLIPPED …).
- **SUGESTIE**: co zmienić w 4GE, żeby było bliżej referencji, z dokładnymi wartościami (pitch START / END / TIME, offset toku,
  LENGTH, PUNCH / SUB / BODY LEVEL, TAIL DRIVE / TONE / HIGH CUT, MASTER CLIP, szerokość, głośność do uczciwego porównania).
  **Nic się nie zmienia samo** – zaznacz i **APPLY SELECTED** / **APPLY ALL** = jeden krok UNDO (Ctrl+Z cofa wszystko naraz),
  potem 4GE jest analizowane od nowa. Ścieżka referencji zapisuje się w projekcie.

## PRESETY – przeglądarka (moduł 15)
- **Klik na nazwę presetu** otwiera przeglądarkę (drugi klik / CLOSE wraca). Filtry: szukaj (nazwa, tagi, notatki), STYLE,
  LENGTH, ALL / FACTORY / USER / FAVORITES / RECENT, TAG (charakter: CLEAN / DIRTY / HEAVY, PUNCHY, SCREECH, LAYERS, SAMPLES,
  STEREO, MODULATED, SYNCED, NOTE CUT, TAIL …, Twoje tagi). Gwiazdka (+ / *) na końcu wiersza = ulubiony.
- Po prawej: z czego zrobiony jest wybrany preset (styl, długość, rdzeń, przester, warstwy, sample, screech, stereo, modulacja)
  i **brakujące sample** (na czerwono + RELINK prowadzi na stronę SMP). Pole na **Twoje tagi i notatki** (zapisywane w pliku
  presetu użytkownika).
- LOAD (dwuklik), **PREVIEW** (posłuchaj bez wczytywania), SAVE, SAVE AS, **DUPLICATE** (preset fabryczny staje się Twoim do
  edycji), RENAME, DELETE (do kosza), IMPORT (pliki .4gp – nic nie jest nadpisywane), EXPORT.
- **A / B**: dwa pełne brzmienia do porównania (puste B zaczyna jako kopia A), COPY A > B.

## LENGTH (moduł 14)
- W nagłówku pod STYLE: przycisk **LENGTH: MEDIUM 450 ms** – otwiera panel długości:
  - **SHORT 180 / MEDIUM 400 / LONG 750 / X-LONG 1600** ms (zakresy ~100–250 / 250–500 / 500–1000 / 1000–2500) – ustawia LENGTH
    i włącza AUTO LENGTH, części kicka skalują się z nim (jedno UNDO);
  - **LENGTH SYNC**: OFF / 1/4 / 1/2 / 1 BEAT / 2 BEATS – długość z tempa projektu (1 BEAT przy 150 BPM = 400 ms); wtedy gałka
    LENGTH jest wyszarzona;
  - **FOLLOW** dla PUNCH / BODY / TAIL / SUB – ile dana część rośnie / maleje z LENGTH (domyślnie punch 0 % – zostaje krótki,
    body 50 %, tail i sub 100 %) + własna długość każdej części.
- Styl i długość są niezależne: EARLY X-LONG albo MODERN SHORT – bez problemu. Eksport AUTO czeka na ogony efektów.

## KICK END – długość z piano rolla
- W nagłówku pod PITCH TRACKING: **LENGTH / NOTE / +FX**.
  - **LENGTH** – kick gra całą swoją długość (LENGTH), długość nuty nie ma znaczenia (jak dotąd).
  - **NOTE** – kick kończy się tam, gdzie kończy się kostka w piano rollu (5 ms wyciszenia, bez klika); efekty (rezonatory,
    opóźnienia stereo, release kompresora, przester na masterze) wybrzmiewają dalej.
  - **+FX** – jak NOTE, ale na końcu nuty ucinane są też ogony efektów.

## STYLE (moduł 13)
- W nagłówku pod nazwą: **STYLE** – EARLY / MODERN / RAW / EXPERIMENTAL. Wybór ustawia **punkt startowy charakteru**: generatory
  PUNCH / BODY / TAIL / SUB, pitch drop, szum / transient, screech, master clip i Twój łańcuch przesteru na TAIL:
  EARLY SOFT CLIP → TUBE, MODERN HARD → SOFT → DIGITAL CLIP, RAW ASYMMETRIC → HARD → WAVEFOLD, EXPERIMENTAL WAVEFOLD → RECTIFIER
  (zamiast ring moda, dopóki moduł 05 nie ma stopnia RING MOD) → CUSTOM.
- Styl **nigdy nie zmienia DŁUGOŚCI** (LENGTH, długości / decay / release części) ani warstw, sampli, EQ, dynamiki, modulacji,
  stereo, makr i mastera – i niczego nie blokuje.
- **Twoje zmiany zostają**: gałka, którą sam ruszyłeś (albo automatyka z FL), nie jest nadpisywana przy zmianie stylu
  (komunikat pod stylem: „…, 2 of your changes kept”). Prawy klik na STYLE: APPLY ALL (nadpisz też moje), PREVIEW (posłuchaj bez
  zmiany), FORGET MY CHANGES. Jedna zmiana stylu = jedno UNDO. Wczytanie presetu zaczyna od czystej karty.
- RANDOMIZE po wyborze stylu losuje w tym samym stylu; nazwy eksportu {STYLE} biorą styl z wyboru.

## Strona EXPORT (moduł 12)
- Przycisk **EXPORT** (górny rząd, po prawej). **MASTER OUTPUT**: poziom MASTER, MUTE (bez klika), BYPASS (pomija master clip /
  master stereo / dynamikę wyjścia / safety – do porównania), STEREO / MONO, **SAFETY CLIP** i **SAFETY LIMITER** – domyślnie
  WYŁĄCZONE, a gdy działają, strona to wyraźnie pokazuje. Wtyczka sama niczego nie ścisza ani nie limituje.
- **MASTER METERS**: peak L / R z hold i CLIP, MAX PEAK, **TRUE PEAK** (osobno od sample peak), RMS, **LUFS** M / S / I, DC OFFSET,
  RESET.
- **Eksport**: SOURCE = cały kick, grupa (PUNCH / BODY / TAIL / SUB / TEXTURE / CUSTOM – z całą obróbką grupy i mastera) albo jedna
  warstwa / sample. LENGTH: AUTO (min. LENGTH kicka, potem aż ogon ucichnie), FIXED, UNTIL SILENCE + PRE-ROLL, POST-ROLL, próg ciszy
  (z histerezą), MIN SILENCE, MAX LENGTH, TRIM. Format WAV / AIFF / FLAC, 16 / 24-bit lub 32-bit float, 44.1–192 kHz albo HOST RATE
  (kick liczony od razu w tej częstotliwości), DITHER (OFF / TPDF / NOISE SHAPING – tylko 16 / 24-bit).
- **NORMALIZE**: OFF / PEAK / LUFS – zwykłe wzmocnienie; jeśli cel wymagałby przekroczenia TRUE PEAK CEILING, gain staje na suficie
  i dostajesz komunikat, że potrzebny byłby limiter (nic nie jest dodawane po cichu). Plik 16 / 24-bit powyżej 0 dBFS = ostrzeżenie.
- **SEED**: PRESET SEED (dokładnie to, co gra wtyczka), FIXED (0), NEW (nowa wariacja szumu / random / sekwencera przy każdym
  eksporcie), CUSTOM. NEW PRESET SEED daje presetowi nowy seed – słychać od razu, UNDO cofa.
- **Nazwy plików** z szablonu: {PRESET} {STYLE} {NOTE} {BPM} {GROUP} {VARIANT} {VERSION} {SEED} {DATE}, F# → Fs. Bez OVERWRITE nic
  nie jest nadpisywane (nowy numer wersji). **BATCH EXPORT**: wybrane źródła × obecny dźwięk lub wszystkie presety fabryczne ×
  warianty seedów → `4GE_EXPORT / STYLE / KICK_nn / KICK_<GRUPA>.wav`.
- **PREVIEW** = render + pomiar + odsłuch, nic nie zapisuje (pokazuje przewidywany gain). **EXPORT** pisze najpierw plik tymczasowy –
  CANCEL / błąd / brak miejsca nigdy nie zostawia uszkodzonego pliku. Render to migawka ustawień: kręcenie gałkami w trakcie nie
  zmienia rozpoczętego eksportu. Wynik (ścieżka, format, długość, peak, true peak, LUFS) – **przeciągnij go do FL**.
- **EXPORT HISTORY**: ostatnie 50 eksportów, RE-EXPORT z tymi samymi ustawieniami i seedem.

## Presety
**6 presetów na każdy STYLE** (v0.6) – każdy ma inną długość / charakter, wszystkie z limiterem na wyjściu (LIMITER 1 na MASTER OUTPUT,
PUNCH, −0,3 dB), więc grają głośno i równo jak gotowe kicki z paczek. Strojone pomiarami do Twoich referencji.

| STYLE | Presety (długość) | Wzór |
|---|---|---|
| **EARLY** | Classic Boom (450 ms), Dark Tonal (400), Punchy Short (220), Melodic Long (800), Warm Tube (500), Anthem XL (1100) | kick_hardstyle_28 / 84: tonalny, wyraźny pitch drop, cieplejszy przester |
| **MODERN** | Festival Main (675), Clean Punch (450), Tok Bounce (520), Euphoric Long (950), Tight Short (240), Wide Anthem XL (1400) | OPS KICK2_*: tok ~365 Hz, czysty sub, gęsty, kontrolowany |
| **RAW** | Screamer Main (815), Rawstyle Punch (500), Square Scream (650), Gated Short (240), Hardcore Crush (450), Rumble XL (1500) | OPS KICK_*: tok ~900 Hz, screech, szeroki ogon (dół mono) |
| **EXPERIMENTAL** | Metal Fold, Double Drop, Laser Zap, Glitch Crush, Formant Scream, Pulse Machine | działający kick + jeden nietypowy element: metaliczny fałd, podwójny drop, „laser” z 2 kHz, bitcrush, formanty, PULSE |

Gęstość ogona = ruch unisonu (DRIFT / DETUNE), szerokość = WIDTH ogona (mono przy sumowaniu). **RAW - Square Scream** jest w duchu
Twoich RANDOM Picków (klasyczny drop, SCREAM, szeroki).
Dalej: **MATCH - Festival Drive / Buzz Saw / Short Tonal** (pierwsze dopasowania z v0.5) i **RANDOM - Pick 01…11** (Twoje wybrane wyniki z RANDOM).

## Co dalej
Wszystkie 17 modułów są gotowe – teraz szlif brzmienia i presetów według Twoich uwag z odsłuchu.
