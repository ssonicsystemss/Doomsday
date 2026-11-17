# 4TUNE v0.1 (wersja testowa)
Syntezator do hardstyle'owych melodii: plucki, leady, supersawy, melodie euphoric, festiwalowe leady. VST3 (instrument) do FL Studio.
Następca Fortune of Seekers (FoS zostaje jak był – stare projekty działają dalej).

## Instalacja (Windows)
Build robi się sam na GitHubie: zakładka **Actions** → *Build 4TUNE VST3* → **Artifacts** → `4TUNE-VST3-Windows`.
Folder `4TUNE.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.

## Tor sygnału (pasek na górze, kliknięcie otwiera stronę)
```
OSC 1 + OSC 2 -> GRUPA A (głośność / pan / szerokość) --+
                                                         +-> FILTR -> OBWIEDNIA AMP -> SCREAM -> EQ -> DIST -> CHORUS -> DELAY -> REVERB -> STEREO -> LIMITER
OSC 3 + OSC 4 -> GRUPA B (głośność / pan / szerokość) --+
```

## FORTUNE (duża złota gałka)
Jedna gałka, która przerabia dźwięk: **0 %** czysty, przyjemny pluck → **50 %** jaśniejszy, szerszy → **100 %** szeroki, jasny, agresywny festiwalowy lead.
Naraz: filtr w górę (do +2 oktaw), detune unisona (grupa B mocniej, grupa A zostaje skupiona), szerokość stereo, SCREAM
(przy SCREAM 0 dokłada tylko trochę, przy włączonym dużo), pogłos (mix do +15, rozmiar do +10 punktów).
**FORTUNE nigdy nie zastępuje Twoich gałek – dokłada do nich** (KOŃCOWA WARTOŚĆ = GAŁKA + FORTUNE). Pod gałką widać na żywo, ile dokłada.
Na stronie MOD → **FORTUNE TARGETS** ustawiasz, ile FORTUNE rusza każdą rzecz (np. FORTUNE bez pogłosu).

## Strona SYNTH
- **OSC 1–4:** kształt (SAW / SINE / TRIANGLE / SQUARE / PULSE / WAVETABLE), OCT / SEMI / FINE, LEVEL, PAN, SPREAD (stereo oscylatora),
  WT POS (WAVETABLE: sinus → trójkąt → piła → kwadrat; PULSE: szerokość impulsu), WARP (zgięcie fali), VOICES (unison 1–16), DETUNE,
  UNI SPREAD (rozłożenie głosów w stereo), UNI MIX (głośność bocznych głosów – „supersaw mix”), PHASE, RANDOM (losowa faza), RETRIG
  (włączone = każda nuta zaczyna od tej samej fazy; wyłączone = fala biegnie dalej).
- **GRUPA A (CORE)** = podstawa melodii, **GRUPA B (EXPANSION)** = szerokość / supersaw / góra. Każda grupa: VOLUME, PAN, WIDTH.
- **FILTER:** LP 12 / LP 24 / HP 12 / HP 24 / BAND / NOTCH, CUTOFF, RESONANCE, DRIVE, KEY TRACK, ENV AMOUNT; ROUTE (A + B / tylko A / tylko B przez filtr).
- **FILTER ENVELOPE / AMP ENVELOPE:** ATTACK, HOLD, DECAY, SUSTAIN, RELEASE, CURVE. Pluck = krótki decay i mały sustain, lead = duży sustain.
- **SCREAM:** krzyczący przester z kontrolą głośności (więcej SCREAM = inny charakter, nie głośniej). DRIVE, SATURATION, TONE, BITE (ostrość 2–3 kHz),
  BODY (ile dołu idzie przez przester), MIX, OUTPUT; 1x / 2x / 4x = jakość (oversampling).
- **PLAY:** POLY / MONO / LEGATO, liczba głosów, GLIDE, zakres pitch bend, TRANSPOSE, MASTER.

## Strona MOD
LFO 1 / 2 (SINE, TRIANGLE, SAW, RAMP, SQUARE, S&H, RANDOM; SYNC z nutami, też z kropką i triolami; PHASE, FADE IN, RETRIG),
MOD ENVELOPE, **MOD MATRIX** 16 slotów: źródło (obwiednie, LFO, velocity, mod wheel, aftertouch, key track, random, FORTUNE) →
cel (pitch, fine, wavetable, warp, cutoff, rezonans, detune, stereo, pan, głośność, poziomy oscylatorów i grup, drive filtra, SCREAM, delay, reverb), ilość ±.

## Strona FX
EQ (4 pasma), DISTORTION, CHORUS, DELAY (z tempem, ping-pong, filtr), REVERB (pre-delay, rozmiar, decay, tłumienie, szerokość), STEREO
(szerokość, balans, mono bas), LIMITER (input, ceiling, release), MASTER.

## Strona MELODY
Generator melodii + piano roll + przeciąganie MIDI do FL – **następny etap** (spec 21–24).

## Presety
Kategorie z nazwy: PLUCKS, EUPHORIC, LEADS, RAW, SUPERSAWS, FESTIVAL, CLASSIC, KEYS, PADS, BASS, EXPERIMENTAL, INIT.
Klik w nazwę presetu = przeglądarka (szukanie, kategorie, ulubione *). **SAVE** → `Dokumenty\4TUNE\Presets\*.4tune`.
**RANDOMIZE** = SMART RANDOM (muzyczne zakresy, bez przesady z detune / rezonansem / przesterem / pogłosem / głośnością),
**FULL RANDOM** = wszystko w całym zakresie.
