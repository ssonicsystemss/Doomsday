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

## Vital (Matt Tytel, darmowy) → syntezatory (PoS, Dark Serpent, Fortune)
Źródła: Synth Anatomy, KVR, RouteNote, Bedroom Producers Blog.
- **Budowa:** 3 oscylatory wavetable + slot sampla, każdy z **warpami częstotliwości i waveshaperami** (harmonic / inharmonic stretch,
  Shepard, spectral time, „screw”, FM, RM, sync) – Vital **wygina harmoniczne fali** (nie tylko przesuwa po tablicy jak Serum).
  Edytor wavetable, **tekst → wavetable**, preset → wavetable. 2 filtry (32 typy, drive), routing 4 sygnałów do 1 / 2 filtrów.
  3 obwiednie, 4 LFO do pasma audio, 2 randomizery (Perlin, Lorenz, S&H, sinus) w stereo. **Modulacja przeciągnij-upuść
  z animowanymi wskaźnikami na gałkach**, macierz. 8 efektów (multiband kompresor = OTT, delay, phaser, reverb…). MPE, mikrotonalność.
  Wersja darmowa ma wszystkie funkcje.
- **Dla nas:** warpy harmonicznych jako tryby oscylatora screecha (stretch / FM / sync), animowane wskaźniki modulacji na gałkach,
  randomizery Perlin / Lorenz jako źródła (żywe, „oddychające” screeche), multiband kompresor jako efekt wbudowany (mamy `Ott3`).

## ShaperBox 3 (Cableguys) → przyszła wtyczka sidechain / volume shaper (np. odświeżony Sidechain Prophecy)
Źródła: MusicTech (10/10), MusicRadar, Bonedo, Magnetic Mag.
- **Budowa:** „shapery” sterowane narysowaną krzywą: VOLUME, FILTER, PAN, WIDTH, TIME, DRIVE, CRUSH, NOISE, LIQUID (flanger / phaser).
  Każdy shaper **dzielony na 3 pasma**, envelope follower, **AUDIO TRIGGER** (sygnał z sidechaina przekracza próg → odpala krzywą;
  algorytmy Drums / General / Complex, filtr wejścia, czułość; może wysyłać MIDI), MIDI trigger. Edytor: punkt jednym kliknięciem,
  przełączanie krzywa / punkt, zaznaczanie bez zmiany narzędzia, randomize. Presety: tagi, ulubione, chmura.
- **Chwalą:** audio trigger („rewolucja”), płynność edycji. **Krytykują:** brak master volume, brak delay / reverb shapera, słaby randomize.
- **Dla nas:** `mod::Lfo` DRAWN + `mod::Shape` z bend już są → wtyczka „shaper” to głównie UI. Koniecznie: **audio trigger od kicka**
  (sidechain bez MIDI), podział na pasma (sidechain tylko dołu – bas pod kickiem), master volume (ich brak to częsty zarzut).

## soothe2 (oeksound) → Mirror of Souls (tryb RESONANCE) / Throne
Źródła: instrukcja soothe2 (oeksound.com), recenzje Whipped Cream Sounds, MusicRadar.
- **Jak działa:** dynamiczny tłumik rezonansów – analizuje widmo i na bieżąco wycina wystające szczyty. Gałki: **DEPTH** (ile – jak próg,
  nie dB; do −60 dB), **SHARPNESS** (jak wąskie wycięcia), **SELECTIVITY** (tylko najwyższe szczyty ↔ prawie wszystko), ATTACK / RELEASE,
  tryb SOFT (przezroczysty) / HARD (mocniejszy), krzywa czułości (EQ mówiący, gdzie działać mocniej / słabiej, pasma 6 typów + low / high cut),
  **DELTA** (słyszysz tylko to, co zabiera), MIX, TRIM, sidechain, oversampling, rozdzielczość (eco / high / ultra), osobna jakość dla renderu.
- **Dla nas:** w Mirror tryb **„RESONANCE”** (jak soothe) – szczególnie na piszczące screeche i hihaty; DELTA wszędzie, gdzie coś zabieramy;
  „osobna jakość dla renderu offline” jako zasada dla ciężkich algorytmów (HQ przy eksporcie, lżej przy graniu).

