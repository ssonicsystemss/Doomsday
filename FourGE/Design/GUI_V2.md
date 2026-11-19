# 4GE GUI v2 – CLEAN CONSOLE (owner 2026-11-19)

Owner: "żeby nie wyglądał jak AI slop tylko kompleksowy VST", "ma być clean", "profesjonalny wygląd",
"wszystko ładnie czytelne i uporządkowane i nie nasrane". Picked: **C (CONSOLE) with a switch to B (LANES)**.
Mockups: `gui_v2/4GE_C_Console.png` (main view), `gui_v2/4GE_B_Precision.png` (the second view).
Rejected: themed / glowing / ornate (the TikTok "Screecher" look) – "nie rób takiego".

## Tokens
| Token | Hex | Use |
|---|---|---|
| ground | #111214 | window |
| panel | #17181B | panels (flat, 1 px border #23262B, radius 5) |
| line | #23262B | borders, dividers, knob track |
| control | #1D1F23 | fields, buttons, knob cap |
| text | #E6E8EB / dim #8B9098 / mute #6E737B | |
| red | #E33135 | the ONE accent: selection, active, the fader of the selected strip |
| parts | PUNCH #E33135, BODY #E6A23C, TAIL #9B70D5, SUB #36A9D6, SCREECH #E65B9A, TOK / CLICK #A0A5AD | only on data: arcs, faders, waveform, the strip name bar |

Type: **Michroma** everywhere (owner 2026-11-19: picked B from 15 wide fonts, "musi być ta czcionka wszędzie"; "ważne, żeby były widoczne – skróty znane producentom").
Knob / segment labels in CAPITALS with producer abbreviations (`GeLook::abbrev`: LEN, ATK, DEC, SUS, REL, RES, FB, AMT, LVL, VEL...),
brighter label grey, every "%" drawn in Barlow (Michroma's % reads like "0/0"). (Was: Barlow Semi Condensed.)
Strip names = Bold caps (the console's scribble strips, the one strong typographic element); knob labels = Medium,
values = SemiBold; no monospace, no tracked-out eyebrow labels, no middle-dot meta strings.

## Rules
1. No sentences on the screen. Explanations live in the tooltips and the INFO LINE (status bar at the bottom).
2. No coloured bars on panel titles, no glows, no gradients, no glass. Colour = data only.
3. Knob size = importance (strip knobs 40, page knobs 40, macros 44). One knob drawing everywhere: track arc,
   value arc in the part colour, dark cap, light pointer.
4. Navigation on top: blocks KICK / SYNTHESIS / PROCESSING / MODULATION / SYSTEM, the pages of the block next to them.
5. MAIN = CONSOLE: the waveform with the pitch curve on top; strips TOK / CLICK / PUNCH / BODY / TAIL / SUB / SCREECH
   (type, 3 key knobs, fader + meter, mute / solo or AUTO / ON / OFF, the name bar = click = all parameters of the part);
   right column = PITCH + MASTER. A switch CONSOLE | LANES on the waveform: LANES = the parts as tracks with their
   envelopes (B) and the selected part's knobs on the right.
6. Everything sits on an 8 px grid; equal gaps between panels (12 px).

## Stage 2 (2026-11-19 evening): every page
- Barlow is the default font of the whole plugin (`GeLook::getTypefaceForFont`, shared look = the default look while 4GE is open).
- Buttons: an outline only, filled when ON / selected; a page's selected tab (its buttonColourId) = filled + a line in that colour.
  Button text in sentence case (`GeLook::caseOf`, short acronyms stay). Combo boxes: dark field, hairline.
- No instruction sentences on the pages (23 removed: "drag = ..., double-click = ..."); graph titles are short words.
  Inactive-drawing notes are quiet grey, one short sentence ("Drawing switches the envelope to Multipoint").
- Next: per page, bigger structural clean-ups where a page is still crowded (Layers table, Mod matrix, Export).
