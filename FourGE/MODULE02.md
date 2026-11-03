<!-- Owner specification of module 02 (2026-11-03), verbatim. Plan: PLAN.md. -->
4GE — HARDSTYLE KICK DESIGNER

MODULE 02: ADVANCED PITCH & ENVELOPE ENGINE

Manufacturer: Sonicus / Sonicsystems
Plugin: 4GE
Module: Advanced Pitch & Envelope Engine
Priority: CRITICAL
Dependencies: Module 01 — Core Kick Engine
Implementation: C++ / JUCE

⸻

1. CEL MODUŁU

Rozbudować podstawowy Pitch Engine i Envelope Engine o profesjonalny system wielopunktowych obwiedni.

System musi umożliwiać precyzyjne sterowanie zmianą wysokości dźwięku oraz innymi parametrami w czasie.

Głównym celem jest stworzenie narzędzia, które umożliwia projektowanie:

* klasycznych hardstyle pitch dropów,
* gwałtownych pitch dropów charakterystycznych dla nowoczesnych kicków,
* podwójnych i wieloetapowych zmian pitchu,
* narastających tonów,
* screechy i tonalnych ogonów,
* niestandardowych, eksperymentalnych ruchów pitchu.

System musi działać niezależnie dla PUNCH, BODY, TAIL i SUB.

Nie ograniczaj użytkownika do kilku gotowych obwiedni. Presety są jedynie punktem wyjścia. Użytkownik musi móc tworzyć własne krzywe.

2. ADVANCED PITCH ENGINE

2.1 Podstawowe parametry

START PITCH:

* zakres: 20–2000 Hz
* określa początkową częstotliwość oscylatora.

END PITCH:

* zakres: 20–2000 Hz
* określa końcową częstotliwość.

PITCH AMOUNT:

* zakres: -48 do +48 semitonów.
* określa dodatkową zmianę wysokości względem wartości bazowej.

PITCH TIME:

* zakres: 1–2500 ms.
* określa czas trwania zmian pitchu.

PITCH CURVE:

* zakres: 0–100%.
* steruje kształtem interpolacji pomiędzy punktami krzywej.

PITCH FINE:

* zakres: -100 do +100 centów.

PITCH OCTAVE:

* zakres: -4 do +4 oktaw.

PITCH TRACKING:

* ON/OFF.
* umożliwia reagowanie generatora na wysokość nuty MIDI.

Ważne: częstotliwość bazowa oraz względna obwiednia pitchu muszą być rozdzielone. Zmiana START PITCH nie może przypadkowo zmieniać zapisanej krzywej obwiedni.

3. MULTIPOINT PITCH ENVELOPE

Najważniejsza funkcja tego modułu.

Zaimplementuj graficzny edytor obwiedni pitchu.

Użytkownik powinien mieć możliwość dodawania, przesuwania i usuwania punktów krzywej.

3.1 Parametry punktu

Każdy punkt posiada:

* TIME — pozycja czasowa.
* VALUE — wartość pitchu.
* CURVE — kształt przejścia do kolejnego punktu.
* TENSION — kontrola napięcia krzywej, jeśli używany jest interpolator obsługujący ten parametr.

Zakres czasu:

* 0–2500 ms.

Zakres wartości:

* -48 do +48 semitonów względem bazowej częstotliwości.

Liczba punktów:

* minimum 2.
* maksimum 64.

Pierwszy punkt musi znajdować się w czasie 0 ms.

Ostatni punkt wyznacza zakończenie obwiedni albo przejście do utrzymania wartości, zależnie od trybu pracy.

3.2 Rodzaje interpolacji

Każdy odcinek krzywej musi obsługiwać:

* LINEAR
* EXPONENTIAL
* LOGARITHMIC
* EASE IN
* EASE OUT
* EASE IN/OUT
* BEZIER
* STEP

BEZIER musi pozwalać na kontrolowanie uchwytów krzywej.

STEP utrzymuje poprzednią wartość aż do następnego punktu.

Nie implementuj tych trybów jako samych zmian graficznych. Każdy musi rzeczywiście wpływać na obliczaną wartość pitchu.

