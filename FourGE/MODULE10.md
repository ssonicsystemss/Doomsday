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