## OTT (Xfer, darmowy) → wszystkie (już mamy `Ott3`)
Źródła: MixElite, MusicRadar, Bedroom Producers Blog.
- **Budowa:** 3 pasma, kompresja w dół i **w górę (do +36 dB)**, gałki DEPTH (= mix, domyślnie 100 % – za dużo), TIME, IN / OUT GAIN,
  UPWARD / DOWNWARD globalnie, poziomy pasm H / M / L po kompresji.
- **Typowo:** wokal 7–20 %, leady 15–25 % (EQ wycina dół przed OTT), reverb / delay **po** OTT; na master zwykle nie (jeśli już: ~15 %, upward 0).
  **Pułapki:** wyciąga szum i ogony pogłosu, robi się ostro (ściszyć H), nie mieszać równolegle (fazowe efekty pasm) – używać DEPTH.
- **Dla nas:** nasz `Ott3` – dodać osobno UPWARD / DOWNWARD i poziomy pasm; domyślne DEPTH niskie (~20 %), nie 100 %;
  w łańcuchach: reverb / delay po OTT.

## Ozone 12 (iZotope) → Throne of Titans
Źródła: Attack Magazine, Gearnews.
- **Nowości:** **Master Assistant** (AI: wybierasz moduły, cel głośności, gatunek → ustawia łańcuch), **Stem EQ** (EQ osobno na wokal / bębny /
  bas / resztę w gotowym miksie – separacja AI), **Bass Control** (wyrównanie dołu), **Unlimiter** (przywraca dynamikę przeciśniętym miksom),
  Maximizer **IRC 5** (głośno bez pompowania i przesteru).
- **Dla nas (Throne):** nasz LEARN to mały „asystent” – rozbudować: **gatunek + cel → ustawia pasma, clip i limit** (np. HARDSTYLE / RAW / UPTEMPO),
  „BASS CONTROL” prostą wersją: stały poziom dołu względem reszty (mamy pasmo LOW i MONO BASS); separacja stemów – za trudne, pomijamy.

## FabFilter Saturn 2 → przestery we wszystkich (Forge STAGE 1 / 2, Dark Serpent, PoS), może osobna wtyczka saturacji
Źródło: fabfilter.com (strona produktu), KVR, Plugin Boutique.
- **Budowa:** 28 stylów (od lampy przez wzmacniacze gitarowe po bitcrush) + 5 kreatywnych; **do 6 pasm** (zbocza 6 / 12 / 24 / 48 dB,
  opcjonalnie linear phase); na pasmo: DRIVE, MIX, **FEEDBACK**, **DYNAMICS** (przester zależny od głośności – zachowuje transjenty
  albo je podkręca), TONE, LEVEL, solo / mute. Modulacja: 16-krokowe XLFO, obwiednie, envelope follower z wykrywaniem transjentów,
  MIDI, XY, macierz 50 slotów (przeciągnij-upuść), widać modulację na żywo. Jakość: Good (8x) / Superb (32x).
- **Dla nas:** przester **wielopasmowy** jako standard (mamy `Crossover3` + `drive::Adaa1`), gałka **DYNAMICS** (przester słabszy
  na transjentach = punch zostaje, albo odwrotnie), FEEDBACK, oversampling do wyboru. W Forge: STAGE 1 / 2 z podziałem na pasma.

## iZotope Trash 2 → przestery, Forge / Dark Serpent
Źródła: MusicRadar, MusicTech, Ask.Audio.
- **Budowa:** 6 modułów w dowolnej kolejności (Graph): TRASH (2 stopnie przesteru, do 4 pasm, 69 algorytmów w 7 kategoriach:
  Distort, Drive, Faulty, Fuzz, Heavy, Retro, Saturate; część ze suwakiem STYLE), 2 filtry 6-pasmowe (20+ typów, w tym **samogłoski**),
  CONVOLVE (impulsy kolumn / mikrofonów + własne IR), DYNAMICS (upward / gate, węzły na wykresie), DELAY (6 typów, „trash” degraduje echa).
  **Rysowany waveshaper**: krzywa bazowa (czerwona) + Twoja poprawka (żółta) = wynik (niebieska).
