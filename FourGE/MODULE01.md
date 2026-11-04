<!-- Owner specification of module 01 (2026-11-03), verbatim. Plan: PLAN.md step 1. -->
> **Zasada nadrzędna:** 4GE ma dokładnie 8 MASTER MACROS (IMPACT, BODY, TAIL, PITCH, GRIT, SCREAM, WEIGHT, CHAOS). Makra / kontrolery opisane w tym module są LOKALNE i sterowane przez Macro Mapping (M09) – patrz `RULES.md`.
4GE — HARDSTYLE KICK DESIGNER

MODULE 01: CORE KICK ENGINE

Manufacturer: Sonicus / Sonicsystems
Plugin: 4GE
Module: Core Kick Engine
Priority: CRITICAL
Implementation: C++ / JUCE
Plugin format: VST3

⸻

1. CEL MODUŁU

Zaimplementować podstawowy silnik syntezy kicka hardstyle, który generuje dźwięk od podstaw na podstawie niezależnych komponentów:

1. PUNCH — początkowy atak kicka.
2. BODY — główna część dźwięku.
3. TAIL — ogon kicka, odpowiedzialny za charakterystyczne brzmienie hardstyle.
4. SUB — niskoczęstotliwościowa warstwa basowa.

Każdy komponent musi być generowany i przetwarzany niezależnie.

Silnik nie może być zwykłym odtwarzaczem sampli. Jego podstawowym zadaniem jest synteza dźwięku w czasie rzeczywistym.

2. ARCHITEKTURA SILNIKA

Utwórz cztery niezależne moduły generatorów:

* PunchGenerator
* BodyGenerator
* TailGenerator
* SubGenerator

Każdy generator powinien posiadać własne:

* źródło sygnału,
* obwiednię głośności,
* obwiednię wysokości dźwięku, jeśli jest potrzebna,
* długość,
* poziom wyjściowy,
* ustawienia fazy,
* możliwość wyciszenia i odsłuchu SOLO.

Wszystkie generatory muszą być połączone z głównym mikserem CoreMixer.

Architektura musi pozwalać na późniejsze dodawanie kolejnych źródeł, warstw i efektów bez przebudowy całego silnika.

3. GLOBALNE PARAMETRY KICKA

3.1 LENGTH

Główne pokrętło określające długość kicka.

Zakres: 100–2500 ms.

Wartość początkowa: 450 ms.

Parametr określa docelowy czas całego kicka, ale nie może automatycznie rozciągać wszystkich jego elementów w identyczny sposób.

Przykład:

* PUNCH może trwać 35 ms.
* BODY może trwać 120 ms.
* TAIL może trwać 400 ms.
* SUB może trwać 300 ms.

Zmiana LENGTH powinna odpowiednio dostosowywać przede wszystkim długość BODY i TAIL, natomiast PUNCH powinien zachować swój charakterystyczny, krótki atak.

3.2 MASTER

Zakres: -inf do +6 dB.

Kontroluje poziom całego wygenerowanego kicka.

Powinien działać na końcu głównego toru sygnałowego.

3.3 AUTO LENGTH

Typ: ON/OFF.

ON:

* silnik automatycznie dostosowuje długości komponentów do wartości LENGTH.

OFF:

* każdy komponent zachowuje własną długość, niezależnie od globalnego LENGTH.

3.4 PHASE RESET

Typ: ON/OFF.

ON:

* oscylatory rozpoczynają każdy kick od zdefiniowanej fazy początkowej.

OFF:

* silnik wykorzystuje ustawienia fazy i ewentualną modulację fazy.

Domyślnie ON.

4. PUNCH GENERATOR

PUNCH odpowiada za pierwszy impuls kicka, który nadaje mu siłę i czytelność.

Parametry:

SOURCE:

* Sine
* Triangle
* Saw
* Square
* Noise
* Click

LEVEL:

* zakres: -inf do +12 dB
* wartość początkowa: 0 dB

PITCH:

* zakres: 20–2000 Hz

LENGTH:

* zakres: 1–150 ms
* wartość początkowa: 35 ms

ATTACK:

* zakres: 0–20 ms

DECAY:

* zakres: 1–150 ms

TONE:

* zakres: 20 Hz–20 kHz
* kontroluje tonalność źródła poprzez filtr.

DRIVE:

* zakres: 0–100%
* na tym etapie może sterować prostym nieliniowym przesterowaniem.

MIX:

* zakres: 0–100%

Obwiednia PUNCH

Zaimplementuj niezależną obwiednię amplitudy:

* Attack
* Decay
* Sustain
* Release

W przypadku kicka obwiednia musi umożliwiać bardzo szybki atak i krótki, kontrolowany zanik.

Wymagania

* PUNCH musi być niezależny od BODY i TAIL.
* Musi posiadać SOLO i MUTE.
* Zmiana długości PUNCH nie może zmieniać długości pozostałych komponentów.
* PUNCH musi być możliwy do generowania bez pozostałych warstw.

5. BODY GENERATOR

