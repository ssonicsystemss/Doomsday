<!-- Owner specification of module 06 (2026-11-03), collected from chat (text + tables transcribed from screenshots). Plan: PLAN.md 1f.
> **Zasada nadrzędna:** 4GE ma dokładnie 8 MASTER MACROS (IMPACT, BODY, TAIL, PITCH, GRIT, SCREAM, WEIGHT, CHAOS). Makra / kontrolery opisane w tym module są LOKALNE i sterowane przez Macro Mapping (M09) – patrz `RULES.md`.
     Claude's notes / open questions are at the end (marked NOTES). -->
# 4GE — MODULE 06: FILTER, RESONANCE & SCREECH ENGINE

Cel modułu: stworzenie profesjonalnego silnika filtracji, rezonansu i generowania screechy, który pozwoli budować zarówno subtelne, klasyczne brzmienia Early Hardstyle, jak i agresywne, metaliczne, wysokie screechy charakterystyczne dla Rawstyle i współczesnego hardstyle'u.
To nie może być zwykły zestaw filtrów z pokrętłem Resonance. Moduł ma być pełnoprawnym systemem syntezy i przetwarzania dźwięku, z niezależnymi źródłami wzbudzenia rezonansu, własną obwiednią, modulacją, routingiem i kontrolą stabilności.
Najważniejsza zasada: screech nie może być wyłącznie próbką ani filtrem high-pass. Musi dać się go syntetyzować, kształtować i modulować od podstaw.

## 1. Architektura modułu
Trzy niezależne silniki: 1. FILTER ENGINE — wieloetapowa filtracja sygnału. 2. RESONANCE ENGINE — generowanie i kontrolowanie rezonansów. 3. SCREECH ENGINE — synteza charakterystycznych hardstyle'owych screechy.
Każdy silnik działa niezależnie, ale umożliwia przesyłanie sygnału między pozostałymi silnikami.
Schemat ogólny: Źródła (Oscillators / Noise / Layers / Transients) → FILTER ENGINE (filtracja i kształtowanie widma) + RESONANCE ENGINE (rezonatory i wzbudzanie) → SCREECH ENGINE (synteza screechy i kontrola charakteru) → Distortion Engine (Module 05) → Mixer → Output.
Każdy z trzech silników musi mieć możliwość pracy: niezależnej, równoległej, szeregowej, jako insert dla wybranej warstwy, jako procesor grupy PUNCH / BODY / TAIL / SUB / TEXTURE.