3.3 Edytor graficzny

Wymagania:

* siatka czasu w milisekundach,
* pionowa skala pitchu,
* możliwość dodawania punktów przez kliknięcie,
* przeciąganie punktów,
* usuwanie punktu,
* zoom poziomy i pionowy,
* dopasowanie całej obwiedni do widoku,
* reset do domyślnej krzywej,
* undo/redo,
* snap do siatki z możliwością wyłączenia,
* ręczne wpisywanie wartości punktu.

Podczas przeciągania punktów nie pozwalaj na nieprawidłowe uporządkowanie czasu.

Użytkownik może przesuwać punkty w poziomie i pionie, ale ich kolejność czasowa musi być zachowana.

4. PITCH MODES

Przygotuj bibliotekę gotowych kształtów obwiedni:

CLASSIC DROP

Szybki wzrost częstotliwości na początku, następnie kontrolowany spadek do niższej wartości.

FAST DROP

Bardzo szybki spadek pitchu w początkowej fazie.

SLOW DROP

Dłuższy, bardziej płynny spadek.

DOUBLE DROP

Pierwszy spadek, krótkie odbicie w górę, drugi spadek.

RISE

Stopniowy wzrost wysokości dźwięku.

DROP + RISE

Spadek, a następnie wzrost wysokości.

STABLE TONE

Stała wysokość dźwięku.

HARDSTYLE KICK

Szybki początkowy pitch transient i charakterystyczny spadek w dalszej części.

CUSTOM

Krzywa utworzona przez użytkownika.

Wszystkie tryby muszą być edytowalne.

Zmiana trybu nie może blokować edycji punktów.

5. PITCH ENVELOPE PER COMPONENT

Każdy generator musi posiadać własną obwiednię pitchu.

PUNCH PITCH:

* pozwala na bardzo szybki, agresywny pitch transient.
* niezależny czas i zakres.
* możliwość wyłączenia obwiedni.

BODY PITCH:

* kontroluje tonalny ruch środkowej części kicka.
* może być zsynchronizowany z PUNCH albo działać niezależnie.

TAIL PITCH:

* umożliwia projektowanie tonalnego ogona.
* musi umożliwiać stabilny ton, pitch drop i pitch rise.
* posiada własną długość obwiedni.

SUB PITCH:

* domyślnie stabilny i kontrolowany.
* możliwość niezależnego pitch dropu.
* dodatkowa opcja FOLLOW BODY, która pozwala na śledzenie obwiedni BODY z regulowaną głębokością.

Każdy komponent posiada:

* ENABLE,
* AMOUNT,
* TIME SCALE,
* OFFSET,
* RESET,
* COPY ENVELOPE,
* PASTE ENVELOPE.

6. PITCH LINK SYSTEM

Utwórz system synchronizacji obwiedni.

Dostępne tryby:

INDEPENDENT:
Każda warstwa ma własną obwiednię.

FOLLOW:
Wybrana warstwa śledzi obwiednię innej warstwy.

COPY:
Kopiuje punkty obwiedni do drugiej warstwy. Po skopiowaniu obwiednie stają się niezależne.

LINKED:
Zmiana obwiedni źródłowej aktualizuje również warstwę docelową.

FOLLOW DEPTH:

* zakres: 0–200%.
* określa, jak mocno warstwa śledzi źródłową obwiednię.

FOLLOW OFFSET:

* zakres: -48 do +48 semitonów.

FOLLOW TIME:

* zakres: 10–400%.

LINK musi zapobiegać zapętleniu zależności. Nie wolno dopuścić do sytuacji, w której A śledzi B, a B jednocześnie śledzi A.

7. ENVELOPE ENGINE

Zaimplementuj uniwersalny silnik obwiedni, który może sterować różnymi parametrami.

Obsługiwane typy:

1. AMPLITUDE
2. PITCH
3. FILTER CUTOFF
4. DISTORTION DRIVE
5. RESONANCE
6. NOISE LEVEL
7. FM AMOUNT
8. STEREO WIDTH
9. WAVEFOLDER AMOUNT

Każda obwiednia musi być niezależna i przypisywalna do odpowiedniego parametru.

