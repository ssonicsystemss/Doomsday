<!-- Owner specification of module 09 (2026-11-03), from his PDF (original: Specs/MODULE09_owner.pdf). Plan: PLAN.md 1i. Claude's notes at the end. -->
# 4GE — MODULE 09: MODULATION MATRIX & ADVANCED MODULATION ENGINE

Cel: centralny system modulacji całej wtyczki – dynamiczne sterowanie parametrami wszystkich silników (oscylatory, pitch envelope, warstwy, distortion, filtry, screech, kompresja, transient shaper, limiter). Nie wizualna macierz – każde połączenie rzeczywiście wpływa na DSP.
Wymóg nadrzędny: każdy cel modulacji ma działające połączenie ze wspólnym systemem parametrów 4GE. Zakaz atrap zmieniających tylko grafikę.
Zakres: źródła, macierz routingu, LFO, sekwencer krokowy, randomizer, makra, MIDI / host, podgląd sygnałów, bezpieczeństwo DSP, testy.
| Priorytet | Wymaganie |
|---|---|
| MUST | Modulacja w czasie rzeczywistym na rzeczywistych parametrach DSP |
| MUST | Każde połączenie: głębokość, polaryzacja, krzywa, zakres |
| MUST | Brak kliknięć, zipper noise, skoków i niestabilności feedbacku |
| MUST | Wynik deterministyczny w trybach wymagających powtarzalności |
| SHOULD | Min. 32 sloty macierzy, docelowo 64+ |
| SHOULD | Drag-and-drop i szybki podgląd routingu |

## 1. Architektura
Jeden centralny Modulation Engine (nie osobne systemy per moduł).
1.1 Przepływ: źródło → normalizacja → slot (polaryzacja, krzywa, depth, offset, smoothing) → clamp do bezpiecznego zakresu → suma z innymi źródłami → parametr docelowy → DSP. Wewnętrznie domena znormalizowana (-1…+1 lub 0…1); mapowanie na Hz / dB / ms / % / półtony dopiero w warstwie parametru.
1.2 Łączenie źródeł: domyślnie suma + clamp do legalnego zakresu; tryby ADD (domyślny), MULTIPLY, MAX, MIN, REPLACE (jawny priorytet / kolejność, niezależny od wątków). Wartość bazowa niezależna od modulacji – wyłączenie slotu przywraca bazę bez zmiany zapisu. Zakaz NaN, inf, ujemnego czasu i innych nielegalnych wartości.

## 2. Źródła modulacji
Każde: stabilny ID, nazwa, zakres, częstotliwość aktualizacji, stan aktywności.
| Źródło | Opis |
|---|---|
| Pitch Envelope | Obwiednia wysokości kicka – dla filtrów, oscylatorów, distortion, screech… |
| Amplitude Envelope | Obwiednia głośności sygnału lub grupy PUNCH/BODY/TAIL/SUB/TEXTURE |
| Filter Envelope | Dedykowana, niezależna od amplitudy |
| LFO 1–8 | Min. 8 niezależnych: przebieg, tempo, faza, retrigger |
| Velocity | Z MIDI / hosta; bez velocity – jawna wartość domyślna |
| Key Tracking | Wysokość nuty: root note, zakres, krzywa |
| Macros | Wartości makr |
| Step Sequencer | Aktualny krok (kierunek, prawdopodobieństwo, retrigger) |
| Random Source | FIXED / PER TRIGGER / FREE RUN |
| Audio Follower | Obwiednia z sygnału: wejście, PUNCH, BODY, TAIL, SUB, grupa, MASTER |
| MIDI CC | Numer CC i zakres |
| Host Automation | Przez standardowy system parametrów |
2.1 Audio Follower: detekcja PEAK / RMS / HYBRID; ATTACK, RELEASE, SENSITIVITY, INPUT GAIN, THRESHOLD, RANGE, SMOOTHING, SOURCE (INPUT, PUNCH, BODY, TAIL, SUB, TEXTURE, GROUP, MASTER). Stabilny przy ciszy (floor / gate). Cykle (follower za procesorem, który sam moduluje) ograniczone lub zablokowane.

