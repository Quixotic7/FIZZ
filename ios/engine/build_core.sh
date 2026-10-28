#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# FIZZ's core for FIZZ for iOS: ios/FizzKit/Frameworks/FizzCore.xcframework (iphoneos arm64, iphonesimulator arm64 +
# x86_64, macOS arm64 for swift test and fizzshots) and FizzCore.json beside it, built locally and committed (as site/fizz.wasm
# is) so the app builds without FM1VST.
#
#   ios/engine/build_core.sh
#
# As site/build_wasm.sh: FM1VST's own core build (cmake/FM1Cores.cmake and core-glue/fizz/core.cmake, mirrored, never
# written) makes the core's static library per slice; fizzhost.c (the clock and the queues, fizzhost.h) is linked with
# it into one object whose only global symbols are fizzhost.h's (ld -r, exported list), then stripped.
#
# Environment:
#   FIZZ_DIR     the FIZZ sources (default: this checkout)
#   FM1VST_DIR   an FM1VST tree with core-glue/fizz (read only; default: the quadrille worktree)
#   FIZZ_IOS_OPT the optimisation level (default -Os)
#   FIZZ_VERSION the version the core reports (default: git describe of FIZZ_DIR)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
FIZZ_DIR="${FIZZ_DIR:-$(cd "$HERE/../.." && pwd)}"
FM1VST_DIR="${FM1VST_DIR:-/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/FM1VST/.claude/worktrees/quadrille}"
FIZZ_IOS_OPT="${FIZZ_IOS_OPT:--Os}"
OUT="$HERE/.build"
SRC="$OUT/src"
DEST="$HERE/../FizzKit/Frameworks"

[ -f "$FM1VST_DIR/core-api/fm1core.h" ] || { echo "build_core.sh: no FM1VST at $FM1VST_DIR (FM1VST_DIR)" >&2; exit 2; }
[ -f "$FM1VST_DIR/core-glue/fizz/core.cmake" ] || { echo "build_core.sh: $FM1VST_DIR has no core-glue/fizz" >&2; exit 2; }
[ -f "$FIZZ_DIR/core/glue/core_quadrille.c" ] || { echo "build_core.sh: no FIZZ sources at $FIZZ_DIR" >&2; exit 2; }

# ---- 1. a mirror of what the build reads from FM1VST (tracked files only)
mkdir -p "$SRC"
echo "== mirror of $FM1VST_DIR (cmake, core-api, core-glue)"
git -C "$FM1VST_DIR" ls-files -z cmake core-api core-glue | rsync -a --from0 --files-from=- "$FM1VST_DIR/" "$SRC/"
cp "$HERE/CMakeLists.txt" "$SRC/CMakeLists.txt"
FIZZ_VERSION="${FIZZ_VERSION:-$(git -C "$FIZZ_DIR" describe --tags --match 'v*' 2>/dev/null | sed 's/^v//')}"
if [ -n "$FIZZ_VERSION" ]; then
    sed -E -i.orig "s/^([[:space:]]*VERSION )[^)[:space:]]+/\1$FIZZ_VERSION/" "$SRC/core-glue/fizz/core.cmake"
    rm -f "$SRC/core-glue/fizz/core.cmake.orig"
fi
printf '_fzh_*\n_fizzcore_get\n' > "$OUT/exports.txt"

