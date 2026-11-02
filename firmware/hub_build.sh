#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
# The signed release build on GitHub's runner: The Firmware Hub's shared workflow (zvenson/fm1-build@v1) runs this as
# its build command (.github/workflows/release.yml; BUILDING.md "Signed builds"). It also runs on the owner's Mac.
#   firmware/hub_build.sh [VERSION]        build VERSION's two packages: the version from the argument, else from the
#                                          tag (vX.Y.Z[-beta.N]), else (a run from the Actions tab) the run's input
#   firmware/hub_build.sh --version [V]    print version=V and prerelease=true|false (release.yml's outputs)
#   firmware/hub_build.sh --check DIR [V]  check the five files in DIR (release names, fizz-V.fwsc, ..., or the plain
#                                          ones, fizz.fwsc, ...) against site/firmware/releases.json and the
#                                          committed site/firmware/V/, no build
# The build is firmware/release.py V --rebuild: the very builds firmware/release.py V made for the install page (the
# same APP_VERSION, identity and stamp checks), its five files and SHA256SUMS into firmware/app/build/hub-V/. Each
# sha256 is then checked against releases.json and the committed files when the version is listed (on a tag it must
# be), and on GitHub's runner the files are copied to the repository root under the release's names
# (fizz-V.fwsc, fizz-xdata-V.bin, fizz-ble-V.fwsc, fizz-ble-xdata-V.bin, fizz-ble-xdata2-V.bin): the shared workflow
# attests those (its "packages": fizz-*), keeps them as the run's artifact and, on a tag, uploads them with its
# SHA256SUMS to the draft release. A file whose sha256 is not the listed one stops here, before anything is signed.
# Pillow is pinned to the release builds' (fm1-base's font generator draws the UI fonts with it) and must have Raqm.
set -euo pipefail
cd "$(dirname "$0")/.."

PILLOW_VERSION=11.1.0           # the release builds' (another version may draw the fonts otherwise)
NAMES=(fizz.fwsc fizz-xdata.bin fizz-ble.fwsc fizz-ble-xdata.bin fizz-ble-xdata2.bin)

mode=build
case "${1:-}" in
    --version) mode=version; shift ;;
    --check) mode=check; dir="${2:?--check DIR}"; shift 2 ;;
esac

# the version: the argument, the tag, or the version input of a run from the Actions tab (read from the event file,
# never put into a command line)
if [ -n "${1:-}" ]; then
    v="$1"
elif [ "${GITHUB_REF_TYPE:-}" = tag ]; then
    v="${GITHUB_REF_NAME#v}"
elif [ "${GITHUB_EVENT_NAME:-}" = workflow_dispatch ]; then
    v="$(python3 -c 'import json, os; print(json.load(open(os.environ["GITHUB_EVENT_PATH"]))["inputs"]["version"])')"
else
    echo "hub_build.sh: no version (an argument X.Y.Z[-beta.N], a tag vX.Y.Z[-beta.N] or a run's version input)" >&2
    exit 1
fi
# (firmware/release.py VERSION_RE; a -suffix, 0.12.0-beta.13-test, is for a build from the Actions tab only)
if ! [[ "$v" =~ ^[0-9]{1,2}\.[0-9]{1,2}\.[0-9]{1,2}(-beta\.[0-9]{1,3})?(-[a-z0-9]+)?$ ]]; then
    echo "::error::version '$v' is not X.Y.Z or X.Y.Z-beta.N (tags are vX.Y.Z[-beta.N])"
    exit 1
fi
if [ "${GITHUB_REF_TYPE:-}" = tag ] && ! [[ "$v" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-beta\.[0-9]+)?$ ]]; then
    echo "::error::the tag v$v: a release is vX.Y.Z or vX.Y.Z-beta.N (a -suffix is for a build from the Actions tab)"
    exit 1
fi
if [ "$mode" = version ]; then
    echo "version=$v"
    if [[ "$v" == *-* ]]; then echo "prerelease=true"; else echo "prerelease=false"; fi
    exit 0
fi

