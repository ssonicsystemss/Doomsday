<!-- Owner specification (2026-11-03), verbatim. The build plan is PLAN.md. -->
4GE — HARDSTYLE KICK DESIGNER

Kompletny dokument projektowy i techniczna specyfikacja VST

Nazwa produktu: 4GE
Typ: Professional Hardstyle Kick Synthesizer & Designer
Producent: Sonicus / Sonicsystems
Główne zastosowanie: projektowanie, synteza, przetwarzanie i eksport profesjonalnych kicków hardstyle.

⸻

1. GŁÓWNA KONCEPCJA

4GE ma być kompleksowym narzędziem do tworzenia kicków hardstyle, obejmującym zarówno klasyczne brzmienia Early Hardstyle, jak i współczesne Modern Hardstyle, Rawstyle oraz Experimental.

Wtyczka musi umożliwiać:

* Tworzenie kicków całkowicie od podstaw.
* Generowanie kicków z oscylatorów i generatorów transientów.
* Importowanie własnych próbek kicków i ich przetwarzanie.
* Łączenie syntezy z samplami.
* Niezależne projektowanie punchu, body i taila.
* Precyzyjne sterowanie wysokością dźwięku w czasie.
* Tworzenie charakterystycznego distortion i harmonicznych.
* Projektowanie kicków krótkich, średnich, długich i ekstremalnie długich.
* Tworzenie kicków tonalnych, melodycznych, surowych, agresywnych i eksperymentalnych.
* Eksport gotowych kicków do WAV.

Zasada nadrzędna: 4GE nie może być jedynie kolekcją efektów. To przede wszystkim instrument do projektowania kicków. Każdy główny element brzmienia powinien mieć własny generator, edytor lub procesor.

2. GŁÓWNY INTERFEJS I NAWIGACJA

Zalecana nawigacja: pionowy pasek z pięcioma blokami. Każdy blok rozwija listę modułów, a wybrany moduł wyświetla się w głównym obszarze roboczym.

2.1. Pięć głównych bloków

1. KICK

* Main
* Style
* Length
* Punch
* Body
* Tail
* Pitch

2. SYNTHESIS

* Oscillators
* Noise
* Transient Generator
* Sub Generator
* Layer Engine
* Sample Engine

3. PROCESSING

* Distortion
* Waveshaper
* Filter
* Resonance
* EQ
* Dynamics
* Stereo

4. MODULATION

* Envelopes
* LFO
* Modulation Matrix
* Sequencer
* Randomizer

5. SYSTEM

* Presets
* Browser
* Export
* Settings

Blok SYSTEM powinien być oddzielony wizualnie od pozostałych i umieszczony przy dolnej krawędzi panelu.

2.2. Elementy stale widoczne

Niezależnie od wybranego modułu na ekranie powinny pozostawać:

* Nazwa aktualnego presetu.
* Wybór stylu kicka.
* Wybór długości kicka.
* Główne makra.
* Master Output.
* BPM hosta.
* Przycisk odsłuchu.
* Przycisk generowania losowego kicka.
* Wskaźnik poziomu sygnału.
* Przycisk eksportu.

Główny waveform powinien być dostępny w sekcji KICK i w uproszczonej formie stale widoczny w dolnym lub górnym panelu.

3. STYLE ENGINE — CZTERY GŁÓWNE CHARAKTERY

Użytkownik wybiera jeden z czterech podstawowych silników brzmieniowych.

Wybór stylu ustawia domyślne wartości parametrów, routing, charakterystykę pitch envelope, proporcje warstw i typowe ustawienia distortion. Nie może jednak blokować ręcznej edycji żadnego parametru.

3.1. EARLY HARDSTYLE

Cel: klasyczny charakter kicków z okresu wczesnego hardstyle, z wyraźnym uderzeniem, tonalnym ogonem i charakterystycznym, często bardziej otwartym brzmieniem distortion.

Charakterystyka:

* Mocny, czytelny punch.
* Wyraźny pitch drop.
* Tonalne body.
* Charakterystyczny distortion, ale bez automatycznego przesadnego przesterowania.
* Wyraźna separacja punchu i taila.
* Możliwość uzyskania krótkich, sprężystych oraz dłuższych, melodycznych kicków.

Domyślne ustawienia:

* Punch: mocny.
* Body: średnie lub mocne.
* Distortion: umiarkowany.
* Pitch Drop: wyraźny.
* Tail: tonalny.
* Stereo: ograniczone.
* Sub: mono.

Dodatkowe charakterystyczne kontrolki:

* CLASSIC PUNCH
* OLD SCHOOL DRIVE
* TAIL TONE
* EARLY COLOR

Nie należy ograniczać EARLY do jednej konkretnej epoki lub jednego brzmienia. Ma to być szeroki silnik pozwalający uzyskiwać różne klasyczne warianty.

3.2. MODERN HARDSTYLE

Cel: nowoczesny, mocny, klarowny kick o dużej energii, precyzyjnym uderzeniu i kontrolowanym, często bardziej złożonym ogonie.

Charakterystyka:

* Bardzo wyraźny transient.
* Mocny i kontrolowany sub.
* Precyzyjny pitch envelope.
* Wielowarstwowe body.
* Nowoczesne harmoniczne.
* Możliwość tworzenia jasnych, agresywnych taili.
* Kontrolowana szerokość stereo w wyższych częstotliwościach.

Domyślne ustawienia:

* Punch: mocny.
* Body: mocne.
* Distortion: wieloetapowy.
* Pitch Drop: precyzyjny.
* Tail: warstwowy.
* Sub: mocny i mono.
* Stereo: kontrolowane.

Dodatkowe kontrolki:

* MODERN IMPACT
* BODY DENSITY
* HARMONIC BRIGHTNESS
* TAIL PRESENCE

3.3. RAWSTYLE

Cel: bardzo agresywne, szorstkie, mocno przesterowane kicki z charakterystycznym rezonansem, dużą ilością harmonicznych i możliwością uzyskania screechowych ogonów.

Charakterystyka:

* Agresywny punch.
* Mocne distortion.
* Wyraźne rezonanse.
* Możliwość uzyskania screechowego taila.
* Duża kontrola nad harmonicznymi.
* Możliwość tworzenia kicków metalicznych, ostrych, brudnych i przesterowanych.
* Mocne, charakterystyczne body.

Dodatkowe kontrolki:

* RAW DRIVE
* SCREAM
* RESONANCE
* GRIT
* METAL TONE

Silnik RAWSTYLE powinien umożliwiać tworzenie zarówno niskich, ciężkich kicków, jak i jasnych, bardzo agresywnych taili.

3.4. EXPERIMENTAL

Cel: tworzenie niestandardowych kicków, których brzmienie nie musi odpowiadać klasycznym schematom hardstyle.

Dostępne możliwości:

* Nietypowe pitch envelope.
* Ekstremalne modulacje.
* Wavefolding.
* Frequency modulation.
* Ring modulation.
* Granularne przetwarzanie sampli.
* Niestandardowe krzywe amplitudy.
* Niekonwencjonalne połączenia warstw.
* Rytmiczne modulacje taila.
* Zmienna faza i niestandardowe przebiegi oscylatorów.

Dodatkowe kontrolki:

* CHAOS
* EXPERIMENT
* FM AMOUNT
* MOVEMENT
* DESTRUCTION

Tryb EXPERIMENTAL nie może oznaczać przypadkowego generowania hałasu. Wszystkie procesy muszą być kontrolowane i możliwe do odtworzenia.

4. KICK LENGTH ENGINE — DŁUGOŚĆ KICKA

To jeden z najważniejszych elementów 4GE.

Użytkownik musi mieć możliwość wyboru długości kicka niezależnie od wybranego stylu.

4.1. Przełącznik długości

Dostępne opcje:

SHORT

* Krótkie, zwarte kicki.
* Szybki tail.
* Mocne uderzenie.
* Mniej zajętego miejsca w aranżacji.
* Dobre do szybkich, dynamicznych patternów.

MEDIUM

* Standardowa długość.
* Wyraźne uderzenie i czytelny tail.
* Uniwersalne zastosowanie.

LONG

* Długi, rozwinięty tail.
* Więcej miejsca na tonalne i harmoniczne elementy.
* Dobre do melodycznych kicków i dłuższych fraz.

EXTRA LONG

* Bardzo długie ogony.
* Rozbudowane pitch movement.
* Możliwość tworzenia ekstremalnych, eksperymentalnych brzmień.

4.2. Parametry Length Engine

* LENGTH: całkowita docelowa długość kicka.
* PUNCH LENGTH: długość fazy punchu.
* BODY LENGTH: długość fazy body.
* TAIL LENGTH: długość fazy taila.
* TAIL FADE: czas wygaszania.
* RELEASE: czas końcowego wygaszenia.
* PRE-TAIL: czas przejścia pomiędzy body i tailem.
* AUTO LENGTH: automatyczne dopasowanie długości do wybranego profilu.
* LENGTH SCALE: globalne skalowanie czasu wszystkich faz.

Długość może być podawana w milisekundach lub wartościach rytmicznych względem BPM.

Przykładowe wartości profili mają być punktami startowymi, a nie sztywnymi ograniczeniami:

* SHORT: około 100–250 ms.
* MEDIUM: około 250–500 ms.
* LONG: około 500–1000 ms.
* EXTRA LONG: około 1000–2500 ms.

Wartości powinny być edytowalne, a maksymalna długość powinna być konfigurowalna z uwzględnieniem pamięci i wydajności.

4.3. Ważne zachowanie

Zmiana profilu długości ma skalować odpowiednie obwiednie czasowe, ale nie może bezmyślnie rozciągać wszystkich elementów w ten sam sposób.

Przykładowo:

* Punch może pozostać krótki nawet przy EXTRA LONG.
* Body może zostać umiarkowanie wydłużone.
* Tail może zostać wydłużony niezależnie.
* Transient nie powinien automatycznie tracić swojej ostrości.

Użytkownik musi móc wyłączyć AUTO LENGTH i ręcznie kontrolować każdy czas.