- **Chwalą:** wszechstronność, brzmienie, modulacja, waveshaper. **Krytykują:** brak routingu L / R, mało presetów filtrów.
- **Dla nas:** **rysowany waveshaper** (mamy `mod::Shape` → krzywa przesteru), **filtr samogłoskowy** (mamy `Formant` w PoS),
  CONVOLVE z własnym IR (kolumny / „pudło” – ciekawe na screeche), kolejność modułów przeciąganiem.

## Gross Beat (FL Studio) → przyszła wtyczka „czasowa” / efekty build-upów
Źródło: instrukcja FL Studio (Image-Line).
- **Jak działa:** bufor 2 taktów, **36 obwiedni CZASU** (przesunięcie odtwarzania: pozioma linia = normalnie, nachylona = szybciej /
  wolniej / wstecz → stutter, tape stop, scratch, half-speed) i **36 obwiedni GŁOŚNOŚCI** (gate / trance gate / sidechain).
  Sloty przełączane z klawiatury MIDI na żywo (głośność od C1/C2, czas od C4/C5), Trig Sync (start od 1/4 do 4 bitów), Pos Sync,
  Key-held / One-shot, HOLD (powtarzanie taktu), skreczowanie myszą, wygładzanie ATT / REL / TENSION, redukcja trzasków przy skokach,
  HQ resampling (sinc).
- **Dla nas:** pomysł na wtyczkę **„build-up / FX”**: tape stop, stutter, half-speed, reverse, gate – sloty grane z klawiatury, sync z taktem.
  Klocki: `mod::Shape` (obwiednie), `mod::syncBeats` (sync), bufor + interpolacja. Ważne: wygładzanie skoków (bez trzasków).

## Maximus (FL Studio) → Throne of Titans (to jego „pierwowzór”)
Źródło: instrukcja FL Studio (Image-Line) – Maximus Controls.
- **Budowa:** high-pass na wejściu → 3 pasma LOW / MID / HIGH + MASTER; **kompresja rysowaną krzywą (brak gałki threshold –
  próg i ratio to punkty na krzywej)**, typy krzywych (single, double, hold, stairs…), narzędzia (flip, normalize, smooth, „analyze audio”),
  PRE / POST, saturacja (THRES: typ A w lewo / B w prawo, CEIL – twardość), ATT, **REL i REL2** (przyspieszający i zwalniający release,
  da się mieszać), SUSTAIN (RMS), PEAK / RMS, zbocza 12 / 24 dB, linear phase, LMH MIX (równoległa), look-ahead, separacja stereo,
  **Master Mid** (pasmo MID steruje kompresją całości – wewnętrzny sidechain / de-esser), spektrogram / heatmapa w tle, oversampling.
- **Dla nas (Throne):** **rysowana krzywa kompresji** zamiast THRESH / RATIO / KNEE (mamy `mod::Shape` + krzywą transferu na ekranie),
  dwa release'y, PEAK / RMS, wewnętrzny sidechain pasma (np. wokal / lead ścisza dół), heatmapa spektrum. Producent FL zna Maximusa –
  Throne powinien dać się obsłużyć „po maximusowemu”, ale prościej (LEARN, podpowiedzi).

## Kickstart 2 (Nicky Romero / Cableguys) → wtyczka sidechain (Sidechain Prophecy v2)
Źródła: MusicTech (recenzja), MusicRadar, instrukcja Kickstart 2 (Cableguys).
- **Budowa:** 16 gotowych krzywych (10 ducking, 6 ucinanie ogona), **jedna duża gałka MIX**, sync z DAW, x2 / ½ prędkości, przesuwanie
  krzywej w przód / tył (nudge), MIDI i **audio trigger** (sidechain od kicka), podgląd: fala kicka nałożona na fale basu,
  przesuwane zbocze, **tryb multiband** (ducking tylko dołu).
