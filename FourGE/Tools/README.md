# 4GE (rebuild) – narzędzia dla programisty
Python (numpy / scipy / matplotlib). Kicki referencyjne są właściciela – NIE w repo.
- `kmatch4.py <ref.wav> <nuta MIDI> <fine ct> <out.json>` – dopasowuje ~16 parametrów modułu 01 do referencji (losowanie + zejście po
  współrzędnych dla CHARACTER x źródło), render przez `RenderTest <dir> --render <settings.txt> <out.wav>`; ścieżka binarki w `BIN`.
- `kab4.py` – mp3 A/B: REF x4, 4GE x4 na 150 BPM (wyrównany szczyt, cięcie na bit z 6 ms fade).
- `kana.py <wav...>` – raport: f0 w czasie, RMS, udział 6 pasm, centroid, długość. `kplot.py` – fala + spektrogram REF vs 4GE.
Uruchamiaj z katalogu roboczego, w którym jest `tools/` (ścieżki względne `tools/kmatch4.py`, `match2/`).
- `kq.py <wav...>` – jakość kicka: gęstość (płaskość 1–8 kHz w ogonie), pisk (wąskie prążki 1,5–6 kHz), wejście basu, kontrast tok → ogon, udział > 2 kHz.
- `presets_v06.txt` – specyfikacja 24 presetów stylów dla `RenderTest <out> --presetlab presets_v06.txt` (`<refs>` = folder z referencjami właściciela).