5. MAIN KICK ENGINE

Główna sekcja do szybkiego budowania kicka.

Powinna zawierać:

* STYLE.
* LENGTH.
* PUNCH.
* BODY.
* TAIL.
* PITCH.
* DISTORTION.
* SUB.
* MASTER.

W centralnej części:

* Waveform całego kicka.
* Widok amplitudy w czasie.
* Widok pitch envelope.
* Podgląd spektrum.
* Możliwość odsłuchu pojedynczego kicka.
* Możliwość odsłuchu aktualnej warstwy.

Użytkownik powinien móc kliknąć dowolny segment waveformu i przejść bezpośrednio do jego edycji.

6. PUNCH ENGINE

Punch odpowiada za pierwszą fazę kicka, czyli początkowe uderzenie.

Musi być oddzielony od body i taila.

6.1. Punch Generator

Typy źródła:

* Sine.
* Triangle.
* Saw.
* Square.
* Noise.
* Click.
* Imported Sample.
* Custom Waveform.

Parametry:

* PUNCH LEVEL.
* PUNCH PITCH.
* PUNCH LENGTH.
* PUNCH DECAY.
* PUNCH ATTACK.
* PUNCH TONE.
* PUNCH DRIVE.
* PUNCH FILTER.
* PUNCH CLICK.
* PUNCH PHASE.
* PUNCH MIX.

6.2. Punch Envelope

Osobna edycja obwiedni:

* Attack.
* Hold.
* Decay.
* Sustain.
* Release.

Dodatkowo:

* Curve.
* Velocity Sensitivity.
* One Shot.
* Retrigger.

Użytkownik powinien móc rysować własną krzywą amplitudy.

6.3. Punch Layer

Możliwość dodania maksymalnie kilku warstw punchu.

Każda warstwa musi mieć:

* Własne źródło.
* Gain.
* Pitch.
* Start Offset.
* Length.
* Filter.
* Distortion.
* Phase.
* Pan.
* Mute/Solo.

7. BODY ENGINE

Body jest środkową częścią kicka, łączącą punch z tailiem.

Powinno być projektowane niezależnie.

Parametry:

* BODY LEVEL.
* BODY PITCH.
* BODY LENGTH.
* BODY DECAY.
* BODY TONE.
* BODY DRIVE.
* BODY RESONANCE.
* BODY FILTER.
* BODY SATURATION.
* BODY MIX.

Źródła:

* Oscillator.
* Sample.
* Noise.
* Punch Tail.
* Imported Layer.

Body powinno mieć własny pitch envelope i amplitudę.

Dodatkowo:

* Body Harmonics.
* Body Width.
* Body Phase.
* Body Transient.
* Body Compression.

Użytkownik musi mieć możliwość zbudowania gładkiego przejścia pomiędzy body i tailem albo uzyskania wyraźnego podziału między nimi.

8. TAIL ENGINE

Tail to jedna z najważniejszych części kicka hardstyle. Musi posiadać rozbudowany system syntezy i przetwarzania.

8.1. Tail Generator

Źródła:

* Sine.
* Saw.
* Square.
* Triangle.
* Pulse.
* Noise.
* Custom Wavetable.
* Sample.
* FM Oscillator.
* Imported Tail.

Parametry:

* TAIL LEVEL.
* TAIL PITCH.
* TAIL LENGTH.
* TAIL DECAY.
* TAIL DRIVE.
* TAIL TONE.
* TAIL RESONANCE.
* TAIL FILTER.
* TAIL DISTORTION.
* TAIL MIX.
* TAIL WIDTH.

8.2. Tail Character

Wybór charakteru:

* CLEAN.
* TONAL.
* DIRTY.
* SCREAM.
* METALLIC.
* BUZZ.
* CRUSHED.
* NOISY.
* CUSTOM.

Wybór charakteru powinien ustawiać odpowiednie parametry początkowe, ale nie może ograniczać edycji.

8.3. Tail Envelope

Osobne obwiednie:

* Amplitude.
* Pitch.
* Filter.
* Distortion.
* Resonance.
* Stereo Width.

Każda obwiednia powinna mieć edytor krzywej, możliwość synchronizacji z BPM oraz niezależne ustawienie czasu.

9. PITCH ENGINE

Silnik odpowiedzialny za zmianę wysokości dźwięku kicka w czasie.

To kluczowy moduł dla uzyskania charakterystycznego hardstyle kicka.

9.1. Pitch Envelope

Parametry:

* START PITCH.
* END PITCH.
* PITCH DROP.
* PITCH RISE.
* PITCH TIME.
* PITCH CURVE.
* PITCH AMOUNT.
* PITCH HOLD.
* PITCH RELEASE.
* PITCH SMOOTH.

Możliwość rysowania własnej krzywej pitch.

Krzywa powinna obsługiwać:

* Dowolną liczbę punktów.
* Przeciąganie punktów.
* Regulację krzywizny.
* Dodawanie i usuwanie punktów.
* Snap do siatki.
* Kopiowanie i wklejanie krzywych.
* Presety krzywych.

9.2. Pitch Modes

