# FIZZ for iOS: a native Manager for iPhone and iPad (plan)

Status (2026-10-27): plan, mock-ups (`ios/mockups/`; the owner: "Mockups look good"), first slice (`ios/FizzKit/`,
`ios/FIZZ/`), branch `ios-app`. Nothing pushed, nothing written to a unit.

Checked on the real FM-1 from the Mac (FizzKit's CoreMIDI transport, read-only, 2026-10-27): the scanner found the
"FM-1" pair, the handshake round-tripped and the unit was classed correctly; it was running firmware `FM-1_52000201`
(not FIZZ: `FM-1_46…`), so no FIZZ reads were made. A port "FM-1 7938 Bluetooth" (a BLE MIDI pair of the unit?) was
also listed and did not answer the update handshake in 2 s: one input for I7. The full feature inventory of the web Manager, with file:line references,
is `ios/MANAGER-INVENTORY.md`; this plan condenses it.

## 0. Why

The web Manager (`site/manager/`) needs Web MIDI with SysEx. No iOS browser has Web MIDI (WebKit never shipped it,
and every iOS browser is WebKit), so an iPhone or iPad owner cannot back up, edit or import sounds. CoreMIDI on iOS
does SysEx, over USB (the FM-1 is class-compliant) and over Bluetooth LE MIDI. A native app gives them the Manager.

Goal: **the same features as the web Manager, the same protocol, the same files**, so a backup made on the phone
restores from the web page and the other way round. **No firmware change; nothing added to the protocol.**

## 1. Every Manager feature, its SysEx, and the iOS way

Semantic frames: `F0 7D 46 5A 01 cmd body chk F7` (`tools/fizz_sysex.mjs`, `core/app/fizz_sx.h`). Bulk: the update
framing with command byte 0x46 (`fm1lib.js` `frame`, `fizzlink.js` `bulkFrame`). W = writes to the unit, R = reads
only, L = local. "Same" = the iOS app does what the page does with the same calls (FizzKit's `FizzLink` mirrors
`fizzlink.js` method for method).

### 1.1 Connect and the header

| Feature | SysEx | | iOS |
| --- | --- | --- | --- |
| Find the unit | update handshake `F0 00 32 45 00 00 00 40 7F F7` → id "FM-1_46…" (OTA: "ota-…" = update mode; anything else: not FIZZ) | R | `MIDIPortScanner` probes every candidate port pair, any name (§2.1); several units → a picker |
| Identify | IDENTITY 01 → 41 "FIZZ 0.12.0-beta.11" (no answer: safe mode / no data blob → backup and restore only) | R | same |
| Read the unit | FZBI (store lo, size, CRC) · LIST 21 (factory + pool) · STATUS 02 · PROJECTS 23 · DUMP 22 mine 0 / factory 0 (tile art) · DUMP track (TRACK capable?) | R | same, on connect, pull-to-refresh, and on `scenePhase == .active` (the page's window-focus re-read: STATUS first, full re-read only if `written` / counts / project changed) |
| Plug / unplug | port state change → re-attach after 800 ms; lost → offline | L | CoreMIDI notify block (object added / removed), same delays |
| TRACK switch (live) | DUMP track (src 4) to enable; then edits and auditions go to the unit's track | R | the pink TRACK pill in the top bar on LIBRARY / EDIT / IMPORT; "seen" hint flag in `UserDefaults` |
| BROWSER unit (wasm FIZZ, heard on the computer) | the same SysEx, into a simulated unit | L | later, optional (§4.7) |
| LOG | last 400 lines | L | a LOG sheet (shake or long-press on the unit pill), copyable for bug reports |

### 1.2 BACKUP (yellow)

| Feature | SysEx | | iOS |
| --- | --- | --- | --- |
| THIS FM-1 card: version, projects, presets, store used, the meter | STATUS, LIST | R | same |
| ● BACK UP (whole store, CRC-checked, + every MY preset) | FZBI, FZBR off 256 (2 in flight, 16 KB pieces, resend overdue), CRC-32 vs FZBI; LIST; DUMP mine 0 (+ missing slots one by one) | R | same logic in Swift (`FizzLink.backup`); the `.fizzbak` ZIP made by the JS `makeFizzbak` (§3); screen kept awake |
| PRESETS ONLY | LIST, DUMP mine 0 | R | same |
| Backups kept (IndexedDB `fizz-manager/backups`) | — | L | `Documents/Backups/*.fizzbak` + `index.json` (date, kind, counts, safety, fizz version); visible in the Files app (`UIFileSharingEnabled`, `LSSupportsOpeningDocumentsInPlace`) |
| ↓ save, × delete per row | — | L | share sheet (`ShareLink`: Files, AirDrop, Mail) · swipe to delete (warns if never shared) |
| Open a file (.fizzbak .fizzlib .bin) | — | L | `.fileImporter`, "Open in FIZZ" from Files / AirDrop (`onOpenURL`, exported UTTypes), iPad drag and drop |
| RESTORE whole store | safety backup first; FZSR len crc (resent ≤ 3 if not taken up); answer the unit's 0x30 read requests (`response`), two passes (check, write); FZOK / FZER 10–14 | **W** | same, in Swift; confirm by **slide to restore** (the web's typed RESTORE); progress CHECKING → WRITING · DO NOT UNPLUG; cancel only in the check pass |
| ADD presets (a .fizzlib, a presets backup, another store size) | DUMP mine 0 (skip duplicates), STORE/STORE6 slot 0, BATCH | **W** | same |
| ▶ play a backup (the web player) | — | L | later, with the BROWSER unit (§4.7) |

### 1.3 LIBRARY (cream)

| Feature | SysEx | | iOS |
| --- | --- | --- | --- |
| MY PRESETS n / 256, FACTORY 23; tile art | LIST, DUMP mine 0 / factory 0, DUMP one (missing) | R | `List` of rows; MY / FACTORY segmented on iPhone, two columns + detail on iPad |
| Filters: ALL / FACTORY / MINE, IN POOL / NOT, 7 tags | — | L | chips row |
| Pick a preset (TRACK on: heard on the track, KEEP / CANCEL) | DUMP track; STORE LOAD+AUDITION (+ extras); CANCEL = `trackFrame(dump)` | W (track only, nothing stored) | tap a row; KEEP / CANCEL bar above the tab bar |
| Rename | RENAME 13 (mine) | **W** | tap the name in the preset sheet |
| FIZZ tags | TAGS 12 | **W** | tag toggles in the sheet |
| Web tags (free text) | — | L | same, kept in `Documents/webtags.json`, written into backups |
| + POOL / − POOL | POOL REF 16 slot 0; DELETE 14 pool (IN USE → confirm) | **W** | swipe right on a row / sheet button |
| USED BY | USERS 17 → 57 | R | sheet chips with step dots |
| DELETE | DELETE 14 mine, confirm | **W** | swipe left / sheet, confirm alert |
| Reorder | SWAP 15 mine | **W** | long-press drag (`.onMove`) |
| LOAD ON FM-1 (the unit's own browser opens on it; OCT+ on the unit loads) | BROWSE 31; STATUS polled 0.6 s until `browse` clears; DUMP track | W (unit UI) | sheet button with the six track buttons |
| EDIT | (as pick) | L / W track | sheet button → EDIT tab |
| EXPORT one / EXPORT ALL | DUMP if missing | R + file | share sheet with the format cards (§1.7) |
| Computer keys play the sound | Note On/Off on the track's channel (`extras.chan`, default 15) | W (notes) | on-screen keybed strip (pull up), hardware keyboard on iPad (`onKeyPress`, same key map) |

### 1.4 EDIT (pink)

| Feature | SysEx | | iOS |
| --- | --- | --- | --- |
| All 115 values (OPERATORS, OP ENV, PLAY, FILTER, AMP, DRIVE·CRUSH, LFO ×3, MOD ×8, VOICE); MIDI sounds: 28 CCs | — | L | page chips; drag on a card or knob, the turned value takes the screen while the finger is down (the FM-1's own rule); double-tap = default |
| Follow the unit's track (SELECTED / 1..6), its REC mark | DUMP track every 500 ms (skipped ≤ 700 ms after a PARAM) | R | same |
| A value turned, TRACK live | PARAM 30 with ACK, one at a time, latest value per id wins; MIDI sound: the whole sound as AUDITION | W (track) | same pump in Swift (`ParamPump`); haptic tick per step |
| REVERT | `trackFrame(base)` | W (track) | same |
| SAVE / SAVE AS NEW | STORE6 OWN slot n / 0 (+ LOAD if live) | **W** | same, confirm on SAVE over |
| Open a file in the editor | — | L | from IMPORT rows or Files |
| Keybed (27 keys, OCT ±) | Note On/Off | W (notes) | full-width keybed at the bottom (iPad), a swipe-up strip (iPhone) |

### 1.5 POOL (green)

| Feature | SysEx | | iOS |
| --- | --- | --- | --- |
| Project picker (load another: undo cleared) | PROJECT 24 load | **W** | chips + confirm |
| 128 places, tag colours, step dots | LIST (pool), USERS 17 per place | R | 8-column grid (iPhone), 16 × 8 (iPad) |
| Put a sound in a place | POOL REF 16 slot k | **W** | pick a sound in the SOUNDS drawer, tap a place; or drag (iPad) |
| Move / swap places (locks follow; stopped only) | SWAP 15 pool (a job) | **W** | long-press drag tile onto tile, confirm |
| CLEAR / CLEAR UNUSED / SORT BY TAG | DELETE 14 / n × DELETE / n × SWAP | **W** | bottom bar buttons, confirm |

### 1.6 PROJECTS (cyan)

| Feature | SysEx | | iOS |
| --- | --- | --- | --- |
| 8 slots, names, current, demo, use bar | PROJECTS 23 | R | **first slice** (list) |
| Read the patterns (32-dot previews) | FLUSH 03 if dirty and stopped; FZBI + FZBR (whole store, not kept) → `projects.js modelOf` | R | same; the model built by the JS in JSC (§3) |
| LOAD / RENAME / COPY TO / MOVE TO / DELETE | PROJECT 24 load/copy/delete, RENAME 13 src 3 | **W** | action row + tap a target slot, confirms |
| Pattern grid A01–D16: select, COPY, PASTE, CLEAR, UNDO, DISCARD | — (queued locally) | L | tap / drag-select; Edit menu + iPad ⌘C ⌘V ⌘Z |
| .fizzpat / .fizzprj out and in, KEEP, KEPT list, LOAD INTO | — | L | share sheet / file importer; kept projects in `Documents/Projects/` |
| WRITE TO FM-1 (the edited store image, safety backup first) | STATUS (refuse if playing); FZSR + 0x30 serving → FZOK | **W** | same as RESTORE |

### 1.7 IMPORT (violet) and EXPORT

| Feature | SysEx | | iOS |
| --- | --- | --- | --- |
| Read .fizzlib .fizzsnd .fizzbak .dnsnd .dn2pst .dn2snd; fidelity bar, notes, duplicates | DUMP mine 0 (duplicates) | R + L | the import sheet; files from `.fileImporter`, "Open in FIZZ", iPad drop; conversion by the JS readers in JSC |
| Audition a row (TRACK on) | STORE AUDITION, KEEP / CANCEL | W (track) | ▶ per row |
| ↓ SEND n TO FM-1 → MY PRESETS / POOL / TRACK | STORE / STORE6 slot 0, BATCH after the first, tags | **W** | same |
| EXPORT: .fizzsnd / .fizzlib / .dnsnd / .dn2pst / .dn2snd (Digitone experimental; several → a ZIP) | — | L | format cards in a sheet, then the share sheet |

## 2. Transport on iOS

### 2.1 USB

- iPhone 15 and later and USB-C iPads: a USB-C cable. Lightning devices: the Lightning to USB 3 Camera Adapter (with
  power in; iOS refuses a device that draws too much from the plain adapter).
- CoreMIDI sees the FM-1 as a source/destination pair named by the unit's USB descriptor ("FIZZ FM-1", "FM-1") or a
  name the host cached from an earlier firmware ("ChoralRoot FM-1"); one unit can show up as two pairs (seen on macOS
  2026-10-27). The app trusts no name: it probes every candidate pair (`/fm-?1|felucca|ota|composite|sinco|usb-midi|
  fizz|choralroot/i`, as `Updater.find` plus FIZZ's names) with the handshake, and de-duplicates by CoreMIDI device.
- API: `MIDIClientCreateWithBlock` (setup changes), `MIDIInputPortCreateWithProtocol(._1_0)` (`MIDIEventList` in),
  `MIDISendEventList` (out). SysEx travels as UMP type-3 (SysEx7) packets, 6 bytes each; `FizzKit/UMP.swift` packs
  and reassembles them, unit-tested without CoreMIDI. Notes are UMP type-2 (MIDI 1.0 channel voice).
- The unit's rules hold exactly as in the browser (`fizzlink.js`): **one request outstanding**; **≥ 3 ms between two
  frames sent** (fm1-base keeps one inbound SysEx frame, a second within ~2 ms is dropped; the page uses Web MIDI
  timestamps, the app an async clock); **reads retried** only when nothing of the reply came (the owner's unit lost
  ~1 request in 4 on a busy bus); **a quiet link woken by a STATUS** before a command that changes something; the port
  **always read** (a unit whose replies are not read stalls).
- iOS suspends a backgrounded app within seconds. Backup / restore / WRITE keep the screen awake
  (`isIdleTimerDisabled`) and take `beginBackgroundTask` time only to stop cleanly. A restore interrupted in its check
  pass changes nothing (FZER 12); interrupted in the write pass it is the web page's closed-tab case (FZER 13: FIZZ
  clears its store): the safety backup is the way back, and the screen says DO NOT LEAVE while writing.

### 2.2 Bluetooth LE MIDI (for FIZZ's Bluetooth build)

- Pairing: CoreAudioKit's `CABTMIDICentralViewController` in a sheet from the connect screen's Bluetooth button. A
  connected BLE MIDI peripheral is an ordinary CoreMIDI source/destination, so the same scanner, probe and link run
  over it; nothing else changes. (Advertising from the app is not needed: the FM-1 is the peripheral.)
- Speed: BLE MIDI moves roughly 3–10 KB/s in practice (7.5–15 ms connection interval, SysEx split over packets).
  LIST, DUMP, STATUS, PARAM, AUDITION are fine. A full backup (~300 KB region) would take ~40 s–2 min, a restore twice
  that (the unit pulls the image twice). Over BT the unit pill shows BT, BACKUP offers PRESETS ONLY first, and a full
  backup / restore asks for the cable.
- Pacing constants (gap, pipelining depth, timeouts) are per transport in FizzKit. To settle with the Bluetooth
  firmware session: its MTU, whether its inbound SysEx keeps the one-frame rule, its port name.

## 3. What to reuse: three ways

The Manager is ~6,000 lines of JavaScript in three kinds: the protocol and link (small, timing-critical); the pure data
logic (big, evolving with the firmware: the store image reader/writer `fizzstore.js`, `projects.js` pattern copy and
the store image's writer, `.fizzbak` / `.fizzlib` / `.fizzprj` / `.fizzpat` ZIPs, `sound.js`, the Digitone readers
`fm1dnsnd.js` / `dn2pst.js` / `digitone.js` and writer `fizz2dn.js`); the UI (DOM, not reusable).

| | A. WKWebView shell | B. Native UI, Swift link, JS data logic in JavaScriptCore | C. All Swift |
| --- | --- | --- | --- |
| What | the Manager page in a `WKWebView`; `navigator.requestMIDIAccess` faked by a script-message bridge to CoreMIDI | SwiftUI screens; FizzKit (Swift) for SysEx, link, CoreMIDI; the pure JS modules bundled into one script run in a `JSContext` | SwiftUI and Swift ports of everything |
| Parity on day one | full | protocol at once; files as each module is bridged | slow: every module rewritten |
| Feels native on a phone | no (a desktop page in a box) | yes | yes |
| Keeps up with the web Manager | automatically | data logic automatically (same source, rebuilt); screens by hand | by hand, every change twice |
| Risks | the link's timing through a message bridge (no timestamped sends); file pickers / downloads in WKWebView; App Review "a web page in an app" | the JS ↔ Swift seam (`Uint8Array` ↔ `Data`, promises); JSC lacks `TextEncoder/Decoder`, `atob/btoa`, `setTimeout`, `DecompressionStream`, and has no JIT / WebAssembly on iOS (small polyfills; inflate from Apple's Compression framework, whose ZLIB is raw deflate) | two implementations of the store format, the Digitone maps and the pattern walker drifting apart |

**Recommendation: B.**
- **Protocol and link in Swift** (FizzKit, built in this slice): small, timing-critical, and checked byte-for-byte
  against the JS by generated vectors. `fizzlink.js`'s rules are ported as they are.
- **Data logic stays JavaScript, run in JavaScriptCore**: `fizzstore.js`, `fizzbak.js`, `projects.js`, `sound.js`,
  `fizzvalues.js`, `library.js`, `digitone.js`, `fm1dnsnd.js`, `dn2pst.js`, `fizz2dn.js`, and the pure halves of
  `importer.js` / `exporter.js` / `presets.js`. One source of truth for formats that change with the firmware (the
  'P' record walker, MIDI sounds, the store layouts). A build script (`ios/jsbundle/build.mjs`, plain Node, no
  esbuild here) bundles them into `FizzData.js` in the app; a test runs the same calls in Node and in JSC and compares.
- **Screens in SwiftUI**, as the mock-ups. Port a JS module to Swift only when the bridge costs more than the port.
- Not A: cheapest parity, but a desktop page on a phone, and the link's timing through a bridge. A WKWebView is still
  the right home for the optional BROWSER unit (§4.7).

**Licence (the owner decides before any App Store build).** New iOS code is MPL-2.0 like the firmware. FizzKit ports
`tools/fizz_sysex.mjs` (MPL-2.0) and fm1-base's framing (MPL-2.0). But the modules B would bundle are copies in
`site/`, which is GPL-3.0-only (only `fm1lib.js` there is MPL), and GPLv3 code on the App Store is a known conflict
with Apple's terms. The owner wrote those files; relicensing the bundled `site/js` and `site/manager` data modules to
MPL-2.0 (as `tools/` already is) makes it clean. The same applies already to FizzKit's `FizzLink.swift`: it ports
the logic of `site/manager/fizzlink.js` (GPL-3.0-only) into an MPL-2.0 file, which is fine only because the owner
wrote both and says so. TestFlight and App Store also need an Apple Developer account,
a bundle id and the app name.

## 4. The app

### 4.1 Layout
- `ios/FizzKit/`: Swift package (iOS 17+, macOS 14+ so `swift test` runs on the Mac): `SysEx.swift`, `Bulk.swift`,
  `UMP.swift`, `CoreMIDITransport.swift` (transport + `MIDIPortScanner`), `FizzLink.swift` (actor). No dependencies.
- `ios/FIZZ.xcodeproj` + `ios/FIZZ/`: the SwiftUI app (iPhone + iPad, one target), depends on FizzKit by local path.
- Later: `ios/jsbundle/` (the bundle script and its test), `ios/FIZZ/Data/` (JSC bridge).

Build and check (I1):
- `cd ios/FizzKit && swift test`: FizzKit (80 tests: vectors, UMP, link, writes, restore, live) + FizzUI (23: `UnitModel`,
  backups, `UnitActions` against `DemoFizz`) + FizzData (10).
- `cd ios && xcodebuild -project FIZZ.xcodeproj -scheme FIZZ -destination 'generic/platform=iOS Simulator' build`.
- `cd ios/FizzKit && swift run fizzshots <dir>`: the screens rendered to PNG with demo data on the Mac (no window, no
  simulator). The app runs on the demo unit with the launch argument `-demo` (`-demo update|notfizz|two`).
- `node ios/FizzKit/Tests/gen_vectors.mjs` after any change to `tools/fizz_sysex.mjs` or `fm1lib.js`.
- On the iOS simulator (iOS 27 runtime installed 2026-10-27): `ios/sim_test.sh [UDID]` builds the test bundles, runs
  them in the simulator with `simctl spawn … xctest` (43 pass on an iPhone 17), then installs and launches the app on
  the demo unit; `-tab library|projects|…` opens a tab, `xcrun simctl io <udid> screenshot` captures it. Boot with
  `xcrun simctl boot <udid>` only (Simulator.app stays closed). Not `xcodebuild test`: this Mac's Xcode has its default
  toolchain overridden to a 2024 Swift snapshot, whose runtime xcodebuild injects into the test process, which then
  aborts at launch.

FIZZ's Bluetooth package (firmware branch `ble`, on the owner's unit 2026-10-27; ports "ChoralRoot FM-1" on USB,
"FM-1 C1F0 Bluetooth"): 4 projects and 128 MY presets; store region 0xAF000..0xDFFFF (49 sectors, magic "Z2FB",
FZBI reports it); STATUS ends with `01 04 01 00` (package, projects, presets); over Bluetooth FZSR and FZXD answer
FZER 15 (need USB). The app: connect, LIST, STATUS, PROJECTS work as they are; a full BACK UP fails at
"NOT SAVED" until the web Manager's `ble-site` changes (fizzstore.js LAYOUTS) are merged into `site/` and
`node ios/jsbundle/build.mjs` rebuilds FizzData.js; then show "n / 128" and 4 project slots from STATUS's tail, and
offer RESTORE (I4) only over USB.

First run on the owner's iPhone 13 Pro Max (Lightning, iOS via the Camera adapter), 2026-10-27, FIZZ's Bluetooth
package on the unit: CoreMIDI listed only "Network Session 1": the FM-1 did not appear as a USB MIDI device at all
(below the app). Open: power through the adapter, or iOS refusing the unit's USB descriptors (iOS is stricter than
macOS about class-compliant MIDI / audio descriptors); try the RemoteFM1 unit on the same adapter.
**Bluetooth works** (same evening): paired from the app's Bluetooth button (CABTMIDICentralViewController), port
"FM-1 C1F0 Bluetooth". FIZZ does not answer the update handshake over Bluetooth, so the probe falls back to FIZZ's
IDENTITY; then connected: FIZZ DEV, 0 MY presets (empty store), 21 factory, read over BLE MIDI. Relaunching the app
drops the BLE MIDI link (reconnect from the Bluetooth button).

