<!-- Owner specification of module 08 (2026-11-03), from his PDF (original: Specs/MODULE08_owner.pdf). Plan: PLAN.md 1h. Claude's notes at the end. -->
# 4GE — MODULE 08: DYNAMICS, COMPRESSOR, TRANSIENT & LIMITER ENGINE

CEL: kompletny silnik kontroli dynamiki kicka. Każdy parametr steruje rzeczywistym algorytmem DSP – bez atrap, pustych kontrolek i trybów różniących się tylko nazwą.
Zakres: kompresor, multiband dynamics, transient shaper, clipper, limiter, crest factor analyzer, automatyczne zarządzanie poziomem. Dokument wykonawczy – zakresy, zachowania, routing i kryteria akceptacji to wymagania. Współpraca z modułami 02, 04, 05, 06, 07.

## 0. Zasady implementacji
- Najpierw działające DSP, potem GUI. Bez kontrolek niepodłączonych do parametrów procesora.
- Każdy silnik: własny stan DSP, bypass, parametry, routing, zapis / odczyt presetu.
- Wszystkie parametry widoczne dla użytkownika automatyzowalne przez DAW.
- Zakresy = wymagania początkowe; parametr nieobsługiwany przez algorytm → kontrolka nieaktywna.
- Zakaz normalizacji próbka po próbce. Auto Gain = pomiar w oknie czasowym + wygładzona korekcja.
- Nie dublować silników Module 05 i Module 07 – wspólne komponenty i punkty routingu.

## 1. Architektura modułu
| Silnik | Odpowiedzialność |
|---|---|
| COMPRESSOR ENGINE | Kompresja sygnału i sterowanie dynamiką |
| MULTIBAND DYNAMICS | Niezależne przetwarzanie dynamiki pasm |
| TRANSIENT SHAPER | Kontrola ataku, sustainu i obwiedni |
| CLIPPER ENGINE | Kontrolowane ograniczanie szczytów i kształtowanie amplitudy |
| LIMITER ENGINE | Ograniczanie maksymalnego poziomu wyjściowego |
| CREST FACTOR CONTROL | Pomiar i kontrola relacji Peak/RMS |
| AUTO GAIN & LEVEL MANAGEMENT | Kompensacja poziomu i gain staging |
Domyślny łańcuch: PUNCH/BODY/TAIL/SUB/TEXTURE → Compressor → Multiband Dynamics → Transient Shaper → Clipper → Limiter → Crest/Level Analysis → Master Output.
Zmiana kolejności, włączanie / wyłączanie sekcji, poziom warstwy / grupy / całego kicka.

## 2. Compressor Engine
### 2.1. Typy
| Typ | Zachowanie |
|---|---|
| VCA | Szybka, precyzyjna kontrola dynamiki |
| FET | Szybka, agresywna reakcja |
| Opto | Łagodniejsza, płynniejsza reakcja zależna od sygnału |
| Vari-Mu | Miękka, nieliniowa charakterystyka |
| Digital | Precyzyjna kompresja cyfrowa |
| Upward | Wzmacnianie sygnału poniżej progu |
| Parallel | Równoległe mieszanie oryginału i kompresji |
| Sidechain | Detektor sterowany z wybranego źródła |
Każdy typ ma odmienną implementację detekcji, obwiedni lub charakterystyki – nie tylko nazwę / preset.
### 2.2. Parametry
| Parametr | Zakres | Opis |
|---|---|---|
| THRESHOLD | -60 do 0 dBFS | Poziom rozpoczęcia kompresji |
| RATIO | 1:1–50:1 | Stopień kompresji |
| ATTACK | 0.01–500 ms | Czas reakcji detektora |
| RELEASE | 1–5000 ms | Powrót do spoczynku |
| KNEE | 0–24 dB | Szerokość przejścia wokół progu |
| INPUT GAIN | -24 do +24 dB | Przed kompresją |
| OUTPUT GAIN | -24 do +24 dB | Po kompresji |
| MIX | 0–100% | Dry/wet |
| LOOKAHEAD | 0–20 ms | Wcześniejsza reakcja |
| STEREO LINK | 0–100% | Wspólne sterowanie kanałów |
### 2.3. Detekcja i sidechain
Peak, RMS, Hybrid (Peak + RMS), True Peak (oversamplowana analiza międzypróbkowa). Sidechain Source: własny sygnał, PUNCH, BODY, TAIL, SUB, TEXTURE, zewnętrzne wejście hosta (jeśli jest). Sidechain Filter HPF/LPF z częstotliwością i poziomem.
Parametry: SIDECHAIN SOURCE, SIDECHAIN FILTER, SIDECHAIN HPF, SIDECHAIN LPF, SIDECHAIN GAIN, SIDECHAIN MIX, INTERNAL/EXTERNAL MODE.
### 2.4. Parallel Compression i Auto Release
Parallel: DRY LEVEL, WET LEVEL, MIX, OUTPUT GAIN; bez przypadkowego podwojenia poziomu przy sumowaniu.
Auto Release: FAST, MEDIUM, SLOW, PROGRAM DEPENDENT; ręczny Release nadal dostępny.

