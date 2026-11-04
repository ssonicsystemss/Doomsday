<!-- Owner specification of module 12 (2026-11-04), collected from chat piece by piece. Plan: PLAN.md 1l. -->
> **Zasada nadrzędna:** 4GE ma dokładnie 8 MASTER MACROS (IMPACT, BODY, TAIL, PITCH, GRIT, SCREAM, WEIGHT, CHAOS). Makra / kontrolery opisane w tym module są LOKALNE i sterowane przez Macro Mapping (M09) – patrz `RULES.md`.
# 4GE — MODULE 12: MASTER OUTPUT & EXPORT ENGINE

## 1. CEL MODUŁU
Master Output & Export Engine odpowiada za końcowy sygnał wyjściowy instrumentu, kontrolę poziomu, mierniki oraz eksport gotowych kicków do plików audio.
Moduł nie zastępuje procesorów dynamiki z Module 08 – zarządza wyjściem i prawidłowym renderowaniem dźwięku.
Najważniejsze cele: profesjonalny master output; dokładne mierniki poziomu; pomiar True Peak; pomiar RMS i LUFS; eksport pojedynczych kicków; eksport wybranych grup i warstw; Batch Export; Offline Rendering; kontrola formatu i jakości plików; powtarzalność renderowania.
WAŻNE: wtyczka nie może automatycznie zmieniać głośności, normalizować ani limitować sygnału bez wiedzy użytkownika.

## 2. MASTER OUTPUT
| Parametr | Zakres |
|---|---|
| MASTER LEVEL | -60 do +12 dB |
| MASTER MUTE | ON/OFF |
| MASTER BYPASS | ON/OFF |
| OUTPUT MODE | MONO/STEREO |
| SAFETY CLIP | ON/OFF |
| SAFETY LIMITER | ON/OFF |
MASTER LEVEL = końcowa regulacja poziomu; zmiana wygładzana, bez kliknięć.
SAFETY CLIP i SAFETY LIMITER – opcjonalne zabezpieczenia, domyślnie WYŁĄCZONE. Po aktywacji interfejs wyraźnie informuje o ich działaniu. Eksport nie korzysta z nich automatycznie, jeśli użytkownik ich nie wybrał w ustawieniach eksportu.

## 3. MASTER METERING ENGINE
- PEAK METER (dBFS): Left, Right, Maximum Peak, Peak Hold, Clip Indicator, Reset.
- TRUE PEAK METER: oversampling pomiarowy, wartość w dBTP, mierzony niezależnie od Sample Peak.
- RMS METER: określone okno czasowe, średni poziom energii.
- LOUDNESS METER (LUFS): Integrated LUFS (wymagany; dostępny po zakończeniu renderowania), Short-Term i Momentary – opcjonalnie.
- DC OFFSET DETECTION: ostrzeżenie przy wykryciu; usuwanie DC = osobna, jawna funkcja.

## 4. EXPORT FORMAT ENGINE
Formaty: WAV (wymagany), AIFF i FLAC (dodatkowe).
Bit depth: 16-bit PCM, 24-bit PCM, 32-bit Float (bez ditheringu).
Sample rate: 44.1, 48, 88.2, 96, 176.4, 192 kHz, USE PROJECT / HOST RATE. Zmiana SR – wysokiej jakości resampling z filtrem antyaliasingowym. Eksport nie zmienia SR projektu / hosta.

## 5. DITHER ENGINE
Dla formatów integer PCM. Tryby: OFF, TPDF, NOISE SHAPING (opcjonalnie). Stosowany przy redukcji głębi (np. 16-bit); nie dla 32-bit Float. Wykonywany tylko raz, na samym końcu (po przetwarzaniu i resamplingu). Domyślnie OFF.

