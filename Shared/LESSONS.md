# Wnioski – co brzmiało źle i dlaczego (czytaj przed budową nowej wtyczki)

Wiedza, której nie widać w samym kodzie. Każdy wpis: **problem → przyczyna → zasada na przyszłość**.
Dopisuj po każdej uwadze właściciela („brzmi źle”, „słabo działa”, „mało intuicyjne”).

## Brzmienie / DSP
- **Throne: „napierdala przesterem”** → `dyn::saturate` miał wzmocnienie k/tanh(k) dla cichych sygnałów (+3..5 dB),
  więc SAT podbijał poziom i pchał limiter; limiter z lookahead 1,5 ms i szybkim release ruszał gainem w obrębie jednego
  okresu basu. → Każdy przester / saturacja: **wzmocnienie 1:1 dla cichego sygnału** (test: −30 dBFS przed = po).
  Limiter na sumie: lookahead ≥ 3 ms + **hold ≥ jeden okres basu (~20 ms)**; test THD sinusa 55 Hz pod limiterem.
- **Throne: presety za głośne** → presety z LIMIT +8..12 dB zakładały cichy miks; producenci wrzucają już głośne mastery.
  → Presety mastering: zachowawczy LIMIT (2–7 dB), głośność ustawia użytkownik (przycisk LEARN pod cel LUFS).
- **Mirror: „słabo wycina”** → zakres GAIN ±24, a ekran tylko ±18 (myszką max −18 dB, szeroki bell). → Cięcia głębsze niż
  podbicia (−48 / +24, 0 dB na środku gałki), NOTCH = prawdziwy notch, cięcia 96 dB/okt. Skala ekranu musi sięgać zakresu.
- **Forge: kicki „jak millennium hardcore”** → pojedynczy przester na czystej fali daje równy, powtarzalny kwadrat z opadającym
  pitchem (gabber). Nowoczesne kicki mają: **gęsty, nieregularny atak** (szum / pogłos wpuszczony W przester – CHAOS, ROOM INTO DIST),
  **dwa stopnie przesteru z EQ pomiędzy** (STAGE 1 → MID EQ → STAGE 2 = okrągły, gruby ogon), przesterowany punch, OTT.
  Proporcje harmonicznych ogona zgadzały się z referencjami, różnica była w pierwszych ~100 ms.
- **Forge: dopasowanie po widmie to za mało** → miary widma / obwiedni / pitchu nie słyszą „gęstości” ataku ani charakteru.
  Pomaga płaskość widma 100–2000 Hz w pierwszych 150 ms (atak chaotyczny vs okresowy). Ostatecznie decyduje ucho właściciela.
- **Forge: GRIT, SCREAM ciągle brudziły czysty ogon** → powiązane z obwiednią DRIVE (rysowana „linia brudu”), wtedy da się zrobić
  czysty ogon euphoric i reverse bass.
- **Ancient Drums: PITCH zmieniał długość sampla** → WSOLA (ziarna 42 ms z dopasowaniem korelacją) zamiast prostego resamplingu.

## Praca z właścicielem
- Najpierw **porządna wtyczka (silnik, funkcje)**, potem presety. Nie dopasowywać presetów do słabego silnika.
- Testy łapią błędy techniczne, nie to, czy brzmi dobrze. Po zmianie brzmienia daj **plik A/B** (referencja vs wtyczka).
- Interfejs po angielsku, krótkie podpowiedzi na ekranie (np. linia „TOO HOT – DISTORTION”); opisy w README po polsku, prosto.
- „Bebechy” (DSP, UI) zawsze do `Shared/`, z opisem w `Shared/README.md` i testem w którejś wtyczce.

