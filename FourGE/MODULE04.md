<!-- Owner specification of module 04 (2026-11-03), verbatim. Plan: PLAN.md. -->
> **Zasada nadrzędna:** 4GE ma dokładnie 8 MASTER MACROS (IMPACT, BODY, TAIL, PITCH, GRIT, SCREAM, WEIGHT, CHAOS). Makra / kontrolery opisane w tym module są LOKALNE i sterowane przez Macro Mapping (M09) – patrz `RULES.md`.
# 4GE — HARDSTYLE KICK DESIGNER
## MODULE 04: NOISE, TRANSIENT & LAYER ENGINE

**Manufacturer:** Sonicus / Sonicsystems  
**Plugin:** 4GE  
**Module:** Noise, Transient & Layer Engine  
**Priority:** HIGH  
**Dependencies:** Module 01 — Core Kick Engine; Module 02 — Advanced Pitch & Envelope Engine; Module 03 — Oscillator & Waveform Engine  
**Implementation:** C++ / JUCE

---

# 1. CEL MODUŁU

Zaimplementować trzy niezależne systemy odpowiedzialne za dodatkową konstrukcję dźwięku:

1. NOISE ENGINE — generowanie i kształtowanie szumu.
2. TRANSIENT ENGINE — projektowanie początkowego ataku i kliknięcia kicka.
3. LAYER ENGINE — łączenie wielu niezależnych źródeł dźwięku w jedną konstrukcję.

Moduł musi umożliwiać tworzenie:
- mocnych, krótkich punchy,
- ostrych kliknięć,
- metalicznych ataków,
- szorstkich i ziarnistych tekstur,
- szumowych ogonów,
- wielowarstwowych kicków,
- złożonych struktur dźwiękowych z różnych generatorów.

Wszystkie trzy systemy muszą być zintegrowane z Core Kick Engine.

Nie mogą zastępować podstawowych generatorów PUNCH, BODY, TAIL i SUB. Mają je rozszerzać.

# 2. NOISE ENGINE

## 2.1 Cel

Zaimplementować niezależny generator szumu, który może dostarczać sygnał do PUNCH, BODY, TAIL, SUB lub dowolnej dodatkowej warstwy.

Generator musi pracować w czasie rzeczywistym i umożliwiać kontrolowanie charakteru szumu.

## 2.2 Noise Sources

Obsługiwane źródła:

### WHITE NOISE
Równomierny rozkład energii w paśmie częstotliwości.

Zastosowanie:
- kliknięcia,
- transienty,
- ostre tekstury,
- agresywne ataki.

### PINK NOISE
Energia maleje wraz ze wzrostem częstotliwości.

Zastosowanie:
- bardziej miękkie tekstury,
- naturalniejsze szumy,
- wypełnianie BODY.

### BROWN NOISE
Silniejsza koncentracja energii w niskich częstotliwościach.

Zastosowanie:
- ciemne tekstury,
- dodatkowa masa BODY,
- eksperymentalne warstwy.

### BLUE NOISE
Większa koncentracja energii w wysokich częstotliwościach.

Zastosowanie:
- ostre kliknięcia,
- jasne transienty,
- wysokie tekstury.

### VIOLET NOISE
Silnie podkreślone wysokie częstotliwości.

Zastosowanie:
- bardzo ostre tekstury,
- cyfrowe kliknięcia,
- agresywne wysokie pasmo.

### METALLIC NOISE
Szum przechodzący przez zestaw rezonatorów lub filtrów rezonansowych.

Parametry:
- Resonator Frequency
- Resonator Q
- Resonator Amount
- Metallic Character

### DIGITAL NOISE
Kontrolowany generator szumu o obniżonej rozdzielczości lub ziarnistej strukturze.

Parametry:
- Bit Depth
- Sample Rate Reduction
- Quantization
- Grain Amount

### CUSTOM NOISE
Możliwość wykorzystania niestandardowego źródła szumu lub własnego profilu jego kształtowania.

Każde źródło musi mieć rzeczywiście odmienną charakterystykę sygnału.

Nie implementuj różnych typów szumu jako jednego generatora z przypadkowymi zmianami głośności.

# 3. NOISE PARAMETERS

Każdy generator szumu musi posiadać:

LEVEL:
- zakres: -inf do +12 dB.

COLOR:
- zakres: 0–100%.
- kontroluje tonalną charakterystykę szumu.

FILTER:
- LP / HP / BP / NOTCH.

CUTOFF:
- zakres: 20 Hz–20 kHz.

RESONANCE:
- zakres: 0–100%.

DRIVE:
- zakres: 0–100%.

ATTACK:
- zakres: 0–2500 ms.

