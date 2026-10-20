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

## Technika (pułapki)
- `juce::AudioBuffer` po utworzeniu ma śmieci → zawsze `clear()` (niestabilne testy).
- W pluginach z `using namespace doomsday` niektóre `jmin / jmax<int64>` łapią przeciążenia z `juce::dsp::SIMDRegister` → użyj `std::min`.
- Nazwa składowej klasy = nazwa z `ParamIDs` (np. `band`, `distType`) przesłania ID → pisz `ParamIDs::...`.
- `KickVoice::prepare()` alokuje (bufory ROOM) → nigdy w wątku audio; w `trigger()` tylko reset.
- Parametry DSP liczone raz na blok (`KickParams::cook`), nie co próbkę.
- Zmiana zakresu parametru (np. GAIN −48..+24) nie psuje zapisanych projektów (stan trzyma wartości), ale automatyzacja hosta jest w 0..1.
