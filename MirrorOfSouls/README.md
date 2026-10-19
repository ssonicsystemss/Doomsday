# MIRROR OF SOULS v1.0
8-pasmowy equalizer do hardstyle (efekt VST3, FL Studio). Czarny + lodowy błękit.

## Instalacja (Windows)
Zakładka **Actions** → *Build Mirror of Souls VST3* → **Artifacts** → `Mirror-of-Souls-VST3-Windows`.
Folder `Mirror of Souls.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.
To efekt: wstaw go w slot miksera (Mixer → slot → Mirror of Souls).

## Ekran
- Widmo dźwięku **po** korekcji (wypełnione) i **przed** nią (cienka linia), biała krzywa EQ, numerowane punkty pasm.
- **Przeciągnij punkt**: częstotliwość + wzmocnienie (Shift = 0 dB). Złapanie wyłączonego pasma włącza je.
- **Kółko myszy** nad punktem: szerokość (Q). **Dwuklik na punkcie**: 0 dB. **Dwuklik na pustym**: nowe pasmo BELL w tym miejscu.
  **Prawy klik**: włącz / wyłącz pasmo.
- Po najechaniu: częstotliwość, nuta (C5 = środkowe C jak w FL) i dB; nad punktem jego ustawienia.
- Pasma DYNAMIC pokazują kółkiem, gdzie jest ich wzmocnienie w tej chwili.

## Pasmo (dół, po wybraniu numeru 1–8)
LOW CUT / LOW SHELF / BELL / NOTCH / HIGH SHELF / HIGH CUT, nachylenie cięć 12 / 24 / 48 dB, FREQ, GAIN, Q,
STEREO / MID / SIDE (działa na całość, sam środek albo same boki), **DYNAMIC** + THRESH (pasmo działa dopiero,
gdy dźwięk w jego zakresie przekroczy próg – np. zbija piszczące rezonanse tylko wtedy, gdy się pojawiają).

## SOUL (prawo)
- **AMOUNT** 0–150%: jak mocno działa cała krzywa (0% = płasko; cięcia odjeżdżają na skraj pasma).
- **OUTPUT**, **MONO BASS** (boki poniżej tej częstotliwości są usuwane – dół w mono), **ANALYZER** OFF / POST / PRE + POST.

## Presety
Flat, KICK (Punch Focus, Clean Tail, Raw Crunch, Sub Weight), HIHAT (Crisp, Soft, Air, Tame Harsh),
SNARE / CLAP (Body, Crack, Clap Wide, Box Fix), BASS (Clean Low, Growl, Sub Only, Kick Space),
LEAD / SCREECH (Presence, Screech Tame, Wide Air, Mid Push), MELODY / MIDS (Clear Mids, Pluck Snap, Pad Smooth, Chords Vocal Room).
Presety to punkt wyjścia – dobierz AMOUNT i przesuń punkty pod swój dźwięk. **SAVE** → `Dokumenty\Mirror of Souls\Presets\*.mos`.

Zbudowany na wspólnych klockach z `../Shared`.
