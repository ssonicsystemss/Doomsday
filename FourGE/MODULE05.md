<!-- Owner specification of module 05 (2026-11-03), verbatim, COMPLETE (sent again in full the same evening). Plan: PLAN.md 1e. Claude notes at the end. -->
> **Zasada nadrzędna:** 4GE ma dokładnie 8 MASTER MACROS (IMPACT, BODY, TAIL, PITCH, GRIT, SCREAM, WEIGHT, CHAOS). Makra / kontrolery opisane w tym module są LOKALNE i sterowane przez Macro Mapping (M09) – patrz `RULES.md`.
# 4GE — HARDSTYLE KICK DESIGNER
## MODULE 05: DISTORTION & WAVESHAPER ENGINE

**Manufacturer:** Sonicus / Sonicsystems  
**Plugin:** 4GE  
**Module:** Distortion & Waveshaper Engine  
**Priority:** CRITICAL  
**Dependencies:** Module 01–04  
**Implementation:** C++ / JUCE

---

# 1. CEL MODUŁU

Zaimplementować profesjonalny, wielostopniowy silnik distortion przeznaczony do projektowania kicków hardstyle.

Silnik musi umożliwiać kontrolowane przekształcanie sygnału poprzez różne rodzaje nieliniowego przetwarzania.

Główne zastosowania:

- klasyczne hardstyle distortion,
- agresywny clipping,
- przesterowane tonalne BODY,
- brutalnie przesterowany TAIL,
- screech i metaliczne harmoniczne,
- cyfrowe zniekształcenia,
- asymetryczne przesterowanie,
- eksperymentalne struktury harmoniczne.

Podstawowe założenie:

**Użytkownik musi mieć pełną kontrolę nad charakterem przesterowania, jego intensywnością, kolejnością i miejscem zastosowania.**

Distortion Engine musi współpracować z:
- Oscillator Engine,
- Pitch Engine,
- Noise Engine,
- Transient Engine,
- Layer Engine.

Nie może być jedynie pojedynczym efektem na wyjściu całego kicka.

# 2. DISTORTION ARCHITECTURE

Utwórz główną klasę DistortionEngine.

System musi obsługiwać minimum 4 niezależne stopnie distortion.

Docelowa architektura powinna pozwalać na 8 stopni.

Każdy stopień posiada:
- ENABLE,
- TYPE,
- DRIVE,
- INPUT GAIN,
- OUTPUT GAIN,
- MIX,
- BIAS,
- SYMMETRY,
- PRE FILTER,
- POST FILTER,
- OVERSAMPLING,
- ROUTING.

Każdy stopień musi posiadać niezależny stan DSP.

Użytkownik powinien móc zmieniać kolejność stopni.

Przykład:

DISTORTION 1 → FILTER → DISTORTION 2 → DISTORTION 3

albo:

DISTORTION 1 → DISTORTION 2 → FILTER → DISTORTION 3

Kolejność przetwarzania musi rzeczywiście zmieniać dźwięk.

# 3. DISTORTION TYPES

Zaimplementuj następujące algorytmy.

## 3.1 SOFT CLIP

Miękkie ograniczanie amplitudy sygnału.

Charakter:
- łagodne nasycenie,
- stopniowe wprowadzanie harmonicznych,
- kontrolowane zaokrąglanie szczytów przebiegu.

Parametry:
- DRIVE: 0–100%.
- THRESHOLD: 0–100%.
- KNEE: 0–100%.
- OUTPUT: -24 do +12 dB.
- MIX: 0–100%.

Zastosowanie:
- Early Hardstyle,
- delikatne wzbogacenie BODY,
- kontrolowanie szczytów sygnału.

## 3.2 HARD CLIP

Twarde ograniczanie amplitudy po przekroczeniu określonego progu.

Parametry:
- DRIVE: 0–100%.
- CEILING: -24 do 0 dBFS.
- SYMMETRY: -100 do +100%.
- MIX: 0–100%.

Zastosowanie:
- Modern Hardstyle,
- Rawstyle,
- agresywny punch i tail.

Musi generować rzeczywiste zniekształcenia harmoniczne.

## 3.3 TUBE SATURATION

Symulacja nieliniowego charakteru lampowego.

Parametry:
- DRIVE: 0–100%.
- BIAS: -100 do +100%.
- WARMTH: 0–100%.
- EVEN HARMONICS: 0–100%.
- ODD HARMONICS: 0–100%.
- MIX: 0–100%.

