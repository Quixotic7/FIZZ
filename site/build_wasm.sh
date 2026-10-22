#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# FIZZ for the browser: site/fizz.wasm and site/fizz.json, built locally and committed (the Pages workflow only
# publishes site/; it compiles nothing).
#
#   site/build_wasm.sh
#
# The approach is RemoteFM1's native engine (web/engines/native/build.sh, GPL-3.0-only): FM1VST's own core build
# (cmake/FM1Cores.cmake and the fizz glue, core-glue/fizz/core.cmake) under Emscripten, the core's static library
# linked with engine/wasm_glue.c into one standalone module that engine/engine.js drives.
#
# Environment:
#   FIZZ_DIR     the FIZZ sources (default: this checkout, the folder above site/)
#   FM1VST_DIR   an FM1VST tree with core-glue/fizz (read only; default: the quadrille worktree, branch core-quadrille)
#   EMSDK        Emscripten's SDK (default ~/emsdk)
#   FM1WEB_OPT   the optimisation level (default -Os)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
FIZZ_DIR="${FIZZ_DIR:-$(cd "$HERE/.." && pwd)}"
FM1VST_DIR="${FM1VST_DIR:-/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/FM1VST/.claude/worktrees/quadrille}"
EMSDK="${EMSDK:-$HOME/emsdk}"
FM1WEB_OPT="${FM1WEB_OPT:--Os}"
OUT="$HERE/.build"
SRC="$OUT/src"
BLD="$OUT/cmake"

[ -f "$FM1VST_DIR/core-api/fm1core.h" ] || { echo "build_wasm.sh: no FM1VST at $FM1VST_DIR (FM1VST_DIR)" >&2; exit 2; }
[ -f "$FM1VST_DIR/core-glue/fizz/core.cmake" ] || { echo "build_wasm.sh: $FM1VST_DIR has no core-glue/fizz" >&2; exit 2; }
[ -f "$FIZZ_DIR/core/glue/core_quadrille.c" ] || { echo "build_wasm.sh: no FIZZ sources at $FIZZ_DIR (FIZZ_DIR)" >&2; exit 2; }
# shellcheck disable=SC1091
command -v emcc > /dev/null || source "$EMSDK/emsdk_env.sh" > /dev/null 2>&1
command -v emcc > /dev/null || { echo "build_wasm.sh: no emcc (EMSDK=$EMSDK)" >&2; exit 2; }
WASM_OPT="$(dirname "$(command -v emcc)")/../bin/wasm-opt"
[ -x "$WASM_OPT" ] || WASM_OPT="$EMSDK/upstream/bin/wasm-opt"

# ---- 1. a mirror of what the build reads from FM1VST (tracked files only; FM1VST is never written)
mkdir -p "$SRC" "$BLD"
echo "== mirror of $FM1VST_DIR (cmake, core-api, core-glue)"
git -C "$FM1VST_DIR" ls-files -z cmake core-api core-glue | rsync -a --from0 --files-from=- "$FM1VST_DIR/" "$SRC/"
cp "$HERE/engine/CMakeLists.txt" "$SRC/CMakeLists.txt"

# ---- 2. the core's static library under Emscripten
echo "== configure (emcmake, $FM1WEB_OPT, FIZZ_DIR=$FIZZ_DIR)"
emcmake cmake -S "$SRC" -B "$BLD" -DCMAKE_BUILD_TYPE=Release -DFIZZ_DIR="$FIZZ_DIR" \
    -DFM1WEB_CORO="$HERE/engine/coro_wasm.c" -DFM1WEB_OPT="$FM1WEB_OPT" > "$BLD/configure.log" 2>&1 ||
    { cat "$BLD/configure.log"; exit 1; }
grep -E "fizz core: version" "$BLD/configure.log" || true
echo "== build core_fizz"
cmake --build "$BLD" -j8 --target core_fizz > "$BLD/build.log" 2>&1 || { tail -60 "$BLD/build.log"; exit 1; }

# ---- 3. link: one standalone module, no Emscripten JavaScript
echo "== link fizz.wasm"
emcc "$FM1WEB_OPT" -sSTANDALONE_WASM --no-entry -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=33554432 \
    -sSTACK_SIZE=1048576 -sERROR_ON_UNDEFINED_SYMBOLS=1 -sSUPPORT_LONGJMP=wasm -I"$SRC/core-api" -Wall -Wextra \
    "$HERE/engine/wasm_glue.c" "$BLD/libcore_fizz.a" -o "$OUT/fizz.wasm"
# (a core whose main loop runs on the coroutine imports fm1w_co_yield: Asyncify then, as RemoteFM1 does)
if node -e 'const m=new WebAssembly.Module(require("fs").readFileSync(process.argv[1]));process.exit(WebAssembly.Module.imports(m).some(i=>i.name==="fm1w_co_yield")?0:1)' "$OUT/fizz.wasm"; then
    "$WASM_OPT" "$OUT/fizz.wasm" --enable-bulk-memory --enable-nontrapping-float-to-int --enable-sign-ext \
        --enable-mutable-globals --asyncify --pass-arg=asyncify-imports@env.fm1w_co_yield "$FM1WEB_OPT" -o "$OUT/fizz.wasm"
fi
cp "$OUT/fizz.wasm" "$HERE/fizz.wasm"

# ---- 4. fizz.json: what the page shows and the feedback form carries
ver=$(grep -E "^-- fizz core: version" "$BLD/configure.log" | sed -E 's/.*version ([^,]*),.*/\1/')
commit=$(git -C "$FIZZ_DIR" rev-parse HEAD)
dirty=false
git -C "$FIZZ_DIR" diff --quiet HEAD -- core third_party 2>/dev/null || dirty=true
glue=$(git -C "$FM1VST_DIR" rev-parse HEAD)
size=$(wc -c < "$HERE/fizz.wasm" | tr -d ' ')
date=$(date -u +%Y-%m-%dT%H:%M:%SZ)
cat > "$HERE/fizz.json" <<EOF
{
  "name": "FIZZ",
  "version": "$ver",
  "commit": "$commit",
  "dirty": $dirty,
  "fm1vst": "$glue",
  "bytes": $size,
  "built": "$date",
  "opt": "$FM1WEB_OPT"
}
EOF
printf '== done: site/fizz.wasm %s bytes, FIZZ %s (%s%s)\n' "$size" "$ver" "${commit:0:7}" "$([ $dirty = true ] && echo ', dirty')"
