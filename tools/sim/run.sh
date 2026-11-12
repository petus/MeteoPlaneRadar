#!/bin/sh
# Builds the host simulator (see sim.cpp) and runs one card through it.
#
#   ARDUINOJSON=<dir with ArduinoJson.h, v7> tools/sim/run.sh <card.json> <answer.json> <outdir> [cz|en] [status]
#
# Needs g++ and Python 3 with Pillow (for the PNG conversion). The answer is the
# JSON the card's URL returned, saved to a file - e.g. with curl.
set -e
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../MeteoPlaneRadar
: "${ARDUINOJSON:?set ARDUINOJSON to the directory that holds ArduinoJson.h (v7)}"
card=$1; answer=$2; out=$3; lang=${4:-cz}; status=${5:-$(date +%H:%M)}
[ -n "$out" ] || { echo "usage: $0 <card.json> <answer.json> <outdir> [cz|en] [status]"; exit 2; }
mkdir -p "$out"
bin=${SIM_BUILD:-$out}/sim
if [ ! -x "$bin" ] || [ -n "$SIM_REBUILD" ]; then
  gcc -c -O1 -I "$src" "$src/qrcode.c" -o "$out/qrcode.o"
  g++ -std=c++17 -O1 -w \
    -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=1 \
    -DARDUINOJSON_ENABLE_ARDUINO_PRINT=1 -DARDUINOJSON_ENABLE_PROGMEM=0 \
    -I "$here/shim" -I "$ARDUINOJSON" -I "$src" \
    "$here/sim.cpp" "$src/ScreenCards.cpp" "$src/Cards.cpp" "$src/UI.cpp" "$src/Layout.cpp" \
    "$src/AAFont.cpp" "$src/Lang.cpp" "$out/qrcode.o" -o "$bin"
fi
"$bin" "$card" "$answer" "$out" "$lang" "$status"
for f in "$out"/*.ppm; do
  python3 "$here/ppm2png.py" "$f" "${f%.ppm}.png"
done
python3 "$here/ppm2png.py" "$out/2-card.ppm" "$out/preview.png" --round
echo "pictures are in $out (preview.png is the round one)"
