# Promo – film reklamowy na TikToka

Generator renderuje prawdziwy plugin klatka po klatce: dźwięk z Prophecy of Scream, obraz z jego interfejsu
(wykres, widmo, scope, miernik i gałki ruszają się razem z muzyką). Od v3.1: wygląd minimal / flat i **sterowany kursor**,
który łapie kropki na wykresie (CUTOFF, FORMANT, OSC 2) – parametry zmieniają się naprawdę, więc słychać ruch filtra i formantu. Kick pochodzi z Ancient Drums i leży gotowy w `assets/kick.flac`.

## Render
```
Promo/render.sh
```
Wynik: `Promo/out/Prophecy_of_Scream_TikTok.mp4` (1080×1920, 30 fps, ~30 s) + klatki kontrolne `still_*.png`.
Pierwszy raz trwa dłużej (kompilacja), kolejne rendery ok. 5 minut.

## Co gdzie zmienić
| Co | Gdzie |
|---|---|
| Napisy, kamera i kolejność scen | `sections` w `promo.cpp` |
| Ruchy kursora i kropek na wykresie (zakresy Hz / rezonans / gain, kiedy łapie i puszcza) | blok `the scripted mouse` w `promo.cpp` (`keys` / `logKeys` = punkty w czasie sceny 0..1) |
| Presety w scenach | `switch (sec)` i `presetCycle` w `promo.cpp` |
| Riff (nuty) | `barA / barB / barC` i `riff` w `promo.cpp` (siatka szesnastek: step 0..15, `slide` = legato do następnej nuty) |
| Tempo, liczba taktów, kiedy gra kick | `song.h` |
| Miks (sidechain, głośność) | koniec `main()` w `promo.cpp` |
| Inny kick | wyrenderuj nowy `assets/kick.flac` (`tools/kick_from_ancient_drums.cpp`); generator bierze z niego jedno uderzenie (takt 2) i kładzie na każdą ćwierćnutę |

Generator nie wchodzi do builda VST3 (opcja `PROPHECY_BUILD_PROMO`, domyślnie wyłączona).
