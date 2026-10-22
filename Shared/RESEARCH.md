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

## u-he Diva → syntezatory (brzmienie analogowe)
Źródła: MusicRadar / Computer Music, Sound On Sound, KVR.
- **Budowa:** wybierane **modele** oscylatorów (Triple VCO – Minimoog, DCO – Alpha Juno z sub-oscylatorem, Dual VCO z sync / cross-mod)
  i **filtrów** (ladder Mooga, czysta kaskada, multimode, agresywny „Bite”) liczone symulacją obwodów (zero-delay feedback);
  3 typy obwiedni (ADS Mooga, analogowy ADSR, cyfrowy „schodkowy”), **TRIMMERY**: rozstrojenie i rozrzut cutoffu / obwiedni między głosami
  („slop” – każdy głos trochę inny jak w analogu). **Tryby dokładności** (Draft … Divine) – Divine bardzo ciężki dla CPU.
  Przeglądarka: ulubione / „śmieci”.
- **Chwalą:** brzmienie jak prawdziwy Minimoog, prosty interfejs w zakładkach. **Krytykują:** CPU.
- **Dla nas:** **„slop” / rozrzut między głosami** (cutoff, obwiednie, strój – tanie, a daje analogowe życie), modele filtrów jako
  tryby (`filter::Ladder` = „ladder”, SVF = „cascade / clean”), tryby dokładności = nasz przełącznik HQ (lżej grając, HQ przy renderze).

## Soundtoys Decapitator → saturacja (Forge / Dark Serpent / osobna wtyczka)
Źródło: instrukcja Decapitator (Soundtoys).
- **Budowa:** 5 **stylów** (A – preamp taśmy Ampex 350, gładki; E – Chandler / EMI TG, gruby dół i powietrze; N – Neve 1057, skupiony środek;
  T – trioda Culture Vulture, parzyste harmoniczne, ciepło; P – pentoda, nieparzyste, agresywnie), DRIVE, **AUTO (auto-gain: ściszanie
  przy podkręcaniu)**, **PUNISH (+20 dB na ostro)**, LOW CUT przed przesterem, TONE (tilt przed), HIGH CUT po + **STEEP** (6 → 30 dB/okt,
  przy ~4–5 kHz brzmi jak kolumna gitarowa), **THUMP** (garb basowy przy low cut jak „head bump” taśmy), MIX (równolegle), OUTPUT.
- **Dla nas:** **AUTO-GAIN** przy każdej gałce drive (koniec z „głośniej = lepiej”), przycisk **PUNISH**, low cut + THUMP przed przesterem,
  STEEP high cut (szybka „kolumna”), style nazwane charakterem (ciepło / parzyste vs agresja / nieparzyste) zamiast samych nazw krzywych.

## FabFilter Pro-C 2 → Throne (pasma) / przyszły kompresor
Źródło: MusicTech (recenzja).
- **Budowa:** 8 **stylów** (Clean, Classic, Opto, Vocal – auto knee / ratio, **Mastering**, Bus, **Punch**, **Pumping** – pod sidechain EDM),
  własny knee, **HOLD** (do 500 ms), lookahead, auto-gain, **sidechain EQ** (HP / LP + środkowe pasmo), wykres: **przewijana fala z krzywą
  redukcji** (widać attack / release) + wykres knee / ratio.
- **Dla nas:** styl „PUMPING” w Throne / sidechain, HOLD w kompresorach pasm, **przewijana fala z redukcją** (ten sam pomysł co w Pro-L 2 –
  pora go zrobić jako klocek UI w `Shared/`).

## Sytrus (FL) → screeche FM (PoS / Dark Serpent)
Źródło: Sound On Sound (recenzja), samouczek Sytrus (Image-Line).
- **Budowa:** **6 operatorów**, każdy jak mały syntezator (obwiednia, LFO, pan, pitch, faza), **macierz FM / RM** (każdy moduluje każdego,
  także siebie), kształt fali przez harmoniczne + suwaki Shape / Tension / Skew / Sine Shaper, szum, 3 filtry (12 / 24 / 36 dB, łańcuchowo),
  obwiednie wielopunktowe rysowane, chorus + 3 delaye. Brzmi jasno i cyfrowo.
- **Dla nas:** macierz FM 3–4 operatorów jako tryb oscylatora screecha (FM / RM z rysowanymi obwiedniami operatorów = klasyczny
  „FL-owy” screech), „Sine Shaper” jako prosty waveshaper sinusa.