7.1 Envelope parameters

ATTACK:

* 0–2500 ms.

DECAY:

* 0–2500 ms.

SUSTAIN:

* 0–100%.

RELEASE:

* 0–2500 ms.

AMOUNT:

* -100 do +100%.

DELAY:

* 0–2500 ms.

CURVE:

* 0–100%.

POLARITY:

* UNIPOLAR / BIPOLAR.

TRIGGER MODE:

* ONE SHOT
* RETRIGGER
* LEGATO

7.2 ADSR

Klasyczna obwiednia ADSR musi być dostępna jako szybki tryb pracy.

ATTACK:
Czas narastania do maksymalnej wartości.

DECAY:
Czas przejścia od wartości maksymalnej do SUSTAIN.

SUSTAIN:
Poziom utrzymywany do zakończenia fazy podtrzymania.

RELEASE:
Czas wygaszenia po zakończeniu fazy podtrzymania.

Dla kicka podstawowym trybem jest ONE SHOT, ponieważ jego wybrzmienie powinno być kontrolowane przez generator, a nie przez długość trzymania nuty MIDI.

8. ENVELOPE EDITOR

Przygotuj uniwersalny edytor wielopunktowy.

Maksymalnie 64 punkty na obwiednię.

Każdy punkt:

* TIME
* VALUE
* CURVE TYPE
* TENSION

Edytor musi obsługiwać:

* zoom,
* snap,
* kopiowanie punktów,
* zaznaczanie wielu punktów,
* przesuwanie zaznaczonych punktów,
* zmianę wartości kilku punktów jednocześnie,
* reset,
* undo/redo,
* zapis i wczytywanie kształtów obwiedni.

Każda obwiednia musi być możliwa do zapisania jako osobny preset.

9. TIME SCALING

Zaimplementuj globalny system skalowania czasu.

TIME SCALE:

* zakres: 10–400%.

Zmienia długość obwiedni bez zmieniania jej kształtu.

TIME OFFSET:

* zakres: 0–2500 ms.

Przesuwa rozpoczęcie działania obwiedni względem początku kicka.

GLOBAL SYNC:

* ON/OFF.

Gdy aktywny:

* wybrane obwiednie mogą synchronizować czas do tempa hosta.

Tempo hosta musi być pobierane z DAW.

W przypadku braku informacji o BPM plugin powinien korzystać z wartości lokalnej.

10. MODULATION ASSIGNMENT

Przygotuj architekturę pozwalającą przypisywać obwiednie do parametrów.

Przykładowe połączenia:

* PITCH ENVELOPE → OSCILLATOR PITCH.
* AMPLITUDE ENVELOPE → LAYER LEVEL.
* FILTER ENVELOPE → FILTER CUTOFF.
* DISTORTION ENVELOPE → DRIVE.
* RESONANCE ENVELOPE → RESONANCE.

Każde przypisanie musi posiadać:

* SOURCE,
* DESTINATION,
* AMOUNT,
* POLARITY,
* CURVE,
* ENABLE.

Jedno źródło może sterować wieloma parametrami.

Kilka źródeł może sterować jednym parametrem, jeśli docelowy parametr obsługuje sumowanie modulacji.

Wartości modulacji muszą być ograniczane do bezpiecznych zakresów parametrów.

11. MIDI I HOST SYNC

System musi obsługiwać:

* MIDI Note On,
* MIDI Velocity,
* MIDI Note Off,
* host BPM,
* host transport,
* host automation.

Każdy kick musi otrzymać własny stan obwiedni.

Nowe Note On powinno tworzyć nowy głos bez nadpisywania obwiedni poprzedniego głosu.

Note Off nie może ucinać kicka w trybie ONE SHOT.

W trybie LEGATO zachowanie musi być jednoznacznie określone i przetestowane.

12. DSP REQUIREMENTS