## 3. Modulation Matrix
Min. 32 sloty (produkcyjnie), dane rozszerzalne do 64+ bez przebudowy DSP.
| Pole | Zachowanie |
|---|---|
| ENABLE | Wł. / wył. bez usuwania ustawień |
| SOURCE | Wybór źródła |
| DESTINATION | Konkretny parametr 4GE |
| DEPTH | -100…+100% (opcjonalnie w jednostkach celu) |
| POLARITY | BIPOLAR, UNIPOLAR POSITIVE, UNIPOLAR NEGATIVE |
| CURVE | LINEAR, EXPONENTIAL, LOGARITHMIC, S-CURVE, INVERTED, CUSTOM |
| OFFSET | Przesunięcie przed depth |
| SMOOTHING | Przeciw kliknięciom / zipper |
| LAG | Opcjonalna bezwładność (nie zastępuje smoothingu parametru) |
| MODE | ADD, MULTIPLY, MAX, MIN, REPLACE |
| MUTE / SOLO | Mute pojedynczego routingu; Solo – odsłuch / podgląd jednego połączenia |
| LABEL | Nazwa użytkownika, np. „LFO → Screech Pitch” |
3.1 Cele rejestrowane centralnie: unikalny ID, nazwa, jednostka, min / max, skala (linear / log / discrete), domyślna, smoothing, flaga „bezpieczna w realtime”.
3.2 Grupy celów:
- OSCILLATORS: pitch, fine tune, phase, waveform position, pulse width, detune, unison spread, level, FM amount.
- NOISE: level, color, filter cutoff, resonance, pitch, drive, stereo width, decay.
- TRANSIENT: attack level, click level, click pitch, click length, transient mix, position offset.
- LAYERS: layer level, pan, pitch, fine, length, start offset, filter cutoff, drive, width, group level.
- PITCH / ENVELOPES: pitch amount, pitch decay, envelope time, envelope curve, retrigger (gdzie bezpieczne).
- FILTER / RESONANCE: cutoff, resonance, drive, key tracking, envelope amount, resonance decay, excitation level.
- SCREECH: pitch, formant, resonance, feedback (w bezpiecznych granicach), distortion send, level, decay, stereo width.
- DISTORTION: drive, bias, symmetry, fold amount, mix, input gain, output gain, filter cutoff, multiband drive.
- EQ: band gain, frequency, Q (gdzie smoothing ciągły jest obsługiwany).
- DYNAMICS: threshold, ratio, attack, release, knee, mix, sidechain sensitivity, transient attack / sustain.
- CLIPPER / LIMITER: drive, ceiling, release, input gain, mix – safety ceiling nigdy nie omijany modulacją.
- GLOBAL: master level (w bezpiecznych limitach), group levels, stereo width, globalna intensywność z makr.

## 4. LFO Engine
Min. 8 LFO (sygnał modulacyjny, nie audio), każdy do wielu celów.
4.1 Przebiegi: SINE, TRIANGLE, SAW UP, SAW DOWN, SQUARE, PULSE, SAMPLE & HOLD, SMOOTH RANDOM; CUSTOM (min. 16 punktów, interpolacja LINEAR / CUBIC); PULSE WIDTH / DUTY dla SQUARE i PULSE; zmiana kształtu wygładzona / zsynchronizowana.
4.2 Parametry:
| Parametr | Zakres / opis |
|---|---|
| RATE | 0.01–40 Hz (FREE); rozszerzalny jeśli stabilny |
| SYNC | OFF / HOST SYNC |
| DIVISION | 1/1, 1/2, 1/4, 1/8, 1/16, 1/32 + kropkowane i triole |
| PHASE | 0–360° |
| RETRIGGER | OFF, ON NOTE, ON KICK TRIGGER, ON BAR |
| MODE | FREE RUN, ONE SHOT, LOOP |
| POLARITY | BIPOLAR / UNIPOLAR |
| FADE IN | 0–5000 ms lub tempo |
| FADE OUT | 0–5000 ms lub tempo |
| SMOOTHING | Wygładzenie wyjścia |
| DEPTH PREVIEW | Podgląd przebiegu i aktualnej wartości |
4.3 ON KICK TRIGGER resetuje fazę przy wyzwoleniu; FREE RUN nie resetuje; ONE SHOT – jeden cykl / długość, potem trzyma wartość końcową albo wraca do zera (wybieralne). Audio-rate dla celów, które tego wymagają – nie ograniczać LFO do odświeżania GUI.

