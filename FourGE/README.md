# 4GE v0.4

Syntezator kicków hardstyle (instrument VST3, FL Studio), **następca Forge of Wrath** (Forge jest zamrożony na v2.1).
Silnik to Forge plus nowe moduły, zbudowane według analizy architektury kicka (`Shared/RESEARCH.md`, „Architektura hardstyle kicka”):
osobne sekcje **PUNCH / TAIL / TEXTURE / SUB / CLICK**, a na końcu **BUS** (klej) i clipper.

**v0.2: reverse bass wyprowadzony do osobnej wtyczki RE4GE** (`Re4GE/`). 4GE to teraz czysty kick synth: cztery zakładki, bez presetów BASS.

Presety `.fow` z Forge się tu nie wczytują (świadomie, bo 4GE to nowa wtyczka). **Twoje kicki są przeniesione jako presety fabryczne**
(OWNER - QL / QL 1 / QL 2 / Nice / Nice 1 / Nice 2 / Sgsdg, Volcanic, Undertow) i brzmią jak w Forge.
Swoje presety zapisujesz w `Dokumenty\4GE\Presets` (przycisk **Folder**).

## Instalacja (Windows)
GitHub → zakładka **Actions** → *Build 4GE VST3* → **Artifacts** → `4GE-VST3-Windows`.
Folder `4GE.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, a w FL: *Options → Manage plugins → Find installed plugins*.
Wstaw jako **instrument** (Channel Rack).

## v0.4: PEŁNIEJSZY DŹWIĘK (UNISON / WIDTH)
Po porównaniu z Twoimi samplami (kick_hardstyle_28 / 84) ogon kicka może grać **kilkoma lekko rozstrojonymi, „pływającymi” kopiami**
po lewej i prawej. Dół poniżej **FROM** (domyślnie 350 Hz) zostaje mono i stabilny, z czystego środka.
- **GLUE + WIDTH** na głównym ekranie: **UNISON** (OFF / 2–7), **DETUNE**, **WIDTH**.
- W ADVANCED (Bus / FX): **UNISON / WIDTH**: Voices, Detune, **Drift** (powolne pływanie), Mix, Width, From.
- Nowe presety **FULL - QL / QL 1 / QL 2 / Nice / Nice 2 / Sgsdg / Volcanic / Undertow**. To Twoje kicki z unisonem, CLEAN SUB na 70 Hz
  (150–400 Hz zostaje „mięsiste”, nie zamienia się w sinus) i lżejszym clipem: kick oddycha zamiast być kwadratem.
  Oryginały OWNER zostały bez zmian. INIT startuje z UNISON 5.
- UNISON trzy razy liczy kicka (środek + lewo + prawo), więc zużywa więcej CPU.

## Ekran
**v0.3: EKRAN GŁÓWNY bez zakładek** (bo było ich za dużo i robiło się nieczytelnie). Najważniejsze gałki są na jednym ekranie:
- **PUNCH**: typ, On, Into dist, Level, Start, End, Amount, Length, Drive;
- **TAIL**: Level, Start, Drop, Pitch, Fine, Curve / Drawn;
- **DISTORTION**: typ, Drive, Tone, Bias, Stage 2;
- **CHARACTER**: Scream, Scream Hz, Crunch, Grit, Chaos;
- **LOW END**: Clean sub, Sub level, Body, Low cut;
- **GLUE**: typ BUS, Bus, OTT, Room, High cut.

Przycisk **Advanced >** otwiera wszystkie gałki w zakładkach (opisane niżej), a **< Main** wraca.

Na górze presety (< lista > Random Save Folder Play Undo Redo), pod spodem **THE ANVIL** (podgląd + rysowane obwiednie AMP / DRIVE / PITCH)
i **MASTER** (Length, Clip, Soft, Velocity, Output, przełącznik **AUTO GAIN**). Niżej cztery zakładki i pasek **SOLO / MUTE**.

### Pasek SOLO / MUTE
Każda warstwa: PUNCH, TAIL, TEXTURE, SUB, CLICK, SAMPLE. **M** wycisza, a **S** zostawia tylko zaznaczone warstwy (można kilka).
Nie zapisuje się w presecie, służy tylko do odsłuchu.

### 1. PUNCH
- **PUNCH**: typ źródła **SINE / TRI / SAW / NOISE / RESO**:
  - SINE: czysty tok;
  - SAW: twardy;
  - NOISE: szumowy tok wokół wysokości;
  - RESO: dzwoniący szczyt, który jedzie razem z pitchem.

  Gałki:
  - **LEVEL**;
  - **START → END** (Hz, nie zależy od nuty);
  - **AMOUNT**: ile z ruchu START → END się dzieje. 0 % = płasko na END, 100 % = cały ruch;
  - **DROP**;
  - **ATTACK**: miękkie wejście;
  - **DECAY**;
  - **LENGTH**: TOK, czyli szpic → półka → cięcie;
  - **PHASE**: 90° = start od szczytu, mocniejsze uderzenie.

  **INTO DIST** jak w Forge: punch przez przester ogona.
- **FIXED HEAD**: początek kicka zawsze na tej samej wysokości (jak w kickach OPS).
- **CLICK**: krótki szum na start.
- **PUNCH FILTER** (OFF / LP / HP / BP / NOTCH, CUTOFF, RESO): filtr **przed** drive'em puncha. HP = tok bez dołu, a dół zostaje dla taila i subu.
- **PUNCH DRIVE**: typ (SOFT … CRUSH) + DRIVE (gdy INTO DIST jest wyłączony).
- **TOK > TAIL**:
  - **DUCK**: tail ustępuje, kiedy gra punch, więc tok ma miejsce;
  - **TAIL ATT**: miękkie wejście taila.

### 2. TAIL
- **TAIL PITCH**: LEVEL, **START x** (tryb RATIO: start = START × nuta) albo **START Hz** (tryb HZ: zawsze ten sam start, więc niska nuta ma długi zjazd),
  DROP, HOLD, CURVE, PITCH, FINE, tryb CURVE / DRAWN (rysowany pitch).
- **OSC A**: WAVE (sinus → trójkąt → piła), LEVEL, PHASE.
- **OSC B**: drugi oscylator na tej samej wysokości (SAW / PULSE / TRI), osobny **LEVEL** (OFF = wyłączony), WIDTH dla PULSE.
  Cicha piła pod głośnym sinusem daje grubszy ogon bez dokręcania drive'u.
- **PRE FILTER** przed STAGE 1 (OFF / LP / HP / BP / NOTCH, CUTOFF, RESO), do tego **ENV** (sweep: na starcie filtr otwarty wyżej (+) albo niżej (−)
  i wraca w czasie **DECAY**). Decyduje, w co wgryza się przester.
- **STAGE 1**: typ (SOFT, TUBE, HARD, FOLD, RAW, **CRUSH** = cyfrowe schodki), DRIVE, TONE, **BIAS** (asymetria, czyli parzyste harmoniczne, rawowe chrupanie),
  **MIX** (poniżej 100 % zostaje trochę czystego ogona).
- **MID EQ** między stopniami, bez zmian.
- **STAGE 2**: typ, DRIVE, **BIAS**, **MIX**.

### 3. TEXTURE
- **TEXTURE LAYER**: osobna warstwa brudu, nigdy nie rusza dołu.
  - Źródło: **WHITE / PINK** szum albo **PULSE / SAW** na wysokości taila (jadą za jego zjazdem, PITCH przesuwa o półtony).
  - Gałki: LEVEL (OFF = brak), FREQ + RESO (filtr pasmowy), DECAY, DRIVE, LOW CUT.
- **CHARACTER**: SCREAM, CRUNCH, GRIT (jak w Forge).
- **IMPACT**: CHAOS.
- **FILTER + SUB**: LOW / HIGH CUT, BODY, CLEAN SUB oraz **SUB LEVEL**, czyli osobna głośność czystego subu (omija BUS).

### 4. BUS / FX
- **BUS DISTORTION**: przester na sumie PUNCH + TAIL + TEXTURE + CLICK. Klei warstwy w jeden kick, a czysty sub idzie bokiem. OFF = warstwy osobno.
- **OTT**.
- **ROOM**: z nowym **LOW CUT**, żeby pogłos nie zamulał dołu.
- **SAMPLE**: własna warstwa (LOAD albo przeciągnij WAV).

## AUTO GAIN
Gdy jest włączony (domyślnie w nowych kickach), podkręcenie DRIVE w STAGE 1 / 2 zmienia **charakter, a nie głośność**, więc ucho nie daje się oszukać
„głośniej = lepiej”. Twoje przeniesione presety mają go wyłączonego, żeby brzmiały dokładnie jak w Forge.

## Co się zmieniło pod maską
- Filtr DC w silniku działa teraz tak samo na 44.1, 48 i 96 kHz. W Forge przy 96 kHz ucinał trochę fundamentu (−1 dB).
  Przez to przeniesione presety mogą mieć minimalnie więcej dołu niż w Forge.
- Test sprawdza, że kick brzmi tak samo przy 44.1 / 48 / 96 kHz i że dwa uderzenia są identyczne co do próbki.

## Plan (następne wersje)
- **v0.2**: rysowany PITCH i AMP dla puncha / toka, dwa punche naraz (tick + tok), crossover między warstwami.
- **v0.3**: tryb jakości 2× / 4× / 8× (8× przy eksporcie), **EXPORT WAV**, A/B ustawień, stereo dla górnych warstw, RANDOM w kierunkach.