## Forge of Wrath – kicki właściciela (2026-10-22: „nice”, „nice1”, „sgsdg”)
- Wszystkie trzy robią „reverse” obwiednią AMP w samym kicku (spadek do ~0 po 170–420 ms, potem narasta do 0.9–1.0 i znika na końcu),
  LENGTH ~460 ms (= cały beat przy 130 BPM), DRIVE env z dołkiem i powrotem. Lubi: SCREAM wysoko (0.55–1.0, 0.9–1.35 kHz), TONE na skrajach,
  CLIP ~3.5–4.6 dB. Szczyt ~1.16 (ponad 0 dBFS) – zostawione jak ustawił, test dopuszcza dla OWNER.
- „Punch chuja daje”: czysty sinusowy PUNCH ginie przy przesterowanym ogonie; to, co właściciel słyszy jako uderzenie, to początek
  ogona (START × nuta, więc chodzi z nutą). Chce: punch brzmiący jak początek taila, ale ze stałą wysokością → PUNCH INTO DIST
  (punch przez łańcuch przesterów ogona, wysokość tylko z gałek), domyślnie włączony w INIT.
- Wniosek: reverse w kicku to jego sposób pracy → sekcja REVERSE BASS (v2.1) jako uzupełnienie, nie zamiennik obwiedni.

## Z dkd Damiana (`ssonicsystemss/dkd`, 2026-10-22) – technika, której nam brakowało (szczegóły: RESEARCH.md)
- **Oversampler (i inne bufory) tworzyć RAZ w konstruktorze, w `prepare` tylko `reset` / `initProcessing`.** FL przy „Replace” /
  ponownym prepare potrafi mieć jeszcze callback audio w locie – `make_unique` w prepare zwalnia obiekt pod wątkiem audio (crash).
  U nas tak robią: AlphaEngine, ForgeEngine, FortuneEngine, SerpentEngine, KickEngine (Ancient Drums), ScreechEngine (PoS), DoomsdayDrive
  `OverSampler` – do poprawki. Do tego flaga `prepared` (atomic) – processBlock bez prepare = cisza.
- **Latencja oversamplingu zgłaszana hostowi** (`setLatencySamples`) – u nas tylko Throne. Część sygnału omijająca oversampler
  (np. CLICK) opóźniona o tę samą latencję.
- `apvts.getRawParameterValue ("id")` w processBlock buduje `juce::String` (może alokować) → `host::ParamCache` albo wskaźniki zapisane raz.
- `setStateInformation`: `host::sanitizeState (xml)` przed `replaceState` (NaN z zepsutego projektu = cisza / trzaski).
- Zegar tempa: przy skoku pętli / przewinięciu ustaw „poprzednią pozycję” tuż przed beatem, na który host skoczył, inaczej pierwszy kick
  po pętli znika (`host::BeatClock`).
- FL nie daje wtyczce pozycji „automation” w menu parametru → „last tweaked” (pusty gest) + podpowiedź „Tools > Last tweaked > Create
  automation clip”. RANDOM jako animacja w gestach = FL może nagrać zmianę.
- Testy renderu z **udawanym hostem** (`AudioPlayHead`, transport gra) i **sprawdzające muzykę** (uderzenie na każdy beat, cisza przed
  kickiem, wysokość nuty), plus **głośność niezależna od sample rate** (44.1 / 48 / 96 kHz ±0.5 dB; współczynniki DC / wygładzania liczone
  od sample rate). Nasze testy prawie nie ustawiają playheada.
- Przy okazji: test `VolumeShaper` w `Shared/Tests` czytał poza wektorem (przechodził przypadkiem) – poprawione. Przy `juce_dsp`
  w tym samym pliku `juce::jmin<juce::int64>` się nie kompiluje (SIMDRegister) – użyć `std::min`.

## Z toolboxa Damiana (ssonicsystemss/sonic-toolbox, `screens/CLAUDE.md`) – do stosowania u nas
- Ekrany / wyświetlacze: **trzy warstwy** – statyczne tło w cache'owanym obrazie (ramka, szkło, skale), warstwa „live”
  (tylko to, co się rusza) i nakładka (odbicia, scanlines). U nas tak robi już tło canvasu; THE ANVIL / spektrum mogą też.