BODY stanowi środkową część kicka. Odpowiada za jego ciężar, tonalność i przejście pomiędzy uderzeniem a ogonem.

Parametry:

SOURCE:

* Sine
* Triangle
* Saw
* Square
* Noise

LEVEL:

* zakres: -inf do +12 dB

PITCH:

* zakres: 20–2000 Hz

LENGTH:

* zakres: 10–1000 ms

ATTACK:

* zakres: 0–100 ms

DECAY:

* zakres: 1–1000 ms

TONE:

* zakres: 20 Hz–20 kHz

DRIVE:

* zakres: 0–100%

RESONANCE:

* zakres: 0–100%

MIX:

* zakres: 0–100%

Obwiednia BODY

BODY musi mieć własną obwiednię amplitudy i możliwość niezależnego sterowania jej kształtem.

Wymagane punkty:

* Attack
* Decay
* Sustain
* Release

Dodatkowo przygotuj możliwość późniejszego dodania wielopunktowej obwiedni graficznej.

6. TAIL GENERATOR

TAIL jest jednym z najważniejszych elementów całego 4GE.

Odpowiada za charakterystyczny ogon hardstyle kicka. Musi umożliwiać tworzenie zarówno czystych, tonalnych ogonów, jak i agresywnych, przesterowanych brzmień.

Parametry:

SOURCE:

* Sine
* Triangle
* Saw
* Square
* Pulse
* Noise

LEVEL:

* zakres: -inf do +12 dB

PITCH:

* zakres: 20–2000 Hz

LENGTH:

* zakres: 10–2500 ms

ATTACK:

* zakres: 0–200 ms

DECAY:

* zakres: 1–2500 ms

TONE:

* zakres: 20 Hz–20 kHz

DRIVE:

* zakres: 0–100%

RESONANCE:

* zakres: 0–100%

MIX:

* zakres: 0–100%

Charakter TAIL

Przygotuj parametr CHARACTER z następującymi trybami:

* CLEAN
* TONAL
* DIRTY
* SCREAM
* METALLIC
* BUZZ
* CRUSHED

Na tym etapie tryby mogą korzystać z różnych ustawień generatora, filtracji i podstawowego przesterowania.

Nie mogą być jedynie zmianą nazwy presetu. Każdy tryb musi realnie zmieniać charakter generowanego sygnału.

Obwiednia TAIL

TAIL musi mieć własną obwiednię amplitudy.

W przyszłości będzie również posiadać niezależne obwiednie:

* Pitch
* Filter
* Distortion
* Resonance

7. SUB GENERATOR

SUB odpowiada za najniższą część kicka.

Musi być niezależnym generatorem, a nie tylko efektem podbicia basu w EQ.

Parametry:

SOURCE:

* Sine
* Triangle

LEVEL:

* zakres: -inf do +12 dB

PITCH:

* zakres: 20–120 Hz

LENGTH:

* zakres: 10–1500 ms

ATTACK:

* zakres: 0–100 ms

DECAY:

* zakres: 1–1500 ms

DRIVE:

* zakres: 0–100%

PHASE:

* zakres: 0–360°

Wymagania

* SUB domyślnie pracuje w mono.
* SUB musi mieć osobne SOLO i MUTE.
* Musi istnieć możliwość synchronizacji fazy z pozostałymi generatorami.
* Musi być możliwość wyłączenia SUB bez wpływania na pozostałe komponenty.
* Należy przewidzieć późniejszy moduł kontroli fazy i częstotliwości odcięcia.

8. PITCH ENGINE — PODSTAWOWA WERSJA

Zaimplementuj wspólny system kontroli wysokości dźwięku.

Główne parametry:

START PITCH:

* zakres: 20–2000 Hz

END PITCH:

* zakres: 20–2000 Hz

PITCH TIME:

* zakres: 1–1000 ms

PITCH CURVE:

* zakres: 0–100%

PITCH AMOUNT:

* zakres: -48 do +48 semitonów

Pitch Envelope musi umożliwiać tworzenie charakterystycznego spadku wysokości dźwięku hardstyle kicka.

Przygotuj następujące tryby:

* CLASSIC DROP
* FAST DROP
* SLOW DROP
* DOUBLE DROP
* RISE
* STABLE TONE
* CUSTOM

Ważne: Pitch Engine musi umożliwiać niezależne sterowanie PUNCH, BODY i TAIL w przyszłości. Nie zakładaj, że cały kick zawsze musi korzystać z jednej wspólnej obwiedni.

9. CORE MIXER

Utwórz wewnętrzny mikser czterech komponentów.

Każdy kanał musi mieć:

* Level
* Pan
* Mute
* Solo
* Phase Invert
* Peak Meter

Kanały:

1. PUNCH
2. BODY
3. TAIL
4. SUB

Mikser sumuje sygnały i przekazuje je do wyjścia Core Kick Engine.

Nie implementuj jeszcze pełnego zaawansowanego routingu. Przygotuj architekturę umożliwiającą jego dodanie w kolejnych etapach.