* CLASSIC DROP.
* FAST DROP.
* SLOW DROP.
* DOUBLE DROP.
* RISE.
* DROP + RISE.
* STABLE TONE.
* CUSTOM.

9.3. Tuning

* Root Note.
* Semitone.
* Fine Tune.
* Octave.
* MIDI Note Tracking.
* Key Lock.
* Pitch Quantization.

Użytkownik musi wiedzieć, jaka jest podstawowa wysokość tonalna kicka.

Wtyczka powinna umożliwiać analizę przybliżonej częstotliwości podstawowej, ale nie może zakładać, że każdy przesterowany kick ma jednoznaczną wysokość.

10. OSCILLATOR ENGINE

Główny generator dźwięku.

Co najmniej cztery niezależne oscylatory.

Każdy oscylator:

* Waveform.
* Volume.
* Octave.
* Semitone.
* Fine Tune.
* Phase.
* Phase Random.
* Unison.
* Detune.
* Stereo Spread.
* Pulse Width.
* Sync.
* FM Amount.
* Ring Mod Amount.
* Pan.
* Enable/Disable.

Dostępne przebiegi:

* Sine.
* Saw.
* Square.
* Triangle.
* Pulse.
* Noise.
* Wavetable.
* Custom.

Każdy oscylator powinien mieć własną obwiednię amplitudy i możliwość przypisania do Punch, Body lub Tail.

11. NOISE ENGINE

Generator szumu do tworzenia ataku, tekstury i agresywnego charakteru.

Typy:

* White Noise.
* Pink Noise.
* Brown Noise.
* Filtered Noise.
* Metallic Noise.
* Digital Noise.
* Custom Noise.

Parametry:

* Noise Level.
* Noise Color.
* Filter Cutoff.
* Resonance.
* Decay.
* Attack.
* Distortion.
* Mix.
* Stereo Width.

Noise powinien móc działać jako osobna warstwa albo być przypisany do punchu, body i taila.

12. TRANSIENT ENGINE

Moduł odpowiedzialny za ostrość i czytelność pierwszego uderzenia.

Parametry:

* TRANSIENT AMOUNT.
* ATTACK BOOST.
* ATTACK REDUCE.
* SUSTAIN.
* CLICK LEVEL.
* CLICK TONE.
* CLICK LENGTH.
* CLICK DECAY.
* HIGH TRANSIENT.
* LOW TRANSIENT.
* MIX.

Powinien umożliwiać:

* Wzmacnianie pierwszego uderzenia.
* Redukowanie zbyt ostrego ataku.
* Dodawanie warstwy click.
* Kontrolowanie wysokich i niskich transientów osobno.

13. SUB ENGINE

Oddzielny generator niskich częstotliwości.

Typy:

* Sine.
* Triangle.
* Clean Oscillator.
* Filtered Body.
* Sub Sample.

Parametry:

* SUB LEVEL.
* SUB PITCH.
* SUB OCTAVE.
* SUB PHASE.
* SUB LENGTH.
* SUB ATTACK.
* SUB DECAY.
* SUB SATURATION.
* SUB DRIVE.
* SUB FILTER.
* SUB MIX.

Dodatkowe funkcje:

* Mono Sub.
* Phase Alignment.
* Sub Ducking.
* Sub Crossover.
* Sub Harmonics.
* Sub Solo.

Domyślnie sub powinien być mono. Stereo dla niskich częstotliwości musi być opcjonalne i kontrolowane.

14. DISTORTION ENGINE

Jeden z najważniejszych modułów 4GE.

Powinien umożliwiać tworzenie klasycznego, nowoczesnego i ekstremalnego distortion.

14.1. Typy distortion

* Soft Clip.
* Hard Clip.
* Tube Saturation.
* Diode Distortion.
* Wavefolding.
* Bit Crush.
* Rectifier.
* Asymmetric Distortion.
* Digital Clipping.
* Foldback.
* Custom Waveshaper.

14.2. Parametry

* DRIVE.
* THRESHOLD.
* BIAS.
* SYMMETRY.
* HARDNESS.
* MIX.
* OUTPUT.
* TONE.
* PRE-FILTER.
* POST-FILTER.
* OVERSAMPLING.

14.3. Multi-stage Distortion

Minimum cztery niezależne stopnie distortion.

Każdy stopień powinien posiadać:

* Type.
* Drive.
* Tone.
* Mix.
* Bias.
* Filter.
* Output.
* Enable/Disable.

Możliwość wyboru routingu:

* Serial.
* Parallel.
* Split Frequency.
* Custom Routing.

Użytkownik musi mieć możliwość ustawienia różnych typów distortion dla Punch, Body i Tail.

14.4. Waveshaper Editor

Graficzny edytor krzywej przesterowania.

Funkcje:

* Dodawanie punktów.
* Przesuwanie punktów.
* Regulacja krzywizny.
* Reset.
* Symmetry.
* Mirror.
* Preset Shapes.
* Undo/Redo.

Wszystkie ustawienia muszą faktycznie wpływać na algorytm DSP.

15. FILTER ENGINE

Filtry do kontroli barwy i rezonansu.

Typy:

* Low Pass.
* High Pass.
* Band Pass.
* Notch.
* Ladder.
* Diode.
* State Variable.
* Comb.
* Formant.

Parametry:

* Cutoff.
* Resonance.
* Drive.
* Mix.
* Slope.
* Key Tracking.
* Envelope Amount.
* Filter Modulation.

Możliwość umieszczenia filtra:

* Przed distortion.
* Po distortion.
* Pomiędzy stopniami distortion.
* Na osobnej warstwie.

16. RESONANCE ENGINE

Oddzielny moduł do projektowania rezonansów.

Parametry:

* Resonance Frequency.
* Resonance Amount.
* Resonance Decay.
* Resonance Width.
* Resonance Drive.
* Resonance Pitch.
* Resonance Mix.

Tryby:

* CLEAN.
* METALLIC.
* SCREAM.
* RUBBER.
* CUSTOM.

Moduł powinien umożliwiać dodanie kontrolowanego rezonansu do taila bez konieczności przesterowywania całego sygnału.

17. SAMPLE ENGINE

Użytkownik musi móc importować własne próbki.

Obsługiwane formaty:

* WAV.
* AIFF.

Obsługiwane typy:

* Mono.
* Stereo.
* 16-bit.
* 24-bit.
* 32-bit float.

Funkcje:

* Import Sample.
* Sample Start.
* Sample End.
* Trim.
* Normalize.
* Reverse.
* Fade In.
* Fade Out.
* Gain.
* Pitch.
* Time Stretch.
* Loop.
* Root Note.
* Sample Playback Mode.

Sample Modes:

* Original.
* Pitch Processed.
* Distorted.
* Filtered.
* Granular.
* Hybrid.

Sample może być przypisany do:

* Punch.
* Body.
* Tail.
* Transient.
* Sub.

Wymagany jest waveform editor z możliwością ustawiania punktów startu i końca.

18. LAYER ENGINE

Możliwość łączenia wielu źródeł w jeden kick.

Minimum osiem niezależnych warstw.

Każda warstwa:

* Source.
* Gain.
* Pan.
* Pitch.
* Length.
* Start Offset.
* Phase.
* Filter.
* Distortion.
* EQ.
* Envelope.
* Stereo Width.
* Mute.
* Solo.

Każda warstwa może być przypisana do jednej z grup:

* Punch.
* Body.
* Tail.
* Sub.
* Texture.
* Custom.

Wtyczka musi umożliwiać odsłuch każdej warstwy osobno.

19. ENVELOPE ENGINE

Centralny system obwiedni.

Oddzielne obwiednie dla:

* Amplitude.
* Pitch.
* Filter.
* Distortion.
* Resonance.
* Stereo Width.
* FM.
* Noise.

Każda obwiednia:

* Attack.
* Hold.
* Decay.
* Sustain.
* Release.
* Curve.
* Amount.
* Sync.
* One Shot.
* Retrigger.

Wymagany jest graficzny edytor wielopunktowy.

20. MODULATION ENGINE

Minimum cztery niezależne LFO.

Typy:

* Sine.
* Triangle.
* Saw.
* Square.
* Pulse.
* Random.
* Sample & Hold.
* Custom.

Parametry:

* Rate.
* Depth.
* Phase.
* Offset.
* Smooth.
* Retrigger.
* Sync.
* Polarity.

Modulation Matrix:

Użytkownik wybiera:

* Source.
* Destination.
* Amount.
* Polarity.
* Curve.
* Enable/Disable.

Przykładowe zastosowania:

* LFO moduluje cutoff.
* Envelope moduluje distortion.
* Pitch Envelope moduluje FM.
* LFO moduluje resonance.
* Random moduluje wavetable position.

Modulacje muszą być deterministyczne w trybie renderowania i eksportu, chyba że użytkownik włączy kontrolowaną randomizację.

21. SEQUENCER

Opcjonalny sekwencer do projektowania patternów kicków i testowania brzmienia w kontekście rytmicznym.

Parametry:

* 16–32 kroki.
* Step On/Off.
* Velocity.
* Probability.
* Pitch.
* Gate.
* Length.
* Accent.
* Swing.
* Humanize.

Funkcje:

* BPM Sync.
* Pattern Save.
* Pattern Load.
* Random Pattern.
* Copy/Paste.
* MIDI Export.

Sekwencer nie może być wymagany do normalnej pracy instrumentu. Wtyczka musi działać również jako pojedynczy instrument wyzwalany przez MIDI.

22. EQ ENGINE

Minimum ośmiopasmowy equalizer parametryczny.

Każde pasmo:

* Frequency.
* Gain.
* Q.
* Type.
* Enable/Disable.

Typy:

* Bell.
* Low Shelf.
* High Shelf.
* Low Cut.
* High Cut.
* Notch.

Dodatkowo:

* Spectrum Analyzer.
* Pre/Post EQ.
* Mid/Side EQ.
* Per-Layer EQ.

23. DYNAMICS ENGINE

Moduł kontroli dynamiki.

Elementy:

* Compressor.
* Limiter.
* Clipper.
* Transient Processor.
* Peak Meter.
* RMS Meter.