## 3. Multiband Dynamics Engine
Korzysta z crossoverów Module 07. Domyślne pasma: SUB 20–80 Hz (stabilizacja subbasu), LOW 80–250 Hz (masa, dolne BODY), MID 250 Hz–2,5 kHz (środek, charakter), HIGH 2,5–20 kHz (klik, screech, wysokie harmoniczne). Punkty podziału edytowalne.
Każde pasmo: niezależny Compressor, Expander, Gate, Limiter, Transient Control – każdy z własnym detektorem.
Parametry pasmowe: THRESHOLD -60…0 dBFS, RATIO 1:1–20:1, ATTACK 0.1–500 ms, RELEASE 1–2000 ms, KNEE 0–24 dB, MAKEUP -24…+24 dB, MIX 0–100%.
Band Link: SUB+LOW, MID+HIGH albo wspólny detektor dla wszystkich; bez niezamierzonych zmian balansu tonalnego.

## 4. Transient Shaper
Rzeczywista detekcja obwiedni (nie EQ).
| Parametr | Zakres | Funkcja |
|---|---|---|
| ATTACK | -100 do +100% | Wzmacnia / osłabia transient |
| SUSTAIN | -100 do +100% | Wzmacnia / osłabia energię po ataku |
| SENSITIVITY | 0–100% | Czułość detekcji |
| TIME | 1–500 ms | Okno detekcji |
| RELEASE | 1–2000 ms | Powrót detektora |
| MIX | 0–100% | Dry/wet |
| OUTPUT | -24 do +12 dB | Poziom wyjściowy |
Detekcja: FAST TRANSIENT, SLOW TRANSIENT, RMS, PEAK, HYBRID. Tryby: PUNCH MODE, SUSTAIN MODE, BODY MODE, TAIL MODE.
Niezależnie dla PUNCH, BODY, TAIL, SUB, TEXTURE. Przykład: PUNCH Attack +65%; BODY Attack +10%, Sustain +20%; SUB Attack 0%, Sustain -10%; TAIL Attack -20%, Sustain +30%.
DSP: rzeczywista obwiednia, kontrola artefaktów przy ekstremach, bez kliknięć i pików, płynne zmiany, mono i stereo.

## 5. Clipper Engine
Cel: kontrolowane ograniczanie amplitudy i szczytów (Distortion M05 = charakter harmoniczny). Może używać podobnych funkcji, ale osobny cel, sterowanie i zachowanie.
Typy: Soft Clip (łagodnie), Hard Clip (twarde obcięcie), Asymmetric Clip (różnie dla półfal), Sine Clip (zaokrąglanie sinusem), Cubic Clip (wielomian), Tanh Clip (gładka saturacja), Custom Curve (krzywa użytkownika).
| Parametr | Zakres |
|---|---|
| INPUT GAIN | -24 do +24 dB |
| CEILING | -24 do 0 dBFS |
| DRIVE | 0–100% |
| SYMMETRY | -100 do +100% |
| SOFTNESS | 0–100% |
| MIX | 0–100% |
| OUTPUT GAIN | -24 do +12 dB |
| OVERSAMPLING | OFF / 2x / 4x / 8x / 16x |
Clip Curve Editor: wybór algorytmu, podgląd rzeczywistej krzywej, punkt clippingu i symetria, własna krzywa, zapis charakterystyki; walidacja krzywych niepoprawnych / nieciągłych.
Tryby PUNCH CLIP, BODY CLIP, TAIL CLIP, FULL CLIP – z rzeczywistego routingu lub detekcji komponentów, nie tylko innego zestawu wartości.
Oversampling: interpolacja przed, filtr antyaliasingowy, decymacja po, kontrola latencji, zgodność realtime/offline.

## 6. Limiter Engine
Niezależny od clippera i kompresora.
| Parametr | Zakres |
|---|---|
| CEILING | -24 do 0 dBFS |
| INPUT GAIN | -24 do +24 dB |
| RELEASE | 1–2000 ms |
| LOOKAHEAD | 0–20 ms |
| ATTACK | 0–100 ms |
| KNEE | 0–12 dB |
| STEREO LINK | 0–100% |
| TRUE PEAK | ON/OFF |
| OVERSAMPLING | 1x / 2x / 4x / 8x |
| MIX | 0–100% |
Tryby TRANSPARENT, AGGRESSIVE, PUNCH, LOUD, TRUE PEAK – odmienne strategie detekcji / release / ograniczania.
Pokazuje aktualną, maksymalną i średnią Gain Reduction. True Peak z oversamplowanego sygnału.
Wymagania: ceiling nigdy przekroczony, True Peak i latencja poprawnie, bez pompowania, możliwie dobre zachowanie transientu.

