<!-- Owner specification of module 03 (2026-11-03), verbatim. Plan: PLAN.md. -->
4GE — HARDSTYLE KICK DESIGNER

MODULE 03: OSCILLATOR & WAVEFORM ENGINE

Manufacturer: Sonicus / Sonicsystems
Plugin: 4GE
Module: Oscillator & Waveform Engine
Priority: CRITICAL
Dependencies: Module 01 — Core Kick Engine; Module 02 — Advanced Pitch & Envelope Engine
Implementation: C++ / JUCE

⸻

1. CEL MODUŁU

Zaimplementować zaawansowany silnik oscylatorów odpowiedzialny za generowanie podstawowych przebiegów dźwiękowych wykorzystywanych przy projektowaniu kicków hardstyle.

Silnik musi umożliwiać:

* generowanie klasycznych przebiegów,
* kontrolę harmonicznych,
* tworzenie niestandardowych kształtów fal,
* modulację częstotliwości FM,
* modulację amplitudy i ring modulation,
* kontrolę fazy,
* unison i detuning,
* synchronizację oscylatorów,
* przypisywanie oscylatorów do PUNCH, BODY, TAIL i SUB.

Minimalna liczba oscylatorów: 4 niezależne jednostki.

Architektura musi pozwalać na rozszerzenie do minimum 8 oscylatorów bez przebudowy całego silnika.

Każdy oscylator musi być niezależnym źródłem dźwięku, a nie tylko kopią jednego generatora z inną głośnością.

2. OSCILLATOR ARCHITECTURE

Utwórz klasę bazową OscillatorEngine.

Każda instancja oscylatora musi posiadać:

* Oscillator ID
* Enable
* Waveform
* Frequency
* Octave
* Semitone
* Fine Tune
* Phase
* Phase Reset
* Level
* Pan
* Unison
* Detune
* Stereo Spread
* Pulse Width
* Sync
* FM Amount
* Ring Mod Amount
* Pitch Modulation Input
* Amplitude Modulation Input
* Output Routing

Każdy oscylator musi posiadać własny stan fazy i niezależne parametry.

Oscylatory powinny być przetwarzane w sposób deterministyczny i synchronizowane z tym samym zegarem próbkowania.

3. WAVEFORM GENERATOR

Każdy oscylator musi obsługiwać następujące podstawowe przebiegi:

SINE

Czysta sinusoida.

Zastosowanie:

* SUB,
* czysty BODY,
* tonalne TAIL,
* podstawowy sygnał do FM.

TRIANGLE

Trójkąt.

Zastosowanie:

* bardziej harmoniczny BODY,
* mniej agresywny materiał niż piła,
* źródło do dalszego przesterowania.

SAW

Piła.

Zastosowanie:

* bogate harmoniczne,
* agresywny BODY,
* tonalny lub screechowy TAIL.

SQUARE

Prostokąt.

Zastosowanie:

* mocne harmoniczne nieparzyste,
* metaliczny charakter,
* eksperymentalne brzmienia.

PULSE

Przebieg impulsowy z regulowaną szerokością impulsu.

PULSE WIDTH:

* zakres: 1–99%.

Zmiana szerokości impulsu musi rzeczywiście zmieniać kształt przebiegu i jego widmo harmoniczne.

NOISE

Źródło szumu.

Obsługiwane typy:

* White
* Pink
* Brown
* Metallic
* Digital

Szum powinien być generowany przez osobny Noise Engine, ale musi być dostępny jako źródło w architekturze oscylatorów.

CUSTOM WAVEFORM

Użytkownik może zaimportować lub utworzyć własny przebieg.

Obsługa:

* rysowanie przebiegu,
* edycja punktów,
* import WAV,
* konwersja importowanego materiału do cyklicznego przebiegu,
* normalizacja,
* usuwanie DC offset,
* odwracanie fazy,
* zapis własnych waveformów.

Importowany WAV nie może być automatycznie traktowany jako gotowa próbka kicka. W tym trybie materiał jest źródłem cyklicznego przebiegu oscylatora.

4. ANTI-ALIASING

Wszystkie oscylatory muszą posiadać zabezpieczenie przed aliasingiem.

Jest to szczególnie istotne przy:

* piłach,
* prostokątach,
* szybkich zmianach pitchu,
* FM,
* hard sync,
* wavefoldingu.

Wymagania:

