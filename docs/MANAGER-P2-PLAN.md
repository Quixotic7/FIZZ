# FIZZ web manager, Phase 2: sound editor, export, projects and patterns, the browser's project library (plan)

Status: **plan and mock-ups only** (2026-10-24, branch `manager-p2-plan`). Nothing in `core/`, `firmware/` or `site/` is
changed. It builds on `docs/WEB-MANAGER-PLAN.md` (Phases 0 and 1 are built: the manager page, the Phase 1 SysEx
commands STATUS .. PROJECT, the SysEx speed fixes) and `docs/SYSEX.md`.

Mock-ups: `mockups/fizz-manager-p2.html` (`?shot=1..8`, `?sheet=1`), rendered by
`python3 mockups/render_png.py p2-1 .. p2-8 sheet-p2` to `mockups/png/fizz-manager-p2-01-edit-operators.png` ..
`-08-unload.png` and `fizz-manager-p2-sheet.png`.

The owner's four requests:

1. **Sound editor.** Select a sound in LIBRARY and see its parameters drawn (algorithm, operators, envelopes, filter,
   LFOs ..), edit them, hear the edit on the FM-1 at once, save to MY PRESETS.
2. **Export.** One preset as FIZZ's own `.fizzsnd`, a Digitone `.dnsnd`, or a Digitone II `.dn2pst` / `.dn2snd`, with
   what doesn't carry listed.
3. **Projects and patterns.** Switch projects from the web, see a project's patterns, copy or move a pattern from one
   project to another.
4. **The project library in the browser.** Keep projects in the browser (IndexedDB), unload one from the FM-1 to free
   its slot, load stored ones back, swap quickly; more projects than the FM-1's 8 slots. The projects' sounds travel
   with them.

The player page as a big screen for the unit (screen mirroring or a state-synced wasm) is covered in
**`docs/WEB-MIRROR-PLAN.md`** (another agent), not here.

---

## 0. Numbers this plan uses

| | value | source |
| --- | --- | --- |
| a SysEx round trip, unit stopped | **~7 ms** | the brief (after sysex-speed 2; Phase 0 measured 49..112 ms before it) |
| a block | **256 B** (a store page's payload is 240 B) | `FZBR`, `QDS_PAYLOAD` |
| flash free, dual core | **~20.8 KB** | the brief (Phase 1 left 21,676 B; sysex-speed took 176 B, more since) |
| flash free, single core | ~17 KB | Phase 1: 17,868 B, less the same changes |
| main RAM (.data + .bss) | **+0 B allowed** (dual 1,148 B free, single-core 540 B) | WEB-MANAGER-PLAN Phase 1 |
| the pool (`qd_app_t` lives there) | dual ~22.9 KB free above fm1-base's 8 KB demand | same |
| the projects' store | 51 sectors, 765 pages, ~615 usable for the 8 projects; the owner's projects ~51 pages today; a full project of 64 dense patterns ~835 (does not fit) | `persist.h` |
| a pattern record ('P' 5, demo density) | ~1.6 KB (7 pages) | `persist.c` |
| MY PRESETS | 256 places, ids never reused | `persist.h` |

Estimates are marked E. Phase 1 came out at **twice** its flash estimate (handlers at -O2), so every flash figure below
gives the estimate and the 2x figure.

---

## 1. Sound editor

### 1.1 What the page shows (shots 1..3)

The page draws the sound as FIZZ's own editor does, only bigger: one group at a time, the group keys down the left
(the device's groups: OPERATORS, OP ENV, FILTER, AMP, LFO, MOD, PLAY (portamento, keys, trig)), each value a part of a
drawing that is dragged, in its knob's colour (KNOB 1 blue, 2 green, 3 cream, 4 orange, ALGORITHM pink;
`core/ui/ui.h` UI_K1..UI_KALG). The operators take the knobs' colours in order: C blue, A green, B1 cream, B2 orange.

- **OPERATORS** (shot 1): the algorithm's graph big (the same rules as `ui_snd.c` `sd_algo`: modulators above, carriers
  on the bus, feedback a loop, the modulations' dots running), ALGO 1..8 a big pink number; a card an operator (RATIO
  big, the level a bar); HARM, DTUN, FDBK, MIX, DRIVE as arcs.
- **FILTER + AMP** (shot 2): the filter's response curve (the machine as a chip row: LEGACY, LP4, MULTI, COMB+, COMB−,
  EQ), FREQ and RESO the curve's handle; the amp and filter envelopes as shapes with handles; BASE / WIDTH / KEY TRK /
  LEVEL / CRUSH as chips.
- **LFO + MOD** (shot 3): each LFO's wave in its colour, its destination a chip, SPEED and DEPTH; the eight E4 slots as
  cables, their thickness the depth.
