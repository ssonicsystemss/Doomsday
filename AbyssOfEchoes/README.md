# ABYSS OF ECHOES v1.0
Reverb + delay + stereo do miksu hardstyle (efekt VST3, FL Studio). Czarny + bursztyn.

## Instalacja (Windows)
Zakładka **Actions** → *Build Abyss of Echoes VST3* → **Artifacts** → `Abyss-of-Echoes-VST3-Windows`.
Folder `Abyss of Echoes.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.
To efekt: wstaw go w slot miksera. Na ścieżkę (insert) – zostaw **DRY** włączone. Na osobną szynę (send) – wyłącz **DRY**.

## Ekran
- **THE ABYSS** – oś czasu: słupki to kolejne echa delaya (lewy kanał nad linią, prawy pod), bursztynowa mgła to ogon
  reverbu (kreska PRE = pre-delay). Wszystko świeci mocniej, gdy efekt gra.
- **STEREO FIELD** – goniometr (środek w górę, boki w lewo / prawo) + korelacja (+1 = mono, 0 = bardzo szeroko, minus = problem w mono).

## DELAY
STEREO / PING-PONG / WIDE (prawy kanał 1,5× dłuższy), **SYNC** (czas w nutach z tempa projektu; wyłączone = ms),
TIME, FEEDBACK, LOW CUT / HIGH CUT (filtry w pętli – każde kolejne echo ciemniejsze), WOBBLE (falowanie jak taśma),
DRIVE (echa się przesterowują), **DUCK** (echa cichną, gdy grasz, i wychodzą w przerwach), MIX.

## REVERB
PRE-DELAY, SIZE, DECAY (0,2–20 s), DAMP (wycisza górę ogona), LOW CUT, MOTION (ruch ogona), **SHIMMER** (ogon idzie
oktawę w górę – anielskie pady), DUCK, WIDTH, MIX, **FREEZE** (zatrzymuje ogon na zawsze – do breakdownów / przejść).

## STEREO / OUTPUT
DELAY > REV (echa wpadają do pogłosu), WIDTH (0% = mono, 200% = bardzo szeroko), SPREAD (szerokość także z mono źródeł),
MONO BASS (boki poniżej tej częstotliwości usuwane – dół zostaje w mono), MIX (ile całego efektu), DRY, OUTPUT.

## Presety
Default Space; LEAD – Euphoric Hall, Drop Ducked, Screech Plate, Wide Doubler; RAW – Screech Ping; UPTEMPO – Tight Space;
MELODY – Pluck Echo; PAD – Endless Wash; BREAKDOWN – Cathedral, Shimmer Heaven; VOCAL – Ping Throw, Slap Room;
KICK – Short Room; SNARE – Big Plate; CLAP – Wide Hall; FX – Dub Echo, Tape Wobble, Endless Abyss;
MIX – Glue Room, Stereo Widener, Mono Bass Fix. **SAVE** → `Dokumenty\Abyss of Echoes\Presets\*.aoe`.

Zbudowany na wspólnych klockach z `../Shared` (`DoomsdaySpace.h`: delay, reverb FDN, shimmer, ducking, mono bass).