* band-limited waveform generation,
* PolyBLEP lub inna odpowiednia metoda dla przebiegów nieciągłych,
* oversampling dla procesów generujących nowe harmoniczne,
* prawidłowe ograniczanie pasma do częstotliwości Nyquista.

Nie wystarczy filtrować aliasingu dopiero na końcu całego sygnału.

5. WAVETABLE ENGINE

Zaimplementuj podstawowy silnik wavetable.

Każda tabela może zawierać różne przebiegi, pomiędzy którymi użytkownik może płynnie przechodzić.

Parametry:

WAVETABLE POSITION:

* zakres: 0–100%.

WAVETABLE MORPH:

* zakres: 0–100%.
* kontroluje sposób przechodzenia pomiędzy przebiegami.

WAVETABLE SCAN:

* steruje ruchem pozycji w czasie.

WAVETABLE START:

* określa początkową pozycję.

WAVETABLE END:

* określa końcową pozycję.

WAVETABLE MODE:

* STATIC
* LINEAR SCAN
* ENVELOPE SCAN
* LFO SCAN
* RANDOM SCAN

RANDOM SCAN musi mieć możliwość ustawienia deterministycznego seeda.

Wymagania techniczne

* interpolacja pomiędzy pozycjami tabeli,
* mipmapping lub równoważny system ograniczania pasma,
* kontrola jakości interpolacji,
* możliwość ładowania własnych wavetable,
* obsługa zapisanych tabel w presetach.

Nie implementuj skanowania wavetable jako zwykłego crossfade dwóch przypadkowych próbek.

6. HARMONIC ENGINE

Zaimplementuj osobny moduł kontroli harmonicznych.

Użytkownik musi móc kontrolować strukturę harmoniczną sygnału bezpośrednio.

Liczba harmonicznych:

* minimum 32,
* docelowo 64.

Każda harmoniczna posiada:

* Amplitude
* Phase
* Enable

Dodatkowe parametry:

HARMONIC AMOUNT:

* 0–200%.

ODD HARMONICS:

* 0–200%.

EVEN HARMONICS:

* 0–200%.

HARMONIC ROLL-OFF:

* 0–100%.

HARMONIC TILT:

* -100 do +100%.

HARMONIC SPREAD:

* 0–100%.

HARMONIC PHASE:

* kontrola fazy harmonicznych.

HARMONIC RANDOM:

* 0–100%.
* deterministyczna zmienność faz lub amplitud.

Użytkownik powinien mieć możliwość:

* podbicia harmonicznych nieparzystych,
* podbicia parzystych,
* usunięcia wybranych harmonicznych,
* stworzenia niestandardowego widma,
* zapisania własnego profilu harmonicznego.

Wizualizacja musi pokazywać rzeczywiste harmoniczne generowanego sygnału.

7. UNISON ENGINE

Unison umożliwia tworzenie kilku lekko rozstrojonych głosów jednego oscylatora.

UNISON VOICES:

* 1–16.

DETUNE:

* 0–100%.

STEREO SPREAD:

* 0–100%.

UNISON BLEND:

* 0–100%.

UNISON MODE:

* CENTERED
* SYMMETRIC
* ASYMMETRIC
* RANDOM

PHASE MODE:

* RESET
* RANDOM
* FREE

Każdy głos unisonu musi posiadać własną fazę i częstotliwość.

Ważne:

* nie twórz unisonu przez prostą kopię identycznego sygnału,
* detuning musi rzeczywiście zmieniać częstotliwość głosów,
* RANDOM musi korzystać z kontrolowanego seeda,
* unison nie może powodować niekontrolowanego wzrostu głośności.

Dla SUB domyślnie jeden głos i wyłączony stereo spread.

Dla PUNCH domyślnie jeden lub dwa głosy.

Dla BODY i TAIL użytkownik może korzystać z szerszego unisonu.

8. PHASE ENGINE

Zaimplementuj niezależną kontrolę fazy.

PHASE:

* 0–360°.

PHASE OFFSET:

* zakres: 0–360°.

PHASE RESET:

* ON/OFF.

PHASE MODE:

* RESET
* RANDOM
* FREE RUN
* SYNCED

PHASE RANDOM:

* 0–100%.

PHASE ALIGN:

* możliwość synchronizacji wybranych oscylatorów.

PHASE INVERT:

* ON/OFF.

Zasady:

