# RE4GE – plan v0.3 → v1.0 (dokument właściciela, 2026-11-01)

Specjalistyczne narzędzie do **reverse bassu** (hybryda: syntezator + procesor audio), a nie kolejny wielki syntezator.
Nazwa na ekranie / w FL: **RE4GE**, w kodzie / folderach: `Re4GE`. Windows VST3 (Mac: później, jeśli będzie sprzedaż).
Ten plik to umowa z właścicielem: zanim coś zbudujesz, przeczytaj go. Zmiany ustaleń zapisuj tutaj.

## Cel produktu
- **Klasyczny reverse bass (2007–2012) łatwo osiągalny** (tryb CLASSIC + szablony), ale **głównie narzędzie do tworzenia
  brzmień, których wcześniej nie było** – „największa wartość wtyczki” (właściciel). Nie zamykamy jej w jednym stylu.
- **Reverse bass ≠ odwrócony sampel.** Podstawą jest SYNTEZA charakterystycznego ruchu: rytmiczne narastanie między kickami
  (późny szczyt albo pompowanie), podjazd pitchu ku kickowi, reset fazy w każdym kroku, harmoniczne otwierające się z narastaniem,
  czysty sub, cisza przed kickiem. Odwracanie (ogon kicka / sample / audio) to jedno z kreatywnych źródeł. INIT i większość
  presetów fabrycznych = synteza (jego własne OWNER - 1 / 2 też są na SYNTH).

## Pięć funkcji – znak rozpoznawczy (priorytet przed resztą)
1. **Reverse Shape Designer** – jeden ekran, krzywe AMP / PITCH / HARMONIC (filtr + drive) / PHASE na **wspólnej osi czasu**
   (każda może mieć własną długość, SYNC / MS, ANCHOR START / END, RETRIGGER / FREE / LOOP), typy odcinków LIN / EXP / LOG / S / CUSTOM.
2. **Kick-to-Reverse** – załaduj kick → automatyczny podział TRANSIENT / PUNCH / BODY / TAIL (głośność + pitch + jasność; sekcje mogą
   mieć długość 0, pewność wykrycia, granice zawsze ręcznie edytowalne, nienadpisywane bez RE-DETECT) → ROOT z wykrytej nuty →
   tryby ORIGINAL / PITCH PROCESSED (WSOLA z Ancient Drums do dużych ruchów) / DISTORTED / HYBRID (oscylatory zestrojone + zgodna faza).
   Sample zapisany w presecie / projekcie (OWN SOUND z dkd), RELINK gdy tylko ścieżka.
3. **Advanced Distortion Lab** – 2 stopnie + rysowany waveshaper (X wejście / Y wyjście, SYMMETRIC, kropka na żywo, własne krzywe w
   `Dokumenty\RE4GE\Shapes` + w presecie), typy SOFT / HARD / SATURATION / TUBE / DIODE / FOLDBACK / WAVEFOLD / BITCRUSH (prawdziwy) /
   RECTIFIER / CUSTOM, gałki DRIVE / THRESHOLD / HARDNESS / BIAS / SYMMETRY / MIX / TONE (PRE/POST) / OUTPUT + AUTO GAIN, ADAA,
   oversampling 1–16× (osobno do eksportu), PRE / POST EQ.
4. **Full Modulation Matrix** – 16 wierszy: SOURCE / DESTINATION / AMOUNT / POLARITY (UNI / BI / INV) / CURVE / VIA / ON.
   Źródła: LFO 1–4, obwiednie AMP / PITCH / HARMONIC / PHASE / DRIVE + ENV 1–2, velocity / mod wheel / AT / pitch bend / CC / key,
   makra 1–8, random (stały zarodek), step, **kick in** (wejście audio). Wiele źródeł na gałkę = suma przycięta do zakresu, pierścienie
   w kolorze źródła. Przypisywanie jak w 5CR34M 4LPH4 (ASSIGN → klik w gałkę → suwak zakresu z +/−).
5. **Reverse Bass Randomizer** – tryby FULL / OSC / PITCH / DIST / FILTER / MOD / REVERSE, AMOUNT = odległość od obecnego brzmienia,
   kłódki modułów + FREEZE gałek, losowanie w KIERUNKACH (styl → zakresy), wyrównanie głośności renderem offline, zakresy muzyczne,
   reguły wykluczające złe kombinacje, 1 klik = 1 krok UNDO. Test: 1000 losowań bez NaN / przesteru / ciszy.