# the five files in $1 against site/firmware/releases.json and site/firmware/V/: a version not listed is an error on a
# tag (firmware/release.py lists it before the tag), a notice otherwise
check() {
    python3 - "$v" "$1" "${NAMES[@]}" <<'PY'
import hashlib, json, os, sys
from pathlib import Path
v, d, names = sys.argv[1], Path(sys.argv[2]), sys.argv[3:]
asset = lambda n: f"{n.rsplit('.', 1)[0]}-{v}.{n.rsplit('.', 1)[1]}"     # fizz.fwsc -> fizz-V.fwsc
def file(n):
    for p in (d / asset(n), d / n):
        if p.is_file():
            return p
    sys.exit(f"::error::{d} has no {asset(n)} (nor {n})")
got = {n: hashlib.sha256(file(n).read_bytes()).hexdigest() for n in names}
entry = next((r for r in json.loads(Path("site/firmware/releases.json").read_text())["releases"]
              if r.get("version") == v), None)
if entry is None:
    if os.environ.get("GITHUB_REF_TYPE") == "tag":
        sys.exit(f"::error::{v} is not in site/firmware/releases.json: firmware/release.py {v} lists it, then the tag")
    print(f"::notice::{v} is not in site/firmware/releases.json: built, not a release (the install page will not offer it)")
    for n in names:
        print(f"  {got[n]}  {asset(n)}")
    sys.exit(0)
want = {}
for p in entry["packages"]:
    for key in ("fwsc", "data", "data2"):
        if key in p:
            want[p[key]["path"].split("/", 1)[1]] = p[key]["sha256"]
if sorted(want) != sorted(names):
    sys.exit(f"::error::releases.json's {v} lists {sorted(want)}, not the five files {sorted(names)}")
site = Path("site/firmware") / v
bad = [n for n in names if got[n] != want[n]]
bad += [f"{n} (site/firmware/{v}/)" for n in names
        if not (site / n).is_file() or hashlib.sha256((site / n).read_bytes()).hexdigest() != got[n]]
if bad:
    sys.exit(f"::error::{', '.join(bad)}: not the sha256 releases.json and site/firmware/{v}/ hold for {v} "
             "(or missing): this build is not the release the install page serves")
for n in names:
    print(f"  {got[n]}  {asset(n)}")
print(f"site/firmware/releases.json and site/firmware/{v}/: {v}'s five sha256s match")
PY
}
if [ "$mode" = check ]; then
    check "$dir"
    exit 0
fi

PY="${PYTHON:-python3}"
# (the Mac: Pillow finds Homebrew's libraqm, as firmware/build.py's DYLD_LIBRARY_PATH does for the build)
if [ "$(uname -s)" = Darwin ] && [ -z "${DYLD_FALLBACK_LIBRARY_PATH:-}" ] && [ -f /opt/homebrew/lib/libraqm.dylib ]; then
    export DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib
fi
# GitHub's runner: Raqm's libraries (Pillow's wheels load them at run time), make (firmware/app/tools/gen_units.py
# reads tools/Makefile's lists; the runner image has it) and the pinned Pillow (the shared workflow installs the
# newest), then fm1-base and felucca-bsp (the shared workflow's checkout fetches no submodules)
if [ "${GITHUB_ACTIONS:-}" = true ]; then
    sudo apt-get update -q
    sudo apt-get install -y -q --no-install-recommends libfribidi0 libraqm0 make
    "$PY" -m pip install --disable-pip-version-check -q "Pillow==$PILLOW_VERSION"
    git submodule update --init --recursive
elif [ ! -f firmware/fm1-base/tools/build.py ] || [ ! -d firmware/fm1-base/bsp/chip ]; then
    git submodule update --init --recursive
fi
git submodule status --recursive
"$PY" - "$PILLOW_VERSION" <<'PY'
import sys
import PIL
from PIL import features
print(f"Python {sys.version.split()[0]}, Pillow {PIL.__version__} (raqm {features.version('raqm')}, fribidi "
      f"{features.version('fribidi')})")
if not features.check("raqm"):
    sys.exit("hub_build.sh: Pillow has no Raqm layout (libfribidi or libraqm missing?): the fonts could differ")
if PIL.__version__ != sys.argv[1]:
    sys.exit(f"hub_build.sh: the release builds use Pillow {sys.argv[1]}, not {PIL.__version__} (PYTHON=... for "
             "another python3)")
PY
if [ "$(uname -s)" = Linux ]; then     # (the Mac runs the toolchain in Docker: fm1-base tools/build.py)
    tc="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
    echo "toolchain: $(basename "$(readlink -f "$tc")"): $("$tc/pi32v2/bin/clang" --version | head -1)"
fi

ulimit -n 8192 || true          # the JieLi linker opens many files (the toolchain's README)
out="firmware/app/build/hub-$v"
"$PY" firmware/release.py "$v" --rebuild "$out"
for n in "${NAMES[@]}" SHA256SUMS; do
    [ -s "$out/$n" ] || { echo "::error::$out/$n is missing"; exit 1; }
done
if [ "$(grep -c . "$out/SHA256SUMS")" != "${#NAMES[@]}" ]; then
    echo "::error::$out/SHA256SUMS does not list the ${#NAMES[@]} files"
    exit 1
fi
(cd "$out" && sha256sum -c SHA256SUMS)
check "$out"

# what the shared workflow signs and releases (its "packages": fizz-*), in the root under the release's names; on
# GitHub's runner only (on the Mac $out/ has them)
if [ "${GITHUB_ACTIONS:-}" = true ]; then
    rm -f fizz-*
    for n in "${NAMES[@]}"; do cp "$out/$n" "./${n%.*}-$v.${n##*.}"; done
    shopt -s nullglob
    got=(fizz-*)
    if [ "${#got[@]}" != "${#NAMES[@]}" ]; then
        echo "::error::the root has ${#got[@]} files fizz-*, not the ${#NAMES[@]} packages and data blobs"
        exit 1
    fi
    ls -l fizz-*
fi