## 7. Crest Factor Control
Crest = różnica szczyt / średnia; pomiar + kontrola przez rzeczywiste przetwarzanie dynamiki.
Parametry: TARGET CREST 0–30 dB, PUNCH CONTROL / BODY CONTROL / TAIL CONTROL -100…+100%, SENSITIVITY 0–100%, MIX 0–100%.
Analyzer: Peak Level, RMS Level, Crest Factor, Peak-to-RMS Ratio, krótkookresowy poziom, historia zmian – z pomiaru, nie szacowania z ustawień.

## 8. Auto Gain & Level Management
Tryby OFF, PEAK MATCH, RMS MATCH, LOUDNESS MATCH. Analiza w oknie czasowym + wygładzona korekcja; zakaz normalizacji próbka po próbce.
Każdy procesor: Input Level, Output Level, Peak Meter, RMS Meter, Clip Indicator. Ostrzeżenia: za wysoki poziom wejściowy, przesterowanie wewnętrzne, nadmierny wzrost gain między procesorami, przekroczenie limitu wyjściowego.
Level Match: porównanie przed / po przy wyrównanej głośności.

## 9. Makra dynamiki
| Makro | Funkcja |
|---|---|
| PUNCH | Wyrazistość ataku – nie podnosi po prostu głośności całego kicka |
| BODY | Masa i sustain |
| TIGHTNESS | Skraca i porządkuje dynamikę |
| DENSITY | Gęstość sygnału |
| LOUDNESS | Odczuwalna głośność przez kombinację procesorów, nie sam Output Gain |
| CRUSH | Agresywność clippingu |
| GLUE | Spójność dynamiki |
| CONTROL | Ogranicza nadmierne piki |
Konfigurowalna mapa parametrów, zależna od stanu silników, bez skoków poziomu.

## 10. Routing i integracja
Poziomy LAYER, GROUP, MASTER. Grupy: PUNCH, BODY, TAIL, SUB, TEXTURE, GLOBAL OUTPUT.
Rawstyle: PUNCH → Transient Shaper → Clipper; BODY → Compressor → Multiband Dynamics; SUB → Sub Control → Limiter; TAIL → Compressor → Clipper; suma → Master Limiter.
Early Hardstyle: PUNCH → Transient Shaper; BODY → Light Compressor; TAIL → Gentle Dynamics; suma → Transparent Limiter.
Integracja obowiązkowa z Module 02, 04, 05, 06, 07 – bez duplikatów. Gałęzie równoległe z kontrolą poziomu i wyrównaniem latencji; routing nie dubluje sygnału niejawnie.

## 11. GUI
Zakładki: COMPRESSOR, MULTIBAND, TRANSIENT, CLIPPER, LIMITER, CREST/LEVEL.
Compressor Transfer Curve, Gain Reduction History, Input/Output Waveform, Transient Envelope, Clipper Transfer Curve, Limiter GR Meter (aktualna + max), Crest Factor Analyzer, Peak/RMS Meters – wszystko z rzeczywistego stanu DSP.
A/B, Undo/Redo, Copy/Paste, bypass każdego silnika, solo/mute pasm, MIDI Learn, reset parametrów, tooltips. Dane do GUI bez blokowania wątku audio.

## 12. Presety
| Styl | Preset | Charakter |
|---|---|---|
| EARLY | Classic Punch | Naturalny, wyrazisty atak |
| EARLY | Soft Dynamics | Delikatna kontrola dynamiki |
| MODERN | Tight Impact | Zwarty i mocny kick |
| MODERN | Festival Loud | Gęsty, głośny charakter |
| RAWSTYLE | Extreme Crush | Mocny clipping |
| RAWSTYLE | Heavy Punch | Agresywny atak |
| EXPERIMENTAL | Broken Dynamics | Niestandardowa dynamika |
| EXPERIMENTAL | Maximum Density | Mocno skompresowany sygnał |
Presety pojedynczych procesorów i całego łańcucha; komplet parametrów + routing.

## 13. Wymagania DSP i bezpieczeństwa
13.1 Kompresja: poprawne ratio i knee, stabilna detekcja Peak/RMS/Hybrid, smoothing + lookahead, kontrola pompowania, stereo linking, bez kliknięć przy automatyzacji.
13.2 Clipper i limiter: rzeczywiste nieliniowości + kontrola aliasingu, oversampling (interpolacja / filtracja / decymacja), True Peak, lookahead, ceiling, latencja, ochrona DC / NaN / Inf.
13.3 Transient i ogólne: rzeczywista obwiednia, stabilne rozdzielanie ataku i sustainu; brak alokacji, blokad, plików i ciężkiego GUI w callbacku; denormal / DC / NaN / Inf; smoothing; deterministyczny offline = realtime; 44.1 / 48 / 88.2 / 96 / 192 kHz.

