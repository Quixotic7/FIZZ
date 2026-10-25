# FIZZ web manager: project backup, sound library, pool, Digitone import (plan)

Status: Phase 0 and Phase 1 built (below, 2026-10-24); Phase 2 not started. The original plan follows. The
six-track change is being made right now by another agent; everything below assumes **6 tracks** (`QD_TRACKS 6`,
store magic `Z3FS`).

Mock-ups: `mockups/fizz-web-manager.html` (`?shot=1..7`), rendered by `python3 mockups/render_png.py wm-1 .. wm-7` to
`mockups/png/fizz-web-manager-01-backup.png` .. `-07-phone-library.png`.

The owner's request: a web page that (1) backs up and restores projects from and to the FM-1, (2) manages the sound
library (reorder, move between Library / MY PRESETS / Pool, tag, rename, delete), (3) edits a project's pool, and (4)
imports Digitone 1 and Digitone II presets, converted in the browser.

---

## 1. What exists today, and why "it isn't loading"

### 1.1 Inventory

| piece | where | state |
| --- | --- | --- |
| FIZZ SysEx protocol 1 (`F0 7D 46 5A 01 ..`): IDENTITY, STORE, STORE6, LABELS, LIST, DUMP | `core/app/fizz_sx.c`, `docs/SYSEX.md` | **works**, tested on the host and in the wasm (`site/tools/test_engine.mjs`: LIST, DUMP everything, restore into a blank unit, byte-for-byte) |
| the same SysEx on the unit | `firmware/app/app.c` `app_sysex` (fm1-base `APP_SYSEX`; the main loop's `ota_frame_get`, one inbound frame of at most 640 bytes) | **built in** (`APP_SYSEX` in `app.h`), replies go through `fz_sysex_out` → `plat_sysex_send`. **Not tested against the unit from any page or tool** |
| the store's whole-region backup and restore (`46 "FZBI"`, `"FZBR" off n` ≤ 256 B, `"FZSR" len crc`) in the update's framing | `firmware/app/app.c` `app_usb_cmd` (P2.13) | **works from Python** (`firmware/tools/fizz_xdata.py backup FILE` / `restore FILE`). No web page |
| the preview site's **My presets** panel (LIST, DUMP, `.fizzsnd` / `.fizzlib`, Digitone export) | `site/js/library.js`, `site/js/library-ui.js` | **works, but only against the FIZZ running in the browser** (`main.js`: `toFizz = (b) => player.midi(b)`). Checked today: `node site/tools/test_site.mjs` passes "My presets lists them .." |
| the preview site's **Import sounds** (`.dnsnd`, `.dn2pst`, `.dn2snd`, `.fizzsnd`, `.fizzlib` → STORE / STORE6) | `site/js/digitone.js`, `digitone-ui.js`, `fm1dnsnd.js`, `dn2pst.js`, `fizz_sysex.js` | **works, browser FIZZ only** (same `toFizz`) |
| Connect FM-1 on the preview site | `site/js/use-fm1.js`, `fm1-link.js`, `remote/` | talks to **RemoteFM1** firmware only (the unit as a panel). A unit running FIZZ firmware never answers it ("No FM-1 running RemoteFM1 answered") |
| the install page's **Back up FIZZ** link | `site/install/index.html`, `install.js` (`fetch("backup/", {method: "HEAD"})`) | **missing**: `site/install/backup/` was never made (fm1.toml `backup = false`: fm1-base's backup page carries Felucca's storage objects, not FIZZ's store). The page says "FIZZ has no backup of its own yet". `https://quixotic7.github.io/FIZZ/install/backup/` returns **404** (checked 2026-10-24) |
| the install page's "Check my FM-1" | `site/install/install.js` `readUnit` | **works**: Web MIDI + SysEx, it already asks FIZZ's IDENTITY on the unit |
| a store reader on the host | `tools/flash_dump.c`, `tools/flash_check.py` | works on a flash image (lists projects, patterns, pool, MY PRESETS, the settings) |
| tags | FIZZ's 7 (BASS KEYS PLUCK PAD LEAD DRUMS FX) in each preset record's tags byte; STORE's tags byte; LIST shows them | works; **no SysEx sets them** after the import |
| rename, delete, reorder, pool edits | on the panel only (`qd_lib_replace`, `qd_preset_delete`, `qd_pool_swap`, `qd_pool_copy`, `qd_pool_delete`, `qd_tags_set`, `qd_proj_*` exist in `persist.c`) | **no SysEx for any of them** |
| per-project backup / restore | — | **missing** (only the whole region, raw) |
| Digitone `.syx` (a SysEx sound dump) | — | **missing**. `dn2pst.js` already reads a bare 359-byte DN2 record, so a `.syx` reader only needs the de-framing |

### 1.2 Why it doesn't load

There is no page that talks to an FM-1 running FIZZ firmware, apart from the installer's version check.

1. **The site's library and import panels only reach the FIZZ inside the browser.** Opened with the FM-1 plugged in, they
   list and fill the *browser's* MY PRESETS (an IndexedDB flash), never the unit's. "Connect FM-1" looks for RemoteFM1,
   so with FIZZ firmware on the unit it searches and then turns red.
2. **The backup page the installer links to was never built** (404). Its link is hidden by `install.js`, so the page
   simply says there is no backup.
3. **The site's wasm is old**: `fizz.json` (local and deployed) says `0.10.0-p2.1`, 8 tracks, from 2026-10-23
   (HANDOFF open item 5). A unit on 0.11 / the coming 6-track build and the preview are different FIZZes.
4. LIST / DUMP / STORE have **never been run against the unit** over USB. They should work (same core code, the
   firmware has `APP_SYSEX`), but the round-trip time per frame, the 4 KiB reply ring's pacing over real USB, and the
   lock time are unmeasured (§3).

### 1.3 Urgent: the six-track change wipes the unit's store

`core/store/store.h` now has `QDS_MAGIC "Z3FS"` (was `"Z2FS"`). Both stores (MY PRESETS' and the projects') use it.
`fz_store_foreign()` treats Z2FS sectors as another firmware's and the first boot **erases them**. So when the 6-track
build is flashed, the owner loses **all projects and all MY PRESETS**, and an `FZBR` image taken before cannot be put
back either (FZSR would restore it, then the boot wipes it again as foreign).

