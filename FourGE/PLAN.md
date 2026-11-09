# 4GE – plan przebudowy (2026-11-03)

> **Najpierw przeczytaj `RULES.md`** – zasady nadrzędne dla wszystkich 17 modułów (8 MASTER MACROS, makro ≠ parametr DSP, GUI z parametrów).

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
**Paleta zatwierdzona (właściciel, 2026-11-04): `Design/PALETTE.md` – BLACK INDUSTRIAL / BLOOD RED, stały kod kolorów PUNCH / BODY / TAIL / SUB / SCREECH / TEXTURE, proporcje 80 / 15 / 5.**

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
  Przykładowa logika GRIT → M05 Distortion Drive, M05 Waveshaper Fold, M05 Asymmetry, M08 Clipper Drive, M08 Saturation Amount.
  Każde przypisanie: Source Macro, Destination Parameter, Minimum Value, Maximum Value, Response Curve, Modulation Depth, Polarity,
  Smoothing, Enable/Disable, Priority/Combination Mode. Wiele źródeł na jeden parametr – jasno zdefiniowana kolejność i sposób łączenia.
  BARDZO WAŻNE: wartości bazowe niezależne od makr – ruch makra nigdy nie nadpisuje parametru bazowego; FINAL = f(BASE, aktywne modulacje).
  **Ostateczna rekomendacja (właściciel):**
  | Poziom | Nazwy | Funkcja |
  |---|---|---|
  | Master Macros | IMPACT, BODY, TAIL, PITCH, GRIT, SCREAM, WEIGHT, CHAOS | Główne sterowanie całym kickiem |
  | Macro Mapping | System modułu 09 | Łączenie makr z parametrami |
  | Local Controls | BRIGHTNESS, CRUSH, LOUDNESS itd. | Sterowanie konkretnymi modułami |
  | User Macros | Dowolne nazwy użytkownika | Własne kontrolery i przypisania |
  Moduł 09 pozwala tworzyć np. 4 własne makra użytkownika, niezależne od ośmiu głównych (MOVEMENT / DENSITY / SPACE mogą tam trafić).