* każdy oscylator zachowuje własny stan fazy,
* RESET rozpoczyna głos od określonej fazy,
* FREE RUN pozwala na ciągłą pracę oscylatora,
* RANDOM losuje fazę przy każdym triggerze, ale tylko z kontrolowanym seedem,
* SYNCED synchronizuje fazę z wybranym źródłem.

Przygotuj możliwość podglądu relacji fazowej pomiędzy oscylatorami.

9. FM ENGINE

Zaimplementuj Frequency Modulation.

FM umożliwia modulowanie częstotliwości jednego oscylatora za pomocą drugiego.

Każdy oscylator może być:

* CARRIER,
* MODULATOR,
* oboma jednocześnie.

FM AMOUNT:

* 0–100%.

FM RATIO:

* 0.1–32.

FM FINE:

* -100 do +100%.

FM FEEDBACK:

* 0–100%.

FM ENVELOPE:

* możliwość przypisania obwiedni FM Amount.

FM ROUTING:

* OSC 1 → OSC 2
* OSC 2 → OSC 3
* OSC 3 → OSC 4
* dowolne połączenie w matrycy.

Przygotuj FM Matrix, w której użytkownik może określić połączenia pomiędzy oscylatorami.

Wymagania:

* modulacja musi odbywać się w domenie częstotliwości,
* FM Amount musi realnie wpływać na intensywność modulacji,
* FM Ratio określa stosunek częstotliwości modulatora do carriera,
* feedback musi być stabilny i ograniczony,
* FM nie może powodować niekontrolowanych skoków amplitudy.

FM musi być dostępne jako źródło do budowania metalicznych i agresywnych brzmień TAIL.

10. RING MODULATION

Zaimplementuj Ring Modulation.

Ring Modulation polega na mnożeniu dwóch sygnałów.

RING MOD SOURCE:

* wybór oscylatora źródłowego.

RING MOD TARGET:

* wybór oscylatora docelowego.

RING AMOUNT:

* 0–100%.

RING MIX:

* 0–100%.

RING FREQUENCY:

* możliwość określenia częstotliwości modulatora.

RING ENVELOPE:

* możliwość modulacji intensywności efektu.

Efekt powinien umożliwiać tworzenie metalicznych, nieharmonicznych i agresywnych struktur tonalnych.

Nie utożsamiaj Ring Modulation z FM. To dwa różne procesy i muszą posiadać niezależne algorytmy.

11. HARD SYNC

Zaimplementuj opcjonalny Hard Sync.

SYNC MASTER:

* wybór oscylatora nadrzędnego.

SYNC SLAVE:

* wybór oscylatora synchronizowanego.

SYNC AMOUNT:

* 0–100%.

SYNC MODE:

* HARD
* SOFT

Oscylator slave resetuje lub odpowiednio koryguje fazę na podstawie cyklu oscylatora master.

Umożliwia to tworzenie ostrzejszych, bardziej agresywnych harmonicznie przebiegów.

Hard Sync musi być zabezpieczony przed aliasingiem.

12. OSCILLATOR ROUTING

Każdy oscylator musi mieć możliwość przypisania do jednego lub kilku komponentów:

* PUNCH
* BODY
* TAIL
* SUB
* CUSTOM BUS

Routing:

* SINGLE
* MULTI
* SPLIT

SINGLE:
Jeden oscylator trafia do jednego komponentu.

MULTI:
Jeden oscylator może trafiać do kilku komponentów.

SPLIT:
Możliwość rozdzielenia sygnału na różne tory z niezależnymi poziomami.

Każde połączenie musi mieć kontrolę poziomu.

Nie twórz ukrytego kopiowania sygnału bez kontroli gainu.

13. OSCILLATOR MACROS

Przygotuj osiem makr przypisywanych do parametrów oscylatorów:

* OSC MIX
* HARMONICS
* DETUNE
* SPREAD
* FM
* RING
* PHASE
* WAVE MORPH

Makra muszą korzystać z systemu modulacji z Modułu 02.

Jedno makro może sterować wieloma parametrami.

Każde przypisanie posiada:

* MIN
* MAX
* CURVE
* POLARITY
* ENABLE.

14. GUI

Zaprojektuj osobny widok OSCILLATORS.

Widok główny:

* 4 oscylatory w osobnych sekcjach,
* ENABLE,
* WAVEFORM,
* LEVEL,
* OCTAVE,
* SEMITONE,
* FINE,
* PHASE,
* UNISON,
* ROUTING.

