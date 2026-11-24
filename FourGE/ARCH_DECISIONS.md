# DECYZJE WŁAŚCICIELA do DSP_ARCHITECTURE.md (2026-11-24)

Decyzja: zostawić TAIL TYPE jako osobne zachowania wewnątrz wspólnego TAIL ENGINE
Czyli:
TAIL ENGINE
│
├── EARLY TAIL
├── MODERN TAIL
├── RAW TAIL
├── EXPERIMENTAL TAIL
├── GABBER TAIL
├── MILLENNIUM TAIL
├── HARDCORE TAIL
└── UPTEMPO TAIL

ale nie:
EARLY_TAIL_ENGINE.cpp
RAW_TAIL_ENGINE.cpp
...

Każdy typ powinien korzystać z tego samego API i podstawowych DSP building blocks.

To jest ważne rozróżnienie:
wspólny silnik ≠ identyczne zachowanie każdego stylu.

Dlaczego nie scalałbym EARLY/RAW/EXPERIMENTAL do jednego zachowania?
Bo właśnie TAIL jest jednym z miejsc, gdzie styl naprawdę może zmieniać architekturę sygnału, a nie tylko wartości parametrów.

Przykładowo RAW może potrzebować:
TAIL
→ distortion
→ filter movement
→ harmonic rise
→ nonlinear shaping

a EARLY może wymagać zupełnie innego przebiegu:
TAIL
→ pitch/body
→ relatively clean distortion
→ resonance
→ decay

Jeżeli zrobimy jeden identyczny algorytm i będziemy tylko zmieniać:
DRIVE = 35%
DRIVE = 70%
DRIVE = 90%

to zaczniemy udawać różne style parametrami, zamiast faktycznie modelować ich zachowanie.

I dokładnie tego chcemy uniknąć.
Ale zrobiłbym ważną zmianę w architekturze
Niech TAIL TYPE nie oznacza całkowicie osobnego silnika.

Zamiast:
if EARLY:
    runEarlyTail();

if RAW:
    runRawTail();

if EXPERIMENTAL:
    runExperimentalTail();

lepiej:
TailEngine
    ├── Source
    ├── Pitch
    ├── Envelope
    ├── Distortion
    ├── Filter
    ├── Resonance
    ├── Harmonic Motion
    └── Type Profile

a Type Profile mówi silnikowi jak ma pracować.

Czyli:
TailTypeProfile
{
    sourceMode
    pitchMode
    envelopeMode
    distortionRouting
    filterMode
    resonanceMode
    harmonicMode
    lengthBehavior
}

To daje nam możliwość zachowania obecnego ARCHITECTURE.md, bez rozwalania tego, co już macie.
I jeszcze jedna rzecz: CRUNCH
Tutaj natomiast scaliłbym dwa CRUNCH-e.

Czyli obecnie:
RAW CRUNCH
HARDCORE CRUNCH

→ jeden:
CRUNCH ENGINE

ale z profilem:
CrunchTypeProfile

Na przykład:
CRUNCH ENGINE
│
├── RAW PROFILE
├── HARDCORE PROFILE
├── GABBER PROFILE
├── MILLENNIUM PROFILE
└── UPTEMPO PROFILE

Dlaczego tutaj warto scalać?

Bo wszystkie mają tę samą fundamentalną funkcję:

źródło tonalne → envelope → pitch → nonlinear processing → asymmetry → clipping

Różni się przede wszystkim:
- źródło,
- timing,
- pitch,
- envelope,
- pasmo,
- drive,
- asymmetry,
- routing.
To jest dokładnie przypadek, gdzie wspólny engine ma sens.
SCREAM — też bym zmienił
Tutaj zdecydowanie warto dodać:
SCREAM SOURCE

z opcjami:
OSCILLATOR
TAIL RESAMPLE
BODY
CRUNCH
HYBRID

I wtedy:
TAIL
   │
   ▼
SCREAM SOURCE
   │
   ├── PITCH
   ├── FILTER
   ├── DISTORTION
   └── ENVELOPE

czyli SCREAM może dostać tail jako materiał wejściowy, ale nie musi.

To jest dużo lepsze niż robienie screecha wyłącznie z oscylatora.
Długość CRUNCH — tutaj zdecydowanie bym to teraz poprawił
Wasze:
CRUNCH LENGTH = procent

jest OK jako GUI, ale wewnętrznie zrobiłbym zależność:
\[
L_C = clamp
\left(
L_{base}
+
k_P L_P
+
k_B L_B
+
k_T L_T
\right)
\]
gdzie:
- \(L_C\) = długość crunchu
- \(L_P\) = długość PUNCH
- \(L_B\) = długość BODY
- \(L_T\) = długość TAIL
- \(k_P,k_B,k_T\) = współczynniki profilu
Przykładowo RAW:
\[
L_C =
L_{base}
+
0.15L_P
+
0.35L_B
\]
a Uptempo:
\[
L_C =
L_{base}
+
0.10L_P
+
0.20L_B
\]
Natomiast GUI nadal może pokazywać:

CRUNCH LENGTH: 65%

Ten procent jest wtedy mnożnikiem całego modelu:
\[
L_C^{final}=L_C\cdot LengthMultiplier
\]
Czyli nie zabieramy obecnego parametru, tylko nadajemy mu sens fizyczny.
Styl powinien mieć jeszcze jeden poziom
To, co napisałeś:
„Styl zmienia tylko wartości, a nie zachowanie”

jest jedynym punktem, który naprawdę chciałbym zmienić.

Nie dla wszystkich modułów.

Zrobiłbym:
STYLE = parametry + behavior flags
Czyli profil może powiedzieć:
RAW:
crunchTiming = DELAYED
crunchRise = BODY_END_APPROACH
tailBehavior = RAW

a:
EARLY:
crunchTiming = EARLY
crunchRise = OFF
tailBehavior = EARLY

To nie oznacza osobnego silnika.

To oznacza, że profil może zmienić sposób sterowania modułem.
Energia warstw — zdecydowanie dodać
To jest bardzo dobry brak.

Nie tylko:
FULL KICK RMS
FULL KICK PEAK

ale:
PUNCH ENERGY
BODY ENERGY
CRUNCH ENERGY
SUB ENERGY
TAIL ENERGY
TOK ENERGY
SCREAM ENERGY

Można liczyć:
\[
E_i = \frac{1}{N}\sum_{n=0}^{N-1}x_i[n]^2
\]
i dodatkowo:
\[
E_{dB}=10\log_{10}(E_i+\epsilon)
\]
Ale zrobiłbym jeszcze band energy:
LOW
LOW-MID
MID
HIGH-MID
HIGH

dla każdej warstwy.

To będzie zajebiście przydatne przy developmentcie, bo będziemy mogli zobaczyć np.:
CRUNCH
TOTAL: -14.2 dB

LOW:      -31 dB
LOW-MID:  -18 dB
MID:      -12 dB
HIGH-MID: -10 dB
HIGH:     -24 dB

i od razu będzie wiadomo, dlaczego CRUNCH brzmi jak „DUM” albo jak „SZHH”.