- Reverse bass: **4GE NIE generuje reverse bassu i nie ma jego silnika** (właściciel) – to RE4GE. 4GE = Hardstyle Kick Designer (RULES.md pkt 5).
- Sample w presetach: bez preferencji → osadzane w presecie / stanie (jak RE4GE), RELINK gdy brak i nie ma kopii.
- PRE-ROLL screecha: potwierdzone przez właściciela jako PRE-TRIGGER (M06 SCREECH START MODE) – ciche rozkręcenie, słyszalny START bez zmian.
- SYNC TO BODY END: TAK – jako SCREECH START MODE (M06 dodatek: MANUAL / BODY END / BODY END + OFFSET / TAIL / KICK / HOST SYNC).

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
**STAN (2026-11-04): ✅ krok 0 (szkielet + Parameter Registry) i ✅ moduł 01 (v0.5) zrobione – czekają na ucho właściciela (A/B).**
Kod: `Source/Parameters/` (ParamRegistry = jedna tabela X-macro → APVTS / enum P:: / `Tests/param_ids.txt` / tooltipy; ParamFrame = migawka
wartości na blok), `Source/DSP/Core/` (Blocks: Osc / Noise / kickEnv / pitchShape; Generators: Punch / Body / Tail / Sub), `Source/DSP/Engine.h`
(KickVoice, 8 głosów CHOKE / POLY, CORE MIXER, 4x oversampling, `renderKick` offline), `Source/GUI/` (pierwszy ekran + WaveView z eksportem WAV),
`Source/Presets/`. Testy: `Tests/RenderTest.cpp` = 14.1–14.12 z MODULE01 + rejestr + CHARACTER + GUI. Narzędzia: `Tools/` (dopasowanie do
referencji `kmatch4.py`, A/B `kab4.py`, cechy `kana.py`, wykresy `kplot.py`). Nowy kod wtyczki `Fg5K` (stare projekty z v0.4 zostają na starym pliku).
Wniosek z A/B: rdzeń (1 oscylator na część) daje czyste, „sterylne” prążki – gęstość referencji przyjdzie z M03 (unison / drift), M04 (szum),
M05 (stopnie przesteru). DIRTY potrzebuje ~40 dB gainu, żeby mieć górę jak referencje (zakresy drive per CHARACTER w `Engine::cook`).
**STAN 2026-11-05 (wieczór): MODUŁ 07 GOTOWY** – `DSP/Tone/Tone.h` + `ToneCook.h` (16 pasm EQ na sekcjach TPT SVF, 4 dynamiczne, multiband 4 pasma
LR12/24/48 + Butterworth / Bessel z dynamiką i transientami, spectral: tilt / harmonic / exciter / kompresja na 24 pasmach / freeze, stereo 3 pasma +
MONO BELOW), jeden procesor dla każdego miejsca (jak M05 / M06, przed / po przesterze, też warstwy); `Presets/M07Tools.h`, `GUI/EqPage.*` +
`EqScope.h` (tło). Zero latencji; linear phase NIE zrobione (pre-ringing na kickach – do decyzji właściciela). Testy M07 OK. Następny: moduł 08.
**STAN 2026-11-05 (noc): MODUŁ 08 GOTOWY** – `DSP/Dyn/Dyn.h` + `DynCook.h`: 4 kompresory (8 typów, sidechain od części, parallel, lookahead
tylko na MASTER OUTPUT = lokalizacja 15 po downsamplingu, latencja zgłaszana hostowi, eksport przycięty o lookahead), 5 transient shaperów,
2 clippery (7 krzywych, CUSTOM = `DistData::clip`, oversampling), 2 limitery (per-kanał + STEREO LINK, MinQ lookahead, true peak), crest
control, AUTO GAIN (okno 50 ms) / LEVEL MATCH, ORDER, BAND LINK multibandu M07. Hooki po M07 w każdym miejscu (też warstwy). `Presets/M08Tools.h`,
`GUI/DynPage.*` + `DynScope.h` (krzywa, historia GR z `dyn::Probe`, crest; mierniki `Engine::m8Meters`). Testy M08 (tabela 14) OK. Następny: moduł 09.
**STAN 2026-11-08: MODUŁ 09 – część 1 GOTOWA** – `DSP/Mod/Mod.h` (jeden silnik: źródła → slot → ADD / MUL / MAX / MIN / REPLACE → clamp w domenie
znormalizowanej parametru; tick co 32 próbki na własnej siatce restartowanej przy każdym kicku = realtime == eksport; sekcje cooka `Engine::cookSubs`
re-gotowane tylko gdy modulacja je ruszy – test „partial = full” dla 1743 celów), `Presets/ModState.h` (64 sloty + 64 kroki, child MODM, brak = domyślny
MACRO MAPPING 25 slotów), parametry: 8 MASTER + 4 USER MACROS (tylko źródła), LFO 1–8, SEQ, RND 1–2, MOVEMENT; `GUI/ModPage.*` + 8 makr na MAIN.
Testy M09 (tabela 13) OK.
**STAN 2026-11-08 (wieczór): MODUŁ 09 – część 2 GOTOWA** – AUDIO FOLLOWER (`Engine::follow`, surowe szyny części), MIDI CC 1–4 + MIDI LEARN
(CC źródło / makro; CC → makro przez parametr hosta na wątku wiadomości = jedna wartość GUI / host / DSP), CUSTOM LFO (16 punktów, LINEAR /
CUBIC, w MODM „lfo1..8”), MACRO MORPH (zestawy A / B w MODM), VARIANT RANDOMIZER `Presets/VariantRandomizer.h` (style, scope, locks, seed,
regenerate, routing, wyrównanie głośności; podpięty pod RANDOMIZE), pierścienie modulacji na gałkach (`Knob::parameterId` + `setModulation`),
prawy klik na gałce = MODULATE WITH (`Knob::onContextMenu` w Shared), drag & drop źródła na gałkę (`GeCanvas` = DragAndDropContainer / Target),
DESTINATION MONITOR, waveform i drag WAV z modulacją. Moduł 09 kompletny wg tabeli 13. Następny: moduł 10.
**STAN 2026-11-08 (noc): MODUŁ 10 GOTOWY** – ADVANCED SAMPLE ENGINE: `DSP/Sample/Sample.h` (`SamplePlayer`: region, ONE SHOT / GATE / LOOP
z crossfade equal-power / PING-PONG, REVERSE, RATE x PITCH x KEY TRACKING (ROOT), fades + min. 1 ms przy starcie w środku, Hermite, 3 ms
crossfade przy podmianie; `resampleSinc` Kaiser), 8 slotów = warstwy 9–16 w `LayerBankVoice` (te same grupy / routing / link / solo),
parametry `smpN_*` (dopisane za `macro_morph`), obwiednie `AmpSample1..8`; `Presets/SamplePool.h` (wątek w tle: dekodowanie, osadzanie
base64 WAV 24 bit ≤ 20 s w dziecku stanu SAMPLES `sNname / sNpath / sNdata`, MISSING / ERROR, generacje zadań, cmentarz wskaźników
zwalniany po 96000 próbkach audio, overview + transienty, PREVIEW pliku / regionu), `Presets/SampleBrowser.h` (folder, szukanie,
ulubione / ostatnie w `4GE/browser.settings`), API procesora (import w tle z jednym UNDO, CLEAR, narzędzia, RELINK, COLLECT,
`samplePlayPos` z `Engine::smpPosMon`), `GUI/SamplePage.*` (strona SMP: sloty, edytor znaczników ze snapem, zoom, przeglądarka),
waveform i drag WAV z samplami. Testy M10 (tabela 13 + narzędzia, FLAC, 4 kanały, crossfade, CPU) i 14.11 dla `smp*` OK. Następny: moduł 11.
**STAN 2026-11-09: MODUŁ 11 GOTOWY** – STEREO & SPATIAL ENGINE: `DSP/Spatial/Spatial.h` (`Unit`: SWAP → POLARITY → CH DELAY (ułamkowe,
bufor 65536) → LOW MONO (LR4 tylko na side, mid nietknięty) → WIDTH: M/S / DECORRELATION (3 all-passy 1. rzędu, inne rogi L / R,
150 Hz–5 kHz, tylko z wysokich mid) / HAAS / OFF + MIX → adaptacyjna kompensacja energii (nigdy głośniej) → M/S ENGINE → PAN (mid,
stała moc) → BALANCE → OUTPUT; wygładzanie 5 / 30 ms; `Analyzer` korelacja / M / S / L / R / punkty scope), `SpatialCook.h`
(parametry `spc_<grupa>_*` x 7 + `spc_sub_mono_lock`, kuchnia w grupie GTone M09), w `Engine`: grupy PUNCH–SUB po M08 AFTER,
TEXTURE / CUSTOM w pętli szyn, MASTER przed downsamplingiem; SUB MONO LOCK = SUB zawsze mono + MASTER low mono ≥ 120 Hz (gdy master ON);
procesor: `analyzer` + `monoAudition` (poza stanem); `GUI/StereoPage.*` (zakładki, ALL UNITS, scope); nagłówek: siatka 5 x 3 przycisków.
Testy M11 (tabela 13 + CPU) i 14.11 dla `spc*` OK. Następny: moduł 12.
**STAN 2026-11-09: MODUŁ 12 GOTOWY** – MASTER OUTPUT & EXPORT: parametry `master_mute / bypass / output_mode / safety_clip / safety_limiter`
(poziom = `core_master`), etap wyjścia w `Engine::renderChunk` (mono, safety clip 0 dBFS, limiter -0.2 dBFS bez lookahead, mute 5 ms);
SEED: `Engine::setSeed` (salt w głosach, warstwach, screechu, LFO S&H / RND / SEQ; 0 = jak dotąd), seed presetu = właściwość `presetSeed`;
`DSP/Meter/MasterMeter.h` (LiveMeter peak / hold / clip / true peak 8x / RMS / LUFS M S I / DC + `measure()`), `DSP/Export/Render.h`
(migawka `Snapshot`, źródła przez `Engine::setExportKeep` + solo, długości AUTO / FIXED / UNTIL SILENCE z histerezą, latency out,
ScopedNoDenormals), `DSP/Export/Finish.h` (normalizacja bez limitera, mono, dither TPDF / shaped), `Presets/Export.h` (ustawienia w dziecku
EXPORT, nazwy, zapis `.part` + weryfikacja + rename, kontrola miejsca / folderu, historia JSON, `Exporter` wątek + batch), procesor
`makeExportJobs` / snapshoty presetów fabrycznych, `GUI/ExportPage.*` (strona 11). Testy M12 (tabela 17) OK. Następne: moduły 13–17.
**STAN 2026-11-09: MODUŁ 13 GOTOWY** – STYLE ENGINE: `Presets/StyleEngine.h` (szablony EARLY / MODERN / RAW / EXPERIMENTAL z presetów
v0.4, które właściciel słyszał + łańcuchy przesteru właściciela na TAIL; zakres = generatory / pitch / szum / transient / screech /
dist pool / master clip, NIGDY długości (`isLength`) ani reszta; `values()` / `onFrame()` (PREVIEW) / `apply()` = jedno UNDO przez
`setParamInUndoStep`), procesor: `touched` (gesty = ruszone przez użytkownika, zapis w stanie, czyszczone przez preset), właściwość
`style` (presety fabryczne z nazwy, eksport {STYLE}, RANDOMIZE `vrStyle`); nagłówek: combo STYLE + menu prawego klika + komunikat.
Testy M13 OK. Uwaga: EXTRA LONG nie wydłuża jeszcze części – to moduł 14 (LENGTH ENGINE skaluje PUNCH / BODY / TAIL / SUB).
**2026-11-09 (prośba właściciela): KICK END** – parametr `core_note_cut` (LENGTH / NOTE / NOTE + FX): note-off dławi głosy (5 ms) i screech,
NOTE + FX dodatkowo wycisza całe wyjście (`outGate` po dynamice mastera); część modułu 14.
**STAN 2026-11-09: MODUŁ 14 GOTOWY** – KICK LENGTH ENGINE: `kickLengthMs (f, bpm)` w `DSP/Engine.h` (LENGTH albo `len_sync` = część beatu),
AUTO LENGTH skaluje części przez `len_follow_punch / body / tail / sub` (domyślnie 0 / 0.5 / 1 / 1 = dokładnie jak wcześniej), eksport
i waveform liczą efektywną długość; `Presets/LengthEngine.h` (profile SHORT / MEDIUM / LONG / EXTRA LONG + etykieta), nagłówek: przycisk
LENGTH → panel (CallOutBox) z profilami, AUTO, SYNC, FOLLOW i długościami części; KICK END (LENGTH / NOTE / +FX). Testy M14 OK.
**STAN 2026-11-09: MODUŁ 16 GOTOWY** – KICK ANALYSIS: `DSP/Analysis/KickAnalysis.h` (czysta analiza bufora: waveform, obwiednia 5 ms,
długość -40 dB, attack / transient / click, pitch NSDF na dwóch kopiach (2 kHz dla spadku, 250 Hz dla niskiego ogona pod skrzeczącym
środkiem) + nuta ogona = mediana ważona głośnością, czas spadku i TOK, spektrum FFT 8192 + 6 pasm + % suba + harmoniczne, stereo /
korelacja / korelacja < 150 Hz, peak / RMS / LUFS / crest / clip, cechy słowami); `Presets/AnalysisTools.h` (wczytanie referencji,
`renderMine` = render eksportu migawki, `suggest` = sugestie z wartościami, `apply` = jeden krok UNDO); `GUI/AnalysisPage.*` = strona 13
(przycisk ANALYZE w nagłówku, drag & drop, nakładki REF / 4GE, tabela, sugestie z checkboxami). `RenderTest <out> --analyse <wav> [styl]`.
Testy M16 OK.
**STAN 2026-11-09: MODUŁ 15 GOTOWY** – PRESET & BROWSER: `Presets/PresetLibrary.h` (metadane z wartości: styl, długość, typ ogona, tagi
charakteru + tagi / notatki użytkownika w atrybutach .4gp, moduły, brakujące sample; filtry, RECENT w `4GE/presets.settings`,
duplicate / rename / delete do kosza / import (walidacja, wolne nazwy) / export, preset fabryczny zapisywany przez prywatny procesor),
procesor: `setPresetName`, A / B (`abSwitch` / `abCopyToOther` na pełnym stanie), `onPresetLoaded`, tagi / notatki w stanie;
`Shared` PresetBar: `onBrowse` (własna przeglądarka zamiast wbudowanej); `GUI/PresetPage.*` = strona 12 (klik na nazwę presetu),
PREVIEW = render migawki w tle. Testy M15 OK.

