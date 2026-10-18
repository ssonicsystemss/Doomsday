# FORTUNE OF SEEKERS v1.0
Syntezator do melodii hardstyle (leady, supersawy, plucki, pady, hoovery). VST3 (instrument), działa w FL Studio.

## Instalacja (Windows)
Build robi się sam na GitHubie: zakładka **Actions** → *Build Fortune of Seekers VST3* → **Artifacts** → `Fortune-of-Seekers-VST3-Windows`.
Folder `Fortune of Seekers.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.

## Tor sygnału
```
PART A (zielony):  OSC A1 + OSC A2 -> FILTER -> AMP -> DIST -> CHORUS -> DELAY -> REVERB -> PUMP -> LEVEL --+
PART B (fiolet):   OSC B1 + OSC B2 -> FILTER -> AMP -> DIST -> CHORUS -> DELAY -> REVERB -> PUMP -> LEVEL --+--> VOLUME
```
Polifonia do 12 głosów (akordy) albo **MONO** (legato + GLIDE). Oscylatory, filtry i DIST liczone z 2× oversamplingiem.

## Oscylator (4 sztuki: A1, A2, B1, B2)
| | |
|---|---|
| SIN / TRI / SAW / SQR / PLS / NSE | kształt fali (PLS = puls 25%, NSE = szum) |
| VOICES | unison 1–9 głosów |
| OCTAVE / SEMI | strojenie (np. SEMI +7 = kwinta) |
| VOLUME | głośność oscylatora |
| PHASE | faza startowa przy każdej nucie, na końcu skali **FREE** (losowa) |
| DETUNE / STEREO | rozstrojenie i rozłożenie głosów unisona w stereo |
| PAN | pozycja całego oscylatora |
| SCREAM | przester + zawijanie fali jak w Prophecy of Scream – nosowy, krzyczący charakter |

## PART A / PART B
- **Filtr:** LP12 / LP24 / BAND / HIGH, CUTOFF, RESO, DRIVE, F.ENV (otwarcie filtra na początku nuty), F.DECAY.
- **Obwiednia:** ATTACK, DECAY, SUSTAIN, RELEASE.
- **Efekty (zakładki, każda z ACTIVE):**
  - **DIST:** SOFT / HARD / TUBE / SCREAM, DRIVE, MIX.
  - **CHORUS:** RATE, DEPTH, MIX.
  - **DELAY:** TIME w nutach, FEEDBACK, MIX, P.PONG.
  - **REVERB:** SIZE, DAMP, MIX.
  - **PUMP:** sidechain zsynchronizowany z tempem projektu; RATE, DEPTH, RELEASE.

## Środek
- **FORTUNE** – makro: otwiera filtry obu części, poszerza detune, dokłada scream i pogłos.
- **THE PATH** – dźwięk PART A (zielony) i PART B (fiolet) płynie z góry na dół; na górze to, co gra teraz.
- **GLOBAL** – A LEVEL, B LEVEL, GLIDE, VOLUME, MONO.
- **PLAY** (nagłówek) – przytrzymaj, żeby zagrać akord C-dur.

## Presety
Seeker's Anthem, Golden Compass, Treasure Screech, Lost Map Pad, Raw Fortune, Euphoric Horizon, Hoover Seekers, Crystal Arp, Init.
**SAVE** → `Dokumenty\Fortune of Seekers\Presets\*.fos`. **RANDOMIZE** losuje lead / pluck / pad w rozsądnych zakresach.

## Fonty
Michroma i Barlow Semi Condensed (SIL Open Font License, licencje w `Resources/Fonts`).
