# 4GE – plan przebudowy (2026-11-03)

Pełna specyfikacja właściciela: **`SPEC.md`** (bez zmian). Ten plik: co już jest w 4GE v0.4, czego brakuje i w jakiej kolejności budujemy.
Zasady jak w RE4GE: krok po kroku, po każdym kroku kompilacja + testy + A/B dla właściciela (z JEGO kickami, nastrojonymi – LESSONS),
żadnych atrap (każda gałka zmienia DSP – test min / max), nowe ID parametrów tylko dopisywane, DSP / GUI / pliki osobno.

## Decyzje właściciela (najważniejsze)
- **STYL i DŁUGOŚĆ niezależne** (krótki Early, długi Raw…). Styl ustawia punkt startowy, nigdy nie blokuje gałek.
- Style: **EARLY / MODERN / RAW / EXPERIMENTAL** (zakresy: `Shared/RESEARCH.md` „Style kicka…” + nasze 38 pomiarów).
- Długość: **SHORT / MEDIUM / LONG / EXTRA LONG** (100–250 / 250–500 / 500–1000 / 1000–2500 ms, punkty startowe) + ms / wartości nut,
  **AUTO LENGTH** skaluje fazy z głową: punch zostaje krótki, body trochę, tail niezależnie, transient nie traci ostrości.
- Wygląd: **własna identyfikacja** – czerwono-czarny (graphite / black / dark grey + czerwony akcent), industrialny, techniczny,
  **duży centralny waveform**, **5 pionowych bloków**: KICK / SYNTHESIS / PROCESSING / MODULATION / SYSTEM (SYSTEM osobno, na dole).
- Zawsze widoczne: preset, STYLE, LENGTH, makra (IMPACT / BODY / TAIL / PITCH / GRIT / SCREAM / WEIGHT / CHAOS), master, BPM,
  Play, Random, miernik, Export + mały waveform.

## Co już jest (v0.4) → czego brakuje
| Spec | 4GE v0.4 | Brakuje |
|---|---|---|
| Punch (6) | typy Tonal / Hard / Click / Reso, level / start / end / drop / decay / attack / drive / into dist, rysowany AMP | źródła saw / square / noise / sample, PUNCH TONE / FILTER / PHASE, kilka warstw puncha |
| **Body (7)** | tylko gałka BODY (EQ) | **cały silnik BODY**: własne źródło, pitch + amp env, tone / drive / reso / filter, przejście do taila |
| Tail (8) | osc A + B, start / drop / curve / hold, rysowany PITCH / DRIVE, dist, scream / crunch / grit | TAIL CHARACTER (CLEAN … CRUSHED), FM / wavetable, osobne obwiednie filtra / reso / width |
| Pitch (9) | modes, drawn pitch, tune / fine, start w Hz | DOUBLE DROP / DROP+RISE / RISE, snap / kopiuj / presety krzywych, analiza f0 (jest w RE4GE) |
| **Length (4)** | jedna gałka LENGTH | **profile + AUTO LENGTH + długości faz** |
| **Style (3)** | brak | **silnik stylów + kontrolki stylu** |
| Osc (10), Noise (11), Transient (12) | osc A / B, CHAOS / TEXTURE, CLICK | 4 osc z unison / FM / ring / sync, typy szumu, transient shaper w pasmach |
| Sub (13) | CLEAN SUB (crossover), SUB LEVEL | typy, oct / harm / sat (jest w RE4GE `LowEnd.h`), ducking |
| Distortion (14) | 2 stopnie, 5–6 typów, bias / mix / auto gain, BUS | **4 stopnie**, Distortion Lab z RE4GE (ADAA, waveshaper editor), routing serial / parallel / split, osobno P / B / T |
| Filter / Resonance (15–16) | pre filter + sweep, SCREAM | moduł RESONANCE (CLEAN / METALLIC / SCREAM / RUBBER), ladder / comb / formant |
| Sample (17), Layers (18) | SAMPLE layer | edytor + tryby (RE4GE KickView / GrainReader), 8 warstw z solo |
| Envelopes / LFO / Matrix (19–20) | rysowane AMP / DRIVE / PITCH | z RE4GE: LFO 1–4 + MATRIX, dodatkowe obwiednie |
| Sequencer (21) | – | z RE4GE (opcjonalny) |
| EQ / Dynamics / Stereo (22–24) | OTT, clipper, unison / width | z RE4GE: EQ 8 + kompresor; limiter, mono check |
| Macros (25) | – | 8 makr (RE4GE `Macros.h`) z min / max / curve |
| Randomizer (26) | prosty Random | z RE4GE: tryby + STYLE + AMOUNT + LOCK + seed |
| Presets (27) | lista + user | kategorie z RE4GE (+ długość w nazwie), tagi / ulubione później |
| Export (28) | – | z RE4GE + opcje SR / bit / normalize / stems |
| Analysis / A/B / Morph (29, 34) | – | z RE4GE: Reference, A/B + MORPH, f0 |