Parametry kompresora:

* Threshold.
* Ratio.
* Attack.
* Release.
* Knee.
* Makeup Gain.
* Mix.

Parametry limitera:

* Ceiling.
* Release.
* Lookahead.
* Input Gain.
* Output Gain.

Limiter nie może automatycznie niszczyć dynamiki kicka. Powinien być opcjonalny i mieć możliwość wyłączenia.

24. STEREO ENGINE

Parametry:

* Width.
* Pan.
* Stereo Spread.
* Mid/Side.
* Micro Detune.
* Phase Offset.
* Mono Below Frequency.

Domyślnie:

* Sub mono.
* Punch stabilny i najlepiej centralny.
* Stereo można rozszerzać na wyższych warstwach i częstotliwościach.

Dodatkowo:

* Mono Check.
* Phase Correlation Meter.
* Stereo Analyzer.

Nie wolno używać niekontrolowanego przesunięcia fazowego jako domyślnego sposobu poszerzania kicka.

25. MACRO ENGINE

Osiem głównych makr widocznych niezależnie od aktualnie otwartej sekcji.

Proponowane makra:

1. IMPACT
Kontroluje siłę punchu, transient i początkowy poziom kicka.

2. BODY
Kontroluje gęstość i poziom środkowej części kicka.

3. TAIL
Kontroluje poziom, długość i charakter ogona.

4. PITCH
Kontroluje intensywność zmian wysokości dźwięku.

5. GRIT
Kontroluje stopień zabrudzenia i nasycenia.

6. SCREAM
Kontroluje agresywność, rezonans i jasność taila.

7. WEIGHT
Kontroluje masę niskich częstotliwości i sub.

8. CHAOS
Kontroluje wybrane parametry eksperymentalne, takie jak FM, modulacje i niestandardowe krzywe.

Każde makro musi mieć możliwość przypisania do wielu parametrów.

Użytkownik musi móc ustawić:

* Minimum.
* Maximum.
* Direction.
* Curve.
* Modulation Amount.

Makra nie mogą zmieniać niepowiązanych parametrów.

26. RANDOMIZER ENGINE

Generator nowych kicków.

Tryby:

* Full Random.
* Style Random.
* Punch Random.
* Body Random.
* Tail Random.
* Distortion Random.
* Pitch Random.
* Experimental Random.

Funkcja LOCK:
Użytkownik może zablokować wybrane elementy, aby randomizer ich nie zmieniał.

Przykład:

* Punch zablokowany.
* Sub zablokowany.
* Tail odblokowany.
* Distortion odblokowany.

Randomizer musi umożliwiać:

* Ustawienie intensywności losowania.
* Ustawienie zakresu parametrów.
* Zachowanie aktualnego stylu.
* Zachowanie długości kicka.
* Cofnięcie ostatniej operacji.
* Zapisanie wygenerowanego kicka jako presetu.

Każdy wynik randomizacji powinien być możliwy do odtworzenia przez zapisanie seeda i parametrów.

27. PRESET MANAGER

System presetów.

Kategorie:

* Early.
* Modern.
* Raw.
* Experimental.
* Short.
* Medium.
* Long.
* Extra Long.
* Punch.
* Tonal.
* Screech.
* Heavy.
* Clean.
* Distorted.

Funkcje:

* Save.
* Save As.
* Rename.
* Delete.
* Favorite.
* Search.
* Tags.
* Category.
* Preview.
* Import.
* Export.

Presety fabryczne powinny pokazywać charakter kicka i jego długość.

Każdy preset musi zawierać komplet ustawień potrzebnych do odtworzenia brzmienia.

28. EXPORT ENGINE

Możliwość eksportowania kicków do WAV bez konieczności nagrywania ich w DAW.

Ustawienia:

* Sample Rate: 44.1 / 48 / 88.2 / 96 / 176.4 / 192 kHz.
* Bit Depth: 16 / 24 / 32-bit float.
* Mono / Stereo.
* Normalize.
* Peak Target.
* Tail Render.
* Render Quality.
* Oversampling.
* Filename.
* Export Folder.

Dodatkowe funkcje:

* Export Single Kick.
* Export Multiple Presets.
* Batch Export.
* Export All Variations.
* Export Dry.
* Export Processed.
* Export Layer Stems.

Użytkownik powinien móc wyeksportować osobno:

* Punch.
* Body.
* Tail.
* Sub.
* Full Kick.

Eksport musi uwzględniać pełny czas wygaszania sygnału, aby nie ucinać ogona.

29. AUDIO ANALYSIS

Wtyczka powinna zawierać narzędzia do analizy brzmienia.

Elementy:

* Waveform Display.
* Spectrum Analyzer.
* Peak Meter.
* RMS Meter.
* Phase Meter.
* Frequency Display.
* Fundamental Frequency Estimation.
* Loudness Display.

Dodatkowo:

* A/B Comparison.
* Before/After.
* Reference Sample Import.
* Reference Spectrum.
* Gain Matching.

Reference Sample służy do porównania, nie do automatycznego kopiowania nieznanych parametrów.

