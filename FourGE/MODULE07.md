<!-- Owner specification of module 07 (2026-11-03), collected from chat piece by piece. Plan: PLAN.md. -->
# 4GE — MODULE 07: EQ, MULTIBAND PROCESSING & SPECTRAL SHAPING ENGINE

Cel modułu: stworzenie profesjonalnego systemu kształtowania widma, który pozwoli precyzyjnie kontrolować częstotliwości wszystkich elementów kicka — od subbasu i uderzenia PUNCH, przez BODY, aż po wysokie harmoniczne, screech i ogon TAIL.
Moduł ma być czymś znacznie więcej niż zwykłym korektorem graficznym. Powinien łączyć klasyczny EQ, dynamiczne EQ, wielopasmowe przetwarzanie oraz zaawansowane narzędzia do kształtowania harmonicznych i widma.
Najważniejsza zasada: użytkownik musi móc zarówno subtelnie poprawić brzmienie kicka, jak i radykalnie zmienić jego charakter tonalny. Wszystkie operacje muszą działać na rzeczywistym sygnale audio.

## 1. Architektura modułu
Pięć głównych silników:
1. PARAMETRIC EQ — klasyczny, wielopunktowy korektor parametryczny.
2. DYNAMIC EQ — korektor reagujący na poziom sygnału.
3. MULTIBAND ENGINE — niezależne przetwarzanie pasm częstotliwościowych.
4. SPECTRAL SHAPER — zaawansowane kształtowanie widma i harmonicznych.
5. STEREO FREQUENCY ENGINE — kontrola szerokości i rozmieszczenia częstotliwości w stereo.
Każdy silnik niezależny, ale wszystkie współpracują z pozostałymi modułami 4GE.

Schemat przepływu sygnału (domyślny):
PUNCH / BODY / TAIL / SUB / TEXTURE → PARAMETRIC EQ → DYNAMIC EQ → MULTIBAND PROCESSING → SPECTRAL SHAPER → STEREO FREQUENCY ENGINE → OUTPUT
To domyślna konfiguracja. Użytkownik musi móc zmienić kolejność i routing modułów.

## 2. PARAMETRIC EQ — korektor parametryczny
### 2.1. Liczba pasm
- Minimum 8 niezależnych pasm EQ, docelowo 16.
- Każde pasmo ma osobny zestaw parametrów.
- Pasma dowolnie rozmieszczane w zakresie 20 Hz–20 kHz.
- Aktywowanie / dezaktywowanie poszczególnych pasm.
- Kopiowanie, duplikowanie i usuwanie pasm.
### 2.2. Typy filtrów EQ
| Typ | Funkcja |
|---|---|
| Bell | Podbijanie lub wycinanie wybranego pasma |
| Low Shelf | Regulacja niskich częstotliwości |
| High Shelf | Regulacja wysokich częstotliwości |
| Low Cut | Odcinanie niskich częstotliwości |
| High Cut | Odcinanie wysokich częstotliwości |
| Band Pass | Przepuszczanie wybranego pasma |
| Notch | Precyzyjne wycinanie częstotliwości |
| Tilt EQ | Jednoczesne rozjaśnianie i przyciemnianie |
| Resonant Peak | Wzmocnienie wybranego rezonansu |
Każdy typ musi posiadać własną prawidłową charakterystykę.
### 2.3. Parametry każdego pasma
| Parametr | Zakres |
|---|---|
| ENABLE | ON/OFF |
| TYPE | Lista typów |
| FREQUENCY | 20 Hz–20 kHz |
| GAIN | -24 do +24 dB |
| Q | 0.1–30 |
| SLOPE | 6–48 dB/oct |
| MIX | 0–100% |
| STEREO MODE | Stereo / Left / Right / Mid / Side |
Dla typów filtrów, które nie obsługują GAIN lub Q, odpowiednie parametry muszą zostać wyłączone.
### 2.4. Charakterystyka i jakość dźwięku
- Precyzyjna regulacja częstotliwości.
- Płynne zmiany gain bez kliknięć.
- Stabilność przy ekstremalnych wartościach Q.
- Brak niepotrzebnych zmian fazowych poza charakterystyką danego filtra.
- Możliwość wyboru trybu Linear Phase, jeżeli implementacja jest dostępna i uzasadniona.
- W trybie minimum phase poprawne zachowanie fazowe.
- Odpowiednia kompensacja latencji w trybie linear phase.
Linear Phase nie może być domyślnym trybem dla każdego zastosowania – przy bardzo krótkich transientach może powodować pre-ringing, więc użytkownik wybiera tryb odpowiedni do materiału.

