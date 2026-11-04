<!-- Owner specification of module 10 (2026-11-04), collected from chat piece by piece. Plan: PLAN.md. -->
# 4GE — MODULE 10: ADVANCED SAMPLE ENGINE
Specyfikacja techniczna dla AI / developera · SonicSystems • 4GE Kick Synthesizer • Module 10

## 1. CEL MODUŁU
Advanced Sample Engine odpowiada za importowanie, odtwarzanie, edycję i przetwarzanie zewnętrznych próbek audio wewnątrz 4GE.
Moduł rozszerza możliwości syntezy kicków hardstyle. Nie może zamieniać 4GE w zwykły sampler. Sample są jednym z dostępnych źródeł dźwięku, równorzędnym z oscylatorami, generatorem szumu, transjentami i pozostałymi generatorami.
Silnik musi umożliwiać:
- Wczytywanie własnych próbek.
- Łączenie sampli z syntezą wewnętrzną.
- Budowanie wielowarstwowych kicków.
- Edycję początku i końca próbki.
- Reverse playback.
- Loop i Ping-Pong.
- Pitch shifting i kontrolę playback rate.
- Routing sampli do poszczególnych sekcji kicka.
- Przetwarzanie sampli przez pozostałe moduły 4GE.
- Modulowanie parametrów sampli za pomocą Modulation Matrix.

## 2. OBSŁUGIWANE FORMATY AUDIO
| Format | Obsługa |
|---|---|
| WAV | Wymagana |
| AIFF | Wymagana |
| FLAC | Opcjonalna |
| MP3 | Opcjonalna |
Głębie bitowe: 16-bit PCM, 24-bit PCM, 32-bit float.
Częstotliwości próbkowania: 44.1, 48, 88.2, 96, 176.4, 192 kHz.
Pliki mono i stereo. Pliki wielokanałowe odpowiednio zmapowane albo odrzucone z czytelnym komunikatem.
Konwersja sample rate: wysokiej jakości resampling z filtrowaniem antyaliasingowym. Nie wolno modyfikować oryginalnego pliku użytkownika.

## 3. ARCHITEKTURA SLOTÓW SAMPLE
Minimum 8 niezależnych slotów, docelowo 16 lub więcej. Każdy slot jest niezależnym źródłem dźwięku.
Każdy slot: ENABLE, MUTE, SOLO, LEVEL, PAN, PITCH, FINE TUNE, START, END, REVERSE, PLAY MODE, ROOT NOTE, WIDTH, OUTPUT ROUTING.
Przypisanie do grupy: PUNCH, BODY, TAIL, SUB, TEXTURE, CUSTOM. Zmiana grupy nie kasuje ustawień próbki.

## 4. TRYBY ODTWARZANIA
- ONE SHOT — od początku wybranego regionu do końca, niezależnie od długości nuty MIDI.
- GATE — zależne od długości nuty; po zwolnieniu kontrolowany release.
- LOOP — zapętlenie fragmentu; Loop Start, Loop End, Loop Crossfade, Loop Enable; crossfade zapobiega kliknięciom na granicy pętli.
- PING-PONG — naprzemiennie do przodu i do tyłu.
- REVERSE — odwrócenie kierunku bez destrukcyjnej zmiany pliku źródłowego.

## 5. SAMPLE PLAYBACK ENGINE
Każdy slot ma niezależną kontrolę odtwarzania.
| Parametr | Zakres / zachowanie |
|---|---|
| START | 0–100% długości próbki |
| END | 0–100% długości; END musi być za START |
| FADE IN | 0–500 ms |
| FADE OUT | 0–500 ms |
| PLAYBACK RATE | 0.25x–4.0x; zmienia prędkość i wysokość (tryb bez kompensacji pitch) |
| PITCH SHIFT | Jeśli jest algorytm niezależnego pitch shiftingu – wysokość bez zmiany czasu trwania |
| ROOT NOTE | Nuta bazowa próbki |
| PITCH TRACKING | ON/OFF – reakcja na nuty MIDI względem nuty bazowej |
| FINE TUNE | -100 do +100 centów |

## 6. TIME STRETCH
Opcjonalny algorytm zmieniający długość próbki bez zmiany wysokości.
Wymagania: zachowanie transjentów; minimalizacja artefaktów; zachowanie oryginalnej długości lub dopasowanie do określonego czasu; mono i stereo.
Ciężkie obliczenia – przygotowanie danych poza wątkiem audio; nie blokować audio callback.

## 7. WAVEFORM EDITOR
Każdy slot ma edytor przebiegu. Pokazuje: waveform mono / stereo, START i END marker, Loop Start / End, Fade In / Out, aktualną pozycję odtwarzania.
Wymagane: Zoom In / Out, Horizontal Scroll, Fit to Sample, Region Preview, precyzyjna edycja markerów, Undo, Redo.
Dodatkowe: Transient Detection, Silence Trimming, DC Offset Removal, Normalize.
Normalizacja i inne operacje destrukcyjne działają na kopii roboczej, nie na oryginalnym pliku.

## 8. SLOT PROCESSING
Każdy slot ma niezależne przetwarzanie:
| Parametr | Zakres |
|---|---|
| LEVEL | -60 do +12 dB |
| PAN | -100 do +100 |
| WIDTH | 0–200% |
| PITCH | -48 do +48 semitones |
| FINE | -100 do +100 cents |
| PHASE OFFSET | kontrolowany zakres |
| POLARITY | Normal / Invert |
| FILTER SEND | 0–100% |
| DISTORTION SEND | 0–100% |
Każdy slot ma własną obwiednię amplitudy: ADSR, One Shot, Multi-Point Envelope.
Parametry slotów dostępne w Modulation Matrix.

## 9. SAMPLE ROUTING
Sample Engine współpracuje z pozostałymi generatorami 4GE.
