# 4GE – plan przebudowy (2026-11-03)

Pełna specyfikacja właściciela: **`SPEC.md`** (bez zmian). Ten plik: co już jest w 4GE v0.4, czego brakuje i w jakiej kolejności budujemy.
Zasady jak w RE4GE: krok po kroku, po każdym kroku kompilacja + testy + A/B dla właściciela (z JEGO kickami, nastrojonymi – LESSONS),
żadnych atrap (każda gałka zmienia DSP – test min / max), nowe ID parametrów tylko dopisywane, DSP / GUI / pliki osobno.

## Decyzje właściciela (najważniejsze)
- **STYL i DŁUGOŚĆ niezależne** (krótki Early, długi Raw…). Styl ustawia punkt startowy, nigdy nie blokuje gałek.
- Style: **EARLY / MODERN / RAW / EXPERIMENTAL** (zakresy: `Shared/RESEARCH.md` „Style kicka…” + nasze 38 pomiarów).
- Długość: **SHORT / MEDIUM / LONG / EXTRA LONG** (100–250 / 250–500 / 500–1000 / 1000–2500 ms, punkty startowe) + ms / wartości nut,
  **AUTO LENGTH** skaluje fazy z głową: punch zostaje krótki, body trochę, tail niezależnie, transient nie traci ostrości.
- Wygląd: **własna identyfikacja** – czerwono-czarny (graphite / black / dark grey + czerwony akcent), industrialny, techniczny,
  **duży centralny waveform**, **5 pionowych bloków**: KICK / SYNTHESIS / PROCESSING / MODULATION / SYSTEM (SYSTEM osobno, na dole).
- Zawsze widoczne: preset, STYLE, LENGTH, makra (IMPACT / BODY / TAIL / PITCH / GRIT / SCREAM / WEIGHT / CHAOS), master, BPM,
  Play, Random, miernik, Export + mały waveform.

## Docelowa architektura 4GE – 17 modułów (właściciel, 2026-11-04)
| Moduły | Odpowiedzialność |
|---|---|
| 01–03 | Core, pitch, oscylatory |
| 04–07 | Generatory i processing |
| 08–09 | Dynamika i modulacja |
| 10–12 | Sample, stereo, eksport |
| 13–14 | Style i długość kicka |
| 15–16 | Presety i analiza |
| 17 | Interfejs użytkownika |

**Decyzja architektoniczna (właściciel, 2026-11-04):** GUI (moduł 17) projektujemy NA PODSTAWIE rzeczywistych parametrów i funkcji
modułów 01–16 (Parameter Registry) – żadnego „pięknego” interfejsu, którego nie da się sensownie połączyć z DSP. Makieta `Design/mock.html`
jest tylko kierunkiem wizualnym (kolory, charakter), układ ekranów powstaje po DSP.

