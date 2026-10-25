# FIZZ SysEx (protocol 1)

FIZZ (from 0.6.4-p1.5r) takes sounds over MIDI System Exclusive and stores them in its **LIBRARY** (from 0.7.0-p1.5t:
**MY PRESETS**, the user's part of the Preset Library, 256 places; each import is marked as imported and gets its
kind) or the active
project's **POOL**. It can also put a sound on a track, together with the track's pan and sends. A page or tool can
ask FIZZ who it is. The use this was built for is the FIZZ web preview loading a user's Digitone `.dnsnd` presets: the
page converts each preset in the browser with ChoralRoot's reader, then hands the result to FIZZ through the
engine's `midiIn`.

- Code: `core/app/fizz_sx.h` / `fizz_sx.c` (the app) and `core/glue/core_quadrille.c` (the transport).
- Host-side helper: `tools/fizz_sysex.mjs` (pure JS, no DOM).
- Tests: `core/tests/test_sysex.c`, `tools/test_sysex_dnsnd.mjs`, and the SysEx check in `tools/test_abi.c`.

## Frames

```
F0  7D  46 5A  01  <cmd>  <body ...>  <chk>  F7
    |   |      |
    |   |      protocol version (1)
    |   "FZ"
    non-commercial manufacturer ID
```

- Every byte between `F0` and `F7` is 7-bit (0x00..0x7F).
- **Checksum.** `chk` makes the 7-bit sum of `cmd`, the body and `chk` zero: `chk = (0x80 - (sum of cmd and body & 0x7F)) & 0x7F`. The header bytes `7D 46 5A 01` are not in the sum.
- **The patch's 7-bit packing.** The 96 bytes are packed in groups of 7, each group as 8 bytes:
  - The first byte of a group holds the top bits: bit k is byte k's bit 7.
  - The next seven bytes are the low 7 bits of each byte, in order.
  - The last group has 5 bytes, packed as 6.
  - 96 bytes pack to **110**.
- **Unknown frames.** FIZZ never holds a frame that does not start `7D 46 5A`, so other SysEx, notes and the clock pass by untouched. A frame with a different protocol byte gets the error reply `7F 06 <protocol>`.

### Over the ABI's `midi_in` (USB-MIDI event packets)

The core's `midi_in` takes 32-bit USB-MIDI packets on cable 0: `CIN | b0 << 8 | b1 << 16 | b2 << 24`.

| CIN | packet holds |
| --- | --- |
| 4 | three bytes, and the frame goes on |
| 5 | the last 1 byte (the `F7`) |
| 6 | the last 2 bytes (ending at `F7`) |
| 7 | the last 3 bytes (ending at `F7`) |

`usbPackets(frame)` in `tools/fizz_sysex.mjs` makes them. The identity request, for example, is three packets:

```
0x467DF004   0x01015A04   0x00F77F06
```

- **"Retry later".** While one whole frame waits, `midi_in` returns **0** for the next frame's first packet; the host offers that packet again later. FIZZ handles a frame as soon as its `F7` arrives, so in practice this does not happen.
- **Replies.** They come out of `midi_out_take` as the same kind of packets.
- **Frame size.** A frame may be up to 640 bytes; FIZZ's STORE frame is 140, or 141 with the tags byte (P1.5x).

## Commands

### `01` IDENTITY

Request: `F0 7D 46 5A 01 01 7F F7`

Reply: `F0 7D 46 5A 01 41 01 <ASCII> <chk> F7`

- `01` is the protocol version.
- `<ASCII>` is the version string, for example `FIZZ 0.6.4-p1.5r`. It is up to about 40 characters, with no terminator: it runs up to the checksum.

### `10` STORE SOUND

The body is 132 bytes, or 133 with the optional **tags** byte (P1.5x), then `chk`. A 132-byte body is read as before.