- **Glow = szeroki półprzezroczysty obrys pod ostrym kształtem**, nigdy blur / DropShadow / GlowEffect (drogie, źle się skalują).
- **Odświeżanie 30 Hz** wystarcza; `repaint()` tylko gdy obraz się zmienił (porównuj wartości / sformatowany tekst). 60 Hz tylko oscyloskopy.
- Ścieżki (`juce::Path`) jako składowe, `clear()` i wypełnianie od nowa – bez alokacji po rozgrzaniu; jedno `fillPath` na kolor.
- Wskazówki / mierniki z balistyką liczoną w czasie (`1 - exp(-dt / tau)`), dt ograniczone (ukryte okno nie „skacze”).
- Dane audio → GUI przez lock-free bufor; GUI kopiuje do własnej tablicy w timerze (u nas: `AnalyzerTap`, `LevelFeed`, `hitBuf`).
- Galeria 20 ekranów (`screens/gallery/screen-bench.html`) + `ScreenKit.h` – gotowe wzory na liczniki BPM / Hz / poziomu (VFD, nixie, VU, LED ladder, scope…).

## Z analizy Serum 2 (Xfer, 2025) – co robi go popularnym i co z tego bierzemy
Źródła: Xfer „Serum 2 What's New” (PDF), recenzja MusicTech (8/10), MusicTech news, KVR.
Co chwalą: **wszystko widać i da się złapać myszką** (krzywe, filtry, splittery, modulacje rysowane na gałkach),
undo / redo, przeglądarka presetów (szukanie, kategorie, tagi, oceny, podgląd, przewijanie kółkiem nad nazwą), kopiuj / wklej
modułów, makra, LFO rysowane z siatką i trybami chaos (Lorenz / Rossler), splittery FX (L/H, L/M/H, M/S), tryb „clean” drive, tryb HQ.
Co krytykują: **ciężki dla CPU i RAM**, dużo funkcji ukrytych w menu pod prawym przyciskiem, cena.

Wnioski dla Doomsday (kolejność = opłacalność):
1. **Przeglądarka presetów** we wszystkich wtyczkach: kategorie z prefiksu nazwy (RAW / HARD …), szukanie, ulubione,
   zmiana presetu kółkiem nad nazwą. Jeden klocek w `DoomsdayUI` (PresetBar v2) – zyskują wszystkie wtyczki naraz.
2. **Undo / redo** (Ctrl+Z): `AudioProcessorValueTreeState` przyjmuje `UndoManager` – tani zysk; obwiednie (`Shape`) też przez undo.
3. **Modulacja „przeciągnij na gałkę”** z pierścieniem zakresu na gałce: mamy `mod::Matrix` + `mod::Lfo`, brakuje klocka UI.
4. **Splitter pasm w przesterach**: kick / bas – przester tylko powyżej ~100 Hz, sub zostaje czysty (mamy `dyn::Crossover3`).
   W Forge: opcja „CLEAN SUB”. Ogólnie: każdy przester z wyborem pasma.
5. **Makra** (4–8 gałek sterujących kilkoma parametrami) – wygodne pod automatykę w FL.
6. **Tryb HQ** (ADAA + oversampling z `DoomsdayDrive.h`) jako przełącznik: jakość vs CPU. Domyślnie lekko – nie powtarzać błędu Serum z CPU.
7. **Chaos LFO** (Lorenz) w `mod::Lfo` – „żywe”, nieregularne ruchy dla screechy.
8. **Key & scale** w syntezatorach (Prophecy / Fortune / Forge): nuty dociągane do tonacji utworu.
9. **Nie chować funkcji** w prawym przycisku – wszystko ważne widoczne, podpowiedzi na najechanie (tooltipy) zostają.

