# THRONE OF TITANS v1.0
Wielopasmowy mastering + limiter + pomiar głośności (efekt VST3, FL Studio). Jak Maximus, tylko z dodatkami. Czarny + platyna.

## Instalacja (Windows)
Zakładka **Actions** → *Build Throne of Titans VST3* → **Artifacts** → `Throne-of-Titans-VST3-Windows`.
Folder `Throne of Titans.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.
Wstaw go jako **ostatni** efekt na kanale **Master** w mikserze FL.

## Jak zacząć (3 kroki)
1. Wybierz preset pod gatunek (np. **HARDSTYLE – Club Loud**) i ustaw **TARGET** (−7 dla hardstyle).
2. Kręć **LIMIT**, aż SHORT-TERM LUFS zbliży się do celu (liczba zrobi się zielona). Pasek LIMITER nie powinien stale stać na -8 dB i więcej.
3. Włącz **MATCH** i porównaj: wtyczka ścisza się do głośności oryginału – słyszysz, czy brzmi lepiej, a nie tylko głośniej.

## Ekran THE THRONE
Widmo po masteringu (wypełnione) i przed (pomarańczowa linia), trzy pasma LOW / MID / HIGH, **przeciągnij pionowe linie**,
żeby zmienić podział pasm, **kliknij pasmo**, żeby je edytować. Słupki = ile dB pasmo ścisza (zielony = podbicie upward).

## BAND (wybrane pasmo)
Włącznik, **SOLO** (słyszysz tylko to pasmo), PRE, THRESH, RATIO (ściszanie głośnych), **UPWARD** (podbijanie cichych – jak OTT),
KNEE, ATTACK, RELEASE, **SATURATE** (ciepło / brud), **WIDTH** (szerokość stereo tylko tego pasma: LOW na 0% = bas w mono),
POST. Obok krzywa: jak pasmo zmienia głośność (wejście → wyjście).

## MASTER
**LIMIT** (jak mocno pchasz w limiter = głośność), INPUT, TILT (+ jaśniej / − ciemniej), MONO BASS, **CLIP** (miękki clipper
ścina szczyty przed limiterem – głośniej i bardziej punchy), **CEILING** (maksymalny true peak w dBTP, standard −0,3; pod
streaming −1), RELEASE limitera.

## LOUDNESS
SHORT-TERM / MOMENTARY / INTEGRATED LUFS (norma EBU R128 / BS.1770), cel i różnica, paski IN / OUT / LIMITER / CLIP / TRUE PEAK,
**MATCH**, **RESET** (od nowa liczy integrated i true peak). Limiter pilnuje true peak (szczyty między próbkami) z lookahead 1,5 ms.

## Presety
Default Master; HARDSTYLE – Club Loud; RAWSTYLE – Brutal; EUPHORIC – Wide & Clear; UPTEMPO – Smash; FRENCHCORE – Dense Wall;
HARDCORE – Punch Keeper; STREAMING – Gentle −1 dBTP; OTT – Hyper Dense; WARM – Tape Glue; BRIGHT – Air Lift; WIDE – Stereo Master;
BUS – Kick Punch / Lead Glue / Vocal Smooth / Drum Crunch; LIMITER ONLY – Clean / Clip + Limit.
Głośność zależy od Twojego miksu – preset to punkt wyjścia, LIMIT dostrajasz pod cel. **SAVE** → `Dokumenty\Throne of Titans\Presets\*.tot`.

Zbudowany na wspólnych klockach z `../Shared` (`DoomsdayDynamics.h`: crossover, kompresor up/down, limiter true peak, LUFS).
