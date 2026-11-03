<!-- Owner specification of module 05 (2026-11-03), verbatim. NOTE: the file ended at 3.8 ASYMMETRIC DISTORTION (cut off) - waiting for the rest. -->
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

Parametry