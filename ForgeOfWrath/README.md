# FORGE OF WRATH v2.1

**v2.1 – REVERSE BASS** (pomysł z dkd Damiana): każdy kick odpala jeden beat basu na tej samej nucie, w tempie FL.
Bas narasta między kickami, na końcu podjeżdża w górę (jak odwrócony ogon kicka), a przed następnym kickiem jest czysta przerwa.
Włącznik ON, kroki **1/4 / 1/8 / 1/16**, **IN / OUT** (gdzie w kroku zaczyna i kończy), **CURVE** (wcześnie / późno), **RISE** (podjazd),
**TUNE** (względem nuty kicka), **DRIVE**, **TONE**, **LEVEL**. Okienko obok pokazuje jeden beat: kick (jego LENGTH) i bas –
napis **OVERLAP**, gdy kick wchodzi na bas (skróć LENGTH albo przesuń IN). Presety: BASS - Euphoric Kick + Reverse Bass, BASS - Raw Kick + Dirty Reverse.
**PITCH + FINE w sekcji TAIL** stroją sam ogon (półtony + centy); PUNCH i CLICK zostają na swoich wysokościach.
**PUNCH → INTO DIST** (przełącznik w sekcji PUNCH, w INIT włączony): punch idzie przez ten sam przester co ogon, więc brzmi jak
początek taila – ale jego wysokość ustawiają tylko START / END punchu, nie nuta. Wyłączony = czysty sinus na wierzchu (jak w v2.0).
Twoje kicki: **OWNER - Nice / Nice 1 / Sgsdg** (dokładnie tak, jak je ustawiłeś; mają szczyt trochę ponad 0 dB – OUTPUT −1.5 dB, jeśli przeszkadza).

**v2:** łańcuch STAGE 1 → MID EQ → STAGE 2, ROOM (INTO DIST), OTT, rysowany PITCH, PUNCH DRIVE, CHAOS, HOLD, własny SAMPLE (LOAD / przeciągnij WAV), **CLEAN SUB** (poniżej wybranej częstotliwości czysty sinus zamiast przesteru – ciasny dół, clipper go nie rusza).
Syntezator kicków hardstyle (instrument VST3, FL Studio). Kick wychodzi gotowy prosto z wtyczki – bez dokładania
przesterów, EQ i clipperów w mikserze. Czarny + magma.

## Instalacja (Windows)
Zakładka **Actions** → *Build Forge of Wrath VST3* → **Artifacts** → `Forge-of-Wrath-VST3-Windows`.
Folder `Forge of Wrath.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.
Wstaw jako **instrument** (Channel Rack), wpisz nuty w Piano Roll.

## Jak zacząć (3 kroki)
1. Wybierz preset (**RAW**, **HARD**, **EUPHORIC**, **REVERSE**…) i kliknij **PLAY** – usłyszysz kick na G1.
2. Graj nutą tonację utworu: nuta = dźwięk ogona (F1, F#1, G1, G#1, A1 – typowe dla hardstyle). W nawiasie [ ] w nazwie
   presetu jest nuta, na której był dopasowywany (na niej brzmi jak oryginał).
3. Dopasuj **LENGTH** do tempa (150 BPM ≈ 400 ms) i głośność **CLIP** / **OUTPUT**. Gotowe – na kanał miksera bez dodatków.

## Ekran THE ANVIL
Podgląd kicka: kształt fali (magma), opadający **pitch** (przerywana linia, liczby w rogu) i biegnąca linia przy każdym uderzeniu.
Dwie rysowane obwiednie (przełączasz **AMP** / **DRIVE**):
- **AMP** – głośność ogona w czasie (np. dziura po punchu, długi ogon, twarde ucięcie na końcu).
- **DRIVE** – jak brudny jest kick w czasie: przester, SCREAM i GRIT idą za tą linią. Nisko na początku i wysoko później =
  **reverse bass**. Wysoko na początku i nisko później = **czysty ogon** (euphoric).

Klik = nowa kropka, przeciągnij = przesuń (kropki nie przeskakują sąsiadów, pierwsza zawsze stoi na uderzeniu),
podwójny klik / prawy klik = usuń. Oś czasu = cała długość (LENGTH) – obwiednie rozciągają się razem z nią.

## Sekcje
- **TAIL** (ogon): LEVEL, **START** (pitch na starcie = START × nuta), **DROP** (jak długo opada), **CURVE** (kształt opadania),
  **WAVE** (sinus → trójkąt → piła przed przesterem), **PITCH** (półtony) + **FINE** (centy) – tylko ogon, punch bez zmian.
- **DISTORTION**: TYPE (SOFT, TUBE, HARD, FOLD, **RAW**), **DRIVE**, TONE (przed przesterem: bas ↔ środek), LOW CUT, HIGH CUT,
  **SCREAM** + SCREAM HZ (rezonansowy „wrzask” rawstyle), **CRUNCH** + CRUNCH DEC (ostry trzask na początku ogona),
  **BODY** (po przesterze: więcej / mniej dołu), **GRIT** („piasek” – brudna faktura na ogonie).
- **PUNCH**: czysty sinus z własnym szybkim opadaniem – „uderzenie”. LEVEL, START, END, DROP, DECAY, włącznik.
- **CLICK**: krótki szum na samym początku, żeby kick przebił się w miksie. LEVEL, TONE, LENGTH.
- **MASTER**: LENGTH, **CLIP** (wbija w clipper – głośniej, twardziej), SOFT (0 = twardy clip, 1 = miękki), VELOCITY, OUTPUT.

## Presety
Dopasowane do 20 kicków referencyjnych (porównanie widma w czasie, pitchu i obwiedni) + kilka narzędziowych:
RAW (Rusted Chainsaw, Unchained Beast, Tribal Fury, Volcanic Eruption, Galactic Roar), HARD (Forged Steel, Old Skool Pick,
Old Skool Hammer, Iron Fist), EUPHORIC (Classic Thump, Bright Thump, Deep Thump, Clean Hammer, Short Tok, Soft Pillow),
REVERSE (Rising Wrath, Undertow, Backdraft, Long Rise), SPECIAL – Double Strike, TOOL (Pure Sub Tail, Punch + Click Only,
Reverse Bass Builder). **RANDOMIZE** losuje nowy kick, **SAVE** → `Dokumenty\Forge of Wrath\Presets\*.fow` (razem z obwiedniami).

Silnik: `../Shared/Source/DoomsdayKick.h` (renderowany 4× oversampling). Ancient Drums zostaje bez zmian.