## Rdzeń (musi brzmieć, zanim dojdą dodatki)
- **Źródła**: OSC 1–4 (SINE / SAW / SQUARE / TRI / PULSE / NOISE / CUSTOM rysowany / SAMPLE / KICK TAIL), każdy: ON, VOLUME, OCT (−4…+4,
  w CLASSIC / MODERN −1…+3), SEMI, FINE, PHASE, PHASE RANDOM (domyślnie 0), RESET / FREE, UNISON 1–8, DETUNE, DRIFT, SPREAD, PAN (tylko
  powyżej MONO BELOW), WIDTH, WARP, SYNC do innego OSC, PM. Generowane na żywo (także KICK TAIL liczony „wstecz w czasie”, bez renderu).
- **Pitch**: BASE NOTE, OFFSET, BEND (0–24), AMOUNT, AHDSR + CURVE, ANCHOR START / END, RANDOM (0), SNAP TARGETS / ALL,
  szablony STABLE / DROP / RISE / BOUNCE, glide / legato, priorytet nut LAST / FIRST / HIGH / LOW.
- **Phase**: START, OFFSET między warstwami, RESET / FREE, PM, PHASE DISTORTION (tryb) + WARP (ruch w czasie), RANDOM (0), **PHASE LOCK
  domyślnie ON** (warstwy + sub z jednej fazy), oscyloskop WAVE / STEP (OSC 1–4 albo całość).
- **Obwiednie**: AMP / PITCH / FILTER / PHASE / DRIVE (+ ENV 1–2): DELAY, INITIAL, ATTACK, HOLD, DECAY, SUSTAIN, RELEASE, CURVE,
  SYNC / MS, gałki logarytmiczne (0–300 ms na pół obrotu), wpisywanie liczb, ONE SHOT / LOOP / RETRIGGER / FREE, VELOCITY → AMOUNT,
  LINK TO AMP. W PATTERN release nie wchodzi pod następny kick. AMP **przed** przesterem (brud rośnie z narastaniem) + GATE / FADE po filtrach.
- **Reverse Amount** = zdefiniowany zestaw (narastanie + krzywa + szczyt + podjazd pitchu + późne otwarcie harmonicznych), nie głośność.
- **Filtry**: FILTER 1 / 2 (SERIAL / PARALLEL, PRE / POST): SVF LP / HP / BP / NOTCH, LADDER (Shared), DIODE, COMB, FORMANT;
  CUTOFF, RESO, DRIVE, MIX, KEY TRACK, ENV AMOUNT, SLOPE 12–48; cutoff z obwiedni + LFO + MIDI. HP nigdy nie tnie subu.
- **Transient**: ATTACK ±, SUSTAIN ±, MIX; CLICK (szum / tick, własny HP ~1 kHz, poza obwiednią i przesterem, opcja INTO DIST),
  CLICK AT START / PEAK / STEP.
- **Sub**: SINE / TRI / CLEAN OSC / FILTERED MAIN; VOLUME, MIX, CROSSOVER, OCTAVE, PHASE + wskaźnik zgodności + AUTO, ENV (FOLLOW AMP),
  HARMONICS (czyste sinusy 2. / 3.), SATURATION / DISTORTION (domyślnie 0 / OFF), MONO (ON), WIDTH (tylko EXPERIMENTAL), DUCKING (DEPTH + RELEASE).
- **Sekwencer** 16 / 32 kroki: ON, VEL, ACCENT (źródło w MATRIX), PROB (stały zarodek), LENGTH, GATE, PITCH; SWING, HUMANIZE, RANDOMIZE
  w kierunkach, COPY / PASTE; 1/1…1/32, triole, kropki, FREE; start HOST (BeatClock) / NOTE; **KICK MASK**; wzory w `Patterns` + 8 slotów A–H.
- **Gate** (tnie gotowy dźwięk, nie wyzwala): AMOUNT, ATTACK, RELEASE, LENGTH, RATE, SWING, SHAPE, MIX; HARD / SOFT / TRANCE /
  RHYTHMIC / CUSTOM; edytor siatka + krzywa; GATE BAND FULL / HIGHS ONLY; PRE / POST.