## NI Massive (2007) i Massive X → syntezatory screechy / leadów (PoS, Dark Serpent, Fortune)
Źródła: instrukcja Massive (NI – Oscillator Section), Sound On Sound (Massive), MusicRadar (Massive X), Wikipedia.
- **Massive – oscylatory:** 3 wavetable (80+ tablic), **POSITION** (przesuwanie po tablicy) + gałka **INTENSITY, której sens zależy od trybu**:
  SPECTRUM (mniej / więcej harmonicznych – wbudowany „filtr”), BEND (zmienia prędkość odczytu fali – PWM / FM-owy charakter),
  **FORMANT (przesuwa formanty bez zmiany wysokości = samogłoski)**. Jeden sinusowy **oscylator modulujący** z trybami RING (metal),
  **PHASE (FM – klasyczny bas / screech)**, POSITION (szorstka faktura), **FILTER FM** (moduluje cutoff filtra z częstotliwością audio –
  warkot, „growl”). Szum z tablic sampli (COLOR).
- **Massive – filtry i routing:** 2 filtry (LP / HP 2- i 4-polowe, BP, notch, comb, **SCREAM**, **DAFT**), każdy oscylator ma suwak wysyłki
  F1 ↔ F2, **suwak szeregowo ↔ równolegle** (modulowany = routing „oddycha”), **pętla FEEDBACK** z powrotem do filtrów (od ciepła po
  mocny przester – filtry reagują na poziom jak analog, „headroom”), 2 sloty INSERT wstawiane w dowolne miejsce (przed / między / za
  filtrami: bitcrusher, sine / parabolic shaper, S&H, frequency shifter), BYPASS (oscylator / sub omija filtry), 2 master FX + EQ.
- **Massive – modulacja:** 4 obwiednie z pętlą i morfingiem kształtu, 4 LFO (dwa kształty z płynnym przejściem) zamienialne na
  **PERFORMER** (16 kroków, każdy krok to krzywa, morfing między dwiema sekwencjami) albo **STEPPER** (16 kroków na głos);
  8 makr, **Random Track** (losowa wartość na każde naciśnięcie klawisza), przeciągnij-upuść na gałki, źródło może skalować inną modulację.
  Unison rozrzuca pitch, panoramę i pozycję w tablicy.
- **Massive X:** 2 wavetable z **10 trybami** (m.in. **GORILLA** – ekstremalne formanty, ART – „filtrowanie”), 2 oscylatory PM, 2 szumy,
  **unison ↔ akord płynnie (6 głosów)**, routing jak moduł z kablami (pętle feedbacku!), 9 typów filtrów, Performery do 8 taktów
  i 12 sekwencji, EXCITER (jednorazowe obwiednie), Trackery, randomizer głosów, 16 makr.
- **Chwalą:** brzmienie z „zapasem” (gęstnieje, nie rozpada się), skanowanie tablic, routing / feedback / morfing, czytelna modulacja.
  **Krytykują:** CPU (tryb Eco gorzej brzmi), brak arpeggiatora i kompresora, zaśmiecone presety; w X: brak importu własnych fal,
  słaby podgląd modulacji (statyczne grafiki), mało slotów FX.
- **Dla nas:**
  1. **Gałka INTENSITY z trybami SPECTRUM / BEND / FORMANT** na oscylator – trzy charaktery jedną gałką (jak Spire CTRL A / B).
     FORMANT (samogłoski bez zmiany wysokości) to wprost „mówiący” screech.
  2. **Jeden oscylator modulujący z trybami RING / PHASE (FM) / FILTER FM** – tani, a daje cały arsenał screechy i growli.
  3. Filtry: tryb **SCREAM** (mamy SCREAM w Forge / Fortune), suwak **szeregowo ↔ równolegle** modulowany, **pętla FEEDBACK** przez filtr
     (nasz `filter::Ladder` z nasyceniem znosi to dobrze), BYPASS dla dołu.
  4. **PERFORMER**: 16 kroków z krzywą na krok + morfing dwóch sekwencji – rytmiczne ruchy screecha zsynchronizowane z tempem
     (budujemy na `mod::Shape` + `mod::syncBeats`). **Random Track** na każdą nutę (żywe screeche).
  5. **Unison ↔ akord płynnie** (Massive X), INSERT w dowolnym miejscu łańcucha.
  6. Nie powtarzać: ciężkiego CPU, statycznych grafik modulacji, zaśmieconych presetów.

