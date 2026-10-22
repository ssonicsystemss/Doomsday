# RE4GE v0.1

**Reverse bass** jako osobny instrument VST3 (FL Studio). Wydzielony z 4GE i zbudowany według analizy architektury reverse bassu
(`Shared/RESEARCH.md`, „Reverse bass”). Przester, filtry i ogon kicka są te same co w 4GE (wspólny silnik `Shared/Source/GeKick.h`).

## Instalacja (Windows)
GitHub → **Actions** → *Build RE4GE VST3* → **Artifacts** → `RE4GE-VST3-Windows`.
Folder `RE4GE.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, a w FL: *Options → Manage plugins → Find installed plugins*.
Wstaw jako **instrument** na osobny kanał obok kicka (4GE albo dowolnego).

## Jak tego używać (2 sposoby)
- **PATTERN** (domyślnie): w piano rollu RE4GE **piszesz te same nuty co kick**, czyli nuta na każdym kicku. Bas sam gra kroki
  (1/4, 1/8 albo 1/16) między kickami, na wysokości nuty. **STEPS** wybiera, które kroki grają (np. tylko offbeaty), **OFFSET** je przesuwa,
  a **BEATS** mówi, ile beatów basu gra jedna nuta.
- **GATE**: każda nuta to jedna nuta basu. Długość nuty w piano rollu = długość basu, a po puszczeniu jest **FADE**. Tak piszesz zwykłą linię basową.

**TIMING**:
- **SYNC**: DELAY / HOLD / DECAY / GATE w % kroku, więc bas sam pasuje do tempa;
- **MS**: te same czasy w milisekundach.

**SWELL** jest zawsze w ms.

## Ekran
Na górze presety, pod spodem **jeden beat** w tempie FL z podziałką w ms. Pomarańczowe = kick (Twoja nuta), fioletowe = bas,
przerywana linia = pitch. **MASTER**:
- **LEVEL**;
- **CLIP**: miękkie wyrównanie szczytów;
- **SUB Hz** + **SUB LEVEL**: poniżej SUB Hz bas jest czystym sinusem na nucie. Nigdy nie jest ucinany ani przesterowany, to jest to „pierdolnięcie”;
- **VELOCITY**.

### SOURCE
- **SOURCE**:
  - **SYNTH**: piła ↔ pulse (SAW/PULSE), WIDTH, SINE na fundament;
  - **TAIL**: klasyka, czyli ogon kicka puszczony **od tyłu**. Jego zjazd pitchu staje się podjazdem przed kickiem;
  - **SAMPLE**: Twój WAV, np. kick wyrenderowany z 4GE. Przeciągnij go na wtyczkę, a SAMPLE włączy się sam.
- **PHASE WARP** (tylko SYNTH): zagina fazę oscylatora, co daje inne harmoniczne niż drive.
  - **BEND**: sinus → piła, „sprężysty” bas;
  - **SQUEEZE**: nosowy, prawie kwadrat;
  - **SYNC**: gadający, jak hard sync;
  - **MIRROR**: metaliczny, parzyste harmoniczne.

  **AMOUNT** to siła, **ENV** sprawia, że warp otwiera się razem z narastaniem basu.
- **KICK TAIL** (dla TAIL): START, DROP, CURVE, WAVE, typ przesteru, DRIVE, TONE, HIGH CUT, SCREAM, SCREAM Hz. W trybie GATE dochodzi **LENGTH**: ile ogona grać od tyłu.
- **SAMPLE**: LOAD / x, **REVERSE** (od tyłu, kończy się na końcu gate'u), **ROOT** (nuta WAV-a), START.

### SHAPE
- **TRIGGER**: PATTERN / GATE, SYNC / MS, krok 1/4 / 1/8 / 1/16, OFFSET, BEATS.
- **STEPS**: 1–4, które kroki beatu grają (1/8: 1 + 2, 1/16: 1–4).
- **PITCH**:
  - **STABLE**: na nucie;
  - **DROP**: z góry na nutę na początku;
  - **RISE**: podjazd na końcu, klasyczny reverse;
  - **BOUNCE**: oba.

  Gałki: AMOUNT (półtony), TIME, TUNE, FINE, **KEY TRACK** (wyłączony = zawsze na NOTE, nuty dają tylko rytm).
- **ENVELOPE**: jedna obwiednia na wszystkie style:
  - **DUCK**: poziom, zanim bas wejdzie (0 = cisza pod kickiem);
  - **INITIAL**: od jakiego poziomu bas rusza;
  - **SWELL**: czas narastania (Twoje wzorce FHSP: 50–80 ms);
  - **CURVE**: + = szybko, potem ląduje (jak sidechain); − = wolno, później;
  - **SUSTAIN** + **DEC CURVE**: dokąd i jak opada;
  - **FADE**: cięcie przed kickiem.

  Do tego **TIMES**: DELAY, HOLD, DECAY, GATE.
  Szablony jednym klikiem (zmieniają tylko obwiednię, barwa zostaje):
  - **PUMP**: jak FHSP, szybki powrót, półka, cięcie;
  - **SWELL**: oddychający, szczyt wcześnie, długi ogon;
  - **TIGHT**: krótki, zwarty;
  - **ROLL**: rozlewający się;
  - **PULSE**: 1/16, agresywny.

### TONE
- **PRE FILTER** (przed drive'em): OFF / LP / HP / BP / NOTCH, CUTOFF, RESO.
- **DRIVE**: typ (SOFT, TUBE, HARD, FOLD, RAW, CRUSH), DRIVE, **ENV** (czysty na starcie, najbrudniejszy w szczycie), **ASYM**, **MIX**.
- **POST EQ**: **LOW CUT** (domyślnie OFF, tnie tylko przesterowaną część, nigdy SUB), HIGH CUT, BELL (Hz, gain, Q).

**Zasada:** domyślnie dół zostaje nietknięty. Zabiera go tylko świadomie ustawiony LOW CUT.

## Presety
- **INIT - Pump Tail**.
- Twoje ogony z Forge z basem jak w presetach BASS:
  - **PUMP - QL Tail**;
  - **PUMP - Volcanic Tail [F]**;
  - **PUMP - Undertow Tail [G]**.
- Na każdy szablon:
  - **SWELL - QL Tail**;
  - **SWELL - Synth Bend**;
  - **TIGHT - Synth Squeeze 1/8**;
  - **ROLL - Synth Mirror**;
  - **PULSE - Synth Sync Offbeats**.
- **RISE - QL Tail Pitch Up**.
- **GATE - Bass Line (notes)**: do pisania linii basu nutami.

## Plan (v0.2)
- Rysowane linie w okienku: AMP / DRIVE-WARP / PITCH.
- Opcjonalne wejście sidechain od kicka, żeby GATE wyciszał bas, kiedy wpada kick.
- Podział klawiatury.