## 2. FILTER ENGINE — zaawansowana filtracja
### 2.1. Typy filtrów
| Typ | Zastosowanie |
|---|---|
| Low Pass | Usuwanie wysokich częstotliwości |
| High Pass | Usuwanie niskich częstotliwości |
| Band Pass | Izolowanie wybranego pasma |
| Notch | Wycinanie wybranego pasma |
| Ladder | Charakterystyczna filtracja drabinkowa |
| Diode Ladder | Bardziej agresywna filtracja |
| State Variable | Elastyczna filtracja multimodalna |
| Comb | Metaliczne i grzebieniowe rezonanse |
| Formant | Formowanie samogłoskowych charakterów |
| All Pass | Kontrola fazy i specjalne efekty |
Każdy algorytm musi mieć własną implementację DSP. Nie wolno udawać różnych typów filtrów poprzez zmianę jednego parametru w tym samym filtrze.
### 2.2. Parametry pojedynczego filtra
| Parametr | Zakres | Opis |
|---|---|---|
| ENABLE | ON/OFF | Aktywacja filtra |
| TYPE | Lista typów | Rodzaj filtra |
| CUTOFF | 20 Hz–20 kHz | Częstotliwość odcięcia |
| RESONANCE | 0–100% | Intensywność rezonansu |
| DRIVE | 0–100% | Nasycenie wejścia filtra |
| MIX | 0–100% | Dry/Wet |
| SLOPE | 6–48 dB/oct | Stromość zbocza, gdy obsługiwana |
| KEY TRACK | 0–200% | Reakcja cutoff na wysokość MIDI |
| ENV AMOUNT | -100–100% | Wpływ obwiedni |
| VELOCITY | -100–100% | Reakcja na velocity |
| STEREO MODE | Mono/Stereo/M-S | Tryb przetwarzania |
Dla filtrów, które nie obsługują danego parametru, interfejs ma odpowiednio go wyłączyć. Nie wyświetlać działających na niby kontrolek.
### 2.3. Wieloetapowy system filtrów
Minimum 4 niezależne instancje filtrów, rozszerzalne do 8. Każda ma: własny typ, własne parametry, własną modulację, własne wejście i wyjście, możliwość przypisania do grupy, możliwość zmiany pozycji w łańcuchu.
Routing: SERIAL — filtr za filtrem; PARALLEL — równoległe przetwarzanie; SPLIT — podział na pasma i niezależna filtracja; MIXED — kombinacja.
Przy miksowaniu ścieżek równoległych należy kontrolować poziom i fazę, aby nie powodować przypadkowego podbicia głośności ani niepożądanych wygaszeń.
### 2.4. Obwiednia filtra
Niezależna obwiednia ADSR: ATTACK 0–2000 ms, DECAY 0–5000 ms, SUSTAIN 0–100%, RELEASE 0–5000 ms, AMOUNT -100–100%, CURVE Linear / Exponential / Logarithmic.
Dodatkowo: wybór krzywej obwiedni, odwrócenie obwiedni, synchronizacja z długością kicka, przypisanie obwiedni do cutoff, resonance i drive.
Przykład: po uderzeniu kicka filtr otwiera się bardzo wysoko, a następnie szybko zamyka, pozostawiając ciemniejszy ogon.

## 3. RESONANCE ENGINE — silnik rezonansu
Osobny generator i procesor rezonansu. Nie może być jedynie dodatkiem do filtra.
### 3.1. Źródła wzbudzenia
Sine, Saw, Square, Noise, Impulse, External Signal, Oscillator Excitation. Źródło wzbudza rezonator, który następnie generuje własną odpowiedź częstotliwościową.
### 3.2. Tryby rezonatora
| Tryb | Charakter |
|---|---|
| CLEAN | Czysty, kontrolowany rezonans |
| METALLIC | Metaliczny, dzwoniący |
| SCREECH | Agresywny, wysokotonowy |
| RUBBER | Gumowy, sprężysty |
| FORMANT | Rezonans przypominający głos |
| CUSTOM | Użytkownik tworzy własną charakterystykę |
### 3.3. Parametry rezonatora
| Parametr | Zakres | Opis |
|---|---|---|
| FREQUENCY | 20 Hz–20 kHz | Częstotliwość rezonansu |
| Q | 0.1–100 | Szerokość rezonansu |
| DECAY | 1–5000 ms | Czas wygaszania |
| AMOUNT | 0–100% | Intensywność |
| DAMPING | 0–100% | Tłumienie wysokich częstotliwości |
| EXCITATION | 0–100% | Siła wzbudzenia |
| FEEDBACK | 0–95% | Sprzężenie zwrotne |
| PITCH TRACK | 0–200% | Śledzenie wysokości dźwięku |
| MIX | 0–100% | Dry/Wet |
FEEDBACK musi być stabilizowany. Wartość 95% nie może powodować niekontrolowanego wzrostu sygnału, DC offsetu ani uszkadzających pików.
### 3.4. Resonance Pitch Envelope
Rezonans ma własną obwiednię wysokości. Przykład: początek 8000 Hz, po 50 ms 3500 Hz, po 250 ms 1800 Hz — opadające metaliczne i screechowe ogony.
Parametry: START FREQUENCY, END FREQUENCY, ATTACK, DECAY, SUSTAIN, RELEASE, CURVE, AMOUNT. Wysokość rezonansu ustawiana ręcznie lub zsynchronizowana z nutą MIDI.