**STAN 2026-11-05 (popołudnie): MODUŁ 06 GOTOWY** – `DSP/Filter/Filters.h` (8 filtrów, 10 algorytmów, 2 rezonatory, LOCATION + przed / po
przesterze M05, SERIAL / PARALLEL / SPLIT), `Screech.h` (8 trybów syntezy, 4 oscylatory, formanty, feedback ze strażnikiem, obwiednie, SCREECH
START MODE), `M06Cook.h`; `Presets/M06Tools.h` (8 presetów, pliki, A / B, randomize z LOCK), `GUI/ScreechPage.*` + `M06Scope.h` (tło).
Testy M06 (tabela 13 + START MODE) OK; makra i matryca modulacji → M09. Właściciel: bazowy kick „drewniany” – poprawić przed oceną presetów.
Następny: moduł 07.

**STAN 2026-11-05: MODUŁ 05 GOTOWY** – `DSP/Dist/Distortion.h` (11 krzywych w tablicach 8192 + ADAA1, halfbandy 8x / 16x polyphase,
filtry pre / post, modulacja, AUTO GAIN, de-click przy zmianie typu, `staticCurve` dla wizualizera) + `DistCook.h`; pula 16 stopni z LOCATION
(GLOBAL / PUNCH–SUB / TEXTURE / LAYER 1–8) i ORDER, bloki SERIAL / PARALLEL / MULTIBAND (LR4 + all-pass); krzywe CUSTOM w child DIST;
`Presets/DistTools.h` (pula, 18 presetów, pliki .4gstage/.4gchain/.4gmband/.4gpar/.4gdist), `GUI/DistPage.*` + `GUI/DistScope.h` (tło).
Testy M05 19.1–19.18 + GUI OK; makra M05 → moduł 09. Następny: moduł 06.