DECAY:
- zakres: 1–2500 ms.

LENGTH:
- zakres: 1–2500 ms.

MIX:
- zakres: 0–100%.

STEREO WIDTH:
- zakres: 0–100%.

PITCH TRACKING:
- ON/OFF.

ENVELOPE:
- możliwość przypisania obwiedni z Modułu 02.

Każdy parametr musi rzeczywiście wpływać na sygnał.

# 4. NOISE MODULATION

Generator musi obsługiwać modulację:

- Noise Level
- Filter Cutoff
- Resonance
- Drive
- Color
- Stereo Width
- Grain Amount

Źródła modulacji:
- Pitch Envelope,
- Amplitude Envelope,
- LFO,
- Macro,
- inne dostępne źródła modulacji.

Przykładowe zastosowanie:

Noise Level może być najwyższy w pierwszych 20 ms kicka, a następnie szybko zanikać.

Filter Cutoff może otwierać się podczas ataku, a później zamykać.

Drive może narastać w trakcie TAIL, tworząc coraz bardziej agresywną teksturę.

Wszystkie połączenia muszą korzystać z systemu modulacji z Modułu 02.

# 5. NOISE RANDOMIZATION

Zaimplementuj kontrolowany generator losowości.

NOISE SEED:
- deterministyczny identyfikator sekwencji losowej.

RANDOM MODE:
- FIXED
- PER TRIGGER
- FREE RUN

FIXED:
Ten sam sygnał szumu przy każdym wyzwoleniu kicka.

PER TRIGGER:
Nowy przebieg szumu przy każdym Note On.

FREE RUN:
Generator pracuje ciągle.

RANDOM AMOUNT:
- zakres: 0–100%.

RANDOM AMOUNT nie może powodować przypadkowego zmieniania wszystkich parametrów generatora. Kontroluje wyłącznie wskazane elementy losowości.

Wymagane jest zachowanie deterministyczności podczas offline exportu.

# 6. TRANSIENT ENGINE

## 6.1 Cel

Transient Engine odpowiada za kontrolę początkowego uderzenia kicka.

Ma umożliwiać budowanie wyrazistego ataku niezależnie od podstawowego PUNCH.

Transient Engine nie jest zwykłym kompresorem. Musi umożliwiać zarówno generowanie dodatkowych sygnałów ataku, jak i kształtowanie istniejącego transientu.

## 6.2 Transient Sources

Obsługiwane źródła:

- SINE CLICK
- NOISE CLICK
- IMPULSE
- SHORT PULSE
- METALLIC CLICK
- OSCILLATOR SNAP
- CUSTOM TRANSIENT

Każde źródło powinno mieć własną obwiednię i poziom.

IMPULSE musi być ograniczony czasowo i zabezpieczony przed niekontrolowanym poziomem szczytowym.

## 6.3 Transient Parameters

ATTACK LEVEL:
- zakres: -24 do +24 dB.

ATTACK TIME:
- zakres: 0–50 ms.

DECAY TIME:
- zakres: 1–200 ms.

CLICK LEVEL:
- zakres: -inf do +12 dB.

CLICK LENGTH:
- zakres: 0.1–100 ms.

CLICK PITCH:
- zakres: 20–20000 Hz.

CLICK TONE:
- zakres: 0–100%.

CLICK DRIVE:
- zakres: 0–100%.

CLICK MIX:
- zakres: 0–100%.

LOW TRANSIENT:
- zakres: -24 do +24 dB.

HIGH TRANSIENT:
- zakres: -24 do +24 dB.

TRANSIENT WIDTH:
- zakres: 0–100%.

## 6.4 Transient Shaper

Zaimplementuj procesor analizujący obwiednię amplitudy sygnału wejściowego i umożliwiający zmianę proporcji ataku oraz sustainu.

PARAMETERS:

ATTACK:
- zakres: -100 do +100%.

SUSTAIN:
- zakres: -100 do +100%.

SENSITIVITY:
- zakres: 0–100%.

TIME:
- zakres: 1–500 ms.

MIX:
- zakres: 0–100%.

ATTACK:
Zwiększa lub zmniejsza poziom szybkich zmian amplitudy.

SUSTAIN:
Zwiększa lub zmniejsza poziom wolniej zmieniającej się części sygnału.

SENSITIVITY:
Kontroluje reakcję detektora na zmiany amplitudy.

TIME:
Kontroluje czas analizy i wygładzania detektora.

Procesor musi działać na podstawie rzeczywistego detektora obwiedni, a nie prostego statycznego podbicia wysokich częstotliwości.

## 6.5 Transient Position

