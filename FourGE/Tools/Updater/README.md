# 4GE Updater (Windows)

Program, który sam pilnuje, żebyś miał najnowszy testowy build 4GE.

- **Instalacja (raz):** pobierz `Zainstaluj_4GE_Updater.bat` (link niżej: prawy klik → „Zapisz jako…”), dwuklik.
  https://github.com/ssonicsystemss/Doomsday/raw/main/FourGE/Tools/Updater/Zainstaluj_4GE_Updater.bat
- Instalator prosi **raz** o zgodę administratora (podmiana w `C:\Program Files\Common Files\VST3`).
- Potem **co 10 minut** sprawdza w tle GitHuba (Releases „4ge-latest”). Gdy jest nowy build, **sam go pobiera i podmienia**
  `4GE.vst3`, a potem pokazuje okienko „4GE zaktualizowany” z datą i opisem zmiany.
  Gdy FL jest otwarte (FL trzyma stary plik), raz mówi „podmienię go sam, gdy zamkniesz FL” i robi to po zamknięciu FL.
- **Ikona na pulpicie** „Sprawdz aktualizacje 4GE” = sprawdź teraz (pokaże też „Masz najnowszy 4GE”).
- **Usunięcie:** `Odinstaluj_4GE_Updater.bat` (plugin zostaje).
- Pliki programu i dziennik: `%LOCALAPPDATA%\4GE_Updater` (`log.txt` – wyślij go, gdy coś nie działa).
- Pierwsze uruchomienie: Windows może pokazać SmartScreen („Więcej informacji” → „Uruchom mimo to”).