| offset | bytes | field |
| --- | --- | --- |
| 0 | 1 | **flags**: `01` POOL (else the LIBRARY), `02` BATCH, `04` LOAD |
| 1 | 2 | **slot**, 14-bit: high 7 bits, then low 7 bits. LIBRARY (MY presets) 1..256, POOL 1..128. **0 = the first free slot.** A slot given is overwritten. (0.6.x: LIBRARY 1..128, POOL 2..128.) |
| 3 | 12 | **name**, ASCII 0x20..0x7E, padded with spaces (or 0). Trailing spaces are dropped, other bytes become `?`, and an empty name becomes `IMPORTED`. |
| 15 | 1 | **drive**: FIZZ's DRIVE, 0..127. This is the `drive` of ChoralRoot's `dnsndToQuad`. |
| 16 | 1 | **track** 0..5, with LOAD (6 tracks since 0.12; 6 or 7: BAD TRACK, nothing stored); otherwise 0 |
| 17 | 1 | **pan** + 64 (0..127, 64 = centre), with mixmask `01` |
| 18 | 1 | **chorus** send 0..127, with mixmask `02` |
| 19 | 1 | **delay** send 0..127, with mixmask `04` |
| 20 | 1 | **reverb** send 0..127, with mixmask `08` |
| 21 | 1 | **mixmask**: which of pan, chorus, delay, reverb the frame gives. P1.5x: **the preset keeps them, LOAD or not** (the rest: none, pan centre); with LOAD the track takes them too |
| 22 | 110 | **patch**: the 96-byte FM TONE blob, 7-bit packed |
| 132 | 1 | **tags** (optional, P1.5x): FIZZ's tags as a 7-bit mask, bit 0 BASS, 1 KEYS, 2 PLUCK, 3 PAD, 4 LEAD, 5 DRUMS, 6 FX. Nonzero: the preset's tags. Absent or 0: FIZZ's classifier decides from the sound, as before. A page maps Digitone / Digitone II tags onto these (`tools/dn2pst.mjs` `fizzTags`: BASS, CHORD → KEYS, PAD, LEAD, KICK / SNARE / PERCUSSION / HI-HAT / CYMBAL → DRUMS, SOUND FX → FX) |

**The patch** is ChoralRoot's FM TONE blob: `'Q'` (0x51), version 5, the values (`eng_quad.c` `quad_pack`), then zeros.

- `dnsndToQuad(parsed).blob` or a ChoralRoot sound file's `patch` is exactly this.
- An older version in 96 bytes is read the way ChoralRoot's `quad_unpack` reads it. Send version 5 (what `quadToV4` / `quadToV5` make).
- Byte 87, where FIZZ's own packing keeps DRIVE, is ignored. The **drive** field sets DRIVE.

### `11` STORE6 (E1, 0.8.0)

STORE SOUND with FIZZ's **version-6 blob**: the same body, the patch field 147 bytes (the 128-byte blob, 7-bit packed:
18 groups of 7, the last 2 bytes a group of their own). The body is 169 bytes, or 170 with the tags byte (at 169). The
reply is STORE's (`50`), with the same statuses.