## 3. DYNAMIC EQ — dynamiczna korekcja
Dynamic EQ musi reagować na rzeczywisty poziom sygnału w wybranym paśmie.
Przykład: jeśli kick ma zbyt agresywny rezonans przy 3,5 kHz, Dynamic EQ automatycznie ogranicza tę częstotliwość tylko wtedy, kiedy rezonans staje się zbyt mocny.
### 3.1. Parametry
| Parametr | Zakres |
|---|---|
| THRESHOLD | -60 do 0 dBFS |
| RANGE | -24 do +24 dB |
| ATTACK | 0.1–500 ms |
| RELEASE | 1–2000 ms |
| RATIO | 1:1–20:1 |
| KNEE | 0–24 dB |
| FREQUENCY | 20 Hz–20 kHz |
| Q | 0.1–30 |
| MIX | 0–100% |
### 3.2. Tryby działania
- DYNAMIC CUT — automatyczne ograniczanie wybranego pasma.
- DYNAMIC BOOST — automatyczne podbijanie pasma.
- UPWARD DYNAMIC — wzmacnianie pasma poniżej ustalonego poziomu.
- DOWNWARD DYNAMIC — ograniczanie pasma powyżej ustalonego poziomu.
- SIDECHAIN EQ — detektor sterowany z innego źródła.
### 3.3. Detektor
Peak Detection, RMS Detection, wybór długości okna detekcji, regulacja Attack i Release, filtr sidechain, wybór źródła detekcji.
Źródła detekcji: aktualne pasmo, pełny sygnał kicka, inna grupa, zewnętrzny sygnał sidechain (jeśli host udostępnia wejście).
### 3.4. Integracja z kickiem
Dynamic EQ może działać osobno na PUNCH, BODY, TAIL, SUB i TEXTURE.
Przykłady: ograniczanie zbyt mocnego kliknięcia w PUNCH, kontrolowanie agresywnego rezonansu BODY, redukowanie syczących częstotliwości SCREECH, utrzymywanie stabilnego subbasu, dynamiczne rozjaśnianie ogona.

## 4. MULTIBAND ENGINE — wielopasmowe przetwarzanie
Silnik dzieli sygnał na osobne zakresy częstotliwości, które można niezależnie przetwarzać.
### 4.1. Podział pasm
Domyślnie 4 pasma:
| Pasmo | Zakres początkowy | Typowe zastosowanie |
|---|---|---|
| SUB | 20–80 Hz | Fundament niskich częstotliwości |
| LOW | 80–250 Hz | Masa i uderzenie |
| MID | 250 Hz–2,5 kHz | Charakter BODY |
| HIGH | 2,5–20 kHz | Klik, screech, harmoniczne |
To tylko ustawienia początkowe – użytkownik dowolnie przesuwa punkty podziału.
Wymagane: minimum 4 niezależne pasma, docelowo 6; wyłączenie dowolnego pasma; łączenie pasm; solo i mute każdego pasma; odsłuch pasm przed i po przetwarzaniu.
### 4.2. Crossover Engine
Punkty podziału korzystają z poprawnych filtrów crossoverowych. Tryby: Linkwitz-Riley 12 dB/oct, Linkwitz-Riley 24 dB/oct, Linkwitz-Riley 48 dB/oct, Butterworth, Bessel, Custom.
Możliwie płaska suma częstotliwościowa po ponownym połączeniu pasm – bez niezamierzonych dziur lub podbić w okolicach crossoverów.
Wymagania techniczne: komplementarne charakterystyki filtrów, kontrola fazy, poprawna rekonstrukcja sygnału, smoothowanie zmian crossoverów, brak kliknięć przy przesuwaniu punktów podziału, obsługa różnych częstotliwości próbkowania.
### 4.3. Procesory dostępne dla każdego pasma
Każde pasmo ma własny zestaw: Gain, EQ, Saturation Send, Distortion Send, Filter Send, Dynamic Control, Stereo Width, Phase Invert, Mute, Solo.
Bez osobnych kopii silników distortion i filter – korzystać z instancji i punktów routingu z Module 05 i Module 06.
### 4.4. Multiband Dynamics
Każde pasmo może posiadać własny procesor dynamiki.
| Parametr | Zakres |
|---|---|
| THRESHOLD | -60 do 0 dBFS |
| RATIO | 1:1–20:1 |
| ATTACK | 0.1–500 ms |
| RELEASE | 1–2000 ms |
| KNEE | 0–24 dB |
| MAKEUP | -24 do +24 dB |
| MIX | 0–100% |
Dodatkowe tryby: Compressor, Expander, Gate, Limiter, Transient Control.
Transient Control osobno wzmacnia lub ogranicza atak w wybranym paśmie – na rzeczywistej detekcji obwiedni, a nie zwykłym podbiciu EQ.
### 4.5. Przykładowe zastosowania
- Rawstyle: SUB – kontrola stabilności dołu; LOW – mocny, skoncentrowany fundament; MID – agresywne harmoniczne; HIGH – wyraźny, ostry atak i screech.
- Early Hardstyle: SUB – czysty dół; LOW – zaokrąglone BODY; MID – subtelne harmoniczne; HIGH – ograniczone, kontrolowane wysokie częstotliwości.
- Experimental: niezależne przesterowanie pasm, przesuwanie punktów crossover, szerokie manipulacje widmem, nietypowe relacje fazowe.

