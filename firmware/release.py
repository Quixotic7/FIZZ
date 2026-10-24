#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Cut a FIZZ release for testers: build it, put it on the site's install page, and print the git commands that
publish it (it runs none of them: nothing is pushed, tagged or released from here).

  firmware/release.py VERSION [--date YYYY-MM-DD]       the release, into this tree (site/firmware/, CHANGELOG.md)
  firmware/release.py VERSION --dry-run [--out DIR]     the same checks and builds, its files into DIR (default
                                                        firmware/app/build/release-dry-run/): the tree is not touched
  firmware/release.py --notes VERSION                   CHANGELOG.md's section for VERSION, with the install page and
                                                        Discord (the GitHub release's text: .github/workflows/release.yml)

VERSION is X.Y.Z-beta.N for a tester beta (0.11.0-beta.1), or X.Y.Z; a dry run may add a suffix
(0.11.0-beta.0-dryrun). What it does:
  1. checks: the tree clean (nothing changed or untracked, fm1-base and felucca-bsp as committed), the version new
     (not in releases.json, no tag vVERSION), CHANGELOG.md's "## Unreleased" section written (the notes for testers),
     site/install/fm1lib.js what fm1-base makes (firmware/tools/sync_installer.py --check);
  2. builds each package in PACKAGES with firmware/build.py --release X.Y.Z (fm1-base: the identity FM-1_46XXYYZZ)
     and APP_VERSION set to the whole VERSION, so the unit reports "FIZZ 0.11.0-beta.1" (FIZZ's SysEx IDENTITY,
     app.c fz_init(APP_NAME " " APP_VERSION); fm1-base defines APP_VERSION from its build's VERSION, which this sets
     on fm1-base's build module before the app is compiled: firmware/build.py and firmware/app are not changed);
     each checked: the identity, the update loader, the version string in the image, the data blob's stamp;
  3. site/firmware/VERSION/: NAME.fwsc, NAME-xdata.bin (FIZZ's data: the page writes it after the install), SHA256SUMS;
  4. site/firmware/releases.json: the release first (the page offers it by default), its notes from CHANGELOG.md;
  5. CHANGELOG.md: "## Unreleased" becomes "## VERSION (DATE)", a new empty Unreleased above it;
  6. prints the git commands: commit, tag vVERSION, push master (Pages deploys the page), push the tag (the
     release workflow makes the GitHub pre-release with the .fwsc attached).
The build needs what firmware/build.py needs (the JieLi toolchain, the AC79 SDK)."""
import argparse
import datetime
import hashlib
import json
import os
import re
import runpy
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
APP = HERE / "app"
BUILD = APP / "build"
SITE_FW = Path("site") / "firmware"           # (relative to the tree, or to --out)
INDEX = SITE_FW / "releases.json"
CHANGELOG = Path("CHANGELOG.md")
PREFIX = "FM-1_46"                            # firmware/app/fm1.toml id_prefix
INSTALL_URL = "https://quixotic7.github.io/FIZZ/install/"
DISCORD = "https://discord.gg/emf6DmZ9V"
REPO_URL = "https://github.com/Quixotic7/FIZZ"
# the packages a release carries: (file name, firmware/build.py's arguments, what it is, for the page)
# (P2.14: the dual build, fm1.toml cpu2 = true with cpu2_fallback; "--single-core" makes fizz-1core, core 1 off)
PACKAGES = [("fizz", [], "8 voices across both cores, USB MIDI")]
UNRELEASED = "## Unreleased"
UNRELEASED_HINT = "<!-- The next release's notes, for testers, in plain language. firmware/release.py moves them. -->"
VERSION_RE = re.compile(r"(\d{1,2})\.(\d{1,2})\.(\d{1,2})(?:-beta\.(\d{1,3}))?(-[a-z0-9]+)?")


def die(msg):
    raise SystemExit(f"release: {msg}")


def git(*args, cwd=REPO):
    return subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True, check=True).stdout.strip()


def sha256(b):
    return hashlib.sha256(b).hexdigest()


def product_of(raw):
    """the package identity (fm1-base tools/make_site.py product_of)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(20) if (m := raw[i * 0x30 + 0x2F]) != 0x7D)


# ---- CHANGELOG.md
def sections(text):
    """[(heading line, body)] of the "## " sections, in order"""
    parts = re.split(r"(?m)^(## .*)$", text)
    return [(parts[i].strip(), parts[i + 1]) for i in range(1, len(parts) - 1, 2)]


def clean_notes(body):
    return re.sub(r"<!--.*?-->", "", body, flags=re.S).strip()


def notes_of(text, version):
    for head, body in sections(text):
        if head == f"## {version}" or head.startswith(f"## {version} "):
            return clean_notes(body)
    return None


def unreleased(text):
    for head, body in sections(text):
        if head == UNRELEASED:
            return clean_notes(body)
    return None


def changelog_released(text, version, date):
    """CHANGELOG.md with its Unreleased section named VERSION (DATE) and a new empty one above"""
    m = re.search(r"(?ms)^" + re.escape(UNRELEASED) + r"[ \t]*\n(.*?)(?=^## |\Z)", text)
    if not m or len(re.findall(r"(?m)^" + re.escape(UNRELEASED) + r"[ \t]*$", text)) != 1:
        die(f"CHANGELOG.md needs one '{UNRELEASED}' section")
    return (text[:m.start()] + f"{UNRELEASED}\n\n{UNRELEASED_HINT}\n\n## {version} ({date})\n\n{clean_notes(m[1])}\n\n" +
            text[m.end():]).rstrip("\n") + "\n"


def release_body(version, notes):
    return (f"{notes}\n\n---\n\n**Install it from the browser** (Chrome or Edge, USB): {INSTALL_URL}?v={version}\n\n"
            f"The page installs FIZZ and then writes FIZZ's data. With the files below instead (a terminal), install "
            f"the `.fwsc`, then write the `-xdata` file: `firmware/tools/fizz_xdata.py push --blob "
            f"{PACKAGES[0][0]}-xdata-{version}.bin`.\n\n"
            f"**Feedback:** FIZZ's Discord, {DISCORD}\n")


# ---- the build (in a child process: fm1-base's build module with the version string set)
def child_build(version, args):
    """firmware/build.py ARGS, its fm1-base module's VERSION (APP_VERSION) set to VERSION before the app is built"""
    sys.argv = [str(HERE / "build.py"), *args]
    g = runpy.run_path(str(HERE / "build.py"), run_name="fizz_build")   # (its __main__ block is not run)
    fb = g["fb"]
    build_app = fb.build_app

    def stamped():
        fb.VERSION = version
        return build_app()
    fb.build_app = stamped
    # firmware/build.py's __main__ block, as it is
    sys.argv = [str(g["BASE"] / "tools" / "build.py"), str(g["APP"]), *[a for a in args if a != "--single-core"]]
    return fb.main()


def build(version, xyz, name, args):
    env = dict(os.environ)
    r = subprocess.run([sys.executable, str(Path(__file__).resolve()), "--_build", version, *args, "--release", xyz],
                       cwd=REPO, env=env)
    if r.returncode:
        die(f"{name}: the build failed")
    fwsc = BUILD / f"{name}-{xyz}.fwsc"
    image = BUILD / f"{name}.bin"
    xdata = BUILD / "fizz-xdata.bin"                  # (firmware/build.py: one name for every package)
    for p in (fwsc, image, xdata):
        if not p.exists():
            die(f"{name}: no {p}")
    raw, img, blob = fwsc.read_bytes(), image.read_bytes(), xdata.read_bytes()
    want = PREFIX + "%02d%02d%02d" % tuple(int(x) for x in xyz.split("."))
    if product_of(raw) != want:
        die(f"{fwsc.name}: identity {product_of(raw)!r}, not {want}")
    if b"FELUCCA-LOADER-1" not in raw:
        die(f"{fwsc.name}: no update loader in it")
    if f"FIZZ {version}".encode() not in img:
        die(f"{image.name}: the version string 'FIZZ {version}' is not in the image")
    crc = zlib.crc32(blob) & 0xFFFFFFFF
    at = img.find(struct.pack("<II", 0x44585A46, 0x504D5453))   # fz_port.c fz_stamp
    if at < 0 or struct.unpack_from("<II", img, at + 8) != (len(blob), crc):
        die(f"{image.name}: its data stamp is not {len(blob)} B, CRC-32 {crc:08x} (fizz-xdata.bin)")
    return raw, blob, crc, want


def main():
    if len(sys.argv) > 2 and sys.argv[1] == "--_build":
        return child_build(sys.argv[2], sys.argv[3:])
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("version", nargs="?")
    ap.add_argument("--dry-run", action="store_true", help="build and stage into --out; the tree is not touched")
    ap.add_argument("--out", type=Path, help="the dry run's directory (default firmware/app/build/release-dry-run)")
    ap.add_argument("--date", help="the release's date (default today)")
    ap.add_argument("--notes", metavar="VERSION", help="print CHANGELOG.md's section for VERSION, for the GitHub release")
    a = ap.parse_args()

    if a.notes:
        notes = notes_of((REPO / CHANGELOG).read_text(encoding="utf-8"), a.notes)
        if not notes:
            die(f"CHANGELOG.md has no section '## {a.notes}'")
        print(release_body(a.notes, notes))
        return 0
    v = a.version or die("give a VERSION (0.11.0-beta.1)")
    m = VERSION_RE.fullmatch(v) or die(f"{v}: X.Y.Z-beta.N or X.Y.Z, each up to two digits")
    if m[5] and not a.dry_run:
        die(f"{v}: a suffix after the version is for --dry-run only")
    xyz = f"{int(m[1])}.{int(m[2])}.{int(m[3])}"
    date = a.date or datetime.date.today().isoformat()
    out = (a.out or BUILD / "release-dry-run").resolve() if a.dry_run else REPO

    # 1. the checks
    problems = []
    status = git("status", "--porcelain", "--ignore-submodules=none")
    if status:
        problems.append("the tree is not clean:\n    " + "\n    ".join(status.splitlines()[:12]))
    for sub in ("firmware/fm1-base", "firmware/fm1-base/bsp"):
        if (REPO / sub / ".git").exists() and git("status", "--porcelain", cwd=REPO / sub):
            problems.append(f"{sub} has changes")
    index = json.loads((REPO / INDEX).read_text(encoding="utf-8")) if (REPO / INDEX).exists() else {"releases": []}
    if any(r.get("version") == v for r in index.get("releases", [])) or (REPO / SITE_FW / v).exists():
        problems.append(f"{v} is released already (site/firmware/)")
    if git("tag", "-l", f"v{v}"):
        problems.append(f"the tag v{v} exists")
    log = (REPO / CHANGELOG).read_text(encoding="utf-8") if (REPO / CHANGELOG).exists() else ""
    notes = unreleased(log)
    if not notes:
        problems.append(f"CHANGELOG.md's '{UNRELEASED}' section is empty: write the notes for testers first")
    sync = subprocess.run([sys.executable, str(HERE / "tools" / "sync_installer.py"), "--check"], cwd=REPO,
                          capture_output=True, text=True)
    if sync.returncode:
        problems.append(sync.stdout.strip() or sync.stderr.strip())
    if problems:
        die("not ready:\n  - " + "\n  - ".join(problems))
    source = git("rev-parse", "HEAD")
    base = git("rev-parse", "HEAD", cwd=REPO / "firmware" / "fm1-base")
    print(f"release {v}: from {source[:10]} (fm1-base {base[:10]}), {'dry run into ' + str(out) if a.dry_run else 'into this tree'}", flush=True)

    # 2. the builds
    built = []
    for name, args, label in PACKAGES:
        raw, blob, crc, identity = build(v, xyz, name, args)
        built.append((name, label, raw, blob, crc, identity))
        print(f"  {name}: {len(raw)} B, identity {identity}, data {len(blob)} B (CRC-32 {crc:08x}), 'FIZZ {v}' in the image")
    if len({b[5] for b in built}) != 1:
        die("the packages' identities differ")

    # 3. the files
    if a.dry_run and out.exists():
        shutil.rmtree(out)
    dest = out / SITE_FW / v
    dest.mkdir(parents=True)
    packages, sums = [], []
    for name, label, raw, blob, crc, identity in built:
        (dest / f"{name}.fwsc").write_bytes(raw)
        (dest / f"{name}-xdata.bin").write_bytes(blob)
        sums += [f"{sha256(raw)}  {name}.fwsc", f"{sha256(blob)}  {name}-xdata.bin"]
        packages.append({"name": name, "label": label,
                         "fwsc": {"path": f"{v}/{name}.fwsc", "bytes": len(raw), "sha256": sha256(raw)},
                         "data": {"path": f"{v}/{name}-xdata.bin", "bytes": len(blob), "sha256": sha256(blob),
                                  "crc32": crc}})
    (dest / "SHA256SUMS").write_text("\n".join(sums) + "\n")

    # 4. releases.json, the newest first
    rel = {"version": v, "date": date, "channel": "beta" if m[4] else "stable", "identity": built[0][5],
           "source": source, "fm1base": base, "notes": notes, "packages": packages}
    index = {"name": "FIZZ", "latest": v, "releases": [rel] + [r for r in index.get("releases", []) if r.get("version") != v]}
    (out / INDEX).parent.mkdir(parents=True, exist_ok=True)
    (out / INDEX).write_text(json.dumps(index, indent=2) + "\n", encoding="utf-8")

    # 5. CHANGELOG.md
    (out / CHANGELOG).write_text(changelog_released(log, v, date), encoding="utf-8")

    # 6. what is left to do, by hand
    print(f"\n  {SITE_FW / v}/: " + ", ".join(p.name for p in sorted(dest.iterdir())))
    print(f"  {INDEX}: {v} first ({len(index['releases'])} in all); {CHANGELOG}: '## {v} ({date})'")
    if a.dry_run:
        print(f"\ndry run: nothing in the tree changed; the files are in {out}")
        return 0
    print(f"""
Nothing is published yet. Look at the diff (git diff --stat; the page: node site/tools/assemble.mjs, then serve it),
then publish:

  git add {SITE_FW / v} {INDEX} {CHANGELOG}
  git commit -m "Release FIZZ {v}"
  git tag -a v{v} -m "FIZZ {v}"
  git push origin master          # Pages deploys the install page with {v}
  git push origin v{v}       # the release workflow makes the GitHub pre-release, the .fwsc attached
""")
    return 0


if __name__ == "__main__":
    sys.exit(main())