## Z analizy Sylenth1 (LennarDigital) – czemu od lat rządzi w hardstyle (leady, supersaw)
Źródła: lennardigital.com/sylenth1, recenzja MusicRadar, Splice blog, KVR.
Budowa: 4 oscylatory bez aliasingu (part A / B, po 2), każdy do 8 głosów unison w stereo, 2 filtry analogowe 4-stopniowe
z nieliniowym nasyceniem w środku (12 / 24 dB, LP / HP / BP, DRIVE + „Warm Drive”), 2 ADSR + 2 obwiednie AMP, 2 LFO,
arpeggiator ze step sequencerem (pitch / velocity / hold na krok), efekty w małym panelu „LCD” (dist 4x OS, phaser, chorus,
EQ, delay, reverb, kompresor), 16 głosów, wyłącza nieużywane części dla CPU.
Co chwalą: **brzmi świetnie od razu** (ciepło, „analogowo”), **filtry z drive**, **unison**, **bardzo mało CPU**, prosty,
czytelny interfejs – presety podkręca się w sekundy. Co krytykują: prosta modulacja, tylko LP / HP / BP, brak filtrów
szeregowo, widać tylko 2 z 4 oscylatorów naraz.

Wnioski dla Doomsday:
1. **Mniej, ale świetnie brzmiące** – Sylenth wygrywa prostotą i dźwiękiem z pudełka, nie liczbą funkcji (przeciwieństwo Serum).
   Przy każdej wtyczce: domyślny dźwięk ma być już dobry, najważniejsze gałki duże i na wierzchu.
2. **Filtr analogowy z nasyceniem w środku** – ZROBIONE: `filter::Ladder` w `DoomsdayFilter.h` (testy: −12 dB na cutoff, 24 dB/okt,
   samooscylacja dokładnie na cutoff, stabilny przy dzikiej modulacji, DRIVE dokłada harmoniczne). Do wpięcia w leady / screeche.
3. **Unison jak w Sylenth** – SPRAWDZONE: nasz `UnisonBank` ma już nieliniowy rozkład detune, losową fazę i rozłożone stereo.
   Brakowało MIX-u supersawa (głośność bocznych głosów wobec środka) – dodane jako `sides` (domyślnie 1 = stare brzmienie bez zmian).
4. **CPU**: każdy moduł, który jest wyłączony / na 0, nic nie liczy (zasada dla wszystkich klocków) + przełącznik HQ (z Serum).
5. **Arpeggiator ze step sequencerem** (pitch / velocity / hold na krok) – przyda się w syntezatorach melodii.
6. **Kompaktowy panel efektów** w stylu wyświetlacza (tu pasują ekrany z toolboxa Damiana).

## Kolejne analizy (szczegóły w `RESEARCH.md`)
Kick 2 → Forge: EXPORT WAV w tonacji, pitch w nazwach nut, warstwy osobno na podglądzie, więcej warstw sampla.
Pro-Q 4 → Mirror: dynamic z auto-progiem, EQ MATCH do referencji, rysowanie krzywej. Pro-L 2 → Throne: AUDITION / DELTA,
style limitera, wykres redukcji w czasie. Valhalla → Abyss: tryby o wyraźnym charakterze, ATTACK, losowa modulacja bez falowania
wysokości. Spire → syntezatory: wzór „TRYB + CTRL A / CTRL B”, unison z akordami, dwa filtry szeregowo / równolegle.
Vital → warpy harmonicznych, animowane wskaźniki modulacji, randomizery Perlin / Lorenz. ShaperBox → wtyczka shaper z AUDIO TRIGGER
od kicka i podziałem na pasma. soothe2 → tryb RESONANCE w Mirror + DELTA. OTT → domyślnie nisko (~20 %), reverb / delay po OTT.
Ozone 12 → asystent „gatunek + cel” w Throne. Saturn 2 / Trash 2 → przestery wielopasmowe, DYNAMICS, rysowany waveshaper,
filtr samogłoskowy. Gross Beat → wtyczka build-up / FX (tape stop, stutter). Maximus → Throne z rysowaną krzywą kompresji, REL + REL2.
Kickstart 2 → prosty tryb sidechaina (gotowe krzywe + jedna gałka + audio trigger + tylko dół). Diva → „slop” między głosami, tryby
dokładności. Decapitator → AUTO-GAIN, PUNISH, THUMP, STEEP. Pro-C 2 → styl PUMPING, HOLD, przewijana fala z redukcją. Sytrus → FM
operatorów na screeche. Massive → INTENSITY z trybami (SPECTRUM / BEND / FORMANT), oscylator modulujący RING / PHASE / FILTER FM,
pętla FEEDBACK przez filtry, PERFORMER (sekwencje krzywych), filtry szeregowo ↔ równolegle. **Technika kicka (NI): EQ przed przesterem tnie dół i podbija 300–400 Hz, między przesterami kolejny EQ,
bitcrush na ogonie, punch strojony do ogona, ogon ściszany od punchu.** **Pełna lista pomysłów: koniec `RESEARCH.md`.**