- **EQ**: PRE / POST / MASTER × 8 pasm (z Mirror of Souls), analizator PRE / POST, PEAK HOLD, hover z nutą, FREEZE. Low cut w PRE / POST nie tnie subu.
- **Dynamika**: COMP (SC HP, tryb OTT) → TRANSIENT → CLIPPER (SOFTNESS) → LIMITER (lookahead domyślnie 0); GR / PEAK / TRUE PEAK.
- **Stereo**: WIDTH, MONO BELOW (domyślnie ~350 Hz), SPREAD, MID / SIDE GAIN, PAN, MICRO DETUNE, STEREO PHASE, HAAS (tylko powyżej
  MONO BELOW, 0–25 ms); MONO / STEREO / DUAL MONO / M/S; korelacja + MONO CHECK.
- **Master**: OUTPUT, MASTER DRIVE, DRY / WET (tylko FX), SOFT CLIP, LIMITER, DC zawsze, PEAK / RMS / TRUE PEAK / LUFS, BYPASS, A / B
  (+ COPY, wyrównanie głośności), UNDO / REDO także dla krzywych (+ lista historii).

## Trzy tryby, dwa silniki, dwa pliki
- **CLASSIC / MODERN / EXPERIMENTAL** = punkt startowy + widok (ile modułów widać), nie osobne silniki. Zmiana trybu nie kasuje brzmienia;
  INIT trybu osobno. Preset pamięta tryb.
- Silniki **Standalone** (oscylatory) + **Integrated** (ogon kicka / sample / audio) grają naraz, wspólny tor.
- **RE4GE** (instrument: MIDI / transport / sekwencer / ręczny, sidechain opcjonalny) i **RE4GE FX** (efekt na mikserze: kick z wejścia albo
  sidechain, wykrywanie uderzeń THRESHOLD / SENSITIVITY / LEVEL-TRANSIENT / HOLDOFF / RETRIGGER, INPUT ±24 dB, gate wejścia, miernik,
  DRY ON/OFF, CAPTURE). Odwrócenie na żywo = ogon **poprzedniego** kicka (pierwszy po ciszy bez basu) albo CAPTURE.
  Ścieżki MIDI i audio osobne. Ten sam silnik i presety w obu.
- Każdy moduł ma ON / OFF (wyłączony = 0 CPU, ustawienia zostają, zapis w presecie). Domyślnie włączone: źródło, obwiednia, filtr, przester,
  sub, master.

## Routing
`TRIGGER → ŹRÓDŁA (PITCH / PHASE sterują z boku) → AMP ENV → FILTER BANK → PRE EQ → WAVESHAPER ⇄ DISTORTION → POST EQ → TRANSIENT → GATE →
SUB → DYNAMICS → STEREO → MASTER EQ → OUTPUT`. Środek toru = sloty do przeciągania; BUS A / B (każdy OSC wybiera A / B / oba, MIX A/B),
szablon MULTIBAND; automatyczne wyrównanie opóźnień między torami. Domyślnie jeden tor w tej kolejności.

## Makra i główny ekran
8 makr (BODY / WEIGHT / REVERSE / PITCH / GRIT / BITE / MOVEMENT / SPACE) = źródła w MATRIX (wiele celów, zakres, INV, krzywa, nazwa do
zmiany), działają **na wierzch** gałek (0 = to, co w modułach), automatyzowalne. Obok szybkie gałki: BASS TAIL, CLICK, TAIL SHAPE, BASS GATE,
KICK INTERACTION. **MAIN wystarcza do 80 % pracy** (lekcja z 4GE: za dużo zakładek = nieczytelne).
Nawigacja (**układ właściciela, 2026-11-01**; 14 zakładek = przeładowany interfejs i brak miejsca na kontrolki przy mniejszych rozdzielczościach):
pionowy pasek po lewej, 4 bloki robocze + SYSTEM:
- **01 SOUND**: Main, OSC, Pitch, Phase, Envelope, Shape
- **02 PROCESS**: Filter, Distortion, Waveshaper, Transient, EQ, Dynamics
- **03 MODULATION**: LFO, Mod Matrix, Sequencer, Gate
- **04 SOURCE**: Kick Tail, Audio Input, Sub, Stereo
- **05 SYSTEM**: Presets, Randomizer, Export, Settings – **dolna część paska, wizualnie oddzielona** od 4 bloków roboczych.

