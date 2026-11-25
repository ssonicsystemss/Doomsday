# 4GE – ANALIZA SAMPLI wg protokołu właściciela (2026-11-25)

Sample: `hardcore-gabber-kick-8`, `hardcore-gabber-kick-162` (GABBER), `Tritone - Kick Main 1` (MILLENNIUM).
Pomiary: `FourGE/Tools/Analysis/kick_protocol.py` (oś czasu z punktów zmiany widma / pitchu, pitch z kopii LP 320 Hz,
9 pasm co 5 ms, udział energii NA harmonicznych nuty = „tonalność”, asymetria, clipping, ogon) + wcześniejsze pomiary
z tej sesji (modulacja pasm, szpilki, widmo harmonicznych). Refy są właściciela – nie ma ich w git.
„Tonalność” pasma 1.0 = cała energia leży na harmonicznych nuty; ~0.1–0.15 = praktycznie szum / energia poza harmonicznymi.

---

# SAMPLE ANALYSIS – hardcore-gabber-kick-8

## 1. GLOBAL CHARACTER
297 ms, 44.1 kHz / 16 bit, peak −0.01 dBFS (1 krótki odcinek na 0 dBFS). Tonalny kick: nuta ~59.7 Hz (A#1 +43 ct),
okrągły sinus + ostre szpilki. Największa energia: 60–120 Hz (nuta). Charakter zmienia się głównie w pierwszych ~150 ms
(pitch), potem stabilny; poziom spada w ostatnich ~40 ms.

## 2. TIMELINE (z punktów zmiany: 15, 40, 170, 195, 230, 255 ms)
0–15 ms: transient – najgłośniej 120–250 Hz, crest 11.6 dB (szpilki), brak stabilnego pitchu.
15–40 ms: punch – pitch ~95 Hz, nadal szpilki (crest 11.7 dB), energia 120–250 Hz.
40–170 ms: zjazd pitchu 89 → 60 Hz (osiada ~135 ms), najgłośniej 60–120 Hz, crest spada do 7 dB.
170–255 ms: stabilny ton 59.5 Hz, crest 4.3–4.9 dB.
255–297 ms: wygaszanie (−2 dB), crest rośnie do 7.6 dB (szpilki na cichszej fali).

## 3. TOK
Brak osobnego TOK (brak trzymanego wysokiego pitchu na starcie).

## 4. PUNCH
~0–40 ms, ~95 Hz, tonalny z nałożonymi szpilkami; energia low-mid (120–250 Hz). Nie jest odseparowany od BODY – ten sam ton
schodzi dalej. DISTORTED / HYBRID (tonalny + impulsy).

## 5. BODY
40–170 ms, 60–120 Hz, pitch 89 → 60 Hz (krzywa ~wykładnicza, stała ~35 ms). Płynne przejście w TAIL, bez osobnego transjentu.

## 6. CRUNCH
Brak osobnego tonalnego eventu crunch. Gęstość harmoniczna środka pochodzi z **dwóch krótkich szpilek na cykl**
(modulacja pasma 300–2500 Hz = 119 Hz = 2 × f0), na szczytach fali, skierowanych do środka, z małym „schodkiem” po szpilce.
W paśmie środka (200–2500 Hz) pomiędzy szpilkami prawie cisza: crest środka 16 dB, centroid 878 Hz.

## 7. SUB
Brak osobnej warstwy sub: najmocniejsza składowa poniżej nuty (50.5 Hz) jest 13 dB pod nutą = rozmycie / boczne pasmo, nie osobny ton.

## 8. TAIL
170–297 ms, 59.5–59.7 Hz, stabilny (±1 %). Harmoniczne ogona (dB wzgl. f0): h2 −17, h3 −15, h4 −21, h5 −24, h6 −25, h7 −33,
h8 −26 … do h16 ~−34. **Parzyste = nieparzyste** (różnica −0.1 dB). Ogon = główne źródło basu.

## 9. FREQUENCY BEHAVIOR
0–40 ms: 120–250 Hz dominuje; 40–170: przesuwa się do 60–120 Hz razem z pitchem; od 170: 60–120 Hz + płaskie „futro”
wysokich harmonicznych do >5 kHz (szpilki). Pasma powyżej 500 Hz w ogonie: tonalność 0.13–0.15 = energia poza harmonicznymi.

## 10. PITCH BEHAVIOR
start ~95 Hz (15–40 ms) → 59.7 Hz; osiada ~135 ms; potem stabilny (lekki spadek 59.6 → 58.7 Hz przy końcu).

## 11. DISTORTION
PUNCH: szpilki już od 0 ms. BODY / TAIL: fala bazowa prawie sinus (brak rails – 0 % próbek przy szynach), więc **nie ma
twardego clipa całego kicka**; nieliniowość widoczna jako szpilki na szczytach + schodek.

## 12. ASYMMETRY
OBSERVATION: parzyste harmoniczne tak mocne jak nieparzyste; szczyty +0.81 / −0.72 w 170–195 ms; udział próbek dodatnich
0.35–0.66 zmienia się między fazami. HYPOTHESIS: asymetryczne przetwarzanie lub dwie różne szpilki na cykl (dodatnia ≠ ujemna).
CONFIDENCE: MEDIUM.

## 13. REVERB / SPACE
Brak (koniec bez rozmytego ogona; mono / brak różnicy kanałów).

## 14. OBSERVED FACTS
- dwie szpilki na cykl, na szczytach, w stronę zera, + schodek; środek pomiędzy szpilkami cichy;
- fala bazowa okrągła (0 % rails), nie kwadrat;
- parzyste ≈ nieparzyste harmoniczne do h16;
- pitch ~95 → 59.7 Hz w ~135 ms, potem stabilny.

## 15. HYPOTHESES
- (A) szpilki = artefakt łączenia (splice) przy time-stretchu / pętli w samplerze, synchroniczne z okresem fali;
- (B) szpilki = efekt waveshapera z nieciągłością (fold / wrap) przy szczycie;
- (C) przester z mocnym EQ wysokich po nim (podbija krawędzie). Właściciel: tutorial = kick z FPC → 2× Fast Dist (PRE 100 %).
  Uwaga: 2× mocny przester daje kwadrat (rails), a ten sample ma 0 % rails → ten konkretny sample nie wygląda na
  „2× Fast Dist z pełnym PRE” bez późniejszej obróbki (np. ściszenia / EQ / filtra).

## 16. CONFIDENCE
HIGH: pitch, stabilność ogona, brak TOK, brak SUB, szpilki 2 / cykl, brak twardego clipa.
MEDIUM: asymetria, „schodek” po szpilce.
LOW: każda hipoteza produkcyjna (A / B / C).

## 17. 4GE IMPLEMENTATION
TAIL TYPE HARDCORE: SOURCE BD, pitch DROP ~25 st / EXP / TIME ~140 ms, HOLD ~18 ms; MID SCREAM FOLLOW PITCH, HARMONIC 2,
CLICK ~0.8, CLICK LEN ~0.2 ms, CLICK BAL ~0.6 (parzyste), CLICK NOISE ~0.3; DRIVE 1 umiarkowany; brak twardego clipa;
PRE DECAY dla ruchu na końcu; start kliknięć ~30 ms (właściciel: „niepotrzebnie klika na początku”).

## 18. PARAMETERS
`hc_src 2, hc_drop 25, hc_time 140, hc_hold 18, hc_ms_track 2, hc_ms_harm 2, hc_ms_click 0.8, hc_click_len 0.2,
hc_click_bal 0.6, hc_click_noise 0.3, hc_d1 ~11, hc_ms_start 30, hc_ms_fade 30, hc_pre_decay 0.6` (wynik dopasowania).

## 19. WHAT IS MISSING FROM 4GE
CURRENT 4GE MODEL DOES NOT FULLY EXPLAIN THIS SAMPLE: kliki MID SCREAM to impuls (+ szum), a w samplu po szpilce jest
**schodek** (poziom zostaje przesunięty do następnej szpilki). Brakuje: CLICK STEP (przesunięcie trzymane do następnego kliku)
– albo mechanizmu „splice” (pętla okresu z nieciągłością), jeśli hipoteza A się potwierdzi.

## 20. FINAL TECHNICAL RECOMMENDATION
Zostawić łańcuch: okrągła nuta + 2 kliki / cykl idące za pitchem (już w 4GE), dodać CLICK STEP i sprawdzić na ucho.
Do zweryfikowania u właściciela: czy „FPC + 2× Fast Dist” daje ten konkretny kształt (wysłać WAV przed / po).

---

# SAMPLE ANALYSIS – hardcore-gabber-kick-162

## 1. GLOBAL CHARACTER
431 ms, 44.1 kHz / 16 bit, peak −0.01 dBFS, **35 odcinków na 0 dBFS** (twarde ograniczenie na końcu łańcucha). Gęsty,
brudny kick; nuta ~53.5 Hz (G#1 +53 ct). Najwięcej energii: 120–250 Hz na starcie, potem 60–120 i 20–60 Hz.
Charakter zmienia się przez ~300 ms (długi zjazd pitchu).

## 2. TIMELINE (punkty zmiany: 25, 45, 100, 125, 300, 395 ms)
0–25 ms: transient / punch – najgłośniej **1–2 kHz**, ~155 Hz, prawie bez tonalności (0.05).
25–45 ms: 149 → 138 Hz, najgłośniej 120–250 Hz.
45–125 ms: 135 → 90 Hz, 60–250 Hz.
125–300 ms: 88 → 51.5 Hz (osiada ~250 ms), 60–120 Hz.
300–395 ms: stabilny ~52 Hz, 20–60 Hz.
395–431 ms: wygaszanie.

## 3. TOK
Brak (start ~155 Hz bez trzymanego wysokiego pitchu).

## 4. PUNCH
0–25 ms, energia 1–2 kHz, nietonalny (0.05) – NOISY / DISTORTED. Nie jest oddzielony od BODY (pitch ciągły).

## 5. BODY
25–125 ms, 150 → 90 Hz, gęsty, nietonalny w środku (0.02–0.10), crest ~5.5 dB.

## 6. CRUNCH
Brak osobnego eventu: brud środka (300–2500 Hz) w **seriach 2× na cykl** (modulacja 113 Hz = 2 × f0) przez cały kick;
tonalność środka 0.05–0.30. Wcześniej widoczne pasmo rezonansowe ~1.3 kHz.

## 7. SUB
Brak osobnej warstwy (składowa pod nutą −15 dB).

## 8. TAIL
125–395 ms; pitch dojeżdża do ~52 Hz dopiero ~250–300 ms. Harmoniczne: h2 −23, h3 −12, h4 −39, h5 −24, h6 −45 …
(**nieparzyste mocniejsze o ~10 dB**). Ogon = bas.

## 9. FREQUENCY BEHAVIOR
Start w 1–2 kHz → zjazd energii w dół razem z pitchem → koniec w 20–60 Hz. Środek i góra przez cały czas brudne (nietonalne).

## 10. PITCH BEHAVIOR
~155 Hz → 52 Hz, **wolny** zjazd ~250–300 ms (znacznie wolniej niż kick-8: 135 ms).

## 11. DISTORTION
Cały kick: crest 4.4–6.2 dB, 35 odcinków na 0 dBFS = twarde ograniczenie / clip na wyjściu; środek gęsty i nietonalny.

## 12. ASYMMETRY
OBSERVATION: h2 −23 dB przy nieparzystych −12 dB, szczyty symetryczne ±1.0, udział dodatnich 0.43–0.58.
HYPOTHESIS: głównie symetryczny clip; niewielka asymetria. CONFIDENCE: MEDIUM.

## 13. REVERB / SPACE
Brak wyraźnego.

## 14. OBSERVED FACTS
twardy limit na 0 dBFS; wolny zjazd pitchu ~250 ms; brud środka 2× na cykl; nieparzyste harmoniczne przeważają.

## 15. HYPOTHESES
Mocno przesterowany 909 / kick z wolnym pitchem + clip; brud środka = intermodulacja przesteru z szumem / rezonansem.
Ten sample zgadza się z „krótki kick → przester ×2 → clip” lepiej niż kick-8 (rails, crest ~5 dB). CONFIDENCE: LOW–MEDIUM.

## 16. CONFIDENCE
HIGH: pitch i jego czas, clip na wyjściu, brak TOK / SUB. MEDIUM: rezonans ~1.3 kHz, 2× / cykl. LOW: technika produkcji.

## 17. 4GE IMPLEMENTATION
HARDCORE: SOURCE 909, DROP ~20 st, TIME ~300 ms (wolno), FAST DIST 1 + 2 (PRE 100 %), MID SCREAM FOLLOW PITCH 2 + CLICK NOISE
(dłuższe serie 2–3 ms, środek), LOW PASS ~3.5–5 kHz, FINAL CLIP.

## 18. PARAMETERS
`hc_src 0, hc_drop 20, hc_time 300, hc_fd1_on/fd2_on 1, hc_fd1_pre/fd2_pre 1, hc_ms_track 2, hc_ms_harm 2, hc_ms_click 1,
hc_click_len 3, hc_click_noise 1, hc_lp 3500, hc_out_clip ~6`.

## 19. WHAT IS MISSING FROM 4GE
Rezonans ~1.3 kHz w brudzie (pasmowy, stały) – jest tylko jako EQ; jeśli ucho potwierdzi, przyda się REZONANS / FORMANT
przed przesterem. Brak pewności – do odsłuchu.

## 20. FINAL TECHNICAL RECOMMENDATION
Ten sample jest bliżej łańcucha z tutoriala (krótki kick → 2× Fast Dist → clip) niż kick-8; testować FAST DIST na nim.

---

# SAMPLE ANALYSIS – Tritone - Kick Main 1 (MILLENNIUM)

## 1. GLOBAL CHARACTER
707 ms (główny dźwięk do −30 dB: ~470 ms), 44.1 kHz / 24 bit, peak 0.0 dBFS (3 odcinki na 0 dBFS), mono (side −41 dB).
Tonalny: nuta ~53.7 Hz (A1 −40 ct), w ogonie tonalność 20–60 Hz 0.92, 120–250 Hz 0.86, 250–500 Hz 0.63.
Największa energia: 20–60 Hz (nuta). Właściciel: charakter = ref **bez 350–2500 Hz** (dół + góra).

## 2. TIMELINE (punkty zmiany: 15, 35, 365, 465 ms)
0–15 ms: transient – najgłośniej **500–1 kHz** (brzęczenie), bez pitchu nuty.
15–35 ms: brzęczenie 250–500 Hz + powolne „pchnięcie” dołu (pół okresu ~30–40 Hz), bez pitchu nuty.
35–365 ms: nuta: 79 → 57 Hz (osiada ~155 ms), potem stabilna 53.7 Hz; crest 4.7 dB; 2 % próbek przy szynach.
365–465 ms: koniec nuty + szybkie wygaszenie; **dzwonienie ~45 Hz** (pitch niżej niż nuta) przez ~2 okresy.
465–707 ms: bardzo cichy (−37 dB) tonalny ton 500–1 kHz schodzący ~790 → 350 Hz – ślad / pogłos po kicku.

## 3. TOK
Brak tonalnego TOK; start = brzęczenie ~600 Hz (11. harmoniczna nuty, wcześniejsze pomiary) + niskie pchnięcie.

## 4. PUNCH
0–35 ms: NIE ma tonu nuty; jest niski „bump” + brzęczenie. HYBRID (dół + brzęczenie).

## 5. BODY
35–~155 ms: zjazd 118 Hz (przy 40 ms) → 54 Hz, wykładniczy (stała ~45 ms), miękko nasycony sinus z asymetrycznym „hakiem”.

## 6. CRUNCH
Brak osobnego eventu crunch. Góra (>2.5 kHz) = **szum ~6 kHz w seriach 2× na cykl** (modulacja 107 Hz = 2 × f0),
poziom −13 dB (0–40 ms) → −19 (80 ms) → −24 dB (ogon) → −30 dB (koniec) wzgl. całości. To jest część „kawałka 3”.

## 7. SUB
Brak osobnej warstwy (pod nutą −27 dB). Nuta sama jest dołem.

## 8. TAIL
155–365 ms, 53.7 Hz stabilny, harmoniczne: h2 −26, h3 −18, h4 −19, h5 −28, h6 −27, h7 −19, h8 −31 … (parzyste ≈ nieparzyste, +2 dB).
Miękko przesterowany (crest 4.7 dB, 2 % rails) – nie kwadrat. Ogon = bas.

## 9. FREQUENCY BEHAVIOR
0–35 ms: 250 Hz–1 kHz (brzęczenie) + 20–60 Hz (bump); 35–155: energia schodzi z 120–250 do 20–60 Hz; ogon: 20–60 Hz +
harmoniczne do ~500 Hz (tonalne) + szum 2–10 kHz w seriach.

## 10. PITCH BEHAVIOR
brak nuty 0–35 ms → 79–118 Hz → 53.7 Hz (~155 ms) → przy zwolnieniu spada do ~45 Hz (dzwonienie).

## 11. DISTORTION
PUNCH: brzęczenie = mocne nieparzyste (kwadrat ~600 Hz). BODY / TAIL: umiarkowany przester (rails 2 %), harmoniczne do ~h16.
Góra: szum w seriach (nie harmoniczne).

## 12. ASYMMETRY
OBSERVATION: parzyste ≈ nieparzyste w ogonie, asymetryczny „hak” w każdym cyklu, udział dodatnich 0.49.
HYPOTHESIS: asymetryczne przetwarzanie / bias. CONFIDENCE: MEDIUM.

## 13. REVERB / SPACE
Po końcu: dzwonienie ~45 Hz (~2 okresy, ~40 ms) + bardzo cichy ton 790 → 350 Hz do ~670 ms. HYPOTHESIS: rezonans filtra
/ HP przy zwolnieniu albo pogłos o małym poziomie. CONFIDENCE: LOW. Nie jest głównym charakterem (−37 dB).

## 14. OBSERVED FACTS
brak nuty w pierwszych 35 ms; nuta 79–118 → 53.7 Hz wykładniczo; szum ~6 kHz 2× / cykl malejący −13 → −24 dB; ogon miękko
nasycony z parzystymi; dzwonienie przy zwolnieniu.

## 15. HYPOTHESES
Kick warstwowy: osobny punch / bump + ton z pitchem + przester (asymetryczny) + EQ podbijający górę po przesterze (szum
w seriach = przester szumu / clicku synchroniczny z falą). Właściciel: „EARLY Classic Boom + clipper 100 %” brzmi „trochę
millennium” – zgodne z „punch + body + przester kształtuje”.

## 16. CONFIDENCE
HIGH: pitch, czasy, brak nuty na starcie, szum 2× / cykl, poziomy pasm. MEDIUM: asymetria, rola „haka”. LOW: technika produkcji, pogłos.

## 17. 4GE IMPLEMENTATION
HARDCORE: SUB PUNCH (60 → 45 Hz, 25 ms) na „bump”; nuta: PUNCH −30 dB (cisza 0–38 ms), HOLD 38 ms, DROP 13.5 st, EXP TIME 180;
DRIVE 1 ~6 dB + ASYM 0.9 / BIAS 0.7 (parzyste); MID SCREAM OWN LEVEL, FOLLOW PITCH 2, CLICK 1, CLICK NOISE 1, CLICK LEN 1.5–2.5 ms,
DIP 0.7 od 60 ms (−13 → −24 dB), PRE-EQ HIGH +10..12 dB @ 3.5–4 kHz, LOW PASS 7–8 kHz (centroid ~6 kHz).
(Runda 3 – wersje A / B / C; góra zgodna z refem ±2 dB w każdym oknie.)

## 18. PARAMETERS
`hc_punch_db -30, hc_hold 38, hc_drop 13.5, hc_time 180, hc_curve 1, spk_mode 1 / 60 → 45 Hz / 25 ms, hc_d1 6, hc_stages 2,
hc_asym 0.9, hc_bias 0.7, hc_ms_own 1, hc_ms_track 2, hc_ms_harm 2, hc_ms_click 1, hc_click_noise 1, hc_click_len 1.5–2.5,
hc_click_bal 0.6, hc_ms_mix 1.2–1.5, hc_ms_dip 0.7, hc_ms_dip_at 60, hc_ms_dip_len 400, hc_eq_high_db 10–12 @ 3.5–4 kHz, hc_lp 7–8 kHz`.

## 19. WHAT IS MISSING FROM 4GE
- RELEASE / dzwonienie przy zwolnieniu z lekkim spadkiem pitchu (~45 Hz) – dziś HARDCORE kończy się 4 ms fadem.
- Brzęczenia ~600 Hz właściciel nie uznał za istotne (kawałek 3) – nie budować na nim profilu.

## 20. FINAL TECHNICAL RECOMMENDATION
Profil MILLENNIUM oprzeć na: punch / bump + tonalna nuta (umiarkowany, asymetryczny przester) + jasny szum 2× / cykl malejący
w czasie. Sprawdzić drugą drogę „EARLY + clip + asymetria” (właściciel: „trochę millennium”) i wybrać na ucho.

---

# PORÓWNANIE

| ELEMENT | kick-8 | kick-162 | Tritone Main 1 |
|---|---|---|---|
| TOK | NO | NO | NO (brzęczenie 600 Hz) |
| PUNCH | tonalny + szpilki | NOISY 1–2 kHz | bump + brzęczenie, bez nuty |
| BODY | tonalny 89 → 60 Hz | gęsty, nietonalny 150 → 90 Hz | miękki przester 118 → 54 Hz |
| CRUNCH | szpilki 2× / cykl | brud 2× / cykl | szum 6 kHz 2× / cykl |
| TAIL | 59.7 Hz, okrągły | ~52 Hz, clip | 53.7 Hz, miękki |
| zjazd pitchu | ~135 ms | ~250–300 ms | ~155 ms |
| SUB | NO | NO | NO |
| ASYMMETRY | MEDIUM (parzyste = nieparzyste) | LOW | MEDIUM (parzyste = nieparzyste) |
| clip na wyjściu | NO (0 % rails) | YES (35 odcinków) | lekko (2 % rails) |

**WHAT IS COMMON?** Brak TOK, brak osobnego SUB, ogon = bas (tonalny, stabilny). We wszystkich trzech **brud / szpilki
przychodzą 2× na cykl nuty** i idą za pitchem – to jest wspólny mechanizm.
**WHAT IS DIFFERENT?** charakter tej serii (ostra szpilka / szumowy brud w środku / jasny szum 6 kHz), czas zjazdu pitchu,
ilość clipu na wyjściu, obecność osobnego startu (Tritone: bump + cisza nuty).
**WHAT SHOULD BECOME A 4GE PARAMETER?** seria 2× / cykl synchroniczna z falą: długość, szum, pasmo (EQ / LP), bilans
dodatnia / ujemna, opadanie w czasie – już są (MID SCREAM CLICK / LEN / NOISE / BAL / DIP); brakuje CLICK STEP i RELEASE.
**WHAT SHOULD REMAIN STYLE-SPECIFIC?** pasmo serii (gabber: środek / szpilka, millennium: góra 6 kHz), czas zjazdu pitchu,
clip na wyjściu (gabber-162 tak, kick-8 nie), start z bumpem (millennium).
