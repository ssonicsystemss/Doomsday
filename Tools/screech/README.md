# Prophecy of Scream – narzędzia dopasowywania screechy (dla programisty)

Użyte przy PoS v3 (11 presetów dopasowanych do screechy referencyjnych użytkownika). Referencje (WAV) nie są w repo.
- `feat.py` – cechy screecha (pasma, centroid, płaskość, korelacja stereo, crest, f0 przez YIN)
- `tune.py` – render kandydata przez `RenderTest held <note> prog=-1 name=... hold=... param=value ...` i porównanie z referencją
- `search.py` – losowe przeszukiwanie parametrów, `pair.py` / `spec.py` – pary spektrogramów ref vs PoS, `embed.py`, `gen_presets.py` – presety do kodu
- `*.json` – zapisane punkty startowe / przestrzenie poszukiwań, `run3.sh` – przebieg, `pairs.txt` – pary referencja ↔ preset
Ścieżki są na górze plików (RenderTest w `build/` w katalogu głównym repo).
