# 4GE — KICK DSP ARCHITECTURE
## BUILD SPECIFICATION — DEVELOPMENT VERSION

### STATUS

Dokument opisuje docelową logikę poszczególnych silników DSP w rozwijanym buildzie 4GE.

Nie jest to jeszcze zamrożona specyfikacja V1.

Priorytet:

**najpierw poprawna architektura DSP i zgodność z rzeczywistymi technikami produkcji hardstyle / rawstyle / hardcore / uptempo, dopiero później finalne zakresy GUI i presetów.**

---

# 1. GŁÓWNA ARCHITEKTURA

4GE ma być systemem zależnych modułów:

```text
KICK VOICE
│
├── MIDI / ROOT
├── BPM / LENGTH
├── SOURCE
├── PITCH
├── TOK
├── PUNCH
├── BODY
├── CRUNCH
├── SUB
├── TAIL
├── SCREAM / TEXTURE
├── DISTORTION
├── ASYMMETRY
├── EQ
├── CLIP
└── FINAL BUS
```

Style nie tworzą osobnych silników.

Style dostarczają profile parametrów do tych samych silników.

```text
CORE
│
├── EARLY
├── MODERN
├── RAW
├── EXPERIMENTAL
├── GABBER
├── MILLENNIUM
├── HARDCORE
└── UPTEMPO
```

---

# 2. KICK VOICE MANAGER

Kick Voice Manager odpowiada za uruchomienie wszystkich modułów w ramach jednej nuty MIDI.

Przy NOTE ON:

```text
create voice
reset envelopes
reset phase
calculate root
calculate initial pitch
start all enabled engines
```

Przy NOTE OFF:

```text
do not instantly kill kick
respect current envelope / tail behaviour
```

Każdy kick musi być deterministyczny.

Ten sam:

```text
MIDI NOTE
+
PARAMETERS
+
BPM
```

musi wygenerować ten sam waveform.

Nie używać randomizacji fazy ani losowych burstów audio jako podstawowego mechanizmu brzmienia.

---

# 3. ROOT / TUNING ENGINE

MIDI note:

\[
f_{root}=440\cdot2^{(n-69)/12}
\]

gdzie:

- n = MIDI note number.

Przykład:

A4 = 440 Hz.

A3:

\[
440\cdot2^{-12/12}=220Hz
\]

Root jest podstawą dla:

- BODY
- TAIL
- SUB
- tonal CRUNCH

Nie wszystkie elementy muszą jednak kończyć się dokładnie na root.

Każdy moduł może mieć:

```text
ROOT
OFFSET
FIXED
FOLLOW MIDI
```

---

# 4. SOURCE ENGINE

Source Engine generuje materiał wejściowy.

Źródła:

```text
SINE
SAW
TRIANGLE
909
ANALOG BD
HYBRID
```

Source Engine nie powinien decydować o całym brzmieniu kicka.

Jego zadaniem jest dostarczenie materiału do kolejnych modułów.

---

# 5. OSCILLATOR PHASE

Dla źródeł tonalnych:

```text
PHASE MODE
```

wartości:

```text
FIXED
FREE
OFFSET
```

Domyślnie:

```text
FIXED
```

dla kicków wymagających powtarzalnego transientu.

Nie stosować automatycznej phase randomization.

Stała faza jest szczególnie ważna w kickach z warstwami i kontrolowanym transientem.

---

# 6. PITCH ENGINE

Pitch Engine steruje częstotliwością źródła w czasie.

Podstawowe równanie:

\[
f(t)=f_{root}\cdot2^{P(t)/12}
\]

gdzie:

\[
P(t)=P_{start}\cdot E(t)
\]

a:

```text
E(t) ∈ [0,1]
```

jest envelope.

Przykład:

```text
START = +24 semitones
END = 0
TIME = 25 ms
```

oznacza:

```text
wysoka częstotliwość
↓
szybki pitch drop
↓
BODY / TAIL
```

---

# 7. PITCH CURVE

Dostępne:

```text
LINEAR
EXP
FAST
S-CURVE
CUSTOM
```

Ogólna funkcja:

\[
P(t)=P_{start}+(P_{end}-P_{start})C(t)
\]

gdzie C(t) jest funkcją krzywej.

---

# 8. PUNCH ENGINE

PUNCH odpowiada za fizyczne pierwsze uderzenie.

