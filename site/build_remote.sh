#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# "Connect FM-1" for the site: RemoteFM1's web back end, copied from RemoteFM1 (read only) and committed here.
#
#   site/build_remote.sh            (rarely: only when RemoteFM1's protocol or web/remote changes)
#
# Writes site/remote/remote.js (RemoteFM1 web/remote/remote.js, unchanged) and site/remote/remotefm1.mjs
# (libremotefm1's portable core, host/src/{codec,screen,panel,session}.c + web/remote/src/rfm_web.c, compiled with
# Emscripten exactly as RemoteFM1's web/remote/build.sh does; one ES module, the wasm inlined).
#
# Environment:
#   REMOTEFM1_DIR  a RemoteFM1 clone (default: the one next to this repo); nothing in it is written
#   REMOTEFM1_REF  the commit to take (default origin/main: the published site's)
#   EMSDK          Emscripten's SDK (default ~/emsdk)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REMOTEFM1_DIR="${REMOTEFM1_DIR:-/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/RemoteFM1}"
REMOTEFM1_REF="${REMOTEFM1_REF:-origin/main}"
EMSDK="${EMSDK:-$HOME/emsdk}"
SRC="$HERE/.build/remotefm1"

# shellcheck disable=SC1091
command -v emcc > /dev/null || source "$EMSDK/emsdk_env.sh" > /dev/null 2>&1
command -v emcc > /dev/null || { echo "build_remote.sh: no emcc (EMSDK=$EMSDK)" >&2; exit 2; }

commit=$(git -C "$REMOTEFM1_DIR" rev-parse "$REMOTEFM1_REF")
echo "== RemoteFM1 $REMOTEFM1_REF ($commit)"
[ -d "$SRC" ] && mv "$SRC" "$SRC.old.$$"
mkdir -p "$SRC"
git -C "$REMOTEFM1_DIR" archive "$commit" host proto firmware/src web/remote | tar -x -C "$SRC"

mkdir -p "$HERE/remote"
cd "$SRC"
emcc -O2 -std=c99 -Wall -Wextra -Wno-unused-parameter -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,worker,node \
    -sSINGLE_FILE=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=8MB -sFILESYSTEM=0 \
    -sEXPORTED_FUNCTIONS=_malloc,_free -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPU16,HEAPU32,UTF8ToString,stringToUTF8 \
    -Ihost/include -Iproto -Ifirmware/src -sEXPORT_NAME=createRemoteFM1 \
    host/src/codec.c host/src/screen.c host/src/panel.c host/src/session.c web/remote/src/rfm_web.c \
    -o "$HERE/remote/remotefm1.mjs"
cp web/remote/remote.js "$HERE/remote/remote.js"
echo "{ \"remotefm1\": \"$commit\" }" > "$HERE/remote/source.json"
ls -l "$HERE/remote"
