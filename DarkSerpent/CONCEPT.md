# DARK SERPENT – koncepcja (do zbudowania w kolejnej sesji)

Lead / screamer do dropów (klimat raw: Ran-D, Adaro), grywalny w melodiach.

## Wygląd
- czarne tło + limonka (toxic lime), styl "sceny" jak PoS / Fortune of Seekers
- nazwę DARK SERPENT owija wąż z dymu (rysowany kodem): limonkowo-białe smugi, raz za, raz przed literami,
  głowa ze świecącymi oczami przy końcu napisu, dym delikatnie dryfuje, świeci mocniej gdy lead gra
- na środku pad XY samogłosek (X = A-E-I-O-U, Y = intensywność)

## Analiza referencji (7 sampli "SCREECH HOUSE LEAD", C5 = 261.6 Hz)
- mało przesteru: crest 10–14 dB, 70–90% energii harmoniczne
- bardzo gęsty unison: harmoniczne rozmyte na 85–250 ct
- ekstremalne stereo: korelacja L/R ≈ 0, side ≈ mid
- 2. harmoniczna = podstawowa, parzyste > nieparzyste -> warstwa oktawy (piła + piła +12)
- ruch barwy: centroid pływa o 1–3 kHz, ~3–4 Hz (LFO na filtrze / wah); w jednym obwiednia zamykająca przez 1.5 s
- brak bendu na starcie, lekkie vibrato ~4–5 Hz, pełne sustain

## Silnik
- warstwy CORE (unison do 16, oktawy), EDGE (jasna, szeroka), BODY (czysty mono środek)
- WIDTH do "ściany" + dół w mono
- MOVE: LFO zsynchronizowane z tempem na filtr / samogłoskę (głębokość, kształt) + obwiednia filtra
- TALK: filtr samogłoskowy (formanty), opcjonalnie przesuw samogłoski przy każdej nucie
- RAGE: przester wielostopniowy z wyrównaniem głośności, domyślnie lekki
- BEND / pitch dive: opcja, domyślnie wyłączone (albo tylko pierwsza nuta frazy)
- ENERGY: makro (przester, otwarcie samogłoski, szerokość, vibrato)
- granie: LEGATO / RETRIG, 2–3 głosy (oktawy), brzmienie wyrównane na klawiaturze, velocity -> samogłoska/przester
- FX: reverb, delay, PUMP, WIDTH

## Plan pracy
1. Wyciągnąć wspólne klocki z Fortune of Seekers do `Shared/` (kontrolki z kolorem jako parametrem,
   presety, unison osc, SVF, obwiednie, FX, szablon CMake/CI/testu). Starych pluginów nie przerabiać.
2. Dark Serpent na tych klockach + własny workflow `build-dark-serpent.yml`.
3. Porównać wynik z samplami referencyjnymi tymi samymi pomiarami (poprosić o ponowne wrzucenie sampli).