## 4. SCREECH ENGINE — właściwy generator screechy
Najważniejsza część modułu pod kątem Rawstyle i nowoczesnego hardstyle'u. Ma tworzyć wysokie, agresywne, metaliczne i modulowane dźwięki: warstwa screechowa w ogonie kicka, osobny element TAIL, tekstura w grupie TEXTURE, warstwa wypełniająca przestrzeń między uderzeniem a ogonem, samodzielny dźwięk do eksperymentalnych kicków.
Nie projektować go jako jednego oscylatora z filtrem. Kilka metod syntezy i możliwość ich łączenia.
### 4.1. Tryby generowania
| Tryb | Mechanizm | Charakter |
|---|---|---|
| TONAL | Oscylator + modulowana filtracja | Czysty, wyrazisty screech |
| METALLIC | Rezonatory i harmoniczne | Metaliczny, agresywny |
| FORMANT | Filtry formantowe | Głosowy, krzyczący |
| FM SCREECH | Modulacja częstotliwości | Ostry, złożony |
| NOISE SCREECH | Szum wzbudzający rezonatory | Brudny, teksturalny |
| FEEDBACK | Kontrolowany feedback | Dziki, surowy |
| HYBRID | Łączenie kilku metod | Maksymalna elastyczność |
| CUSTOM | Konfiguracja użytkownika | Własny charakter |
Każdy tryb musi korzystać z odpowiedniego algorytmu. Zmiana trybu nie może być tylko zmianą presetu parametrów jednego generatora.
### 4.2. Oscillator Section
Minimum 2 niezależne oscylatory, docelowo 4. Przebiegi: Sine, Saw, Square, Triangle, Pulse, Custom Wavetable.
| Parametr | Zakres |
|---|---|
| WAVEFORM | Lista przebiegów |
| LEVEL | 0–100% |
| PITCH | -48 do +48 semitonów |
| FINE | -100 do +100 centów |
| PHASE | 0–360° |
| DETUNE | 0–100% |
| FM AMOUNT | 0–100% |
| RING MOD | 0–100% |
| PAN | -100 do +100 |
| ENABLE | ON/OFF |
FM AMOUNT musi rzeczywiście sterować modulacją częstotliwości pomiędzy wybranymi oscylatorami; wybór źródła modulacji. Dla każdego oscylatora: źródło FM, kierunek modulacji, stosunek Carrier/Modulator, poziom modulacji, synchronizacja fazy.
### 4.3. Screech Filter Section
Minimum 2 niezależne filtry z algorytmów Filter Engine: FILTER TYPE, CUTOFF, RESONANCE, DRIVE, KEY TRACK, ENV AMOUNT, MIX.
Szczególnie ważna modulacja cutoff — screech zmienia charakter w czasie, nie nieruchomy zapętlony ton. Przykład: początkowo wysoki ostry dźwięk, gwałtowne otwarcie filtra, stopniowe zejście częstotliwości, zwiększenie rezonansu w określonym punkcie, wygaszenie w końcowej części ogona.
### 4.4. Formant Engine
FORMANT A, E, I, O, U, FORMANT MORPH, FORMANT WIDTH, FORMANT RESONANCE. Bank filtrów formantowych z kontrolowanymi częstotliwościami i poziomami rezonansów. MORPH płynnie przechodzi między samogłoskami (przykład A → E → I → O → U); automatyczne przejście przez wybrany zakres lub ręczne ustawienie pozycji.
### 4.5. Feedback Engine
Agresywne, nieprzewidywalne brzmienia przez kontrolowane sprzężenie zwrotne.
| Parametr | Zakres |
|---|---|
| FEEDBACK | 0–95% |
| FEEDBACK TONE | 0–100% |
| FEEDBACK FILTER | LP / HP / BP |
| FEEDBACK DELAY | 0–100 ms |
| FEEDBACK DRIVE | 0–100% |
| FEEDBACK MIX | 0–100% |
| STABILITY | 0–100% |
Feedback może być realizowany przez opóźnioną pętlę sygnału, ale z ograniczeniem poziomu i zabezpieczeniami przed niestabilnością: kontrola maksymalnej amplitudy, ograniczenie narastania energii, DC blocker, kontrola NaN i nieskończoności, automatyczne obniżenie feedbacku przy wykryciu niestabilności. Niewielka zmiana parametru nie może powodować niekontrolowanego wzrostu sygnału.

