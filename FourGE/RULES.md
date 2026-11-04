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