Zastosowanie:
- cieplejsze BODY,
- tonalne TAIL,
- kontrolowane wzbogacanie harmonicznych.

Nie ograniczaj implementacji do zwykłego soft clippera z inną nazwą.

## 3.4 DIODE DISTORTION

Symulacja charakterystycznej nieliniowości układów diodowych.

Parametry:
- DRIVE: 0–100%.
- BIAS: -100 do +100%.
- ASYMMETRY: 0–100%.
- THRESHOLD: 0–100%.
- MIX: 0–100%.

Powinna generować charakterystyczne, bardziej agresywne zniekształcenia.

## 3.5 WAVEFOLDING

Zaimplementuj wavefolder.

Wavefolding powoduje wielokrotne zawijanie przebiegu po przekroczeniu określonych poziomów amplitudy.

Parametry:
- FOLD AMOUNT: 0–100%.
- FOLD THRESHOLD: 0–100%.
- FOLD BIAS: -100 do +100%.
- FOLD SYMMETRY: -100 do +100%.
- FOLD STAGES: 1–8.
- MIX: 0–100%.

Zastosowanie:
- Experimental Hardstyle,
- metaliczne TAIL,
- agresywne harmoniczne,
- nienaturalne tekstury tonalne.

Wizualizacja musi przedstawiać rzeczywistą transformację przebiegu.

## 3.6 BITCRUSH

Cyfrowe zniekształcenie przez redukcję rozdzielczości sygnału.

Parametry:
- BIT DEPTH: 1–24 bit.
- SAMPLE RATE REDUCTION: 1–100%.
- QUANTIZATION: 0–100%.
- DITHER: OFF / TPDF / NOISE.
- MIX: 0–100%.

Zastosowanie:
- digital raw,
- eksperymentalne kicki,
- ziarniste i agresywne tekstury.

Redukcja sample rate musi rzeczywiście zmieniać częstotliwość aktualizacji sygnału.

## 3.7 RECTIFIER

Zaimplementuj prostowanie sygnału.

Tryby:
- HALF WAVE,
- FULL WAVE,
- POSITIVE,
- NEGATIVE,
- CUSTOM.

Parametry:
- RECTIFY AMOUNT: 0–100%.
- BIAS: -100 do +100%.
- MIX: 0–100%.

Pozwala tworzyć nowe struktury harmoniczne poprzez zmianę symetrii przebiegu.

## 3.8 ASYMMETRIC DISTORTION

Niezależne przetwarzanie dodatniej i ujemnej części przebiegu.

Parametry:
- POSITIVE DRIVE: 0–100%.
- NEGATIVE DRIVE: 0–100%.
- POSITIVE THRESHOLD: 0–100%.
- NEGATIVE THRESHOLD: 0–100%.
- BIAS: -100 do +100%.
- MIX: 0–100%.

Zastosowanie:
- nietypowe harmoniczne,
- agresywne BODY,
- charakterystyczne Rawstyle TAIL.

## 3.9 FOLDBACK DISTORTION

Zaimplementuj foldback distortion, w którym przekroczenie określonego poziomu powoduje zawijanie sygnału.

Parametry:
- DRIVE: 0–100%.
- FOLD THRESHOLD: 0–100%.
- FOLD AMOUNT: 0–100%.
- BIAS: -100 do +100%.
- MIX: 0–100%.

Nie utożsamiaj Foldback z klasycznym Wavefolding. Powinny być osobnymi algorytmami z różnymi charakterystykami transferu.

## 3.10 DIGITAL CLIP

Cyfrowe ograniczanie szczytów.

Parametry:
- CEILING: -24 do 0 dBFS.
- DRIVE: 0–100%.
- HARDNESS: 0–100%.
- MIX: 0–100%.

Zastosowanie:
- agresywne kicki,
- kontrolowane obcinanie szczytów,
- nowoczesne przesterowane brzmienia.

## 3.11 CUSTOM WAVESHAPER

Najważniejsza funkcja dla zaawansowanych użytkowników.

Utwórz graficzny edytor krzywej transferu.

Oś X:
- poziom sygnału wejściowego.

Oś Y:
- poziom sygnału wyjściowego.

Użytkownik może:
- dodawać punkty,
- przesuwać punkty,
- usuwać punkty,
- rysować krzywą,
- tworzyć krzywe symetryczne,
- tworzyć krzywe asymetryczne,
- odwracać krzywą,
- resetować krzywą,
- zapisywać własne kształty.