## Kolejność (zgodnie z etapami SPEC 37, z tym co już mamy)
0. **Makieta ekranu** + szkielet nowego projektu (komponenty `Source/DSP` / `Parameters` / `Presets` / `GUI`, nowe ID od zera); v0.4 zostaje
   w historii gita jako wzór. (Wcześniej planowany refaktor bit w bit – niepotrzebny po decyzji „od nowa”.) Makieta (industrial, 5 bloków, waveform) do zatwierdzenia.
1. **Core** – szczegółowa specyfikacja właściciela: **`MODULE01.md`** (PunchGenerator / BodyGenerator / TailGenerator / SubGenerator →
   CoreMixer: level / pan / mute / solo / phase invert / peak; LENGTH 100–2500 ms (domyślnie 450) + AUTO LENGTH, PHASE RESET, MASTER;
   PITCH ENGINE z trybami CLASSIC / FAST / SLOW / DOUBLE DROP, RISE, STABLE, CUSTOM, przygotowany na osobne krzywe P / B / T;
   TAIL CHARACTER CLEAN / TONAL / DIRTY / SCREAM / METALLIC / BUZZ / CRUSHED (realnie różne tory); głosy nakładające się, note-off nie ucina;
   12 kryteriów akceptacji = testy RenderTest). **Decyzja właściciela (2026-11-03): silnik budujemy OD NOWA w oparciu o doświadczenia** (Forge / 4GE v0.4 / RE4GE / LESSONS) –
   bez obowiązku zgodności z presetami OWNER / FULL; stary kod to tylko źródło wiedzy (co brzmiało dobrze: smaczki w pamięci, LESSONS).
   Poprzedni opis kroku: silnik BODY, LENGTH ENGINE (profile + AUTO LENGTH + fazy), nowy ekran KICK (Main / Style / Length / Punch / Body / Tail / Pitch),
   waveform z klikalnymi strefami, eksport WAV. **Bramka: ucho właściciela.**
1b. **Moduł 02 – Advanced Pitch & Envelope** (specyfikacja: **`MODULE02.md`**): wielopunktowe krzywe do 64 punktów (TIME / VALUE / CURVE /
   TENSION; LINEAR / EXP / LOG / EASE IN / OUT / IN-OUT / BEZIER / STEP – wszystkie w DSP), osobny pitch dla PUNCH / BODY / TAIL / SUB
   (ENABLE / AMOUNT / TIME SCALE / OFFSET / COPY / PASTE, SUB FOLLOW BODY), PITCH LINK (INDEPENDENT / FOLLOW / COPY / LINKED, bez cykli),
   uniwersalny silnik obwiedni (AMP / PITCH / CUTOFF / DRIVE / RESO / NOISE / FM / WIDTH / FOLD; ADSR + DELAY / CURVE / POLARITY,
   ONE SHOT / RETRIGGER / LEGATO), TIME SCALE 10–400 % + OFFSET + sync do BPM, przypisania źródło → cel (suma z clampem), edytor
   (zoom, snap, wielokrotne zaznaczenie, undo, presety kształtów). Pitch liczony w półtonach (f = f0 · 2^(st/12)), base osobno od krzywej.
   Klocki do wzięcia: `Shared` `mod::Shape` + RE4GE `ShapeDesigner` (rozbudować o typy interpolacji / tension / Bezier / 64 punkty).
1c. **Moduł 03 – Oscillator & Waveform Engine** (specyfikacja: **`MODULE03.md`**): min. 4 oscylatory (architektura do 8), każdy z własną fazą;
   SINE / TRI / SAW / SQUARE / PULSE (1–99 %) / NOISE (white / pink / brown / metallic / digital) / CUSTOM (rysowany / import cyklu z WAV);
   anti-aliasing u źródła (PolyBLEP / band-limited, oversampling dla FM / ring / sync); WAVETABLE (position / morph / scan: static / linear /
   env / LFO / random z seedem, mipmapy); HARMONIC ENGINE 32–64 harmonicznych (amp / phase / on, odd / even / roll-off / tilt / spread / random);
   UNISON 1–16 (detune / spread / blend, tryby, gain pod kontrolą); PHASE (reset / random / free / synced, align, invert); FM MATRIX
   (ratio 0.1–32, fine, feedback, env); RING MOD (osobny algorytm); HARD / SOFT SYNC; routing osc → PUNCH / BODY / TAIL / SUB / CUSTOM BUS
   (single / multi / split, poziom na połączenie); 8 makr osc; widok OSCILLATORS + spectrum z prawdziwego sygnału; presety osc.
   Klocki: `Shared` polyBLEP / `Oscillator`, RE4GE `OscBank` (PM, unison), 5CR34M 4LPH4 (FM / wavetable – sprawdzić).
2. **Style engine**: EARLY / MODERN / RAW / EXPERIMENTAL + kontrolki stylu + presety startowe (każdy styl × długości).
3. **Synthesis**: 4 osc, noise, transient, sub z RE4GE, resonance, sample + 8 warstw z solo.
4. **Processing**: 4 stopnie + Distortion Lab + waveshaper, filtry, EQ 8, dynamika, stereo.
5. **Modulation**: obwiednie, LFO + matrix, makra z mapowaniem, Randomizer (tryby / style / LOCK / seed), sekwencer.
6. **Pro tools**: browser + kategorie, A/B + morph, reference, batch / stems export, history, snapshot.