## 5. Step Sequencer
Rytmiczne zmiany parametrów (pulsowanie distortion, ruch filtra, poziom screech, warstwy).
5.1 Długość 1–64 (szybkie 16 / 32 / 64); HOST SYNC lub FREE RATE; podziały 1/1–1/64 + kropkowane i triole; kierunki FORWARD, BACKWARD, PING-PONG, RANDOM, ALTERNATE; reset OFF, ON NOTE, ON KICK TRIGGER, ON BAR; SWING i HUMANIZE w ograniczonym, przewidywalnym zakresie.
5.2 Krok:
| Parametr | Opis |
|---|---|
| VALUE | -100…+100% lub 0–100% (unipolar) |
| PROBABILITY | Pominięcie = zero, hold albo skip (określone) |
| TIE | Łączenie z następnym krokiem |
| ACCENT | Dodatkowe wzmocnienie w ustalonym zakresie |
| RATCHET | 1–8 powtórzeń w kroku |
| CURVE | LINEAR, STEP, RAMP UP, RAMP DOWN, SMOOTH |
| MUTE | Wyłączenie kroku bez usuwania ustawień |
Sekwencer = źródło w macierzy (bez bezpośrednich połączeń do parametrów).
5.3 Edycja: klik ustawia, przeciąganie zmienia, rysowanie wielu kroków; kopiuj / wklej / odwróć / przesuń / losuj; pozycja odtwarzania i aktualna wartość; skrócenie sekwencji nie usuwa kroków poza zakresem.

## 6. Randomizer Engine
Kontrolowane, powtarzalne zmiany: źródło modulacji + generator wariantów; nie nadpisuje przypadkiem całego presetu.
| Tryb | Zachowanie |
|---|---|
| FIXED SEED | Ten sam seed + ustawienia = identyczny wynik |
| PER TRIGGER | Nowa wartość przy każdym uderzeniu; seed = powtarzalność |
| FREE RUN | Zmiana w czasie wg rate |
| ONE SHOT | Jedna wartość na uruchomienie / wyzwolenie |
| CONTROLLED RANDOM | W zakresie, z ograniczeniem odchylenia od bazy |
6.1 SEED (w projekcie i presecie), AMOUNT 0–100%, RANGE MIN / MAX, LOCK, SCOPE (SELECTED PARAMS, CURRENT LAYER, CURRENT GROUP, CURRENT MODULE, FULL PRESET), REGENERATE (bez zmiany zablokowanych), UNDO, RANDOMIZE ROUTING (opcjonalnie, domyślnie wył.).
Nie łamie limitów DSP; chronione: limiter safety ceiling, max poziom wyjściowy, granice feedbacku.

## 7. Macro Engine
Każde makro = źródło modulacji z wieloma celami.
7.1 Min. 8 makr, rozbudowa do 16. Każde: nazwa, kolor / identyfikator, wartość 0–100%, domyślna, opcjonalna ikona. Przypisanie: MIN, MAX, CURVE, POLARITY, ENABLE. Jedno makro → wiele parametrów z różnych modułów; parametr ← kilka makr (zasady łączenia z macierzy).
7.2 Makra startowe 4GE:
| Makro | Przykładowe zastosowanie |
|---|---|
| PUNCH | Poziom transientu, atak, click level, krótkie pasmo punch |
| BODY | Poziom body, długość body, nasycenie, kontrola środka |
| TAIL | Długość tail, poziom tail, filtr, distortion w ogonie |
| GRIT | Drive distortion, asymmetry, fold amount, bitcrush mix |
| SCREAM | Screech level, resonance, formant, distortion send |
| MOVEMENT | Głębokość LFO, cutoff modulation, sekwencer, ruch parametrów |
| DENSITY | Layer levels, multiband compression, saturation, crest factor |
| SPACE | Stereo width warstw, reverb / send jeśli jest, przestrzenność tekstury |
Wartości startowe – użytkownik zmienia nazwy, zakresy i przypisania. Makro nie wymusza stylu ani nie blokuje edycji parametrów źródłowych.
7.3 Macro Morph: płynne przejście między zestawami wartości makr / stanami A/B – interpolacja (nie skok), z uwzględnieniem skali parametru.

