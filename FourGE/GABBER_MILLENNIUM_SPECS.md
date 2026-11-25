# GABBER / MILLENNIUM - the owner's specs (2026-11-24 / 25), condensed

## GABBER ("4GE - GABBER KICK ENGINE")
909 / BD source -> FAST pitch drop (+12..24 st, 30-70 ms, EXP / FAST) -> STABLE tonal tail (tail = bass, long, amplitude falls
slowly, pitch stays) -> PRE-EQ -> OVERDRIVE -> HARD CLIP -> ASYMMETRY (+ / - drive, bias) -> POST-EQ -> HARD CLIP 2 -> FINAL CLIP,
a distortion LOW-PASS / HIGH CUT (darker than RAW / UPTEMPO). Short punch kept out of the extreme distortion. CRUNCH = generated
by the distortion (no separate layer by default), SUB optional, TOK optional, SCREAM / REVERB off. Not RAW + drive, not SINE +
distortion, not "more highs = more gabber". "DISTORTION CREATES THE KICK."
Owner by ear (2026-11-24/25): "bardziej pierdzacy", "za bardzo syczy", "przekombinowany - takie kicki robili w 1994": a short
EDM punch + clipper 100 % / an FPC kick + 2x Fruity Fast Dist (PRE 100 %). Refs: hardcore-gabber-kick-8 / -162.

## MILLENNIUM ("4GE - MILLENNIUM HARDCORE KICK", 1999-2008, 150-180 BPM)
More controlled / tonal / punchy than gabber: 909 / synth BD -> pitch drop (40-100 ms, keep the natural bend: distortion must
not kill it) -> STRONG punch (low-mid, f = root x harmonic) -> BODY -> PRE-EQ (low-mid, 2nd-harmonic emphasis) -> DISTORTION ->
EQ -> COMPRESSOR (~50 ms attack: punch survives) -> DISTORTION 2 -> ASYMMETRY (moderate) -> tail shaping -> POST-EQ -> FINAL CLIP.
Band-limited distortion (low less, mid most, high controlled), crunch integrated (L = Lbase + kP Lpunch + kB Lbody), tail
200-300 ms tonal = bass, filter subtle / off, optional RESAMPLE / POGO (not reverse bass), PERIOD CORRECT mode, sub-profiles
EARLY / CLASSIC / DARK / MAINSTREAM / MODERN. "DISTORTION SHAPES THE KICK."
Owner by ear: the ref Tritone Kick Main 1 is right; its character is in the ref WITHOUT 350-2500 Hz (low + highs): measured =
a saturated note (h2-h4 -16..-20 dB) + bright noise bursts ~6 kHz twice per cycle, -13 dB at the start -> -24 dB in the tail.
"EARLY Classic Boom + clipper 100 %" sounded "a bit millennium".

## Engine pieces added for these (TAIL TYPE HARDCORE, all off by default)
hc_clip2, hc_lp, hc_fuzz, hc_harm_db / _n, hc_comp / _atk / _rel, hc_ms_click + hc_click_len / _noise / _bal, hc_ms_track
FOLLOW PITCH, hc_ms_own, hc_pre_decay, hc_fd1_* / hc_fd2_* / hc_fd_asym / _pos / _neg / _bias (FAST_DIST.md).
