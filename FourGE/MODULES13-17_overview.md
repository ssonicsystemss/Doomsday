<!-- Owner overview of modules 13-17 (2026-11-04) – the management layer of 4GE. Detailed specs per module may follow (MODULE13.md …). Plan: PLAN.md 1m. -->
> **Zasada nadrzędna:** 4GE ma dokładnie 8 MASTER MACROS (IMPACT, BODY, TAIL, PITCH, GRIT, SCREAM, WEIGHT, CHAOS). Makra / kontrolery opisane w tym module są LOKALNE i sterowane przez Macro Mapping (M09) – patrz `RULES.md`.
# 4GE — MODULES 13–17: warstwa zarządzająca instrumentem (przegląd właściciela)

## MODULE 13 — STYLE ENGINE (Priorytet) · EARLY / MODERN / RAW / EXPERIMENTAL
Silnik charakteru brzmienia. Każdy styl ustawia wartości początkowe, konfigurację generatorów, pitch, distortion, filtrów, screechu i dynamiki.
- EARLY — klasyczny hardstyle, charakterystyczny punch, reverse bass i starsza estetyka.
- MODERN — czysty, mocny, kontrolowany kick z nowoczesną strukturą harmoniczną.
- RAW — agresywny, przesterowany, ostry i ciężki charakter.
- EXPERIMENTAL — nietypowe modulacje, tekstury, niestandardowe przebiegi i routing.
Najważniejsza zasada: styl i długość są niezależne. Styl nie blokuje parametrów ani nie narzuca długości. Zmiana stylu może zmieniać ustawienia, ale nie może niszczyć indywidualnych ustawień użytkownika.

## MODULE 14 — KICK LENGTH ENGINE · SHORT / MEDIUM / LONG / EXTRA LONG / AUTO
Niezależny system czasu trwania kicka.
- SHORT ~100–250 ms · MEDIUM ~250–500 ms · LONG ~500–1000 ms · EXTRA LONG ~1000–2500 ms · AUTO LENGTH – automatyczne dopasowanie.
Osobny czas trwania PUNCH, BODY, TAIL i SUB; zachowanie ogonów efektów; obwiednie; synchronizacja z BPM.
Kluczowe: można zrobić EARLY EXTRA LONG albo MODERN SHORT. Żaden styl nie narzuca długości.

## MODULE 15 — PRESET & BROWSER ENGINE · PRESETS / TAGS / SEARCH / A-B
- Preset Browser z kategoriami i tagami.
- Wyszukiwanie po stylu, długości, charakterze i typie kicka.
- Favorites, Recently Used, User Presets.
- Save, Save As, Duplicate, Rename, Delete.
- A/B Comparison, Preset Preview.
- Informacje o wykorzystanych modułach i ustawieniach.
- Import i eksport presetów.
- Kontrola brakujących sampli i zasobów.

## MODULE 16 — KICK ANALYSIS ENGINE · ANALYZE / DETECT / RECOMMEND
Analiza kicków z 4GE i importowanych plików referencyjnych:
waveform; pitch curve; spektrum i harmoniczne; transient; długość kicka; subbas i energia pasm; stereo i faza; Peak, RMS, Crest Factor; wykrywanie charakterystycznych cech; sugestie ustawień 4GE na podstawie analizy.
Ważne: analiza daje informacje i sugestie – nie zmienia parametrów automatycznie bez zgody użytkownika.

## MODULE 17 — GUI & UX SYSTEM · COMPLETE INTERFACE ARCHITECTURE
Główna nawigacja; rozmieszczenie wszystkich 17 modułów; centralny waveform editor; parametry globalne; style i długość; Preset Browser; Analyzer; system makr; skalowanie GUI; tooltips i informacje o parametrach; drag & drop, menu kontekstowe, workflow; spójny design wszystkich ekranów.

---
## NOTES (Claude, 2026-11-04)
- M13: styl = „szablon startowy” zapisany jako zestaw wartości + flaga „user touched” per parametr – zmiana stylu nie nadpisuje parametrów ruszonych przez użytkownika (albo pyta). Domyślne łańcuchy distortion per styl już są w PLAN.
- M14: długość skaluje czasy obwiedni PUNCH / BODY / TAIL / SUB (multipoint z M02) zamiast ucinać; AUTO = z BPM (np. do następnego uderzenia) lub z detekcji ciszy (M12).
- M15: baza `Shared` PresetBar + kategorie z RE4GE (`presetCategory`), A/B + morph z RE4GE.
- M16: baza RE4GE `RefAnalysis.h` + ForgeOfWrath/Tools (matching do referencji, 38 pomiarów w RESEARCH) – sugestie jako lista „co zmienić” z przyciskiem APPLY (1 klik = 1 undo).
- M17: makieta `Design/mock.html` (czerwono-czarna, 5 bloków) – czeka na opinię właściciela.
- PYTANIE na koniec: EARLY ma mieć „reverse bass” – reverse bass jest teraz osobno w RE4GE. Czy 4GE ma mieć prostą warstwę reverse (np. z RE4GE jako źródło), czy EARLY tylko przygotowuje kick pod RE4GE?