## 8. MIDI, DAW i automatyzacja
MIDI CC jako źródła + standardowa automatyzacja hosta. MIDI Learn (wybierz parametr / makro, porusz kontrolerem). Mapowanie CC: zakres MIN / MAX, krzywa, polaryzacja, smoothing. Jeden spójny model parametrów dla GUI, hosta i modulation engine (zakaz dwóch wartości GUI / DSP). Zmiany z hosta bezpiecznie do audio thread.

## 9. Routing, priorytety i bezpieczeństwo
9.1 Centralny Parameter Registry: stabilny ID, typ, jednostka, zakres, domyślna, skala, smoothing, flagi. Macierz nie używa nazw tekstowych jako ID.
9.2 Walidacja: brak połączeń do nieistniejących celów; tylko parametry jawnie modulowalne; wykrywanie i blokowanie cykli; parametry dyskretne domyślnie niemodulowalne (jeśli tak – przełączanie bez glitchy + jasny próg); twarde limity; wartości safety-critical z limitami nie do nadpisania.
9.3 Kolejność: host / MIDI → obwiednie i LFO → sekwencer / randomizer → macierz → walidacja i clamp → smoothing → DSP. Audio-rate osobną ścieżką bez łamania kolejności. Bez zależności od kolejności kontenerów, adresów, wątków. Ten sam stan + seed = ten sam wynik.

## 10. GUI / UX
Czytelne przy wielu połączeniach – szybko widać, co steruje parametrem i jak mocno.
10.1 Widoki: MOD MATRIX (tabela slotów), LFO EDITOR (wykres, rate, sync, division, phase, retrigger, polarity, fade, smoothing), STEP SEQUENCER (siatka, value, probability, ratchet, curve, swing, długość, kierunek), RANDOMIZER (seed, amount, range, scope, lock list, regenerate, undo), MACROS (przypisania, mapa min / max, krzywe), SOURCE MONITOR (wartości źródeł na żywo), DESTINATION MONITOR (bazowa, suma modulacji, finalna).
10.2 Drag-and-drop źródło → parametr; alternatywnie wybór z listy w slocie; pierścień / pasek głębokości na modulowanym parametrze; klik wskaźnika = lista połączeń do parametru; podgląd BASE VALUE / MODULATION SUM / FINAL VALUE; wyszukiwanie i filtr wg modułu.
GUI nie odświeża się z częstotliwością audio – buforowanie, 30–60 Hz.

## 11. Presety i stan
Zapis wszystkich slotów, źródeł, LFO, sekwencerów, randomizerów, makr, seedów, MIDI. Stabilne ID parametrów (nie indeksy GUI). Po wczytaniu weryfikacja ID – nieznane cele nieaktywne + informacja. Identyczny routing po ponownym otwarciu. Pełny stan modułu 09 w stanie 4GE zapisywanym przez hosta.

## 12. Wymagania DSP i wydajność
Bez alokacji, mutexów i I/O w callbacku; bezpieczne przekazywanie GUI / host / audio. Smoothing zależny od charakteru (cutoff, gain, pitch, time, feedback – różne algorytmy), ale nie niszczy szybkich transientów ani rytmu. LFO i obwiednie z właściwą fazą i czasem przy różnych SR i buforach. Reset, stop i zmiana tempa bez złego stanu. Offline = realtime (poza jawnie FREE RUN random); FIXED SEED powtarzalny w obu. Denormal / NaN / inf.

## 13. Testy i kryteria akceptacji
| Test | Warunek |
|---|---|
| Matrix routing | Źródło zmienia wskazany parametr DSP; wyłączenie slotu usuwa wpływ |
| Multiple sources | Przewidywalny wynik zgodny z trybem łączenia |
| LFO sync | HOST SYNC trzyma synchronizację po zmianie BPM i transportu |
| LFO retrigger | ON NOTE, ON KICK TRIGGER, FREE RUN zachowują się odmiennie |
| Step sequencer | Kroki, probability, ratchet, swing, kierunki działają |
| Randomizer seed | Ten sam seed i stan = identyczny wynik |
| Macro mapping | Jedno makro steruje kilkoma parametrami w różnych modułach |
| MIDI Learn | CC steruje przypisanym źródłem / makrem |
| Host automation | Bez rozjazdu GUI / DSP |
| Parameter safety | Żadne połączenie nie przekracza twardych zakresów i limitów |
| Preset persistence | Identyczny routing i ustawienia po wczytaniu |
| Audio stability | Bez kliknięć, trzasków, NaN, zawieszeń, CPU pod kontrolą |
| Realtime/offline | Powtarzalność dla deterministycznych źródeł i seeda |

