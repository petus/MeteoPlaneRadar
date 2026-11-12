#!/bin/sh
# Regenerates MeteoPlaneRadar/src/berry/generate - the constant tables Berry is
# built with. Run it after changing src/berry/src/berry_conf.h or updating the
# Berry sources. It needs a checkout of Berry for its generator (tools/coc):
#
#   BERRY=<path to a berry checkout> tools/berry_prebuild.sh
#
# The generated headers include "be_constobj.h" by name. The Arduino build puts
# only the sketch folder on the include path, so the name is rewritten to a
# path relative to the generate folder.
set -e
: "${BERRY:?set BERRY to a checkout of https://github.com/berry-lang/berry}"
dir=$(cd "$(dirname "$0")/../MeteoPlaneRadar/src/berry" && pwd)
rm -f "$dir"/generate/*.h
(cd "$dir" && python3 "$BERRY/tools/coc/coc" -o generate src -c src/berry_conf.h)
sed -i 's|#include "be_constobj.h"|#include "../src/be_constobj.h"|' "$dir"/generate/*.h
echo "$(ls "$dir/generate" | wc -l) headers in $dir/generate"