Umożliw użytkownikowi przesunięcie dodatkowego transientu względem początku kicka.

POSITION:
- zakres: 0–250 ms.

Możliwość synchronizacji do początku:
- Kick Start
- Punch Start
- Body Start
- Tail Start

Dzięki temu użytkownik może stworzyć dodatkowy impuls w momencie wejścia TAIL.

# 7. LAYER ENGINE

## 7.1 Cel

Layer Engine umożliwia tworzenie złożonych kicków poprzez łączenie wielu niezależnych źródeł dźwięku.

Minimalna liczba warstw: 8.

Docelowa liczba warstw: 16.

Każda warstwa musi posiadać własny tor sygnałowy.

Warstwa nie może być tylko suwakiem głośności nałożonym na cały kick.

## 7.2 Layer Sources

Każda warstwa może korzystać z jednego głównego źródła:

- OSCILLATOR
- NOISE
- TRANSIENT
- INTERNAL GENERATOR
- EXTERNAL SAMPLE INPUT — interfejs przygotowany do integracji z Sample Engine z późniejszego modułu.

Na tym etapie nie implementuj pełnego Sample Engine. Przygotuj interfejs źródła, aby późniejsze rozszerzenie nie wymagało przebudowy Layer Engine.

## 7.3 Layer Groups

Każda warstwa może należeć do jednej grupy:

- PUNCH
- BODY
- TAIL
- SUB
- TEXTURE
- CUSTOM

Grupa określa miejsce warstwy w strukturze kicka i umożliwia wspólne sterowanie poziomem oraz routingiem.

Użytkownik może zmienić grupę bez zmiany źródła dźwięku.

## 7.4 Layer Parameters

Każda warstwa posiada:

ENABLE:
- ON/OFF.

LEVEL:
- zakres: -inf do +12 dB.

PAN:
- zakres: -100 do +100.

PITCH:
- zakres: -48 do +48 semitonów.

FINE:
- zakres: -100 do +100 centów.

LENGTH:
- zakres: 1–2500 ms.

START OFFSET:
- zakres: 0–2500 ms.

PHASE:
- zakres: 0–360°.

PHASE INVERT:
- ON/OFF.

STEREO WIDTH:
- zakres: 0–100%.

FILTER ENABLE:
- ON/OFF.

FILTER CUTOFF:
- zakres: 20 Hz–20 kHz.

FILTER TYPE:
- LP / HP / BP / NOTCH.

DRIVE:
- zakres: 0–100%.

ENVELOPE:
- przypisanie własnej obwiedni.

MUTE:
- ON/OFF.

SOLO:
- ON/OFF.

Każda warstwa musi posiadać indywidualną kontrolę parametrów.

# 8. LAYER ENVELOPE

Każda warstwa musi posiadać niezależną obwiednię amplitudy.

Tryby:
- ADSR
- ONE SHOT
- MULTIPOINT

Parametry:
- Attack
- Decay
- Sustain
- Release
- Delay
- Amount
- Time Scale

Możliwość kopiowania obwiedni między warstwami.

Zmiana obwiedni jednej warstwy nie może automatycznie modyfikować pozostałych.

# 9. LAYER GROUP CONTROL

Zaimplementuj kontrolę grupową.

Każda grupa musi posiadać:

GROUP LEVEL:
- zakres: -inf do +12 dB.

GROUP PAN:
- zakres: -100 do +100.

GROUP WIDTH:
- zakres: 0–100%.

GROUP MUTE:
- ON/OFF.

GROUP SOLO:
- ON/OFF.

GROUP ENABLE:
- ON/OFF.

GROUP OUTPUT:
- wybór magistrali wyjściowej.

Zmiana poziomu grupy musi wpływać na wszystkie przypisane do niej warstwy.

Poziom grupy nie może niszczyć indywidualnych ustawień poziomu warstw.

# 10. LAYER ROUTING

Każda warstwa musi umożliwiać routing do:

- PUNCH BUS
- BODY BUS
- TAIL BUS
- SUB BUS
- TEXTURE BUS
- CUSTOM BUS
- MASTER BUS

Tryby routingu:

SINGLE:
Jedna warstwa trafia do jednej magistrali.

MULTI:
Warstwa może zostać wysłana do kilku magistrali z osobnymi poziomami send.

PRE-FADER SEND:
Wysyłka sygnału niezależna od poziomu kanału.

POST-FADER SEND:
Wysyłka sygnału po regulacji poziomu kanału.

Nie dopuszczaj do przypadkowego wielokrotnego sumowania sygnału.

# 11. LAYER LINK

Utwórz system synchronizacji parametrów pomiędzy warstwami.

Dostępne tryby:

