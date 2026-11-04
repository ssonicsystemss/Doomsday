<!-- Owner specification of module 11 (2026-11-04), collected from chat piece by piece. Plan: PLAN.md 1k. -->
> **Zasada nadrzędna:** 4GE ma dokładnie 8 MASTER MACROS (IMPACT, BODY, TAIL, PITCH, GRIT, SCREAM, WEIGHT, CHAOS). Makra / kontrolery opisane w tym module są LOKALNE i sterowane przez Macro Mapping (M09) – patrz `RULES.md`.
# 4GE — MODULE 11: STEREO & SPATIAL ENGINE
Specyfikacja techniczna dla AI / developera · SonicSystems • 4GE Kick Synthesizer • Module 11

## 1. CEL MODUŁU
Stereo & Spatial Engine odpowiada za kontrolę przestrzeni stereo, pozycjonowanie warstw, szerokość dźwięku, relacje fazowe oraz zgodność sygnału z odsłuchem mono.
Precyzyjna kontrola obrazu stereo na trzech poziomach: pojedyncza warstwa; grupa PUNCH / BODY / TAIL / SUB / TEXTURE / CUSTOM; cały instrument — MASTER.
Cel: szerokie, przestrzenne brzmienie hardstyle bez utraty kontroli nad środkiem, dołem pasma i kompatybilnością mono.
Zasada projektowa: szerokość stereo nie może być uzyskiwana kosztem przypadkowego odwracania fazy, niekontrolowanego opóźniania kanałów lub niepożądanego wzrostu poziomu.

## 2. PODSTAWOWE PARAMETRY STEREO
- PAN: -100% (całkowicie lewy) / 0% (środek) / +100% (całkowicie prawy). Dla źródeł mono panoramowanie o stałej mocy lub inna jasno udokumentowana krzywa.
- BALANCE: proporcja głośności L/R. Nie może być kopią funkcjonalną PAN – PAN przesuwa pozycję źródła, BALANCE zmienia proporcję kanałów.
- STEREO WIDTH: 0–200% (0% mono, 100% oryginał, 200% maksimum danego algorytmu). Bez niekontrolowanego wzrostu poziomu.
- CHANNEL SWAP: zamiana L i R – funkcja diagnostyczna i kreatywna.

## 3. MID/SIDE ENGINE
Pełna obsługa Mid/Side. Sygnał dzielony na MID (centralna część) i SIDE (różnica między kanałami).
| Parametr | Zakres |
|---|---|
| MID LEVEL | -24 do +12 dB |
| SIDE LEVEL | -24 do +12 dB |
| M/S WIDTH | 0–200% |
| MID SOLO | ON/OFF |
| SIDE SOLO | ON/OFF |
| M/S ENABLE | ON/OFF |
Przy 0% WIDTH sygnał mono; przy 100% WIDTH nominalna relacja Mid/Side. Zmiany poziomu Mid i Side wygładzane (bez kliknięć).

## 4. LOW FREQUENCY MONO
Jedna z najważniejszych funkcji modułu. Hardstyle wymaga stabilnego, centralnego dołu; szerokość przydatna w górnych harmonicznych, screechach i teksturach, ale nie może destabilizować subbasu.
- LOW FREQUENCY MONO: ON/OFF.
- CROSSOVER FREQUENCY: 20–300 Hz, domyślnie 120 Hz. Wszystko poniżej crossovera → mono. Odpowiednio dobrane filtry ograniczające zmiany fazowe i skoki poziomu.
- SUB MONO LOCK: ON/OFF. Aktywny → grupa SUB mono i centralna niezależnie od szerokości innych grup; wyższy priorytet niż globalny WIDTH.

## 5. PHASE & POLARITY ENGINE
- POLARITY INVERT: Left, Right, Both, Selected Layer.
- CHANNEL DELAY: opóźnienie L lub R w Samples / Milliseconds – precyzyjna korekta i kreatywne kształtowanie przestrzeni.
- PHASE OFFSET: tylko dla źródeł, gdzie architektura pozwala na poprawną realizację; bez pozornego parametru dla każdego sygnału.
- PHASE ALIGNMENT: analiza relacji fazowej między warstwami – korelacja, porównanie początku przebiegów, wykrywanie problemów fazowych, opcjonalne dopasowanie offsetu warstw. Automatyczne dopasowanie nie nadpisuje ustawień bez potwierdzenia użytkownika.

## 6. STEREO WIDTH ALGORITHMS
- MODE 01 — M/S WIDTH: podstawowy, przewidywalny – regulacja poziomu Side.
- MODE 02 — CONTROLLED DECORRELATION: kontrolowana dekorelacja kanałów; niewielkie różnice L/R, ograniczone ryzyko grzebieniowania i problemów fazowych.
- MODE 03 — HAAS EFFECT: opcjonalny tryb kreatywny (różnica czasowa L/R); DELAY TIME, WIDTH, MIX, CHANNEL SELECT, MONO CHECK; oznaczony jako efekt kreatywny, który może pogorszyć zgodność mono.
- MODE 04 — DISABLED: wyłączenie przetwarzania przestrzennego.
Każdy tryb: MIX 0–100%, OUTPUT GAIN, BYPASS, MONO AUDITION.

