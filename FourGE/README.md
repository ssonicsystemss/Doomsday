# 4GE v0.5 – Hardstyle Kick Designer (przebudowa od zera)

Syntezator kicków hardstyle (instrument VST3, FL Studio). **v0.5 to nowy 4GE budowany od zera** według Twojej specyfikacji
17 modułów (`SPEC.md`, `MODULE01.md`…`MODULE12.md`, `MODULES13-17_overview.md`, zasady nadrzędne w `RULES.md`, plan w `PLAN.md`).
Ta wersja to **moduł 01 – CORE KICK ENGINE**: cztery niezależne części kicka i ich mikser. Kolejne moduły dokładają resztę.

> **Ważne przy instalacji:** nowy 4GE ma wszystkie parametry nowe, więc ma też **nowy identyfikator wtyczki**.
> Jeśli masz projekty FL ze starym 4GE v0.4, **nie kasuj starego pliku**: zmień jego nazwę na `4GE v0.4.vst3`
> i zostaw obok nowego `4GE.vst3`. Stare projekty otworzą się ze starą wtyczką i będą brzmieć jak dawniej, a nowe kicki robisz w nowej.
> (Na liście wtyczek FL zobaczysz dwa „4GE” – nowy ma w okienku napis v0.5.)
> Stare presety `.4ge` nie wczytują się do nowego 4GE (świadomie – nowy silnik). Nowe presety zapisujesz w `Dokumenty\4GE\Presets` (`.4gp`).

## Instalacja (Windows)
GitHub → zakładka **Actions** → *Build 4GE VST3* → **Artifacts** → `4GE-VST3-Windows`.
Folder `4GE.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, a w FL: *Options → Manage plugins → Find installed plugins*.
Wstaw jako **instrument** (Channel Rack) i graj nutą (np. G1 = 49 Hz).

## Jak działa kick (moduł 01)
Kick składa się z **czterech niezależnych części**. Każda ma własne źródło, obwiednię, długość, ton, drive i fazę.
Spotykają się dopiero w **CORE MIXER**.

| Część | Kolor | Co robi |
|---|---|---|
| **PUNCH** | czerwony | pierwsze uderzenie: źródło SINE / TRI / SAW / SQUARE / NOISE / CLICK, własny PITCH (Hz), który w GLIDE ms spada do pitchu kicka |
| **BODY** | bursztyn | środek kicka: własny PITCH, który spada do pitchu kicka (dłuższy GLIDE), TONE + RESONANCE, DRIVE |
| **TAIL** | fiolet | ogon: gra dokładnie pitch kicka (START → nuta), źródło SINE / TRI / SAW / SQUARE / PULSE / NOISE, **CHARACTER** |
| **SUB** | niebieski | czysty dół: sinus / trójkąt **na granej nucie**, mono, bez przesteru (DRIVE domyślnie 0) |

**TAIL CHARACTER** – siedem różnych algorytmów nasycenia (nie zmiana nazwy presetu):
CLEAN (prawie czysty) · TONAL (lampa, parzyste harmoniczne) · DIRTY (asymetryczny twardy clip) · SCREAM (rezonansowe pasmo w pętli) ·
METAL (modulacja pierścieniowa nieharmoniczną nośną + fold) · BUZZ (kwadratowy, nieparzyste harmoniczne) · CRUSH (bity + zmniejszanie sample rate).
Hardstyle'owy ogon potrzebuje dużo DRIVE – dopiero wtedy ma ostre krawędzie, czyli górę.

**Obwiednia** każdej części: ATTACK → DECAY (do SUSTAIN) → trzyma → RELEASE do końca jej LENGTH.
Najkrótsze możliwe wejście to 0,15 ms, a wyjście 3 ms, więc nie ma klików nawet przy ATTACK / RELEASE = 0.

## Ekran
- **Góra**: LENGTH (100–2500 ms), MASTER, VELOCITY; przełączniki **AUTO LENGTH**, **PHASE RESET**, **PITCH TRACKING**; **CHOKE / POLY**;
  presety + PLAY / RANDOMIZE / SAVE / UNDO / REDO.
- **PITCH ENGINE**: tryb **CLASSIC / FAST / SLOW / DOUBLE / RISE / STABLE**, START, END, TIME, CURVE (nisko = spada szybko i wcześnie,
  wysoko = trzyma się wysoko i spada późno), AMOUNT (dodatkowe półtony na uderzeniu), FINE.
- **Środek**: duży **waveform** – to prawdziwy wyrenderowany kick, każda część w swoim kolorze.
  **Złap go myszką i przeciągnij do FL** – dostaniesz gotowy plik WAV.
- **Cztery karty**: LEVEL, PITCH, LENGTH, DRIVE, MUTE, SOLO i pasek poziomu. **Kliknij kartę**, a na dole pojawią się jej szczegóły
  (źródło, ATTACK / DECAY / SUSTAIN / RELEASE, TONE, RESONANCE, MIX, GLIDE, PULSE W, PHASE).
- **CORE MIXER**: FADER, PAN, PHASE Ø (odwrócenie fazy) i miernik dla każdej części.
- Wyszarzone gałki w danej chwili nic nie robią: SUB PITCH przy włączonym trackingu (sub siedzi na nucie), PULSE W bez źródła PULSE,
  START / TIME / CURVE / AMOUNT w trybie STABLE, TONE puncha przy CLICK.
- Każdą wartość możesz kliknąć i wpisać ręcznie.

**PITCH TRACKING** włączony: kick projektujesz przy END PITCH, a grana nuta przesuwa go tak, że END = nuta (START przesuwa się razem z nim).
Wyłączony: START i END w hercach, niezależnie od nuty.
**AUTO LENGTH** włączony: LENGTH skraca / wydłuża BODY (trochę), TAIL i SUB (w pełni), a PUNCH zostaje krótki.
Wyłączony: każda część ma swoją długość, a LENGTH tylko ucina kick na końcu (z miękkim wyciszeniem).
**CHOKE**: nowy kick wycisza poprzedni w 6 ms (czysty dół). **POLY**: kicki nachodzą na siebie.

## Presety
INIT, trzy kicki dopasowane do Twoich sampli (**MODERN - Festival Drive**, **RAW - Buzz Saw**, **EARLY - Short Tonal**)
i przykłady każdego CHARACTER (CORE - …). Uwaga: to dopiero rdzeń – jeden oscylator na część, bez unisonu, szumu i stopni przesteru,
więc ogon jest „czystszy” niż w gotowych kickach. To przychodzi w modułach 03 (oscylatory, unison), 04 (szum, warstwy) i 05 (przester).

## Co dalej
Moduł po module wg `PLAN.md`: 02 obwiednie wielopunktowe i pitch każdej części → 03 oscylatory → 04 szum / transient / warstwy →
05 przester → 06 filtry / screech → 07 EQ → 08 dynamika → 09 modulacja i makra → 10 sample → 11 stereo → 12 eksport →
13–14 style i długość → 15–16 presety i analiza → 17 docelowy interfejs.