What carries over: **MY PRESETS through LIST / DUMP → STORE** (the preset frames don't depend on the track count).
Projects don't carry over (their records are 8-track) unless the core gets an 8-track reader (question Q1).

Before that flash, the main session should take both:
- `python3 firmware/tools/fizz_xdata.py backup FIZZ-saves/2026-10-2x-before-6track-store.bin` (the raw image, to go back to 0.11 if needed);
- a MY PRESETS export (DUMP everything). Today that needs a small tool, because no page reaches the unit. Phase 0
  below adds it.

The same applies to the browser preview: when the site's wasm moves to 6 tracks, `saves.js`'s carry-over will start
the preview's users on an empty store. The site should offer "export my presets (.fizzlib)" from the old version first
(question Q2).

---

## 2. Architecture

```
  browser (site/manager/, GPL)                          FM-1 running FIZZ (firmware MPL, core MPL)
  ┌──────────────────────────────┐   Web MIDI SysEx    ┌──────────────────────────────────────────┐
  │ manager.js  (UI, no protocol)│  ───────────────▶   │ fm1-base main loop: ota_frame_get (1 frame,│
  │ fizzlink.js (one queue:      │                     │ 640 B) → app_sysex  (7D 46 5A: semantic)  │
  │   semantic + bulk channels)  │  ◀───────────────   │           → app_usb_cmd (00 59 46: bulk,  │
  │ fizzbak.js  (files)          │                     │             the unit can pull from host)  │
  │ digitone.js, dn2pst.js ..    │                     │ replies: 4 KiB ring → plat_sysex_send     │
  └──────────────────────────────┘                     └──────────────────────────────────────────┘
          │ the same fizzlink against the wasm FIZZ (player.midi) for tests and the preview
```

- **Two channels, both existing.** The *semantic* one (FIZZ's `7D 46 5A`, `fizz_sx.c`) for the library, pool, tags and
  projects: small frames, one reply each, under `fz_lock`. The *bulk* one (the update's framing `F0 pack7(00 59 46 len
  .. ~sum) F7`, `app_usb_cmd`) for byte streams: the region image, project records. Its advantage: the unit can **pull**
  data with `plat_host_read` (512 B a request), so a restore streams without a RAM buffer.
- **One link object in the page** (`fizzlink.js`) serialises everything: one request outstanding, a timeout that
  restarts on each frame (as `createPresetLink` does), retries for idempotent reads, and the same code drives the wasm
  FIZZ (`player.midi` / `midiOut`) so every flow is tested in Node without a unit.
- **Where it lives:** a new `site/manager/` page (GPL, like the rest of `site/`) with the FIZZ look, linked from the
  install page ("Back up FIZZ" goes here instead of the 404) and from the player's header. The preview's existing
  panels keep working on the browser FIZZ and gain "on my FM-1" once the link exists.
- **Connecting:** `navigator.requestMIDIAccess({sysex: true})` (Chrome, Edge, Opera; Firefox asks for a site
  permission add-on; Safari has none). Find the port by name `FIZZ FM-1` (fm1.toml `usb_product`), confirm with
  IDENTITY. Only one app may hold the port on Windows: say so when it fails.

---

## 3. Transport: numbers, chunking, checksums, glitches, the mounted store

### 3.1 What a transfer costs

| item | size | frames |
| --- | --- | --- |
| the store's region (device: MY PRESETS 22 sectors + projects 51) | 299,008 B | 1,168 `FZBR` of 256 B (≈ 315 B on the wire each, 8-in-7 packing) |
| one MY preset as DUMP (STORE6) | ≈ 180 B | 1 |
| all 256 MY PRESETS (DUMP slot 0) | ≈ 46 KB | 256, paced by the 4 KiB reply ring |
| LIST of MY PRESETS + pool | 20 B an entry | 16 entries a frame: ≤ 24 frames |
| one project's records (a typical project, 'P' 5 patterns ≈ 1.6 KB each) | ≈ 10..100 KB | 240-byte pages, one frame each |

**Throughput is round-trip bound, not wire bound.** RemoteFM1 measured SysEx at ~375 KB/s to the unit, so 300 KB is
under a second of wire time. But fm1-base takes **one inbound frame per main-loop pass** (`ota_frame_get` → `app_sysex`
/ `usb_cmd`), and a pass includes `app_frame()`. With FIZZ drawing 30..93 ms frames while playing, a request a frame
would make the region backup 35..110 s; idle (frames skipped) it should be a few seconds. **Measure first** (Phase 0,
step 1): time `fizz_xdata.py backup` stopped and playing, and read `fizz_xdata.py stats` (xruns, `lock_sysex_us`,
`frame_max_us`) after it.

Ways to go faster if the number is bad, in order of cost:
1. **Pipeline**: send the next `FZBR` before the reply arrives (2..4 outstanding). Inbound USB is NAK flow-controlled
   (`ep1_rx` leaves the packet while the ring is full), so nothing is lost; the page matches replies by offset. 0 B.
2. **Bigger chunks**: `FZ_BK_CHUNK` 256 → 512 (the `FZBD` reply ≈ 600 B on the wire; the 640-byte limit applies only to
   *inbound* frames, and the request stays small). Stack +≈ 600 B transient (the `m[]`/`w[]` arrays), 0 static.
3. **Skip blank pages**: an `FZBR` reply flag "all FF" lets the page skip the erased half of the region. ≈ 40 B flash.

### 3.2 Checksums and framing

- Semantic frames: FIZZ's 7-bit sum (`docs/SYSEX.md`). Bulk frames: the update framing's `~sum` over the body.
- **Whole-transfer integrity: CRC-32** (IEEE, as `fz_crc32`), of the region (`FZBI` gives it) or of each record.
  The page checks it against the unit's; a mismatch means the store changed while it was read (§3.4): read again.
- The backup file carries its own CRC-32 per member (ZIP's) and the manifest's (§4).

### 3.3 Audio glitches

- Audio is rendered in the audio interrupt; the main loop handles SysEx. So a transfer costs **frames, not sound**,
  as long as nothing runs long with the interrupts off.
- **Under `fz_lock` (interrupts off):** every semantic frame (`app_sysex` → `fz_sysex` inside `fz_lock()`). STORE,
  LIST, DUMP, tag / rename are short (a directory change, a preset read from mapped flash, < 100 µs expected). **Pool
  swaps and deletes are not**: `qd_pool_swap` → `renumber()` walks all 64 patterns, and on the unit only 2 are in RAM
  (`QD_PAT_RAM 2`), so it pages patterns through the store. Run under the lock, that is an xrun. Rule for the new
  commands: anything that touches more than the current pattern is **refused while playing** (status `BUSY`) and, when
  stopped, runs as a **job stepped from the main loop outside the lock** (one pattern a step), replying when done.
  The panel's own pool swap should be measured the same way (`lock_max_us`, `lock_kind`).
- **Outside the lock:** `FZBI`'s CRC of the whole region (≈ 299 KB, bitwise CRC: tens of ms, the UI stalls, the sound
  doesn't), `FZBR` (a 256-byte copy from mapped flash). `FZSR` silences the sound on purpose (`app_panic`) and shows
  "RESTORING ITS STORE".
- Replies wait in the 4 KiB ring and leave through `fz_sysex_out` between strips; `plat_sysex_send` may wait up to
  200 ms per frame for USB room (`ota_wire_send`). A page that stops reading (tab in the background) stalls the unit's
  main loop. The manager must read continuously; the device side should drop to "replies paused" rather than wait
  (a follow-up check on `ota_wire_send`'s 200 ms).

### 3.4 While the store is mounted

- **Reading is safe while mounted**: the store's region is memory-mapped flash; reads never block its writes (same
  thread). Consistency is the issue: autosave may commit a record mid-backup. Today's answer (`fizz_xdata.py`): compare
  the copy's CRC with `FZBI`'s and try again. Better (Phase 1, ≈ 60 B flash): a `STATUS` semantic command that reports
  `qd_persist_dirty`, playing, and the store's commit counter, and `FLUSH` (`qd_persist_flush` when stopped); the page
  flushes, notes the counter, reads, and checks the counter is unchanged.
- **Writing (restore) only when stopped**: `FZSR` already refuses while playing (FZER 10). A per-project restore
  (Phase 2) writes through the store's own queue (`qd_store_put_src`), so the store stays mounted and consistent; its
  §7 rules (no erase unless quiet) still hold, and a restore waits for the quiet erase if it needs blank sectors.
- **Safe mode** (SAVE held at power-on: the store not attached): LIST / DUMP / STORE reply nothing useful; `FZBI` /
  `FZBR` still read the region, and `FZSR` is the way back from a broken store. The manager shows "safe mode" from
  `STATUS`.

### 3.5 Device RAM

Current margins (HANDOFF, before the 6-track change): dual RAM ≈ 1.2 KB, NOINIT 104 B, RAM code 124 B, pool ≈ 368 B over
fm1-base's 8 KB. The 6-track change frees some (to be measured by that agent).

Design rules: **no new buffers**. Replies use the existing 4 KiB `qd_sx_t.ring` (already in RAM on the unit) and
`fz_sx_buf[1024]`. Streams go a page (240 B) at a time through the existing `rd_t` reader / `qd_store_put_src`
source, on the stack. New static state is a cursor or two (≤ 32 B). Code goes to flash (the app slot had ~69 KB free
at P2.4; re-check with `firmware/build.py`'s size report).

---

## 4. Data model

### 4.1 The backup file: `.fizzbak`

A ZIP (stored, as `.fizzlib`, written by `library.js`'s `zipStored`), so it can be opened and inspected:

```
manifest.json           { "format": "fizz-backup", "version": 1, "date": ISO, "fizz": "0.12.0",
                          "unit": { "identity": "FM-1_46..", "usb": "FIZZ FM-1" },
                          "store": { "magic": "Z3FS", "tracks": 6, "region": [0x97000, 299008],
                                     "lib_sectors": 22, "prj_sectors": 51, "crc32": "…" },
                          "projects": [{ "slot": 1, "name": "NOIR", "pages": 51 }, ..],
                          "presets": 143, "kind": "full" | "presets" | "project",
                          "web": { see 4.3 } }
store.bin               the region image (FZBR), exactly as the unit holds it        (kind "full")
presets/001 NAME.fizzsnd  every MY preset as `.fizzsnd` (from DUMP), the portable layer (every kind)
projects/P1/NNNNNNNN.rec  (Phase 2) a project's records: key (hex) → bytes, + index.json with lengths and CRC-32s
```

- **Versioned twice**: the file (`version`) and the store (`magic`, `tracks`). A restore of `store.bin` is offered
  only when the unit's `FZBI` size and the store magic match; otherwise the page offers the portable layer
  (presets, and projects when a converter exists) and says why.
- **The portable layer is always included**: a full backup also carries every MY preset as `.fizzsnd`, so the presets
  survive any store-format change (exactly the 6-track situation now).
- **Readable offline**: the page parses `store.bin` itself (the store's layout is fully described in `store.h`: 256-byte
  pages, 16-byte headers, CRC-16, the commit page, highest sequence wins) to list a backup's projects and presets
  without a unit. A JS port of the store's *reader* only (≈ 150 lines, clean, from FIZZ's own MPL `store.c`).

### 4.2 Library, pool, tags on the device (unchanged)

- **MY PRESETS**: 256 places; each preset has a permanent **id** (`QD_PRESET_MINE0 + n`, never reused), name (12),
  patch (version 6 blob), tags (7 bits), flags (imported), mix (pan, 3 sends). Factory: ids 1..23, read only, their
  tags editable (`FTAG` record).
- **The pool**: per project, 128 numbered places, each a **reference to a preset id** ('R' 1 records) with a cached
  copy. Steps lock pool *numbers*; tracks keep their own copies (or, 'P' 5, references by id).
- So "move a sound between Library and Pool" means: **add a reference** (no copy), **remove a reference**, **swap pool
  numbers** (renumbers locks in all patterns), and **swap MY PRESETS places** (ids stay: no reference changes; cheap).
  Today STORE with the POOL flag *copies* a sound into a new MY preset; the manager needs a `POOL REF` that points a
  pool number at an existing id (§5).

### 4.3 Where tags live

| tags | lives | cost on the unit |
| --- | --- | --- |
| FIZZ's 7 (BASS .. FX) | **on the device**, in each preset record (exists); set from the web by a new `TAGS` command | 0 B RAM, ≈ 150 B flash |
| free tags (the Digitone's 32 words, "SHIMMER", the owner's own), favourites, notes, colours, folders | **web only**: in the browser (IndexedDB, keyed by unit + preset id + patch hash) and in each `.fizzbak` / `.fizzsnd` (`"web": {"tags": [..]}`) | 0 |
| (option) an 8th device bit, FAVOURITE | the tags byte's bit 7 (the SysEx mask is 7-bit, so it travels in LIST's flags byte) | 0 RAM; the browser and the panel learn a star (Q5) |

The web tags follow a preset by id on the same unit and by name + patch hash elsewhere (`sameSound`), so they survive a
restore onto another unit. They are drawn dashed (champagne) in the mock-up, so it is clear the FM-1 doesn't have them.

---

## 5. Protocol additions (semantic, `7D 46 5A 01`)

All replies use the existing reply codes; new statuses: `8 BUSY` (playing, or a job running), `9 NOT FOUND`,
`10 IN USE` (needs confirmation), `11 BAD TRACK` (also for STORE's LOAD with track ≥ 6, which is silently ignored today).

| cmd | name | body | reply | runs | device cost (flash / RAM) |
| --- | --- | --- | --- | --- | --- |
| `02` | STATUS | — | version, tracks, playing, safe mode, persist dirty, commit counter, current project, MY PRESETS used, store pages used / total | lock, µs | 150 B / 0 |
| `03` | FLUSH | — | STATUS after `qd_persist_flush` (stopped only) | job | 60 B / 0 |
| `12` | TAGS | source, slot, tags | STORE-style reply | lock, µs (`qd_tags_set`) | 120 B / 0 |
| `13` | RENAME | source (mine / pool / project), slot, name[12] | reply | lock (`qd_lib_replace` keeps the id; `qd_proj_name`) | 200 B / 0 |
| `14` | DELETE | source, slot, flags (confirm) | `IN USE` + users count first; with confirm, done (`qd_preset_delete` / `qd_pool_delete`; one undo level) | job when it touches patterns | 200 B / 0 |
| `15` | SWAP | source, a, b | reply | MY PRESETS: lock (two directory entries + records; ids kept). POOL: **job, stopped only** (`qd_pool_swap`'s renumber) | 300 B / ≤ 16 B (the job) |
| `16` | POOL REF | pool slot, preset id (0 = clear) | reply | lock (a reference record; `qd_pool_resolve`) | 200 B / 0 |
| `17` | USERS | source, slot | projects and steps using it (`qd_preset_usage`, `qd_pool_users_all`) | job (reads other projects' records) | 150 B / 0 |
| `23` | PROJECTS | — | 8 entries: exists, name, pages, demo | lock | 120 B / 0 |
| `24` | PROJECT | op (load, new, copy to, delete), p, q | reply | job, stopped only (`qd_proj_*`) | 150 B / 0 |
| `21` | LIST (extended) | + flag bit 2: factory tags too; entries gain the favourite bit | as now | as now | 30 B / 0 |

Undo: each command is one undo level on the unit (SHIFT + F1 undoes the web's last change, as it does an import);
`BATCH` joins a run of them, as STORE does. The page also keeps its own undo (the before-state of each command) for its
own Undo button.

The page refreshes from LIST after each change (or the unit sends an unsolicited `LIST` part for the changed entry,
like LABELS' subscription: a later option, 0 RAM).

## 6. Project backup and restore (bulk, `00 59 46`)

Two levels:

1. **Whole unit (exists):** `FZBI` / `FZBR` / `FZSR`. Restore replaces everything; same store format only.
2. **One project (new, Phase 2):** records, streamed.
   - `"FZRL" store p` → the keys and lengths of project *p*'s records (HDR, PAT 0..63, SND 1..128, MFX; the SET record
     is the unit's, not a project's), 32 entries a frame, from `qd_store_each`. Plus the ids of the MY presets its pool
     and its 'P' 5 track sounds reference.
   - `"FZRR" key off n` → `"FZRD" key off <bytes>` (≤ 240 B, `qd_store_read`, no buffer beyond the frame's).
   - `"FZRW" p count` then, per record, `"FZRK" key len crc`: the unit queues `qd_store_put_src` with a source that
     **pulls** each page from the host (`plat_host_read`, 240 B on the stack), checks the CRC before the commit page
     (the `ok` hook), answers `FZOK` / `FZER`. Stopped only; the target project must not be the loaded one (the page
     loads another first, or the unit reloads *p* after the last record).
   - **References across units**: a project's pool and track sounds name MY preset ids, which are this unit's. A
     restore therefore first sends the referenced presets (STORE, which gives new ids), then rewrites the ids inside
     the records **in the browser** before sending them ('R' 1 is the id at a fixed offset; 'P' 5's track references
     are u16 after the first 12 bytes). That needs the record layouts written down: **`docs/RECORDS.md`** (persist.c's
     comment says "a Phase 2 format would spell them out") and a test that the unit's (JieLi clang) and the wasm's
     struct layouts agree (the records are "the host's byte order and struct layouts").
   - Device cost: ≈ 2.5 KB flash, ≤ 32 B static RAM (the record cursor), ≤ 600 B stack transient.

Per-project export also lets the owner move a project between the unit and the browser preview (both store formats are
the same; only the region sizes differ), which the whole-region image can't.

---

## 7. Digitone import

### 7.1 What is already built (reuse, don't redo)

- **Digitone 1 `.dnsnd`**: ChoralRoot's reader (`site/js/fm1dnsnd.js`, the owner's GPL code, proved on controlled
  exports): the ZIP, the `AC 11 D3 03` container, the LZ4 block, the 302-byte record → FM TONE version 5 + pan, drive,
  sends, tags, notes on what didn't carry.
- **Digitone II `.dn2pst` / `.dn2snd` / a bare 359-byte record**: `site/js/dn2pst.js` (`tools/dn2pst.mjs`), with the
  facts and their sources in `research/DN2-FORMAT.md`, the E1..E4 values (AHD, portamento, filter machines, CRUSH,
  modulation slots) in `research/FMTONE-DN2-PLAN.md`; sent as STORE6.
- Tags: `digitone.js` `TAG_MAP` (Digitone words → FIZZ's 7); the rest become **web tags** (§4.3) instead of being
  dropped.
- Tests: `site/tools/test_engine.mjs`, `test_site.mjs`, `tools/test_dn2pst.mjs`, `tools/test_sysex_dnsnd.mjs` (the
  owner's presets read from outside the tree).

The manager's Import tab is today's Import panel pointed at the unit through `fizzlink`, with: a fidelity bar per
sound (from the notes), destination MY PRESETS / a pool slot (STORE, then `POOL REF`) / a track (LOAD, tracks 1..6),
duplicate detection (`sameSound`), and the web tags kept.

### 7.2 What is new

| input | what it needs | confidence |
| --- | --- | --- |
| **Digitone II `.syx`** (a sound sent from the device's SysEx dump) | Elektron's SysEx envelope (`F0 00 20 3C <product> ..`, a message id, the 7-to-8 packing, a length / checksum trailer) unwrapped to the 359-byte record, then `parseDn2pst(record)` | the envelope's exact bytes are **not verified here**: needs one dump of a known sound from the owner's unit (a test capture saved outside the repo) |
| **Digitone 1 `.syx`** | the same envelope → the 302-byte record (or its compressed form), then the `.dnsnd` record reader | same: needs a capture |
| **projects** (`.dn2prj` and a DN1 project): up to 128 pool sounds + kit sounds | the project container's LZ4 chain and the offsets (DNX `dn2image.ts`, `DN2_POOL_OFFSET`); each sound is a bare record, so `parseDn2pst` reads it | inferred; needs one real project file |
| **kits / patterns** | not imported: sounds only (p-locks, trigs and Digitone patterns are a different project) | — |

The Elektron packing is the usual "a byte of high bits, then seven low-7-bit bytes" scheme (the same family as FIZZ's
own); which bit belongs to which byte, the header bytes and the checksum are what the capture settles. Synthetic test
fixtures are made by `dn2RecordFromParams` + a writer for the envelope, never from real files.

### 7.3 Parameter mapping (summary; the full tables are in `research/DN2-FORMAT.md` §4 and `fm1dnsnd.js`)

| Digitone FM TONE | FIZZ (`core/voice/quad`, FM TONE + values 74..109) | carries |
| --- | --- | --- |
| ALGO 1..8, RATIO C / A / B1 / B2, HARM, DTUN, FDBK, MIX | ALGO, RC, RA, RB1, RB2, HARM, DTUN, FDBK, MIX (same laws, hundredths for the hi-res ones) | exact |
| operator A / B envelopes (ATK, DEC, END, LEV), DELAY, TRIG, RESET; phase reset | AATK .. BRST, PHRT | exact |
| ratio offsets | OFSC .. OFSB2 | exact |
| amp ADSR, LEVEL, VEL | EATK .. EREL, LEVEL, VEL | exact (DN1: AMP REL / DEC 127 = INF) |
| amp AHD + HOLD (DN2) | AMP MODE, HOLD (E1) | exact; HOLD's law to confirm (DN2-FORMAT §8.7) |
| filter LP4 / multimode / legacy LP-HP (DN1: one filter) | FTYPE + FREQ RESO FDEPTH, filter envelope | LP4 exact; MULTI's TYPE morph and LEGACY approximate |
| DN2 filter machines EQ, COMB+, COMB− | FTYPE 7 / 5 / 6, FSHAPE, FGAIN (E2) | close; the comb's feedback law inferred |
| base-width filter | BASE, WIDTH, BWPOST | exact |
| LFO 1..2 (DN1), 1..3 (DN2): speed, mult, fade, dest, wave, phase, mode, depth | FIZZ's 3 LFOs | mostly; **LFO MULT 6..23 ordering unproved**; destinations mapped by id, a destination FIZZ lacks → the LFO off + a note |
| overdrive, OD routing | DRIVE, DRIVE ROUTE (E1) | DRIVE exact; PRE routing a guess |
| bit reduction, SRR (DN2) | BITS, SRR, CRPOST (E3) | exact by range |
| portamento (DN2; DN1's unk170 / 172) | PORT, PORT TIME (E1) | DN2 yes; DN1 not read yet |
| mod routings (velocity, mod wheel, aftertouch, breath) | E4's 8 slots | partial: the DN2 mod records 252..311 are not decoded |
| key tracking A / B1 / B2, filter key track | AKEY, B1KEY, B2KEY, FKTRK | DN2 approximate (×1.27); DN1 left at 0 |
| pan, chorus / delay / reverb sends | the preset's mix (P1.5x) | exact |
| the sound's octave (+251) | — | **not mapped** (unknown field); a note |
| the Digitone's FX settings (delay time, reverb size, chorus), the kit's per-track settings, trig conditions, p-locks | global effects / the sequencer | **not mapped**: sounds only |
| WAVETONE, FM DRUM, SWARMER, MIDI machines | — | **refused** (listed greyed: "not FM TONE") |

### 7.4 Licences and clean-room

- `site/` is GPL-3.0-only; `core/` and `firmware/` are MPL-2.0. The importer stays in `site/` (browser JS), so it
  never enters the MPL code.
- ChoralRoot's reader is the owner's own GPL code: copied as it is today.
- Other people's work is read **as data, not code**: DNX (MIT), syxgrid (licence not stated: facts only), Elektroid
  (GPL-3.0: facts only, nothing copied even though `site/` could take GPL code, to keep the option of moving the reader
  to MPL / C later, as FM1VST did). Each fact keeps its citation in `research/`.
- The new device code (STATUS .. PROJECT, FZR*) is written from FIZZ's own `fizz_sx.c` / `app.c` / `store.h`. fm1-base's
  backup page (`web/backup/index.html`, MPL-2.0, Charles Vestal) may be read for its UI flow; its protocol isn't used.
  ChoralRoot's `web/manager.html` / `fm1manager.js` (GPL, partly Felucca-derived `fm1backup.js` © Leo Kuroshita) are
  **not** copied: the manager is written fresh (it is a different data model anyway).
- **Never in the repo:** Elektron factory presets, the owner's preset files, real `.syx` captures, Digitone projects.
  Fixtures are synthetic (`dn2RecordFromParams`, `wrapDn2Record`, a synthetic `.syx` writer). The site's release
  checks that already reject raw preset assets must cover `site/manager/` too.

---

## 8. The page (mock-ups)

`mockups/fizz-web-manager.html`, seven shots, the site's black-and-neon look (Inter Tight, FIZZ's tag colours, the
bubbles), one subject per tab, big values, no instruction text:

| shot | screen | what it shows |
| --- | --- | --- |
| 1 | **BACKUP** | the unit (FIZZ version, projects, presets, store used as a cream / green meter), one big **BACK UP** (yellow), "PRESETS ONLY", the backups kept on this computer (an older-format one marked orange), a drop zone for `.fizzbak` / `.fizzlib` |
| 2 | backing up | one big percentage, the store's 73 sectors filling (MY PRESETS cream, projects green, the sector being read lit), bubbles; CANCEL |
| 3 | **LIBRARY** | MY PRESETS as cassettes (algorithm glyph in the tag's colour, slot, ↓ imported, POOL), tag filter chips, FACTORY beside, the selected preset big: rename in place, the 7 tags as toggles, web tags dashed, which projects use it (dots = steps), + POOL / EXPORT / DELETE |
| 4 | **POOL** | the project picker, 128 places as coloured tiles (16 x 8, like the keybed's rows), dots for the steps using each, a preset dragged from MY PRESETS onto an empty place (lit yellow) |
| 5 | **IMPORT** | Digitone 1 / II / `.syx` files dropped, each sound with its source, a 10-segment fidelity bar (green exact, yellow small losses, orange approximations), the note, its tags; non-FM-TONE greyed; destination MY PRESETS / POOL / TRACK; SEND |
| 6 | **RESTORE** | before → after (projects · presets), the projects in the backup, the presets that would go; and the **FM-1 asks too**: a cassette on its screen, SAVE lit (one press, the owner's single-click rule), HOME dim to cancel |
| 7 | phone (390 px) | the library at phone width, tabs on one row |

Motion (for the build): cassettes slide into place on drop; the pool tile "fizzes" (bubbles) when a reference lands;
the backup's sectors light in order; a restore drops the cassette into the FM-1's screen as the import toast does on
the unit.

The **FM-1's confirmation screen** (shot 6) is the one device screen this adds (a restore from the web should never
happen without a press on the unit). It follows the clarity rules: one drawing, one big word, the LEDs say which key.

---

## 9. Phases

Each phase lists its device cost. "0" means no firmware change.

### Phase 0: connect, measure, back up (no firmware change)

1. **Measure on the unit** (main session; the unit, stopped and then playing): `time fizz_xdata.py backup`, then
   `fizz_xdata.py stats` (xruns, `lock_sysex_us`, `frame_max_us`); a LIST and a DUMP of everything from a tiny Python
   script over `fm1_install`'s `MidoBackend` (`firmware/tools/fizz_presets.py export DIR` / `import DIR`, FIZZ's own
   framing from `tools/fizz_sysex.mjs` ported to Python). This also gives the owner the **pre-6-track preset export**
   (§1.3).
2. `site/js/fizzlink.js`: one queue for both framings, over Web MIDI (port `FIZZ FM-1`, IDENTITY check) or over the
   wasm FIZZ; the update framing's pack / unpack from `site/install/fm1lib.js` (already generated from fm1-base).
3. `site/manager/index.html` with **BACKUP** (whole region: `FZBI` / `FZBR` with pipelining, CRC check and one retry;
   `FZSR` restore with the before → after summary; presets-only export via DUMP) and **LIBRARY read-only** (LIST,
   export, import via STORE / STORE6 into the unit).
4. The install page's "Back up FIZZ" links to `../manager/#backup` (the 404 goes away; `install.js`'s HEAD check
   points there).
5. `.fizzbak` writer and reader, including the offline store reader (lists a backup's projects and presets).

Device cost: **0 B RAM, 0 B flash.** Risk: round-trip speed (step 1 decides pipelining).

### Phase 1: library and pool editing (firmware: semantic commands)

- STATUS, FLUSH, TAGS, RENAME, DELETE, SWAP, POOL REF, USERS, PROJECTS, PROJECT (§5); LOAD's BAD TRACK for 6 tracks.
- The pool jobs stepped outside the lock, refused while playing.
- The manager's LIBRARY (rename, tags, delete with users, reorder MY PRESETS), POOL (drag in, swap, clear, sort by
  tag, clear unused), IMPORT pointed at the unit; web tags in IndexedDB and in the files.
- `docs/SYSEX.md` updated (protocol stays 1: new commands only; older FIZZ answers `BAD COMMAND`, which the page reads
  as "update FIZZ for this").

Device cost: **≈ 2 KB flash, ≤ 16 B static RAM** (the job cursor), stack within today's STORE handler's. Undo: each
command a level in the existing 16 KB log (no new memory).

### Phase 2: per-project backup and restore (firmware: FZR*)

- `docs/RECORDS.md` (the record layouts, little-endian, fixed offsets) + a layout test (unit vs wasm vs host).
- `FZRL` / `FZRR` / `FZRW` / `FZRK` (§6); the page's project export / import, the id remapping, moving a project
  between the unit and the browser preview.
- The FM-1's restore confirmation screen (shot 6) for `FZSR` and `FZRW`.

Device cost: **≈ 2.5 KB flash (+ ≈ 1.5 KB the confirmation screen), ≤ 32 B static RAM, ≤ 600 B stack transient.**

### Phase 3: Digitone extras

- `.syx` (DN1 and DN2) once a capture exists; DN2 project pools; DN1 portamento / key tracking (DN2-FORMAT §9).
- Optional: FZBR 512-byte chunks and blank-page skipping if Phase 0's numbers ask for it (stack +600 B transient).

Device cost: **0** for the importer; the transfer options as above.

### (Separate decision) carrying 8-track projects into the 6-track store

Only with a core change (the 6-track agent's area): either the projects' store keeps reading Z2FS 'P' records and
drops tracks 7..8 on load, or a one-off migration in `tools/flash_dump.c --migrate` style that the manager runs on a
`store.bin` in the browser (wasm) and restores as a Z3FS image. Q1.

---

## 10. Test plan

**Host / core (no unit):**
- `core/tests/test_sysex.c`: every new command, every status (BUSY while playing, IN USE, NOT FOUND, BAD TRACK for
  6..7), undo of each, BATCH; a pool SWAP job stepped to completion gives the same patterns as `qd_pool_swap`.
- `core/tests/test_persist.c`: FZR* round trip (export a project's records, restore into another slot / a blank store,
  `flash_dump` lines equal); a restore cut halfway leaves the store mountable.
- `firmware/app/build/host/fizz_host` scripts (`firmware/app/tests/*.txt`, the `sysex F0 .. F7` command): STATUS,
  LIST, the new commands and the bulk commands through `app_sysex` / `app_usb_cmd` as the unit runs them; the store
  test `store.txt` extended.
- The record layout test (Phase 2): the same records written by the host build, the wasm and the device build's
  `sizeof` / `offsetof` table compared.

**Browser, Node (no unit), in the style of `site/tools/test_install.mjs` and `test_engine.mjs`:**
- `site/tools/test_manager.mjs`: `fizzlink` against **a simulated FIZZ** (the wasm `fizz.wasm` for the semantic channel,
  a JS model of `FZBI` / `FZBR` / `FZSR` / `FZR*` for the bulk channel, as `test_install.mjs` simulates the unit),
  including dropped replies, a reply out of order (pipelining), a CRC mismatch (the store changed: one retry), a
  restore refused while playing, a different store magic (offers presets only), an 8-track `.fizzbak` on a 6-track unit.
- `.fizzbak` round trip: backup → file → offline listing (the JS store reader against `tools/flash_dump`'s listing of
  the same image) → restore → backup again, byte for byte.
- Import: synthetic `.dnsnd`, `.dn2pst`, `.syx` fixtures only (`dn2RecordFromParams`, `wrapDn2Record`, a synthetic
  envelope), each through STORE / STORE6 into the wasm, the values read back (`test_sysex --feed`). The owner's files
  stay outside the tree (`tools/test_sysex_dnsnd.mjs`'s pattern: a path argument).
- A release check: no `.syx`, `.dnsnd`, `.dn2pst`, `.dn2prj` or Elektron names under `site/` except in tests' synthetic
  generators.

**Browser, headless Chromium** (`site/tools/test_site.mjs` style; Chromium.app `--headless=new`, never a window):
- the manager page loads with no console errors at 1280 and 390 px, no horizontal scroll; every tab against the
  wasm FIZZ (the page's "use the browser FIZZ" mode); the screenshots compared with the mock-ups by eye.
- Not for the unit: headless Chromium's Web MIDI mangles SysEx (memory: doubled F0, running-status notes). Unit tests
  run from Python or from the owner's Chrome.

**The emscripten web emulator test** (memory: `~/emsdk`, sourced by the build): `site/build_wasm.sh && node
site/tools/test_engine.mjs` must pass after every core change in Phases 1..2, and the wasm must be rebuilt to the
6-track core before the manager is published (today's site wasm is 0.10.0-p2.1, 8 tracks).

**On the unit** (the main session only; one device session at a time; MASTER low):
- Phase 0: the measurements of §9 step 1; a backup, a restore of the same image, `fizz_xdata.py stats` shows 0 xruns.
- Phase 1: each command once by hand from the owner's Chrome while stopped and while playing (BUSY where expected),
  stats after: 0 xruns, `lock_sysex_us` under 1 ms.
- Phase 2: a project exported, deleted on the unit, restored; played; the stats.
- Each phase's PASS recorded in PLAN.md §13's log.

---

## 11. Open questions for the owner

1. **The 6-track change wipes the store** (§1.3). Is losing the 8-track projects fine (MY PRESETS carried over by
   export / import), or should the projects be carried (tracks 7 and 8 dropped) with a core change before that build is
   flashed?
2. **The browser preview's saves**: offer "export my presets" from the old 8-track preview before the site's wasm moves
   to 6 tracks?
3. **Where does the manager live**: a new `site/manager/` page linked from the player and the installer (recommended),
   or inside the player's sheet?
4. **Restore confirmation on the unit**: should a web restore need a press on the FM-1 (SAVE lit, HOME cancels,
   shot 6)? Recommended for whole-unit and project restores; not for single presets.
5. **Favourites**: a device-side star (the tags byte's 8th bit, shown in the browser and in LIST), or web-only?
6. **Free tags**: OK that tags beyond FIZZ's seven live only in the browser and in backup files (dashed in the page),
   not on the FM-1?
7. **Pool edits while playing**: refuse pool swaps / deletes while the sequencer plays (they rewrite every pattern), or
   allow them with a short sound gap?
8. **Deleting a preset that projects use**: the page shows the users and asks; on the unit those steps fall back to the
   track's sound (today's panel behaviour). Same from the web?
9. **Digitone `.syx`**: can you capture one SysEx sound dump each from the Digitone and the Digitone II of a sound you
   also have as `.dnsnd` / `.dn2pst` (kept outside the repo) so the envelope can be checked? And a DN2 project file for
   pool import?
10. **Digitone project import**: sounds only (pool and kit sounds into MY PRESETS / a pool), or is a pattern import
    (trigs, p-locks) wanted later?
11. **Browsers**: Chrome / Edge on a computer only (Web MIDI with SysEx), as the installer, is enough?

## Owner decisions (2026-10-24)

- 8-track projects are not carried over (fresh start; MY PRESETS kept). The device store before the change is backed up outside the repo.
- The manager is a new `site/manager/` page.
- A web restore needs a press on the unit: RESTORE?, SAVE goes ahead, HOME cancels.
- Tags: the device's 7 tags, plus free-form tags kept only in the browser and backup files.
- Digitone `.syx` dumps are skipped for now; only the formats that already convert are supported.

## Phase 0 built (2026-10-24, no device change)

- `site/manager/` (`index.html`, `manager.js`, `manager.css`), `fizzlink.js` (both channels, one queue), `fizzbak.js`
  (.fizzbak: manifest, `store.bin`, `presets/*.fizzsnd` with `web.tags`, and a `library.json` so the preview's Import
  reads it as a .fizzlib), `fizzstore.js` (the offline store reader: device and host layouts), `simfizz.js` (the
  simulated unit: the wasm for the semantic channel, a model of FZBI / FZBR / FZSR). Tests: `site/tools/test_manager.mjs`.
  The install page's "Back up FIZZ" links to `../manager/#backup`; the player's header has "Backup".
- Restore: the page first saves the unit (a "before restore" .fizzbak), then needs RESTORE typed; the unit itself
  refuses while playing (FZER 10). A cancel is honoured only while the unit still checks the image (the remaining
  reads are answered with zeros: its CRC check fails, FZER 12, nothing erased). The press on the unit (owner decision)
  is a later firmware phase.
- **Pipelining:** fm1-base holds one inbound SysEx frame and *drops* a frame that arrives while one waits
  (`bsp/chip/usb.c` `sysex_byte`: `sx_collect = !sx_ready`; the NAK in `ep1_rx` is for the MIDI ring only), so §3.1's
  "nothing is lost" does not hold. The page defaults to one FZBR at a time; `?depth=2&gap=MS` sends the next request
  `gap` ms after the last (a dropped one is sent again, replies matched by offset, the CRC-32 checks the whole) for
  measuring on the unit. Every backup logs its timing (KB/s, ms a read, round trips, resends) in the page's LOG and
  the console, and writes it into the manifest (`transfer`).

## Phase 1 built (2026-10-24, branch web-manager-p1)

**Firmware (core):** the ten commands of §5 (STATUS 02, FLUSH 03, TAGS 12, RENAME 13, DELETE 14, SWAP 15, POOL REF 16,
USERS 17, PROJECTS 23, PROJECT 24) and STORE's BAD TRACK, in `core/app/fizz_sx.c` (`sx_edit`: the handlers; `qd_sx_work`:
the jobs), documented in `docs/SYSEX.md` ("The web manager's commands"). The quick ones run in the handler (a directory
entry, a sound-cache entry, a record left for the autosave); everything that pages patterns, reads other projects'
records or writes flash is a job (`qd_sx_t.wk`, 20 B in `qd_app_t`: the pool on the unit) stepped by the store's own
step (`qd_persist_step`, between frames, outside the locks; the same place f53f9fa's pool sweep runs), refused while
playing (BUSY) and paused if PLAY comes meanwhile. `persist.c` gained the per-step pieces: `qd_pool_swap_slots` +
`qd_pool_renumber_pat` (a pattern out of RAM renumbered through its record in the arena's room, written again only
if it changed), `qd_preset_will_change_proj`, `qd_preset_usage_proj`.

Not done from §5: LIST's favourite bit (owner question Q5 open); RENAME of a pool slot (the page renames its MY preset
instead); FLUSH writes everything in one step (`qd_persist_flush`, stopped only).

**Memory:** main RAM (.data + .bss) +0 B on both builds (dual 1,148 B free, single-core 540 B); the pool −32 B (dual
22,896 free) / −16 B (single-core 12,768 free); flash +4,032 B on both (dual 21,676 B free, single-core 17,868 B):
about twice §9's estimate, the handlers at -O2. Ratchet raised: flash_arm 440,129 → 444,169, flash_pi32v2
462,169 → 465,981, ram 376,816 → 376,832 (the job's state in `qd_app_t`).

**Web:** `site/manager/` LIBRARY (rename in place, the 7 device tags as toggles, the web tags, USED BY with dots,
+ POOL, EXPORT, DELETE with IN USE asked once more, a cassette dropped on another or ALT + ↑ / ↓ to reorder), POOL
(`pool.js`: the project picker that loads another project with a second press, 128 tiles with the steps using each
filled in the background, a MY preset dropped or picked then a tile clicked = POOL REF, a tile dropped on another =
SWAP, CLEAR, SORT BY TAG, CLEAR UNUSED), IMPORT (`importer.js`: `.dnsnd` / `.dn2pst` / `.dn2snd` read with
`js/digitone.js`, the 10-segment fidelity bar, non-FM-TONE greyed, duplicates unticked, MY PRESETS / POOL / TRACK,
Digitone tags FIZZ lacks kept as web tags; `.syx` not read: owner decision), and BACKUP's numbers from STATUS without
a backup. `fizzlink.js` has a method per command; `simfizz.js` runs them on the wasm (its jobs step as the unit's;
`tapPlay()` for BUSY). Tests: `site/tools/test_manager.mjs` (protocol, Node), `site/tools/test_manager_page.mjs`
(the page in headless Chromium with `?sim`, both widths).

### The SysEx speed (built, owner-approved, 2026-10-25)

Both fixes below are in `firmware/app/app.c`: `FZ_XFER_HOLD_MS` 300 / `FZ_XFER_FRAME_MS` 250 in `fz_step` (the last FIZZ
frame's time in `qd_sx_t.rx_ms`, set by `app_sysex` and the store's FZB* commands), and in `fz_input` inside a frame
FIZZ's semantic frames (not STORE / STORE6 / POOL REF) with `fz_sysex_out` sending only what the USB queue takes at once
there (no 200 ms wait inside a frame). Flash +176 B, RAM +0 (the 4 B are in `qd_app_t`). The simulator's
`tests/xfer.txt`: 30 frames looked at in 1 s normally, 4 during a transfer, 30 again 400 ms after it. Not measured on
the unit yet. The analysis as proposed:

Phase 0 measured ~87 ms a round trip on the unit. The cause is fm1-base's loop: it holds **one** inbound SysEx frame
(`bsp/chip/usb.c`: `sx_collect = !sx_ready`, a second frame arriving while one waits is dropped) and hands it over
once a pass (`device/main_fm1.c`: `ota_frame_get` → `app_sysex`, then `usb_cmd(ota_service())`), and a pass includes
`app_frame()`: FIZZ's drawing, 30..93 ms while anything animates. Two cheap changes in FIZZ's port, cheapest first:

1. **Transfer pacing** (`firmware/app/app.c` `fz_step`): while FIZZ SysEx frames keep coming (one in the last
   ~300 ms: `qd_sx_t` gets the time of the last one, 4 B in `qd_app_t`, i.e. the pool, 0 B main RAM), skip drawing
   except every 250 ms (a 4 fps screen during a transfer). A pass is then ~1 ms, so the round trip becomes the
   host's MIDI latency plus a ms: estimated 3..6 ms instead of 87 (the 300 KB backup ~5 s instead of ~100 s; a
   DUMP of 256 presets ~1.5 s). Cost ≈ 60 B flash. The screen pauses during a transfer (the bubbles slow down); the
   sound is untouched (it is the interrupt's).
2. **Frames between strips** (`app.c` already takes fm1-base's remote frames, 7D 4C 43, between strips: P2.6): take
   FIZZ's own frames there too (`app_sysex` for 7D 46 5A, and the bulk framing's FZBR through `app_usb_cmd`), and
   send the replies that fit without waiting (`plat_sysex_send` may wait up to 200 ms for USB room: a non-waiting
   variant between strips). While playing (when 1 cannot skip frames) that is a frame per strip, 6..12 a frame
   instead of one. Cost ≈ 120 B flash, 0 RAM; each strip gap grows by one handler (≤ 0.1 ms).

Not recommended: a second inbound frame buffer in fm1-base (2 x 640 B of main RAM, which FIZZ does not have) to let
the page pipeline requests; with 1 the round trip is small enough that depth 1 is fine.