## Prophecy of Scream – zamknięty po v3.1 (2026-10-21)
- Właściciel: presety „chujowe”, w jego własnych presetach (a1–a5) „brakuje polotu”. Prototyp LIFT (vibrato z opóźnieniem, RISE filtra /
  formantu, AIR = piła oktawę wyżej, SHIMMER = chorus z feedbackiem) w A/B: „nie o to chodzi” – ruch i szerokość to nie to.
- Pomiar vs jego wzorce (Massive / Sylenth / Serum, FHSP): centroid wzorców ~2–3 kHz z dużym ruchem; PoS a2–a5 w tym zakresie, a1 ~6 kHz
  (za dużo szumu + OTT). Preset 3 (najbardziej „środkowy”) podobał mu się najbardziej. Wniosek: brakuje bogactwa źródła / łańcucha
  (wavetable, comb, kilka przesterów, formanty), nie modulacji – silnik saw/square + FM + 1 przester ma za mały sufit.
- Decyzja właściciela: PoS zamrożony (ostatnia zmiana PoS: commit `f745e5e`; tagów nie da się wypchnąć z tego środowiska), nowy syntezator screechy pisany od zera na wiedzy z `RESEARCH.md` i klockach `Shared/`.
  Przed budową: poprosić o 1 przykład z „polotem” i porównać w tej samej nucie.

## 5CR34M 4LPH4 – pierwsze A/B (2026-10-21)
- Dopasowanie automatem (CMA-ES na kształt widma + płaskość) dało „**odkurzacz**”: automat podrobił gęstą górę wzorców SZUMEM
  (chaos = szum w fazie). Ucho słyszy „czysty ton + syk”. → CHAOS w silniku to teraz pływanie wysokości każdego głosu (dźwięk dalej ma nutę),
  szum tylko jako mały GRIT; matcher mierzy tonalność pasm i karze szum w paśmie głosu (400–1600 Hz). Właściciel: zrób z tego hoovery → v0.1.

## 5CR34M 4LPH4 – co właścicielowi się podoba (jego presety „6”, „7”, „8”, 2026-10-22: „świetnie brzmią”)
- Szerokie rozstrojone piły (4–7 głosów, detune 45–52 ct, SAW 0.75–0.9) + CHAOS 0.33–0.58 przy 10 Hz, GRIT 0, bez comb.
- DRIVE 1 SOFT/FOLD ~20–23 dB → DRIVE 2 TUBE; formant lekki (~1.35 kHz, +1…4 dB); FORTUNE 0.44–0.86; OTT 0.35–0.47, delay ~0.21, reverb 0.28–0.35.
- Rzut nuty: +3 st / 280 ms albo −6 st / 450 ms; glide 42 ms albo bardzo długi (550 ms).
- Preset „8” (2026-10-22) to drugi, „wokalny” klimat: 3 głosy, detune 14 ct, mocny CORE (0.85, shape 0.77), COMB 0.28 ×2,
  formant mocny i otwarty (~1.08 kHz, +10 dB, Q 2), DRIVE 1 łagodny (6 dB) → DRIVE 2 TUBE ~10 dB, FORTUNE 1.0, OTT ~0, chorus 0.44, low cut 125 Hz.
  Wniosek: nie ma jednego „jego” brzmienia – są (na razie) dwa kierunki; RANDOM musi losować kierunek, a nie uśredniać zakresy.