## 7. STEREO ROUTING
Przetwarzanie dostępne na poziomie Layer, Group i Master.
Przykładowa architektura:
- PUNCH → STEREO CONTROL → PUNCH GROUP OUTPUT
- BODY → STEREO CONTROL → BODY GROUP OUTPUT
- TAIL → STEREO CONTROL → TAIL GROUP OUTPUT
- SUB → SUB MONO LOCK → SUB GROUP OUTPUT
- wszystkie grupy → MASTER BUS → MASTER STEREO → OUTPUT
Każda grupa ma niezależne ustawienia PAN, WIDTH i pozostałych dostępnych parametrów.

## 8. MODULATION MATRIX INTEGRATION
Cele modulacji: PAN, WIDTH, MID LEVEL, SIDE LEVEL, LOW MONO CROSSOVER, CHANNEL DELAY, M/S WIDTH. Modulacja wygładzana.
Przykłady: LFO → szerokość grupy TAIL; Velocity → poziom SIDE; Envelope zwiększa szerokość tekstury w końcowej fazie kicka; Macro → globalny WIDTH.
Bez gwałtownych skoków parametrów powodujących słyszalne artefakty.

## 9. STEREO VISUALIZER
Wizualizacja rzeczywistego obrazu stereo:
- STEREO FIELD DISPLAY — aktualna szerokość i rozłożenie L/R.
- CORRELATION METER — -1…+1 (+1 zgodny, 0 brak korelacji, -1 przeciwfaza). Wartości ujemne = ostrzeżenie, ale bez automatycznej zmiany brzmienia.
- VECTOR SCOPE — relacja między kanałami.
- MID/SIDE METERS — osobne mierniki Mid i Side.
- MONO COMPATIBILITY — natychmiastowe przełączenie odsłuchu stereo / mono; nie zmienia parametrów presetu.

## 10. DEFAULT SETTINGS
| Grupa | PAN | WIDTH | Mono |
|---|---|---|---|
| PUNCH | 0% | 100% | Zalecane |
| BODY | 0% | 100% | Opcjonalne |
| TAIL | 0% | 120% | Nie |
| SUB | 0% | 0% | Tak |
| TEXTURE | 0% | 130% | Opcjonalne |
| MASTER | 0% | 100% | Nie |
Wartości są ustawieniami startowymi, a nie ograniczeniami.

## 11. INTEGRACJA Z POZOSTAŁYMI MODUŁAMI
Współpraca z: Module 04 (Layer Engine), 05 (Distortion & Waveshaper), 06 (Filter & Screech), 08 (Dynamics), 09 (Modulation Matrix), 10 (Advanced Sample Engine). Wszystkie grupy i warstwy korzystają ze wspólnego systemu routingu.

## 12. DSP I STABILNOŚĆ
Bezpieczne mono i stereo; brak niekontrolowanego wzrostu poziomu i niezamierzonych zmian fazowych; wygładzanie parametrów; ochrona przed NaN / Inf i denormals; kompensacja opóźnień tam, gdzie wymagana; brak blokowania audio callback; powtarzalny rendering offline.

## 13. TESTY AKCEPTACYJNE
1. WIDTH 0% generuje sygnał mono.
2. WIDTH 100% zachowuje oryginalną szerokość.
3. SUB MONO LOCK utrzymuje SUB w centrum.
4. Mono Audition – kontrola kompatybilności bez zmiany presetu.
5. Correlation Meter prawidłowo wskazuje korelację.
6. PAN i BALANCE działają odmiennie.
7. Polarity Invert działa na wybranych kanałach i warstwach.
8. Modulacja WIDTH nie powoduje kliknięć.
9. Stereo processing na poziomie warstwy, grupy i mastera.
10. Render offline zachowuje ustawienia przestrzenne.

---
## NOTES (Claude, 2026-11-04) – decyzje techniczne
- Moduł 07 pkt 6 (Stereo Frequency Engine: width per pasmo, M/S per pasmo, mono below) i moduł 11 to JEDNA klasa stereo – M07 dokłada do niej podział na pasma. Bez dwóch równoległych systemów szerokości.
- PAN mono: stała moc (sin/cos, -3 dB w środku); BALANCE: tylko tłumienie przeciwnego kanału (0 dB w środku). WIDTH M/S z kompensacją poziomu (bez wzrostu głośności przy 200 %).
- LOW FREQUENCY MONO: Linkwitz-Riley 24 dB/oct (ta sama klasa crossovera co M07), suma płaska; SUB MONO LOCK stosowany po wszystkich modulacjach WIDTH (priorytet).
- Dekorelacja: krótkie all-passy o różnych współczynnikach L/R, tylko powyżej crossovera low-mono; Haas z ostrzeżeniem mono w GUI.
- Correlation / vector scope / M-S meters: FIFO z audio → GUI 30–60 Hz; MONO AUDITION = przełącznik odsłuchu poza stanem presetu.
- Obecny 4GE v0.4 ma już UNISON / WIDTH (centre + L / R voices, lows mono) – brzmienie do zachowania jako punkt startowy.