Nie powinien być po prostu kopią BODY.

PUNCH:

```text
krótki
dynamiczny
szeroki harmonicznie
wysokoenergetyczny
```

Może wykorzystywać:

```text
909
BD
SINE
SAW
NOISE
HYBRID
```

ale source selection zależy od stylu.

---

# 9. PUNCH ENVELOPE

Podstawowy model:

\[
A_p(t)=G_p\cdot E_p(t)
\]

gdzie:

```text
G_p = punch gain
E_p = punch envelope
```

Envelope może być:

```text
ATTACK
DECAY
SUSTAIN
RELEASE
```

Dla kicka najczęściej:

```text
ATTACK ≈ 0
SUSTAIN ≈ 0
DECAY = short
```

---

# 10. TOK ENGINE

TOK jest osobnym elementem poprzedzającym właściwy BODY/TAIL.

TOK nie powinien być automatycznie utożsamiany z CRUNCH.

Struktura:

```text
TOK
↓
PUNCH
↓
BODY
↓
TAIL
```

TOK może korzystać z:

- istniejącego PUNCH,
- osobnego transientu,
- krótkiej kopii kicka,
- specjalnego source.

TOK może mieć:

```text
FOLLOW MIDI
FIXED KEY
FIXED LENGTH
AUTO LENGTH
```

---

# 11. BODY ENGINE

BODY jest główną tonalną masą kicka.

Model:

\[
B(t)=A_b(t)\sin(2\pi\phi(t))
\]

z:

\[
\frac{d\phi}{dt}=f(t)
\]

BODY wykorzystuje PITCH ENGINE.

To właśnie BODY powinien otrzymać dużą część distortion.

Nie należy zakładać, że BODY = czysty sine.

Po distortion:

```text
BODY
↓
harmonics
↓
character
```

---

# 12. BODY PITCH

BODY może zaczynać się znacznie wyżej niż końcowy root.

Przykład:

```text
START = root + 24 semitones
END = root
```

lub:

```text
START = root + 36 semitones
END = root
```

Dokładny zakres zależy od stylu.

Nie ustalać jednego pitch range dla całego 4GE.

---

# 13. CRUNCH ENGINE

CRUNCH jest osobnym tonalnym elementem.

Nie jest:

```text
noise generator
```

Nie jest:

```text
random crack generator
```

Główna architektura:

```text
CRUNCH SOURCE
↓
CRUNCH ENV
↓
OPTIONAL PITCH
↓
PRE EQ
↓
SATURATION
↓
ASYMMETRIC DISTORTION
↓
WAVESHAPER
↓
CLIP
↓
POST EQ
```

---

# 14. CRUNCH SOURCE

Możliwe:

```text
SINE
ANALOG BD
909
SINE + 909
```

SAW może być używany eksperymentalnie.

Domyślnie:

```text
RAW:
SINE / BD / 909
```

```text
GABBER:
909
```

```text
MILLENNIUM:
909 / BD
```

```text
UPTEMPO:
SINE / SINE + 909
```

---

# 15. CRUNCH PITCH

CRUNCH może posiadać własny pitch.

Tryby:

```text
FIXED
FOLLOW BODY
OWN ENVELOPE
FOLLOW PUNCH
```

Nie zakładać, że CRUNCH zawsze musi mieć stały pitch.

---

# 16. CRUNCH ENVELOPE

\[
C(t)=A_c(t)\cdot S_c(t)
\]

gdzie:

- \(A_c(t)\) = amplitude envelope
- \(S_c(t)\) = source

CRUNCH może rozpoczynać się po PUNCH.

\[
t_{crunch}=t_{punch}+D_c
\]

gdzie:

```text
D_c = CRUNCH DELAY
```

---

# 17. CRUNCH DISTORTION

Pierwszy stage:

```text
TUBE / SATURATION
```

Cel:

dodać harmoniczne.

Drugi:

```text
ASYMMETRIC DISTORTION
```

Cel:

wygenerować charakterystyczny crunch.

Trzeci:

```text
WAVESHAPER
```

Czwarty:

```text
CLIP
```

Nie używać jednego maksymalnego DRIVE jako substytutu całego chainu.

---

# 18. ASYMMETRIC DISTORTION

Model:

\[
y=f(x)
\]

ale:

\[
f(x)\neq -f(-x)
\]