## 5. SCREECH ENVELOPE
Własna obwiednia amplitudy i osobna obwiednia modulacji.
### 5.1. Amplitude Envelope
ATTACK 0–2000 ms, DECAY 0–5000 ms, SUSTAIN 0–100%, RELEASE 0–5000 ms, CURVE Linear / Exp / Log, HOLD 0–2000 ms.
### 5.2. Modulation Envelope
Cele: pitch, FM Amount, filter cutoff, resonance, feedback, formant morph, drive, oscillator detune. Każdy cel z niezależną głębokością.
### 5.3. Pitch Envelope
-48 do +48 semitonów względem nuty bazowej. START PITCH, END PITCH, PITCH AMOUNT, ATTACK, DECAY, RELEASE, CURVE, PITCH MODE: Absolute / Relative / MIDI Tracked. Synchronizacja z Pitch Engine (Module 02) na żądanie; bez niekontrolowanego konfliktu dwóch źródeł pitch.

## 6. SCREECH TIMING — pozycja w kicku
| Parametr | Zakres |
|---|---|
| START POSITION | 0–2500 ms |
| LENGTH | 1–2500 ms |
| FADE IN | 0–1000 ms |
| FADE OUT | 0–2000 ms |
| PRE-ROLL | 0–250 ms |
| SYNC TO TAIL | ON/OFF |
| AUTO LENGTH | ON/OFF |
W SYNC TO TAIL długość screechy = długość TAIL. Screech nie wykracza poza długość wygenerowanego kicka, chyba że użytkownik aktywuje tryb niezależnego ogona.
Przykład (nie sztywne ograniczenia): PUNCH 0–30 ms, BODY 30–180 ms, SCREECH 180–650 ms, TAIL 650–1200 ms.

## 7. MODULATION MATRIX — integracja z resztą 4GE
Źródła: Pitch Envelope, Amplitude Envelope, Filter Envelope, LFO, Velocity, Macro Controls, Randomizer, Sequencer, Screech Envelope, Resonance Envelope.
Cele: Filter Cutoff, Filter Resonance, Filter Drive, Resonance Frequency, Resonance Q, Feedback, Screech Pitch, Oscillator Pitch, FM Amount, Formant Morph, Screech Level, Distortion Drive.
Każde połączenie: SOURCE, DESTINATION, AMOUNT (-100–100%), POLARITY, CURVE, ENABLE. Widoczne w Modulation Matrix, zmiana głębokości słyszalna w czasie rzeczywistym.

## 8. ROUTING I INTEGRACJA
Przypisanie każdego silnika do: PUNCH, BODY, TAIL, SUB, TEXTURE, MASTER.
Warianty: FILTER → DISTORTION, DISTORTION → FILTER, FILTER → RESONANCE, RESONANCE → SCREECH, SCREECH → DISTORTION, SCREECH → TAIL, SCREECH → MASTER + własny łańcuch.
Module 05 udostępnia swoje instancje distortion jako punkty routingu. Module 06 nie implementuje drugiego, niezależnego systemu distortion.

## 9. MAKRA
| Makro | Działanie |
|---|---|
| SCREAM | Zwiększa agresywność screechy, rezonans i wybrane harmoniczne |
| RESONANCE | Zwiększa intensywność rezonatorów |
| METALLIC | Wzmacnia metaliczny charakter |
| FORMANT | Zwiększa udział filtracji formantowej |
| FEEDBACK | Steruje sprzężeniem zwrotnym w bezpiecznym zakresie |
| SHARP | Rozjaśnia brzmienie i zwiększa jego ostrość |
| MOVEMENT | Zwiększa głębokość wybranych modulacji |
Istniejący Macro System, działanie konfigurowalne. SCREAM ma zaprojektowaną mapę zależności uwzględniającą stan silnika (np. przy wyłączonym Feedback nie udaje zwiększania jego działania).

