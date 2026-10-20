# Analizy cudzych wtyczek – co robią dobrze i co z tego bierzemy

Zbiór danych na przyszłość. Każda analiza: **budowa → co chwalą / krytykują → wnioski dla Doomsday** (z przypisaniem do naszej wtyczki).
Krótkie, najważniejsze zasady trafiają też do `LESSONS.md`. Serum 2 i Sylenth1 są w `LESSONS.md`.

---
## Kick 2 (Sonic Academy) → Forge of Wrath
Źródła: MusicRadar (recenzja Kick 2), MusicTech, Gearnews, KVR.
- **Budowa:** syntetyczny SUB (pitch + amp envelope, kształt fali z 8 harmonicznych addytywnie) + **3 warstwy CLICK z sampli**, każda
  z własną obwiednią i keytrack on/off. Krzywe Béziera z przyciąganiem do nut, pitch pokazany w Hz **i w nazwach nut**, bank gotowych krzywych.
  Podgląd: fale wszystkich warstw nałożone, każdą można ukryć, zoom w pionie i poziomie (dopasowanie suba do clicka).
  Przester (clip / tube / wave), 4-pasmowy EQ (gałki gain też na głównym ekranie). **RENDER: eksport WAV w wybranej tonacji i velocity
  prosto z wtyczki.** Velocity 0–100 %, portamento. Presety zawierają swoje sample (przenośne).
- **Chwalą:** prostota + głębia edycji, czysty silnik, eksport WAV, biblioteka 180 clicków.
- **Krytykują:** słabe wbudowane efekty, click tylko z sampli, presety pod big room.
- **Dla Forge:**
  1. **EXPORT WAV** w tonacji (np. G1, F1) + długość / velocity – producenci warstwują i wrzucają kicki do sampli.
  2. Pitch w **nazwach nut** (oś PITCH: G1, G2…), przyciąganie kropek do nut / ms.
  3. Podgląd **warstw osobno** (TAIL / PUNCH / CLICK / SAMPLE) z ukrywaniem + zoom.
  4. Więcej warstw sampla (2–3) z własną obwiednią i keytrack; preset zapisuje swój sample obok siebie.
  5. Efekty robić porządnie albo wcale (zarzut do Kick 2).

## FabFilter Pro-Q 4 → Mirror of Souls
Źródła: MusicTech, Sound On Sound, Tape Op.
- **Budowa:** 24 pasma, 10 kształtów, **nachylenie płynnie 12–96 dB/okt**, interakcja Q–gain (bell węższy przy większym gainie,
  jak konsola), pasmo na Stereo / L / R / Mid / Side. **„Make Dynamic” jednym kliknięciem, próg automatyczny**, attack / release,
  sidechain. Spectral dynamics (jak resonance suppressor). Tryb Character: Warm / Gentle (nasycenie). **EQ Sketch – rysujesz
  przybliżoną krzywą, wtyczka sama wstawia półki i bele.** Instance List (wszystkie EQ w projekcie w jednym oknie), EQ match.
- **Chwalą:** szybkość pracy (Sketch, Instance List), dostępne dynamiczne EQ, łagodna krzywa nauki. **Krytykują:** cena, nie zastąpi Soothe.
- **Dla Mirror:**
  1. **Dynamic z automatycznym progiem** (bez ręcznego THRESH) i dynamika „spectral” na rezonanse (piszczące screeche / hihaty).
  2. **EQ MATCH**: dopasuj barwę do referencji (np. kick do kicka z sampla) – mamy już narzędzia porównywania widm z Forge.
  3. Rysowanie krzywej (Sketch), płynne nachylenie cięć, kanały L / R obok M / S, tryb Character (ciepło).

## FabFilter Pro-L 2 → Throne of Titans
Źródła: Sound On Sound, MusicRadar, Production Expert, Tape Op.
- **Budowa:** **style limitera** (algorytmy o różnym charakterze, w presetach: Loud / Moderate / Safe), dwustopniowy: lookahead
  na transjenty + attack / release na ogólną dynamikę, **channel link osobno dla transjentów i release**, true peak, oversampling
  do 32x (4x zwykle wystarcza), **UNITY GAIN** (słuchasz bez efektu głośniej = lepiej), **AUDITION** (słyszysz tylko to, co limiter
  zabiera), duży wykres redukcji w czasie z etykietami szczytów, LUFS M / S / I, DC offset, dither.
