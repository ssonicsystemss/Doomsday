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