30. SIGNAL FLOW

Zalecany podstawowy routing:

1. MIDI Trigger / Internal Trigger.
2. Pitch Engine.
3. Punch Generator.
4. Body Generator.
5. Tail Generator.
6. Sub Generator.
7. Layer Mixer.
8. Pre-Processing.
9. Distortion Stages.
10. Filter / Resonance.
11. EQ.
12. Dynamics.
13. Stereo Engine.
14. Master Output.

Routing musi być konfigurowalny w zaawansowanym trybie.

Punch, Body, Tail i Sub powinny posiadać niezależne ścieżki przetwarzania przed połączeniem w głównym mikserze.

31. DSP I ARCHITEKTURA TECHNICZNA

Zalecana technologia:

* C++.
* JUCE.
* VST3 jako obowiązkowy format.
* Standalone jako dodatkowa wersja.
* AU jako opcjonalne rozszerzenie dla macOS.

Wymagania DSP:

* Sample-rate independent processing.
* Parameter smoothing.
* Anti-aliasing.
* DC offset protection.
* Denormal protection.
* Oversampling.
* Real-time safe processing.
* No memory allocation in audio callback.
* No blocking operations in audio callback.
* Stabilne przetwarzanie przy zmianie parametrów.
* Poprawna synchronizacja z hostem.

Oversampling:

* 1x.
* 2x.
* 4x.
* 8x.
* Opcjonalnie 16x w trybie najwyższej jakości.

Najbardziej wymagające procesory distortion i waveshaper powinny korzystać z oversamplingu.

Wtyczka powinna poprawnie obsługiwać:

* MIDI Note On.
* MIDI Note Off.
* Velocity.
* Host BPM.
* Host Transport.
* Host Automation.
* Preset State.
* Plugin State Restore.

32. MIDI I AUTOMATION

Każdy istotny parametr musi być dostępny dla automatyzacji w DAW.

Wymagania:

* Stabilne Parameter IDs.
* MIDI Learn.
* Host Automation.
* Velocity Mapping.
* Macro Mapping.
* Preset State Save/Restore.

Zmiana presetu nie może powodować niekontrolowanych skoków parametrów podczas odtwarzania.

33. GUI I DESIGN SYSTEM

Kierunek graficzny:

* Industrial.
* Dark.
* Techniczny.
* Nowoczesny.
* Profesjonalny.

Kolory:

* Graphite.
* Black.
* Dark Grey.
* Red jako kolor akcentu.
* Jasny szary dla tekstów i wartości.

Interfejs powinien wyglądać jak profesjonalne oprogramowanie audio, a nie jak gra komputerowa.

Wymagania:

* Skalowanie GUI.
* HiDPI.
* Minimum 75–200% UI Scale.
* Tooltips.
* Manual Value Entry.
* Double Click Reset.
* Undo/Redo.
* Drag & Drop.
* Czytelne wykresy.
* Responsywny układ.

Wykresy muszą umożliwiać bezpośrednią edycję krzywych.

34. DODATKOWE FUNKCJE PROFESJONALNE

34.1. A/B KICK COMPARISON

Możliwość porównywania dwóch wersji kicka bez konieczności ręcznego zapisywania presetów.

34.2. KICK MORPH

Płynne przechodzenie pomiędzy dwoma zestawami parametrów.

Przykładowo:

* Early Kick → Raw Kick.
* Short Kick → Long Kick.
* Clean Tail → Distorted Tail.

Morph musi interpolować parametry w sposób kontrolowany. Parametry dyskretne powinny zmieniać się zgodnie z określoną logiką, a nie powodować przypadkowych przeskoków.

34.3. LAYER MATCHING

Pomocnicze dopasowanie fazy i poziomu warstw, aby uniknąć niepożądanego znoszenia się sygnałów.

34.4. KICK HISTORY

Lista ostatnio odsłuchiwanych lub wygenerowanych wersji kicka.

34.5. SNAPSHOT

Zapisywanie tymczasowych stanów parametrów i szybki powrót do nich.

35. ZASADY DZIAŁANIA — KRYTYCZNE DLA PROGRAMISTY

1. Wybór stylu nie może blokować edycji parametrów.
2. Wybór długości nie może automatycznie zmieniać stylu.
3. Punch, Body, Tail i Sub muszą być niezależnymi elementami.
4. Każda warstwa musi mieć możliwość odsłuchu Solo.
5. Każda kontrolka musi faktycznie wpływać na DSP.
6. Nie wolno tworzyć dekoracyjnych kontrolek bez działania.
7. Preset musi odtwarzać dokładnie zapisane ustawienia.
8. Randomizer musi respektować blokady LOCK.
9. Kick musi być poprawnie renderowany offline i realtime.
10. Eksport nie może ucinać taila.
11. Zmiana długości musi zachowywać kontrolę nad punch transientem.
12. Wartości parametrów muszą być ograniczone do bezpiecznych zakresów.
13. Nie wolno dopuszczać do niekontrolowanego DC offsetu ani niebezpiecznych skoków poziomu.
14. Wszystkie parametry powinny być opisane i mieć sensowne wartości domyślne.