INDEPENDENT:
Brak połączenia.

LINK LEVEL:
Wspólna regulacja poziomu.

LINK PITCH:
Wspólna regulacja pitchu.

LINK LENGTH:
Wspólna regulacja długości.

LINK ENVELOPE:
Wspólna obwiednia.

LINK ALL:
Wspólne sterowanie wszystkimi wybranymi parametrami.

LINK musi posiadać możliwość wyboru parametrów osobno.

Zmiana jednego parametru nie może nadpisywać pozostałych ustawień warstwy.

Nie pozwalaj na cykliczne zależności pomiędzy warstwami.

# 12. LAYER RANDOMIZER

Przygotuj osobny system losowania konfiguracji warstw.

Tryby:
- RANDOM LAYER LEVEL
- RANDOM PITCH
- RANDOM LENGTH
- RANDOM SOURCE
- RANDOM FILTER
- RANDOM PHASE
- RANDOM ENVELOPE
- RANDOM ALL

INTENSITY:
- zakres: 0–100%.

LOCK:
Każda warstwa posiada blokadę.

LOCKED warstwa nie może zostać zmieniona przez Randomizer.

SEED:
Deterministyczna wartość losowania.

PRESERVE GROUPS:
ON/OFF.

PRESERVE ROUTING:
ON/OFF.

UNDO:
Możliwość cofnięcia losowania.

Randomizer nie może tworzyć nieprawidłowych wartości ani konfiguracji, które powodują niekontrolowany wzrost poziomu.

# 13. LAYER SOLO SYSTEM

Zaimplementuj profesjonalny system odsłuchu.

Dostępne tryby:

SOLO LAYER:
Odsłuch pojedynczej warstwy.

SOLO GROUP:
Odsłuch całej grupy.

SOLO SOURCE:
Odsłuch źródła przed dodatkowymi efektami warstwy.

SOLO PROCESSED:
Odsłuch sygnału po przetwarzaniu warstwy.

MULTI SOLO:
Możliwość jednoczesnego odsłuchiwania kilku wybranych warstw.

Solo nie może zmieniać parametrów zapisanych w projekcie.

Po wyłączeniu Solo pełny miks musi powrócić bez zmiany ustawień.

# 14. PHASE ALIGNMENT I LAYER CORRELATION

Zaimplementuj podstawową kontrolę relacji fazowej pomiędzy warstwami.

PHASE ALIGN:
- ON/OFF.

PHASE SEARCH:
- zakres: 0–360°.

REFERENCE LAYER:
- wybór warstwy odniesienia.

ALIGN TARGET:
- wybór warstwy dopasowywanej.

CORRELATION DISPLAY:
- wizualizacja korelacji sygnałów.

Ważne:
Automatyczne dopasowanie fazy musi być opcjonalne.

Nie wolno automatycznie zmieniać fazy wszystkich warstw bez wiedzy użytkownika.

Dopasowanie powinno analizować rzeczywiste sygnały w określonym przedziale czasu.

Nie zakładaj, że maksymalna korelacja zawsze oznacza najlepsze brzmienie.

# 15. LAYER VISUALIZER

Przygotuj graficzny widok wszystkich warstw.

Każdy wiersz przedstawia jedną warstwę.

Wyświetlane informacje:
- nazwa,
- grupa,
- źródło,
- poziom,
- długość,
- status MUTE,
- status SOLO,
- status LOCK.

Dodatkowo:
- mini waveform,
- możliwość zmiany kolejności warstw,
- przeciąganie warstw do grup,
- kopiowanie,
- duplikowanie,
- usuwanie,
- zmiana nazwy.

Zmiana kolejności warstw w GUI nie może przypadkowo zmieniać ich routingu ani ustawień.

# 16. NOISE / TRANSIENT / LAYER PRESETS

Utwórz osobne kategorie presetów:

NOISE:
- WHITE IMPACT
- DARK NOISE
- METALLIC NOISE
- DIGITAL CRUSH
- HIGH TEXTURE
- LOW TEXTURE

TRANSIENT:
- CLASSIC CLICK
- HARD IMPACT
- METALLIC SNAP
- SHORT PUNCH
- DEEP IMPACT
- DIGITAL ATTACK

LAYERS:
- CLASSIC PUNCH STACK
- MODERN BODY STACK
- RAW TAIL TEXTURE
- METALLIC LAYER
- SUB FOUNDATION
- EXPERIMENTAL STACK

Użytkownik musi móc zapisywać pojedyncze źródła, konfiguracje warstw oraz kompletne zestawy.

# 17. GUI

Przygotuj trzy główne widoki:

## NOISE