dla asymmetry.

Czyli dodatnia i ujemna część waveformu są traktowane inaczej.

Przykładowo:

\[
y=
\begin{cases}
g_+(x), & x\ge0\\
g_-(x), & x<0
\end{cases}
\]

gdzie:

\[
g_+(x)\neq -g_-(-x)
\]

To generuje dodatkowe harmoniczne parzyste i zmienia charakter distortion.

---

# 19. ASYMMETRY CONTROL

Parametr:

```text
ASYMMETRY
```

0%:

```text
symmetry
```

100%:

```text
maximum asymmetry
```

Możliwa implementacja:

```text
POSITIVE DRIVE
NEGATIVE DRIVE
BIAS
```

zamiast prostego jednego pokrętła.

---

# 20. DISTORTION ENGINE

Distortion Engine musi umożliwiać serię etapów:

```text
EQ
↓
DISTORTION
↓
EQ
↓
DISTORTION
↓
EQ
↓
ASYMMETRY
↓
CLIP
```

Każdy stage może mieć osobny:

```text
DRIVE
MIX
TONE
BIAS
FILTER
```

---

# 21. DLACZEGO WIELE STAGE?

Jeżeli:

\[
y=f(x)
\]

zrobimy jeden raz bardzo agresywnie, waveform zostaje ekstremalnie spłaszczony.

Zamiast:

```text
100% DRIVE
```

preferowane:

```text
30% DRIVE
↓
EQ
↓
35% DRIVE
↓
ASYMMETRY
↓
20% CLIP
```

Wiele mniejszych nonlinear transformations pozwala kontrolować rozkład harmonicznych.

---

# 22. PRE-EQ

EQ przed distortion decyduje, które częstotliwości będą napędzały nonlinear stage.

Matematycznie:

\[
x_{eq}(t)=EQ\{x(t)\}
\]

następnie:

\[
y(t)=D(x_{eq}(t))
\]

Dlatego boost przed distortion nie jest tym samym co boost po distortion.

---

# 23. POST-EQ

Po distortion:

\[
y_{eq}(t)=EQ\{D(x(t))\}
\]

Post-EQ służy do usunięcia harmonicznych, których nie chcemy.

To rozróżnienie musi być zachowane w GUI.

```text
PRE EQ ≠ POST EQ
```

---

# 24. SUB ENGINE

SUB powinien być możliwie stabilnym źródłem tonalnym.

Typowe źródło:

```text
SINE
```

Model:

\[
S(t)=A_s(t)\sin(2\pi f_{root}t)
\]

SUB nie powinien być automatycznie przepuszczany przez pełny distortion chain.

Może mieć własny:

```text
LOWPASS
GAIN
DECAY
TUNING
```

---

# 25. TAIL ENGINE

TAIL może być:

```text
TONAL
DISTORTED
HYBRID
```

W zależności od style profile.

Tail powinien być kontrolowany przez:

```text
TAIL LENGTH
TAIL DECAY
TAIL GAIN
TAIL PITCH
TAIL DISTORTION
TAIL FILTER
```

---

# 26. TAIL LENGTH

Nie można uzależniać wszystkiego wyłącznie od Note Off.

Plugin nie zna przyszłej długości MIDI note w momencie Note On.

Dlatego należy rozdzielić:

```text
VOICE LENGTH
```

od:

```text
TAIL DESIGN LENGTH
```

TAIL może być:

```text
FREE
BPM SYNC
GRID SYNC
NOTE OFF FOLLOW
```

---

# 27. TAIL RISE ENGINE

Istniejący koncept pozostaje.

Rise nie zmienia końca kicka.

Działa jako dodatkowa modulacja energii:

\[
G(t)=G_0+R\cdot E_r(t)
\]

gdzie:

```text
R = RISE AMOUNT
E_r = RISE ENVELOPE
```

Start może być zależny od gridu:

\[
t_{start}=T_{grid}\cdot p
\]

gdzie:

```text
p = START %
```

---

# 28. RISE MODULATION

Rise może sterować:

```text
GAIN
PITCH
DRIVE
HARMONICS
FILTER
NOISE/TEXTURE
```

Każdy parametr niezależnie.

Nie wszystkie muszą być aktywne jednocześnie.

---

# 29. SCREAM ENGINE

SCREAM nie powinien być częścią podstawowego BODY.