## 14. Kolejność implementacji
1 Parameter Registry · 2 Modulation Engine + 32 sloty (source, destination, depth, polarity, curve, enable) · 3 źródła podstawowe (pitch env, amp env, velocity, key tracking, makra) · 4 LFO (sync, retrigger, przebiegi, smoothing) · 5 Step Sequencer · 6 Randomizer (seed, locki, tryby) · 7 Audio Follower, MIDI CC, MIDI Learn · 8 GUI monitorów, drag-and-drop, wskaźniki, diagnostyka · 9 walidacja cykli, bezpieczeństwo, optymalizacja, testy realtime / offline.

## 15. Przykładowe konfiguracje hardstyle
15.1 Ruch filtra w tailu: LFO 1 (SINE, 1/4 HOST SYNC, ON KICK TRIGGER) → TAIL Filter Cutoff, depth +35%, EXPONENTIAL, smoothing 5 ms – ruch cutoff w ogonie bez ruszania PUNCH.
15.2 Żywy screech: Pitch Envelope → Screech Pitch (+24 st) i Screech Resonance (+15%); LFO 2 (TRIANGLE, 1/8) → Screech Formant (+10%).
15.3 Rytmiczny rawstyle distortion: Step Sequencer (16 kroków, 1/16 HOST SYNC) → Distortion Drive (+20%) i Mix (+10%), ADD.
15.4 Makro GRIT → Distortion Drive 0–75%, Asymmetry 0–60%, Wavefold 0–35%, Clipper Drive 0–20% – kontrolowana agresja w bezpiecznych poziomach.
15.5 Mikro-zmienność warstwy: Randomizer (PER TRIGGER, FIXED SEED, AMOUNT 10%) → Texture Layer Fine Pitch (±4 ct) i Level (±1 dB) – powtarzalne dzięki seedowi.

## 16. Instrukcja końcowa
Najpierw Parameter Registry i wspólny interfejs parametrów, potem routing, źródła i GUI. Każde źródło: określone zachowanie czasowe, zakres, reset / retrigger, determinizm. Każdy cel: jawnie zarejestrowany, legalny zakres, smoothing. Wszystko bezpieczne dla audio thread.
Definition of Done: dowolne źródło → dowolny zarejestrowany cel, kontrola głębokości i charakteru, widoczny rzeczywisty wpływ, zapis presetu, powtarzalny wynik bez artefaktów.

---
## NOTES (Claude, 2026-11-03) – decyzje techniczne
- Parameter Registry = APVTS + tabela metadanych (ID, jednostka, skala, smoothing, flaga modulowalności, safety limit); ID zamrożone w `Tests/param_ids.txt` jak w RE4GE. Wskaźniki zapisane raz (ParamBridge z RE4GE) – zero lookupów na audio thread.
- Modulacja liczona per blok kontrolny (32 próbki jak w RE4GE) + audio-rate tylko dla celów, które tego wymagają (pitch / FM / cutoff), żeby CPU było pod kontrolą.
- Klocki z RE4GE: `DSP/ModMatrix.h` (16 wierszy → rozszerzyć do 32/64 + tryby łączenia), LFO 1–4 (→ 8), `DSP/Trigger.h` sekwencer na ppq hosta, Randomizer (style / LOCK / 1 klik = 1 undo, loudness przez offline render), MorphBar (A/B morph z przeskokiem przełączników przy 50 %).
- Makra: 8 głównych makr 4GE = te z pkt 7.2 (PUNCH, BODY, TAIL, GRIT, SCREAM, MOVEMENT, DENSITY, SPACE). Makra „modułowe” z M06 (SCREAM, METALLIC…), M07 (BRIGHTNESS…) i M08 (LOUDNESS, CRUSH…) proponuję jako gotowe MAPY / presety przypisań do tych samych 16 slotów makr, a nie osobne gałki – do potwierdzenia na koniec.
- Owner PDF: `Specs/MODULE09_owner.pdf`.