## Odpowiedzi na pytania końcowe (2026-11-04)
- **Makra = trzy poziomy, jeden nadrzędny system** (właściciel): poziom 1 = MASTER MACROS – jedyne 8 makr stale w głównym GUI,
  każde steruje kilkoma parametrami z różnych modułów (np. GRIT: drive M05 + harmoniczne + agresywność wybranych elementów),
  z zaprojektowaną krzywą działania i określonymi celami – NIE podnosi wszystkiego naraz. Makra modułów 05–08 (BRIGHTNESS, LOUDNESS,
  CRUSH…) to wewnętrzne narzędzia pod spodem – nigdy drugi konkurencyjny zestaw głównych makr. **MASTER MACROS (właściciel, 2026-11-04):** 01 IMPACT – siła i charakter uderzenia ·
  02 BODY – masa i charakter korpusu · 03 TAIL – długość i charakter ogona · 04 PITCH – charakter i ruch pitchu · 05 GRIT – brud, agresja,
  nasycenie · 06 SCREAM – rezonans i screech · 07 WEIGHT – ciężar i energia niskich częstotliwości · 08 CHAOS – kontrolowana niestabilność
  i eksperymentowanie. (= zestaw z głównej SPEC; moduł 09 nie ma własnych makr – ma MACRO ROUTING & MAPPING, czyli przypisywanie tych 8 do parametrów całej wtyczki. Opis poziomów 2–3: czekamy na właściciela.)
  Przykładowe cele w Modulation Matrix (właściciel): IMPACT → Transient Level, Punch Drive, Attack · BODY → Body Level, Oscillator Mix,
  Body Decay · TAIL → Tail Length, Tail Envelope, Release · PITCH → Pitch Envelope, Pitch Depth, Pitch Curve · GRIT → Distortion Drive,
  Fold, Asymmetry · SCREAM → Resonance, Screech Level, Screech Pitch · WEIGHT → Sub Level, Low Body, Sub Saturation · CHAOS → Random Amount,
  Modulation Depth, Phase Variation.
  **Poziom 2:** moduł 09 = MACRO ROUTING & MAPPING (przypisania 8 MASTER MACROS); MOVEMENT / DENSITY / SPACE = lokalne kontrolery,
  opcjonalnie jako makra użytkownika (sloty 9–16). **Poziom 3:** makra modułów 05–08 (GRIT/HARDNESS/WARMTH/FOLD/… z M05, SCREAM/
  RESONANCE/METALLIC/… z M06, BRIGHTNESS/CLARITY/… z M07, PUNCH/LOUDNESS/CRUSH/GLUE/… z M08) zostają jako LOKALNE KONTROLERY w swoich
  modułach – nie konkurują z MASTER MACROS (master macro może je sterować jako cel).
  Hierarchia (właściciel, `Design/macro_hierarchy.png`): 4GE MASTER MACROS → GLOBAL MODULATION & MAPPING (moduł 09) → lokalne makra:
  M05 DISTORTION (DRIVE · CRUSH · WARMTH), M06 FILTER & SCREAM (BRIGHTNESS · RESONANCE), M07 EQ & TONE (TONE · PRESENCE · LOW END),
  M08 DYNAMICS (LOUDNESS · PUNCH · GLUE). Lokalne makra wpływają na swój moduł; główne mogą nimi sterować przez przypisania.
  Przykład (właściciel): GRIT 80 % → rośnie lokalny DRIVE i CRUSH, asymetria waveshapera, poziom harmonicznych; lokalny CRUSH nadal
  ręcznie w M05. Ręczna zmiana CRUSH NIE nadpisuje GRIT i nic się samo nie resetuje.
  Realizacja: makro główne = MODULACJA (offset w matrycy na wartości bazowej), nie zapis do parametru. Ręczne pokręcenie zmienia
  wartość bazową, GRIT dalej dodaje swój offset na wierzchu; pierścień na gałce pokazuje BASE / FINAL (M09 pkt 10.2).
  **Kluczowa zasada techniczna (właściciel): makro NIE jest parametrem DSP.** Każde makro to nadrzędny kontroler, który wysyła wartości
  do parametrów wyłącznie przez system mapowania (M09). Żaden kod DSP nie czyta wartości makra bezpośrednio.
- Reverse bass przy EARLY: bez preferencji → 4GE robi kick pod RE4GE (bez dublowania); ewentualnie później prosta warstwa reverse.
- Sample w presetach: bez preferencji → osadzane w presecie / stanie (jak RE4GE), RELINK gdy brak i nie ma kopii.
- PRE-ROLL screecha: bez preferencji → ciche rozkręcenie silnika przed START (słychać od START).

## Domyślna konfiguracja komponentów (rekomendacja właściciela, 2026-11-03)
Nie każdy komponent ma te same funkcje włączone na start – początkujący od razu robi sensowny kick, zaawansowany ma całą maszynerię:
| Komponent | Domyślnie |
|---|---|
| PUNCH | Sine + Click |
| BODY | Sine + Saw |
| TAIL | Saw + FM |
| SUB | Sine |
Reszta funkcji (unison, wavetable, harmonic engine, ring, sync, kolejne oscylatory) – dostępna, ale wyłączona w INIT.