## Technika: jak robi się hardstyle'owy kick (Native Instruments – „hardstyle music”)
Źródło: Native Instruments blog (krok po kroku, 150–160 BPM).
1. **EQ PRZED przesterem: wytnij dół, podbij środek** (−13 dB @ 50 Hz, −11,5 dB @ 170 Hz, +12 dB @ 383 Hz) → przester (Dirt).
2. Drugi przester (Bite), potem **EQ przed kolejnym przesterem** (+4,6 dB @ 50 Hz, +5,8 dB @ ~1 kHz szeroko, +10,5 dB @ 1,86 kHz).
3. **Resampling** do audio → sampler z obwiednią (ogon), na ogon **bitcrusher** (≈10 kHz, 6 bit, crunch) – wysoki „artefakt” na końcu kicka.
4. **Transient shaper w pasmach** (podział 200 Hz / 6 kHz: mniej sustainu w środku, więcej w górze), końcowy EQ (−8 dB @ 861 Hz).
5. **Warstwa PUNCH** (909) strojona do ogona, z własnym przesterem; **ogon ściszany sidechainem od punchu** (pompowanie).
6. Screech i lead dostają tę samą rytmikę co kick, strojone do tonacji; wszystko inne sidechain od kicka.
- **Dla Forge (konkretnie):** **PRE EQ przed STAGE 1** (cięcie dołu + podbicie ~300–400 Hz – dziś mamy tylko TONE / tilt) – z CLEAN SUB dół
  wraca czysty; **BITCRUSH na ogonie** (rate + bity); **transient shaper w pasmach**; **ducking ogona od punchu** (sidechain wewnątrz kicka);
  **EXPORT WAV** (resampling to standard pracy).

## dkd (Damian / sonicsystems, `ssonicsystemss/dkd`, v1.0.0, 2026-10-22) → Forge of Wrath, Ancient Drums, nowa wtyczka kick + reverse bass
Analiza kodu (silnik `Source/RutherfordEngine.h`, procesor, `SONICSYSTEMS_shared/OwnSound.h`, `FISSION_shared/Fission.*`, test `Tools/RenderDemo.cpp`).
Co robi: **jedna trzymana nuta = kick na każdą ćwierćnutę + reverse bass między kickami** (kroki 1/4, 1/8, 1/16), zablokowane z tempem FL.
- **Silnik jest prosty** (~370 linii): kick = sinus z wykładniczym zjazdem wysokości (start → nuta ogona, `pow(1-x, 3)`), ogon liczony jako
  **ułamek beatu** (TAIL 0.1–0.98 → długość idzie za tempem); bas = piła (PolyBLEP) + sinus, amplituda `x^CURVE` od IN do OUT kroku,
  w ostatnich 40 % podjazd wysokości (odwrócony ogon kicka), krótkie wejście / wyjście 1 / 1.5 ms. Oba → 4x oversampling → przester
  (kick: tanh / hard / foldback, bas: tanh) → 2× SVF LP 24 dB → suma + CLICK (szum przez HP 1.5 kHz, 5 ms) → DC → gain → clipper.
  Nuty składane do E1–D#2 (każde F = F1 43.65 Hz) – kick zawsze w tym samym rejestrze.
