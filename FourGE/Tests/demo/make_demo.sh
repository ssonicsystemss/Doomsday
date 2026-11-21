#!/bin/bash
# THE KICK TEST (owner 2026-11-16): every kick is judged in context - "EVERYBODY SCREAM", 150 BPM:
#   the bass line = 4GE kicks (everybody_scream_kick.mid), the melody = 5CR34M 4LPH4 "SCREECH - Owner 9 evbd scrm"
#   (everybody_scream_melody.mid = the owner's Melo.mid two octaves up), mixed with the melody matched to the kick by RMS.
# usage: FourGE/Tests/demo/make_demo.sh <4GE preset.4gp | - | "factory:<name>"> <out.mp3> [4GE id=value ...]
# needs: FourGE + ScreamAlpha RenderTest built (cmake ... -DPROPHECY_BUILD_TESTS=ON), python3 + scipy, ffmpeg
set -e
D="$(cd "$(dirname "$0")" && pwd)"; R="$(cd "$D/../../.." && pwd)"; T="$(mktemp -d)"
preset="$1"; out="$2"; shift 2
xvfb-run -a "$R/FourGE/build/RenderTest_artefacts/Release/RenderTest" "$T/kicks.wav" --midi "$preset" "$D/everybody_scream_kick.mid" 150 "$@"
[ -f "$T/../evbd_melody_cache.wav" ] || xvfb-run -a "$R/ScreamAlpha/build/RenderTest_artefacts/Release/RenderTest" "$T/../evbd_melody_cache.wav" --midi "Owner 9" "$D/everybody_scream_melody.mid" 150
python3 "$R/Shared/Tools/midi_demo.py" "$T/kicks.wav" "$T/../evbd_melody_cache.wav" "$out" -1
echo "demo: $out"
