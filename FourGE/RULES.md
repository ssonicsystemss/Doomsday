# 4GE — ZASADY NADRZĘDNE (obowiązują wszystkie 17 modułów)

## 1. MASTER MACROS (właściciel, 2026-11-04)
4GE posiada dokładnie osiem globalnych makr MASTER MACROS: IMPACT, BODY, TAIL, PITCH, GRIT, SCREAM, WEIGHT oraz CHAOS.
Żaden moduł nie może definiować alternatywnego zestawu globalnych makr ani tworzyć duplikatów tych funkcji.
Wszystkie moduły mogą udostępniać lokalne kontrolery, dostępne bezpośrednio w swoich interfejsach i sterowane przez MASTER MACROS za pośrednictwem centralnego systemu Macro Mapping w module 09.
Lokalne kontrolery nie mogą nadpisywać wartości bazowych ani zmieniać konfiguracji globalnych makr.
System zachowuje niezależność wartości bazowych, lokalnych ustawień i modulacji.
Cel: osiem łatwych do zrozumienia pokręteł na głównym ekranie, a pod spodem profesjonalny system modulacji bez bałaganu w nazewnictwie.
Szczegóły (cele, pola przypisania, user macros): PLAN.md → „Odpowiedzi na pytania końcowe”.

## 2. Makro nie jest parametrem DSP
Makro to nadrzędny kontroler wysyłający wartości przez Macro Mapping (M09). FINAL = BASE + aktywne modulacje; ruch makra nigdy nie zapisuje wartości bazowej.

## 3. GUI z rzeczywistych parametrów
GUI (M17) powstaje na podstawie rzeczywistych parametrów i funkcji modułów 01–16 (Parameter Registry).

## 4. Bez atrap
Każda widoczna kontrolka zmienia dźwięk (test min vs max); DSP, GUI i pliki nigdy w jednym komponencie.

## 5. 4GE ≠ RE4GE – rozdział odpowiedzialności (właściciel, 2026-11-04)
4GE NIE generuje reverse bassu i nie ma jego silnika – to funkcja osobnej wtyczki RE4GE. Dwa produkty jednego ekosystemu SonicSystems, różne zadania:
- **4GE — Hardstyle Kick Designer** (główny instrument): tworzenie pełnych kicków hardstyle od podstaw – transient, punch, body, tail, pitch, sub, distortion, screech i warstwy.
- **RE4GE** – wyspecjalizowany instrument do tworzenia reverse bassu.
(Przy stylu EARLY „reverse bass” = estetyka / kick pasujący pod reverse z RE4GE, nie generowanie reverse w 4GE.)

### Co 4GE generuje w każdym stylu (właściciel, 2026-11-04)
| Styl | Co powinien generować 4GE |
|---|---|
| EARLY | Klasyczne, melodyjne kicki hardstyle z charakterystycznym punch'em, tonalnym body, prostszym distortion i wyraźnym pitch dropem. |
| MODERN | Mocne, czyste i kontrolowane kicki z precyzyjnym transientem, wielowarstwowym body, dopracowanym tail'em i nowoczesną dynamiką. |
| RAW | Agresywne kicki z mocnym distortion, ostrym screech'em, asymetrią, wyraźnymi harmonicznymi i surowym charakterem. |
| EXPERIMENTAL | Nietypowe, hybrydowe i eksperymentalne kicki wykorzystujące niestandardową syntezę, modulację, warstwowanie i przetwarzanie. |
(Zastępuje „reverse bass” w opisie EARLY w MODULES13-17_overview.md.)

### EARLY – rozróżnienie (właściciel)
EARLY MOŻE zawierać: klasyczne pitch dropy; tonalne body; klasyczne punch'e; analogowo brzmiące distortion; prostsze, wyraźne warstwy; klasyczną dynamikę kicka.
EARLY NIE MOŻE zawierać: generatora reverse bassu, jego obwiedni ani dedykowanego silnika syntezy.

### Współpraca 4GE ↔ RE4GE – tak, ale bez kopiowania funkcji
1. Standalone – oba pluginy działają całkowicie niezależnie.
2. DAW Routing – wyjście RE4GE do ścieżki z 4GE lub odwrotnie, standardowym routingiem DAW.
3. Preset Compatibility – wspólna koncepcja bibliotek, tagów i metadanych, ale BEZ założenia, że presety jednego pluginu otwierają się w drugim.
Bez obowiązkowego połączenia między wtyczkami – każda działa samodzielnie.