## 5. SPECTRAL SHAPER — zaawansowane kształtowanie widma
Silnik zmieniający rozkład energii częstotliwościowej w sposób bardziej zaawansowany niż klasyczny EQ.
Nie może być wyłącznie kolejnym korektorem parametrycznym pod inną nazwą.
### 5.1. Spectral Tilt
Sterowanie ogólnym balansem tonalnym.
| Parametr | Zakres |
|---|---|
| TILT | -100 do +100% |
| PIVOT FREQUENCY | 20 Hz–20 kHz |
| INTENSITY | 0–100% |
| MIX | 0–100% |
Wartość ujemna: ciemniejsze brzmienie; dodatnia: jaśniejsze. Pivot Frequency = częstotliwość, wokół której następuje zmiana balansu.
### 5.2. Harmonic Shaper
Kształtowanie istniejących harmonicznych. Parametry: EVEN HARMONICS, ODD HARMONICS, LOW HARMONICS, HIGH HARMONICS, HARMONIC DRIVE, HARMONIC MIX, HARMONIC BALANCE.
Użytkownik kontroluje udział parzystych i nieparzystych harmonicznych.
Może korzystać z nieliniowych algorytmów generowania harmonicznych, ale musi rzeczywiście zmieniać zawartość widma – nie jedynie regulacja głośności pasma.
### 5.3. Spectral Compression
Ogranicza nadmierne skupienie energii w wybranych częstotliwościach.
Parametry: SENSITIVITY, THRESHOLD, REDUCTION, ATTACK, RELEASE, FREQUENCY RANGE, SPECTRAL SMOOTHING, MIX.
Działanie: 1. analizuje rozkład energii częstotliwościowej; 2. wykrywa pasma przekraczające określony poziom; 3. ogranicza ich energię; 4. zachowuje pozostałe części widma.
Jeśli korzysta z FFT: poprawne okienkowanie, overlap-add i kontrola artefaktów; bez niepotrzebnego opóźnienia i metalicznych artefaktów przy krótkich sygnałach.
### 5.4. Spectral Exciter
Dodaje harmoniczne lub teksturę w określonym zakresie częstotliwości.
| Parametr | Zakres |
|---|---|
| FREQUENCY RANGE | 20 Hz–20 kHz |
| EXCITATION | 0–100% |
| COLOR | 0–100% |
| DRIVE | 0–100% |
| HARMONIC MODE | Even / Odd / Both |
| MIX | 0–100% |
Tryby: WARM, BRIGHT, CRISP, METALLIC, AGGRESSIVE, CUSTOM.
Exciter generuje dodatkową zawartość harmoniczną lub odpowiednio wzmacnia istniejące składowe – nie wolno go zrobić jako zwykłego high-shelf EQ.
### 5.5. Spectral Freeze — funkcja eksperymentalna
Opcjonalne zamrażanie widma: analizuje aktualne widmo, zapisuje jego rozkład, utrzymuje wybraną teksturę, pozwala modulować zapisane widmo.
Parametry: FREEZE, CAPTURE TIME, SPECTRAL DECAY, SMOOTHING, PITCH SHIFT, MIX.
Może być oznaczona jako EXPERIMENTAL, ale jeśli jest w GUI – musi rzeczywiście działać.