Maksymalna liczba punktów: 128.

Interpolacja:
- LINEAR,
- CUBIC,
- BEZIER.

Ważne:
Krzywa musi być matematycznie bezpieczna. Należy zabezpieczyć ją przed nieprawidłowymi wartościami i niekontrolowanym wzmocnieniem sygnału.

Wizualizacja musi odpowiadać rzeczywiście używanej funkcji transferu.

# 4. DRIVE ENGINE

Utwórz wspólny system DRIVE.

DRIVE:
- zakres: 0–100%.

INPUT GAIN:
- zakres: -24 do +24 dB.

OUTPUT GAIN:
- zakres: -24 do +12 dB.

MIX:
- zakres: 0–100%.

DRIVE CURVE:
- LINEAR,
- EXPONENTIAL,
- LOGARITHMIC.

DRIVE RESPONSE:
- możliwość wyboru sposobu reakcji algorytmu na wartość DRIVE.

Zmiana DRIVE musi rzeczywiście zwiększać intensywność przetwarzania, a nie tylko głośność wyjściową.

## AUTO GAIN

ON/OFF.

ON:
System próbuje kompensować zmianę poziomu wynikającą z przesterowania.

OFF:
Użytkownik kontroluje poziom samodzielnie.

Auto Gain nie może automatycznie normalizować każdej próbki niezależnie, ponieważ powodowałoby to niepożądane zmiany dynamiki i modulację poziomu.

# 5. MULTI-STAGE DISTORTION

Utwórz graficzny system łączenia stopni distortion.

Minimum 4 niezależne stopnie.

Każdy stopień można:
- włączyć,
- wyłączyć,
- zmienić jego typ,
- przesunąć w kolejności,
- skopiować,
- zduplikować,
- zapisać jako preset,
- wybrać jego routing.

Przykładowe konfiguracje:

EARLY:
SOFT CLIP → TUBE SATURATION.

MODERN:
HARD CLIP → SOFT CLIP → DIGITAL CLIP.

RAWSTYLE:
ASYMMETRIC DISTORTION → HARD CLIP → WAVEFOLDING → DIGITAL CLIP.

EXPERIMENTAL:
FM SOURCE → WAVEFOLDING → RING MOD INPUT → CUSTOM WAVESHAPER.

Każda konfiguracja jest jedynie punktem startowym. Użytkownik musi mieć możliwość pełnej edycji.

# 6. PARALLEL DISTORTION

Zaimplementuj równoległe przetwarzanie sygnału.

Tryby:

SERIAL:
Sygnał przechodzi kolejno przez wszystkie stopnie.

PARALLEL:
Sygnał zostaje rozdzielony na kilka torów, które są przetwarzane niezależnie, a następnie sumowane.

MIXED:
Możliwość łączenia torów równoległych i szeregowych.

Każdy tor posiada:
- ENABLE,
- INPUT LEVEL,
- DISTORTION CHAIN,
- OUTPUT LEVEL,
- MIX,
- PHASE INVERT.

Musi istnieć możliwość blendowania sygnału czystego z przesterowanym.

Zaimplementuj kompensację poziomu na magistrali, aby przypadkowe podwojenie sygnału nie powodowało gwałtownego wzrostu głośności.

# 7. MULTIBAND DISTORTION

Zaimplementuj system wielopasmowego przesterowania.

Podział pasma:

- LOW,
- MID,
- HIGH.

Parametry podziału:
- LOW/MID CROSSOVER: 20–2000 Hz.
- MID/HIGH CROSSOVER: 200 Hz–20000 Hz.

Każde pasmo musi posiadać:
- ENABLE,
- DISTORTION TYPE,
- DRIVE,
- INPUT GAIN,
- OUTPUT GAIN,
- MIX,
- FILTER,
- OVERSAMPLING.

Użytkownik musi móc wybrać inny typ distortion dla każdego pasma.

Przykład:

LOW:
SOFT CLIP — kontrolowany dół.

MID:
ASYMMETRIC DISTORTION — agresywny środek.

HIGH:
WAVEFOLDING — metaliczna tekstura.

## CROSSOVER REQUIREMENTS

- Crossover musi zachowywać poprawną sumę sygnałów.
- Należy ograniczyć problemy fazowe przy ponownym łączeniu pasm.
- Preferowane są komplementarne filtry o kontrolowanej charakterystyce fazowej.
- Użytkownik musi mieć możliwość odsłuchania każdego pasma osobno.
- Zmiana crossovera nie może powodować niekontrolowanych skoków poziomu.