* Interpolacja musi być wolna od kliknięć.
* Obwiednie muszą być przeliczane z uwzględnieniem aktualnego sample rate.
* Nie wolno opierać czasu obwiedni na stałej liczbie próbek bez uwzględnienia sample rate.
* Zmiany parametrów muszą być wygładzane, jeśli mogą powodować nieciągłości.
* Nie używać dynamicznej alokacji pamięci w callbacku audio.
* Nie używać blokujących operacji w wątku audio.
* Obwiednie muszą być deterministyczne.
* Wartości NaN i nieskończone muszą być zabezpieczone.
* Zmiana presetu nie może powodować niekontrolowanych skoków pitchu.
* Wszystkie krzywe muszą działać identycznie podczas realtime playback i offline export.

Pitch powinien być przeliczany w odpowiedniej domenie:

* częstotliwość bazowa w Hz,
* zmiana wysokości w semitonach lub centach,
* przeliczenie na częstotliwość oscylatora.

Dla zmiany pitchu w semitonach wykorzystaj relację:

frequency = baseFrequency × 2^(semitones / 12)

Nie interpoluj bezmyślnie częstotliwości liniowo, jeśli użytkownik wybrał krzywą semitonową. Interpolacja musi zachować zamierzony charakter ruchu pitchu.

13. GUI

Główny widok modułu powinien zawierać:

* duży edytor krzywej,
* wybór komponentu PUNCH / BODY / TAIL / SUB,
* wybór typu obwiedni,
* START VALUE,
* END VALUE,
* AMOUNT,
* TIME,
* CURVE,
* LINK,
* COPY,
* PASTE,
* RESET.

Przełącznik VIEW:

* PITCH
* AMPLITUDE
* FILTER
* DISTORTION
* RESONANCE

Użytkownik musi wiedzieć, którą obwiednię aktualnie edytuje.

Wizualizacja powinna wyświetlać krzywą obwiedni na tle osi czasu.

Jeśli użytkownik wybierze PITCH, pionowa skala musi pokazywać semitony lub częstotliwość. Jeśli wybierze AMPLITUDE, musi pokazywać poziom procentowy lub dB.

14. TESTY I KRYTERIA AKCEPTACJI

Moduł można uznać za ukończony, jeśli:

1. Można utworzyć obwiednię z minimum 64 punktami.
2. Wszystkie typy interpolacji działają w DSP.
3. Każdy komponent ma niezależną obwiednię pitchu.
4. Można kopiować i łączyć obwiednie.
5. LINK nie powoduje cyklicznych zależności.
6. TIME SCALE działa bez zmiany kształtu obwiedni.
7. Obwiednie synchronizują się z BPM hosta.
8. Wszystkie obwiednie działają w trybie ONE SHOT.
9. MIDI Note Off nie ucina kicka w ONE SHOT.
10. Modulacja rzeczywiście zmienia docelowe parametry DSP.
11. Presety zapisują i przywracają wszystkie punkty.
12. Zmiana parametrów nie powoduje niekontrolowanych trzasków.
13. Realtime playback i offline export generują zgodny rezultat.
14. Nie występują błędy podczas nakładania się głosów.

15. ZAKAZ ATRAP

Nie twórz wizualnych krzywych, które nie mają wpływu na dźwięk.

Nie implementuj interpolacji wyłącznie po stronie GUI.

Nie ograniczaj obwiedni do kilku stałych punktów.

Nie pozwalaj na nieskończone lub nieprawidłowe wartości pitchu.

Nie łącz wszystkich komponentów w jedną obwiednię bez możliwości niezależnego sterowania.

Każdy parametr widoczny w interfejsie musi mieć rzeczywiste połączenie z silnikiem DSP.

16. REZULTAT KOŃCOWY

Po ukończeniu modułu użytkownik powinien móc:

* projektować własne krzywe pitchu,
* tworzyć klasyczne i eksperymentalne pitch dropy,
* kontrolować pitch każdej warstwy oddzielnie,
* tworzyć wielopunktowe obwiednie,
* sterować filtrem, distortion i innymi parametrami w czasie,
* kopiować i synchronizować obwiednie,
* synchronizować wybrane obwiednie z tempem DAW,
* zapisywać własne kształty obwiedni.

Moduł musi być pełnoprawnym systemem modulacji, a nie tylko zestawem pokręteł START PITCH i END PITCH.