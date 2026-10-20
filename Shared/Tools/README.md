# Shared/Tools – małe skrypty analizy dźwięku (Python: numpy / scipy)
- `mix_band_report.py <wav> [nazwy]` – energia w pasmach + centroid + płaskość 2–8 kHz dla kolejnych taktów (A/B presetów przy 150 BPM)
- `kick_band_report.py <wav...>` – pasma / centroid / RMS / peak ogona kicka (2. uderzenie przy 150 BPM, WAV 24-bit)
- `f0_track_example.py` – śledzenie wysokości (autokorelacja) dla screechy
Większe zestawy: `ForgeOfWrath/Tools` (dopasowywanie kicków, CMA-ES, A/B mp3), `Tools/screech` (Prophecy of Scream).
