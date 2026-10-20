# Promo – film reklamowy na TikToka

> **Uwaga (v3.1):** narzędzie jest zrobione pod stary wygląd v3.0 (StageLookAndFeel, fonty Michroma / Barlow, zakładki FxTabs).
> Od v3.1 plugin ma wygląd minimal / flat, więc `PROPHECY_BUILD_PROMO` się nie zbuduje – do nowego filmiku trzeba przepiąć
> kamerę / zakładki na `OscCard` i fonty na `doomsday::flat`. Stara wersja: historia gita (przed v3.1).

Generator renderuje prawdziwy plugin klatka po klatce: dźwięk z Prophecy of Scream, obraz z jego interfejsu
(scope, miernik i gałki ruszają się razem z muzyką). Kick pochodzi z Ancient Drums i leży gotowy w `assets/kick.flac`.

## Render
```
Promo/render.sh
```
Wynik: `Promo/out/Prophecy_of_Scream_TikTok.mp4` (1080×1920, 30 fps, ~20 s) + klatki kontrolne `still_*.png`.
Pierwszy raz trwa dłużej (kompilacja), kolejne rendery ok. 5 minut.

## Co gdzie zmienić
| Co | Gdzie |
|---|---|
| Napisy, kamera i kolejność scen | `sections` w `promo.cpp` |
| Presety w scenach | `switch (sec)` i `presetCycle` w `promo.cpp` |
| Riff (nuty) | `phrase` w `promo.cpp` |
| Tempo, liczba taktów, kiedy gra kick | `song.h` |
| Miks (sidechain, głośność) | koniec `main()` w `promo.cpp` |
| Inny kick | wyrenderuj nowy `assets/kick.flac` (`tools/kick_from_ancient_drums.cpp`) |

Generator nie wchodzi do builda VST3 (opcja `PROPHECY_BUILD_PROMO`, domyślnie wyłączona).
