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
