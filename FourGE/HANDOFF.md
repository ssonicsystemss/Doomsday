# HANDOFF – 4GE (2026-11-24, z sesji czatu do Claude Code)

Czytaj najpierw: `CLAUDE.md` (root), `FourGE/ARCHITECTURE.md`, `FourGE/DSP_ARCHITECTURE.md` (spec właściciela + lista braków na końcu),
`FourGE/ARCH_DECISIONS.md` (decyzje właściciela), `FourGE/HARDCORE_ENGINE.md`, `Shared/LESSONS.md` (ostatnie wpisy).
Odpowiadaj po polsku, prosto. Oceniaj ucho właściciela, nie liczby.

## Gdzie jesteśmy
- TAIL TYPE HARDCORE (`DSP/Tone/HardcoreKick.h`, `hc_*`) + style GABBER / MILLENNIUM / HARDCORE / UPTEMPO = profile jednego silnika
  (`Presets/StyleEngine.h`, 4 szablony z `"tail_engine", 5`), presety fabryczne w `FactoryPresets.cpp` (`hard()`).
- Dziś dodane: MID SCREAM (`hc_ms_*`), OUTPUT CLIP (`hc_out_clip`), LAYER ENERGY (`DSP/Analysis/LayerEnergy.h`,
  `RenderTest <out.txt> --energy <preset | factory:name>`).
- Profile 4 stylów dopasowane POMIARAMI (energia w 5 pasmach, crest, centroid) do refów właściciela.

## OSTATNIA OCENA WŁAŚCICIELA (najważniejsze)
- "hardcore chujnia, millenium też, uptempo też, gabber też" – wszystkie 4 style złe po dopasowaniu pomiarami.
- Wcześniej: "basic nudny w chuj".
- Podejrzenie: MID SCREAM to STAŁY ton 400–800 Hz niezwiązany z nutą = "pikanie" (właściciel tego nie znosi). W refach środek to
  harmoniczne nuty. Najpewniej wyłączyć MID SCREAM w profilach albo zrobić go z harmonicznych nuty.
- Dopasowanie liczbami NIE działa u tego właściciela. Działało najlepiej: odtworzenie jednego konkretnego refa krok po kroku
  (raw crack: "no prawie, ale blisko").
- Proponowany następny krok (czeka na odpowiedź): właściciel wybiera 1 ref na styl (zacząć od HARDCORE) + mówi, co przeszkadza
  (pikanie / za cienki / brak uderzenia / barwa); odtworzyć ten jeden kick, A/B, dopiero potem profil.

## Refy (pliki właściciela – NIE ma ich w git, trzeba je dostać od niego ponownie)
- HARDCORE: OPS_FREE_HARDCORE_KICK_18_G, 24_E, 29_E, 31_G, 34_G, 39_G, 46_D, 47_G, 55_A, 56_D
- MILLENNIUM ("Tritone to takie millenium"): Tritone - Kick Main 1, Kick Main 2, Main Kick Crunch 1, Kick 3, 4, 8, 21, 25, 29, 30
- UPTEMPO: uptempo-kick-piercing-slide_200bpm_F, uptempo-rawstyle-kick_190bpm_C
- GABBER: hardcore-gabber-kick-162, hardcore-gabber-kick-8
- Pomiary (mediany grup): HARDCORE pełna głośność do cięcia (crest ~2 dB), pitch trzyma wysoko 50–80 ms; MILLENNIUM długi, gaśnie.

## Kolejka (ARCH_DECISIONS.md, decyzje 2026-11-24)
1. [zrobione] energia warstw (per warstwa + 5 pasm) – jeszcze nie ma jej w GUI (strona ANALYSIS).
2. Długość crunchu: L_C = clamp(L_base + kP·L_punch + kB·L_body + kT·L_tail) × gałka % (współczynniki z profilu).
3. Jeden CRUNCH ENGINE z profilami (scalić RAW CRUNCH `cn_*` i crunch HC `hc_cr_*`; stare presety brzmią tak samo).
4. SCREAM SOURCE: OSCILLATOR / TAIL RESAMPLE / BODY / CRUNCH / HYBRID dla screecha modułu 06.
5. STYLE = parametry + flagi zachowania (crunchTiming, crunchRise, tailBehavior…).
6. TAIL ENGINE: wspólna rama (TailTypeProfile) WOKÓŁ istniejących TAIL TYPE – właściciel: "nie ruszać działających TAIL TYPE",
   ich DSP zostaje, null test przed / po.
- Otwarte z wcześniej: RAW CRACK klasyczny raw 2015–2017 (najbliżej = rekonstrukcja z rawstyle-kick_G_major; właściciel nie wybrał
  między: 1) rekonstrukcja jako punch w 4GE, 2) najpierw dokładna rekonstrukcja).

## Zasady (skrót, pełne w CLAUDE.md)
- Push prosto na main jako `ssonicsystemss <tym222q@gmail.com>`; symlink JUCE usuwać przed commitem.
- ID parametrów APPEND ONLY (`Tests/param_ids.txt`). Nie odpalać dwóch RenderTestów naraz.
- Po każdej zmianie brzmienia: plik A/B dla właściciela; po każdej skardze wpis w `Shared/LESSONS.md`.