I1 follow-ups: while nothing answers, `UnitModel` re-probes every 2 s (a handshake to each candidate port, a
never-answering BLE port included); slow it to port changes + ~5 s before shipping. `FizzSysEx.frame` and a public
`FizzStatus` init would let `DemoFizz` drop its own frame helper.

### 4.2 Screens (mock-ups: `ios/mockups/png/`)
iPhone: a floating bottom bar of five coloured pills (BACKUP, LIBRARY, EDIT, POOL, PROJECTS) and a violet "+" for
IMPORT (a sheet; files arriving from Files / AirDrop open it too). Top: the bubble logo, the unit pill (dot, version,
USB/BT), the TRACK pill where it applies. iPad: a left rail with the unit card and the six tabs; LIBRARY and EDIT as
the web Manager's multi-column layouts. Look: the web Manager's black and neon, Inter Tight, tag colours, one big
subject a screen, nothing under 12 pt, no hint text; motion everywhere it is cheap (bubbles while searching, meters
filling, the turned value taking the screen).

### 4.3 State
One `@Observable` `UnitModel` per connected unit (identity, status, list rows, dumps, projects, store info), fed by
`FizzLink`; views read it. Writes go through one `UnitActions` layer that re-reads what the page re-reads after each
command (LIST / STATUS / DUMP), so the screens stay as truthful as the page's.