## 6. NORMALIZATION ENGINE
- OFF – brak automatycznej zmiany poziomu.
- PEAK NORMALIZE – PEAK TARGET (dBFS).
- LUFS NORMALIZE – LUFS TARGET.
Własne wartości docelowe. TRUE PEAK CEILING – osobny parametr maksymalnego True Peak.
Jeśli osiągnięcie celu wymaga limitowania – system NIE uruchamia limitera automatycznie, tylko informuje o konieczności dodatkowego przetwarzania. Przed eksportem pokazuje przewidywaną zmianę gainu.

## 7. EXPORT SOURCE SELECTION
FULL KICK, PUNCH, BODY, TAIL, SUB, TEXTURE, CUSTOM GROUP, SELECTED LAYER.
Eksport grupy uwzględnia wszystkie procesory tej grupy wg aktualnego routingu. Eksport warstwy bez sygnału pozostałych warstw. Procesor wspólny dla kilku grup – jasna informacja, czy jest uwzględniany w eksporcie.

## 8. KICK LENGTH & TAIL HANDLING
Współpraca z systemem długości kicka (moduł KICK). Tryby: AUTO (długość z sygnału i ustawień długości), FIXED LENGTH (dokładny czas), UNTIL SILENCE (do progu ciszy).
Parametry: PRE-ROLL, POST-ROLL, SILENCE THRESHOLD, MINIMUM SILENCE DURATION, MAXIMUM RENDER LENGTH, TRIM SILENCE.
POST-ROLL uwzględnia wybrzmiewanie reverbu, delay, tail, screech i innych procesorów z ogonami – bez ucinania końcówki tylko dlatego, że BODY już wybrzmiało. Detekcja ciszy z histerezą.

## 9. OFFLINE RENDER ENGINE
Wysoka jakość niezależnie od wydajności realtime. Poza audio callback; kompensacja opóźnień; oversampling, obwiednie, modulacje, LFO, randomizer, sample start, wszystkie ogony efektów; mono i stereo.
Ta sama logika DSP co realtime – eksport nie może dać innego kicka niż nagranie wyjścia na żywo.

## 10. DETERMINISTIC RENDERING
Szczególnie ważne (randomizer + modulacje). Zapisane seedy. Tryby: FIXED SEED (ten sam preset + seed = ten sam wynik), NEW SEED (każdy render może być nowy), PRESET SEED (preset zapisuje własny seed), CUSTOM SEED (wpisany przez użytkownika).
Wszystkie generatory losowe korzystają z kontrolowanego systemu seedów. Render offline nie zmienia seedów bez zgody użytkownika.

## 11. BATCH EXPORT ENGINE
Wiele wariantów bez ręcznego uruchamiania każdego renderu: wiele presetów, grup, wariantów, seedów, wybranych warstw; automatyczne nazewnictwo plików.
Przykładowa struktura:
```
4GE_EXPORT/
    EARLY/KICK_01/     KICK_FULL.wav  KICK_PUNCH.wav  KICK_BODY.wav  KICK_TAIL.wav
    MODERN/KICK_02/    KICK_FULL.wav  KICK_PUNCH.wav  KICK_BODY.wav  KICK_TAIL.wav
    RAWSTYLE/KICK_03/  KICK_FULL.wav  KICK_PUNCH.wav  KICK_BODY.wav  KICK_TAIL.wav
```
Wybór folderu docelowego. Bez nadpisywania istniejących plików bez wyraźnej zgody.

## 12. FILE NAMING SYSTEM
Generator nazw plików. Elementy: PRESET NAME, STYLE, ROOT NOTE, BPM, GROUP, VARIANT, VERSION, SEED.
Przykład: `4GE_MODERN_KICK01_F#_150BPM_TAIL_V02.wav`. Użytkownik może stworzyć własny schemat nazewnictwa.

## 13. EXPORT HISTORY
Zapamiętuje ostatnie eksporty: nazwa pliku, nazwa presetu, data, format, bit depth, sample rate, długość, Peak, True Peak, LUFS (jeśli dostępny), seed. Ponowny eksport z identycznymi ustawieniami.