**STAN 2026-11-05 (rano): MODUŁ 04 GOTOWY** – `DSP/Layers/` (NoiseGen, Transient, Layers, LayerCook, PhaseAlign), 7 stereo magistral w silniku
(PUNCH / BODY / TAIL / SUB dokładane do kanałów części, TEXTURE / CUSTOM przed clipem, MASTER po clipie), AttackShaper przed clipem,
15 nowych slotów rysowanych obwiedni, GUI `GUI/LayerPage.*` + `GUI/KickPreview.h` (rendery i analiza fazy w tle), narzędzia / pliki
`Presets/LayerTools.h` (nazwy / LOCK / kolejność w child LAYERS), `setParamInUndoStep` synchronizuje drzewo APVTS przed zapisem.
Testy M04 19.1–19.18. NASTĘPNY: moduł 05 (distortion).
**STAN 2026-11-05: MODUŁ 03 GOTOWY** (silnik `DSP/Osc/` OscBank / Noise / Wavetable / OscTables, dane w child OSCS, tablice budowane w procesorze
poza wątkiem audio i oddawane lock-free; GUI `GUI/OscPage.*` + `GUI/OscScope.*`; pliki / presety oscylatorów `Presets/OscFiles.h`,
`Presets/OscPresets.h`; testy M03 17.1–17.15 + GUI). Makra oscylatorów (spec 13) odłożone do M09 (RULES: lokalne kontrolery + mapowanie).
NASTĘPNY: moduł 04 (noise / transient / layers) – NoiseEngine z M03 do ponownego użycia.
**STAN 2026-11-04 23:00: MODUŁ 02 GOTOWY.** SOUND PASS zrobiony + po A/B właściciela: PITCH HOLD (tok trzymany, potem skok – „pup pup”),
TOK LEVEL (`tail_hold_level`), presety MODERN / RAW: CLICK + NOISE pod tokiem + sub / body po toku (power), EARLY = nowa referencja
kick_hardstyle_28 (stary kick_84 właściciel odrzucił). Procesor: child ENVS (undo, presety użytkownika, reset przy presetach fabrycznych),
host BPM / LOCAL BPM, `setParamInUndoStep`. GUI: strona ENVELOPES (`GUI/EnvPage.*`, `GUI/EnvEditor.*`). Testy M02 14.2–14.14 w RenderTest.
NASTĘPNY: moduł 03 (oscylatory).
**STAN 2026-11-04 17:30 (historia):** moduł 02 = silnik gotowy i przetestowany (WIP commit a5e7235: `DSP/Env/MultiEnv.h`,
`DSP/Env/KickEnvelopes.h`, parametry M02 w rejestrze, test 14.11 z kontekstami) – BRAKUJE: podpięcie banku krzywych w procesorze
(ValueTree child ENVS + undo -> `env::BankExchange`, host BPM -> `Engine::setHostBpm`, zapis kształtów w presetach / plikach), GUI edytora
(strona ENVELOPES: wybór części + VIEW, edytor punktów, LINK / COPY / PASTE / RESET), testy 14.1–14.14 z MODULE02.
**NAJPIERW (właściciel po A/B: „kicki nie przypominają referencji”, zgoda na „najpierw dźwięk”): SOUND PASS** – przenieść sprawdzony tor v0.4:
TAIL UNISON + DRIFT (3 strony: środek + L / R, dół mono z środka, LR4 przy WIDTH FROM), STAGE 2 + MID BELL (typy z `DSP/Core/Stages.h`,
już napisane), TAIL LOW CUT / HIGH CUT (sub z generatora SUB = czysty dół), MASTER CLIP (sub omija clip), PUNCH INTO DRIVE, rysowana AMP
(dołek po toku, potem narastanie – jak OWNER QL w v0.4). Potem dopasowanie `Tools/kmatch4.py` z większą przestrzenią i nowe A/B.
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