## Domyślne łańcuchy distortion per styl (właściciel, 2026-11-03) → STYLE ENGINE
| Styl | Charakter | Łańcuch stopni |
|---|---|---|
| EARLY | ciepły, kontrolowany, klasyczny | SOFT CLIP → TUBE |
| MODERN | gęsty, mocny, precyzyjny | HARD CLIP → SOFT CLIP → DIGITAL CLIP |
| RAW | agresywny, asymetryczny, mocno nasycony | ASYMMETRIC → HARD CLIP → WAVEFOLD |
| EXPERIMENTAL | nietypowe harmoniczne, metaliczne tekstury | WAVEFOLD → RING MOD → CUSTOM SHAPER |
Wniosek do modułu 05: wśród typów stopnia potrzebne też **DIGITAL CLIP** i **RING MOD** (mnożenie przez wewnętrzny nośnik
o częstotliwości względem nuty) – nie tylko ring mod między oscylatorami z modułu 03. Styl ustawia tylko te łańcuchy na start.

## OBOWIĄZKOWE: routing distortion per warstwa (właściciel, 2026-11-03)
Nie każda warstwa jest przesterowana tak samo. **SUB musi móc zostać całkiem czysty, nawet gdy TAIL jest ekstremalnie zniszczony.**
Routing przesteru na poziomie pojedynczej warstwy to funkcja obowiązkowa (od modułu 05, architektura gotowa od modułu 01):
każda warstwa / komponent ma własny wybór „do których stopni idzie” (łańcuch per PUNCH / BODY / TAIL / SUB / warstwa + DRY / bypass),
SUB domyślnie BYPASS (czysty), żadnego wspólnego przesteru na sumie, który by go łapał bez wiedzy użytkownika.
Test: TAIL z maksymalnym przesterem, SUB bypass → widmo SUB-u bez harmonicznych (THD ~ 0), sinus identyczny jak solo.

## Co już jest (v0.4) → czego brakuje
| Spec | 4GE v0.4 | Brakuje |
|---|---|---|
| Punch (6) | typy Tonal / Hard / Click / Reso, level / start / end / drop / decay / attack / drive / into dist, rysowany AMP | źródła saw / square / noise / sample, PUNCH TONE / FILTER / PHASE, kilka warstw puncha |
| **Body (7)** | tylko gałka BODY (EQ) | **cały silnik BODY**: własne źródło, pitch + amp env, tone / drive / reso / filter, przejście do taila |
| Tail (8) | osc A + B, start / drop / curve / hold, rysowany PITCH / DRIVE, dist, scream / crunch / grit | TAIL CHARACTER (CLEAN … CRUSHED), FM / wavetable, osobne obwiednie filtra / reso / width |
| Pitch (9) | modes, drawn pitch, tune / fine, start w Hz | DOUBLE DROP / DROP+RISE / RISE, snap / kopiuj / presety krzywych, analiza f0 (jest w RE4GE) |
| **Length (4)** | jedna gałka LENGTH | **profile + AUTO LENGTH + długości faz** |
| **Style (3)** | brak | **silnik stylów + kontrolki stylu** |
| Osc (10), Noise (11), Transient (12) | osc A / B, CHAOS / TEXTURE, CLICK | 4 osc z unison / FM / ring / sync, typy szumu, transient shaper w pasmach |
| Sub (13) | CLEAN SUB (crossover), SUB LEVEL | typy, oct / harm / sat (jest w RE4GE `LowEnd.h`), ducking |
| Distortion (14) | 2 stopnie, 5–6 typów, bias / mix / auto gain, BUS | **4 stopnie**, Distortion Lab z RE4GE (ADAA, waveshaper editor), routing serial / parallel / split, osobno P / B / T |
| Filter / Resonance (15–16) | pre filter + sweep, SCREAM | moduł RESONANCE (CLEAN / METALLIC / SCREAM / RUBBER), ladder / comb / formant |
| Sample (17), Layers (18) | SAMPLE layer | edytor + tryby (RE4GE KickView / GrainReader), 8 warstw z solo |
| Envelopes / LFO / Matrix (19–20) | rysowane AMP / DRIVE / PITCH | z RE4GE: LFO 1–4 + MATRIX, dodatkowe obwiednie |
| Sequencer (21) | – | z RE4GE (opcjonalny) |
| EQ / Dynamics / Stereo (22–24) | OTT, clipper, unison / width | z RE4GE: EQ 8 + kompresor; limiter, mono check |
| Macros (25) | – | 8 makr (RE4GE `Macros.h`) z min / max / curve |
| Randomizer (26) | prosty Random | z RE4GE: tryby + STYLE + AMOUNT + LOCK + seed |
| Presets (27) | lista + user | kategorie z RE4GE (+ długość w nazwie), tagi / ulubione później |
| Export (28) | – | z RE4GE + opcje SR / bit / normalize / stems |
| Analysis / A/B / Morph (29, 34) | – | z RE4GE: Reference, A/B + MORPH, f0 |

