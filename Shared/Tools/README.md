# Shared/Tools – małe skrypty analizy dźwięku (Python: numpy / scipy)
- `mix_band_report.py <wav> [nazwy]` – energia w pasmach + centroid + płaskość 2–8 kHz dla kolejnych taktów (A/B presetów przy 150 BPM)
- `kick_band_report.py <wav...>` – pasma / centroid / RMS / peak ogona kicka (2. uderzenie przy 150 BPM, WAV 24-bit)
- **`doomscope/`** – DOOMSCOPE: pełny analizator sampli dla Claude'a (pitch kicka, strojenie ogona, LUFS / true peak, harmoniczne, pasma, stereo, A/B `compare`) – zacznij tu, opis w `doomscope/README.md`
- `f0_track_example.py` – śledzenie wysokości (autokorelacja) dla screechy
Większe zestawy: `ForgeOfWrath/Tools` (dopasowywanie kicków, CMA-ES, A/B mp3), `Tools/screech` (Prophecy of Scream).
- `midi_grid.py <file.mid> [bars]` – onset grid per bar of a MIDI file (top voice, bass in lower case); used to derive the 4TUNE melody rules from the owner's reference MIDIs (no dependencies).