- RANDOM: 45 % kierunek A (6 / 7), 30 % kierunek B (8), 25 % „dziko”.
- Filtr ladder (doomsday::filter) NIE jest przezroczysty nawet na 20 kHz – ma własne nasycenie (~2.5 dB mniej szczytów).
  Presety 6 / 7 grały przez niego (Wobble + Depth na otwartym filtrze), więc MOD na CUTOFF zawsze trzyma ladder włączony.
  Przy zmianach toru: porównać RMS / peak fabrycznych presetów przed i po (RenderTest wypisuje je). Uwaga: Wobble z Cutoff 20 kHz i Depth + nic nie robi (filtr i tak otwarty).

## Technika (pułapki)
- **RANDOMIZE bez wyrównania głośności** (PoS do v3.1): losowe brzmienia wychodziły 10–20 dB ciszej niż presety (filtr BAND, formant,
  FOLD) – właściciel: „tych z randomu wgl nie słychać”. → Po losowaniu zmierz brzmienie offline (prywatny silnik, 2 trzymane nuty,
  RMS) i ustaw VOLUME na cel (2 przejścia, bo soft clip nie jest liniowy). Test: `randomize level` w RenderTest PoS.
- **Podział pasm przez odejmowanie** (`high = x - lowpass (x)`) daje garb przy częstotliwości podziału (przesunięcie fazy) –
  CLEAN SUB w Forge podbijał 3. harmoniczną o +6 dB. → Zawsze **Linkwitz-Riley 24 dB** (2x Butterworth LP / HP, `Svf lp / hp`).
- **Clipper za czystym subem zjada efekt** (sub znów robi się kwadratem). → Przy CLEAN SUB clipper dostaje tylko górę, sub idzie obok
  (miękkie kolano tuż pod 0 dBFS jako zabezpieczenie).
- `juce::AudioBuffer` po utworzeniu ma śmieci → zawsze `clear()` (niestabilne testy).
- W pluginach z `using namespace doomsday` niektóre `jmin / jmax<int64>` łapią przeciążenia z `juce::dsp::SIMDRegister` → użyj `std::min`.
- Nazwa składowej klasy = nazwa z `ParamIDs` (np. `band`, `distType`) przesłania ID → pisz `ParamIDs::...`.
- `KickVoice::prepare()` alokuje (bufory ROOM) → nigdy w wątku audio; w `trigger()` tylko reset.
- Parametry DSP liczone raz na blok (`KickParams::cook`), nie co próbkę.
- Zmiana zakresu parametru (np. GAIN −48..+24) nie psuje zapisanych projektów (stan trzyma wartości), ale automatyzacja hosta jest w 0..1.
- `juce::String` z `"..."` traktuje bajty jak ASCII: „·”, „–”, polskie litery wychodzą jako krzaki („Â·”) → `String (CharPointer_UTF8 ("\xc2\xb7"))`
  albo tekst bez znaków spoza ASCII.
- Fonty Google / systemowe OTF (CFF) do BinaryData: przycięte do łaciny + polskich znaków i przerobione na TTF (fontTools: subset + cu2qu)
  – 3 grubości Inter = ~170 KB zamiast ~1.9 MB, pewne renderowanie na Windows.
- `Graphics::drawImage` rysuje z przezroczystością **aktualnego koloru** (np. po `setColour (white.withAlpha (0.03f))` obraz jest prawie niewidoczny)
  → `g.setOpacity (1.f)` tuż przed `drawImage` (film promo v3.1 wyszedł przez to ciemny).
- `TabStrip` / zakładki: przyciski włącznika wszystkich zakładek w tym samym miejscu → `removeFromRight` liczyć RAZ, nie w pętli (zjadało pasek zakładek).