Może korzystać z:

```text
TAIL
BODY
RESAMPLED TAIL
OSCILLATOR
```

Jedna z praktycznych metod stosowanych przez producentów to pobranie istniejącego taila, przestrojenie go i użycie jako screechu.

W 4GE:

```text
SOURCE = TAIL
↓
PITCH UP
↓
FILTER
↓
DISTORTION
↓
SCREAM
```

---

# 30. TEXTURE ENGINE

Texture jest opcjonalnym dodatkiem.

Może zawierać:

```text
NOISE
CRUSH
SHORT TRANSIENT
SAMPLE-LIKE MATERIAL
```

Texture nie może zastępować:

```text
PUNCH
CRUNCH
BODY
```

Noise jest dodatkiem, nie podstawowym generatorem tonalnego crunchu.

---

# 31. PHASE ENGINE

Każda warstwa może mieć:

```text
PHASE
```

ale randomizacja nie może być domyślna.

Dla dwóch warstw:

\[
x(t)=x_1(t)+x_2(t+\phi)
\]

zmiana \(\phi\) może powodować:

```text
reinforcement
cancellation
different crunch
different punch
```

Dlatego faza jest parametrem brzmieniowym.

---

# 32. FINAL BUS

Wszystkie elementy:

```text
TOK
PUNCH
BODY
CRUNCH
SUB
TAIL
TEXTURE
SCREAM
```

trafiają do:

```text
KICK BUS
```

Następnie:

```text
PRE EQ
↓
BUS DISTORTION
↓
ASYMMETRY
↓
CLIP
↓
POST EQ
```

Nie należy automatycznie stosować maksymalnego clippingu na całym busie.

---

# 33. STYLE ENGINE

Style nie tworzą nowych DSP modules.

Style są zestawami parametrów:

\[
Style=\{Source,Pitch,Envelope,Distortion,EQ,Length,Routing\}
\]

Przykład:

```text
GABBER:
Source = 909
Pitch = medium/fast
Body = heavy
Tail = long
Distortion = extreme
Crunch = dark
```

```text
UPTEMPO:
Source = sine/909
Pitch = fast
Body = aggressive
Tail = short
Crunch = brighter
Mid/High = stronger
```

---

# 34. GABBER PROFILE

Główny kierunek:

```text
909
↓
EQ
↓
HARD CLIP
↓
ASYMMETRY
↓
EQ
↓
CLIP
```

On Point Samples pokazuje właśnie 909 + EQ boost + hard clipping + asymmetrical distortion + kolejny clipping jako metodę budowy Gabber/Hardcore kicka.

---

# 35. MILLENNIUM PROFILE

Profil powinien zwiększać:

```text
BODY
LOW-MID
DISTORTION DENSITY
CRUNCH
```

bez zwykłego:

```text
DRIVE +20%
```

Czyli zmieniać strukturę chainu, nie tylko jego głośność.

---

# 36. HARDCORE PROFILE

Profil oparty o:

```text
909 / BD
↓
EQ
↓
DISTORTION
↓
EQ
↓
DISTORTION
↓
CLIP
```

Teknovault pokazuje właśnie naprzemienne EQ i distortion jako podstawę hardcore kicka z 909.

---

# 37. UPTEMPO PROFILE

Profil:

```text
SHORTER
FASTER
BRIGHTER
MORE MID/HIGH CRUNCH
```

Nie może mieć automatycznie długiego taila.

Jeżeli kick zaczyna kolidować z następnym kickiem:

```text
SHORTEN TAIL
```

zamiast tylko ściszać cały kick.

---

# 38. LENGTH MODEL

Każdy element posiada własną długość:

```text
L_tok
L_punch
L_body
L_crunch
L_sub
L_tail
```

Kick:

\[
L_{kick}=\max(L_{tok},L_{punch},L_{body},L_{crunch},L_{sub},L_{tail})
\]

Ale długości nie muszą być identyczne.

---

# 39. ELEMENT DEPENDENCY

Silniki mogą mieć zależności:

```text
PITCH
  ↓
PUNCH
  ↓
BODY
  ↓
CRUNCH
  ↓
TAIL
```

Przykład:

\[
L_{crunch}=k_1L_{body}+k_2L_{punch}
\]

gdzie:

```text
k1, k2
```

pochodzą z Style Profile.