## 10. GUI
Blok PROCESSING, dostęp do screechy również przez SYNTHESIS.
10.1. Wizualizacje (rzeczywisty stan DSP): Filter Response, Resonance Display, Screech Waveform, Screech Spectrum, Envelope Display, Routing Display.
10.2. Obsługa: presety poszczególnych silników, cały moduł jako jeden preset, A/B Compare, Undo/Redo, Copy/Paste, Randomize z blokowaniem parametrów, MIDI Learn, automatyzacja DAW, tooltips.

## 11. PRESET SYSTEM
| Styl | Preset | Charakter |
|---|---|---|
| EARLY | Classic Resonance | Subtelny, czysty rezonans |
| EARLY | Oldschool Screech | Klasyczny, prosty screech |
| MODERN | Wide Screech | Szeroki, jasny screech |
| MODERN | Melodic Tail | Melodyjny ogon |
| RAWSTYLE | Aggressive Metal | Metaliczny i agresywny |
| RAWSTYLE | Feedback Terror | Mocno przesterowany feedback |
| EXPERIMENTAL | Formant Machine | Głosowe tekstury |
| EXPERIMENTAL | Broken Resonator | Niestandardowe rezonanse |
Presety to punkty startowe. Zmiana stylu kicka nie blokuje dostępu do parametrów ani nie wymusza trybu screechy.

## 12. WYMAGANIA DSP
12.1 Filtry: stabilne IIR/FIR, poprawne współczynniki dla 44.1 / 48 / 88.2 / 96 / 192 kHz, zabezpieczenia przy ekstremalnym Q, smoothing cutoff / resonance / drive / mix, brak kliknięć przy automatyzacji, zarządzanie stanem filtrów przy zmianie presetów.
12.2 Rezonatory i feedback: limit energii w pętli, DC, NaN / Inf, stabilność przy wysokim Q, poprawna zmiana częstotliwości rezonansu w trakcie dźwięku.
12.3 Screech: antyaliasing oscylatorów, band-limited wavetable, FM z kontrolą aliasingu, smoothing pitch i modulacji, brak niekontrolowanego wzrostu amplitudy, mono / stereo, determinizm przy seedzie.
12.4 Wydajność: brak alokacji, blokad i operacji plikowych w callbacku; bezpieczne przekazywanie parametrów GUI → DSP; niezależny stan głosów i warstw; reset przy nowym kicku; optymalizacja CPU. Oversampling współpracuje z Module 05 — bez kilku nakładających się kosztownych etapów.

## 13. TESTY
| Test | Oczekiwany rezultat |
|---|---|
| Zmiana cutoff | Płynna, słyszalna zmiana barwy |
| Zmiana resonance | Rzeczywisty wzrost rezonansu |
| Przełączanie typów filtrów | Każdy typ ma odmienną charakterystykę |
| Wysokie Q | Brak niekontrolowanej niestabilności |
| FM Screech | Słyszalna modulacja częstotliwości |
| Formant Morph | Płynne przechodzenie między formantami |
| Feedback | Agresywny dźwięk bez runaway |
| Pitch Envelope | Poprawne opadanie lub wzrost wysokości |
| Screech Timing | Poprawne pozycjonowanie w kicku |
| Layer Routing | Poprawne przypisanie do grup |
| Distortion Routing | Poprawna integracja z Module 05 |
| Preset Recall | Identyczne odtworzenie parametrów |
| Randomize | Losowanie bez naruszania zablokowanych parametrów |
| Offline Render | Zgodność z renderowaniem realtime |
Warunki zaliczenia: 1. wszystkie typy filtrów; 2. min. 4 instancje filtrów; 3. niezależny rezonans; 4. screech bez zewnętrznej próbki; 5. min. 4 metody syntezy screechy; 6. obwiednie pitch / amp / modulacji; 7. integracja z Module 02–05; 8. routing bez skoków poziomu; 9. wszystkie parametry automatyzowalne; 10. testy stabilności i wydajności.