# 8. PRE-DISTORTION FILTER

Każdy stopień distortion może posiadać filtr przed przesterowaniem.

PARAMETRY:

FILTER TYPE:
- LP,
- HP,
- BP,
- NOTCH,
- LADDER,
- STATE VARIABLE.

CUTOFF:
- 20 Hz–20 kHz.

RESONANCE:
- 0–100%.

DRIVE:
- 0–100%.

SLOPE:
- 6–48 dB/oct.

Filtr musi umożliwiać określenie, które częstotliwości będą najmocniej reagowały na distortion.

Przykład:
Ograniczenie wysokich częstotliwości przed distortion pozwala zachować czytelniejszy atak, podczas gdy środek i dół mogą być mocniej przesterowane.

# 9. POST-DISTORTION FILTER

Każdy stopień distortion może posiadać filtr po przesterowaniu.

Parametry analogiczne do PRE-DISTORTION FILTER.

Dodatkowe funkcje:
- FILTER MIX,
- FILTER ENVELOPE,
- FILTER MODULATION.

PRE i POST FILTER muszą działać niezależnie.

Nie wolno traktować ich jako jednego filtra, którego położenie jest tylko zmieniane wizualnie.

# 10. DYNAMIC DISTORTION

Zaimplementuj możliwość dynamicznej modulacji distortion w czasie.

Źródła modulacji:
- Pitch Envelope,
- Amplitude Envelope,
- Filter Envelope,
- LFO,
- Macro,
- Velocity,
- własna obwiednia.

Cele modulacji:
- DRIVE,
- BIAS,
- THRESHOLD,
- FOLD AMOUNT,
- MIX,
- OUTPUT GAIN,
- FILTER CUTOFF.

Każde przypisanie posiada:
- SOURCE,
- DESTINATION,
- AMOUNT,
- MIN,
- MAX,
- CURVE,
- ENABLE.

Przykładowe zastosowanie:

DRIVE ENVELOPE:
Kick rozpoczyna się z umiarkowanym przesterowaniem, a w kolejnych milisekundach distortion stopniowo narasta.

FOLD ENVELOPE:
Wavefolding zwiększa intensywność w końcowej części TAIL.

BIAS ENVELOPE:
Asymetria zmienia się w czasie, tworząc bardziej złożony ruch harmoniczny.

Wszystkie modulacje muszą korzystać z systemu Modulation Matrix z Modułu 02.

# 11. DISTORTION PER LAYER

Distortion Engine musi być dostępny na kilku poziomach:

1. GLOBAL DISTORTION — cały kick.
2. GROUP DISTORTION — PUNCH, BODY, TAIL, SUB, TEXTURE.
3. LAYER DISTORTION — pojedyncza warstwa.

Każdy poziom musi posiadać własną konfigurację.

Użytkownik musi móc zdecydować, czy:
- distortion warstwy odbywa się przed sumowaniem,
- distortion grupy odbywa się po sumowaniu,
- distortion globalne odbywa się na końcu.

Nie wolno automatycznie stosować tego samego przesterowania na wszystkich poziomach.

Przy wielu aktywnych stopniach musi być widoczny całkowity poziom sygnału.

# 12. OVERSAMPLING ENGINE

Zaimplementuj niezależną kontrolę oversamplingu.

Dostępne ustawienia:

- OFF / 1x
- 2x
- 4x
- 8x
- 16x

Użytkownik może ustawić:
- GLOBAL OVERSAMPLING,
- PER STAGE OVERSAMPLING,
- OFFLINE QUALITY.

Oversampling powinien być dostępny przede wszystkim dla algorytmów generujących nowe harmoniczne.

Wymagania:
- poprawna interpolacja sygnału,
- filtr antyaliasingowy,
- poprawna decymacja,
- kontrola latencji,
- stabilne przełączanie ustawień,
- brak kliknięć przy zmianie jakości.

Nie implementuj oversamplingu jako zwykłego podniesienia częstotliwości próbkowania bez odpowiedniego filtrowania.

Offline render może wykorzystywać wyższą jakość niż realtime, ale powinien dawać przewidywalny rezultat.

# 13. DISTORTION MORPH

Zaimplementuj funkcję płynnego przechodzenia pomiędzy charakterami distortion.

MORPH A:
Pierwsza konfiguracja.

MORPH B:
Druga konfiguracja.

MORPH AMOUNT:
0–100%.