### 4.3.1 Write layer (built 2026-10-27, for I3..I6)

The writes the screens call, each the web Manager's own (the JS file:line it ports). Nothing here has run against a
unit: tests use MockFizz (FizzKitTests) and the demo unit (FizzUITests).

**FizzKit, `FizzLink` (`Writes.swift`)**: each op is fizzlink.js's (:221-263), with its timeout and tries; a write is
sent once (only PARAM again, once), a quiet link woken by STATUS first. FIZZ's no comes back as a value:
`FizzResult<V>` = `.ok(V)` | `.refused(FizzRefusal)`; `FizzRefusal` is fizz_sysex.mjs's STATUS (`.full`, `.busy`,
`.notFound`, `.inUse(projects:steps:)`, `.badTrack`, `.updateFizz` = BAD COMMAND, `.badSlot` .., `.other(n)`; `.name`
"BUSY"). Lost / silent: throws `FizzLinkError`.
- `tags(source:slot:tags:)`, `rename(source:slot:name:)`, `renameProject(_:name:)`, `remove(source:slot:confirm:)`,
  `swap(source:a:b:)`, `poolRef(slot:id:)`, `project(_:_:_:)` (30 s), `browse(track:id:)` (3 s) -> `FizzResult<FizzAck>`
- `flush()` -> `FizzResult<FizzStatus>`; `usedBy(source:slot:)` -> `FizzResult<FizzUsers>`
- `store(_ frame:)` -> `FizzResult<FizzStoreReply>` (STORE / STORE6 / AUDITION / a track put back; 3 s)
- `param(track:id:value:)` (with ACK; 1 s, sent again once), `dumpTrack(slot:)` -> frame | nil, `note(channel:note:velocity:)`
- `viaBle`, `isBlePort(_:)` (fizzlink.js isBlePort)
- frames: `FizzSysEx.storeFrame(..)` (js/digitone.js storeFrame: IMPORT's SEND, ADD), `sameSound` (library.js),
  `presetName` (manager.js's name field), `cleanName` (projects.js); `FM1Bulk.readRequest` / `parseResponse` (the
  unit's side of a restore, for the demo and the mock). All checked against the JS (gen_vectors.mjs `writes`).

**Restore (`Restore.swift`)**: `restore(_ image:idleMs:progress:)` -> `FizzRestoreDone` | throws `FizzRestoreError`
(fizzlink.js restore :386): FZSR len crc (again after 3 s while nothing is pulled, 3 sends), each 0x30 read request
answered from the image in order, the check pass then the write pass (`progress(.checking | .writing, fraction)`),
FZOK / FZER (`.refused(fzer:written:)`, texts `FM1Bulk.fzer` 1, 10-15), `.cancelled` (task cancelled in the check
pass: zeros served, FZER 12, nothing erased; ignored while writing), `.timeout` / `.lost(written:)`. Over Bluetooth:
FZER 15 at once. The same call serves PROJECTS' WRITE TO FM-1.

**Live (`Live.swift`, live.js)**: `ParamPump(link:clock:)`: `setTrack`, `setOn` (off drops the queue), `param(id:value:)`,
`sound(_ frame:)` (a MIDI sound's AUDITION in the same queue), `clear`, `touch`, `followBlocked()` (the EDIT screen's
500 ms track read-back skips while busy and ≤ 700 ms after a PARAM: `followQuietMs`), `lastParamAt`, `pending`,
`idle()`, `setOnOld` (BAD COMMAND: the pump turns off), `setOnRefused`. One PARAM in flight, the latest value per id
(live.js pump :149). `TrackAudition(link:)`: `readTrack(slot:)`, `pick(preset:id:name:trackSlot:)` (:221),
`play(name:trackSlot:frame:)` (pickSound :240), `keep()`, `cancel()` (the track's DUMP read before, sent back with
`trackFrame`: :268), `revert(to:)`, `pending` (track, name, own); an AUDITION whose reply is lost is checked on the
track before it is sent again (sure :256).

**FizzUI, `UnitActions` (`model.actions`)**: each write, then the page's re-reads (`refreshRows`: LIST until it shows
the edit, 4 tries 0.3 s apart, STATUS, PROJECTS; a preset's DUMP again: manager.js :769, :840), UnitModel updated,
a LOG line, and a `UnitOutcome` (`.ok`, `.placed(n)`, `.refused(FizzRefusal)`, `.needsConfirm(place:projects:steps:)`,
`.noAnswer`, `.notNow`; `.text(what)` = ui.js refusal). State: `editing`, `busy`, `job`, `frames` (each row's DUMP),
`users`, `restoreRun` / `restoreFailure`, `browsing` / `browseLoaded`, `trackCapable`, `trackOn`, `trackSlot`, `pending`.
- LIBRARY (manager.js renderSel :929-1020, swapMine :1034): `rename`, `setTags` / `toggleTag`, `delete` (CONFIRM, after
  the screen's), `moveMine`, `addToPool`, `removeFromPool(_:confirmed:)` (IN USE: `.needsConfirm(place:)`, then again
  with that place), `usedBy`, `loadOnUnit` (BROWSE, STATUS every 0.6 s: live.js :280), `frame(of:)` / `fetchFrame(of:)`,
  `places(of:)`
- POOL (pool.js :100-270): `place(_:id:)`, `swapPool`, `clearPlace(_:confirm:)`, `sortedPool` + `sortPoolByTag`,
  `clearUnused`, `poolUsers`
- PROJECTS (pool.js :195, projtab.js :209-260): `loadProject`, `renameProject`, `copyProject`, `moveProject`,
  `deleteProject`, `flush`; `writeProjects(image:before:)` (projtab.js write :299 + manager.js writeStore :702:
  refused while playing, `before` kept as "FIZZ before projects ..", then the restore)
- BACKUP (manager.js restoreImage :648, addPresets :713): `restore(image:name:)` (safety backup first through
  `BackupModel.safetyBackup()`, kept as "FIZZ before restore .."; none: nothing restored), `cancelRestore()` (saving and
  checking only), `addPresets(_:)` -> `AddResult`
- IMPORT (importer.js send :109): `send(_:to: .mine | .pool | .track(t))` -> one outcome per sound, stops at FULL
- EDIT (manager.js saveSound :1172, live.js): `save(_:slot:track:)` (STORE6 OWN; 0 = SAVE AS NEW), `probeTrack`,
  `setTrackOn`, `pump`, `audition`, `readTrack`, `pick`, `pickSound`, `keepAudition`, `cancelAudition`, `revert(to:)`,
  `note`

Not ported (the page's, local): web tags (the IndexedDB `web` store), toasts, the confirm boxes (the screens ask, then
call). **DemoFizz** takes every write (`DemoWrites.swift`): its MY PRESETS, pool (one a project), projects, tracks
(PARAM sets the track's blob), the browser (closes itself after 2.5 s, loading), and the restore (FZSR, the pulls asked
again every second, the image read back into its presets and projects); `DemoUnit.setPlaying(true)` gives BUSY / FZER
10; a pool place's steps are made up (`DemoState.steps`: every third place).

### 4.4 Files
Exported UTTypes for `.fizzbak .fizzlib .fizzsnd .fizzprj .fizzpat` (conforming to `public.zip-archive` / `public.json`),
imported for `.dnsnd .dn2pst .dn2snd`. Never `.syx` (the owner's rule).

### 4.5 Write safety
Every write path is the page's, including its safety copies (restore and WRITE TO FM-1 keep a `.fizzbak` first), its
refusals (BUSY while playing, IN USE confirms) and its re-reads. Write phases are tried on a unit only with the owner's
word, one device session at a time, never alongside the firmware session's flashing.

### 4.6 Errors
The page's texts (FZER 10–14, BUSY, IN USE, FULL, NOT FOUND, BAD COMMAND = "update FIZZ") as short screen states
with a colour and a drawing, not paragraphs.

### 4.7 BROWSER unit (optional, later)
The wasm FIZZ cannot run in a `JSContext` on iOS (no JIT, no WebAssembly) but runs in a hidden `WKWebView` (WebKit has
WebAssembly and AudioWorklet since iOS 14.5). FizzKit's `MIDITransport` gets a second implementation bridged to that
web view's `SimFizz`, so every tab works offline and is heard on the iPhone, as the page's BROWSER pill. Alternative:
the core's C sources linked natively behind an `AVAudioSourceNode`. Decide after I6.

## 5. Phases

| Phase | What | Unit |
| --- | --- | --- |
| **I1** (this session) | FizzKit (SysEx, framing, UMP, CoreMIDI transport and finder, `FizzLink` read ops), golden-vector tests; the app shell: connect screen (search / found / update mode / not FIZZ / two units), unit pill, LIBRARY rows (LIST: MY PRESETS + FACTORY), PROJECTS list (PROJECTS), pull to refresh | read-only |
| I2 (built 2026-10-27) | BACKUP: whole-store read (FZBR pipelined) + PRESETS ONLY; the JSC bridge with `fizzbak.js` / `library.js` / `fizzstore.js`; backups in Files, share, open. A demo backup made in the simulator reads in the web Manager's `readBackup` (Node) with the same CRC, 143 presets, 6 projects | read-only |
| I3 (built 2026-10-28; tried on the owner's FIZZ FM-1) | LIBRARY writes (rename, tags, delete, reorder, pool ref, USED BY) and the POOL tab; project load | writes: owner's go |
| I4 (built 2026-10-28; tried on the owner's FIZZ FM-1) | RESTORE and ADD presets; IMPORT (Digitone + FIZZ files via JSC) and SEND; EXPORT | writes: owner's go |
| I5 (built 2026-10-28; tried on the owner's FIZZ FM-1) | EDIT and the TRACK switch: PARAM pump, AUDITION / KEEP / CANCEL, LOAD ON FM-1 (BROWSE), keybed notes | writes: owner's go |
| I6 (built 2026-10-28; tried on the owner's FIZZ FM-1) | PROJECTS patterns (model via `projects.js`), copy / paste / clear, kept projects, .fizzprj / .fizzpat, WRITE TO FM-1 | writes: owner's go |
| I7 | Bluetooth: the pairing sheet, per-transport pacing, measured on FIZZ's BT build | when it exists |
| I8 | (optional) BROWSER unit; TestFlight | owner's call |

## 6. Tests

**On the owner's FIZZ FM-1 (2026-10-28, Bluetooth package, FIZZ DEV, over USB from the Mac):** the app's own
`UnitModel` / `UnitActions` driven by a scratch harness (same code as the phone, CoreMIDI on macOS): full backup
(55 KB/s); SEND 3 to MY PRESETS, RENAME, TAG, MOVE, + POOL / USED BY / - POOL, PLACE, SWAP places, CLEAR place,
DELETE; TRACK on, AUDITION + CANCEL (the track's DUMP back byte for byte), PARAM through the pump (LEVEL 102 -> 31 read
back), REVERT (byte for byte), SAVE AS NEW; NEW / RENAME / COPY TO / DELETE project, LOAD; a pattern copied into the new
project and WRITE TO FM-1 (FZOK, read back); RESTORE of the first backup (safety backup, checking, writing, FZOK). All
passed. After the restore the store differs from the backup in one 256-byte page that was blank: a record FIZZ appends
itself after the restart (its store is log-structured), not the restore. Not tried on the unit: LOAD ON FM-1 (needs the
owner's OCT key), IMPORT of Digitone files (SEND is the same write), the phone's touch gestures.


- **FizzKit** (`cd ios/FizzKit && swift test`, on the Mac): `Tests/gen_vectors.mjs` runs the JS (`tools/fizz_sysex.mjs`,
  `site/install/fm1lib.js`, `fizzlink.js`'s bulk helpers, `fizzbak.js` crc32) over fixed cases and writes
  `vectors.json`; the Swift tests check every builder byte-for-byte and every parser field-by-field, UMP round trips,
  and `FizzLink` against a mock FIZZ (dropped first frames, LIST in parts, DUMP streaming, partial on timeout, one op
  at a time, the 3 ms gap). Re-run the generator whenever `fizz_sysex.mjs` changes.
- **Conversations** (I2+): record request/reply transcripts from `site/tools/test_manager.mjs`'s `SimFizz` (the real
  wasm FIZZ plus the bulk model: backup with drops and autosaves, restore with FZER) and replay them against `FizzLink`.
- **JSC bridge** (I2+): the same calls in Node and in JSC, outputs compared (store images, .fizzbak bytes, imports).
- **App**: builds for the simulator and runs on iPhone 17 / iPad Pro 11 simulators with the demo unit (`ios/sim_test.sh`).
  iPad portrait shows two columns and the tag chips overflow; the iPad layout is designed for landscape (fix in I2).
- **On the unit**: read-only checks (IDENTITY, LIST, PROJECTS) from the Mac through FizzKit's CoreMIDI transport (the
  same code as on iOS); the phone itself once the app runs on a device (needs signing).

## 7. Open questions for the owner

1. The mock-ups: tab structure (five pills + IMPORT "+"), slide to restore, the drag-takes-the-screen editor.
2. Licence: relicense the bundled `site/` data modules to MPL-2.0 (needed for an App Store build with option B)?
3. Distribution: an Apple Developer account, bundle id (e.g. `com.quixotic7.fizz`), name "FIZZ"? Minimum iOS 17?
4. May I download the iOS simulator runtime (several GB) to run the app and UI tests here?
5. Bluetooth: when the BT build will exist, and who measures its SysEx speed.
6. The BROWSER unit on iOS: wanted?
7. Installing FIZZ from the app: the mock-ups' update-mode / not-FIZZ screens show an INSTALL FIZZ button. The
   install page's update protocol (`fm1lib.js` `Updater`: the 0x30 read requests a restore also serves) would work over
   CoreMIDI, so iOS owners could install and update without a computer. Not in the phases unless you want it; it would
   be its own phase, tested only by the session that flashes.