---

# 40. CRUNCH START

Przykład:

\[
t_c=t_p+D
\]

gdzie:

```text
t_p = PUNCH start
D = CRUNCH DELAY
```

CRUNCH może zaczynać się:

```text
0 ms
10 ms
20 ms
30 ms
```

w zależności od stylu.

---

# 41. CRUNCH INTENSITY

Można modelować:

\[
C_{gain}(t)=A_c(t)\cdot D_c(t)\cdot M_c
\]

gdzie:

```text
A_c = envelope
D_c = distortion amount
M_c = final mix
```

To pozwala oddzielić:

```text
CRUNCH LENGTH
CRUNCH DRIVE
CRUNCH MIX
```

i uniknąć sytuacji, w której jedno pokrętło robi trzy różne rzeczy.

---

# 42. DYNAMIC STYLE RESPONSE

Style Profile powinien modyfikować funkcje, a nie tylko wartości.

Przykład:

```text
RAW:
CRUNCH → później + mocniej pod koniec

UPTEMPO:
CRUNCH → wcześniej + krócej + jaśniej

GABBER:
CRUNCH → mocno związany z 909/body

MILLENNIUM:
CRUNCH → cięższy low-mid
```

---

# 43. DETERMINISTYCZNY CHAOS

CHAOS nie może oznaczać:

```text
random waveform
random phase
random pitch
random timing
```

Za każdym renderem.

Zamiast tego:

\[
x_{chaos}=f(x,seed)
\]

Seed jest częścią preset/voice.

Dzięki temu można uzyskać controlled variation:

```text
seed 0
seed 1
seed 2
```

ale ten sam seed daje ten sam kick.

---

# 44. KONTROLA ENERGII

Każdy element powinien mieć własną energię:

\[
E_i=\int |x_i(t)|^2dt
\]

Nie wystarczy wyrównywać peak.

Programista powinien monitorować:

```text
PEAK
RMS
ENERGY
LOW ENERGY
MID ENERGY
HIGH ENERGY
```

Dzięki temu preset nie będzie „głośniejszy = lepszy”.

---

# 45. PHASE / LAYER SAFETY

Przy sumowaniu:

\[
x_{sum}(t)=\sum_i x_i(t)
\]

może dojść do:

```text
constructive interference
```

lub:

```text
destructive interference
```

Dlatego każda istotna warstwa powinna mieć kontrolę:

```text
PHASE
POLARITY
GAIN
```

---

# 46. WAVEFORM ANALYSIS

4GE powinien mieć wewnętrzną analizę waveform.

Dla każdej warstwy:

```text
SOURCE
PITCH
ENVELOPE
DISTORTION
PHASE
OUTPUT
```

Programista powinien móc odsłuchać:

```text
SOLO PUNCH
SOLO BODY
SOLO CRUNCH
SOLO SUB
SOLO TAIL
FULL KICK
```

To jest bardzo ważne podczas developmentu.

---

# 47. NAJWAŻNIEJSZA ZASADA DSP

Nie próbować uzyskać docelowego brzmienia przez jeden parametr.

Zamiast:

```text
DRIVE = 100%
```

stosować:

```text
SOURCE
→ PITCH
→ ENVELOPE
→ PRE EQ
→ DISTORTION
→ EQ
→ ASYMMETRY
→ DISTORTION
→ CLIP
→ POST EQ
```

Każdy etap ma konkretną funkcję.

---

# 48. CO JEST JUŻ DOBRZE POTWIERDZONE

### POTWIERDZONE PRZEZ WIELE ŹRÓDEŁ

```text
PITCH ENVELOPE
SOURCE → DISTORTION
MULTI-STAGE DISTORTION
EQ BETWEEN DISTORTION STAGES
ASYMMETRIC DISTORTION
CLIPPING
SEPARATE PUNCH / BODY / TAIL
CONTROLLED PHASE
```

### ZALEŻNE OD STYLU

```text
CRUNCH PITCH
CRUNCH LENGTH
SOURCE TYPE
TAIL LENGTH
MID/HIGH EMPHASIS
AMOUNT OF ASYMMETRY
NUMBER OF DISTORTION STAGES
```

### NIE NALEŻY TRAKTOWAĆ JAKO UNIWERSALNEJ ZASADY