Zasady: (1) kliknięcie bloku **rozwija jego podmenu**, nie skacze do przypadkowej funkcji; (2) otwarty moduł zajmuje **główną przestrzeń
roboczą po prawej**; (3) **makra, preset, BPM i MASTER OUTPUT zawsze widoczne**, niezależnie od sekcji.
Wygląd: styl flat z `Shared/DESIGN.md`, akcent **czerwony**, grafit / czerń, białe opisy, duże pokrętła makr, animowany oscyloskop,
podświetlenie włączonych modułów; skalowanie 75–200 % zapamiętywane; każda gałka: nazwa, wartość, tooltip, wpisywanie, automacja, MIDI Learn,
menu (reset, learn, copy / paste, FREEZE); copy / paste modułów. Makieta: `Re4GE/Design/` (MAIN + otwarty moduł; do akceptacji właściciela). ID parametrów: jednorazowe porządkowanie przy v0.3 (właściciel nie ma projektów z automacją v0.1 / v0.2), potem zamrożone.

## Presety, eksport, host
- Kategorie: Classic / Early / Nu-Style / Modern / Raw / Euphoric / Screech Bass / Industrial / Experimental / Clean Sub / Kick Based /
  Distorted / Custom; tagi, ulubione, szukaj, podgląd klikiem, SAVE / SAVE AS / RENAME / DELETE / DUPLICATE, import / export `.re4ge` i
  `.re4gebank` (z samplami), presety modułów. Preset i projekt zapisują WSZYSTKO (krzywe, matrix, makra, routing, wzory, sample).
  Zawartość fabryczna: 2–3 na kategorię z pomiarów jego wzorców, reszta dokładana uchem.
- Eksport offline: WAV 16 / 24 / 32f, 44.1–192 kHz; ONE SHOT / LOOP (bez szwu) / PATTERN / MIDI / DRY / WET / NORMALIZE / TAIL do ciszy /
  BATCH (presety × nuty); przeciąganie pliku z okna do FL.
- Host: BPM, pozycja, play / stop (BeatClock), automatyzacja wszystkiego, stan projektu niezależny od presetów, sanitizeState, MIDI,
  sidechain gdy dostępny. **ID parametrów: jednorazowe uporządkowanie w v0.3, potem zamrożone na zawsze** (lista ID w repo + test).
  Nazwy w FL z prefiksami modułów.
- DAW do testów: FL (główny), Ableton, Cubase, REAPER, Studio One – pluginval w CI + checklista ręczna; nie zakładać, że jeden = wszystkie.

## Jakość DSP (wymagania twarde)
Niezależność od SR (44.1–192), oversampling 1–16× tylko w sekcjach nieliniowych (oversamplery tworzone raz), polyBLEP + ADAA
(BITCRUSH celowo aliasuje), DC 5 Hz zawsze, ochrona denormali, wygładzanie dopasowane do typu (triggery / reset fazy NIGDY), wewnętrzne
kawałki po 32 próbki (wynik niezależny od bufora), zero alokacji / plików / blokad / GUI w audio, wyłączone moduły = 0 CPU, analizator z
FIFO poza wątkiem audio, podglądy liczone w wątku tła (nie w GUI).

## Kod
`Re4GE/Source/{DSP,GUI,Presets,Parameters,Utilities}`, `Re4GE/Tests`, `Re4GE/Assets`. **DSP, GUI i pliki nigdy w jednym komponencie.**
Komponenty: Oscillator / Pitch / Phase / Envelope / Filter / WaveShaper / Distortion / Sub / Sample / Trigger / Modulation / Sequencer /
Dynamics / Stereo + PresetManager / Analyzer. Klocki wspólne (modulacja, dynamika, waveshaper, ADAA, ladder, analizator, BeatClock,
OWN SOUND, przeglądarka) w `Shared/`. Każdy komponent: test osobno + RenderTest razem.

## Testy (build nie przechodzi, gdy któryś pada)
Każda gałka coś robi (min vs max), NaN / ≤ 0 dBFS, MIDI on / off / legato / priorytety, host sync ±1 ms + zmiana tempa, preset i projekt
→ identyczny dźwięk co do próbki, stare stany, import formatów, brak sampla (RELINK), aliasing 1× vs 16×, automacja bez trzasków (także wielu
naraz), mono compatibility, CPU (3 scenariusze + max), długa sesja (pamięć), pluginval, randomizer ×1000, bypass (FX przepuszcza kick co do
próbki), stop / start / pętla / przeskok, SR 44.1–192, bufory 1…4096 i zmienne, mono / stereo, wartości skrajne, szybka zmiana presetów, ID.
Ostateczny test: **ucho właściciela + A/B z jego samplami** (Bass_15 / 16, FHSP, kick_hardstyle_28 / 84).