- **Chwalą:** bajecznie prosty i szybki, audio trigger, cena (14 €). **Krytykują:** nie da się rysować krzywych, brak osobnej siły na pasmo.
- **Dla nas:** idealny wzór **prostego trybu**: gotowe krzywe + jedna gałka + audio trigger + „tylko dół”; a nasza przewaga: **rysowanie
  krzywej** (czego Kickstartowi brakuje) i osobna siła na pasmo.

---
## Wspólne wzorce (wszystkie analizy)
- **Słuchanie różnicy**: delta / audition / unity gain (Pro-L 2, nasze MATCH) – producent od razu słyszy, co robi efekt.
- **Rysowanie zamiast kręcenia**: EQ Sketch, krzywe Béziera w Kick 2, nasze obwiednie w Forge.
- **Tryb jako główny wybór charakteru** (Valhalla MODE, Spire tryby oscylatora, style Pro-L 2) + kilka gałek.
- **Eksport / przenośność**: render WAV (Kick 2), presety z samplami w środku.
- **Efekty wbudowane albo świetne, albo żadne** (zarzut do Kick 2).
- **Krzywa / próg zamiast liczb**: soothe (krzywa czułości), ShaperBox (rysowana krzywa), OTT (DEPTH jako jedna gałka).
- **Domyślne ustawienia ostrożne** (OTT na 100 % to typowy błąd) – nasze nowe efekty startują łagodnie.
- **Osobna jakość na eksport** (soothe): w czasie grania lżej, przy renderze HQ.
- **Wielopasmowość jako standard** (Saturn, Trash, Maximus, ShaperBox, Kickstart): przester / ducking / kompresja osobno na pasma.
- **Przester zależny od dynamiki** (Saturn DYNAMICS): punch zostaje, ogon się brudzi.
- **Dwa tryby obsługi**: prosty (gotowe krzywe + jedna gałka – Kickstart) i pełny (rysowanie – ShaperBox / Maximus).

---
## Lista pomysłów do zrobienia (z analiz, wg wtyczki)
- **Forge of Wrath:** EXPORT WAV w tonacji; oś PITCH w nutach + przyciąganie; podgląd warstw osobno + zoom; 2–3 warstwy sampla;
  preset zapisuje swój sample; presety dopasowane do 38 kicków referencyjnych na silniku v2.
- **Mirror of Souls:** dynamic z auto-progiem; tryb RESONANCE (soothe); EQ MATCH; rysowanie krzywej; kanały L / R; płynne nachylenie; DELTA.
- **Throne of Titans:** AUDITION / DELTA; style limitera; wykres redukcji w czasie; asystent gatunek + cel; OTT-owe UPWARD / DOWNWARD osobno.
- **Abyss of Echoes:** tryby reverbu o różnym charakterze; ATTACK; losowa modulacja bez falowania wysokości; kolor epoki.
- **Syntezatory (PoS / Dark Serpent / Fortune):** filtr `Ladder` jako tryb ANALOG; MIX supersawa (`sides`); tryby oscylatora z CTRL A / B
  (FM, HardFM, sync, warp harmonicznych); unison z akordami; 2 filtry szereg / równolegle; randomizery Perlin / Lorenz; key & scale; arp ze step seq.
- **Nowa wtyczka – SHAPER / sidechain (Sidechain Prophecy v2):** tryb prosty jak Kickstart (gotowe krzywe + MIX), tryb pełny jak ShaperBox
  (rysowanie, mamy `mod::Shape`), sync z tempem (mamy), audio trigger od kicka, „tylko dół” / 3 pasma z osobną siłą, master volume.
- **Nowa wtyczka – BUILD-UP / FX (jak Gross Beat):** tape stop, stutter, half-speed, reverse, trance gate; sloty z klawiatury MIDI, sync z taktem.
- **Przestery wszędzie:** wielopasmowość, DYNAMICS (Saturn), rysowany waveshaper (Trash), filtr samogłoskowy, FEEDBACK, HQ.
- **Throne:** rysowana krzywa kompresji „po maximusowemu”, REL + REL2, PEAK / RMS, wewnętrzny sidechain pasma, heatmapa spektrum.
- **Wszystkie:** przeglądarka presetów i UNDO (zrobione), przełącznik HQ, modulacja przeciągnij-na-gałkę z animowanym wskaźnikiem, makra.