## 6. STEREO FREQUENCY ENGINE — kontrola częstotliwości w stereo
Kick ma możliwość precyzyjnego kontrolowania szerokości poszczególnych zakresów częstotliwości.
### 6.1. Stereo Width
| Parametr | Zakres |
|---|---|
| WIDTH | 0–200% |
| LOW WIDTH | 0–200% |
| MID WIDTH | 0–200% |
| HIGH WIDTH | 0–200% |
| MONO BELOW | 20–500 Hz |
| STEREO MIX | 0–100% |
0% = mono, 100% = oryginalna szerokość, 200% = poszerzenie.
### 6.2. Mid/Side Processing
Każde pasmo: MID GAIN, SIDE GAIN, MID EQ, SIDE EQ, MID WIDTH, SIDE WIDTH.
Przykłady: subbas całkowicie w centrum, BODY w umiarkowanym stereo, poszerzone wysokie harmoniczne, charakter screechy rozdzielony pomiędzy kanały.
### 6.3. Mono Compatibility
Obowiązkowe: kontrola korelacji stereo, odsłuch mono, wykrywanie nadmiernych problemów fazowych, ograniczenie szerokości, wymuszenie mono poniżej ustawionej częstotliwości.
Nie wolno automatycznie poszerzać subbasu tylko dlatego, że użytkownik zwiększył ogólny WIDTH.

## 7. FREQUENCY MACROS — makra modułu
| Makro | Funkcja |
|---|---|
| BODY | Wzmacnia lub kształtuje środkową część kicka |
| SUB CONTROL | Kontroluje energię najniższych częstotliwości |
| BRIGHTNESS | Rozjaśnia widmo |
| CLARITY | Ogranicza maskowanie częstotliwości |
| HARMONICS | Zwiększa udział harmonicznych |
| AGGRESSION | Podkreśla agresywne pasma |
| WIDTH | Steruje szerokością stereo |
| TONE | Ogólny balans tonalny |
Każde makro musi mieć konfigurowalną mapę parametrów.
Przykład: BRIGHTNESS jednocześnie zwiększa High Shelf, zmienia Spectral Tilt, zwiększa udział wysokich harmonicznych, delikatnie zmienia cutoff w odpowiedniej sekcji – ale nie może powodować niekontrolowanego wzrostu poziomu sygnału.

## 8. ROUTING I INTEGRACJA Z 4GE
Trzy poziomy: 1. GLOBAL — korekcja całego kicka; 2. GROUP — PUNCH, BODY, TAIL, SUB, TEXTURE; 3. LAYER — pojedyncza warstwa z Module 04.
Punkty routingu: przed / po Distortion Engine, przed / po Filter Engine, przed / po Screech Engine, przed Master Output.
Własne łańcuchy i zmiana kolejności procesorów.
Przykład Rawstyle: `SUB → EQ → MULTIBAND → DISTORTION → SPECTRAL SHAPER → MASTER`
Przykład screechy: `SCREECH → FILTER → DISTORTION → DYNAMIC EQ → STEREO ENGINE → TAIL`
Routing nie może tworzyć niejawnych kopii sygnału. Każda gałąź równoległa ma kontrolę poziomu i wyrównanie latencji.

## 9. GUI — interfejs użytkownika
Pięć zakładek: PARAMETRIC, DYNAMIC, MULTIBAND, SPECTRAL, STEREO.
### 9.1. Główne okno EQ
Koncept właściciela: `Design/M07_eq_concept.png`. Okno „4GE / EQ & SPECTRAL”: pasek 5 zakładek u góry; FREQUENCY RESPONSE (8-BAND EQ) – krzywa z punktami pasm, oś 20 Hz – 1 kHz – 20 kHz; pod spodem pola wybranego pasma: FREQUENCY (np. 120 Hz), GAIN (+3.5 dB), Q (1.20), TYPE (Bell), MIX (100%), MODE (Stereo); na dole MULTIBAND OVERVIEW – 4 kolorowe kafelki SUB 20–80 Hz / LOW 80–250 Hz / MID 250–2.5k / HIGH 2.5–20k.
Ilustracja koncepcyjna – wszystkie wykresy w finalnej wtyczce muszą przedstawiać rzeczywiste dane DSP.
### 9.2. Wymagane funkcje interfejsu
- Przeciąganie punktów EQ myszą; podwójne kliknięcie punktu = edycja wartości.
- Zoom i przesuwanie wykresu.
- Solo i bypass poszczególnych pasm.
- Spectrum Analyzer z regulacją szybkości i rozdzielczości; Pre/Post Spectrum Comparison.
- Undo/Redo, A/B Compare, Copy/Paste ustawień.
- MIDI Learn, automatyzacja DAW, Preset Browser, reset pojedynczego parametru.
