#!/bin/bash
cd "$(dirname "$0")"
while read name note file; do
  [ -f "tune/$name.json" ] && [ "$1" != "force" ] && continue
  python3 -W ignore tools/ktune.py "$file" "$name" 80 "$note" 1 | grep -E "FINAL"
done <<'LIST'
nc 31 ref/002_Noisecontrollers_WS_BD.wav
ws 30 ref/004_Wildstylez_Unleash_The_Beast_Kick.wav
zt 29 ref/005_Zatox_Wakanda_Kick_F.wav
f009 32 ref/FHSP_Hardstyle_Kicks_009_-_Note_G1.wav
f019 33 ref/FHSP_Hardstyle_Kicks_019_-_Old_Kick_Pick.wav
f027 31 ref/FHSP_Hardstyle_Kicks_027_-_Old_Kick_Pick.wav
rb02 31 ref/FHSP_Reversed_Bass_Kicks_002_-_Note_G1.wav
rb24 31 ref/FHSP_Reversed_Bass_Kicks_024_-_Note_G1.wav
rb32 31 ref/FHSP_Reversed_Bass_Kicks_032_-_Note_G1.wav
rb40 32 ref/FHSP_Reversed_Bass_Kicks_040_-_Note_G1.wav
gt08 36 ref/GTKick08.wav
hs04 32 ref/HARDSTYLE-04.wav
hk10 33 ref/HardKick10EDITnoClick.wav
h09 32 ref/Harder09.wav
vul 29 ref/OPS_FREE_KICK1_VULCANO_F.wav
gal 29 ref/OPS_FREE_KICK2_GALACTIC_F.wav
s25 32 ref/Soft25.wav
k32 31 ref/kick_hardstyle_32.wav
k55 34 ref/kick_hardstyle_55.wav
k77 30 ref/kick_hardstyle_77.wav
szp 31 ref2/OPS_FREE_SZPTYPE_KICK_33_F-D.wav
z10 28 ref2/OPS_FREE_ZAAGKICK_10_E_155.wav
z15 34 ref2/OPS_FREE_ZAAGKICK_15_A_155.wav
z22 29 ref2/OPS_FREE_ZAAGKICK_22_F_155.wav
z9 32 ref2/OPS_FREE_ZAAGKICK_9_G_155.wav
k73 35 ref2/OPS_HS_FREE_2_7_KICK_3_B.wav
psy5 34 ref2/OPS_HS_FREE_2_PSY_KICK_5.wav
psy8 34 ref2/OPS_HS_FREE_2_PSY_KICK_8.wav
tri4 33 ref2/Tritone_-_Kick_Short_4.wav
tric 33 ref2/Tritone_-_Main_Kick_Crunch_2.wav
elec 33 ref2/electro-high-distorted-hardstyle-kick_A_major.wav
revc 36 ref2/hardstyle-reverbed-kick_C.wav
mass 36 ref2/mass-distorted-kick_B_minor.wav
rawp 30 ref2/rawstyle-kick-powerful.wav
raws 29 ref2/rawstyle-kick-slow-attack.wav
rbg 31 ref2/reverse-bass-kick_150bpm_G_minor.wav
upp 30 ref2/uptempo-kick-piercing-slide_200bpm_F.wav
upr 31 ref2/uptempo-rawstyle-kick_190bpm_C.wav
LIST
echo ALLDONE
