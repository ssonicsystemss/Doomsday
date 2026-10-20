# Forge of Wrath – narzędzia do dopasowywania presetów (dla programisty)

Skrypty Python (numpy / scipy / matplotlib), które dopasowują parametry Forge of Wrath do kicków referencyjnych.
Kicki referencyjne NIE są w repo (to sample użytkownika) – wrzuć je do `ref/` i `ref2/` obok `runall.sh`.

- `kmatch.py` – przestrzeń parametrów, render przez `RenderTest batch`, cechy (widmo w czasie, obwiednia, pitch, LTAS, gęstość ataku) i odległość
- `ktune.py` – sep-CMA-ES: 3 starty (TUBE / HARD / SOFT) + dostrojenie, wynik do `tune/<nazwa>.json`
- `runall.sh` – lista kicków (nazwa, nuta MIDI, plik), pomija już dostrojone
- `kpresets.py` – z `tune/*.json` robi `Source/FactoryPresets.inc` (z wyrównaniem głośności OUTPUT)
- `kab.py` – mp3 A/B (4× referencja, 4× Forge), `kcmp.py` / `katt.py` / `kharm.py` / `kzoom.py` / `kflat.py` – wykresy porównawcze
- `kfeat.py`, `kspec.py` – analiza i spektrogramy referencji
- `tuned/` – dotychczasowe wyniki (silnik v1; v2 ma nowe parametry, więc do przestrojenia)

Ścieżki do binarki i katalogu roboczego są na górze `kmatch.py` (`BIN`, `SHM`).