## Kolejność (zgodnie z etapami SPEC 37, z tym co już mamy)
0. **Makieta ekranu** + szkielet nowego projektu (komponenty `Source/DSP` / `Parameters` / `Presets` / `GUI`, nowe ID od zera); v0.4 zostaje
   w historii gita jako wzór. (Wcześniej planowany refaktor bit w bit – niepotrzebny po decyzji „od nowa”.) Makieta (industrial, 5 bloków, waveform) do zatwierdzenia.
1. **Core** – szczegółowa specyfikacja właściciela: **`MODULE01.md`** (PunchGenerator / BodyGenerator / TailGenerator / SubGenerator →
   CoreMixer: level / pan / mute / solo / phase invert / peak; LENGTH 100–2500 ms (domyślnie 450) + AUTO LENGTH, PHASE RESET, MASTER;
   PITCH ENGINE z trybami CLASSIC / FAST / SLOW / DOUBLE DROP, RISE, STABLE, CUSTOM, przygotowany na osobne krzywe P / B / T;
   TAIL CHARACTER CLEAN / TONAL / DIRTY / SCREAM / METALLIC / BUZZ / CRUSHED (realnie różne tory); głosy nakładające się, note-off nie ucina;
   12 kryteriów akceptacji = testy RenderTest). **Decyzja właściciela (2026-11-03): silnik budujemy OD NOWA w oparciu o doświadczenia** (Forge / 4GE v0.4 / RE4GE / LESSONS) –
   bez obowiązku zgodności z presetami OWNER / FULL; stary kod to tylko źródło wiedzy (co brzmiało dobrze: smaczki w pamięci, LESSONS).
   Poprzedni opis kroku: silnik BODY, LENGTH ENGINE (profile + AUTO LENGTH + fazy), nowy ekran KICK (Main / Style / Length / Punch / Body / Tail / Pitch),
   waveform z klikalnymi strefami, eksport WAV. **Bramka: ucho właściciela.**
1b. **Moduł 02 – Advanced Pitch & Envelope** (specyfikacja: **`MODULE02.md`**): wielopunktowe krzywe do 64 punktów (TIME / VALUE / CURVE /
   TENSION; LINEAR / EXP / LOG / EASE IN / OUT / IN-OUT / BEZIER / STEP – wszystkie w DSP), osobny pitch dla PUNCH / BODY / TAIL / SUB
   (ENABLE / AMOUNT / TIME SCALE / OFFSET / COPY / PASTE, SUB FOLLOW BODY), PITCH LINK (INDEPENDENT / FOLLOW / COPY / LINKED, bez cykli),
   uniwersalny silnik obwiedni (AMP / PITCH / CUTOFF / DRIVE / RESO / NOISE / FM / WIDTH / FOLD; ADSR + DELAY / CURVE / POLARITY,
   ONE SHOT / RETRIGGER / LEGATO), TIME SCALE 10–400 % + OFFSET + sync do BPM, przypisania źródło → cel (suma z clampem), edytor
   (zoom, snap, wielokrotne zaznaczenie, undo, presety kształtów). Pitch liczony w półtonach (f = f0 · 2^(st/12)), base osobno od krzywej.
   Klocki do wzięcia: `Shared` `mod::Shape` + RE4GE `ShapeDesigner` (rozbudować o typy interpolacji / tension / Bezier / 64 punkty).