# ---- 2. per slice: the core's library (cmake), fizzhost.c, one object, a static library
# slice  cmake-system  sdk  deployment  arch  clang target
SLICES=(
    "ios iOS iphoneos 17.0 arm64 arm64-apple-ios17.0"
    "sim iOS iphonesimulator 17.0 arm64 arm64-apple-ios17.0-simulator"
    "simx86 iOS iphonesimulator 17.0 x86_64 x86_64-apple-ios17.0-simulator"
    "mac Darwin macosx 14.0 arm64 arm64-apple-macos14.0"
)
for s in "${SLICES[@]}"; do
    read -r name sys sdk dep arch triple <<< "$s"
    bld="$OUT/$name"
    sysroot="$(xcrun --sdk "$sdk" --show-sdk-path)"
    mkdir -p "$bld"
    echo "== $name: configure ($sdk $arch $dep, $FIZZ_IOS_OPT)"
    sysargs=()
    [ "$sys" = iOS ] && sysargs=(-DCMAKE_SYSTEM_NAME=iOS)
    cmake -S "$SRC" -B "$bld/cmake" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ${sysargs[@]+"${sysargs[@]}"} \
        -DCMAKE_OSX_SYSROOT="$sysroot" -DCMAKE_OSX_ARCHITECTURES="$arch" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$dep" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DFIZZ_DIR="$FIZZ_DIR" -DFIZZ_IOS_OPT="$FIZZ_IOS_OPT" > "$bld/configure.log" 2>&1 ||
        { cat "$bld/configure.log"; exit 1; }
    echo "== $name: build core_fizz"
    cmake --build "$bld/cmake" -j8 --target core_fizz > "$bld/build.log" 2>&1 || { tail -60 "$bld/build.log"; exit 1; }
    xcrun --sdk "$sdk" clang -target "$triple" -isysroot "$sysroot" "$FIZZ_IOS_OPT" -std=c11 -Wall -Wextra \
        -I"$SRC/core-api" -c "$HERE/fizzhost.c" -o "$bld/fizzhost.o"
    xcrun --sdk "$sdk" clang -target "$triple" -isysroot "$sysroot" -r -nostdlib "$bld/fizzhost.o" \
        -Wl,-force_load,"$bld/cmake/libcore_fizz.a" -Wl,-exported_symbols_list,"$OUT/exports.txt" -o "$bld/FizzCore.o"
    xcrun strip -x "$bld/FizzCore.o"
    rm -f "$bld/libFizzCore.a"
    xcrun libtool -static -o "$bld/libFizzCore.a" "$bld/FizzCore.o" 2> /dev/null
done
# (the simulator's two architectures in one library: a generic simulator build links both)
mkdir -p "$OUT/simu"
rm -f "$OUT/simu/libFizzCore.a"
xcrun lipo -create "$OUT/sim/libFizzCore.a" "$OUT/simx86/libFizzCore.a" -output "$OUT/simu/libFizzCore.a"
LIBS=("$OUT/ios/libFizzCore.a" "$OUT/simu/libFizzCore.a" "$OUT/mac/libFizzCore.a")

# ---- 3. the xcframework: the library and the headers (fizzhost.h, its module, the ABI's fm1core.h)
HDR="$OUT/include"
mkdir -p "$HDR"
cp "$HERE/fizzhost.h" "$HERE/module.modulemap" "$SRC/core-api/fm1core.h" "$HDR/"
mkdir -p "$DEST"
if [ -e "$DEST/FizzCore.xcframework" ]; then
    mv "$DEST/FizzCore.xcframework" "$OUT/old-xcframework-$$"
fi
args=()
for l in "${LIBS[@]}"; do args+=(-library "$l" -headers "$HDR"); done
xcodebuild -create-xcframework "${args[@]}" -output "$DEST/FizzCore.xcframework" > /dev/null

# ---- 4. FizzCore.json: what was built from what
ver=$(grep -E "^-- fizz core: version" "$OUT/mac/configure.log" | sed -E 's/.*version ([^,]*),.*/\1/')
commit=$(git -C "$FIZZ_DIR" rev-parse HEAD)
dirty=false
git -C "$FIZZ_DIR" diff --quiet HEAD -- core third_party 2>/dev/null || dirty=true   # (fizzhost.c: committed with it)
glue=$(git -C "$FM1VST_DIR" rev-parse HEAD)
sz() { wc -c < "$1" | tr -d ' '; }
date=$(date -u +%Y-%m-%dT%H:%M:%SZ)
cat > "$DEST/FizzCore.json" <<EOF
{
  "name": "FIZZ",
  "version": "$ver",
  "commit": "$commit",
  "dirty": $dirty,
  "fm1vst": "$glue",
  "bytes": { "ios-arm64": $(sz "$OUT/ios/libFizzCore.a"), "ios-arm64_x86_64-simulator": $(sz "$OUT/simu/libFizzCore.a"), "macos-arm64": $(sz "$OUT/mac/libFizzCore.a") },
  "built": "$date",
  "opt": "$FIZZ_IOS_OPT"
}
EOF
printf '== done: FizzCore.xcframework, FIZZ %s (%s%s); ios %s, sim %s, mac %s bytes\n' "$ver" "${commit:0:7}" \
    "$([ $dirty = true ] && echo ', dirty')" "$(sz "$OUT/ios/libFizzCore.a")" "$(sz "$OUT/simu/libFizzCore.a")" \
    "$(sz "$OUT/mac/libFizzCore.a")"
