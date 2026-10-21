# 5CR34M 4LPH4 (v0.1 – wersja testowa)
Nowy syntezator screechy do hardstyle / rawstyle, następca Prophecy of Scream, pisany od zera na podstawie twoich wzorców
(analiza: `PLAN.md`). VST3 (instrument) do FL Studio.

## Jak zdobyć
GitHub → zakładka **Actions** → *Build 5CR34M 4LPH4 VST3* → ostatni build → **Artifacts** → `5CR34M-4LPH4-VST3-Windows`.
Folder `5CR34M 4LPH4.vst3` skopiuj do `C:\Program Files\Common Files\VST3\`, w FL: *Options → Manage plugins → Find installed plugins*.

## Jak to działa (tor dźwięku)
```
NOTE THROW (zjazd / podjazd wysokości na KAŻDEJ nucie, też legato) + GLIDE
   ├─ CORE  – czysty rdzeń: sinus → trójkąt → kwadrat (SHAPE), oktawa -1 / 0 / +1
   └─ HASH  – brudna góra: 1–7 rozstrojonych głosów (DETUNE, SAW), FM (RATIO, FM), CHAOS (każdy głos pływa po swojemu),
              GRIT (odrobina szumu – dużo = odkurzacz), LOW CUT, DECAY / SUSTAIN (hasz najjaśniejszy na starcie nuty)
              → SCREAM: DRIVE (5 krzywych) → COMB (filtr grzebieniowy strojony do nuty) → FORMANT (szczyt „głosu”)
CORE + HASH → OUTPUT: DRIVE 2 → TONE → LOW CUT → AMP (ADSR) → CHORUS (szerokość jak w hooverze) → VOLUME
```
UNDO / REDO na górze. Dwuklik na gałce = wartość domyślna, klik w liczbę = wpisz wartość.

## Presety (v0.1)
- **HOOVER - Classic Juno** – rozstrojone piły + kwadrat oktawę niżej, podjazd od dołu, chorus.
- **HOOVER - Rave Dive** – zjazd z góry o oktawę na każdej nucie, mocniejszy przester i formant.
- **HOOVER - Hardstyle Scream** – ostrzejszy: FM, twardy przester, filtr grzebieniowy, krótki rzut nuty.
- **INIT** – punkt startowy.

To wersja do testów: dźwięk i gałki będą się jeszcze zmieniać. Pisz, co brzmi źle.