**The version-6 blob** (FIZZ's `qd_quad_pack`): `'Q'`, 6, version 5's values as above (bytes 2..86), DRIVE at 87 (the
**drive** field overrides it, as in STORE), then FIZZ's values 74..109 one byte each (value − its minimum) at 88..123,
then zeros to 128. A value out of its range, or a nonzero byte after 123, makes it BAD PATCH. The values:

| id | value | range (default) |
| --- | --- | --- |
| 74 | AMP MODE | 0 ADSR, 1 AHD (0) |
| 75 | HOLD (AHD) | 0..126, 127 NOTE (127) |
| 76 | PORT | 0 OFF, 1 ON, 2 LEGATO (0) |
| 77 | PORT TIME | 0..127 (32) |
| 78 | PORT SLOPE | 0 TIME, 1 RATE (0) |
| 79 | DRIVE ROUTE | 0 POST (after the voice), 1 PRE (before the filter) (0) |
| 80 | FSHAPE (E2): MULTI's TYPE morph, the combs' LPF, the EQ's Q | 0..127 (0) |
| 81 | FGAIN (E2): the EQ's gain | −64..63 (0) |
| 82 | BWPOST (E2): the base-width 0 before the filter, 1 after | (0) |
| 83 | BITS (E3): 0 OFF, 1..127 16 bits down to 1 | (0) |
| 84 | SRR (E3): 0 OFF, 1..127 the rate divided by 64^(SRR / 127) | (0) |
| 85 | CRPOST (E3): CRUSH 0 before the filter, 1 after | (1) |
| 86..109 | E4's eight modulation slots: SOURCE 0..8, DEST 0..63, DEPTH −127..127 | (0) |

Every value at its default plays exactly what a version-5 blob plays. E2: FTYPE (value 36) is the **filter machine**,
0..7: 0 OFF, 1 LP12, 2 HP12 (LEGACY), 3 LP24 (LOWPASS 4), 4 MULTI, 5 COMB+, 6 COMB−, 7 EQ (a version-5 blob may carry
4..7 too). `tools/fizz_sysex.mjs` `storeSound` sends a
128-byte blob as STORE6 by itself; `tools/dn2pst.mjs` `dn2pstToQuad` gives `blob6` (and `six`: it needs it). A STORE
whose 96-byte blob says version 6 is BAD PATCH.

Reply: `F0 7D 46 5A 01 50 <status> <0 LIBRARY / 1 POOL> <slot hi> <slot lo> <chk> F7`

- `<slot>` is where the sound went (1-based, as the panel numbers it), or 0 on an error.

| status | name | meaning |
| --- | --- | --- |
| 0 | OK | stored |
| 1 | FULL | slot 0 asked for, and no slot is free |
| 2 | BAD LENGTH | the body is not 132 (or 133) bytes, or the patch does not unpack to 96 |
| 3 | BAD CHECKSUM | the sum is not zero (nothing is read) |
| 4 | BAD PATCH | not an FM TONE blob (FIZZ's `qd_quad_unpack` refuses it) |
| 5 | BAD SLOT | LIBRARY > 256, POOL > 128 |
| 6 | BAD VERSION | (error reply `7F 06 <protocol>`) |
| 7 | BAD COMMAND | (error reply `7F 07 <cmd>`) |

Any other command, or a frame whose checksum fails before its command is known, gets `F0 7D 46 5A 01 7F <status> <cmd> <chk> F7`. A STORE whose checksum fails gets a STORE reply with status 3.

### `20` LABELS (P1.6l, 0.8.0-p1.6l)

What every control does right now, for a page that draws the panel (the owner: "hold Shift and it should update
with labels of available shortcuts"). Request: `F0 7D 46 5A 01 20 <flags> <chk> F7`; flags bit 0 **subscribe**.

Reply: `F0 7D 46 5A 01 60 <count hi> <count lo> <n = 49> { <len> <len ASCII bytes> } x 49 <chk> F7`

- **count**: the label set's change counter (14 bits, two 7-bit bytes); it goes up by one whenever the set differs
  from the last one sent.
- **The 49 labels, in this order**: the knobs SELECT, ALGORITHM, PRESETS, KNOB 1..4, MASTER (8); the 14 buttons as
  the FM-1's panel prints them: FX, TRACKS, MIX, LATCH, EDIT, SHIFT, HOME, SOUNDS, FILL, PATTERNS, PLAY, REC, F1, F2;
  the white keys W1..W16; the black keys B1..B11. Each is 0..10 characters, upper case, printable ASCII; **empty: the
  control has no job here** (SHIFT held: the white keys and B2, B10; the typewriter: PLAY and most buttons; asleep:
  everything but HOME WAKE, PLAY and MASTER).
- **They follow FIZZ's own state** (`core/app/view_labels.c`, from the same tables the controls use): the step view
  (STEP 1..16, the hold modes NOTE .. P-LOCK, the knobs' jobs), a step held (the mode's values: 13 .. 127, 1/4 ..
  INF; RESET), SHIFT held (SNAP SAVE, RECALL, COPY PTN, PASTE, TAP, CLICK, FOLLOW, DOUBLE, CLEAR TRK; SELECT TRACK,
  ALGORITHM SCALE, PRESETS ROOT; F1 UNDO, F2 REDO, HOME VIEW, REC REC SET, TRACKS SETTINGS), the sound editor (its
  groups, the page's value names, DICE, NEW SOUND), the browser (ALL, BASS .. FX, the next source, +POOL / -POOL,
  DELETE, LOAD, CANCEL), the typewriter (the letters, or SHIFT's characters; CURSOR, LETTER, NEW NAME, DELETE /
  CLEAR), SEQ held (the patterns, the banks), SEL held (MUTE / SOLO / TRACK), F1 / F2 / F3, the settings menus, the
  mix, the effects (their cells' names), the keyboard (the notes), P1.6c's MIDI FX screen (the slots' effects on
  B1..B8, BYPASS / PLAY, REMOVE; ALGORITHM GROUP, PRESETS EFFECT, the page's value names; the notes, or LANE 1..16 on
  the arp's lanes; P1.6d: F1 CLOSE, held COPY then PASTE, F2 PICK UP / DROP; P1.6e: REC STEPS / KEYS, on STEPS the
  white keys STEP n and ALGORITHM STEP MFX while steps are held; P1.6f: REC is labelled with what a tap switches to
  everywhere, KEYS in the step view, STEPS in the keyboard; P1.6g: REC OFF while armed or recording, in every view;
  FILL's button reads MIDI FX in the core's descriptor too) and FILL's button (P1.6d: MIDI FX, its
  tap the screen and its hold the arp on / off; SHIFT held FILL; CLOSE in the MIDI FX screen). P1.6d's new contexts:
  the record settings' page 2 (KNOB 1 AUTOMATION, SELECT PAGE) and the track settings' page 3 (KNOB 1 KIND, KNOB 2
  NAME).
- **Subscribed**, FIZZ sends a LABELS reply by itself whenever the set changes, looking at most every 50 ms (20 a
  second); nothing while it stays the same. A request without the flag answers once and ends the subscription.

`tools/fizz_sysex.mjs`: `labelsRequest({subscribe})`, `parseLabels(bytes)` → `{count, list, knobs, buttons, white,
black}`; `parseReply` gives `{kind: "labels", ..}`.

### `21` LIST, `22` DUMP (P1.6m, 0.8.0-p1.6m): presets out

For a page that exports FIZZ's presets (a backup, a library to load back later).

**LIST**: `F0 7D 46 5A 01 21 <flags> <chk> F7`; flags bit 0 the factory presets too, bit 1 the project's pool too.
Replies `61`, one part after another (16 entries a part at most):
`F0 .. 61 <part> <last: 1 the end> <total hi> <total lo> <n> { entry } x n <chk> F7`, an entry 20 bytes:

| bytes | |
| --- | --- |
| 1 | source: 0 MY PRESETS, 1 factory, 2 the pool |
| 2 | slot, 7 bits each (1-based: MY PRESETS 1..256, factory 1..23, pool 1..128) |
| 3 | the preset's id, 7 bits each (a pool slot: the preset it references) |
| 12 | the name, ASCII, spaces after |
| 1 | tags (FIZZ's, bit 0 BASS .. 6 FX) |
| 1 | bit 0 imported (not saved on FIZZ), bit 1 in this project's pool |

**DUMP**: `F0 7D 46 5A 01 22 <source> <slot hi> <slot lo> <chk> F7`. Slot 0: every one of that source, one after another.
Each reply **is a STORE (`10`) or STORE6 (`11`, when the sound uses a value 74..) frame** with everything to make the
preset again: its slot (a factory preset: 0, the first free), name, DRIVE, pan, the three sends (mixmask 0F), its
patch, its tags byte; a pool slot's has the POOL flag (the preset it references, as a pool store makes it). Its flags
carry **DUMP (`08`)**: FIZZ ignores a STORE with DUMP set (no store, no reply), so an echo is harmless. **To restore,
send it back with DUMP cleared and the checksum redone** (`restoreFrame`; another slot by changing bytes 7..8, 0 the
first free): the same name, patch, tags, pan and sends. An empty place: `7F 05 22` (BAD SLOT).

**Pacing**: a long LIST or a DUMP of everything goes out as there is room: FIZZ makes the next reply when its reply
ring (4 KiB) has room for it, and its glue moves replies into the ABI's packet ring (12 KiB) as the host reads, so a
256-preset export never overflows; the page reads the replies in order (test_abi reads 40 at four packets a ms).

`tools/fizz_sysex.mjs`: `listRequest({factory, pool})`, `parseList(bytes)` → `{part, last, total, entries}`,
`dumpRequest({source: "mine" | "factory" | "pool", slot})`, `parseDump(bytes)` → `{source, slot, name, drive, pan,
chorus, delay, reverb, tags, blob, six, frame}`, `restoreFrame(bytes, {slot})`; `parseReply` gives `{kind: "list"}`
and `{kind: "dump"}`.

### The web manager's commands (0.12.x, docs/WEB-MANAGER-PLAN.md §5)

The library, the pool and the projects edited from a page (`site/manager/`). Protocol 1 still: an older FIZZ answers
each of them `7F 07 <cmd>` (BAD COMMAND), which a page reads as "update FIZZ". Code: `core/app/fizz_sx.c` (`sx_edit`,
`qd_sx_work`), tests: `core/tests/test_sysex.c` (`phase1_tests`), `firmware/app/tests/web.txt`,
`site/tools/test_manager.mjs`.

**New statuses**: `8` BUSY (playing, for the commands that need it stopped, or another job running), `9` NOT FOUND
(nothing there), `10` IN USE (DELETE without CONFIRM of something used), `11` BAD TRACK (STORE's LOAD onto track 7 or
8: FIZZ has 6; nothing stored).

**Slots** are 1-based, two 7-bit bytes (high, low), as in STORE. **Sources**: `0` MY PRESETS (1..256), `1` factory
(1..23), `2` the pool (1..128), `3` a project (1..8, RENAME only).

**ACK** (most commands' reply): `F0 7D 46 5A 01 52 <cmd> <status> <a hi> <a lo> <b hi> <b lo> <chk> F7`; a and b are
the command's slots (or, for IN USE, the projects and the steps using it).

**Where they run.** The quick ones in the SysEx handler (on the unit under the port's audio lock: a directory entry, a
sound cache entry, a record queued for the autosave; nothing read in, nothing written to flash: `test_sysex` checks
that no handler pages a pattern or commits a record). The others leave a **job** (`qd_sx_t.wk`, 20 bytes in
`qd_app_t`) that the store's step runs (`qd_persist_step`: on the unit every 15 ms between frames, outside the locks;
on the host and in the wasm from `qd_persist_ms`): a pattern or a project's records a step, never inside the handler.
A job that needs FIZZ stopped waits while PLAY is on. Its reply comes when it is done (a pool swap: ~0.3 s).

| cmd | name | body | reply | runs |
| --- | --- | --- | --- | --- |
| `02` | STATUS | — | `42` (below) | handler |
| `03` | FLUSH | — | `42` STATUS once everything is written | job, stopped only |
| `12` | TAGS | source, slot, tags (FIZZ's 7 bits) | ACK (a = slot, b = tags) | handler (a factory preset's tags are the owner's, as on the panel) |
| `13` | RENAME | source (0 or 3), slot, name[12] | ACK | handler; another project's header: a job |
| `14` | DELETE | source (0 or 2), slot, flags (`01` CONFIRM) | ACK; IN USE (a = projects, b = steps) | job, stopped only |
| `15` | SWAP | source (0 or 2), a, b | ACK | MY PRESETS: handler (the ids go with the presets: no reference changes); the pool: job, stopped only |
| `16` | POOL REF | pool slot (0: the first free), preset id (3 x 7 bits) | ACK (a = the pool slot, b = the id) | handler; id 0: the slot cleared (DELETE's job, CONFIRM) |
| `17` | USERS | source (0 or 2), slot | `57` (below) | job, while playing too |
| `23` | PROJECTS | — | `63` (below) | handler |
| `24` | PROJECT | op (`1` LOAD, `2` NEW, `3` COPY p onto q, `4` DELETE), p, q (1..8) | ACK (a = p, b = q) | job, stopped only |

- **STATUS** (`42`): protocol, tracks (6), flags (`01` playing, `02` no store attached: safe mode on the unit, `04`
  something not written yet, `08` a job running, `10` the project worked on started with the demos), the project
  worked on (0..7), its pattern, the projects there, MY PRESETS used (2), the pool's sounds (2), records committed (3
  bytes: it changes when the store does, so a page can tell whether a backup it read is still current), the library
  store's pages used / all (2 + 2), the projects' (2 + 2), the job's command (0: none).
- **USERS** (`57`): source, slot (2), how many projects, each `<project 0..7> <steps hi> <steps lo>`, then this
  project's tracks playing or locking it (bit t). A MY preset: the projects whose pools reference it (this one's
  from RAM, the others' from their records, a project a step); a pool sound: this project.
- **PROJECTS** (`63`): the one worked on (0..7), 8, then 8 entries of `<flags: 01 there, 02 worked on, 04 the demos>
  <name[12]> <store pages hi> <lo>`.
- **DELETE** without CONFIRM of something used answers IN USE and changes nothing; the page asks, then sends CONFIRM.
  With CONFIRM (or unused): a MY preset: every other project's records that load it keep their own copy (a project a
  step), then `qd_preset_delete` (its pool references go, their steps back to their track's sound: the panel's
  fallback); a pool sound: `qd_pool_delete` (the same for its steps, patterns out of RAM swept a frame at a time). One
  undo level.
- **SWAP** of the pool swaps the two slots and every step lock and track source of the two numbers in all 64 patterns
  (those out of RAM through their records: read into the drawing's arena, renumbered, written again if they changed).
  The undo levels before it are forgotten (their bytes would no longer match). Into an empty place: a move. A power cut
  in the middle leaves the store mountable (each record is committed whole) with the patterns not yet renumbered
  still pointing at the old numbers (`test_sysex` checks the mount).
- **Undo**: TAGS, RENAME, SWAP of MY PRESETS, POOL REF and DELETE are undone by SHIFT + F1 (commands less than half a
  second apart join one level, as a gesture's edits do). PROJECT loads forget the levels, as on the panel.

`tools/fizz_sysex.mjs` (and `site/js/fizz_sysex.js`): `statusRequest`, `flushRequest`, `tagsRequest`,
`renameRequest`, `deleteRequest`, `swapRequest`, `poolRefRequest`, `usersRequest`, `projectsRequest`,
`projectRequest`; `parseStatus`, `parseAck`, `parseUsers`, `parseProjects`; `parseReply` gives `{kind: "status" |
"ack" | "users" | "projects"}`. `site/manager/fizzlink.js` wraps them (`link.status()`, `link.swap(..)` ..).

### The sound editor's commands (Manager P2-B, docs/MANAGER-P2-PLAN.md §1.3)

The web manager's EDIT edits the sound on a track of the open project live, and LIBRARY loads a preset onto a track
with a CANCEL, as the unit's sound browser does. An older FIZZ answers PARAM `7F 07 30` (BAD COMMAND) and a track's
DUMP `7F 05 22` (BAD SLOT): the page then edits in the browser only.

| cmd | name | body | reply | runs |
| --- | --- | --- | --- | --- |
| `30` | PARAM | track (0..5; `7F` the selected one), id (0..109), value + 2^20 (3 x 7 bits, high first), flags (`01` ACK) | none; with ACK, or on an error: ACK (a = the track + 1, b = the id): BAD TRACK, NOT FOUND (id 110..), BAD LENGTH | handler: `qd_player_live_edit`, exactly a knob turned on the unit (the value clamped to its range; the sounding voices follow; the track's sound marked edited, the pattern's copy too). No undo level. While playing too |
| `22` | DUMP, source `4`: a track | slot 1..6 (0: the selected track) | a STORE / STORE6 frame with DUMP and LOAD, its track, the track's sound's name, DRIVE, patch, the track's pan and sends (mixmask 0F), the tags of the preset it came from, then 8 bytes of **extras**: the preset id (3 x 7 bits), the pool sound (2 x 7), flags (`01` edited, `02` the selected track, `04` recording or armed, `08` playing), the track's MIDI channel (0..15), the selected track (0..5) | handler |
| `10` / `11` | STORE flag `10` **AUDITION** (with LOAD) | as STORE, optionally followed (after the tags byte) by the 8 extras | STORE's reply, slot 0 | handler: the sound onto the track only (and the pattern's copy), its mix by mixmask; nothing stored, no undo level, no cassette. With the extras the track's preset id, pool sound and edited mark are set as given: **a track's DUMP sent back with DUMP cleared and AUDITION set puts the track back exactly** (a page's CANCEL; `trackFrame` in fizz_sysex.js). The extras without AUDITION + LOAD: BAD LENGTH |
| `10` / `11` | STORE flag `20` **OWN** | as STORE | as STORE | the preset is the owner's: not marked imported (the editor's SAVE) |
| `10` / `11` | STORE over a used MY PRESETS place | as STORE (a slot given, used) | STORE's reply **when done**; BUSY while playing or another job runs | a **job**, stopped only (paused if PLAY comes): each project's pattern records that load the preset keep their own copy of the sound it was (`qd_preset_will_change_proj`, a project a step, the open one last), then the preset is stored (its id kept). Nothing is read or written inside the handler (before 0.13 this ran in the handler, under the audio lock) |

STATUS (`42`) has two more things: flag `20` recording (or armed: notes a page sends are recorded), and a byte after the
job's command: the selected track (0..5).

`tools/fizz_sysex.mjs` (and `site/js/fizz_sysex.js`): `paramRequest({track, id, value, ack})`, `dumpRequest({source:
"track", slot})` (parseDump gives `track` and `extras`), `storeSound({.., audition, own, extras})`, `trackFrame(dump)`;
parseStatus gives `rec` and `sel`. `site/manager/fizzlink.js`: `link.dumpTrack(slot)`, `link.param(track, id, value)`,
`link.note(ch, n, vel)`.

## What FIZZ does with a sound

- **Stores it** in the LIBRARY slot or POOL number, with its name. A LIBRARY import becomes one of MY presets: its own
  preset id, its first tag read from the sound (BASS, KEYS, PLUCK, PAD, LEAD, DRUMS or FX; the owner can add more in
  the browser, SHIFT + B2..B8), marked imported (the ↓ in the browser).
  - A POOL import (P1.5w: the pool holds references to presets): the sound becomes one of MY presets too (imported,
    in the first free place), and the POOL number references it. The reply keeps its format: the POOL flag and the
    POOL number. With MY presets full, a POOL store replies FULL too.
  - A POOL number stored over: it references the new preset; steps that lock that number play it. Tracks keep their
    own copies.
- **Shows it** for about a second, over any screen including the EDIT view: the cassette drops in with the name and slot number, in the LIBRARY's cream or the POOL's green, with "MY PRESETS" / "POOL" above and **imported** below.
- **Saves it** through FIZZ's usual autosave. The library and the project are written by themselves a moment later, as for any edit (`core/app/persist.c`).
- **Undo.** SHIFT + F1 undoes **one message**, or **one batch**.
  - A batch is the messages after the first, sent with BATCH (`02`) and arriving less than half a second apart. Each joins the first message's level.
  - A track's pan and sends set with LOAD are the project's track settings, which undo does not cover: undo takes the sound back, not the mix.
- **Keeps its mix** (P1.5x): a preset carries its pan and its chorus / delay / reverb sends (the fields mixmask gives). Loading it later from the browser (PRESETS, F2) sets the track's pan and sends to them; a step's sound lock does not.
- **LOAD** (`04`, track 0..5) also puts the sound on that track in the pattern playing, the same as choosing it on the sounds page.
  - A POOL import becomes the track's source (its number); a LIBRARY import is loaded as a copy.
  - mixmask picks which of pan / chorus / delay / reverb the track also takes. The rest stay as they are.

## From a Digitone `.dnsnd` (the web preview)

```js
import { parseDnsnd, dnsndToQuad } from "<ChoralRoot>/web/fm1dnsnd.js";
import { soundFromDnsnd } from "<ChoralRoot>/web/fm1sounds.js";
import { storeSound, identity, parseReply, usbPackets } from "<FIZZ>/tools/fizz_sysex.mjs";

const parsed = await parseDnsnd(bytes);
const q = dnsndToQuad(parsed);          // {blob, values, pan, drive, notes}
const name = soundFromDnsnd(parsed).name;   // the reader's 12-letter name
const frame = storeSound({
  name, blob: q.blob, drive: q.drive,
  slot: 0,                              // the first free
  pool: false,                          // the LIBRARY
  batch: i > 0,                         // a batch: one undo
  // P1.5x: the preset keeps its pan and sends (the Digitone's: parsed.params["send.reverb"] ..), and its tags:
  pan: q.pan, delay: parsed.params["send.delay"], reverb: parsed.params["send.reverb"],
  chorus: parsed.params["send.chorus"], tags: ["BASS"],   // (FIZZ's names, or a mask; none: FIZZ's classifier)
  // optional, onto track 3 as well (LOAD): track: 2,
});
for (const p of usbPackets(frame)) engine.midiIn(p);
// then read engine.midiOutTake() packets back into bytes: parseReply(bytes) -> {kind: "store", status: 0, slot}
```

`tools/test_sysex_dnsnd.mjs` runs this for the owner's presets, which are never copied into this tree. It feeds each
frame to FIZZ and checks three things: the reply is OK, the stored name is the reader's, and all 73 stored FM TONE
values plus DRIVE equal `dnsndToQuad`'s values (P1.5x: and its pan and sends are the preset's). All 256 presets in the folder pass.
`tools/test_fizz_sysex.mjs` checks the frames' P1.5x fields (the mix without LOAD, the tags byte) as FIZZ reads them.

## From a Digitone II `.dn2pst`

`tools/dn2pst.mjs` (`parseDn2pst`, `dn2pstToQuad`: research/DN2-FORMAT.md) gives `{blob, name, drive, pan, sends: {chorus,
delay, reverb}}` and the parsed `fizzTags`; send them as above (`tags: parsed.fizzTags`). E1: it also gives `blob6`
and `six`; when `six` is true send `blob6` (STORE6): the sound uses AHD (AMP MODE 0, HOLD 1:1), portamento (PORT ON
from +226 = 64, TIME 1:1 from +228) or DRIVE before the filter (OD ROUTING +240 = 0, read as PRE: a guess, not yet
verified on a unit). E2: the filter machines too (MULTI, COMB+, COMB−, EQ: FTYPE 4..7; their TYPE / LPF / Q as
FSHAPE, the EQ's GAIN as FGAIN) and BW ROUTING (+238) as BWPOST. FM1VST's drop reads `.dn2pst`
with a C++ port of it (FM1VST `plugin/Dnsnd.cpp` `parseDn2`, checked byte for byte against the JS).

### ChoralRoot's sound format compared with FIZZ's patch

- **The 73 FM TONE values: same order.** FIZZ's patch values 0..72 are ChoralRoot's `eng_quad.c` `QP_*` values, in the same order and the same units. The blob is ChoralRoot's own packing, so nothing is renumbered.
- **DRIVE.** FIZZ's value 73 is **DRIVE**. ChoralRoot keeps the drive in the sound *record* (`SND_P_DIST`, stored as drive + 64), not in the blob; its blobs have 0 in byte 87. So the message carries drive in its own field.
- **Pan.** Pan is in ChoralRoot's record (`SND_P_PAN`, pan + 64). In FIZZ it is a track setting; P1.5x: a preset keeps it (and the sends) and a track takes them when it loads the preset.
- **Macro bytes.** The record's macro bytes (`SND_P_E0..`, for example HARM and DTUNE coarse) are ChoralRoot's performance macros. FIZZ does not use them, because the blob has the exact values.
- **Sends.** The Digitone's reverb, delay and chorus sends are dropped by ChoralRoot's converter ("effects are global on the FM-1"). FIZZ has them per track: pass them with LOAD (they are 0..127 in both).

## Limits

- A frame waits for its `F7`. A partial frame followed by another status byte is dropped.
- Replies wait in a 4 KiB ring inside FIZZ (P1.6l; was 4 replies), then in the ABI's packet ring (12 KiB); a host
  reads them with `midiOutTake` (the web preview's engine every render). FIZZ moves them on each tick as there is
  room, so a long run of replies goes out as the host reads.
- The identity request does not use the universal Identity Request (`F0 7E 7F 06 01 F7`), so that FIZZ does not claim a manufacturer ID.