10. SIGNAL FLOW

Podstawowy przepływ sygnału:

MIDI NOTE ON
↓
KICK VOICE TRIGGER
↓
PITCH ENGINE
↓
PUNCH GENERATOR
BODY GENERATOR
TAIL GENERATOR
SUB GENERATOR
↓
CORE MIXER
↓
MASTER OUTPUT
↓
AUDIO OUTPUT

Każdy generator musi działać niezależnie, ale wszystkie muszą być uruchamiane przez ten sam trigger kicka.

11. MIDI I TRIGGER

Plugin musi reagować na MIDI Note On.

Wymagania:

* Note On uruchamia nową instancję kicka.
* Velocity może sterować globalną głośnością kicka.
* Note Off nie może automatycznie ucinać kicka.
* Kick powinien wybrzmieć zgodnie z ustawioną długością.
* Kolejne Note On powinno uruchomić kolejny kick bez niepożądanego przerwania poprzedniego.
* Należy obsłużyć przypadek nakładających się głosów.

Zaimplementuj kontrolę liczby aktywnych głosów oraz bezpieczne zwalnianie zakończonych głosów.

12. AUDIO ENGINE — WYMAGANIA TECHNICZNE

* Sample-rate independent processing.
* Obsługa minimum 44.1 kHz, 48 kHz, 88.2 kHz i 96 kHz.
* Obsługa różnych rozmiarów bufora.
* Parametry muszą być wygładzane tam, gdzie ich nagła zmiana powodowałaby trzaski.
* Brak dynamicznej alokacji pamięci w callbacku audio.
* Brak blokowania wątku audio.
* Zabezpieczenie przed DC offset.
* Zabezpieczenie przed denormal numbers.
* Stabilna obsługa MIDI.
* Brak kliknięć przy kończeniu głosu.
* Deterministyczne generowanie dźwięku przy tych samych parametrach.

Wszystkie generatory powinny pracować na wspólnym zegarze próbkowania, aby zachować precyzyjną synchronizację.

13. GUI — PIERWSZA WERSJA

Przygotuj prosty interfejs głównego modułu.

Główna sekcja:

* LENGTH
* MASTER
* AUTO LENGTH
* PHASE RESET

Cztery sekcje komponentów:

* PUNCH
* BODY
* TAIL
* SUB

Każda sekcja powinna mieć:

* LEVEL
* PITCH
* LENGTH
* DRIVE, jeśli dotyczy
* MUTE
* SOLO

Dodatkowo:

* centralny waveform display,
* przycisk PLAY / PREVIEW,
* wskaźnik poziomu wyjściowego,
* możliwość ręcznego wpisywania wartości parametrów.

Waveform musi prezentować rzeczywiście wygenerowany sygnał, a nie dekoracyjną animację.

14. TESTY I KRYTERIA AKCEPTACJI

Moduł można uznać za ukończony dopiero wtedy, gdy:

1. Można wygenerować kompletny kick bez korzystania z gotowego sampla.
2. Wszystkie cztery komponenty działają niezależnie.
3. Każdy komponent posiada działające SOLO i MUTE.
4. Zmiana LENGTH nie powoduje niekontrolowanego rozciągania PUNCH.
5. Każdy generator posiada działającą obwiednię.
6. Pitch Engine generuje rzeczywiste zmiany wysokości dźwięku.
7. MIDI Note On uruchamia kick.
8. Nakładające się wyzwolenia nie powodują błędów ani niekontrolowanych przerwań.
9. Plugin działa stabilnie przy różnych częstotliwościach próbkowania.
10. Nie występują trzaski podczas normalnego odtwarzania i wygaszania głosów.
11. Każdy widoczny parametr rzeczywiście wpływa na DSP.
12. Zapis i odtworzenie stanu pluginu przywraca wszystkie ustawienia.

15. ZAKAZ IMPLEMENTOWANIA ATRAP

Nie twórz przycisków, pokręteł ani sekcji, które wyglądają na funkcjonalne, ale nie mają połączenia z silnikiem DSP.

Jeżeli jakaś funkcja nie została jeszcze zaimplementowana, nie pokazuj jej jako działającej.

Nie zastępuj syntezy odtwarzaniem gotowego sampla.

Nie twórz jednego generatora, który tylko zmienia brzmienie w zależności od wybranego stylu.

Każdy komponent musi mieć rzeczywiście niezależny tor generowania sygnału.

16. REZULTAT KOŃCOWY

Po ukończeniu modułu użytkownik powinien móc:

* uruchomić 4GE jako instrument VST3,
* wygenerować kick z czterech niezależnych komponentów,
* kontrolować długość każdego komponentu,
* regulować pitch i obwiednie,
* miksować PUNCH, BODY, TAIL i SUB,
* odsłuchiwać każdy element osobno,
* wyzwalać kicki przez MIDI,
* uzyskać kompletny plik WAV z wygenerowanym kickiem.

To jest fundament wszystkich kolejnych modułów 4GE.