<!-- Owner specification of module 11 (2026-11-04), collected from chat piece by piece. Plan: PLAN.md 1k. -->
# 4GE — MODULE 11: STEREO & SPATIAL ENGINE
Specyfikacja techniczna dla AI / developera · SonicSystems • 4GE Kick Synthesizer • Module 11

## 1. CEL MODUŁU
Stereo & Spatial Engine odpowiada za kontrolę przestrzeni stereo, pozycjonowanie warstw, szerokość dźwięku, relacje fazowe oraz zgodność sygnału z odsłuchem mono.
Precyzyjna kontrola obrazu stereo na trzech poziomach: pojedyncza warstwa; grupa PUNCH / BODY / TAIL / SUB / TEXTURE / CUSTOM; cały instrument — MASTER.
Cel: szerokie, przestrzenne brzmienie hardstyle bez utraty kontroli nad środkiem, dołem pasma i kompatybilnością mono.
Zasada projektowa: szerokość stereo nie może być uzyskiwana kosztem przypadkowego odwracania fazy, niekontrolowanego opóźniania kanałów lub niepożądanego wzrostu poziomu.

## 2. PODSTAWOWE PARAMETRY STEREO
- PAN: -100% (całkowicie lewy) / 0% (środek) / +100% (całkowicie prawy). Dla źródeł mono panoramowanie o stałej mocy lub inna jasno udokumentowana krzywa.
- BALANCE: proporcja głośności L/R. Nie może być kopią funkcjonalną PAN – PAN przesuwa pozycję źródła, BALANCE zmienia proporcję kanałów.
- STEREO WIDTH: 0–200% (0% mono, 100% oryginał, 200% maksimum danego algorytmu). Bez niekontrolowanego wzrostu poziomu.
- CHANNEL SWAP: zamiana L i R – funkcja diagnostyczna i kreatywna.

## 3. MID/SIDE ENGINE
Pełna obsługa Mid/Side. Sygnał dzielony na MID (centralna część) i SIDE (różnica między kanałami).