- Header: the preset's name and place, its tag, **TRACK 3 · LIVE** (which track plays the edit), REVERT, SAVE, SAVE AS
  NEW. Scroll wheel and arrow keys step a hovered value; SHIFT the fine step (hundredths of the hi-res values, as SHIFT +
  a knob on the unit).
- Clarity rules: values big, labels ≥ 12 px and only for values, no instruction text.

The values' table (names, ranges, defaults, hi-res) is already in the site: `site/js/dn2pst.js` `QUAD_RANGES`,
`QUAD_EXT_RANGES`, `QUAD_HIRES`, and `site/js/fizz2dn.js` `Q` (the 110 ids); the pages and groups follow `ctl_snd.c`
`QD_SND_PAGES`. The page edits the 110 values and packs the version-6 blob itself (`fizz2dn.js` `fizzValues` + a
`quadPack6` beside it).

### 1.2 How an edit is heard on the FM-1: options

| option | how | each edit costs | device cost | verdict |
| --- | --- | --- | --- | --- |
| A. STORE6 to a scratch MY PRESETS place, LOAD onto a track | the existing STORE6 with LOAD | ~190 B frame, one round trip, **a flash write of the preset each time** (the autosave) plus the track | 0 | **no**: wears the library store, fills undo, uses a slot |
| B. **PARAM** (new): one value of a track's sound | `qd_player_live_edit(track, id, value)`: exactly what a knob turn does on the unit (sounding voices follow) | an 11-byte frame, no reply | ~200 B flash, 0 RAM | **yes, for every drag** |
| C. **AUDITION** (new STORE6 flag): the whole sound onto a track, nothing stored | `qd_track_load_sound` without the library | ~190 B, one round trip | ~80 B flash, 0 RAM | **yes, to open a preset in the editor and for REVERT** |
| D. A hidden preview voice outside the project | a sound held apart from the tracks, the page's notes play it | as B | a 220-byte patch + a flag in `qd_app_t` (the pool), the player's routing (~600 B flash) | possible later (Q1.1); the pool has room, main RAM untouched |

**Recommended: C to start, then B for each drag.** The audition track is the unit's selected track by default (a
chooser in the header). The page sends notes on that track's MIDI channel (the existing note path:
`qd_app_midi_play` / `qd_app_midi_ui`), from an on-screen strip or the computer keys, or the owner plays the FM-1's
own keys.