- wybór źródła,
- Level,
- Color,
- Filter,
- Resonance,
- Drive,
- Length,
- Envelope,
- Stereo Width,
- Noise Waveform Display.

## TRANSIENT

- Attack Level,
- Attack Time,
- Sustain,
- Click Level,
- Click Length,
- Click Pitch,
- Click Tone,
- Click Drive,
- Transient Shaper,
- Transient Waveform Display.

## LAYERS

- lista warstw,
- grupy,
- źródła,
- Level,
- Pitch,
- Length,
- Pan,
- Phase,
- Filter,
- Drive,
- Mute,
- Solo,
- Lock,
- Routing.

Interfejs musi być czytelny również przy 16 warstwach.

Zastosuj przewijanie listy warstw zamiast zmniejszania wszystkich kontrolek do nieczytelnego rozmiaru.

# 18. DSP REQUIREMENTS

- Wszystkie generatory szumu muszą pracować w czasie rzeczywistym.
- Szum musi być deterministyczny przy ustawionym seedzie.
- Transient Shaper musi wykorzystywać rzeczywisty detektor obwiedni.
- Warstwy muszą posiadać niezależne stany DSP.
- Wszystkie obwiednie muszą być synchronizowane z systemem z Modułu 02.
- Routing musi być bezpieczny i jednoznaczny.
- Parametry muszą być wygładzane tam, gdzie jest to konieczne.
- Brak dynamicznej alokacji pamięci w callbacku audio.
- Brak blokowania wątku audio.
- Zabezpieczenie przed DC offset.
- Zabezpieczenie przed denormal numbers.
- Zabezpieczenie przed NaN i nieskończonymi wartościami.
- Kontrola gainu na poziomie kanałów, grup i mastera.
- Poprawna obsługa mono i stereo.
- Identyczny rezultat przy realtime playback i offline export przy tych samych ustawieniach.

W przypadku dużej liczby warstw silnik musi poprawnie zarządzać zużyciem CPU.

Wyłączone warstwy nie powinny niepotrzebnie wykonywać kosztownych operacji DSP.

# 19. TESTY I KRYTERIA AKCEPTACJI

Moduł można uznać za ukończony, jeśli:

1. Działają wszystkie typy Noise Sources.
2. Noise Engine generuje rzeczywiście różne rodzaje szumu.
3. Można modulować parametry szumu.
4. Seed zapewnia deterministyczny rezultat.
5. Transient Engine generuje niezależne sygnały ataku.
6. Transient Shaper reaguje na rzeczywistą obwiednię sygnału.
7. Layer Engine obsługuje minimum 8 niezależnych warstw.
8. Każda warstwa ma własne parametry i obwiednię.
9. Warstwy można przypisywać do grup.
10. Routing działa zgodnie z konfiguracją.
11. Solo i Mute działają poprawnie.
12. Randomizer respektuje LOCK.
13. Phase Alignment nie zmienia ustawień bez zgody użytkownika.
14. Waveform Display pokazuje rzeczywiste sygnały.
15. Presety zapisują pełną konfigurację.
16. Nie występują trzaski przy włączaniu i wyłączaniu warstw.
17. Plugin pozostaje stabilny przy maksymalnej liczbie warstw.
18. Realtime i offline export dają zgodny rezultat.

# 20. ZAKAZ ATRAP

- Nie twórz Noise Engine opartego na jednym generatorze dla wszystkich typów szumu bez odpowiedniego kształtowania widma.
- Nie implementuj Transient Shaper jako zwykłego EQ.
- Nie twórz Layer Engine, który tylko wizualnie pokazuje warstwy, ale sumuje jeden wspólny sygnał.
- Nie pozwalaj na niekontrolowane podwajanie sygnału przez routing.
- Nie zmieniaj fazy automatycznie bez kontroli użytkownika.
- Nie twórz losowości bez możliwości odtworzenia wyniku.
- Nie pokazuj niezaimplementowanych funkcji jako działających.

Każda kontrolka musi być podłączona do rzeczywistego DSP.

# 21. REZULTAT KOŃCOWY

Po ukończeniu modułu użytkownik powinien móc:

- generować różne rodzaje szumu,
- budować własne kliknięcia i transienty,
- wzmacniać lub osłabiać atak kicka,
- dodawać tekstury do BODY i TAIL,
- tworzyć kicki z minimum 8 niezależnych warstw,
- grupować i routować warstwy,
- kontrolować fazę,
- odsłuchiwać każdą warstwę oddzielnie,
- zapisywać i przywracać konfiguracje warstw.

Moduł ma zapewnić pełną kontrolę nad detalami i strukturą dźwięku kicka hardstyle.