36. TESTOWANIE I KONTROLA JAKOŚCI

Testy muszą obejmować:

Dźwięk

* Czy kick ma prawidłowy punch?
* Czy pitch envelope działa poprawnie?
* Czy tail nie jest przypadkowo ucinany?
* Czy distortion nie generuje niekontrolowanego aliasingu?
* Czy sub pozostaje stabilny?
* Czy warstwy nie powodują niepożądanych problemów fazowych?

Funkcjonalność

* Czy każdy parametr działa?
* Czy presety zapisują i przywracają stan?
* Czy randomizer respektuje LOCK?
* Czy MIDI działa?
* Czy automatyzacja działa?
* Czy eksport odpowiada temu, co słychać w DAW?

Wydajność

* Czy wtyczka nie powoduje trzasków?
* Czy CPU jest kontrolowane?
* Czy zmiana presetu nie blokuje audio?
* Czy oversampling działa poprawnie?
* Czy plugin zachowuje stabilność przy różnych częstotliwościach próbkowania i rozmiarach bufora?

37. PLAN BUDOWY — KOLEJNOŚĆ IMPLEMENTACJI

Nie należy próbować programować wszystkich funkcji jednocześnie. Projekt powinien być podzielony na etapy.

ETAP 1 — CORE KICK ENGINE

* Główny generator.
* Punch.
* Body.
* Tail.
* Sub.
* Pitch Envelope.
* Amplitude Envelope.
* Short / Medium / Long / Extra Long.
* Podstawowy waveform editor.
* MIDI Trigger.
* Podstawowy eksport WAV.

Cel: działający instrument, który potrafi generować kick od zera.

ETAP 2 — STYLE ENGINES

* Early.
* Modern.
* Raw.
* Experimental.
* Domyślne profile.
* Niezależna edycja każdego stylu.
* Presety startowe.

Cel: cztery różne charaktery brzmieniowe.

ETAP 3 — SOUND DESIGN

* Oscillators.
* Noise.
* Transient.
* Layer Engine.
* Sample Engine.
* Sub Engine.
* Resonance.

Cel: pełna kontrola nad źródłami dźwięku.

ETAP 4 — PROCESSING

* Distortion.
* Multi-stage Distortion.
* Waveshaper.
* Filter.
* EQ.
* Dynamics.
* Stereo.

Cel: profesjonalne przetwarzanie brzmienia.

ETAP 5 — MODULATION

* LFO.
* Modulation Matrix.
* Envelope Extensions.
* Sequencer.
* Randomizer.
* Macro Engine.

Cel: zaawansowane możliwości projektowania.

ETAP 6 — PROFESSIONAL TOOLS

* Preset Browser.
* A/B.
* Morph.
* Reference Analysis.
* Batch Export.
* Layer Stems.
* Kick History.
* Snapshot.

Cel: dopracowany produkt komercyjny.

ETAP 7 — RELEASE

* Testy DAW.
* Testy CPU.
* Testy stabilności.
* Instalator.
* Dokumentacja.
* Presety fabryczne.
* Finalne GUI.
* VST3 Release.

38. MASTER PROMPT DLA AI PROGRAMISTY

Zbuduj profesjonalną wtyczkę audio o nazwie 4GE – Hardstyle Kick Designer.

Projekt ma być kompletnym syntezatorem kicków hardstyle, a nie prostym generatorem próbek. Głównym celem jest umożliwienie użytkownikowi tworzenia kicków Early Hardstyle, Modern Hardstyle, Rawstyle i Experimental od podstaw.

Zaprojektuj architekturę w C++ z wykorzystaniem JUCE. VST3 jest obowiązkowym formatem docelowym.

Najważniejsze wymagania:

* Niezależne silniki Punch, Body, Tail i Sub.
* Niezależny wybór stylu i długości kicka.
* Profile Short, Medium, Long i Extra Long.
* Zaawansowany Pitch Envelope Editor.
* Wielowarstwowa synteza.
* Oscillators i Sample Engine.
* Zaawansowany Distortion Engine.
* Multi-stage Distortion.
* Waveshaper.
* Filter i Resonance Engine.
* Transient Engine.
* Sub Engine.
* Modulation Engine.
* Macro Engine.
* Randomizer z LOCK.
* Preset Manager.
* WAV Export.
* MIDI i Host Automation.
* Profesjonalne GUI z pięcioma blokami nawigacyjnymi.

Nie implementuj wszystkich funkcji jako atrap. Każda kontrolka musi być połączona z odpowiednim parametrem DSP.

Pracuj etapami. Najpierw zbuduj działający Core Kick Engine, a dopiero potem dodawaj kolejne moduły. Po każdym etapie kompiluj projekt, testuj dźwięk i poprawiaj błędy.

Architektura ma być modułowa, czytelna, skalowalna i przygotowana do rozwoju produktu komercyjnego.

Nie upraszczaj kluczowych algorytmów syntezy i distortion do zwykłego odtwarzania sampla. 4GE ma umożliwiać rzeczywiste projektowanie kicka, a nie tylko nakładanie efektów na gotowy dźwięk.