What this means: the edit **is** the track's sound in the open project, as if the knobs had turned on the unit (the
track's sound is marked edited and autosaved in its pattern). On leaving the editor without SAVE the page sends the
before-sound back (AUDITION), so the project is as it was; if the page dies meanwhile, the track keeps the edit (seen,
and undone on the unit's own sound page). Q1.1 asks whether that is acceptable or D is wanted.

The other way round: a knob turned on the FM-1 while the page edits changes the track's sound too. The page reads the
track back (DUMP of the track, below) twice a second while idle (one round trip each) and redraws.

### 1.3 Firmware commands (semantic channel, `7D 46 5A 01`)

| cmd | name | body | reply | runs | flash E (2x) | RAM |
| --- | --- | --- | --- | --- | --- | --- |
| `30` | **PARAM** | track (0..5), id (0..109), value (3 x 7 bits, + 2^20 offset: −6400..12700 fits), flags (`01` ACK) | none; ACK `52` only with the flag or on an error (BAD TRACK, NOT FOUND for a bad id; the value clamped as `snd_put` does) | handler (`qd_player_live_edit`: a few µs) | 200 (400) | 0 |
| `22` | DUMP, **source 4 = a track** | slot = track 1..6 | a STORE6 frame (DUMP flag) of the track's sound: name, patch, the track's pan and sends, its preset id in a new trailing byte pair | handler | 120 (240) | 0 |
| `11` | STORE6 flag **`10` AUDITION** | as STORE6 with LOAD and a track | STORE's reply, slot 0 | handler: `qd_track_load_sound` and the mix; no library write, no undo level (the page has the before) | 80 (160) | 0 |
| `11` | STORE6 flag **`20` OWN** | as STORE6 | as STORE | the preset is the owner's (not marked imported), its tags byte and mixmask kept as given (SAVE from the editor) | 40 (80) | 0 |
| `11` | STORE6 over a used place, **as a job when other projects use it** | — | STORE's reply when done | `lib_put` → `qd_preset_will_change` rewrites every record that loads the preset in **all 8 projects** inside the handler today (under the lock). With USERS' per-project step (`qd_preset_will_change_proj`, Phase 1) this becomes a job, stopped only (BUSY while playing) | 250 (500) | 0 (the job's state fits `qd_sx_work_t`) |

Total **~0.7 KB (1.4 KB)** flash, **0 B RAM** anywhere. Undo: PARAM and AUDITION add no undo level (the page's REVERT
and the unit's own sound page are the undo); SAVE is one level, as STORE.

**Finding (today's code):** a STORE over a used MY PRESETS place already calls `qd_preset_will_change` in the handler
(`persist.c` `lib_put`), which may read and write other projects' pattern records under the SysEx lock: the same
xrun risk that made Phase 1 turn pool swaps into jobs. The editor's SAVE makes this common, so it is fixed in the same
phase (the last row).

### 1.4 Times

| action | cost |
| --- | --- |
| open a preset in the editor | DUMP (from Phase 1's cache, else 1 round trip) + AUDITION 1 round trip: **~15 ms** |
| a drag | PARAM frames as fast as the page sends them; the page sends the latest value per id once the previous frame has left (a frame ≈ 1 ms on the wire): **≤ 60 a second, heard in ~2..10 ms** |
| SAVE | 1 round trip; with other projects using it, ~15 ms a project (the job) |
| keeping in step with the panel | DUMP of the track, 2 a second |

Without a unit (or before connecting) the same editor plays the **browser's FIZZ** (`site/fizz.wasm`, the same core),
so presets can be edited anywhere and sent later.

### 1.5 Risks

- **Notes from the page while REC is armed are recorded** (`qd_rec_midi_note`), as any MIDI keyboard's. The page shows
  REC from STATUS and its keyboard strip turns red while armed.
- A drag storm while the unit draws a busy screen: PARAM is handled between strips (sysex-speed's path), so it waits at
  most a strip; the page never queues more than one PARAM per id.
- Version mismatch: an older FIZZ answers PARAM / track DUMP with BAD COMMAND / BAD SLOT; the page then edits in the
  browser only and offers SAVE (STORE6), with "update FIZZ for live editing".

---

## 2. Export

### 2.1 Already built (reuse)

- `.fizzsnd` (FIZZ's JSON, `site/js/library.js` `soundToFizzsnd`): the manager's LIBRARY already exports it (Phase 1),
  with the web tags.
- **Digitone and Digitone II writers exist**: `site/js/fizz2dn.js` (from `tools/fizz2dn.mjs`, 2026-10-22) `fizzToDnsnd`
  and `fizzToDn2pst`, each returning `{bytes, notes, record, payload}`; `site/js/dnexport.js` `digitoneExport` wraps
  one or several (a ZIP). Checked by `tools/test_fizz2dn.mjs` (Elektron's LZ4 rebuilt for all 463 blocks on hand; all 462
  Digitone files round-trip to the same FIZZ patch; 400 random FIZZ patches through each format). Facts and confidence:
  `research/DN2-FORMAT.md` §11.
- **`.dn2snd`** is the same stored container without the ZIP (`DN2-FORMAT.md`: a Transfer / backup folder's
  `soundbanks/A/001 NAME.dn2snd`): it is `fizzToDn2pst(..).payload`. One line.

### 2.2 What is new (site only, 0 firmware)

- The LIBRARY's EXPORT opens shot 4: three homes (FIZZ, DIGITONE, DIGITONE II with `.dn2pst` / `.dn2snd`), each with the
  10-segment carry bar (the importer's fidelity bar, run backwards: 10 minus the notes' weight) and the list of what that
  format drops for *this* sound (from `notes`), then DOWNLOAD. Several selected: a ZIP (`digitoneExport`).
- A `.fizzsnd` also carries the sound's id and a hash of its patch, so a project file (§4) can re-link it.
- The Digitone files are marked **EXPERIMENTAL** until a Digitone and a Digitone II owner confirm they load
  (`DN2-FORMAT.md` "The next proof"): Q2.1.

### 2.3 FIZZ → Digitone: what carries (summary of `fizz2dn.js` / DN2-FORMAT §11)

| FIZZ | Digitone (`.dnsnd`) | Digitone II (`.dn2pst` / `.dn2snd`) |
| --- | --- | --- |
| ALGO, the 4 ratios, HARM, DTUN, FDBK, MIX, offsets, op envelopes A / B, DELAY, TRIG, RESET, PHRT | exact (fine bytes in 1/256 instead of hundredths) | exact |
| RATIO A off the Digitone's 36 steps | the nearest step (noted) | the nearest step |
| amp ADSR, LEVEL | exact | exact |
| amp AHD + HOLD (E1) | **as an ADSR like it** (noted) | kept |
| filter OFF / LP12 / HP12 / LP24 | exact (LP24 = LOWPASS 4) | exact (LEGACY / LOWPASS 4) |
| filter MULTI / COMB+ / COMB− / EQ (E2) | **approximated** (LP2 / HP2 / off; noted) | their machines; FSHAPE → TYPE, FGAIN → GAIN (laws not verified) |
| filter DEC 127 with SUS < 127 | 126 (127 is INF there) | 126 |
| BASE, WIDTH, BWPOST | BASE / WIDTH; BWPOST **dropped** (noted) | kept |
| key tracks A / B1 / B2, filter key track | percent (101 steps for 128) | the same |
| LFO 1, 2 | yes (destinations by id; one the Digitone lacks: off, noted) | yes |
| LFO 3 | **dropped** | kept |
| DRIVE / DRIVE ROUTE | DRIVE (the DN1's is before the filter: noted) | OVERDRIVE + OD ROUTING |
| CRUSH (BITS, SRR, CRPOST, E3) | **dropped** | kept (laws not verified) |
| portamento ON / TIME | written (ON = 64 inferred) | written |
| portamento LEGATO, SLOPE RATE | LEGATO as ON; SLOPE **dropped** | the same |
| VEL (the operators' velocity) | **dropped** | **dropped** |
| the 8 modulation slots (E4) | **not written** (the DN1's records are not decoded) | written where a record exists (inferred); RAND, F ENV sources and a 5th slot of a source **dropped** |
| name | upper case, 16 characters | the same |
| tags (FIZZ's 7) | BASS, CHORD, ARPEGGIO, PAD, LEAD, PERCUSSION, SOUND FX | the same |
| pan, chorus / delay / reverb sends | exact | exact |
| web tags, favourites | not carried (Transfer's tags are a fixed list) | the same |

### 2.4 Licence

- `site/` is GPL-3.0-only; the writers are FIZZ's own code built from **facts**: DNX (MIT) for the field map, syxgrid
  (no licence: facts only) and Elektroid (GPL: facts only) for the container, the owner's own controlled exports for
  the laws. No code copied; every fact cited in `research/DN2-FORMAT.md`.
- **Never in the repo:** Elektron factory sounds, the owner's `.dnsnd` / `.dn2pst` files, `.syx` captures. Tests read the
  owner's files from outside the tree (a path argument) and use synthetic records (`dn2RecordFromParams`). The site's
  release check that rejects raw preset assets covers `site/manager/`.

---

## 3. Projects and patterns

### 3.1 Switching projects (built in Phase 1)

PROJECTS (`23`) lists the 8; PROJECT (`24`) op LOAD switches (stopped only; the current one written first). The new
**PROJECTS** tab (shot 5) shows the 8 slots big (name, patterns used as a 64-dot strip, the one playing marked); a click
on a slot's name loads it (a second click confirms, as POOL's picker does today).

### 3.2 Viewing a project's patterns (shot 6)

64 tiles (4 banks x 16), each with six 16-step rows in the tracks' colours: the first page's trigs, or the densest page
when the first is empty; empty patterns dim; the one playing ringed.

| cmd | name | body | reply | runs | flash E (2x) | RAM |
| --- | --- | --- | --- | --- | --- | --- |
| `25` | **PATTERNS** | project p (0..7), from pattern k | `65` parts of 8 patterns: `<k> <used> <length> { <page mask> <first page's trigs: 3 x 7 bits> } x 6` (~22 B a pattern) | job: a pattern a step (`rd_t` through its record: the per-track page and step masks lead each track's block, so only the masks are read, the steps skipped); the open project's patterns in RAM read directly; while playing too (reads only) | 600 (1,200) | 0 (`qd_sx_work_t`'s cursor) |

Time: 64 patterns at one a store step (every 15 ms on the unit) **≈ 1 s**, 8 reply frames. The page caches each
project's map with STATUS's commit counter and asks again only when it changed.

Zero-flash alternative (the trim if flash runs short): the page reads the whole project with §4's record stream and
parses the masks itself in the wasm (§4.6). That costs a project read (0.3..1 s) instead of 1 s.

### 3.3 Copying or moving a pattern between projects

What makes it more than a byte copy: a pattern's steps lock **pool numbers** (1..128) of *its* project, and its track
sounds may be references to preset ids ('P' 5) or full copies. Preset ids are unit-wide, so track sounds carry over as
they are; the pool numbers must be **re-mapped** into the target's pool.

**Recommended: a device job** (`26` PATCOPY), because the device already has every piece:
`qd_pool_renumber_pat` reads a pattern record into the drawing's arena (`qd_pat_borrow`), renumbers and writes it
again: PATCOPY is that with a map and another destination key.

| cmd | name | body | reply | runs | flash E (2x) | RAM |
| --- | --- | --- | --- | --- | --- | --- |
| `26` | **PATCOPY** | from project p, pattern i; to project q, pattern j; flags `01` MOVE (clear the source after), `02` OVERWRITE (else IN USE when j is used) | ACK (a = q·64 + j, b = pool slots added); FULL (the target pool lacks room: b = how many needed); IN USE; BUSY | job, stopped only: (1) the source pattern's pool numbers used (a 128-bit set in the arena's room); (2) each one's preset id (p's 'R' records); (3) in q's pool the slot referencing that id, else the first free one + a new 'R' record; (4) the pattern read into the arena, its locks and track sources renumbered, written as (q, j); (5) MOVE: (p, i) cleared. q may be the open project (its RAM copy replaced, the screen told) | 1,000 (2,000) | **0** (the map and the set live in the arena during the step; the job's ids in `qd_sx_work_t`) |

- Within one project (p = q) it is the panel's COPY PTN / PASTE with no remap.
- Time: **~0.1..0.3 s** (a ~1.6 KB record read and written, ≤ 128 small 'R' records looked up), plus the target's
  autosave.
- Order of writes for power loss: the target's new 'R' records first, then the pattern; MOVE clears the source last. A
  cut leaves at worst an extra pool entry or both copies (never none).
- **The MIDI FX groups**: a pattern's tracks name the project's MIDI FX groups (1..5, the 'X' record), which differ
  between projects. Q3.2: keep the numbers (the target's groups play), or copy the groups when the target's are empty?
- Undo: a level on the unit when q is the open project; a cross-project MOVE's source clear is not undoable on the
  unit (the page offers UNDO by copying back while it is open).

The browser-held projects (§4) use the **same code in the wasm**: the page loads both projects' records into the
browser FIZZ's store, runs PATCOPY there, and reads the target's records out (§4.6). So a pattern can go from a stored
project into one on the unit by: stored project → wasm → (q's records) → the unit (§4's write), or more simply: the
page loads the stored project into a free slot first.

---

## 4. The project library in the browser

### 4.1 The model

- The FM-1 keeps **8 project slots** in one store (~615 pages shared). The browser keeps **any number** of projects in
  IndexedDB (`fizz-projects`: one entry a project: its manifest, its records, its sounds), each also downloadable /
  droppable as a **`.fizzprj`** file.
- **Unload** = read the project from the unit, check it, keep it in the browser, then free the slot (PROJECT DELETE,
  Phase 1). **Load** = write a stored project into a free slot (or over an unloaded-and-kept one), its sounds first.
  **Swap** = unload slot k + load another into k (shot 5's drag onto a slot).

### 4.2 `.fizzprj` (ZIP, stored, as `.fizzbak`)

```
manifest.json     { "format": "fizz-project", "version": 1, "fizz": "0.13.0", "date": ISO,
                    "store": { "magic": "Z3FS", "prjFormat": 2, "tracks": 6, "npat": 64 },
                    "name": "WINTER LP", "pages": 51, "patterns": [0, 1, 2, ..] (used),
                    "records": [{ "key": "02040000", "kind": "HDR", "len": 112, "ver": "H1", "crc32": ".." }, ..],
                    "sounds": [{ "id": 141, "name": "FOG", "hash": "<fnv/crc of name+patch>", "file": "sounds/141 FOG.fizzsnd",
                                 "users": { "pool": [12], "tracks": [[0, 3]] } }, ..],
                    "factory": [{ "id": 2, "name": "FM PIANO", "hash": ".." }], "web": { "tags": {..}, "notes": "" } }
records/HDR.rec, records/PAT/00.rec .. 63.rec, records/SND/001.rec .., records/MFX.rec    (the bytes as the unit holds them)
sounds/141 FOG.fizzsnd ..   (every MY preset the project references, as .fizzsnd: the portable copy)
```

The records are kept **as they are** (no conversion): the unit can take them back byte for byte, and the wasm reads
them (§4.6). The `.fizzsnd` copies are the portable layer: they survive any store-format change.

### 4.3 How the sounds travel (shot 7)

A project references sounds in two places: its **pool** ('R' 1 records: pool slot → preset id) and its patterns'
**track sounds** ('P' 5: up to 6 preset ids a pattern, at fixed offsets; an edited track sound is a full copy and needs
nothing). Factory presets (ids 1..23) are compiled in.

**Unload** reads, besides the records, every MY preset any of them references (DUMP, 1 round trip each) into
`sounds/`, with its id and a hash of name + patch.

**Load** resolves each sound before any record is written:

| on the unit | what happens | shown |
| --- | --- | --- |
| the same id, the same name + patch | **linked** (nothing sent) | green |
| another preset with the same name + patch (another unit; re-imported) | **re-linked** to that id | green |
| the same id, a different sound (edited since the unload) | **choose** (Q4.3): the project's copy back as a new MY preset (default: the project sounds as it did), or the FM-1's current one | orange |
| not there | **new**: STORE6 (OWN) into the first free place: a new id | cyan |
| a factory id whose name or patch differs (another FIZZ version) | becomes **new** from the copy | cream |

Then the page **rewrites the ids inside the records** before sending them: 'R' 1's id is a u16 at offset 2; 'P' 5's six
track references are u16 at offset 12. Only those bytes change; the record's CRC is recomputed for the transfer
check. This needs the offsets written down and tested: **`docs/RECORDS.md`** (WEB-MANAGER-PLAN Phase 2's item), at least
for 'H', 'P' 5's first 24 bytes, 'R' 1 and 'X'.

MY PRESETS full (256): LOAD refuses up front with how many places are needed (shot 7 shows `147 → 151 / 256`).

**Unload's own presets** (shot 8): MY presets that *only* this project uses (USERS, Phase 1) can be removed from the unit
with it (FREE SLOT + ITS OWN PRESETS), since the browser keeps their copies; loading the project back brings them
again as new ones. Default: keep them (Q4.4).

### 4.4 Firmware commands (bulk channel, the update's framing `00 59 46`, as FZB*)

| cmd | name | body | reply | runs | flash E (2x) | RAM |
| --- | --- | --- | --- | --- | --- | --- |
| `FZRL` | list project p's records | p | the keys, lengths, record versions and a CRC-32 each (`qd_store_each`, the CRC computed from mapped flash), 24 a frame; the referenced preset ids (pool + track refs) | between frames (reads) | 450 (900) | 0 |
| `FZRS` | read project p as one stream | p, stream offset | `FZRD` offset + 256 B of the concatenated `[key u32, len u32, bytes]`: the device walks the records with a cursor (record index, offset); an offset that is not the cursor's restarts the walk (a retry) | between frames, while playing too | 400 (800) | 0 (the cursor, 8 B, in `qd_sx_work_t`) |
| `FZRW` | write project p (begin) | p, total bytes, record count, name | `FZOK` / `FZER` (BUSY playing; IN USE: p is the open project; NO ROOM: the store's free pages after a GC would not hold it); **then the FM-1 asks** (LOAD? SAVE / HOME, the owner's rule for web restores) | job, stopped only: deletes p's leftover records (a cut earlier), then pulls each record from the host (`plat_host_read`, 256 B, on the stack) through `qd_store_put_src`, checking its CRC-32 before the commit page (the `ok` hook); **the HDR record last**: a project exists only once its HDR does | 1,300 (2,600) | 0 (cursor in `qd_sx_work_t`, ≤ 600 B stack transient) |
| `FZRK` | one record's header during FZRW | key, len, crc | (pulled by the device) | | (in FZRW) | |
| the FM-1's question | "LOAD WINTER LP?" a cassette dropping into the 8 slots (shot 7's screen), SAVE lit, HOME cancels | | | a screen and two keys | 900 (1,500) E; shared with the whole-unit restore's question (WEB-MANAGER-PLAN Phase 2), which is then free | 0 (the arena) |

Unload uses Phase 1's PROJECT DELETE after the check. Afterwards PROJECTS refreshes its name and existence from the HDR
(`pname`, `pexists`, already re-read by `qd_proj_*`).

Total **~3.0 KB (5.8 KB)** flash, 0 RAM.

### 4.5 Times (7 ms a round trip, 256 B blocks)

| | small (the owner's, ~51 pages ≈ 12 KB) | medium (16 patterns ≈ 28 KB) | the largest the store takes (~600 pages ≈ 144 KB) |
| --- | --- | --- | --- |
| unload: FZRL + FZRS + DUMP of its presets (~24) + check + delete | 1 + 48 + 24 RT ≈ **0.5 s** | 1 + 110 + 30 ≈ **1 s** | 1 + 563 + 60 ≈ **4.5 s** |
| load: presets (new ones STORE6) + FZRW (pulls) + page programs (~1 ms a page) | ~5 new + 48 pulls ≈ **0.5 s** + the press | ≈ **1 s** | ≈ **5 s**; plus sector erases if the store must collect first (45..400 ms each, up to ~36): **up to ~15 s** |
| swap (unload k, load into k) | **~1 s** + the press | ~2 s | ~10..20 s |

The store's erases normally wait for 10 s of quiet (§7 rule 1). A web load is the unit stopped and waiting on purpose:
FZRW's job counts as quiet (stopped, nothing sounding) so its erases run at once; the page shows the store filling as
in BACKUP's sector strip.

### 4.6 The wasm as the record engine (0 flash)

The browser's FIZZ (`site/fizz.wasm`) is the same core with the same record readers and writers. The page can mount a
stored project's records in the wasm's store and use FIZZ's own code to: list its patterns (§3.2 offline), copy
patterns between stored projects (PATCOPY in the wasm), play it in the browser, and check that a `.fizzprj` loads
before sending it to the unit. Condition: **the record layouts agree** between the unit's build (pi32v2 clang) and the
wasm's (both 32-bit little-endian, but the records are "the host's struct layouts": padding may differ). The
`docs/RECORDS.md` layout test (`sizeof` / `offsetof` of `qd_tset_t`, `qd_lock_t`, the step wire, compared across the
host, the wasm and the device build) is the gate, and the site's wasm must be the unit's FIZZ version (today it is
older: HANDOFF item 5).

### 4.7 Risks

| risk | what protects |
| --- | --- |
| **power lost during a load** | records committed one at a time (the store's commit page); the HDR last, so a cut leaves no visible project, only stray records, deleted by the next FZRW into that slot (and listed by STATUS as stray pages: Q4.6 whether the mount should clean them) |
| power lost during an unload | nothing is deleted until the browser copy is **read, CRC-checked against FZRL's CRCs, and committed in IndexedDB** (the transaction's `complete`); then PROJECT DELETE |
| the browser loses its copy (IndexedDB evicted, site data cleared, another computer) | the page asks for persistent storage (`navigator.storage.persist()`), shows the library's size, and offers a `.fizzprj` download on every unload (Q4.5: always, or a setting) |
| the project changes while it is read | FZRS while playing only reads; the page notes STATUS's commit counter before and after and reads again if it moved (as BACKUP does), or flushes first (FLUSH, stopped) |
| **version mismatch** | the manifest carries the store magic, the projects' format (`QD_PRJ_FORMAT`), the track count and each record's version. Load is offered only when the unit's match (STATUS + IDENTITY); a newer record on an older FIZZ: refused ("update FIZZ"); an older one on a newer FIZZ: sent as it is (FIZZ reads every older version, 'P' 1..5, 'S' 1..5), its `.fizzsnd` copies as the fallback. A different store format (as 8 → 6 tracks): offered only through a converter in the wasm, later |
| the store is too full for the project | FZRW refuses up front (NO ROOM) with the pages needed; the page suggests an unload |
| MY PRESETS full | the page refuses before anything is sent, with the count |
| the unit used during a write | stopped only; PLAY pauses the job (BUSY), the panel's PROJECT LOAD of slot p is refused while it is written |
| a wrong record sent (a page bug) | the device checks each record's CRC and length, the record version byte, and the key's project; the readers already tolerate bad records (a pattern it cannot read keeps its default). The wasm check (§4.6) runs before sending |

---

## 5. Budget summary

| phase | firmware | flash E | flash 2x | main RAM | pool / `qd_app_t` |
| --- | --- | --- | --- | --- | --- |
| A. export + offline editor | none | 0 | 0 | 0 | 0 |
| B. live sound editor | PARAM, DUMP track, STORE6 AUDITION / OWN, SAVE-over as a job | 0.7 KB | 1.4 KB | **0** | 0 |
| C. patterns | PATTERNS, PATCOPY | 1.6 KB | 3.2 KB | **0** | 0 (arena during a step) |
| D. project library | FZRL, FZRS, FZRW / FZRK, the FM-1's LOAD? screen | 3.0 KB | 5.8 KB | **0** | ≤ 8 B in `qd_sx_work_t` (grows 20 → ~28 B) |
| **total** | | **5.3 KB** | **10.4 KB** | **0** | ≤ 8 B |

Against **~20.8 KB free (dual)**: fits even at 2x, leaving ~10 KB. Single core (~17 KB): fits at 2x, ~6.5 KB left.
Nothing here needs main RAM: every buffer is the frame's (stack, ≤ 600 B transient, within today's STORE handler's),
the drawing's arena between frames, or the existing 4 KiB reply ring.

Trims if flash runs short, cheapest loss first:
1. **PATTERNS (−0.6..1.2 KB):** the map from FZRS + the wasm (§3.2).
2. **The LOAD? screen shared** with the whole-unit restore's question (one drawing, two names).
3. **FZRL (−0.45..0.9 KB):** the CRCs from the stream itself (FZRS carries each record's CRC in its `[key, len]`
   header), the referenced ids from the records in the browser.
4. PATCOPY in the wasm only (−1..2 KB): copies only between stored projects or via load / unload.

---

## 6. Phases, by value / cost

1. **P2-A: export + the editor offline** (site only, 0 firmware). EXPORT dialog with the carry bar and notes (shot 4),
   `.dn2snd`, the editor's drawings and editing against the browser's FIZZ, SAVE via STORE6 to the unit (exists).
   High value (the bass shared today), lowest cost. Needs the site's wasm on the current core (HANDOFF item 5).
2. **P2-B: the live editor** (~0.7..1.4 KB). PARAM, track DUMP, AUDITION / OWN, SAVE-over as a job; the page's
   audition-track chooser, REVERT, the 2 Hz read-back. `test_sysex` cases (PARAM clamps, BAD TRACK, AUDITION stores
   nothing, SAVE-over job's records equal `qd_preset_will_change`'s), `test_manager.mjs` against the wasm.
3. **P2-C: projects tab + pattern copy** (~1.6..3.2 KB). The PROJECTS tab (switching exists), PATTERNS, PATCOPY with
   COPY / MOVE (shot 6). `test_persist`: PATCOPY across projects equals a pattern copied by hand and renumbered; a cut
   at every write leaves both projects loadable.
4. **P2-D: the project library** (~3.0..5.8 KB). `docs/RECORDS.md` + the layout test first (unit / wasm / host), then
   FZRL / FZRS / FZRW, the LOAD? screen (mock-up first: shot 7's drawing at 240 x 240 for approval), IndexedDB store,
   `.fizzprj`, the sound re-linking (shot 7), unload with its check (shot 8), swap. Tests: export → IndexedDB → load into
   another slot → `flash_dump` listings equal; the id remap against a unit with different ids (two simulated units);
   a cut after every record leaves the store mountable and no half project visible.
5. **P2-E (later): the wasm as record engine** (0 firmware): stored projects' patterns, PATCOPY between stored projects,
   play a stored project in the browser.

Each phase: the emscripten web test (`build_web.sh` + `test_web_emu.mjs`) and `site/tools/test_manager*.mjs` before a
release; on the unit (main session only): the commands once by hand stopped and playing, `fizz_xdata.py stats` after
(0 xruns, `lock_sysex_us` < 1 ms).

---

## 7. Open questions for the owner

**Sound editor**
- Q1.1 The edit plays on a **track of the open project** (the track's sound changes like a knob turn; REVERT on leaving
  without SAVE). OK, or should it play on a **hidden preview voice** that never touches the project (~600 B flash, a
  220-byte patch in the pool, 0 main RAM)?
- Q1.2 Which track by default: the unit's selected track, or always the same one (e.g. track 1)?
- Q1.3 SAVE over a preset that other projects use: the tracks that loaded it keep their copies (today's rule) and the
  pool references play the new sound. Same from the web?
- Q1.4 A keyboard in the page (an on-screen strip / the computer keys), or only the FM-1's keys?

**Export**
- Q2.1 The Digitone files stay marked EXPERIMENTAL until a Digitone and a Digitone II owner load one (`DN2-FORMAT.md`
  "The next proof"). Can you find someone, or should they ship as they are?
- Q2.2 Several presets exported at once: one ZIP of files (as today), fine?

**Projects and patterns**
- Q3.1 Pattern MOVE between projects can't be undone on the unit (only COPY can, when the target is open). OK with
  the page's own UNDO while it stays open?
- Q3.2 A copied pattern's MIDI FX groups: keep the numbers (the target project's groups play), or copy the source's
  groups into the target when its groups are empty?
- Q3.3 Copy into a used pattern: overwrite after one confirmation, or only into empty ones?

**Project library**
- Q4.1 Does loading a stored project into an **empty** slot need the press on the FM-1 too, or only when it replaces
  something (your rule: web restores need SAVE on the unit)?
- Q4.2 Swap with one drag (unload the slot's project, keep it, load the other) after the one press, OK?
- Q4.3 A sound edited on the FM-1 since the project was unloaded: default to bringing the project's copy back as a new
  MY preset (the project sounds as saved), or use the FM-1's current sound?
- Q4.4 Unload: also remove the MY presets only that project uses (freeing library places), or keep them (default)?
- Q4.5 Download a `.fizzprj` automatically on every unload (a second copy outside the browser), or only on request?
- Q4.6 Should the unit clean stray records of a half-written project at power-on (~200 B flash), or leave it to the
  next load into that slot?
- Q4.7 How many projects do you expect to keep in the browser? (Each ~10..150 KB; IndexedDB holds hundreds.)

---

## 8. Not here

- **The player as the unit's big screen** (screen mirroring or a state-synced wasm): `docs/WEB-MIRROR-PLAN.md`.
- Digitone `.syx` and Digitone project import: WEB-MANAGER-PLAN Phase 3 (the owner skipped `.syx` for now).
- Carrying 8-track projects into the 6-track store: decided against (WEB-MANAGER-PLAN owner decisions).