Użytkownik może przechodzić pomiędzy:
- SOFT CLIP,
- HARD CLIP,
- TUBE,
- WAVEFOLD,
- ASYMMETRIC,
- CUSTOM.

Morph powinien interpolować parametry, które mają ciągły charakter.

W przypadku różnych algorytmów możliwe jest równoległe przetwarzanie obu konfiguracji i płynne mieszanie ich wyjść.

Nie wolno udawać płynnego morphingu poprzez zmianę nazwy algorytmu lub natychmiastowe przełączenie pomiędzy dwoma trybami.

# 14. DISTORTION VISUALIZER

Zaimplementuj trzy widoki graficzne.

## TRANSFER CURVE

Pokazuje zależność pomiędzy sygnałem wejściowym i wyjściowym.

Musi aktualizować się podczas zmiany DRIVE, BIAS, SYMMETRY i innych parametrów.

## WAVEFORM BEFORE / AFTER

Porównanie przebiegu przed i po distortion.

Musi pokazywać rzeczywisty sygnał.

## SPECTRUM ANALYZER

Pokazuje widmo harmoniczne przed i po przesterowaniu.

Wymagania:
- FFT,
- możliwość zamrożenia widma,
- regulacja zakresu,
- wybór kanału,
- możliwość odsłuchu przetwarzanego pasma.

# 15. DISTORTION MACROS

Przygotuj osiem makr:

- GRIT,
- HARDNESS,
- WARMTH,
- FOLD,
- ASYMMETRY,
- CRUSH,
- HARMONICS,
- TOTAL DRIVE.

Każde makro może sterować wieloma parametrami różnych stopni.

Przypisania:
- MIN,
- MAX,
- CURVE,
- POLARITY,
- ENABLE.

Makra muszą być dostępne z głównego interfejsu 4GE.

# 16. PRESET SYSTEM

Utwórz bibliotekę presetów distortion.

Kategorie:

EARLY:
- CLASSIC SATURATION,
- SOFT PUNCH,
- VINTAGE CLIP.

MODERN:
- MODERN HARD CLIP,
- CLEAN AGGRESSION,
- DENSE DISTORTION.

RAWSTYLE:
- RAW ASYMMETRIC,
- BRUTAL FOLD,
- HARD DIGITAL,
- CRUSHED TAIL.

EXPERIMENTAL:
- METALLIC FOLD,
- FM CRUSH,
- DIGITAL CHAOS,
- COMPLEX HARMONICS.

UTILITY:
- SUB CLEANUP,
- PUNCH CONTROL,
- HIGH TEXTURE,
- MASTER CLIP.

Użytkownik musi móc zapisywać:
- pojedynczy stopień,
- cały łańcuch,
- konfigurację multiband,
- konfigurację parallel distortion,
- pełny preset distortion.

# 17. SIGNAL FLOW

Docelowa architektura:

SOURCE GENERATORS
↓
PRE FILTER
↓
DISTORTION STAGE 1
↓
FILTER
↓
DISTORTION STAGE 2
↓
PARALLEL / MULTIBAND ROUTING
↓
DISTORTION STAGE 3
↓
DISTORTION STAGE 4
↓
POST FILTER
↓
OUTPUT GAIN
↓
NEXT PROCESSING MODULE

Każdy element musi być możliwy do pominięcia.

Routing musi być jednoznaczny i nie może tworzyć niekontrolowanych pętli sygnałowych.

# 18. DSP REQUIREMENTS

- Wszystkie algorytmy muszą mieć rzeczywistą implementację matematyczną.
- Każdy algorytm musi mieć odpowiedni zakres stabilnej pracy.
- Zabezpieczenie przed aliasingiem.
- Oversampling dla odpowiednich procesów nieliniowych.
- Zabezpieczenie przed DC offset.
- Zabezpieczenie przed denormal numbers.
- Ochrona przed NaN i nieskończonymi wartościami.
- Parameter smoothing.
- Brak dynamicznej alokacji pamięci w callbacku audio.
- Brak blokowania wątku audio.
- Kontrola poziomu wejściowego i wyjściowego.
- Prawidłowa kompensacja latencji.
- Stabilne przełączanie algorytmów.
- Obsługa różnych sample rate.
- Obsługa mono i stereo.
- Deterministyczny rezultat przy identycznych ustawieniach.
- Realtime i offline export muszą być zgodne.

Każdy stopień musi posiadać niezależny stan filtrów i procesorów.

# 19. TESTY I KRYTERIA AKCEPTACJI

