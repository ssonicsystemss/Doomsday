# 4GE – ANALIZA WSZYSTKICH REFÓW wg protokołu (2026-11-25) – tabele porównawcze + wnioski grup

Pomiary: `Tools/Analysis/kick_protocol.py` (FAKTY), tabele: `Tools/Analysis/protocol_table.py`. Szczegółowe raporty
(kick-8, kick-162, Tritone Main 1): `Analysis/2026-11-25_GABBER_MILLENNIUM.md`.
Legenda: *pitch start → settle* = pierwszy pewny pitch → kiedy pitch osiada na nucie ogona (±3 %); *TOK* = trzymany wysoki
pitch na starcie; *rails* = największy udział próbek przy 0 dBFS w jednej fazie (twardy clip); *clip runs* = liczba odcinków
na 0 dBFS; *even−odd* = energia parzystych minus nieparzystych harmonicznych ogona (≈0 = równowaga = asymetria,
>+20 = prawie same nieparzyste = symetryczny przester / kwadrat); *noise share mids* = udział energii 250 Hz–2 kHz POZA
harmonicznymi nuty w najdłuższej fazie (brud / intermodulacja / szum).

### HARDCORE

| sample | ms | sr/bit | tail Hz (MIDI) | pitch start → settle | TOK | punch: loudest band | rails (max) | clip runs | tail even−odd dB | tail h2/h3/h4 | noise share mids (tail) | SUB |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| OPS_FREE_OPS_FREE_HARDCORE_KICK_18 | 333 | 44k/16 | 52.3 (32.12) | 722 → 185 ms | 722 Hz 15 ms | 500-1k | 36 % | 312 | 7.1 | -21/-12/-23 | 55 % | NO |
| OPS_FREE_OPS_FREE_HARDCORE_KICK_24 | 300 | 44k/16 | 78.3 (39.11) | 99 → 165 ms | NO | 500-1k | 29 % | 99 | 0.1 | -25/-17/-14 | 53 % | NO |
| OPS_FREE_OPS_FREE_HARDCORE_KICK_29 | 300 | 44k/16 | 78.3 (39.12) | 116 → 135 ms | NO | 500-1k | 64 % | 173 | -1.0 | -26/-15/-25 | 47 % | NO |
| OPS_FREE_OPS_FREE_HARDCORE_KICK_31 | 333 | 44k/16 | 48.3 (30.75) | 558 → None ms | 558 Hz 15 ms | 1-2k | 44 % | 187 | 0.3 | -20/-17/-17 | 99 % | NO |
| OPS_FREE_OPS_FREE_HARDCORE_KICK_34 | 300 | 44k/16 | 64.1 (35.66) | 1204 → None ms | NO | 1-2k | 92 % | 367 | 1.9 | -4/3/-3 | 86 % | YES |
| OPS_FREE_OPS_FREE_HARDCORE_KICK_39 | 300 | 44k/16 | 47.8 (30.57) | 928 → None ms | NO | 500-1k | 1 % | 1 | -0.9 | 3/2/-7 | 91 % | YES |
| OPS_FREE_OPS_FREE_HARDCORE_KICK_46 | 308 | 44k/16 | 77.1 (38.84) | 152 → 180 ms | NO | 500-1k | 31 % | 129 | -7.2 | -11/-18/-21 | 82 % | NO |
| OPS_FREE_OPS_FREE_HARDCORE_KICK_47 | 300 | 44k/16 | 50.5 (31.51) | 1289 → None ms | NO | 500-1k | 1 % | 1 | -0.8 | -20/-24/-23 | 98 % | NO |
| OPS_FREE_OPS_FREE_HARDCORE_KICK_55 | 300 | 44k/16 | 55.3 (33.1) | 99 → 185 ms | NO | 60-120 | 51 % | 111 | -8.2 | -1/-23/-16 | 51 % | YES |
| OPS_FREE_OPS_FREE_HARDCORE_KICK_56 | 333 | 44k/16 | 76.3 (38.67) | 289 → 190 ms | NO | 500-1k | 56 % | 287 | 3.0 | -19/-20/-21 | 80 % | NO |

### MILLENNIUM