- **Mocne strony to nie DSP, tylko solidność i obsługa:**
  1. **Zegar beatu**: z hostem, gdy gra; gdy stoi – własny zegar od pierwszego klawisza; **skok (pętla, przewinięcie, start) nie gubi
     kicka, na który trafia**; klawisz wciśnięty dokładnie na beat od razu gra.
  2. **Oversampler tworzony raz (konstruktor), w prepare tylko reset** – w FL „Replace” / ponowny prepare usuwał obiekt pod wątkiem audio
     (crash). Do tego flaga `prepared` – processBlock bez prepare gra ciszę.
  3. **Latencja zgłoszona hostowi** (`setLatencySamples`), a CLICK, który omija oversampler, opóźniony o tę samą latencję (równo z kickiem).
  4. **Parametry bez alokacji** (ParamCache – APVTS po nazwie buduje `juce::String` przy każdym odczycie).
  5. **Stan z NaN / inf odrzucany** przy wczytaniu projektu.
  6. **OWN SOUND**: własny sample (WAV / AIFF / FLAC / MP3 / OGG, ≤ 20 s) **zapisany W stanie wtyczki** (FLAC 24-bit, base64) – projekt i preset
     mają dźwięk nawet po skasowaniu pliku. LAYER / REPLACE, stała wysokość albo za nutą, odtwarzanie wstecz z „fit” (koniec dokładnie
     na kicku), start przesunięty o latencję.
  7. **Prawy klik na gałce**: menu hosta dla parametru (Cubase / Reaper / Bitwig: automation lane), a gdy host nie daje (FL) – „Create
     automation clip” = parametr staje się „last tweaked” + podpowiedź gdzie w FL; **FREEZE** (RANDOM go nie rusza, zapisane w stanie); reset.
  8. **RANDOM jako animacja**: gałki jadą 0.5 s (z przesunięciem), każda w jednym geście hosta → FL może to nagrać, użytkownik WIDZI zmianę;
     20 kroków UNDO, „mignięcie” ekranu. Własne zakresy RANDOM per parametr (lo / hi / lista / skip).
  9. **Test renderu sprawdza muzykę, nie tylko NaN**: kick na KAŻDY beat (14/14), cisza przed każdym kickiem, wysokość basu (F → 43.65 Hz),
     tryb jednostrzałowy, zegar przy zatrzymanym transporcie, cisza po note-off, ID presetów, stan, okno 75 %, **głośność taka sama przy
     44.1 / 48 / 96 kHz** (±0.5 dB) – z udawanym hostem (`FakeHost : AudioPlayHead`).
  10. Budowa: VST3 + **CLAP** (clap-juce-extensions), statyczny runtime MSVC.
- **Słabsze / inne niż u nas:** jeden prosty przester na kick (Forge ma łańcuch STAGE 1 → EQ → STAGE 2, ROOM, OTT, rysowany pitch);
  brak warstwy PUNCH; bas bez filtra z obwiednią; GUI w innym stylu (FISSION: czarny, „komora mgłowa”).
- **Wzięte do `Shared/`** (z podpisanym źródłem, z testami): `DoomsdayHost.h` (ParamCache, BeatClock, sanitizeState),
  `DoomsdayParamMenu.h` (menu prawego kliku + FREEZE + RandomMover), `DoomsdayOwnSound.h` (OWN SOUND), `DoomsdayReverseBass.h` (reverse bass).

---
## Architektura hardstyle kicka (dokument właściciela, 2026-10-23) → Forge of Wrath
Dokument: 4 sekcje (PUNCH / TOK, TAIL, CRUNCH / TEXTURE, SUB / BODY) → suma → bus distortion / clipper → filtr → DC / output / limiter.
Wniosek główny: **szkielet Forge już jest zgodny z tym modelem** – nie przepisujemy silnika, dobudowujemy klocki; każdy nowy parametr
ma nowe ID i wartość neutralną, więc presety właściciela (QL / Nice / Sgsdg / Volcanic / Undertow) brzmią identycznie.

