# Building and releasing FIZZ's firmware

## Building locally

You need:

- JieLi's Linux toolchain in `~/.jieli/toolchain` (`$JIELI_TOOLCHAIN`), from <https://pkgman.jieliapp.com/s/linux-toolchain>;
- the JieLi AC79 SDK at the tag `AC79NN_SDK_V1.2.1_2023-12-13` in `~/fw-AC79_AIoT_SDK` (`$AC79_SDK`). The USB package
  reads three files from it; the Bluetooth package also builds on its Bluetooth libraries and sources;
- Python 3 with Pillow 11.1.0 and Raqm (on a Mac, `brew install libraqm`);
- the submodules: `git submodule update --init --recursive` (fm1-base and its felucca-bsp).

On Linux x86-64 the toolchain runs directly. Anywhere else it runs in Docker (`linux/amd64`, fm1-base's
`tools/build.py`), so start Docker first.

```sh
python3 firmware/build.py           # the USB package: firmware/app/build/fizz.fwsc and fizz-xdata.bin
python3 firmware/build.py --ble     # the Bluetooth package: fizz-ble.fwsc, fizz-ble-xdata.bin, fizz-ble-xdata2.bin
```

`firmware/build.py`'s docstring lists the other builds (the host simulator, the browser emulator, the single-core
package).

## Releasing

1. Write the notes for testers under `## Unreleased` in `CHANGELOG.md`, and commit them.
2. On a clean tree, run `python3 firmware/release.py VERSION` (`0.12.0-beta.13`, or `0.12.0` for a stable release).
   It builds both packages and writes them to `site/firmware/VERSION/` with a `SHA256SUMS`. It also lists them, with
   their sha256s, first in `site/firmware/releases.json`, and moves the notes in `CHANGELOG.md`. `--dry-run` does
   the same into `firmware/app/build/release-dry-run/` and leaves the tree alone.
3. Run `node site/tools/test_install.mjs`, then commit, tag `vVERSION` and push as `release.py` prints.

Pushing master deploys the install page with the new version (Pages serves the committed `site/firmware/`). Pushing
the tag runs `.github/workflows/release.yml`:

- **draft**: checks the tagged commit's `site/firmware/VERSION/` (its `SHA256SUMS`, `releases.json`,
  `test_install.mjs`), then makes a draft release with the notes (`release.py --notes`), `LICENSE`, `LICENSING.md`
  and `LICENSES.zip`;
- **build**: The Firmware Hub's shared workflow `zvenson/fm1-build@v1` builds both packages from the tag with
  `firmware/hub_build.sh`, signs them and adds them to the draft;
- **publish**: checks the draft's files (the attestations, `SHA256SUMS`, `releases.json`), then publishes the
  release. A beta is a pre-release.

The release carries five files, with the version in their names: `fizz-V.fwsc`, `fizz-xdata-V.bin`,
`fizz-ble-V.fwsc`, `fizz-ble-xdata-V.bin`, `fizz-ble-xdata2-V.bin`, and fm1-build's `SHA256SUMS`.

A run from the Actions tab (Run workflow, with a version) only builds and signs. It makes no release; the files are
the run's artifact `packages`.

### If JieLi's server is down

When the build job can't fetch the toolchain, re-run the failed jobs later; the draft waits. If the server stays
down, upload the five committed files from `site/firmware/VERSION/` to the draft by hand, renamed as above, with their
`SHA256SUMS`, and publish the draft. The files are the same bytes, but they have no attestation. The hub then lists
that release without "Built from source on GitHub", and `gh attestation verify` finds nothing.

## Signed builds

The Firmware Hub (<https://thefirmwarehub.com>) shows **Built from source on GitHub** only for a package that its
shared workflow `zvenson/fm1-build/.github/workflows/build.yml` built and attested at `refs/tags/v1`, from this
repository, for a release tag. A file built by our own workflow, or uploaded by hand, doesn't get it.

The workflow is one short file. It checks out the tag (without submodules), sets up Python 3.12 with the newest
Pillow, fetches JieLi's toolchain from `pkgman.jieliapp.com` and clones the AC79 SDK at its tag. Then it runs our
build command (`bash firmware/hub_build.sh`) and attests every file matching `fizz-*`. `release.yml`'s publish job
accepts only files signed by v1's tag object (`FM1_BUILD_V1_SHA`). A new tag of fm1-build means a review and a change
of both.

`firmware/hub_build.sh`:

- installs the pinned Pillow 11.1.0 with Raqm, and stops on any other version (the fonts are drawn with Pillow; a
  different version could draw them differently);
- fetches the submodules;
- runs `python3 firmware/release.py VERSION --rebuild DIR`, which is the same build `release.py VERSION` does, with
  the same version string and checks. It writes only the five files and their `SHA256SUMS`, and it may rebuild a
  version that is already listed;
- checks every sha256 against `releases.json` and the committed `site/firmware/VERSION/`, before anything is signed.
  On a tag the version must be listed;
- copies the five files to the repository root under their release names.

So the signed files are byte for byte the ones the install page serves. That holds because the build doesn't depend
on the machine: the Mac (Python 3.11, the toolchain 20250324.1 in Docker) and fm1-build (Ubuntu 24.04, Python 3.12, a
fresh toolchain download) make the same bytes. To move to a new Pillow, pin it in `hub_build.sh`, install the same
version on the Mac, and release from that.

To check a build before tagging, run `bash firmware/hub_build.sh VERSION` on a clean tree.
`bash firmware/hub_build.sh --check DIR VERSION` checks files you already have.

## Checking a package

Anyone can check that a file is the one GitHub built from the tag's source, with the GitHub CLI:

```sh
gh attestation verify fizz-V.fwsc --repo Quixotic7/FIZZ \
  --signer-workflow zvenson/fm1-build/.github/workflows/build.yml
```

It prints the tag, the commit and the build run. Add `--source-ref refs/tags/vV` to require that tag. The same works
for the data blobs. Releases before the first signed one have no attestation; their sha256s are in `SHA256SUMS` and
`releases.json`.

## The data blobs

The hub installs only the `.fwsc`. FIZZ also needs its data written after every install: `fizz-xdata.bin` for the
USB package, or `fizz-ble-xdata.bin` and `fizz-ble-xdata2.bin` for the Bluetooth package. The install page
(<https://quixotic7.github.io/FIZZ/install/>) does that for you. From a terminal, use
`firmware/tools/fizz_xdata.py push`; the release notes give the command. After installing from the hub, run that
command, or install again from FIZZ's own page, which writes both.
