# DOOMSCOPE – analizator sampli i wtyczek dla Claude'a

Zamienia dźwięk w liczby i wykresy, które Claude potrafi rzetelnie ocenić (Claude nie słyszy – patrzy na pomiary).
Wymaga tylko `numpy`, `scipy`, `matplotlib` (+ `ffmpeg` dla MP3/FLAC itp.). Podstawa: `RESEARCH.md` (deep research, 2026-11-27).

## Komendy
```
python doomscope.py analyze kick.wav [--type auto|kick|screech|lead|rbass] [--bpm 150] [--key F#] [--ref profil/profile.json]
python doomscope.py compare moj.wav referencja.wav [--bpm 150]
python doomscope.py profile folder_z_referencjami/ [--bpm 150] [--out profil]
python doomscope.py gen-tests [--sr 44100]            # sygnał testowy dla EFEKTÓW (do FL)
python doomscope.py plugin-test eksport_z_FL.wav       # analiza: charakterystyka, przester, aliasing, dynamika
python doomscope.py null render_v1.wav render_v2.wav   # czy dwie wersje brzmią identycznie
python doomscope.py selftest                           # 21 testów kontrolnych na sygnałach o znanych parametrach
```

## analyze → `report.md` (liczby = główne źródło prawdy) + 4 obrazy
- **Pitch kicka**: przejścia przez zero (przester ich nie przesuwa) + dopasowanie modelu zjazdu w dziedzinie fazy
  (exp / 2×exp / potęgowy, wybór AIC) → Hz w 0…1000 ms, połowa zjazdu, czas do 1 półtonu i 3 centów.
  **Niezależna kontrola** YIN w 5 rozdzielczościach (ZGODNE / ROZBIEŻNE).
- **Strojenie ogona**: sygnał analityczny → Hz, nuta, centy, dryf, odchyłka od `--key`; flaga gdy ogon wciąż zjeżdża.
- **Screech / lead / reverse bass**: YIN → mediana F0, zakres, vibrato (szybkość + głębokość w centach), modulacja głośności.
- **Harmoniczne**: order tracking → H1…H16, THD, parzyste/nieparzyste, energia nieharmoniczna (aliasing + szum).
- **Poziomy**: sample / true peak, LUFS (BS.1770, każde sr), max momentary, hit loudness, **pętla w BPM**.
- **Obwiednia**: atak, zaniki, T60, crest (całość / 0–15 ms), zaniki osobno sub / body / crunch.
- **Pasma** SUB / LOW / LOWMID / CRUNCH / PRESENCE / AIR (udział + przebieg w czasie), szczyty widma, deskryptory.
- **Transjent vs ton** (HPSS), **ostrość** (przybliżona DIN 45692, 1 acum = szum 1 kHz 60 dB), clipping, stereo per pasmo, DC, kliki.
- Obrazy: `1_przebieg`, `2_spektrogram_pitch`, `3_widmo_pasma`, **`4_punch_lupa`** (spektrogram reasygnowany 0–80 ms = ostre linie częstotliwości).

## profile → kalibracja na kickach właściciela
Folder z kickami, które brzmią dobrze → `profile.json` (mediana, p10–p90 każdej metryki) + `metrics.csv`.
`analyze --ref profile.json` dopisuje tabelę „Na tle referencji”: gdzie sample wypada i co odstaje najbardziej (**!**).
Nuta ogona jest pomijana (zależy od tonacji tracku).

## gen-tests / plugin-test → test efektu jak Plugin Doctor
1. `gen-tests` tworzy jeden plik (~65 s) + instrukcję `JAK_TESTOWAC.txt`.
2. W FL: plik na Playlist od taktu 1, na mikserze tylko testowana wtyczka, eksport WAV.
3. `plugin-test` mierzy: latencję i polaryzację, charakterystykę częstotliwościową + fazę (sweep Farina, kalibrowany),
   wzmocnienie / THD / H2 / H3 dla sinusów 100 Hz i 1 kHz od −36 do 0 dBFS (gdzie zaczyna się przester),
   aliasing (tony 5 / 7 / 11 kHz + spektrogram sweepu 5–20 kHz), IMD, krzywą statyczną, atak / release, koherencję szumu.
Tylko efekty (EQ, przestery, kompresory, reverby). Instrumentów (4GE, RE4GE) tak nie testujemy.

## Sprawdzone (`selftest`, 21 testów)
Filtr K = współczynniki ITU, 997 Hz → −3,01 LUFS, true peak, 4 syntetyczne kicki (ogon ±2 c, krzywa pitchu ±5 c), vibrato,
harmoniczne piły, reasygnacja (sinus / impuls), HPSS, ostrość 1 acum, plugin-test na wtyczkach o znanych parametrach
(przezroczysta 0,00 dB, EQ +6 dB błąd ≤0,01 dB, tanh +6,28 dB vs teoria 6,33, opóźnienie, aliasing, kompresor 4:1), null test.

## Test na ślepo (2026-11-27) i lekcje
`FourGE/build/.../RenderTest <out> --blind <dir> [n]` = n zupełnie nowych kicków z 4GE (losowy styl, seed, AMOUNT, nuta MIDI,
RANDOMIZE 1–3×), przepis w `_SECRET.txt` – do sprawdzania DOOMSCOPE bez znajomości odpowiedzi.
Pierwszy test (6 kicków) wykazał w v0.2: tylko 1/6 rozpoznany jako kick, 2/6 złe nuty (E1 czytane jako F1/F#1 – okno YIN 2048
nie widzi < ~43 Hz). v0.2.1: ogon mierzony w paśmie subu (LP 130 Hz, YIN ~190 ms), zapasowa krzywa pitchu z YIN gdy przejścia
przez zero zawodzą (mocny przester / warstwy) → 6/6 kick, 6/6 właściwa nuta (±20 c).
Działało od początku: „trzaski” (ZAAG TRASK) widoczne w przebiegu i spektrogramie, warstwy tonalne (TOK ~730 Hz, SCREECH ~920 Hz).
Słabe: pierwsze ~10 ms krzywej pitchu przy zapasowej metodzie; reasygnacja poniżej ~150 Hz (za krótkie okno) faluje.

## Ograniczenia
- Progi w „Uwagach” to hipotezy – lepiej porównywać z referencjami (`profile`, `compare`).
- Ostrość to przybliżenie (do porównań między samplami, nie certyfikowany ISO 532-1).
- Ucho właściciela ma ostatnie słowo.
