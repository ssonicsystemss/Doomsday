# 5CR34M 4LPH4 (v0.4 – wersja testowa)
Nowy syntezator screechy do hardstyle / rawstyle, następca Prophecy of Scream, pisany od zera na podstawie twoich wzorców
(analiza: `PLAN.md`). VST3 (instrument) do FL Studio.

## Jak zdobyć
GitHub → zakładka **Actions** → *Build 5CR34M 4LPH4 VST3* → ostatni build → **Artifacts** → `5CR34M-4LPH4-VST3-Windows`.
Folder `5CR34M 4LPH4.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.

## Jak to działa (tor dźwięku)
```
NOTE THROW (zjazd / podjazd wysokości na KAŻDEJ nucie, też legato) + GLIDE
   ├─ CORE  – czysty rdzeń: sinus → trójkąt → kwadrat (SHAPE), oktawa -1 / 0 / +1
   └─ HASH  – brudna góra: 1–7 rozstrojonych głosów (DETUNE, SAW), FM (RATIO, FM), CHAOS (każdy głos pływa po swojemu),
              GRIT (odrobina szumu – dużo = odkurzacz), LOW CUT, DECAY / SUSTAIN (hasz najjaśniejszy na starcie nuty)
              → SCREAM: DRIVE (5 krzywych) → COMB (filtr grzebieniowy strojony do nuty) → FORMANT (szczyt „głosu”)
CORE + HASH → OUTPUT: DRIVE 2 → TONE → LOW CUT → AMP (ADSR) → CHORUS (szerokość jak w hooverze) → VOLUME
   → FILTER (analogowy ladder: LP 24 / LP 12 / BAND / HIGH, CUTOFF, RESO)
   → SPACE: OTT → DELAY (z tempem FL, ping-pong, czas 1/16…1/2) → REVERB
MOD 1 / MOD 2 – każdy rusza JEDNĄ wybraną gałką (cutoff, formant, drive, FORTUNE, delay… prawie każdą)
```

**v0.4 – MOD 1 / MOD 2** (twój projekt):
- Duże okno na dole z zakładkami **MOD 1** (zielony) i **MOD 2** (fioletowy).
- Kształt: OFF / RISE / FALL / UP-DN / WOBBLE / STEPS / **CUSTOM**. Długość 1/16 … 1 takt, z tempem FL.
- **CUSTOM = rysujesz sam**: klik = nowy punkt, przeciągnij = przesuń, kółeczko w połowie linii = wygnij,
  dwuklik / prawy klik na punkcie = usuń, dwuklik na kółeczku = prosto. Punkty skaczą co 1/16 cyklu (SHIFT = bez skakania).
  Klik w gotowy kształt zamienia go na punkty, więc go tylko poprawiasz.
- **ASSIGN** → klikasz dowolną gałkę → gałka dostaje kolor MOD-a, przycisk pokazuje jej nazwę. **×** = odepnij.
- **+ / −** i suwak zakresu: suwak to drugi koniec ruchu. **+**: od pozycji gałki w górę do suwaka, **−**: w dół do suwaka.
  Kręcisz gałką za suwak → suwak jedzie razem z nią. Pierścień na gałce pokazuje zakres i gdzie MOD ją teraz trzyma.
- Stara automatyka cutoffu (v0.3) wczytuje się sama jako MOD 1 → CUTOFF.

**v0.3:**
- **FORTUNE** (duża gałka, jak w Fortune of Seekers) – jedno kręcenie: więcej hasza, przesteru, chaosu, detune, formantu,
  otwiera filtr i ton, mocniejszy rzut nuty, chorus i pogłos; głośność trzyma się w miejscu.
- Automatyka cutoffu (w v0.4 zamieniona na MOD 1). Gdy FL gra – MOD trzyma się taktu; gdy stoi – startuje z pierwszą nutą.

**v0.2:** dodana sekcja SPACE (OTT, delay, reverb) – v0.1 był „suchelec”. Presety mają już ustawioną przestrzeń i są głośniejsze.
**RANDOM** – losuje nowe brzmienie w sensownych zakresach i sam ustawia głośność (każdy random ok. tak głośny jak presety).
**SAVE** – zapis do `Dokumenty\5CR34M 4LPH4\Presets` (lista USER w presetach). UNDO / REDO na górze. Dwuklik na gałce = wartość domyślna, klik w liczbę = wpisz wartość.

## Presety (v0.1)
- **HOOVER - Classic Juno** – rozstrojone piły + kwadrat oktawę niżej, podjazd od dołu, chorus.
- **HOOVER - Rave Dive** – zjazd z góry o oktawę na każdej nucie, mocniejszy przester i formant.
- **HOOVER - Hardstyle Scream** – ostrzejszy: FM, twardy przester, filtr grzebieniowy, krótki rzut nuty.
- **SCREECH - Short Shot / Massive Dive / Loop Hash / Bright Talker / Classic Wail / Digital Rise** – dobrane automatem do twoich wzorców
  (FHSP 107, Massive 023, OPS loop 9, FHSP 069, Sylenth 008, Serum 018), na poprawionym silniku (bez „odkurzacza”), z przestrzenią, głośność wyrównana.
- **SCREECH - Owner 6 / Owner 7 / Owner 8** – twoje własne brzmienia (z RANDOM + FORTUNE). RANDOM losuje najczęściej w ich klimacie: 45 % „szeroki” (jak 6 / 7: dużo rozstrojonych pił, mocny drive), 30 % „wokalny” (jak 8: mało głosów, comb, mocny formant, FORTUNE na full), 25 % dziko.
- **INIT** – punkt startowy.

To wersja do testów: dźwięk i gałki będą się jeszcze zmieniać. Pisz, co brzmi źle.