## 14. Testy i kryteria akceptacji
| Test | Oczekiwany rezultat |
|---|---|
| Compressor Ratio | Poprawna zmiana stopnia kompresji |
| Attack/Release | Słyszalna i mierzalna zmiana reakcji |
| Sidechain | Detekcja z wybranego źródła |
| Parallel Compression | Miksowanie bez przypadkowego wzrostu poziomu |
| Multiband Dynamics | Niezależna kontrola pasm |
| Transient Shaper | Oddzielna kontrola ataku i sustainu |
| Clipper | Kontrolowane ograniczanie szczytów |
| Oversampling | Ograniczenie aliasingu |
| Limiter Ceiling | Poprawna kontrola poziomu wyjściowego |
| True Peak | Kontrola szczytów między próbkami |
| Crest Factor | Poprawny pomiar Peak/RMS |
| Auto Gain | Kompensacja bez pompowania i normalizacji próbka po próbce |
| Group Processing | Niezależna dynamika grup |
| DAW Automation | Płynne sterowanie |
| Offline Render | Zgodność z realtime |
| CPU Stress Test | Stabilność przy maksymalnej konfiguracji |
Warunki zaliczenia: wszystkie typy kompresorów; Multiband rzeczywiście wielopasmowy; Transient niezależnie Attack i Sustain; Clipper min. 5 odmiennych algorytmów; Limiter z True Peak i poprawnym ceiling; Crest Analyzer na rzeczywistych danych; Auto Gain poprawnie; wszystkie grupy; testy stabilności, latencji i wydajności.

## 15. Zalecana kolejność implementacji
1 Compressor Engine · 2 Sidechain i detektory · 3 Parallel Compression · 4 Transient Shaper · 5 Clipper Engine · 6 Oversampling i aliasing · 7 Limiter Engine · 8 Multiband Dynamics · 9 Crest Factor Analyzer · 10 Auto Gain i Level Management · 11 Integracja Layer/Group/Global · 12 GUI, presety i testy.

## 16. Ostateczny rezultat
Mocny punch bez podnoszenia całego sygnału; kontrola sustainu; zagęszczenie kompresją równoległą; agresja clippingiem; ograniczenie szczytów limiterem; subbas pod kontrolą; niezależnie PUNCH/BODY/TAIL/SUB/TEXTURE; pomiar Peak, RMS, Crest; porównanie przed / po przy wyrównanej głośności.
TEST KOŃCOWY: surowy kick → więcej Punch → gęstsze Body → piki ograniczone clipperem → wyjście zabezpieczone limiterem. Mocniej i bardziej zwarcie, bez przypadkowej utraty charakteru transientu.
Zakaz: limiter nie robi automatycznie każdego kicka maksymalnie głośnym – 4GE daje pełną kontrolę nad dynamiką, nie wymusza jednego charakteru.
Następny moduł: Module 09 — Modulation Matrix, LFO & Advanced Modulation Engine.

---
## NOTES (Claude, 2026-11-03) – decyzje techniczne
- Wspólna klasa „Dynamics” (detektor peak / RMS / hybrid / true peak + obwiednia + krzywa ratio/knee) – instancjonowana per warstwa / grupa / master, tak jak EQ z M07; typy VCA / FET / Opto / Vari-Mu = różne detektory i krzywe (Opto: release zależny od historii GR, Vari-Mu: ratio rosnące z poziomem, FET: feedback detector + harmoniczne, VCA/Digital: feed-forward, log domain).
- Multiband dynamics = te same crossovery co M07 (jedna klasa), nie drugi podział.
- Clipper: krzywe z `Shared/DoomsdayLab.h` (ADAA) z CEILING / SOFTNESS / SYMMETRY; walidacja custom curve (monotoniczna, ciągła).
- Limiter: lookahead + true peak 4x (ITU-R BS.1770), doświadczenie z Throne of Titans (Pro-L 2 w RESEARCH). Limiter domyślnie NIE podbija głośności (zakaz z pkt 16).
- Auto Gain: pomiar w oknie (jak loudness matching w RE4GE Randomizer – offline render) + gładka korekcja.
- Makra M08 (PUNCH / BODY / …) – razem z M06 SCREAM i M07: pytanie na koniec, jak łączą się z 8 głównymi makrami 4GE.
- Owner PDF: `Specs/MODULE08_owner.pdf`.