```text
"CRUNCH zawsze = SINE"
"CRUNCH zawsze bez pitch envelope"
"CRACK zawsze = NOISE"
"Gabber = tylko 909"
"Uptempo = tylko sine"
"każdy kick potrzebuje osobnego sub layer"
```

Materiały producentów pokazują różne implementacje, więc 4GE powinien dawać profile i warianty zamiast jednej „prawidłowej” recepty.

---

# 49. DEVELOPMENT PRIORITY

Najpierw dopracować:

```text
1. SOURCE
2. PITCH
3. PUNCH
4. BODY
5. CRUNCH
6. TAIL
7. DISTORTION
8. ASYMMETRY
9. PHASE
10. LENGTH
```

Dopiero później:

```text
SCREAM
TEXTURE
CHAOS
AUTO
ADVANCED STYLE
```

---

# 50. NAJWAŻNIEJSZY TEST

Jeżeli 4GE ma problem z uzyskaniem konkretnego charakteru, nie dodawać od razu kolejnego modułu.

Najpierw sprawdzić:

```text
SOURCE
↓
PITCH
↓
ENVELOPE
↓
DISTORTION
↓
ASYMMETRY
↓
CLIP
```

Jeżeli ten podstawowy łańcuch nie generuje właściwego materiału, kolejny generator noise/impulse nie rozwiązuje problemu.

Najpierw poprawić źródło i nonlinear processing.

---

# 51. CEL ARCHITEKTURY

4GE ma być systemem, w którym:

```text
SOURCE
```

decyduje o materiale,

```text
PITCH
```

decyduje o ruchu tonalnym,

```text
ENVELOPE
```

decyduje o czasie,

```text
DISTORTION
```

decyduje o harmonicznych,

```text
ASYMMETRY
```

decyduje o charakterze crunchu,

```text
CLIP
```

decyduje o zagęszczeniu,

```text
EQ
```

decyduje o rozkładzie energii,

a:

```text
STYLE PROFILE
```

decyduje o relacji między wszystkimi elementami.

To jest podstawowa architektura, którą należy dalej rozwijać i testować.
---

# STAN 4GE vs TA SPECYFIKACJA (2026-11-24, po TAIL TYPE HARDCORE)

JEST: 1 style = profile (GABBER / MILLENNIUM / HARDCORE / UPTEMPO = jeden silnik HARDCORE), 2 deterministyczny seed na nutę
(`Engine.h` seed = nuta + `presetSeed`), 3 root z MIDI + FOLLOW / FIXED (hc_key_mode), 4 źródła 909 / BD / SINE / mixy / HYBRID,
5/31/45 faza na część (`*_phase`, `core_phase_reset`) + polaryzacja (`mix_*_invert`), 6-7 pitch env + krzywe, 8-12 PUNCH / TOK /
BODY osobno, 13-17 crunch tonalny ze startem / pitch / drive / asym (cn_*, hc_cr_*), 18-21 wiele stopni + EQ między + asym
+ clip (pula 16 stopni modułu 05 + hc), 22-23 PRE EQ ≠ POST EQ, 24 SUB sinus z własną obwiednią, 26 LEN / NOTE / +FX + BPM GRID,
27-28 RISE (gain / pitch / drive / harm / filter / noise), 30 texture tylko dodatek, 32 GLOBAL dist + EQ + MASTER CLIP,
46 SOLO PUNCH / BODY / TAIL / SUB / CRUNCH.
CZĘŚCIOWO: 39-41 długość crunchu = % (cn_follow), bez wzoru k1·L_body + k2·L_punch; 42 styl zmienia wartości, nie funkcje;
44 analiza (moduł 16) mierzy cały kick (pasma, RMS, peak), nie każdą warstwę osobno; 19 asym = jedno pokrętło + bias
(jest typ ASYM z osobnym drive +/- w puli dist, w HC nie).
BRAKUJE: 29 SCREAM z taila (screech modułu 06 jest tylko z oscylatorów), 13 jeden wspólny CRUNCH ENGINE (dziś dwa:
RAW CRUNCH cn_* i crunch HC hc_cr_*), 44 licznik energii per warstwa.
UWAGA: EARLY / RAW / EXPERIMENTAL mają własne TAIL TYPE (ARCHITECTURE.md 2026-11-14) - spec 1/33 mówi "jeden silnik, style =
profile"; do decyzji właściciela, czy scalać.