1c. **Moduł 03 – Oscillator & Waveform Engine** (specyfikacja: **`MODULE03.md`**): min. 4 oscylatory (architektura do 8), każdy z własną fazą;
   SINE / TRI / SAW / SQUARE / PULSE (1–99 %) / NOISE (white / pink / brown / metallic / digital) / CUSTOM (rysowany / import cyklu z WAV);
   anti-aliasing u źródła (PolyBLEP / band-limited, oversampling dla FM / ring / sync); WAVETABLE (position / morph / scan: static / linear /
   env / LFO / random z seedem, mipmapy); HARMONIC ENGINE 32–64 harmonicznych (amp / phase / on, odd / even / roll-off / tilt / spread / random);
   UNISON 1–16 (detune / spread / blend, tryby, gain pod kontrolą); PHASE (reset / random / free / synced, align, invert); FM MATRIX
   (ratio 0.1–32, fine, feedback, env); RING MOD (osobny algorytm); HARD / SOFT SYNC; routing osc → PUNCH / BODY / TAIL / SUB / CUSTOM BUS
   (single / multi / split, poziom na połączenie); 8 makr osc; widok OSCILLATORS + spectrum z prawdziwego sygnału; presety osc.
   Klocki: `Shared` polyBLEP / `Oscillator`, RE4GE `OscBank` (PM, unison), 5CR34M 4LPH4 (FM / wavetable – sprawdzić).
1d. **Moduł 04 – Noise, Transient & Layer Engine** (specyfikacja: **`MODULE04.md`**): NOISE (white / pink / brown / blue / violet / metallic
   z rezonatorami / digital z bit / SR reduction / custom; level / color / filter / cutoff / reso / drive / A / D / length / width / tracking;
   seed + FIXED / PER TRIGGER / FREE RUN); TRANSIENT (sine / noise / metallic click, impulse, pulse, snap, custom; attack / click level,
   length, pitch, tone, drive; LOW / HIGH transient; prawdziwy TRANSIENT SHAPER z detektorem; POSITION 0–250 ms od kick / punch / body /
   tail start); LAYERS min. 8 (docelowo 16): źródło osc / noise / transient / internal / (sample później), grupa PUNCH / BODY / TAIL / SUB /
   TEXTURE / CUSTOM, pełne parametry + własna obwiednia, kontrola grup, routing do busów (single / multi, pre / post send), LINK bez cykli,
   randomizer warstw z LOCK / seed / undo, solo layer / group / source / processed / multi, opcjonalny PHASE ALIGN + korelacja,
   lista warstw z mini-waveformem (przewijana), presety noise / transient / layers.
   **Wzór właściciela – przykładowy nowoczesny kick (8 warstw, nie preset):** 01 MAIN PUNCH sine + mocny pitch transient (PUNCH),
   02 CLICK krótki biały szum + impuls (PUNCH), 03 BODY sine + saw, kontrolowane harmoniczne (BODY), 04 METALLIC TEXTURE metallic noise
   + rezonans (TEXTURE), 05 MAIN TAIL saw + FM + distortion (TAIL), 06 HIGH SCREAM wysoka tekstura z FM (TAIL), 07 SUB czysta sinusoida
   mono (SUB), 08 NOISE TAIL filtrowany szum z długim zanikiem (TEXTURE) → test: ten stos musi dać się zbudować i ma być presetem
   „MODERN · Layer Stack” w presetach startowych.
