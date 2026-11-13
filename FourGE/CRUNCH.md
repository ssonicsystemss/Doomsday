# 4GE – CRUNCH (spec, właściciel 2026-11-14)

Skąd: 100 starych kicków FHSP (euphoric 2011–13) zmierzone w strefach 0–3 / 3–10 / 10–25 / 25–45 / 45+ ms:
- uderzenie **trzyma ~650 Hz przez ~20 ms**, potem powoli schodzi (35 ms ~490, 70 ms ~550, 100 ms ~430, 150 ms ~230 Hz) – nie ma zjazdu z 2 kHz (to daje „piep”);
- bas < 180 Hz dochodzi do połowy siły dopiero ~54 ms (33–74);
- 1–8 kHz przez pierwsze ~50 ms w połowie szum (flatness ~0.35–0.4) = **crunch**.
Testy na ucho: sinus 1–2 kHz → 50 Hz bez przesteru = piep (D1–D6), sam przester na sinusie = przester, nie trzask (F1–F6);
fala 650 Hz trzymana 20 ms + ziarno przed przesterem asymetrycznym = TRZASK (G3). Asymetryczny waveshaper > soft / hard.

## Model
TOK + PUNCH + CRUNCH = jedno złożone zdarzenie transientowe, płynnie w BODY → TAIL (+ SCREECH).
**CRUNCH ≠ SCREECH**: crunch bardzo krótki, związany z punchem, nieregularny, bogaty harmonicznie, nie brzmi jak osobny synth;
screech dłuższy, tonalny, rezonansowy, żyje w tailu.

## Routing
```
PUNCH SOURCE (50–1500 Hz, jeden zjazd pitchu)
   ├── CLEAN PATH ── envelope ───────────────────────────────┐
   └── CRUNCH PATH ── drive + bias (asym) ── MOVING BANDPASS ── harmonic shape ── crunch envelope ── makeup
                                                              └── CROSSFADE (wewnętrzny BLEND, w oknie crunchu) → PUNCH OUT → BODY → TAIL
```

## Jedna gałka CRUNCH 0–100 % (steruje strukturą harmoniczną, nie głośnością)
| CRUNCH | 0 % | 50 % | 100 % |
|---|---|---|---|
| Harmonic drive (wewnętrzny pre-gain przed nieliniowością, + makeup) | +18 dB | +30 dB | +42 dB |
| Asymmetry / bias | lekka | średnia | mocna |
| Start band (moving BP) | 2.0 kHz | 3.25 kHz | 4.5 kHz |
| End band | 700 Hz | 850 Hz | 1.0 kHz |
| Bandwidth | wąskie | średnie | szerokie |
| Peak time | 6 ms | 8 ms | 10 ms |
| End time (musi wygasnąć zanim TAIL dominuje – inaczej „fizz”) | 25 ms | 42 ms | 60 ms |
| Upper-mid boost | 0 dB | +3 dB | +6 dB |
| Odbiór | czysty punch | KRA (klasyczny) | KRRRRAAA (rawstyle) |

Zasady:
- **0 % = bypass** ścieżki crunch (brak słyszalnego efektu), a nie „brak drive'u”.
- **100 % ≠ głośniej**: automatyczna kompensacja poziomu po całym procesie – słychać zmianę charakteru, nie głośności.
- **Pasmo śledzi nutę**: częstotliwości × (nuta / G1)^track (track 0..1) – inna nuta nie ma „KRRR” w tym samym miejscu widma.
- 25 % „KA”, 50 % „KRA”, 75 % „KRRRA”, 100 % „KRRRRAAA” – CRUNCH to parametr epoki / stylu (EUPHORIC ~50, RAW ~100).

Prototyp offline: `Tools/crunch_proto.py` (renderuje 0 / 25 / 50 / 75 / 100 % z wyrównaną głośnością).