## 14. EXPORT USER INTERFACE
Panel EXPORT czytelny, podzielony na sekcje:
- SOURCE: Export Source, Selected Layer, Selected Group.
- LENGTH: Auto, Fixed, Until Silence, Pre-Roll, Post-Roll.
- FORMAT: WAV, AIFF, FLAC.
- QUALITY: Bit Depth, Sample Rate, Dither.
- NORMALIZATION: Mode, Target, True Peak Ceiling.
- DESTINATION: Folder, File Name, Naming Template.
- RENDER: Preview, Export, Batch Export, Cancel.
Pasek postępu podczas eksportu; anulowanie bezpiecznie zwalnia zasoby. Po zakończeniu: ścieżka, format, sample rate, bit depth, długość, Peak, LUFS (jeśli dostępny).

## 15. ERROR HANDLING
Obsługa: brak miejsca na dysku, brak uprawnień do folderu, nieprawidłowa ścieżka, nieobsługiwany format, błąd zapisu, przerwanie renderowania, brak wymaganych zasobów, nieprawidłowa konfiguracja eksportu.
Nie wolno zostawić uszkodzonego pliku, który wygląda na ukończony – render do pliku tymczasowego, po poprawnym zakończeniu przeniesienie pod docelową nazwę.
Render na spójnej migawce ustawień – zmiany parametrów w trakcie eksportu nie zmieniają rozpoczętego renderu.

## 16. INTEGRACJA Z POZOSTAŁYMI MODUŁAMI
Wszystkie moduły, szczególnie 04 (Layer), 05 (Distortion & Waveshaper), 06 (Filter & Screech), 08 (Dynamics), 09 (Modulation Matrix), 10 (Advanced Sample), 11 (Stereo & Spatial). Eksport respektuje routing, długość kicka, modulacje, stereo i wszystkie aktywne procesory.

## 17. TESTY AKCEPTACYJNE
1. WAV z prawidłowym nagłówkiem.
2. Bit Depth zgodny z ustawieniem.
3. Sample Rate zgodny z ustawieniem.
4. Eksport stereo i mono poprawny.
5. Dithering tylko gdy potrzebny i włączony.
6. Normalizacja nie uruchamia ukrytego limitera.
7. True Peak i Sample Peak mierzone oddzielnie.
8. Render uwzględnia latency compensation.
9. Render nie ucina ogonów efektów.
10. Fixed Seed daje powtarzalny wynik.
11. Batch Export tworzy wszystkie wybrane pliki.
12. Anulowanie nie zostawia uszkodzonego pliku oznaczonego jako ukończony.
13. Zmiana parametrów podczas eksportu nie zmienia rozpoczętego renderu.
14. Brak miejsca na dysku nie powoduje crasha.

---
## NOTES (Claude, 2026-11-04) – decyzje techniczne
- Baza: RE4GE `Source/Export/Exporter.h` – render na wątku tła na KOPII parametrów i sampla (migawka = pkt 15), ta sama klasa Engine co realtime (pkt 9), przeciąganie gotowego WAV do FL.
- Zapis do `nazwa.wav.part` → po sukcesie rename; anulowanie / błąd = usunięcie pliku tymczasowego.
- Mierniki: Sample Peak + True Peak 4x (ITU-R BS.1770) + RMS + LUFS z Throne of Titans (`Shared` loudness), DC detection.
- Długość: AUTO = z Length Engine + post-roll aż ogony (screech / reverb) spadną poniżej progu z histerezą; MAX RENDER LENGTH jako bezpiecznik.
- Seedy: jeden globalny generator z seedem dla randomizera / random LFO / per-trigger (M09); FIXED / NEW / PRESET / CUSTOM.
- Nazwy plików: opcja zamiany „#” na „s” (F# → Fs) dla programów, które źle znoszą „#”.
- SAFETY CLIP / LIMITER domyślnie OFF; normalizacja nigdy nie włącza limitera sama – tylko komunikat.