1e. **Moduł 05 – Distortion & Waveshaper Engine** (specyfikacja: **`MODULE05.md`**, kompletna + NOTES): min. 4 stopnie
   (docelowo 8), każdy: enable / type / drive / in / out / mix / bias / symmetry / pre + post filter / oversampling / routing, zmienna
   kolejność (także z filtrem między stopniami); typy z własnymi parametrami: SOFT CLIP (threshold / knee), HARD CLIP (ceiling / symmetry),
   TUBE (bias / warmth / even / odd), DIODE (bias / asym / threshold), WAVEFOLD (amount / threshold / bias / symmetry / 1–8 stopni),
   BITCRUSH (1–24 bit / SR reduction / quantization / dither), RECTIFIER (half / full / positive / negative / custom), ASYMMETRIC…
   Klocki: `Shared/DoomsdayLab.h` (ADAA: saturation / diode / wavefold / rectifier / bitcrush / custom), RE4GE WaveshaperView.
   **Makieta ekranu:** `Design/mock.html` → `Design/4GE_makieta_main.png` (do zatwierdzenia).
1f. **Moduł 06 – Filter, Resonance & Screech Engine** (specyfikacja: **`MODULE06.md`**, kompletna + NOTES / 3 pytania): trzy niezależne silniki
   z przesyłaniem sygnału między nimi – FILTER (wieloetapowy), RESONANCE (własne źródła wzbudzenia, obwiednia, modulacja, routing,
   kontrola stabilności), SCREECH (synteza screechy od podstaw – nie sampel, nie sam high-pass). Doświadczenie do wzięcia:
   Prophecy of Scream / 5CR34M 4LPH4 (`Tools/screech`, LESSONS o screechach), `Shared` Ladder / SVF / Formant.
1g. **Moduł 07 – EQ, Multiband & Spectral Shaping** (specyfikacja: **`MODULE07.md`**, kompletna + NOTES): PARAMETRIC EQ 8→16 pasm
   (9 typów, min / linear phase), DYNAMIC EQ (5 trybów, detektor peak / RMS / sidechain), MULTIBAND 4→6 pasm (crossover LR 12/24/48 /
   Butterworth / Bessel, dynamika + transient per pasmo, sendy do M05 / M06 – bez kopii silników), SPECTRAL SHAPER (tilt / harmonic /
   spectral compression / exciter / freeze), STEREO per pasmo + M/S + mono below; poziomy GLOBAL / GROUP / LAYER, 8 makr częstotliwości,
   analizator, 8 presetów, latencja zgłaszana do hosta. Klocki: RE4GE `DSP/Eq8.h` + EqView, `Shared` Crossover3, Mirror of Souls (EQ).
   Kolejność wewnątrz modułu: EQ → analizator → dynamic EQ → multiband → dynamika pasm → spectral → stereo → integracja → testy;
   JEDNA wspólna klasa procesora dla warstwy / grupy / mastera.
1h. **Moduł 08 – Dynamics, Compressor, Transient & Limiter** (specyfikacja: **`MODULE08.md`**, PDF `Specs/MODULE08_owner.pdf`):
   kompresor 8 typów (VCA / FET / Opto / Vari-Mu / Digital / Upward / Parallel / Sidechain) z detekcją peak / RMS / hybrid / true peak,
   multiband dynamics na crossoverach M07, transient shaper per grupa, clipper 7 krzywych + oversampling, limiter 5 trybów z true peak,
   crest factor, auto gain (okno, nie próbka po próbce), 8 makr dynamiki, 8 presetów. Klocki: RE4GE `Comp`, Throne of Titans (limiter, LUFS).
1i. **Moduł 09 – Modulation Matrix & Advanced Modulation** (specyfikacja: **`MODULE09.md`**, PDF `Specs/MODULE09_owner.pdf`):
   NAJPIERW Parameter Registry (fundament dla wszystkich modułów – realnie budowany już w kroku 0 / 1), potem centralny Modulation Engine
   (32→64 sloty, ADD / MULTIPLY / MAX / MIN / REPLACE), źródła (obwiednie, LFO 1–8, velocity, key, makra, sekwencer 1–64, random z seedem,
   audio follower, MIDI CC), 8 makr startowych PUNCH / BODY / TAIL / GRIT / SCREAM / MOVEMENT / DENSITY / SPACE (do 16) + Macro Morph,
   MIDI Learn, monitory BASE / MOD SUM / FINAL. Klocki: RE4GE ModMatrix / LFO / Trigger / Randomizer / MorphBar / ParamBridge.