## Kolejność budowy (propozycja)
0. ✅ **(v0.3, 2026-11-01)** Przebudowa bez nowych funkcji: podział na komponenty, kawałki 32 próbki, ID uporządkowane i zamrożone, ogon kicka bez skoków CPU
   + gałki na żywo (render rozłożony na ciszę przed basem, 2 bufory; „generowanie wstecz w czasie” odrzucone, bo ROOM i dzwonienie filtrów
   potrzebują przyszłości ogona), testy podstawowe. Brzmienie identyczne z v0.2 (±1 LSB).
1. Rdzeń brzmienia: OSC 1–4 + unison / sync, pitch, phase (LOCK), AMP + Reverse Amount, filtr, 2 × przester + ADAA, sub, stereo, 8 makr,
   MAIN. **Bramka: A/B z jego samplami, aż powie „jest”.**
   **Krok 1 – projekt (2026-11-01, wysłany właścicielowi):** pomiar Bass_15/16 vs nasze: góra 2,5–6 kHz −20 vs −48…−65 dB, gęstość (flatness)
   0,18–0,30 vs 0,002, narastanie przez ~cały beat (od −33 dB, szczyt ~240 ms) vs cisza do 120 ms, 150–400 Hz +5 dB u niego.
   Tor: warstwy → PRE filtr (ENV + KEY TRACK) → STAGE 1 → DC → LOW CUT → BELL → STAGE 2 (nowy) → HIGH CUT → SUB → STEREO → OUT.
   OSC 1 = obecne źródło (SYNTH / TAIL / SAMPLE) + osc1_level; OSC 2–4: SINE / TRI / SAW / SQUARE / PULSE / NOISE, LEVEL (OFF), OCT, SEMI,
   FINE, PHASE, WIDTH, SYNC (do fazy nuty), PM; w kanałach L / R po jednej rozstrojonej kopii (tanio). PHASE RESET / FREE.
   8 makr na stałych mapowaniach (matrix w kroku 4). Nowe ID tylko dopisywane na końcu `Tests/param_ids.txt`.
   **Zrobione (v0.4, 2026-11-01):** to wszystko + nowy ekran (NAV / MAIN / makra + MASTER) + preset CLASSIC - Dense Rise (pasma ±2–4 dB,
   gęstość 0,08 vs 0,18–0,30). Czeka na ucho właściciela (A/B). **v0.5:** CLICK, SUB OCT / HARMONICS / SAT, GLIDE, SMEAR (gęstość 0,08 → 0,10; dalsze podnoszenie
   szumem / FM / FOLD daje max ~0,13 i rozjaśnia – reszta czeka na ucho właściciela). ADAA przeniesione do kroku 3 (Distortion Lab; teraz 4× oversampling).
2. Reverse Shape Designer.
3. Distortion Lab (waveshaper rysowany, typy, oversampling 1–16×).
4. Modulation Matrix + LFO 1–4.
5. Kick-to-Reverse (ekstrakcja, tryby, OWN SOUND) + RE4GE FX (audio / sidechain).
6. Randomizer, sekwencer + gate, EQ, dynamika, presety z kategoriami, eksport.
7. Dodatki (morph A/B, waveform compare, reference analyzer, presety modułów).
Szczególnie osobne testy jakości dla: ekstrakcji ogona, zaawansowanej modulacji, wieloetapowego przesteru.

## Prompt właściciela (zasady pracy, skrót)
Pracuj przyrostowo, moduł po module; przed modułem wyjaśnij jego DSP i tor; kompletne, kompilujące się pliki; nigdy nie twierdź, że coś
działa, jeśli nie ma DSP i testu; po każdej zmianie kompiluj, testuj, naprawiaj; nie zastępuj działającego DSP atrapą; nie psuj tego, co
działa; prowadź dokumentację parametrów, zakresów, routingu i zależności; przy niejasności – opisz ją i zaproponuj precyzyjne rozwiązanie
przed kodowaniem.