| sample | ms | sr/bit | tail Hz (MIDI) | pitch start → settle | TOK | punch: loudest band | rails (max) | clip runs | tail even−odd dB | tail h2/h3/h4 | noise share mids (tail) | SUB |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Tritone_-_Kick_21.wav | 1305 | 44k/16 | 54.9 (32.98) | 95 → 170 ms | NO | 500-1k | 99 % | 60 | 9.5 | -26/-15/-31 | 51 % | NO |
| Tritone_-_Kick_25.wav | 1180 | 44k/16 | 56.4 (33.43) | 102 → 195 ms | NO | 120-250 | 4 % | 17 | 49.4 | -68/-13/-69 | 8 % | NO |
| Tritone_-_Kick_29.wav | 811 | 44k/16 | 56.5 (33.48) | 277 → 205 ms | 277 Hz 20 ms | 1-2k | 4 % | 21 | 4.0 | 4/9/13 | 73 % | NO |
| Tritone_-_Kick_3.wav | 527 | 44k/16 | 56.5 (33.48) | 123 → 235 ms | NO | 120-250 | 2 % | 8 | 31.0 | -49/-17/-62 | 84 % | NO |
| Tritone_-_Kick_30.wav | 632 | 44k/16 | 56.8 (33.54) | 90 → 205 ms | NO | 120-250 | 1 % | 3 | 45.5 | -71/-21/-75 | 85 % | NO |
| Tritone_-_Kick_4.wav | 1307 | 44k/16 | 54.9 (32.98) | 80 → 175 ms | NO | 500-1k | 99 % | 82 | 6.3 | -23/-15/-28 | 61 % | NO |
| Tritone_-_Kick_8.wav | 649 | 44k/16 | 50.5 (31.52) | 93 → 140 ms | NO | 120-250 | 0 % | 1 | 8.5 | -21/-16/-23 | 84 % | NO |
| Tritone_-_Kick_Main_1.wav | 707 | 44k/24 | 53.7 (32.59) | 79 → 155 ms | NO | 500-1k | 2 % | 3 | 2.2 | -26/-18/-19 | 66 % | NO |
| Tritone_-_Kick_Main_2.wav | 847 | 44k/24 | 53.4 (32.48) | 79 → 170 ms | NO | 500-1k | 8 % | 18 | 5.0 | -24/-15/-22 | 41 % | NO |
| Tritone_-_Main_Kick_Crunch_1.wav | 705 | 44k/24 | 53.7 (32.6) | 163 → 155 ms | 163 Hz 10 ms | 500-1k | 0 % | 1 | -3.0 | -6/-4/-4 | 44 % | NO |

### UPTEMPO

| sample | ms | sr/bit | tail Hz (MIDI) | pitch start → settle | TOK | punch: loudest band | rails (max) | clip runs | tail even−odd dB | tail h2/h3/h4 | noise share mids (tail) | SUB |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| uptempo-kick-piercing-slide_200bpm | 652 | 44k/16 | 47.1 (30.32) | 740 → None ms | 740 Hz 40 ms | 500-1k | 54 % | 199 | -13.5 | -21/-26/-31 | 66 % | NO |
| uptempo-rawstyle-kick_190bpm_C.wav | 324 | 44k/16 | 50.9 (31.65) | 287 → 135 ms | NO | 250-500 | 73 % | 29 | 5.6 | -20/-12/-23 | 71 % | NO |

### RAW_CRACK

| sample | ms | sr/bit | tail Hz (MIDI) | pitch start → settle | TOK | punch: loudest band | rails (max) | clip runs | tail even−odd dB | tail h2/h3/h4 | noise share mids (tail) | SUB |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| OPS_FREE_RAWSTYLE_PUNCH_10_C.wav | 387 | 44k/16 | 0.0 (None) | 531 → None ms | NO | 500-1k | 68 % | 168 | None | - | - | NO |
| OPS_FREE_RAWSTYLE_PUNCH_11.wav | 387 | 44k/16 | 0.0 (None) | 804 → None ms | NO | 1-2k | 44 % | 130 | None | - | - | NO |
| OPS_FREE_RAWSTYLE_PUNCH_15_D.wav | 387 | 44k/16 | 0.0 (None) | 651 → None ms | NO | 500-1k | 43 % | 122 | None | - | - | NO |
| OPS_FREE_RAWSTYLE_PUNCH_20_F.wav | 387 | 44k/16 | 351.7 (65.12) | 470 → 55 ms | NO | 250-500 | 21 % | 72 | -7.0 | -14/-26/-14 | 96 % | NO |
| rawstyle-kick_G_major.wav | 396 | 44k/16 | 52.8 (32.28) | 65 → 155 ms | NO | 500-1k | 0 % | 1 | 4.1 | -32/-13/-22 | 73 % | NO |