1j. **Moduł 10 – Advanced Sample Engine** (specyfikacja: **`MODULE10.md`**): 8→16 slotów (grupa PUNCH / BODY / TAIL / SUB / TEXTURE /
   CUSTOM), WAV / AIFF (+ FLAC / MP3), tryby ONE SHOT / GATE / LOOP / PING-PONG / REVERSE, start / end / fade / rate / pitch / key track,
   time stretch, edytor przebiegu, obwiednia slotu, browser z RELINK / COLLECT; wszystko w tle na kopii. Klocki: RE4GE sample + GrainReader, WSOLA.
1k. **Moduł 11 – Stereo & Spatial Engine** (specyfikacja: **`MODULE11.md`**, kompletna + NOTES): PAN (stała moc) / BALANCE / WIDTH 0–200 % /
   CHANNEL SWAP, Mid/Side, LOW FREQUENCY MONO (domyślnie 120 Hz) + SUB MONO LOCK (priorytet nad WIDTH), polaryzacja / channel delay /
   phase alignment (tylko z potwierdzeniem), tryby M/S / dekorelacja / Haas / off, poziomy layer / group / master, korelometr, vector scope,
   odsłuch mono; domyślnie SUB 0 %, TAIL 120 %, TEXTURE 130 %. Jedna klasa stereo razem z M07 pkt 6.
1l. **Moduł 12 – Master Output & Export Engine** (specyfikacja: **`MODULE12.md`**, kompletna + NOTES): MASTER LEVEL / MUTE / BYPASS /
   MONO-STEREO, SAFETY CLIP / LIMITER domyślnie OFF; mierniki Peak / True Peak / RMS / LUFS / DC; eksport WAV / AIFF / FLAC, 16 / 24 / 32f,
   SR + resampling, dither raz na końcu (domyślnie OFF), normalizacja bez ukrytego limitera, źródło FULL / grupa / warstwa, długość AUTO /
   FIXED / UNTIL SILENCE z pre / post-roll, offline = realtime, seedy FIXED / NEW / PRESET / CUSTOM, batch + nazwy, historia, plik .part.
   Klocki: RE4GE Exporter / ExportPanel, Throne of Titans (LUFS / true peak).
1m. **Moduły 13+ (właściciel, 2026-11-04): warstwa zarządzająca całym instrumentem – OSOBNE specyfikacje, nie doklejane do 01–12.**
   Brakuje (wg SPEC): STYLE + LENGTH ENGINE (EARLY / MODERN / RAW / EXPERIMENTAL, długość niezależna od stylu), PRESET MANAGER +
   AUDIO ANALYSIS (browser, kategorie, A/B, reference), GUI / DESIGN SYSTEM (5 bloków nawigacji, makieta `Design/`).
   Przegląd właściciela: **`MODULES13-17_overview.md`** – 13 STYLE ENGINE (priorytet), 14 KICK LENGTH ENGINE (SHORT ~100–250 ms /
   MEDIUM ~250–500 / LONG ~500–1000 / EXTRA LONG ~1000–2500 / AUTO), 15 PRESET & BROWSER, 16 KICK ANALYSIS, 17 GUI & UX (17 modułów).
2. **Style engine**: EARLY / MODERN / RAW / EXPERIMENTAL + kontrolki stylu + presety startowe (każdy styl × długości).
3. **Synthesis**: 4 osc, noise, transient, sub z RE4GE, resonance, sample + 8 warstw z solo.
4. **Processing**: 4 stopnie + Distortion Lab + waveshaper, filtry, EQ 8, dynamika, stereo.
5. **Modulation**: obwiednie, LFO + matrix, makra z mapowaniem, Randomizer (tryby / style / LOCK / seed), sekwencer.
6. **Pro tools**: browser + kategorie, A/B + morph, reference, batch / stems export, history, snapshot.
