#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
# FIZZ's update loader (firmware/loader/) on this computer, no FM-1: ldr_test.c against both built packages, each with
# its own extents' table (firmware/build.py writes build/NAME-ldr_extents.h beside the package), then bad tables:
# refused when the loader compiles (_Static_assert) and, built without that, refused at run time before any erase.
#   python3 firmware/build.py && python3 firmware/build.py --ble    (the packages: build/fizz.fwsc, build/fizz-ble.fwsc)
#   bash firmware/app/tests/ldr_test.sh [USB.fwsc BLE.fwsc]          (default: those two)
set -euo pipefail
cd "$(dirname "$0")"
B=../build
USB="${1:-$B/fizz.fwsc}"
BLE="${2:-$B/fizz-ble.fwsc}"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/fizz-ldr-test.XXXXXX")"
CC="${CC:-cc}"
FLAGS=(-std=gnu11 -O2 -Wall -Wno-unused-function)
fail=0

for p in "fizz:$USB" "fizz-ble:$BLE"; do
    name="${p%%:*}" pkg="${p#*:}"
    [ -f "$pkg" ] && [ -f "$B/$name-ldr_extents.h" ] || { echo "no $pkg or $B/$name-ldr_extents.h: build it first"; exit 2; }
    "$CC" "${FLAGS[@]}" -include "$B/$name-ldr_extents.h" -o "$OUT/ldr_test_$name" ldr_test.c
    "$OUT/ldr_test_$name" "$pkg" || fail=1
done

# bad tables: "BLE|TABLE|static": the loader must not compile; "...|runtime": it compiles (the limits hold for each
# extent) and refuses at run time. Each is run against its package, without the asserts, and must be refused there
bad=("1|X(0x97000u, 0xaf000u) X(0xe5000u, 0xfc000u)|static|OBJ_BLE"
     "1|X(0xae000u, 0xb0000u) X(0xe7000u, 0xfc000u)|static|the Bluetooth store"
     "0|X(0x9f000u, 0xa0000u) X(0xe5000u, 0xfc000u)|static|the USB store"
     "0|X(0xe4000u, 0xfc000u)|static|the staged loader and its record"
     "0|X(0xe5000u, 0xfd000u)|static|0xFC000 and above"
     "0|X(0xe5100u, 0xfc000u)|static|not whole sectors"
     "1|X(0xe7000u, 0xfc000u) X(0x97000u, 0xaf000u)|runtime|out of order")
for t in "${bad[@]}"; do
    IFS='|' read -r ble tab when what <<<"$t"
    printf '#define FZ_LDR_BLE %s\n#define LDR_XTAB(X) %s\n' "$ble" "$tab" > "$OUT/bad.h"
    pkg=$([ "$ble" = 1 ] && echo "$BLE" || echo "$USB")
    if "$CC" "${FLAGS[@]}" -include "$OUT/bad.h" -o "$OUT/ldr_bad" ldr_test.c 2>/dev/null; then
        built=1
    else
        built=0
    fi
    if [ "$when" = static ] && [ $built = 1 ]; then
        echo "  bad table ($what): the loader compiled                         FAIL"; fail=1
    elif [ "$when" = runtime ] && [ $built = 0 ]; then
        echo "  bad table ($what): the loader did not compile                  FAIL"; fail=1
    else
        echo "  bad table ($what): $([ $built = 0 ] && echo 'refused when the loader compiles' || echo 'compiles')"
    fi
    "$CC" "${FLAGS[@]}" -DLDR_X_NO_ASSERT -include "$OUT/bad.h" -o "$OUT/ldr_bad" ldr_test.c
    "$OUT/ldr_bad" --bad "$pkg" | grep -v '^loader test' || fail=1
done
mv "$OUT" "$OUT.done"           # (left for a look: the binaries)
[ $fail = 0 ] && echo "ldr_test.sh: all passed" || echo "ldr_test.sh: FAILED"
exit $fail
