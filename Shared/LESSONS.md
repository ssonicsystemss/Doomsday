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
2. **Filtr analogowy z nasyceniem w środku** (4 stopnie, zero-delay feedback, tanh w każdym stopniu, rezonans do samooscylacji,
   12 / 24 dB, DRIVE) – brakuje go w `Shared/` (mamy czyste SVF). Największy zysk dla leadów i screechy (Dark Serpent, PoS, Fortune).
3. **Unison jak w Sylenth**: nieliniowy rozkład detune (zewnętrzne głosy dalej), losowa faza startu, stereo rozłożone – sprawdzić
   nasz `UnisonBank` na tle tego (supersaw to podstawa hardstyle'owych leadów).
4. **CPU**: każdy moduł, który jest wyłączony / na 0, nic nie liczy (zasada dla wszystkich klocków) + przełącznik HQ (z Serum).
5. **Arpeggiator ze step sequencerem** (pitch / velocity / hold na krok) – przyda się w syntezatorach melodii.
6. **Kompaktowy panel efektów** w stylu wyświetlacza (tu pasują ekrany z toolboxa Damiana).

## Technika (pułapki)
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
