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
- **Frame size.** A frame may be up to 640 bytes; FIZZ's STORE frame is 140.

## Commands

### `01` IDENTITY

Request: `F0 7D 46 5A 01 01 7F F7`

Reply: `F0 7D 46 5A 01 41 01 <ASCII> <chk> F7`

- `01` is the protocol version.
- `<ASCII>` is the version string, for example `FIZZ 0.6.4-p1.5r`. It is up to about 40 characters, with no terminator: it runs up to the checksum.

### `10` STORE SOUND

The body has a fixed length of 132 bytes, then `chk`.

| offset | bytes | field |
| --- | --- | --- |
| 0 | 1 | **flags**: `01` POOL (else the LIBRARY), `02` BATCH, `04` LOAD |
| 1 | 2 | **slot**, 14-bit: high 7 bits, then low 7 bits. LIBRARY (MY presets) 1..256, POOL 1..128. **0 = the first free slot.** A slot given is overwritten. (0.6.x: LIBRARY 1..128, POOL 2..128.) |
| 3 | 12 | **name**, ASCII 0x20..0x7E, padded with spaces (or 0). Trailing spaces are dropped, other bytes become `?`, and an empty name becomes `IMPORTED`. |
| 15 | 1 | **drive**: FIZZ's DRIVE, 0..127. This is the `drive` of ChoralRoot's `dnsndToQuad`. |
| 16 | 1 | **track** 0..7, with LOAD; otherwise 0 |
| 17 | 1 | **pan** + 64 (0..127, 64 = centre), with LOAD and mixmask `01` |
| 18 | 1 | **chorus** send 0..127, with LOAD and mixmask `02` |
| 19 | 1 | **delay** send 0..127, with LOAD and mixmask `04` |
| 20 | 1 | **reverb** send 0..127, with LOAD and mixmask `08` |
| 21 | 1 | **mixmask**: which of pan, chorus, delay, reverb the track takes |
| 22 | 110 | **patch**: the 96-byte FM TONE blob, 7-bit packed |

**The patch** is ChoralRoot's FM TONE blob: `'Q'` (0x51), version 5, the values (`eng_quad.c` `quad_pack`), then zeros.

- `dnsndToQuad(parsed).blob` or a ChoralRoot sound file's `patch` is exactly this.
- An older version in 96 bytes is read the way ChoralRoot's `quad_unpack` reads it. Send version 5 (what `quadToV4` / `quadToV5` make).
- Byte 87, where FIZZ's own packing keeps DRIVE, is ignored. The **drive** field sets DRIVE.

Reply: `F0 7D 46 5A 01 50 <status> <0 LIBRARY / 1 POOL> <slot hi> <slot lo> <chk> F7`

- `<slot>` is where the sound went (1-based, as the panel numbers it), or 0 on an error.

| status | name | meaning |
| --- | --- | --- |
| 0 | OK | stored |
| 1 | FULL | slot 0 asked for, and no slot is free |
| 2 | BAD LENGTH | the body is not 132 bytes, or the patch does not unpack to 96 |
| 3 | BAD CHECKSUM | the sum is not zero (nothing is read) |
| 4 | BAD PATCH | not an FM TONE blob (FIZZ's `qd_quad_unpack` refuses it) |
| 5 | BAD SLOT | LIBRARY > 256, POOL > 128 |
| 6 | BAD VERSION | (error reply `7F 06 <protocol>`) |
| 7 | BAD COMMAND | (error reply `7F 07 <cmd>`) |

Any other command, or a frame whose checksum fails before its command is known, gets `F0 7D 46 5A 01 7F <status> <cmd> <chk> F7`. A STORE whose checksum fails gets a STORE reply with status 3.

## What FIZZ does with a sound

- **Stores it** in the LIBRARY slot or POOL number, with its name. A LIBRARY import becomes one of MY presets: its own
  preset id, its kind read from the sound (BASS, KEYS, PLUCK, PAD, LEAD, DRUMS or FX), marked imported (the ↓ in the
  browser). A POOL import gets its kind too.
  - A POOL sound overwritten in place: steps that lock that number play the new sound. Tracks keep their own copies, as when a pool sound is edited.
- **Shows it** for about a second, over any screen including the EDIT view: the cassette drops in with the name and slot number, in the LIBRARY's cream or the POOL's green, with "MY PRESETS" / "POOL" above and **imported** below.
- **Saves it** through FIZZ's usual autosave. The library and the project are written by themselves a moment later, as for any edit (`core/app/persist.c`).
- **Undo.** SHIFT + F1 undoes **one message**, or **one batch**.
  - A batch is the messages after the first, sent with BATCH (`02`) and arriving less than half a second apart. Each joins the first message's level.
  - A track's pan and sends set with LOAD are the project's track settings, which undo does not cover: undo takes the sound back, not the mix.
- **LOAD** (`04`, track 0..7) also puts the sound on that track in the pattern playing, the same as choosing it on the sounds page.
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
  // optional, onto track 3 with its mix (the Digitone's sends: parsed.params["send.reverb"] ..):
  // track: 2, pan: q.pan, delay: parsed.params["send.delay"], reverb: parsed.params["send.reverb"],
  // chorus: parsed.params["send.chorus"],
});
for (const p of usbPackets(frame)) engine.midiIn(p);
// then read engine.midiOutTake() packets back into bytes: parseReply(bytes) -> {kind: "store", status: 0, slot}
```

`tools/test_sysex_dnsnd.mjs` runs this for the owner's presets, which are never copied into this tree. It feeds each
frame to FIZZ and checks three things: the reply is OK, the stored name is the reader's, and all 73 stored FM TONE
values plus DRIVE equal `dnsndToQuad`'s values. All 256 presets in the folder pass.

### ChoralRoot's sound format compared with FIZZ's patch

- **The 73 FM TONE values: same order.** FIZZ's patch values 0..72 are ChoralRoot's `eng_quad.c` `QP_*` values, in the same order and the same units. The blob is ChoralRoot's own packing, so nothing is renumbered.
- **DRIVE.** FIZZ's value 73 is **DRIVE**. ChoralRoot keeps the drive in the sound *record* (`SND_P_DIST`, stored as drive + 64), not in the blob; its blobs have 0 in byte 87. So the message carries drive in its own field.
- **Pan.** Pan is in ChoralRoot's record (`SND_P_PAN`, pan + 64). In FIZZ it is a track setting, so it only applies with LOAD.
- **Macro bytes.** The record's macro bytes (`SND_P_E0..`, for example HARM and DTUNE coarse) are ChoralRoot's performance macros. FIZZ does not use them, because the blob has the exact values.
- **Sends.** The Digitone's reverb, delay and chorus sends are dropped by ChoralRoot's converter ("effects are global on the FM-1"). FIZZ has them per track: pass them with LOAD (they are 0..127 in both).

## Limits

- A frame waits for its `F7`. A partial frame followed by another status byte is dropped.
- At most 4 replies wait at a time; a host should read the reply before sending the next message. The web preview's engine runs `midiOutTake` every render, which is plenty.
- The identity request does not use the universal Identity Request (`F0 7E 7F 06 01 F7`), so that FIZZ does not claim a manufacturer ID.