Dodatkowe zakładki:

* WAVEFORM
* WAVETABLE
* HARMONICS
* UNISON
* FM MATRIX
* RING MOD
* PHASE
* HARD SYNC

Centralny Waveform Display:

* prezentuje rzeczywisty przebieg,
* pozwala porównać kilka oscylatorów,
* umożliwia podgląd sygnału przed i po modulacji.

Spectrum Display:

* pokazuje rzeczywistą strukturę harmoniczną,
* aktualizuje się podczas zmian parametrów,
* umożliwia analizę wybranego oscylatora i sumy sygnałów.

Każda sekcja musi posiadać:

* MUTE,
* SOLO,
* RESET,
* możliwość wpisania wartości ręcznie.

15. PRESET SYSTEM

Utwórz osobną kategorię presetów oscylatorów.

Kategorie:

* CLEAN SINE
* CLASSIC HARDSTYLE
* AGGRESSIVE SAW
* METALLIC
* FM TONES
* SCREAM SOURCES
* EXPERIMENTAL
* SUB SOURCES

Użytkownik musi móc zapisywać:

* pojedynczy oscylator,
* zestaw oscylatorów,
* wavetable,
* profil harmoniczny,
* konfigurację FM,
* konfigurację unison.

Wszystkie ustawienia muszą być przywracane dokładnie.

16. DSP REQUIREMENTS

* Obsługa różnych sample rate.
* Anti-aliasing dla wszystkich nieciągłych przebiegów.
* Oversampling dla FM, ring modulation i hard sync tam, gdzie jest potrzebny.
* Band-limited wavetable.
* Interpolacja wavetable.
* Parametr smoothing.
* Ochrona przed DC offset.
* Ochrona przed denormal numbers.
* Brak alokacji pamięci w callbacku audio.
* Brak blokowania wątku audio.
* Deterministyczny random.
* Ochrona przed NaN i nieskończonymi wartościami.
* Kontrola gainu po unisonie i modulacji.
* Prawidłowe zarządzanie głosami MIDI.

Wszystkie oscylatory muszą być przygotowane do pracy z Pitch Engine z Modułu 02.

Zmiana pitchu musi aktualizować rzeczywistą częstotliwość oscylatora.

17. TESTY I KRYTERIA AKCEPTACJI

Moduł można uznać za ukończony, jeśli:

1. Działają wszystkie podstawowe przebiegi.
2. Można korzystać z minimum czterech niezależnych oscylatorów.
3. Każdy oscylator ma własny stan fazy.
4. Unison rzeczywiście generuje niezależne głosy.
5. Detune działa poprawnie.
6. Wavetable można ładować i skanować.
7. Harmonic Engine zmienia rzeczywistą strukturę widma.
8. FM działa jako modulacja częstotliwości.
9. Ring Modulation działa jako mnożenie sygnałów.
10. Hard Sync działa prawidłowo.
11. Routing pozwala przypisywać oscylatory do komponentów.
12. Spectrum Display prezentuje rzeczywisty sygnał.
13. Nie występują niekontrolowane aliasing i skoki głośności.
14. Presety zapisują pełną konfigurację.
15. Realtime i offline export dają zgodny rezultat.

18. ZAKAZ ATRAP

* Nie implementuj FM jako zwykłego filtra.
* Nie implementuj Ring Modulation jako zmiany głośności.
* Nie implementuj unisonu przez identyczne kopie sygnału.
* Nie implementuj wavetable jako zwykłego odtwarzania sampla.
* Nie twórz dekoracyjnego widma.
* Nie pokazuj funkcji, które nie są podłączone do DSP.
* Nie dopuszczaj do niekontrolowanego wzrostu poziomu sygnału.

19. REZULTAT KOŃCOWY

Po ukończeniu modułu użytkownik powinien móc:

* tworzyć dźwięk z wielu niezależnych oscylatorów,
* budować własne przebiegi,
* edytować harmoniczne,
* tworzyć szerokie i rozstrojone brzmienia,
* korzystać z FM i Ring Modulation,
* tworzyć metaliczne i agresywne źródła dźwięku,
* synchronizować fazę oscylatorów,
* kierować oscylatory do PUNCH, BODY, TAIL i SUB.

Moduł ma stanowić podstawę profesjonalnej syntezy dźwięku w 4GE.