## 14. Ostateczny rezultat
Użytkownik może stworzyć: klasyczny rezonujący kick Early Hardstyle, nowoczesny jasny kick z melodyjnym ogonem, agresywny Rawstyle z metaliczną screechą, eksperymentalny kick z formantami / FM / feedbackiem, własne screechy od zera bez sampli.
Najważniejszy test jakości: pusty projekt 4GE → podstawowy kick → Screech Engine → tryb FM lub METALLIC → własna modulowana screecha wyłącznie z syntezy wewnętrznej.
Zakaz: atrapy parametrów, kontrolki bez wpływu na DSP, tryby korzystające z tego samego algorytmu pod inną nazwą.

---
## NOTES (Claude, 2026-11-03) – decyzje techniczne i pytania na koniec
- Filtry TPT / ZDF (stabilne przy modulacji); ladder / diode z nasyceniem w pętli; SPLIT na zwrotnicach Linkwitz-Riley (`Shared` Crossover3); SLOPE nieaktywny dla Comb / Formant / All Pass.
- Rezonatory: bank filtrów rezonansowych + falowody (Karplus-Strong) z interpolacją ułamkową; obwiednie wysokości liczone w oktawach.
- Feedback: pętla opóźnienia + filtr + nasycenie; stały ogranicznik (STABILITY go nie wyłącza), DC, zerowanie przy NaN, strażnik energii obniżający feedback.
- Jedna strefa oversamplingu na głos (screech + filtry + rezonatory + distortion z M05), QUALITY realtime / export.
- Screech tryby = osobne ścieżki z klocków M03 / M04 / pkt 2–3; robimy wszystkie 8 (warunek „min. 4” = dolna granica).
- Pitch screecha: jedna suma w półtonach (nuta + pitch engine przy SYNC + obwiednia screecha), clamp.
- MIDI Learn – własny (prawy klik na gałce).
- PYTANIA: (1) PRE-ROLL – ciche rozkręcenie silnika przed START czy słyszalne wejście wcześniej? (2) dodać SYNC TO BODY END (screech startuje z końcem BODY – układ z przykładu w pkt 6)? (3) makro SCREAM modułu 06 = to samo co główne makro SCREAM z 8 makr 4GE, czy osobne?

---
## DODATEK (właściciel, 2026-11-04): SCREECH START MODE
Odpowiedź na pytanie o SYNC TO BODY END: TAK – jako osobny system SCREECH START MODE, a nie zwykłe automatyczne przesuwanie parametru START.
System określa, kiedy generator screechu rozpoczyna pracę względem pozostałych elementów kicka.
### 1. Dostępne tryby
| Tryb | Działanie |
|---|---|
| MANUAL | Użytkownik ręcznie ustawia START |
| SYNC TO BODY END | Screech rozpoczyna się dokładnie w momencie zakończenia BODY |
| SYNC TO BODY END - OFFSET | Screech rozpoczyna się po zakończeniu BODY, z regulowanym opóźnieniem |
| SYNC TO TAIL | Screech synchronizuje się z początkiem TAIL |
| SYNC TO KICK | Screech rozpoczyna się względem początku całego kicka |
| HOST SYNC | Opcjonalna synchronizacja do pozycji rytmicznej DAW |
Domyślny tryb: MANUAL.
### 2. SYNC TO BODY END — szczegółowa logika
Schemat: `0 ms | PUNCH | BODY | ← BODY END → SCREECH | KICK END` – screech rozpoczyna się automatycznie w punkcie BODY END.
Zasada: `SCREECH_START = BODY_START + BODY_DURATION + SCREECH_OFFSET`
Przykład: BODY duration 350 ms, SCREECH offset 0 ms → SCREECH start 350 ms. Po zmianie BODY duration na 500 ms SCREECH start automatycznie przesuwa się na 500 ms – użytkownik nic nie przestawia ręcznie.