| Sekcja dokumentu | Forge dziś | Luka |
|---|---|---|
| Oscylatory, faza startowa | tail: 1 osc z WAVE (sine → tri → saw); punch: sinus; faza zawsze od zera | OSC B, pulse / PWM, szum różowy, PHASE |
| Pitch env (tok / tail / strojenie) | PUNCH Hz START → END + DROP (nie idzie za nutą); TAIL START × nuta, DROP, CURVE, HOLD, DRAWN; FIXED HEAD; TUNE + FINE | rysowany pitch toka, START taila w półtonach / FIXED Hz, PUNCH AMOUNT |
| Amp env, one-shot | LENGTH + rysowany AMP; punch DECAY / TOK; note-off nic nie ucina | ATTACK taila / puncha, rysowany AMP toka |
| Punch: osc → env → pre-filter → dist → miks | osc, env, DRIVE / INTO DIST, LEVEL | **pre-filter**, typ przesteru, **typy puncha** (Tonal / Hard / Click / Reso) |
| Tail: A + B → pitch → env → pre → dist A → post A → dist B → post B | TONE (pre, tilt 400 Hz) → STAGE 1 → MID EQ → STAGE 2 → LOW / HIGH CUT → SCREAM / CRUNCH / GRIT | OSC B, mocniejszy pre-filter ze sweepem |
| Texture jako osobny generator | GRIT / CHAOS / CRUNCH = obróbka taila | warstwa TEXTURE (źródło → pitch → BP / reso → drive → env → HP) |
| Stopień przesteru: drive, algorytm, bias, mix, gain, pre / post tone | drive, 5 / 6 algorytmów (SOFT, TUBE = asymetria, HARD, FOLD, RAW) | **BIAS, MIX, OUTPUT GAIN / auto-gain** (tanh(gx)/tanh(g) + korekta RMS, jako przełącznik) |
| Bus distortion | CLIPPER na sumie, PUNCH INTO DIST | DIST BUS (CLEAN SUB go omija), filtr końcowy |
| Oversampling | cały głos 4×, IIR, tworzony raz | QUALITY 2 / 4 / 8× (3 gotowe obiekty) + 8× przy renderze |
| Filtry LP / HP / BP / notch + reso + drive | LOW / HIGH CUT, SCREAM (reso) | wspólny moduł FILTER (raz napisany, w punch / tail / texture) |
| Reverb, sub protection | ROOM w tailu (przed / po przesterze); CLEAN SUB (LR4) | ROOM LOW CUT, ROOM w punchu, SUB LEVEL |
| Stereo | całość mono | WIDTH tylko dla texture / room, < 150–200 Hz zawsze mono, MONO CHECK |
| Silnik: reset, brak alokacji, SR, szczyty | wszystko ✅ | wygładzanie gałek ciągłych (najpierw test, czy słychać schodki) |
| Interfejs | edytory krzywych, podgląd fali, ms w BassLane | EXPORT WAV, A/B, widmo, SOLO / MUTE warstw |
| Testy | head identity, pitch, szczyty presetów | 2 rendery bit w bit, 44.1 / 48 / 96, sam punch / sam tail, drive monotoniczny, aliasing |
| Ponad dokument (smaczki właściciela) | REVERSE BASS z odwróconego taila (PEAK, PUMP, SUB), OTT, SAMPLE layer | umieścić jako 5. sekcję w MIX BUS |

**Plan kroków (propozycja – kolejność do potwierdzenia po pomiarach ~35 referencji i uchem właściciela):**
- **A. Małe gałki, zero ryzyka:** SUB LEVEL, PUNCH AMOUNT, BIAS / MIX / GAIN na STAGE 1 i 2 (+ DC block, auto-gain z `Shared/`),
  ATTACK taila / puncha, ROOM LOW CUT, PHASE taila / puncha, SOLO / MUTE warstw (nie zapisywane).
- **B. Punch:** TYP PUNCHA Tonal / Hard / Click / Reso + PRE-FILTER + typ przesteru; rysowany PITCH / AMP toka (jedno okno, zakładki);
  opcjonalnie CLICK przez łańcuch puncha; potem 2 punche naraz (tick + tok).
- **C. Tail:** OSC B (LEVEL A i LEVEL B osobno), PRE-FILTER ze sweepem (rozbudowa TONE), START taila w półtonach i tryb FIXED Hz.
- **D. Texture:** osobna warstwa (pulse / saw / szum biały / różowy, mały pitch drop, BP / reso, drive, env, HP); PHASE RANDOM (domyślnie off);
  PM (niski priorytet). Stare GRIT / CHAOS / CRUNCH zostają.
- **E. Bus:** przejście tok → tail (ducking taila od toka), crossover ~180 Hz między warstwami, DIST BUS, MIX BUS (glue → clipper z obejściem
  czystego subu → filtr → out), reverse bass jako 5. sekcja, WIDTH dla górnych warstw.
- **F. Jakość i wygoda:** QUALITY + 8× przy eksporcie, wygładzanie gałek, EXPORT WAV, A/B, rozszerzone testy, przełącznik kolejności
  tekstur (przed / po STAGE 2); notatka: BITCRUSH jako typ przesteru (jest w `Shared/`).
Z `Shared/` do wzięcia: ADAA, auto-gain, `Ladder`, oscylator modulujący (PM), bitcrusher, Oversampling.

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
- **Auto-gain przy drive** (Decapitator, Pro-C 2, OTT out gain): słuchamy charakteru, nie głośności.
- **Przewijana fala z krzywą redukcji** (Pro-L 2, Pro-C 2): najczytelniejszy podgląd dynamiki – do zrobienia jako klocek UI.
- **Rozrzut między głosami** (Diva „slop”): tani sposób na analogowe życie.
- **Sample w środku presetu / projektu** (dkd OWN SOUND, Kick 2): przenośność bez „brak pliku”.
- **RANDOM widać i da się nagrać** (dkd): animowane gałki w geście hosta, FREEZE wybranych, UNDO.

