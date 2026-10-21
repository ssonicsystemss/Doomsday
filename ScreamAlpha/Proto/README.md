# 5CR34M 4LPH4 – engine prototype (offline, no plugin yet)

- `../Source/AlphaEngine.h` – the voice: CORE (sine/tri/square) + HASH (unison FM + CHAOS noise in the phase → HP → drive → comb → formant),
  per-note pitch SCOOP (also legato) and hash envelope, drive 2, tone, low cut. 4x oversampled.
- `AlphaRender.cpp` – `AlphaRender out.wav note=69 dur=1.6 [param=value]`, `pattern=<file> bpm=150`, `params=<file>`.
- `../Tools/amatch.py` – fits the engine to a reference (sep-CMA-ES from ForgeOfWrath/Tools): early + late spectrum shape, noisiness,
  harmonic-to-noise, brightness movement, loudness envelope. ~1 min for 50 generations on 2 cores.
- `sounds/*.txt` – fitted starting sounds (FHSP 107 hit, Massive 023 dive, OPS loop 9); `riff16.txt` – the promo 16th riff as a pattern.

Build: `ln -sfn /home/claude/AncientDrums/JUCE ScreamAlpha/JUCE` then
`cmake -S ScreamAlpha/Proto -B <build> -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C <build> AlphaRender`.