- **Chwalą:** brzmienie, interfejs, metering głośności, „żadnych minusów”. **Krytykują:** TP i oversampling kosztują latencję / lekko zmiękczają.
- **Dla Throne:**
  1. **AUDITION / DELTA** – posłuchaj, co zabiera limiter i clipper (od razu słychać przester).
  2. **Style limitera** (CLEAN / PUNCHY / AGGRESSIVE = różne lookahead / hold / release).
  3. **Wykres redukcji w czasie** (przewijany, ze szczytami) zamiast samego paska.
  4. Channel link (transjenty / release). MATCH już mamy (to nasz unity gain).

## Valhalla VintageVerb / Supermassive → Abyss of Echoes
Źródła: Valhalla DSP („VintageVerb: The Modes”), Rekkerd, Magnetic Mag, Dubspot.
- **Budowa:** **MODE jako główna gałka** – każdy tryb to osobny algorytm z własnym charakterem (Concert Hall, Plate, Room, Chamber,
  Dirty Hall / Plate z artefaktami starego sprzętu, Smooth – nowoczesne, Chaotic – wow & flutter jak taśma, Cathedral, Palace…),
  kolory epok (1970s / 1980s / Now). Mało gałek: MIX, PREDELAY, DECAY, SIZE, **ATTACK (balans wczesnych odbić i ogona / narastanie)**,
  EARLY / LATE, HIGH / LOW CUT, MOD RATE / DEPTH. Modulacja: chorus (Hall / Plate) albo **losowe opóźnienie bez zmiany wysokości** (Random).
- **Chwalą:** brzmienie, prostota, tani (Supermassive za darmo), szybkie zmiany charakteru jednym przełącznikiem.
- **Dla Abyss:** tryby reverbu o wyraźnie różnych charakterach (HALL / PLATE / ROOM / DIRTY / CHAOTIC), gałka ATTACK,
  modulacja losowa (bez „falowania” wysokości – ważne pod leady i screeche), kolor epoki (bit-redukcja, filtry starego sprzętu).

## Spire (Reveal Sound) → Prophecy of Scream / Dark Serpent / Fortune of Seekers
Źródła: Wikipedia, Splice blog („Exploring Synthesis with Spire”), MusicRadar.
- **Budowa:** 4 oscylatory, każdy w **7 trybach (Classic, Noise, FM, HardFM, SawPWM, AMSync, Vowel)** z **dwiema gałkami CTRL A / CTRL B,
  których znaczenie zmienia się z trybem** + WT MIX (wavetable 49 fal). Unison do 9 głosów z **trybami (oktawy, akordy – stack)**, DRIFT
  (analogowe pływanie), tryby mono / legato / poly. **12 filtrów (Perfecto / Acido / Infecto), dwa szeregowo lub równolegle, LINK cutoffów.**
  4 obwiednie, 4 LFO, 2 step sequencery, arpeggiator, macierz 15 slotów (2 źródła → 4 cele). Efekty: shaper, phaser, chorus, delay, reverb;
  na wyjściu kompresor + EQ.
- **Chwalą:** moc w prostym interfejsie, elastyczny routing, mały CPU; klasyk hardstyle'owych screechy i leadów (FM / HardFM + filtr).
- **Dla naszych syntezatorów:**
  1. **Wzór interfejsu „TRYB + CTRL A / CTRL B”**: jeden oscylator, wiele algorytmów, zawsze te same dwie gałki – prosto, a głęboko.
  2. **Unison z akordami / oktawami** (stack) – grube leady jednym palcem.
  3. Dwa filtry szeregowo / równolegle z LINK (mamy SVF + nowy `filter::Ladder`), DRIFT, step sequencer jako źródło modulacji.

---
## Wspólne wzorce (wszystkie analizy)
- **Słuchanie różnicy**: delta / audition / unity gain (Pro-L 2, nasze MATCH) – producent od razu słyszy, co robi efekt.
- **Rysowanie zamiast kręcenia**: EQ Sketch, krzywe Béziera w Kick 2, nasze obwiednie w Forge.
- **Tryb jako główny wybór charakteru** (Valhalla MODE, Spire tryby oscylatora, style Pro-L 2) + kilka gałek.
- **Eksport / przenośność**: render WAV (Kick 2), presety z samplami w środku.
- **Efekty wbudowane albo świetne, albo żadne** (zarzut do Kick 2).