---
## Klocki już zbudowane w `Shared/` (gotowe do wpięcia, z testami)
Przestery: ADAA, oversampling, bitcrusher, rysowany waveshaper, DYNAMICS (DynamicDrive), transient shaper, auto-gain, filtr `Ladder`.
Modulacja: Shape z bend, LFO (sync, DRAWN, CHAOS), Performer, random-per-note, macierz, key & scale, arp ze step seq.
Oscylatory: INTENSITY (SPECTRUM / BEND / FORMANT), oscylator modulujący (ring / PM / filter FM), FM operatory, unison ↔ akord, slop, MIX supersawa.
Sidechain / czas: audio trigger, volume shaper z pasmami i 12 krzywymi, tape stop, stutter, reverser.
Widmo: tłumik rezonansów (soothe) z DELTA. UI: przeglądarka presetów + UNDO, GainScope, pierścień modulacji na gałce.
Host (z dkd): ParamCache, BeatClock (skoki pętli), sanitizeState, menu prawego kliku (automation / FREEZE), animowany RANDOM, OWN SOUND
(sample w stanie), reverse bass zsynchronizowany z tempem.
Do zrobienia przy wpinaniu: interfejsy tych funkcji w konkretnych wtyczkach (lista niżej).

## Lista pomysłów do zrobienia (z analiz, wg wtyczki)
- **Z dkd (wszystkie wtyczki):** oversampler tworzony RAZ (nie w prepare) + flaga `prepared` (crash przy FL Replace); `setLatencySamples`
  przy oversamplingu; `sanitizeState` w setStateInformation; prawy klik na gałce = automation clip / FREEZE; RANDOM animowany z FREEZE;
  testy z udawanym hostem (gra transport) i test głośności 44.1 / 48 / 96 kHz; CLAP obok VST3.
- **Forge of Wrath / Ancient Drums (z dkd):** sample warstwy **w stanie** (OwnSound zamiast ścieżki do pliku); tryb **kick + reverse bass
  z jednej nuty** (BeatClock + ReverseBass) albo osobna mała wtyczka; ogon kicka w ułamku beatu (idzie za tempem).
- **Forge of Wrath – RANDOM w kierunkach (pomysł właściciela, 2026-10-22, na później):** jak w 5CR34M 4LPH4 – RANDOM najpierw losuje
  kierunek, potem brzmienie w jego zakresach, zamiast losować wszystko na ślepo. Kierunki z jego ulubionych presetów: „QL” (CLEAN SUB
  ~209 Hz, FIXED HEAD ~65 Hz, reverse bass z wczesnym szczytem, DRIVE basu ~29 dB), „Nice” (reverse obwiednią AMP w kicku, SCREAM wysoko),
  „Sgsdg / Volcanic / Undertow” (raw), + mały procent „dziko”; głośność wyrównana jak w 4LPH4.
- **Forge of Wrath:** EXPORT WAV w tonacji; **PRE EQ przed STAGE 1** (cięcie dołu + podbicie 300–400 Hz); **BITCRUSH ogona**;
  transient shaper w pasmach; ducking ogona od punchu; DYNAMICS przesteru (Saturn); oś PITCH w nutach + przyciąganie; podgląd warstw
  osobno + zoom; 2–3 warstwy sampla; preset zapisuje swój sample; presety dopasowane do 38 kicków referencyjnych na silniku v2.
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
- **Wszystkie:** przeglądarka presetów i UNDO (zrobione), przełącznik HQ, modulacja przeciągnij-na-gałkę z animowanym wskaźnikiem, makra,
  AUTO-GAIN przy drive, klocek UI „przewijana fala + redukcja”.
- **Syntezatory:** „slop” między głosami (Diva), tryb FM 3–4 operatorów (Sytrus), style saturacji nazwane charakterem (Decapitator);
  z Massive: INTENSITY z trybami SPECTRUM / BEND / FORMANT, oscylator modulujący RING / PHASE / FILTER FM, filtry szeregowo ↔ równolegle
  (modulowane), pętla FEEDBACK przez filtr, PERFORMER (16 kroków z krzywymi, morfing sekwencji), Random Track, unison ↔ akord.
