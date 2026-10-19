# DARK SERPENT v1.0
Screamer / lead do dropów (raw hardstyle). VST3 (instrument), działa w FL Studio. Czarny + limonka, w tytule wąż z czarnego dymu.

## Instalacja (Windows)
Zakładka **Actions** → *Build Dark Serpent VST3* → **Artifacts** → `Dark-Serpent-VST3-Windows`.
Folder `Dark Serpent.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.

## Tor sygnału
```
CORE (supersaw do 16 głosów + warstwa oktawy) --+
EDGE (jasna, szeroka warstwa +1/+2 okt.)       --+--> TALK (filtr samogłoskowy A-E-I-O-U) -> FILTER --+--> AMP
BODY (czysty środek mono)  -------------------------------------------------------------------------+
-> RAGE (TUBE / HARD / FOLD, 2x OS, wyrównana głośność) -> OTT -> TONE -> WIDTH + MONO BASS -> DELAY / REVERB / PUMP
```

## Panele
| Panel | Co tam jest |
|---|---|
| CORE | fala SAW / SQUARE / PULSE / HYBRID, VOICES (1–16), DETUNE, OCT LAYER (warstwa +12), LEVEL |
| EDGE | +1 / +2 OCT, VOICES, DETUNE, TONE (od jakiej częstotliwości gra), LEVEL |
| BODY | SINE / TRI / SQUARE, OCTAVE (0 / -1), LEVEL |
| TALK | pad: w poziomie samogłoska A-E-I-O-U, w pionie ile "mówi"; kółko pokazuje samogłoskę na żywo (z MOVE / SWEEP) |
| FILTER | CUTOFF, RESO, TONE (jasność), F.ENV, F.DECAY, SWEEP (samogłoska jedzie przy każdej nucie), S.TIME, KEY (formanty idą za wysokością) |
| MOVE | LFO zsynchronizowane z tempem: kształt, RATE, > CUTOFF, > VOWEL, > AMP, RETRIG |
| PLAY | LEGATO / RETRIG / POLY (3 głosy – oktawy), 1ST ONLY (bend tylko na pierwszej nucie frazy), GLIDE, BEND, B.TIME, VIBRATO, V.RATE, V.DELAY, VELOCITY |
| RAGE | TUBE / HARD / FOLD, DRIVE, MIX, OTT |
| AMP | ATTACK, DECAY, SUSTAIN, RELEASE |
| FX | DELAY (TIME, FEEDBACK, MIX), REVERB (SIZE, MIX), PUMP (RATE, DEPTH) |
| ENERGY | makro: więcej RAGE i OTT, jaśniej, szerzej, mocniejsze vibrato i więcej przestrzeni |
| SPACE / OUT | WIDTH, MONO BASS (dół w mono), VOLUME |

## Brzmienie domyślne
Ustawienia domyślne (preset **Venom Lead**) dostrojone pomiarami do 7 referencyjnych leadów: gęsty unison z warstwą oktawy,
bardzo szerokie stereo, jasność 1.0–2.6 kHz pływająca z MOVE, mało przesteru.

## Presety
Venom Lead, Raw Serpent, Hiss of Chaos, Cobra Choir, Shedding Skin, Coil Stab, Black Smoke, Serpent Octaves, Init.
**SAVE** → `Dokumenty\Dark Serpent\Presets\*.dsp`, **RANDOMIZE**, **PLAY** (przytrzymaj).

Zbudowany na wspólnych klockach z `../Shared`.
