# FIZZ installs from The Firmware Hub: packages that carry their own data (plan, 2026-11-02)

The owner wants the hub's Install button to install FIZZ's **Bluetooth package**, with no change to the hub's
installer. Today the hub's card says "Install on its page ↗". The hub writes one .fwsc, and FIZZ also needs its
data blob(s) written after every install. Without them the unit shows FIZZ NEEDS ITS DATA.

## Status (2026-11-02)

Steps 1 and 2 are done on the branch `hub-signing`, with no device. Step 3 (on a unit) is next.
- The owner chose both packages bundled, and the new asset names (`fizz-V.fwsc` = Bluetooth, `fizz-usb-V.fwsc` = USB).
- `firmware/loader/` holds FIZZ's `ldr_core.c` and `loader.c`. The diff against fm1-base's is the extents' table,
  its limits (`_Static_assert` and a run-time check, -14), `FL_RANGE_OK` grown by the extents, and step 3's loop
  going on through them (`ldr_next`: the sectors holding an update record first, then the rest). `sc[]` covers every sector up to 0xFC000.
- ota.bin: 8,621 B (USB) and 8,668 B (Bluetooth), up from 8,340 B. The limit is 19,968 B.
- Packages: 1,041,857 B (USB) and 1,041,904 B (Bluetooth), up from 611,496 B.
- Extents: USB 0xE5000..0xFC000; Bluetooth 0x97000..0xAF000 and 0xE7000..0xFC000.
- `bash firmware/app/tests/ldr_test.sh` runs the host test against both built packages: everything passes.
- The Mac and Linux (fm1-build's steps) build the same five files for `0.12.0-beta.13-hubtest3`:
  - `70d141d9…`: `fizz-usb-V.fwsc`
  - `fb26e96d…`: `fizz-V.fwsc` (Bluetooth)
  - `870bd3f6…`: `fizz-xdata-V.bin`
  - `e632a56e…`: `fizz-ble-xdata-V.bin`
  - `37269ac6…`: `fizz-ble-xdata2-V.bin`

The stock firmware's update record (0xE8F00) lies inside both packages' first-blob extent. So, after the app area,
step 3 first writes every extent sector that held a valid update record when the pass began (step 4's test). Then it
writes the rest in order. A power cut before that point leaves V15's record and staged loader untouched, so the
install resumes. A cut after it finds no record, and the app area is already whole, so the new FIZZ boots with "NEEDS
ITS DATA" and an install from FIZZ finishes the job. Installs staged by Felucca keep their record at 0xE4F00, outside
the extents. `ldr_test` checks a V15-like layout (record at 0xE8F00, a staged loader over 0xE4000..0xE8EFF) with a cut
after every page of the data pass, for both packages. Still check on a unit: "from the official V15", with an unplug
during the blob pass.

## The idea

FIZZ's .fwsc carries its blobs, and FIZZ's own copy of the update loader writes them.

The hub's page decides nothing about what gets written. In both steps of an install it only answers the device's
0x30 read requests on the package's image (thefirmwarehub.com's installer, after Felucca's protocol). The loader
that does the writing is the one **inside the new package** (its `ota.bin`): the running firmware stages that loader at
0xE0000 and the SPL runs it. So a package whose loader also writes the blobs installs completely from the hub, from
FIZZ's install page and from `fm1_install.py`, all three unchanged.

What stops it today:
- Felucca's loader (fm1-base `bsp/loader/ldr_core.c`) writes only the app area, 0x4000..0x93000.
- `loader.c`'s `FL_RANGE_OK` forbids 0x97000..0xE0000 outright.

The blobs sit outside that area:

| package | blob 1 | blob 2 | store (never written) |
|---|---|---|---|
| USB `fizz` | 0xE5000, 90.6 KB + trailer | none | 0x97000..0xDFFFF |
| Bluetooth `fizz-ble` | 0xE7000, 84.5 KB + trailer | 0x97000..0xAEFFF, 96.7 KB + trailer | 0xAF000..0xDFFFF |

The app reads the blobs in place and unencrypted: `plat_data` reads through the plain window. So the loader writes
them as plain bytes, exactly what `fizz_xdata.py push` writes: the blob, then its trailer `FZXT`, length, CRC-32.

## Design

### 1. The package: flash.bin extended to 0xFC000

`fm1pkg_make.py`'s `flash_image` stops at `FLASH_SIZE` 0x93000. FIZZ's build extends flash.bin to 0xFC000
(1,032,192 B; the loader accepts up to 1 MiB, `LDR_FL_MAX`):
- Blob 1 and its trailer go at their flash offsets.
- For Bluetooth, blob 2 and its trailer go at 0x97000.
- Everything else past 0x93000 stays 0xFF.

The UFW header's flash.bin CRC16 covers all of it, so a damaged package is still refused before anything is erased.

The .fwsc grows from ~611 KB to ~1.03 MB. The package format is unchanged: still one flash.bin and one ota.bin, as
every running firmware's step 1 (Felucca's `ota_stage`, M-VAVE's V15) expects.

The alternative was a third UFW file holding only the blobs. It would keep the check pass shorter, but other
firmwares' step 1 and the hub's server-side checks might not expect a third file. Not chosen.

### 2. The loader: Felucca's, plus one table

FIZZ's copy of `ldr_core.c` and `loader.c` lives in `firmware/loader/`. It keeps Hügelton's copyright and MPL-2.0.
`firmware/build.py` builds it in place of fm1-base's; the submodule stays unchanged.

The only change: after the app area (step 3), a second pass writes the **data extents**. These are a table compiled
into the loader, one per package build:
- USB: 0xE5000 up to the end of blob 1's trailer, rounded up to the sector.
- Bluetooth: 0x97000 up to the end of blob 2's trailer, and 0xE7000 up to the end of blob 1's trailer.

The second pass works the same way as step 3:
- It uses the sector CRC16s it kept in the check pass.
- It skips sectors that are already equal, and verifies each one after writing it.
- It retries twice, as step 3 does.

Hard limits, checked when the loader is compiled (`_Static_assert`) and again when it runs:
- an extent must lie inside 0x97000..0xFC000;
- it must never touch 0xE0000..0xE4FFF (the staged loader and its record);
- for Bluetooth, never 0xE5000..0xE6FFF (OBJ_BLE, the radio's bonding store);
- never the package's own store (USB 0x97000..0xDFFFF; Bluetooth 0xAF000..0xDFFFF). The Bluetooth blob 2 lies in
  the USB store's first sectors, so a USB → Bluetooth switch overwrites them. The switch already wipes the store
  today;
- never 0xFC000 and above (the panel's calibration and Felucca's globals).

`FL_RANGE_OK` grows by exactly these extents.

Everything else stays as it is:
- the identity `ota-FM-1_900` (the hub's `OUR_LOADER` resumes only `ota-FM-1_9NN`);
- the marker `FELUCCA-LOADER-1` (it still never writes the head);
- the chip-key check, the update-record clearing and the resume after power loss: the record survives until step 4,
  so a rerun skips the equal sectors.

The loader grows by well under 1 KB. Today it is 8.3 KB; the limit is ~19.9 KB (`OTA_AREA_LEN - 0x200`).

Install time goes up about 1.5×: the check pass reads 1 MB instead of 0.6 MB, and up to 185 KB more is written.
That's about 1½ minutes instead of 1. The hub's progress bar counts reads, so it keeps moving.

### 3. Both packages, the same way

The USB package bundles its blob too, so both packages are self-contained. FIZZ's install page keeps working
unchanged: its FZXD push now answers "already there" (FZOK 0). A later, separate change can drop that push.

### 4. Which package the hub takes

The hub matches release assets with `fizz-\d[\w.-]*\.fwsc`. The release names become:
- `fizz-V.fwsc` for the **Bluetooth** package;
- `fizz-usb-V.fwsc` for USB;
- the blobs keep their names, though they are no longer needed for an install.

FIZZ's own site keeps its file names (`site/firmware/V/fizz.fwsc`, `fizz-ble.fwsc`), so the install page and Manager
are untouched. `firmware/hub_build.sh` and release.yml change their copy-to-root names and the count stays five.
The bundled packages make the separate .bin files optional, but they are kept for the Manager and fizz_xdata.py.

### 5. The hub's side: a mail, not a change

Mail zven@thefirmwarehub.com, as the hub's KB asks of authors. Say that:
- from version V, FIZZ's .fwsc carries its data and its own build of Felucca's loader writes it;
- that loader is otherwise unchanged and is signed by fm1-build;
- the release's `fizz-V.fwsc` is the Bluetooth package;
- please drop the card's "install" link;
- FIZZ's port name is "FIZZ FM-1";
- the Bluetooth package writes 0x97000..0xAEFFF, where other firmwares keep their data. FIZZ's README says so and
  says to back up first.

Also add `fmhub.json` (what, kinds `groove`, play, editor = the Manager, image), so the card stops saying
"Text by the hub".

The open risk is the hub's server-side "has its update loader" check. If it recognises loaders by their bytes, a
modified one may be refused. The mail asks about this before release.

## Work, in order

1. **Loader and its test** (an Opus agent; no device):
   - `firmware/loader/` (the extents, the limits);
   - `build.py` builds it;
   - a host test in the style of fm1-base's tests: a simulated flash and package covering resume, a damaged package,
     extents refused outside the limits, equal sectors skipped, and that the store and OBJ_BLE are byte-identical
     after the install.
2. **Packages** (the same agent):
   - `build.py` extends flash.bin with the blobs and trailers;
   - `release.py` checks each package's blob region against the separate .bin files;
   - `hub_build.sh` and release.yml use the new asset names.

   The byte-identity proof is rerun: Mac vs. the Linux copy of fm1-build.
3. **On a unit** (the main FIZZ session only: it flashes; backup first). With `fm1_install.py`, check each install
   ends with FIZZ running, no "NEEDS ITS DATA", the stats clean and the store kept:
   - FIZZ USB → Bluetooth;
   - Bluetooth → Bluetooth, a new version;
   - from ChoralRoot;
   - from the official V15;
   - from Felucca;
   - an unplug during the blob pass, then a resume;
   - leaving FIZZ for another firmware still works.

   Also FIZZ's install page, and the browser emulator test before release.
4. **Release** (the owner's word): the signed workflow (branch `hub-signing`, after its dry run), then the mail to the
   hub, then the card.

## Decisions for the owner
- ~~Bundle the USB package too (recommended), or only Bluetooth.~~ Both (2026-11-02).
- ~~Rename the release assets (recommended: no mail needed for the choice), or ask the hub to pick `fizz-ble-*`.~~
  Renamed (2026-11-02).
- The README line about the Bluetooth package overwriting other firmwares' data at 0x97000..0xAEFFF.
