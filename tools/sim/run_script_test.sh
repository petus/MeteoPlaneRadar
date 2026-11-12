#!/bin/sh
# Builds and runs the host test of script cards (see scripttest.cpp).
#
#   ARDUINOJSON=<dir with ArduinoJson.h, v7> tools/sim/run_script_test.sh [workdir]
set -e
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../MeteoPlaneRadar
: "${ARDUINOJSON:?set ARDUINOJSON to the directory that holds ArduinoJson.h (v7)}"
work=${1:-$(mktemp -d)}
mkdir -p "$work/berry"
gcc -c -O1 -I "$src" "$src/qrcode.c" -o "$work/qrcode.o"
for f in "$src"/src/berry/src/*.c; do gcc -c -O1 -w "$f" -o "$work/berry/$(basename "$f" .c).o"; done
g++ -std=c++17 -O1 -w \
  -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=1 \
  -DARDUINOJSON_ENABLE_ARDUINO_PRINT=1 -DARDUINOJSON_ENABLE_PROGMEM=0 -DMPR_HOST_TEST=1 \
  -I "$here/shim" -I "$ARDUINOJSON" -I "$src" \
  "$here/scripttest.cpp" "$src/ScreenCards.cpp" "$src/Cards.cpp" "$src/Script.cpp" "$src/UI.cpp" "$src/Layout.cpp" \
  "$src/AAFont.cpp" "$src/Lang.cpp" "$work/qrcode.o" "$work"/berry/*.o -o "$work/scripttest"
"$work/scripttest"
