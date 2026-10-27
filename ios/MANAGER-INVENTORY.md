# FIZZ Manager: feature inventory for a native iOS port

Repo: `/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/FMToneSeq`. Paths below are relative to it. All line refs are as of 2026-10-27.
Legend: **WRITE** = changes the unit (store, track or open project) · **READ** = asks the unit, changes nothing · **LOCAL** = browser only.
"FizzLink" = `site/manager/fizzlink.js` class `FizzLink`. Semantic frames = `site/js/fizz_sysex.js` (F0 7D 46 5A 01 cmd .. sum F7). Bulk = update framing, command byte 0x46 (`bulkFrame`/`parseBulk`).

## 0. Protocol cheat sheet (what the tables below refer to)

| Command (host→unit) | Byte | Builder (fizz_sysex.js) | Reply | Parsed by |
|---|---|---|---|---|
| IDENTITY | 0x01 | `identity()` :124 → `F0 7D 46 5A 01 01 7F F7` | 0x41 `[protocol, ascii "FIZZ 0.12.0…"]` | `parseReply` :303 |
| STATUS | 0x02 | `statusRequest()` :247 → `F0 7D 46 5A 01 02 7E F7` | 0x42 | `parseStatus` :270 |
| FLUSH | 0x03 | `flushRequest()` :248 | 0x42 STATUS (once written) | |
| STORE / STORE6 | 0x10 / 0x11 | `storeSound()` :133 | 0x50 `[status, pool, slotHi, slotLo]` | `parseReply` :304 |
| TAGS | 0x12 | `tagsRequest({source,slot,tags})` :250 | 0x52 ACK | `parseAck` :266 |
| RENAME | 0x13 | `renameRequest({source,slot,name})` :251 (source mine or project=3) | ACK | |
| DELETE | 0x14 | `deleteRequest({source,slot,confirm})` :253 | ACK (status 10 IN USE w/o confirm) | |
| SWAP | 0x15 | `swapRequest({source,a,b})` :254 | ACK (pool: a job) | |
| POOL REF | 0x16 | `poolRefRequest({slot,id})` :255 (slot 0 = first free; ACK.a = slot) | ACK | |
| USERS | 0x17 | `usersRequest({source,slot})` :256 | 0x57 | `parseUsers` :279 |
| LABELS | 0x20 | `labelsRequest` :168 (not used by Manager) | 0x60 | `parseLabels` |
| LIST | 0x21 | `listRequest({factory,pool})` :193 → flags byte (1 factory, 2 pool) | 0x61 in parts, until `last` | `parseList` :201 |
| DUMP | 0x22 | `dumpRequest({source,slot})` :194 (src 0 mine,1 factory,2 pool,4 track; slot 0 = all / selected track) | STORE/STORE6 frame with DUMP flag 0x08 | `parseDump` :211 |
| PROJECTS | 0x23 | `projectsRequest()` :249 | 0x63 | `parseProjects` :286 |
| PROJECT | 0x24 | `projectRequest({op,p,q})` :257 ops 1 load,2 new,3 copy,4 delete | ACK | |
| PARAM | 0x30 | `paramRequest({track,id,value,ack})` :157: `[t(127=selected), id, v>>14, v>>7, v (v=value+2^20, 7-bit each), ack]` | ACK if ack | |
| BROWSE | 0x31 | `browseRequest({track,id})` :163 `[t, id 3×7 bits]` | ACK a=track+1, b=browser place | |
| (error) | — | — | 0x7F `[status, command]` | |

