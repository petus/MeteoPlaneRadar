#!/bin/sh
# Builds and runs the host test of the card store (see storetest.cpp).
#
#   ARDUINOJSON=<dir with ArduinoJson.h, v7> [MBEDTLS=<prefix>] tools/sim/run_store_test.sh [workdir]
#
# Needs g++, openssl and mbedTLS for the PC (libmbedtls-dev, or MBEDTLS pointing
# at a directory that holds include/ and lib/ of it). Two of the cases need
# indexes signed with the store's private key and are skipped without it.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$here/../..
src=$root/MeteoPlaneRadar
: "${ARDUINOJSON:?set ARDUINOJSON to the directory that holds ArduinoJson.h (v7)}"
work=${1:-$(mktemp -d)}
fx=$work/fixtures
mkdir -p "$fx"
key=${MPR_STORE_KEY:-$HOME/.config/meteoplaneradar/store-signing-key.pem}

python3 "$root/tools/make_index.py" --check

cp "$root"/cards/index.json "$root"/cards/index.json.sig "$root"/cards/kurzy-cnb.json "$root"/cards/svatek.json "$root"/cards/den.json "$fx/"
# an index changed after signing: one letter of a title
sed 's/"title": "Svátek"/"title": "Svatek"/' "$fx/index.json" > "$fx/index-changed.json"
# a signature that is valid, but made with a key the firmware does not know
openssl ecparam -name prime256v1 -genkey -noout -out "$work/foreign.pem" 2>/dev/null
openssl dgst -sha256 -sign "$work/foreign.pem" "$fx/index.json" | base64 -w0 > "$fx/index-foreign.sig"
: > "$fx/empty.sig"
echo "bm90IGEgc2lnbmF0dXJl" > "$fx/rubbish.sig"
if [ -f "$key" ]; then
  sign() { openssl dgst -sha256 -sign "$key" "$1" | base64 -w0 > "$1.sig"; }
  sed 's/"file": "svatek.json"/"file": "..\/..\/..\/secret.json"/' "$fx/index.json" > "$fx/index-evil.json"
  sign "$fx/index-evil.json"
  sed 's/"serial": [0-9]*/"serial": 999/' "$fx/index.json" > "$fx/index-newer.json"
  sign "$fx/index-newer.json"
fi

inc=""; lib="-lmbedcrypto"
if [ -n "$MBEDTLS" ]; then
  inc="-I $MBEDTLS/include"
  lib="$(find "$MBEDTLS" -name libmbedcrypto.a | head -1)"
fi
g++ -std=c++17 -O1 -w \
  -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=1 \
  -DARDUINOJSON_ENABLE_ARDUINO_PRINT=1 -DARDUINOJSON_ENABLE_PROGMEM=0 \
  -I "$here/shim" -I "$ARDUINOJSON" $inc -I "$src" \
  "$here/storetest.cpp" "$src/Store.cpp" "$src/Cards.cpp" $lib -o "$work/storetest"
"$work/storetest" "$fx"
