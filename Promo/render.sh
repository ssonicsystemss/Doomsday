#!/usr/bin/env bash
# Renders Promo/out/Prophecy_of_Scream_TikTok.mp4 (1080x1920, 30 fps, H.264 + AAC)
# Needs: cmake, ninja, ffmpeg, xvfb-run (Linux). First build ~5 min, then ~5 min per render.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
OUT="$ROOT/Promo/out"
mkdir -p "$OUT"

if [ ! -d build-promo ]; then
  CXXFLAGS="-DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0 -DJUCE_USE_XRENDER=0" \
    cmake -B build-promo -G Ninja -DCMAKE_BUILD_TYPE=Release -DPROPHECY_BUILD_PROMO=ON
fi
cmake --build build-promo --target PromoRender -j"$(nproc)"

rm -f "$OUT/frames.fifo" "$OUT/video.mp4"
mkfifo "$OUT/frames.fifo"
ffmpeg -loglevel error -y -f rawvideo -pix_fmt bgra -s 1080x1920 -r 30 -i "$OUT/frames.fifo" \
       -c:v libx264 -preset slow -crf 17 -pix_fmt yuv420p "$OUT/video.mp4" &
FF=$!
xvfb-run -a -s "-screen 0 1800x1100x24" \
  build-promo/PromoRender_artefacts/Release/PromoRender "$OUT" "$ROOT/Promo/assets/kick.flac" "$OUT/frames.fifo" 2> "$OUT/render.log"
wait $FF
rm -f "$OUT/frames.fifo"

ffmpeg -loglevel error -y -i "$OUT/video.mp4" -i "$OUT/final.wav" -c:v copy -af "volume=-1.2dB" \
       -c:a aac -b:a 256k -ar 48000 -movflags +faststart -shortest "$OUT/Prophecy_of_Scream_TikTok.mp4"
echo "done: $OUT/Prophecy_of_Scream_TikTok.mp4"