## WNIOSKI – HARDCORE (10 × OPS)
FAKTY: nuty 48–78 Hz; start najgłośniejszy w 500 Hz–2 kHz (środek, nie dół); pitch osiada w 135–190 ms; **8 / 10 twardo
przyciętych** (rails 29–92 %, 99–367 odcinków na 0 dBFS); 39_G i 47_G bez clipu (1 odcinek) – inna obróbka końcowa;
brud środka 47–99 % (bardzo dużo energii poza harmonicznymi); parzyste / nieparzyste różnie (−8 … +7 dB).
TOK: 18_G (722 Hz, ~15 ms) i 31_G (558 Hz, ~15 ms). Uwaga: wcześniejszy pomiar przejść przez zero dla 31_G pokazał ~2 kHz przez
0–8 ms – rozbieżność metod, CONFIDENCE MEDIUM. 34_G: nuta ogona wyszła 64 Hz (plik „G” = 49 Hz) – CONFIDENCE LOW.
HYPOTHESIS (MEDIUM): ton z pitchem + druga składowa harmoniczna przycięte razem (2-tonowy clip – potwierdzone wcześniej na 31_G).

## WNIOSKI – MILLENNIUM (10 × Tritone)
FAKTY: nuty bardzo spójne **50.5–56.8 Hz** (G#1–A1); pitch osiada wolniej niż w hardcore: **140–235 ms**; długie (0.5–1.3 s);
**większość bez clipu** (rails 0–8 %) – wyjątki Kick 4 i Kick 21 (99 % rails = kwadraty, 1.3 s); dwa typy ogona:
(1) z parzystymi (Main 1, Main 2, Kick 8: even−odd +2…+8.5) i (2) prawie same nieparzyste (Kick 3, 25, 30: +31…+49 dB =
symetryczny przester); TOK tylko w Kick 29 (277 Hz, 20 ms) i Main Kick Crunch 1 (163 Hz, 10 ms); brud środka 8–86 %.
Właściciel: charakter = bez 350–2500 Hz (dół + góra) – zgodne z: tonalna nuta + jasny szum w seriach (Main 1).

## WNIOSKI – UPTEMPO (2)
piercing-slide: **TOK 740 Hz trzymany ~40 ms** (16. harmoniczna nuty 47 Hz), potem nuta; 54 % rails.
rawstyle 190: 287 → 51 Hz w 135 ms, **73 % rails**, nuty 47–51 Hz.

## WNIOSKI – RAW CRACK (5)
4 × OPS punch = sample samego punchu (387 ms, bez ogona / nuty): środek 250 Hz–2 kHz, mocno przycięte (21–68 % rails).
rawstyle-kick_G_major: pełny kick, 65 → 52.8 Hz w 155 ms, bez clipu na wyjściu.

## WHAT IS COMMON?
- ogon = tonalna nuta 48–78 Hz (bas) we wszystkich pełnych kickach; osobny SUB – w żadnym (pod nutą zawsze ≥12 dB niżej);
- duży udział energii środka POZA harmonicznymi (brud) w prawie wszystkich – to nie czysty przester tonu;
- pitch osiada w 135–235 ms (czas zależny od stylu).

## WHAT IS DIFFERENT?
- clip na wyjściu: HARDCORE / UPTEMPO / RAW tak (twardy), MILLENNIUM zwykle nie;
- czas zjazdu pitchu: HARDCORE / UPTEMPO 135–190 ms, MILLENNIUM 140–235 ms;
- TOK o stałym pitchu: rzadki (18_G, 31_G, piercing-slide, Tritone 29) – nie cecha stylu, tylko konkretnego kicka;
- symetria: część refów ma parzyste ≈ nieparzyste (asymetria), część prawie same nieparzyste.

## WHAT SHOULD BECOME A 4GE PARAMETER? (większość już jest w TAIL TYPE HARDCORE)
PITCH HOLD (TOK o stałym pitchu) · czas / krzywa zjazdu · FINAL / OUTPUT CLIP (ilość clipu na wyjściu) · bilans parzyste /
nieparzyste (ASYMMETRY, FUZZ BIAS, FAST DIST ASYM) · brud środka synchroniczny z falą (MID SCREAM CLICK / NOISE / LEN / BAL) ·
RELEASE / dzwonienie przy końcu (BRAKUJE).

## WHAT SHOULD REMAIN STYLE-SPECIFIC?
czas zjazdu pitchu, ilość clipu na wyjściu, pasmo brudu (gabber środek, millennium góra ~6 kHz), długość, start (bump /
punch / TOK).

## CURRENT 4GE MODEL DOES NOT FULLY EXPLAIN
- wolne dzwonienie przy końcu (Tritone: ~45 Hz po zwolnieniu) – brak RELEASE z rezonansem;
- „schodek” po szpilce (kick-8) – brak CLICK STEP;
- 34_G / 31_G: rozbieżne pomiary nuty / TOK – do potwierdzenia uchem właściciela.
