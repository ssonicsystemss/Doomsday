# DOOMSCOPE – analizator sampli dla Claude'a

Zamienia dźwięk w liczby i wykresy, które Claude potrafi rzetelnie ocenić (Claude nie słyszy – patrzy na pomiary).
Wymaga tylko `numpy`, `scipy`, `matplotlib` (+ `ffmpeg` dla MP3/FLAC itp.). Podstawa: `RESEARCH.md` (deep research, 2026-11-27).

## Komendy
```
python doomscope.py analyze kick.wav [--type auto|kick|screech|lead|rbass] [--bpm 150] [--key F#] [--out DIR]
python doomscope.py compare moj.wav referencja.wav [--bpm 150] [--out DIR]
python doomscope.py selftest
```
Wynik `analyze`: `report.md` (liczby – główne źródło prawdy), `report.json`, `1_przebieg.png`, `2_spektrogram_pitch.png`, `3_widmo_pasma.png`.
Wynik `compare`: `compare.md` (tabela A / B / Δ, różnice widma w 1/3 okt., null test) + `compare_1_czas.png`, `compare_2_widmo.png`.

## Co mierzy (v0.1 = P0 z RESEARCH.md)
- **Pitch kicka**: przejścia przez zero (przester ich nie przesuwa) + dopasowanie modelu zjazdu w dziedzinie fazy
  (exp / 2×exp / potęgowy, wybór AIC) → częstotliwość w 0…1000 ms, połowa zjazdu, czas do 1 półtonu i do 3 centów.
- **Strojenie ogona**: sygnał analityczny na ogonie → Hz, nuta, centy, dryf, odchyłka od `--key`; flaga gdy ogon wciąż zjeżdża.
- **Screech / lead / reverse bass**: YIN → mediana F0, zakres, vibrato (szybkość + głębokość w centach), modulacja głośności.
- **Harmoniczne**: order tracking (przepróbkowanie po fazie) → H1…H16, THD, parzyste/nieparzyste, energia nieharmoniczna (aliasing + szum).
- **Poziomy**: sample peak, true peak 4×, LUFS wg BS.1770 (filtr K dla każdego sr), max momentary, „hit loudness”,
  **pętla w BPM** (kick co ćwierćnutę, ucinany na następnym uderzeniu) – integrated / short-term / TP.
- **Obwiednia**: atak 10–90 % i „weakest effort”, zaniki do −6/−12/−20/−40/−60 dB, T60, crest (całość i pierwsze 15 ms),
  zaniki osobno dla sub / body / crunch.
- **Pasma** SUB / LOW / LOWMID / CRUNCH / PRESENCE / AIR – udział energii i przebieg w czasie.
- **Clipping** (hard / soft jako charakterystyka, nie błąd), **stereo** per pasmo (korelacja, S/M), DC, klik na końcu, cisza na starcie.

## Sprawdzone (`selftest`)
Filtr K = współczynniki ITU @48 kHz, sinus 997 Hz → −3,01 LUFS, true peak sinusa fs/4, 4 syntetyczne kicki
(ogon w ±2 centach, krzywa pitchu @5–50 ms w ±5 centach), lead z vibrato 6 Hz ±50 c, piła H2 −6 dB / H3 −9,5 dB.

## Ograniczenia
- Progi w „Uwagach” (np. strój ±15 c, korelacja <0,9 poniżej 120 Hz) to hipotezy – kalibrować na kickach referencyjnych właściciela.
- Ocena przez porównanie z referencją (`compare`) jest pewniejsza niż progi z internetu.
- Następne (P1): spektrogram reasygnowany, wielorozdzielczy YIN, HPSS, `gen-tests` / `plugin-test` (testy wtyczek jak Plugin Doctor).
