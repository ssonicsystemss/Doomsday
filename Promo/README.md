# Promo – film reklamowy na TikToka

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