Moduł można uznać za ukończony, jeśli:

1. Działa minimum 10 rodzajów distortion.
2. Custom Waveshaper pozwala tworzyć własne krzywe.
3. Dostępne są minimum 4 niezależne stopnie.
4. Kolejność stopni zmienia rzeczywisty sygnał.
5. Działa routing szeregowy i równoległy.
6. Działa multiband distortion.
7. PRE i POST FILTER działają niezależnie.
8. Dynamic Distortion reaguje na obwiednie.
9. Distortion działa osobno na warstwach i grupach.
10. Oversampling rzeczywiście ogranicza aliasing.
11. Transfer Curve pokazuje rzeczywistą charakterystykę.
12. Spectrum Analyzer pokazuje rzeczywiste widmo.
13. Morph działa bez nagłych skoków parametrów.
14. Presety zapisują pełną konfigurację.
15. Nie występują niekontrolowane skoki poziomu.
16. Nie występują trzaski przy zmianie ustawień.
17. CPU pozostaje pod kontrolą przy maksymalnej liczbie stopni.
18. Realtime i offline export dają zgodny rezultat.

# 20. ZAKAZ ATRAP

- Nie implementuj wszystkich algorytmów jako jednego tanh z różnymi nazwami.
- Nie implementuj Wavefoldingu jako zwykłego clippingu.
- Nie implementuj FM ani Ring Modulation jako distortion.
- Nie implementuj Multiband jako trzech niezależnych pokręteł bez podziału pasma.
- Nie implementuj Parallel jako zwykłego zwiększania MIX.
- Nie implementuj Auto Gain jako normalizacji każdej próbki.
- Nie pokazuj dekoracyjnych krzywych transferu.
- Nie pomijaj oversamplingu przy mocno nieliniowych algorytmach.
- Nie pozwalaj na niekontrolowany wzrost poziomu sygnału.

Każdy parametr musi wpływać na rzeczywisty algorytm DSP.

# 21. REZULTAT KOŃCOWY

Po ukończeniu modułu użytkownik powinien móc:

- wybierać spośród wielu rodzajów distortion,
- budować wielostopniowe łańcuchy przesterowania,
- tworzyć własne krzywe Waveshaper,
- stosować distortion szeregowo i równolegle,
- niezależnie przesterowywać LOW, MID i HIGH,
- modulować DRIVE i inne parametry w czasie,
- przetwarzać każdą warstwę osobno,
- kontrolować aliasing przez oversampling,
- wizualnie analizować charakter przesterowania,
- tworzyć własne presety distortion.

Distortion Engine ma być jednym z głównych elementów wyróżniających 4GE na tle innych narzędzi do projektowania hardstyle kicków.
---
## NOTES (Claude, 2026-11-03) – decyzje techniczne
- 11 typów = osobne funkcje transferu (nie jeden tanh): bazą `Shared/DoomsdayLab.h` (ADAA: saturation / diode / wavefold / rectifier / bitcrush / custom table) + nowe: Tube (bias + osobne even / odd), Foldback (odbicie od progu ≠ sinusowy wavefold), Digital Clip (hardness), Asymmetric (osobne drive / próg dla półfal).
- Modulacja (pkt 10) przez CENTRALNĄ matrycę z Modułu 09 (spec mówi „Modułu 02”, ale wspólny Modulation Engine jest w M09 – jeden system).
- Multiband distortion = te same crossovery (Linkwitz-Riley + korekcja fazy) co M07 – jedna klasa. Pre / post filter = filtry z M06 (osobne instancje, nie jeden filtr przesuwany).
- Oversampling: jedna strefa na głos (M06 / M08 korzystają z niej), QUALITY realtime / OFFLINE; per stage tylko tam, gdzie generuje harmoniczne.
- Auto Gain jak w M08: pomiar w oknie (RMS przed / po), wygładzona korekcja.
- Morph A / B: ciągłe parametry interpolowane, różne algorytmy = oba tory równolegle + crossfade (jak RE4GE MORPH, ale tu z miksem wyjść zamiast skoku przy 50 %).
- Makra M05 (GRIT, HARDNESS, WARMTH, FOLD, ASYMMETRY, CRUSH, HARMONICS, TOTAL DRIVE) – tak jak M06–M08: mapy przypisań do wspólnych slotów makr (pytanie na koniec).
- Per-layer routing + SUB czysty domyślnie – zgodnie z obowiązkową uwagą właściciela (PLAN, test THD).