Frame/checksum: `frame(cmd, body)` fizz_sysex.js:119 = `F0 7D 46 5A 01 cmd body… cs F7`, `cs = (0x80 − (Σ(cmd+body) & 0x7F)) & 0x7F` (:118); protocol byte 01 is **not** summed. Verify: Σ bytes[5..−1] ≡ 0 mod 128 (`sumOk` :199).
STATUS codes (`STATUS` :85): 0 OK, 1 FULL, 2 BAD LENGTH, 3 BAD CHECKSUM, 4 BAD PATCH, 5 BAD SLOT, 6 BAD VERSION, 7 BAD COMMAND (older FIZZ), 8 BUSY (playing / job), 9 NOT FOUND, 10 IN USE, 11 BAD TRACK.
STORE flags (`FLAGS` :82): 0x01 POOL, 0x02 BATCH (joins previous undo level), 0x04 LOAD (onto track), 0x08 DUMP (reply marker), 0x10 AUDITION (track only, nothing stored), 0x20 OWN (owner's preset, not "imported").
STORE body (:150): `[flags, slot>>7, slot&7F, name×12 (ASCII 0x20–0x7E else '?', space-padded), drive, track, pan+64, chorus, delay, reverb, mixMask(1 pan,2 cho,4 del,8 rev), pack7(blob) (110 B for 96-byte blob / 147 B for 128-byte), [tags mask], [8 extras bytes]]`. Extras (`extrasBytes` :156): `id>>14, id>>7, id, sound>>7, sound, flags(1 edited), 0, 0`; in a track DUMP flags byte also 2 selected, 4 rec, 8 playing, 16 browse, then chan, sel (`parseDump` :221).
LIST entry (20 B from offset 11, :205): `[source, slotHi, slotLo, id 3×7, name×12, tags, flags(1 imported, 2 inPool)]`; header `b[6] part, b[7] last, b[8..9] total, b[10] count`.
STATUS reply (:270): `b6 protocol, b7 tracks, b8 flags(1 playing,2 noStore,4 dirty,8 job,16 demo,32 rec,64 browse), b9 project−1, b10 pattern, b11 projects, b12-13 mine, b14-15 pool, b16-18 written(21 bit), b19-22 lib[used,all] pages, b23-26 prj[used,all], b27 jobCommand, [b28 sel]`.
ACK (:266): `b6 command, b7 status, b8-9 a, b10-11 b` (14-bit). USERS (:279): `b6 source, b7-8 slot, b9 n, n×[project−1, steps14], tracks`. PROJECTS (:286): `b6 current−1, b7 count, count×15 B [flags(1 there,2 current,4 demo), name×12, pages14]`.

**Two different 7-bit packings — do not mix them up:**
- `fizz_sysex.js` `pack7` :100 — groups of 7 bytes → 8: first byte holds the 7 top bits (bit j = byte j's bit 7), then the 7 low-7-bit bytes. Used for the FM TONE blob inside STORE/DUMP.
- `fm1lib.js` `pack7` :82 — LSB-first bitstream (8-bit → 7-bit, accumulator). Used for the whole update/bulk frame (`frame`, `response`, `parseRequest`, `parseBulk`).

## 1. Features per tab

### Header controls

| Control | What it does | FizzLink / SysEx | Kind |
|---|---|---|---|
| CONNECT (`#unit`) | `connect()` manager.js:181: `navigator.requestMIDIAccess({sysex:true})`, `onstatechange` → `attach()` :195. Auto-connect at load if the `midi`+sysex permission is already granted (:1216). Re-attach 800 ms after any port state change; a disconnected port → `lost()` (:247). | `findFizz` (update handshake 0x11 on each candidate port) → `identity()` (0x01) → `storeInfo()` (bulk FZBI) → `list({factory:true,pool:true})` (0x21 flags 3) → `status()` (0x02) → `projects()` (0x23); if tab ≠ BACKUP: `loadDumps()` (0x22 mine slot 0, then factory slot 0); `live.probe()` → `dumpTrack(0)` (0x22 src 4 slot 0). | READ |
| Connect error states | `notfound` "NO FM-1", `notfizz` (other firmware: shows its id), `update` (OTA loader: "UPDATE MODE"); FIZZ with no IDENTITY answer (safe mode/no data blob) but FZBI → backup/restore only; no FZBI → "update FIZZ"; no STATUS → library read-only (:216-219). | — | LOCAL |
| BROWSER (`#bunit`) | `useBrowser()` :1166: drops the Web MIDI link, starts `BrowserUnit` (wasm FIZZ in AudioWorklet behind a fake MIDIAccess), boots from IndexedDB kv `browserFlash` else newest full backup kept here, then the same `attach()`; switches TRACK on. Click again → `leaveBrowser()` :1182. | same as CONNECT but against the simulated unit | LOCAL |
| TRACK switch (`#live`, number = followed track) | live.js `setOn()` :62: on → EDIT edits the unit's track live and LIBRARY/IMPORT picks audition on it; off → Manager never touches tracks. Hidden without STATUS; disabled unless a track DUMP answered (`probe` :324). One-time hint, localStorage `fizz-manager-track-seen`. | turning on: `dumpTrack(slot)` (0x22 src 4) | READ (switch itself LOCAL) |
| Refresh (`#refresh`) | `reread()` :690. Also on window focus / visibilitychange (≥2 s apart) as `quick`: STATUS only, full reread if `written, mine, pool, project, projects, dirty, noStore` changed (:689). | (identity if none) → FZBI → LIST (≤4 tries, 300 ms) → STATUS → PROJECTS → `loadDumps` (0x22 mine 0 / factory 0) → `live.probe` | READ |
| LOG (`<details id=logbox>`) | Last 400 lines (`log()` :43): every op, timings, RTT stats, CRCs. | — | LOCAL |
| Toast / `#say` line / progress scrim (`#prog`, sector map, % , time, CANCEL → AbortController) | — | — | LOCAL |

### BACKUP

| Feature | What it does | FizzLink / SysEx | Kind |
|---|---|---|---|
| THIS FM-1 card | FIZZ version (git-describe suffix stripped), projects (STATUS.projects), my presets (LIST count), store used % and 4-part meter (STATUS lib/prj pages, else last backup's `readRegion().used`), store KB. `renderUnit` :156 | data from connect | READ |
| ● BACK UP (`#go-backup`) | `backup("full")` :327 → `readUnit` :288: whole store read, CRC-checked; `readRegion(image)` summary; then all MY PRESETS dumped; `.fizzbak` made (`makeFizzbak`) and **kept in IndexedDB** (not downloaded; `saved:false`). Shows speed (KB/s, ms/read, resent, depth). | `backup({depth:DEPTH=2, gapMs:0})` = FZBI, then FZBR off,n (256-B chunks, 16 KB pieces, ≤2 in flight), CRC-32 vs FZBI (one full re-read if different); then `list()` (0x21), `dumpAll("mine", n)` (0x22 src 0 slot 0), missing slots `dump("mine", slot)` | READ |
| PRESETS ONLY (`#go-presets`) | `backup("presets")`: no store image; LIST + DUMP all MY PRESETS → `.fizzbak` kind `presets`. | 0x21, 0x22 | READ |
| Backups list (`#baks`) | From IndexedDB `backups`, newest first; shows when, projects/presets/size, FIZZ version, "old" (other layout / track count / 8 tracks), safety copies, total size and `navigator.storage.estimate()` quota; `navigator.storage.persist()` asked on first keep. | — | LOCAL |
| ↓ save (per row) | `saveBak` :138: `showSaveFilePicker` (id `fizz-backups`) else download; marks `saved:true`. | — | LOCAL |
| × delete (per row) | confirm (warns if never saved to a file) → `DB.del(id)`. | — | LOCAL |
| ▶ play (full backups) | opens `../?backup=<id>` (the player page boots that store via `js/backup-play.js`). | — | LOCAL |
| RESTORE (per row) | `openFile(bytes)` → `openRestore` :462. | see below | — |
| Drop zone / file input (`.fizzbak .fizzlib .bin`) | `openFile` :423: `readBackup` → kept in list (`file:true, saved:true`) → restore modal if connected. | — | LOCAL |
| RESTORE modal, full mode (image size == unit store size) | Shows now→then (projects · presets), project chips, ± presets, "8 TRACKS · PROJECTS START FRESH"; **first an automatic safety backup** (`backup("full",{safety:true})`, named "FIZZ before restore …"); user must type `RESTORE`; then `restoreImage` :537 with progress (CHECKING → "WRITING · DO NOT UNPLUG", cancel only in check pass). After: `refresh()` :517 (IDENTITY polled 10×500 ms, FZBI, LIST, STATUS, PROJECTS). Error texts per FZER 10/11/12/13/14, lost, timeout (:558-565). | `restore(image)` = `wake()`, bulk `FZSR len crc` (re-sent ≤3× if no read request within 3 s), then answers the unit's 0x30 read requests (≤512 B, `response(addr,data,fl)`) for two passes (check, write), ends on bulk `FZOK` / `FZER code` | **WRITE** (whole store; unit undo cleared) |
| RESTORE modal, presets mode ("ADD n": .fizzlib, presets-only .fizzbak, other store size) | `addPresets` :601: dumps MY PRESETS to skip duplicates (`sameSound`), STOREs each new one (batch after the first) until FULL. Then LIST + STATUS. | `dumpAll("mine")` (0x22), `store(storeFrame(s,{batch}))` (0x10/0x11, slot 0 = first free) | **WRITE** |

### LIBRARY

| Feature | What it does | FizzLink / SysEx | Kind |
|---|---|---|---|
| List (MY PRESETS then FACTORY), "n / 256" | rows from LIST; tile art/algorithm/MIDI chip from DUMP frames (`loadDumps` :967 when tab ≠ BACKUP). | 0x21; 0x22 mine 0 + factory 0 | READ |
| Filters | ALL / FACTORY / MY PRESETS; IN POOL / NOT IN POOL; 7 FIZZ tags (`presets.js`). | — | LOCAL |
| EXPORT ALL | DUMPs if missing, then Export dialog with all MY PRESETS, default FIZZ (.fizzlib). | 0x22 | READ + LOCAL file |
| Click a preset | select; `previewRow` :1128: TRACK on → `live.pick` (keeps the track's own sound for CANCEL, AUDITION onto the track, bar "TRACK n ← NAME [KEEP] [CANCEL]"); TRACK off → heard in the browser FIZZ. Missing frame → `fetchOne` (`dump(source,slot)`). | TRACK on: `dumpTrack` (0x22 src 4) + `store(storeSound({…, track, audition:true, extras}))` (STORE flags LOAD|AUDITION, retried ≤3 with DUMP-track check, live.js:256) | WRITE (track sound only, nothing stored, no undo level) / LOCAL |
| KEEP / CANCEL bar (`#loadbar`) | KEEP: leave it. CANCEL: put the track's own DUMP back as an AUDITION (`trackFrame`). | `store(trackFrame(dump))` | WRITE (track) |
| Name field (MY only) | uppercase printable ≤12 → RENAME, re-DUMP, refresh; web tags follow the new soundKey. | `rename("mine",slot,name)` 0x13 → `dump` → LIST/STATUS | **WRITE** |
| EDIT | `openInEditor` :1047: TRACK on → audition on track + EDIT follows track; off → editor opens the preset locally (browser FIZZ target). | (TRACK on: as Click) | LOCAL / WRITE(track) |
| LOAD ON FM-1 | `live.loadOnUnit` live.js:280: cancels pending audition, opens **the unit's own sound browser** on that preset (unit's OCT− cancels, OCT+ loads); STATUS polled every 0.6 s (≤180 s) until `browse` flag clears, then track DUMP tells which. | `browse(track|"selected", id)` 0x31; `status()` 0x02; `dumpTrack` | WRITE (unit UI; owner confirms on unit) |
| EXPORT | Export dialog for one sound. | — | LOCAL |
| + POOL / − POOL | +: reference in first free pool place; −: clear every place referencing it (IN USE → confirm → with confirm). Shown at once, LIST re-read until it agrees. | `poolRef(0,id)` 0x16 (ACK.a = place); `remove("pool",k)` 0x14, then `remove("pool",k,{confirm:true})` | **WRITE** |
| FIZZ tag chips (7) | toggle bit → TAGS (MY and FACTORY presets), re-DUMP. | `tags(source,slot,mask)` 0x12 | **WRITE** |
| Web tags (+ / ×) | free text tags (≤16 chars) kept by `soundKey` in IndexedDB kv `web`, written into backups. | — | LOCAL |
| USED BY (MY) | projects (+ step dots) using the preset. | `users("mine",slot)` 0x17 → 0x57 | READ (job on unit) |
| Mix row | pan / chorus / delay / reverb / drive from DUMP. | — | READ |
| ↓ IMPORTED chip | LIST flag bit 0. | — | READ |
| DELETE (MY) | confirm (says USED BY / pool places) → delete with confirm. | `remove("mine",slot,{confirm:true})` 0x14 | **WRITE** |
| Reorder | drag a MY cassette onto another, or ALT+↑/↓: swap places (ids move with them), re-DUMP both. | `swap("mine",a,b)` 0x15 | **WRITE** |
| Computer keys | A S D F G H J K L ; ' (white), W E R Y U O P [ (black), Z/X octave (−2..+2), base note 53 (F3): play picked sound — TRACK on: on the unit's track; else browser FIZZ. Also POOL and IMPORT tabs (`KEY_TABS` :1114). | `note(ch,n,vel)` raw 0x9n/0x8n, ch = track's `extras.chan` (default 14 = ch 15) | WRITE (notes only) / LOCAL |

### EDIT (details in §5)

| Feature | What it does | FizzLink / SysEx | Kind |
|---|---|---|---|
| Panel (OPERATORS, OP ENV, PLAY, FILTER, AMP, DRIVE·CRUSH, LFO ×3, MOD ×8) | all 115 values; drag/wheel/arrows (SHIFT fine), dbl-click default, list pickers. MIDI sounds: 7 pages × 4 slots (value, CC#, untouched). | target.param / target.sound | see target |
| Target button (`#ed-target`) | "▶ PLAY IN THIS BROWSER" / "THIS BROWSER · TRACK 1" (browser FIZZ) or "TRACK n · LIVE/SHOWN". | — | — |
| Track strip (TRACK capable) | SELECTED / 1..6 / LIVE; follow the unit's track; DUMP polled every 500 ms (skipped ≤700 ms after a PARAM). | `dumpTrack(slot)` 0x22 src 4 | READ |
| Value change, target = FM-1 track (TRACK on) | latest value per id queued, one PARAM at a time with ACK; older FIZZ (BAD COMMAND) → TRACK off. MIDI sound: whole sound as AUDITION. TRACK off → locked, switch nudged. | `param(t,id,v)` 0x30 ack; `store(storeFrameOf(s,{track,audition:true,extras}))` | WRITE (track) |
| Value change, target = browser FIZZ | whole sound STORE6 into the wasm's MY PRESETS 256 with LOAD track 0, latest-wins (manager.js:1011). | wasm only | LOCAL |
| Name / tags | edited in the sound (sent with SAVE). | — | LOCAL |
| ↺ REVERT | back to base; on a followed track: its original DUMP put back (`trackFrame`), else AUDITION of base. | `store(trackFrame(base))` | WRITE (track) |
| SAVE (preset opened from MY PRESETS n) | confirm → STORE6 over slot n, OWN; TRACK live → also LOAD onto the track. Job if slot used (BUSY while playing). | `store(storeFrameOf(s,{slot,own:true,track?}))` 0x11 | **WRITE** |
| SAVE AS NEW | same, slot 0 (first free). | 0x11 | **WRITE** |
| EXPORT | Export dialog. | — | LOCAL |
| Drop a file on EDIT | `.fizzsnd` or Digitone file opened in the editor (detaches from track). | — | LOCAL |
| Keybed (27 keys from F, OCT ±) + computer keys | note on/off to the target. | `note()` raw or wasm | WRITE (notes) / LOCAL |
| REC indicator | from track DUMP extras.rec. | — | READ |

### POOL

| Feature | What it does | FizzLink / SysEx | Kind |
|---|---|---|---|
| PROJECT picker | buttons for existing projects; another → confirm "LOAD PROJECT" (undo cleared). | `project("load",p)` 0x24 op 1; then LIST/STATUS/PROJECTS | **WRITE** |
| 128 tiles (16×8) "n / 128" | pool rows from LIST; colour by first tag; MIDI chip. | 0x21 (pool flag) | READ |
| Step dots per tile | background USERS per filled place. | `users("pool",k)` 0x17 | READ (job) |
| Click tile | select (name, dots, CLEAR); heard on keys. | — | LOCAL |
| SOUNDS list (MY + FACTORY, same filters) | click = pick (its places ringed) then click a tile; or drag onto a tile. | `poolRef(k,id)` 0x16 | **WRITE** |
| Tile onto tile / ALT+arrow | confirm SWAP/MOVE (every pattern's locks follow; stopped only; undo cleared). | `swap("pool",a,b)` 0x15 (job) | **WRITE** |
| CLEAR | confirm → steps revert to track sound. | `remove("pool",k,{confirm:true})` | **WRITE** |
| SORT BY TAG | confirm → sequence of SWAPs into tag order then name. | n × 0x15 | **WRITE** |
| CLEAR UNUSED | confirm → DELETE each place without confirm; IN USE ones stay. | n × 0x14 | **WRITE** |

### PROJECTS (projtab.js + projects.js model)

| Feature | What it does | FizzLink / SysEx | Kind |
|---|---|---|---|
| 8 slots, store-use bar | names/current/there from PROJECTS; pattern counts and 32-dot preview from the store model. | `projects()` 0x23 | READ |
| READ / auto-read on show (when STATUS.written changed) | if STATUS dirty & not playing: FLUSH first; then full store read (no backup kept) → `modelOf(image)`. | `flush()` 0x03; FZBI+FZBR (`backup()`); `status()` | READ (FLUSH commits pending autosave) |
| LOAD | open that project on the unit. | `project("load",p)` | **WRITE** |
| RENAME | inline, `cleanName` (A–Z0–9 and `-.,'&!?#+/()*:_~ `, ≤12). | `rename("project",p,name)` 0x13 src 3 | **WRITE** |
| COPY TO → click slot | confirm if target used. | `project("copy",p,q)` | **WRITE** |
| MOVE TO → click slot | copy then delete source. | `project("copy")` + `project("delete",p)` | **WRITE** |
| KEEP | project (records + referenced presets) as `.fizzprj` bytes into IndexedDB kv `keptProjects`. | — | LOCAL |
| DELETE | confirm. | `project("delete",p)` | **WRITE** |
| Pattern grid A01–D16 (4×16 tiles, 6 track rows of 16 trigs) | click / SHIFT-click range / CMD-click toggle; demo patterns drawn as such. | — | LOCAL |
| COPY (⌘C) | clip of patterns + their pool refs + presets. | — | LOCAL |
| PASTE (⌘V) / CLEAR | queued ops (pool numbers re-mapped, presets linked/added; errors poolfull/libfull/range/demo). | — | LOCAL (pending) |
| .fizzpat ↓ / IMPORT (.fizzpat/.fizzprj, also drop) | download clip; load clip (pastes if patterns selected) or keep project. | — | LOCAL |
| Pending bar: UNDO (⌘Z), DISCARD | — | — | LOCAL |
| WRITE TO FM-1 | STATUS (refuse if playing); re-read & replay ops if `written` changed; `imageOf(model)`; **safety .fizzbak "FIZZ before projects" kept**; full restore. | `status()`, (FZBI/FZBR), FZSR + 0x30 serving → FZOK | **WRITE** (whole store) |
| KEPT view: LOAD INTO → click slot | queued "load" op (slot's records replaced, presets added) then WRITE. | (at WRITE) | LOCAL→WRITE |
| KEPT: .fizzprj ↓, RENAME, DELETE | — | — | LOCAL |

### IMPORT

| Feature | What it does | FizzLink / SysEx | Kind |
|---|---|---|---|
| Drop / choose `.fizzlib .fizzsnd .fizzbak .dnsnd .dn2pst .dn2snd` | `readImport` (importer.js:22) converts locally; per row: source (FIZZ/DN1/DN II), name, 10-segment fidelity bar (10 − notes), first note, FIZZ tags + 2 web tags, MIDI chip; refused rows greyed with reason. | — | LOCAL |
| Duplicate flags | "already in MY PRESETS" (same name+patch vs DUMPs) / "twice in these files"; not ticked. | `loadDumps` 0x22 if needed | READ |
| Row click, ↑/↓, SPACE | hear (TRACK on: AUDITION on the unit's track with KEEP/CANCEL; else browser FIZZ); SPACE ticks. | `store(… audition)` | WRITE(track) / LOCAL |
| Destination MY PRESETS / POOL / TRACK (+ track 1–6; TRACK only with TRACK switch on) | — | — | LOCAL |
| ↓ SEND n TO FM-1 | each ticked: STORE/STORE6 slot 0 (POOL: pool flag → also referenced in pool; TRACK: first one LOAD onto track), BATCH after first, FIZZ tag mask; stops at FULL; per-row ✓ MY/POOL n; Digitone tags without a FIZZ tag saved as web tags; then LIST/STATUS + DUMPs. | `store(storeFrame(s,{pool,batch,track}))` 0x10/0x11 | **WRITE** |
| CLEAR | empties the list. | — | LOCAL |

### Export dialog (exporter.js, opened from LIBRARY, EXPORT ALL, EDIT)
Format cards FIZZ `.fizzsnd` (1) / `.fizzlib` (many), DIGITONE `.dnsnd` (EXPERIMENTAL), DIGITONE II `.dn2pst` or `.dn2snd` (EXPERIMENTAL); each with carry bar and list of lost/changed values from the writers' notes (`carryOf`/`noteRow`); MIDI sounds skipped for Digitone; several Digitone files → stored ZIP. ↓ DOWNLOAD. LOCAL.

## 2. File formats

| Ext | Produced by | Parsed by | Structure | Browser storage |
|---|---|---|---|---|
| `.fizzbak` | `makeFizzbak` fizzbak.js:51 | `readBackup` fizzbak.js:100 (also accepts `.fizzlib` and raw `.bin`) | Stored ZIP (`zipStored`, library.js:78): `manifest.json` {format "fizz-backup", version 1, kind full/presets, date, fizz, unit{identity,port}, store{layout,magic "Z2FS",format,tracks,region[lo,size],crc32 hex,file "store.bin",used}, projects[{slot,name}], presets n, files[], web{tags}, transfer{…}}; `store.bin` (full only, raw region); `presets/NNN NAME.fizzsnd` (each with optional `web.tags`); `library.json` (a .fizzlib index). Reader checks member CRC-32s and store crc32. | IndexedDB `fizz-manager` v1, store `backups` (keyPath `id`, autoIncrement): {date, name, kind, safety, saved, file?, fizz, identity, bytes (the .fizzbak), size, tracks, layout, projects[names], presets n} |
| `.fizzlib` | `makeFizzlib` library.js:109 | `parseFizzlib` library.js:118 | Stored ZIP: `library.json` {format "fizz-library", version 1, name, author?, date, fizz, count, files[]} + `NNN NAME.fizzsnd`. | file only |
| `.fizzsnd` | `soundToFizzsnd` library.js:39 | `parseFizzsnd` library.js:49 | JSON (2-space): {format "fizz-sound", version 1, name, tags[FIZZ tags], favourite, pan, sends{chorus,delay,reverb}, drive, patch{blob base64, blobVersion 5\|6}, source{fizz, exported, from}, [web{tags}]}. Blob 96 B ('Q' 5) or 128 B ('Q' 6). | file only |
| `.fizzprj` | `makeFizzprj` projects.js:303 | `readFizzprj` :323 | Stored ZIP: `manifest.json` {format "fizz-project", version 1, name, demo, date, fizz, patterns, sounds, records[{kind,i,file}], presets[{id,name,file}]}; `records/<key hex>.rec` (raw store records of the project, project byte 0); `presets/<id> NAME.rec` + `.fizzsnd`. | IndexedDB `fizz-manager` store `kv`, key `keptProjects`: [{id, name, date, pats, sounds, colours, bytes}] |
| `.fizzpat` | `makeFizzpat` :330 | `readFizzpat` :339 | Stored ZIP: `manifest.json` {format "fizz-patterns", version 1, from, date, fizz, patterns[{k,name,file}], pool[{n,id}\|{n,file}], presets[]}; `patterns/A01.rec`, `pool/<n>.rec`, `presets/…`. | file only |
| `.bin` | (firmware/tools/fizz_xdata.py backup) | `readBackup` → `readRegion` | Raw store region: 73×4096 = 299 008 B ("device" layout: MY PRESETS 22 sectors + projects 51) or 1 MiB ("host"). Log-structured pages, see fizzstore.js:14-24. | — |
| store image (internal) | `buildImage/writeStore/convertImage` fizzstore.js:176-222 | `mount/get/readRegion/regionRecords` :44-217 | 4 KiB sectors, header "Z2FS" + seq u32 + 5A5A/~; 15 × 256-B pages: key u32, rseq u32, idx u16, np u16, len u16 (FFFE tombstone), CRC-16/CCITT-FALSE; 240-B payload. Keys `kind<<24 \| proj<<16 \| i` (RK SET 1, HDR 2, PAT 3, SND 4, LIB 5, FTAG 6, MFX 7). MY preset record 'S' 5: name[13], 128-B blob, id u16, tags, flags, pan, cho, del, rev (fizzstore.js:121). Writer keeps 6 blank sectors (RESERVE). | `kv` key `browserFlash` = BrowserUnit's 1 MiB host flash |
| `.dnsnd` (Digitone) | `fizzToDnsnd` fizz2dn.js:360 | `readFiles`→`readDnsnd` digitone.js → `parseDnsnd`/`dnsndToQuad` fm1dnsnd.js | ZIP (Transfer style, may be deflated) of `manifest.json` {FormatVersion, ProductType, Payload, FileType "Sound", FirmwareVersion, MetaInfo.Tags} + payload: Elektron container (31-B header, LZ4 block chain, 12-B trailer with CRC-32) around a v5 sound record. | — |
| `.dn2pst` (Digitone II) | `fizzToDn2pst` fizz2dn.js:368 | `readDn2` → `parseDn2pst`/`dn2pstToQuad` dn2pst.js | ZIP of manifest + container around a 364-B body: u32be record version, 0, 359-B record (BEEFBACE, version, tag bits, name@12, params u16be @ 28+2·id). Only FM TONE machine imports. | — |
| `.dn2snd` | `fizzToDn2pst(…).payload` (exporter.js:59) | `readDn2` | the same stored container without the ZIP. | — |
| Digitone export ZIP | `exportFiles` exporter.js:54 | — | stored ZIP of several `.dnsnd`/`.dn2pst`/`.dn2snd`, `FIZZ presets YYYY-MM-DD (Digitone[ II] kind).zip`. | — |
| Web tags | — | — | `{tags: {soundKey: [TAG…]}}`; `soundKey` = hex CRC-32 of name bytes + 0 + blob[2..86] (fizzbak.js:37). | IndexedDB `kv` key `web` |
| Other browser storage | — | — | localStorage `fizz-manager-track-seen` = "1" (live.js:25). File picker id `fizz-backups`. URL `?sim`, `?depth=N` (1–4, default **2**: manager.js:34; header comment says 1), `&gap=MS`, `#tab`. | |

Not read anywhere: `.syx` (owner's decision, importer.js:9). Not imported by the Manager: `site/js/saves.js`, `labels.js`, `dnexport.js`, `digitone-ui.js` (player page).

## 3. Connection

**Port discovery** — `Updater.find(filter)` fm1lib.js:205: for each MIDI input not `disconnected` whose name matches `/fm-?1|felucca|ota|composite|sinco|usb-midi/i` (never probes other gear), output = same name, else the only output; open both, `new Link(in,out)` (queues only messages starting F0, :135), `handshake(link, 2)` :159: drain, send `HS_QUERY` up to 2×, read replies ≤1000 ms each until `parseIdentity` accepts one. Returns the first `{link, id, name}` passing `filter`.

**findFizz** fizzlink.js:75: `find(isFizzId)` where `isFizzId = !IS_OTA(id) && id.text.startsWith("FM-1_46")` (:39, :48; "FM-1_46" = firmware/app/fm1.toml id_prefix, e.g. `FM-1_46001100`; BrowserUnit uses `FM-1_46999999`). If none: `find(() => true)`; none → `notfound`; `IS_OTA(id)` (`/^ota-/i` on model, fm1lib.js:66) → `update` (unit in update loader); else `notfizz` (stock "FM-1_0xx", M-VAVE Open "FM1_OFFICIAL_OPEN"/"FM1_COMMUNITY_n"…). Then `identity()` decides FIZZ answering vs safe mode.

**Update framing** (fm1lib.js, fm1-base/felucca-bsp ota.c):
```
frame(cmd, payload)  :174   F0 pack7LSB([00, 59, cmd, len&FF, len>>8, 00, ...payload, ~Σpayload & FF]) F7
HS_QUERY            :62    = frame(0x11, [])          = F0 00 32 45 00 00 00 40 7F F7
UPGRADE             :63    F0 22 24 35 7F F7          (installer only)
identity reply              unpack7 → 34 bytes: [00, 59, 11, 27, 00, 00, text×27 (NUL-padded), sum]
parseIdentity       :122   requires len 34, d[0]=0, d[1]=59, d[2]=11; text = d[6..33] minus trailing NULs;
                           OPEN_RE /^FM1_(OFFICIAL_OPEN|COMMUNITY_\d+)$/ → {model:"FM-1",open}; else /^([^_]+)_(\d+)$/ → {text, model, version}
read request (unit→host, cmd 0x30)  unpack7 → exactly 15 bytes:
                           [00, 59, 30, 08, 00, 00, fl, addr u32 LE, len u24 LE, ~Σ(u[6..13]) & FF]
parseRequest        :112   → {fl, addr, len}   (e.g. addr 0x200 len 512 = F0 00 32 41 41 00 00 00 00 00 04 00 00 00 40 00 00 7B 01 F7)
response(addr,data,fl) :101  F0 pack7LSB([00, 59, 30, (len+8)&FF, (len+8)>>8, 00, fl, addr u32 LE, len u16 LE, 00, ...data, ~Σ(body[6..]) & FF]) F7
                           (MAXDATA 512; installer answers addr E0000000/F0000000 with "success\0")
```
**Bulk** (fizzlink.js:57-68; firmware app.c `app_usb_cmd`, cmd byte `BULK = 0x46`):
```
bulkFrame(tag, ...u32)  = frame(0x46, [tag ASCII×4, ...each word u32 LE])
  FZBI                  → F0 00 32 19 22 00 00 00 23 5A 04 25 22 0D F7
  FZBR 0, 256           → F0 00 32 19 62 00 00 00 23 5A 04 49 02 00 00 00 00 00 02 00 00 20 19 F7
  FZSR len, crc32
parseBulk(p): unpack7 → u; need u.length ≥ 11, u[0]=0, u[1]=59, u[2]=46; n = u[3] | u[4]<<8; u.length ≥ 7+n, n ≥ 4;
              ~Σ(u[6..6+n)) & FF == u[6+n]; → {tag = ascii u[6..10], data = u[10..6+n)}
replies: FZBI data [lo u32, size u32, crc32 u32]; FZBD [off u32, ≤256 bytes]; FZOK [code]; FZER [code u32]
FZER codes (:43): 1 refused, 10 playing, 11 size not this store, 12 image not whole (nothing changed), 13 write failed (store cleared), 14 read-back mismatch
```
CRC-32: standard reflected 0xEDB88320, init/xorout FFFFFFFF (fizzbak.js:29).

**Timing / flow rules** (FizzLink):
- One operation outstanding: promise queue `this.q` (`op()` :127); replies routed to the current op's `onFrame` (returns undefined = not mine, `MORE` = keep waiting & re-arm timer, value = done). A reader loop polls `link.read(400)` forever (unit stalls if replies aren't read, :12); lost port → `onLost`.
- `send()` :149: frames spaced ≥ `GAP_MS = 3` ms (unit holds one inbound frame, empties it ~every 2 ms) using Web MIDI's timestamped `output.send(bytes, at)`; send failure → lost.
- Retries only for idempotent reads, only if nothing of the reply was heard: `identity` 700 ms ×3 (then null), `dump` 1000 ms ×3, `status` 800 ms ×3, `projects` 800 ms ×3, `users` 5000 ms ×1, `dumpTrack` 800 ms ×2 (then null), `param` 1000 ms ×1, `storeInfo` 2500 ms ×3. No retry: `list` 3000, `dumpAll` 4000 idle (partial result), `store` 3000, `ask1` 15000 (jobs), `project` 30000, `browse` 3000, `restore` 30000 idle.
- `wake()` :199: if nothing received for >1000 ms, a STATUS first (the first frame after a pause is the one most often lost); called before `store`, every non-retried `ask1`, `restore`.
- Backup read: `readPatient` 16 KB pieces (a stalled piece re-read after 1.5 s, ≤5 tries); `readRegion` FZBR per 256-B chunk, `depth` in flight (Manager uses 2; fm1-base holds two frames since 2026-10-27), matched by offset, overdue chunk re-sent one per 10 ms tick, rto 800 ms then `max(150, min(800, 4×max RTT of last 64))`, ≤20 tries/chunk; CRC mismatch vs FZBI → read whole once more (`tries = 2`) then `changed`.
- Restore: FZSR re-sent after 3 s without a read request (≤3 sends); pass 2 detected when addresses restart; cancel only in pass 1 (served with zeros → unit CRC fails → FZER 12, nothing erased).
- Notes: `note()` sends raw 3-byte Note On/Off (not queued).

## 4. Modules

Pure = no DOM / Web MIDI / IndexedDB / AudioWorklet. **JSC caveat:** a bare `JSContext` lacks `TextEncoder/TextDecoder`, `atob/btoa`, `setTimeout`, `DecompressionStream`, `performance`, and (on iOS, no JIT) probably `WebAssembly`; "pure" modules marked † need those small polyfills. All Manager/site/js files are **SPDX GPL-3.0-only**; `site/install/fm1lib.js` is **MPL-2.0** (generated from fm1-base 42dc6304e807, ©Charles Vestal, Leo Kuroshita/Hügelton); the originals in `tools/` (`fizz_sysex.mjs`, `fizz2dn.mjs`, `dn2pst.mjs`, `test_*.mjs`) are **MPL-2.0** (site copies relicensed GPL; `fizz_sysex.js` differs from `tools/fizz_sysex.mjs` only in the header).

| Module | LOC | Pure? | Main exports / role |
|---|---|---|---|
| manager/manager.js | 1219 | UI | page controller: connect/attach, BACKUP, restore, LIBRARY, IndexedDB `DB`, browser target, keys, BROWSER mode, `?sim` |
| manager/fizzlink.js | 417 | logic, but transport/timers (setTimeout, setInterval, performance; duck-typed MIDIAccess) | `findFizz`, `FizzLink`, `parseBulk`, `bulkFrame`, `FZER`, `FIZZ_PREFIX`, `BULK`, `CHUNK`, `GAP_MS` — port to Swift |
| manager/fizzstore.js | 222 | **pure, zero deps** | `LAYOUTS`, `layoutOf`, `mount`, `get`, `preset`, `readRegion`, `records`, `writeStore`, `buildImage`, `regionRecords`, `convertImage`, `RESERVE` |
| manager/fizzbak.js | 147 | pure † (async via dnReadZip) | `crc32`, `hex8`, `soundKey`, `backupFileName`, `makeFizzbak`, `readBackup`, `FORMAT` |
| manager/projects.js | 349 | pure † | `modelOf`, `imageOf`, `walkPat`, `project(s)`, `copyPatterns`, `pastePatterns`, `clearPatterns`, `keepProject`, `loadProject`, `renameKept`, `cleanName`, `make/readFizzprj`, `make/readFizzpat`, `RK`, `rkey`, `patName` |
| manager/sound.js | 129 | pure | `soundFromFrame`, `soundFromLib`, `blob6`, `storeFrameOf`, `libSound`, `copySound`, `sameValues`, `info/text/clampV/stepOf/tickStep`, `lfo`, `mod`, MIDI-sound helpers (`isMidiBlob`, `midiBlob`…) |
| manager/fizzvalues.js | 134 | pure data (generated by tools/gen_values.c) | `VALUES` (115 rows), `ALGOS`, `DEST_ORDER` |
| manager/exporter.js | 156 | `exportFiles`, `carryOf`, `noteRow` pure †; `mountExport` UI | export dialog |
| manager/importer.js | 185 | `readImport`, `markDuplicates` pure; `mountImport` UI | IMPORT tab |
| manager/presets.js | 65 | HTML-string builders; `poolPlaces` pure | `newFilter`, `filterHtml`, `filterClick`, `listHtml`, `shownRows`, `poolPlaces`, `poolBadge` |
| manager/ui.js | 69 | UI (`esc`, `tagsOf`, `colOf` pure; `confirmBox` DOM) | `refusal` texts, tag colours |
| manager/live.js | 347 | UI/state, timers, localStorage | `mountLive` (TRACK switch, PARAM pump, AUDITION/KEEP/CANCEL, BROWSE watch) |
| manager/editor.js | 793 | UI | `mountEditor`, `SECTIONS` |
| manager/pool.js | 279 | UI | `mountPool` |
| manager/projtab.js | 415 | UI | `mountProjects` |
| manager/simfizz.js | 241 | no DOM (Node runs it); needs wasm engine | `SimFizz` — fake MIDIAccess + model of app.c bulk (FZBI/FZBR/FZSR), one-frame inbound slot, update handshake |
| manager/browserunit.js | 65 | AudioWorklet | `BrowserUnit extends SimFizz`, `BROWSER_ID` |
| manager/browserfizz.js | 72 | AudioWorklet | `BrowserFizz` (editor's browser target) |
| js/fizz_sysex.js | 327 | **pure, zero deps** | all semantic builders/parsers (§0), `pack7/unpack7`, `usbPackets`, constants |
| js/library.js | 227 | pure † (btoa/atob, TextEncoder; `createPresetLink` uses setTimeout) | `soundToFizzsnd`, `parseFizzsnd`, `zipStored`, `makeFizzlib`, `parseFizzlib`, `soundFromStoreFrame`, `sameSound`, file names |
| js/digitone.js | 178 | pure † | `readFiles`, `readDnsnd`, `readDn2`, `storeFrame`, `TAG_MAP`, `fizzTagsOf`, `IMPORTABLE`, `createSender` |
| js/fm1dnsnd.js | 440 | pure † except `dnInflateRaw` (DecompressionStream / node:zlib) | `dnReadZip`, `parseDnsnd`, `dnsndToQuad`, `dnLz4Block`, tables |
| js/dn2pst.js | 552 | pure † (inflate as above) | `parseDn2pst`, `dn2pstToQuad`, `quadPack`, `quadPack6`, `QUAD_*_RANGES`, `dn2RecordFromParams`, `wrapDn2Record`, `dn2pstZip` |
| js/fizz2dn.js | 377 | pure † | `fizzToDnsnd`, `fizzToDn2pst`, `fizzValues`, `Q`, `NP=115`, `lz4Elektron`, `dnName` |
| js/audio.js | 109 | AudioWorklet/fetch | `Player` |
| js/build.js | 8 | trivial | `BUILD`, `ver()` (cache-busting `?v=`) |
| install/fm1lib.js | 351 | frame/response/parseRequest/pack7/unpack7/productOf pure; `Updater`/`Link` Web-MIDI-shaped + setTimeout | `Updater`, `IS_OTA`, `IS_STOCK`, `frame`, `response`, `parseRequest`, `pack7`, `unpack7`, `productOf`, `logicalImage`, `ufwEntries`, `fail` |
| engine/engine.js (+ fizz.wasm 811 KB, worklet/host.js 197) | 492 | WebAssembly host glue (Node-runnable) | `createEngine` — used by ?sim, BrowserUnit, tests |

## 5. EDIT tab: parameters and how edits travel

- **Values:** 115 (`fizzvalues.js` VALUES 0..114; its header still says 110). Groups: 0–11 ALGO, RATIO C/A/B1, HARM, DTUNE, FDBK, MIX, OFS C/A/B1/B2; 12–29 op envelopes A/B (ATK DEC END LEV DLY TRIG RESET), PHRT, VEL, A KEY, B1 KEY; 30–41 filter env + FREQ RESO TYPE DEPTH DELAY KTRK BASE WIDTH; 42–46 amp ADSR + LEVEL; 47–70 three LFOs ×8 (SPEED MULT FADE DEST WAVE PHASE TRIG DEPTH: `lfo(k,f)=47+8k+f`); 71 RATIO B2, 72 B2 KEY, 73 DRIVE; 74–85 E values (env MODE, HOLD, PORT, TIME, SLOPE, drive ROUTE, SHAPE, GAIN, crush ROUTE, BITS, SRR, chorus ROUTE); 86–109 eight MOD slots (SOURCE DEST DEPTH: `mod(k,f)=86+3k+f`); 110–114 VOICE page (POLY/MONO, UNISON, DETUNE, SPREAD, PRIORITY). Rules: `int`, `ofs`, `fix2`/`fix2s` (hundredths, hi-res), `list`. Indices `Q` in fizz2dn.js:22.
- **Blob** (`fizzValues` fizz2dn.js:35, `quadPack` dn2pst.js:477, `quadPack6` :460): byte0 'Q' 0x51, byte1 version (5 → 96 B, 6 → 128 B). From byte 2: values 0..72 as `value − min`, one byte each, the 12 hi-res ids `[4,5,34,35,37,40,47,54,55,62,63,70]` as u16 LE (85 bytes, 2..86). Byte 87 DRIVE (v5: ignored by `sameSound`). v6 only: 88..123 values 74..109 (`value − min`, 1 byte); 124 = VOICE (bit0) | (PRIORITY−1)<<1; 125 UNISON−1; 126 DETUNE−20; 127 SPREAD−64. A MIDI-track sound is a 'Q' 6 blob with "MIDI" in 124..127, bytes 2..29 slot values (|0x80 touched), 30..57 CC numbers (sound.js:55-73). Editor always sends v6 (`blob6` → STORE6).
- **Name/tags/mix** travel in the STORE header, not the blob (pan −64..63, sends 0..127, 7-bit tag mask BASS KEYS PLUCK PAD LEAD DRUMS FX).
- **Reaching the unit:** TRACK on + followed track: each change → `L.q.set(id, v)`; `pump()` sends `PARAM(track, id, value, ack=1)` one at a time awaiting ACK (latest value per id wins); MIDI sound → whole sound as STORE6 AUDITION (flags LOAD|AUDITION, track, extras {id, sound, edited:true}). Track read back by DUMP (src 4) every 500 ms, ignored ≤700 ms after the last PARAM. **AUDITION** = STORE/STORE6 with LOAD|AUDITION (+ 8 extras bytes) onto a track, nothing stored. **CANCEL / REVERT** = `trackFrame(dumpBytes)` fizz_sysex.js:225: the track's DUMP with DUMP flag cleared and AUDITION|LOAD set, checksum recomputed, sent back. SAVE = STORE6 with OWN (+LOAD if live), slot n or 0.
- **Keyboard/notes:** 27-key keybed (F3 = MIDI 53 at OCT 0, octave −2..+2), PC keys A row (white) / W row (black), Z/X octave; FM-1 track target sends raw Note On `9n nn 64`/Note Off `8n nn 00` on the track's channel (`extras.chan`, default 14 = MIDI ch 15); browser target sends the same into the wasm (ch 14).

## 6. BROWSER unit

`BrowserUnit` (browserunit.js) = `SimFizz` subclass whose engine is `Player` (js/audio.js): `site/fizz.wasm` (FIZZ core built with Emscripten from FM1VST's core-glue, `site/build_wasm.sh`; `engine/engine.js` drives `fmw_*` exports) running in an **AudioWorklet**, sound out of the computer. It exposes a fake `MIDIAccess` (one in/out named "FM-1 (FIZZ in this browser)", id `FM-1_46999999`), so `findFizz`/`FizzLink` and every tab work unchanged. Semantic SysEx goes to the wasm's own handler; the update handshake and FZBI/FZBR/FZSR are simfizz.js's model of app.c over the wasm's 1 MiB host flash converted to the device layout (`convertImage`), so backups move between browser and unit. Flash is persisted (debounced 1.5 s) to IndexedDB kv `browserFlash`. `BrowserFizz` is the lighter editor-only variant (fresh store each time).

iOS options (brief):
1. **WKWebView**: Safari/WKWebView support AudioWorklet (iOS 14.5+) and WebAssembly, and BrowserUnit needs no Web MIDI, so this part could run as-is in a WKWebView (needs AVAudioSession setup, a user gesture to start audio; no SharedArrayBuffer used — verified 0 hits in host.js/engine.js/audio.js). Same trick could host the whole Manager with a native CoreMIDI bridge injected as a fake `MIDIAccess` (an alternative to a full SwiftUI port).
2. **Native**: link the FIZZ core C sources (the same FM1VST core ABI that `wasm_glue.c` wraps) into the app, render with AVAudioSourceNode, feed SysEx as USB-MIDI packets (`usbPackets`), and port simfizz's ~240-line bulk model to Swift. More work, best latency, no JS.
Licence flag: the core and the whole Manager are GPL-3.0-only; App Store distribution of GPLv3 code needs the owner's own licensing decision (fm1lib's MPL parts are third-party).

## 7. Existing tests (golden-vector sources)

| Test | Covers | Run |
|---|---|---|
| `site/tools/test_manager.mjs` (701 lines, GPL) | fizzlink against SimFizz (real wasm FIZZ + bulk model): connect/refusals, LIST/DUMP vs fizzstore, backup (pipelined, drops, autosave), .fizzbak round trip, restore (FZER 10/11/12, unplug), Phase 1 commands, sound.js round trips through real FIZZ, P2-B PARAM/AUDITION/trackFrame/BROWSE, MIDI sounds, import/export, projects model, owner's saves (`../FIZZ-saves`, optional) | `node site/tools/test_manager.mjs` (Node ≥18; needs committed `site/fizz.wasm`; no browser) |
| `site/tools/test_manager_page.mjs` (775) | whole page in headless Chromium via DevTools on `?sim` | `node site/tools/test_manager_page.mjs` (Chromium; see test_site.mjs flags) |
| `site/tools/test_engine.mjs` (655) | wasm engine in Node incl. SysEx via midiIn/midiOut | `node site/tools/test_engine.mjs` (CI: pages.yml) |
| `site/tools/test_install.mjs` (166, MPL) | fm1lib/fizz_data.js vs a fake FM-1 (update framing, FZXD data push, FZOK/FZER), releases.json | `node site/tools/test_install.mjs [FIRMWARE_DIR]` (CI) |
| `site/tools/test_site.mjs`, `test_update.mjs`, `test_dn2wizard.mjs` | player page / deploy / wizard | node, headless Chromium |
| `tools/test_fizz_sysex.mjs` (115, MPL) | `tools/fizz_sysex.mjs` STORE frames fed to the C core | `make -C tools test-host` or `node tools/test_fizz_sysex.mjs tools/build/test_sysex` (C test `core/tests/test_sysex.c`, `--feed`) |
| `tools/test_sysex_dnsnd.mjs` (58) | .dnsnd → STORE → C core end to end | `node tools/test_sysex_dnsnd.mjs DIR tools/build/test_sysex [N]` (owner's presets, optional) |
| `tools/test_dn2pst.mjs` (194), `tools/test_fizz2dn.mjs` (223) | DN2 reader, Digitone writers (LZ4 byte-exact) | `node tools/test_dn2pst.mjs [DN1_DIR] [DN2_DIR]`, `node tools/test_fizz2dn.mjs [DN1_DIR] [DN2_DIR]` |
| `firmware/tools/test_emu.mjs` | firmware emulator wasm | `node firmware/tools/test_emu.mjs app.wasm OUTDIR` |

Golden vectors: every pure module is plain ESM and imports directly in Node (e.g. `node script.mjs` importing `site/js/fizz_sysex.js`, `site/install/fm1lib.js`, `site/manager/fizzlink.js` for `bulkFrame/parseBulk`, `fizzstore.js`, `projects.js`, `sound.js`); verified by generating the sample frames quoted in §0/§3. For full conversations (request/reply transcripts incl. backup/restore), wrap `SimFizz` (as test_manager.mjs does: `new SimFizz({createEngine, module, frameMs:0, tryMs:150}).boot()`) and log `output.send` / `onmidimessage`.

## Notable quirks for the port
- Two pack7 variants (§0). Checksum of semantic frames excludes the protocol byte.
- `digitone.js storeFrame` loads only tracks 0..5; `storeSound` accepts 0..7.
- `library.js soundFromStoreFrame` rejects bodies with extras (track DUMPs) — the Manager uses `sound.js soundFromFrame` for those.
- manager.js header comment says depth default 1; code default is 2 (`DEPTH`, :34).
- Restore and PROJECTS WRITE always keep a safety `.fizzbak` in the backups store first.
