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
0. **Porządek w środku** (jak RE4GE krok 0): komponenty `Source/DSP` / `Parameters` (IDs zamrożone) / `Presets` / `GUI`, brzmienie bit w bit
   jak v0.4 (test porównawczy). + **makieta ekranu** (industrial, 5 bloków, waveform) do zatwierdzenia.
1. **Core**: silnik BODY, LENGTH ENGINE (profile + AUTO LENGTH + fazy), nowy ekran KICK (Main / Style / Length / Punch / Body / Tail / Pitch),
   waveform z klikalnymi strefami, eksport WAV. **Bramka: ucho właściciela.**
2. **Style engine**: EARLY / MODERN / RAW / EXPERIMENTAL + kontrolki stylu + presety startowe (każdy styl × długości).
3. **Synthesis**: 4 osc, noise, transient, sub z RE4GE, resonance, sample + 8 warstw z solo.
4. **Processing**: 4 stopnie + Distortion Lab + waveshaper, filtry, EQ 8, dynamika, stereo.
5. **Modulation**: obwiednie, LFO + matrix, makra z mapowaniem, Randomizer (tryby / style / LOCK / seed), sekwencer.
6. **Pro tools**: browser + kategorie, A/B + morph, reference, batch / stems export, history, snapshot.
