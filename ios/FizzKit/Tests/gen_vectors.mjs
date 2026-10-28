// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FizzKit's golden vectors: the JS's outputs for every builder and parser (tools/fizz_sysex.mjs,
// site/install/fm1lib.js, site/manager/fizzlink.js, fizzbak.js), written to FizzKitTests/Resources/vectors.json.
//   node ios/FizzKit/Tests/gen_vectors.mjs        (from the worktree root)
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as sx from "../../../tools/fizz_sysex.mjs";
import * as fm1 from "../../../site/install/fm1lib.js";
import { bulkFrame, parseBulk, FZER } from "../../../site/manager/fizzlink.js";
import { crc32 } from "../../../site/manager/fizzbak.js";
import { storeFrame } from "../../../site/js/digitone.js";
import { sameSound } from "../../../site/js/library.js";
import { cleanName } from "../../../site/manager/projects.js";

const here = dirname(fileURLToPath(import.meta.url));
// parseIdentity and HS_QUERY are not exported by fm1lib.js: the same source, imported again with them exported
const fm1src = readFileSync(join(here, "../../../site/install/fm1lib.js"), "utf8") +
  "\nexport { parseIdentity as __parseIdentity, HS_QUERY as __HS_QUERY, UPGRADE as __UPGRADE };\n";
const fm1x = await import("data:text/javascript;base64," + Buffer.from(fm1src).toString("base64"));

// mulberry32: fixed-seed bytes
let seed = 0x46495a5a;
const rnd = () => {
  seed = (seed + 0x6d2b79f5) >>> 0;
  let t = seed;
  t = Math.imul(t ^ (t >>> 15), t | 1);
  t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
};
const bytes = (n, max = 256) => Uint8Array.from({ length: n }, () => Math.floor(rnd() * max));
const arr = (u) => Array.from(u);
const replacer = (_, v) => (v instanceof Uint8Array ? Array.from(v) : v);
const js = (v) => (v === undefined ? null : JSON.parse(JSON.stringify(v, replacer)));

// ---- builders
const builders = [];
const build = (fn, args, call) => {
  try { builders.push({ fn, args, out: arr(call()) }); } catch (e) { builders.push({ fn, args, throws: true }); }
};
const blobs = { b96: bytes(96), b128: bytes(128), b96b: bytes(96), bad: bytes(95) };
const withBlob = (a) => ({ ...a, blob: blobs[a.blob] });

build("identity", {}, () => sx.identity());
const names = ["", "PIANO", "Grand Piano Extra Long", "Café ☕ bar", "🎹 Keys", "TAB\there", "~~~~~~~~~~~~~"];
const stores = [
  { blob: "b96" }, { blob: "b128" }, { blob: "bad" },
  ...[0, 1, 127, 128, 255, 256, 257, -1].map((slot) => ({ blob: "b96", slot })),
  ...[0, 1, 127, 128, 129].map((slot) => ({ blob: "b128", pool: true, slot })),
  ...names.map((name) => ({ blob: "b96", name })),
  { blob: "b96", name: "BASS", drive: 64, batch: true },
  { blob: "b96", drive: 200 }, { blob: "b96", drive: -3 },
  { blob: "b96", track: 0, pan: -64 }, { blob: "b96", track: 7, pan: 63 }, { blob: "b96", track: 8, pan: 0 },
  { blob: "b96", track: -1, pan: 100, chorus: 10 }, { blob: "b96", pan: -100, delay: 127, reverb: 300 },
  { blob: "b128", track: 3, chorus: 0, delay: 0, reverb: 0 },
  { blob: "b96", tags: 5 }, { blob: "b96", tags: 0x1ff }, { blob: "b96", tags: -1 }, { blob: "b96", tags: 0 },
  { blob: "b96", tags: ["bass", "PAD", "nope", "fx"] }, { blob: "b96", tags: [] },
  { blob: "b128", own: true, slot: 12, name: "MINE" },
  { blob: "b128", track: 2, audition: true },
  { blob: "b128", track: 2, audition: true, extras: { id: 300, sound: 129, edited: true } },
  { blob: "b128", track: 5, audition: true, tags: 3, extras: { id: 2097151, sound: 16383, edited: false } },
  { blob: "b96", audition: true, extras: { id: 7, sound: 1, edited: true } },
  { blob: "b96", track: 1, extras: { id: 7, sound: 1, edited: true } },
  { blob: "b96b", name: "FULL", slot: 200, drive: 5, track: 4, pan: -10, chorus: 20, delay: 30, reverb: 40, tags: ["KEYS"],
    batch: true, own: true },
];
for (const a of stores) build("storeSound", a, () => sx.storeSound(withBlob(a)));
for (const track of ["selected", 0, 5])
  for (const [id, value] of [[0, 0], [109, 127], [3, -1], [12, -1048576], [99, 1048575], [1, 1048576], [2, -5000000]])
    for (const ack of [false, true]) build("paramRequest", { track, id, value, ack }, () => sx.paramRequest({ track, id, value, ack }));
for (const track of ["selected", 0, 5]) for (const id of [0, 1, 300000, 2097151])
  build("browseRequest", { track, id }, () => sx.browseRequest({ track, id }));
for (const subscribe of [false, true]) build("labelsRequest", { subscribe }, () => sx.labelsRequest({ subscribe }));
for (const factory of [false, true]) for (const pool of [false, true])
  build("listRequest", { factory, pool }, () => sx.listRequest({ factory, pool }));
for (const source of ["mine", "factory", "pool", "track", "project"]) for (const slot of [0, 1, 127, 128, 255, 256])
  build("dumpRequest", { source, slot }, () => sx.dumpRequest({ source, slot }));
build("statusRequest", {}, () => sx.statusRequest());
build("flushRequest", {}, () => sx.flushRequest());
build("projectsRequest", {}, () => sx.projectsRequest());
const srcs = ["mine", "factory", "pool", "project", "track"];
for (const source of srcs) for (const slot of [1, 128, 256]) {
  for (const tags of [0, 127, ["LEAD", "drums"]]) build("tagsRequest", { source, slot, tags }, () => sx.tagsRequest({ source, slot, tags }));
  for (const name of ["", "NOIR", "Grand Piano Extra Long", "Ünïcødé"]) build("renameRequest", { source, slot, name }, () => sx.renameRequest({ source, slot, name }));
  for (const confirm of [false, true]) build("deleteRequest", { source, slot, confirm }, () => sx.deleteRequest({ source, slot, confirm }));
  build("swapRequest", { source, a: slot, b: 257 - slot }, () => sx.swapRequest({ source, a: slot, b: 257 - slot }));
  build("usersRequest", { source, slot }, () => sx.usersRequest({ source, slot }));
}
for (const [slot, id] of [[0, 0], [1, 1], [128, 300], [127, 2097151]]) build("poolRefRequest", { slot, id }, () => sx.poolRefRequest({ slot, id }));
for (const op of ["load", "new", "copy", "delete"]) for (const [p, q] of [[0, 0], [7, 0], [1, 5], [200, 300]])
  build("projectRequest", { op, p, q }, () => sx.projectRequest({ op, p, q }));

// ---- reply frames
const fz = (cmd, body) => {
  const b = [cmd, ...body];
  return [0xf0, 0x7d, 0x46, 0x5a, 0x01, ...b, (0x80 - (b.reduce((s, x) => s + x, 0) & 0x7f)) & 0x7f, 0xf7];
};
const ascii = (s) => [...s].map((c) => c.charCodeAt(0));
const name12 = (s) => ascii(s.padEnd(12, " ").slice(0, 12));
const r14 = (v) => [(v >> 7) & 0x7f, v & 0x7f];
const frames = [];
const add = (label, f) => frames.push({ label, f: arr(f) });

add("identity", fz(0x41, [1, ...ascii("FIZZ 0.12.0-beta.9")]));
add("identity bare", fz(0x41, [1, ...ascii("0.11.0")]));
add("identity empty", fz(0x41, []));
for (const [st, pool, slot] of [[0, 0, 1], [0, 1, 128], [1, 0, 0], [5, 0, 300], [12, 1, 2]])
  add(`store ${st}`, fz(0x50, [st, pool, ...r14(slot)]));
add("store short", fz(0x50, []));
add("error", fz(0x7f, [7, 0x22]));
add("error 13", fz(0x7f, [13, 0x30]));
add("error short", fz(0x7f, [3]));
// LABELS: 49 strings
const labels = Array.from({ length: 49 }, (_, i) => (i % 7 === 3 ? "" : `L${i}${i % 5 === 0 ? " LONG" : ""}`));
const lbl = (list, n = list.length) => [...r14(1234), n, ...list.flatMap((s) => [s.length, ...ascii(s)])];
add("labels", fz(0x60, lbl(labels)));
add("labels few", fz(0x60, lbl(["A", "BB", "CCC"])));
add("labels count mismatch", fz(0x60, lbl(["A", "BB"], 5)));
// LIST parts
const entry = (src, slot, id, nm, tags, fl) => [src, ...r14(slot), (id >> 14) & 0x7f, (id >> 7) & 0x7f, id & 0x7f, ...name12(nm), tags, fl];
const listPart = (part, last, total, es) => fz(0x61, [part, last ? 1 : 0, ...r14(total), es.length, ...es.flat()]);
const some = Array.from({ length: 16 }, (_, i) => entry(i % 3, i * 17 + 1, i * 1000 + 5, `PRESET ${i}`, i & 0x7f, i & 3));
add("list part 0", listPart(0, false, 35, some));
add("list part 1", listPart(1, false, 35, some.slice(3, 16).reverse()));
add("list part 2 last", listPart(2, true, 35, [entry(0, 256, 2097151, "LAST  ", 0x7f, 1), entry(5, 1, 1, "ODD SRC", 0, 2),
  entry(2, 128, 77, "", 64, 3), entry(1, 23, 9, "FACTORY 23", 1, 0), entry(0, 1, 1, "TRAIL\t ", 0, 0), entry(0, 2, 2, "x", 0, 0)]));
add("list empty last", listPart(0, true, 0, []));
add("list count over entries", fz(0x61, [0, 1, 0, 2, 3, ...entry(0, 1, 1, "ONLY", 0, 0)]));
// DUMPs: STORE / STORE6 frames with the DUMP flag
const dumpFrame = ({ six = false, flags = 0x08, slot = 1, nm = "DUMPED", drive = 10, track = 0, pan = 64, ch = 1, dl = 2, rv = 3,
  mask = 0, tags = null, extras = null, blob = null }) => {
  const b = blob || bytes(six ? 128 : 96);
  return fz(six ? 0x11 : 0x10, [flags, ...r14(slot), ...name12(nm), drive, track, pan, ch, dl, rv, mask, ...sx.pack7(b),
    ...(tags !== null ? [tags] : []), ...(extras || [])]);
};
add("dump mine", dumpFrame({ slot: 1 }));
add("dump mine 256 tags", dumpFrame({ slot: 256, tags: 0x11, nm: "TAGGED   " }));
add("dump pool", dumpFrame({ flags: 0x09, slot: 128, pan: 0, tags: 0 }));
add("dump six", dumpFrame({ six: true, slot: 130, pan: 127, tags: 4 }));
add("dump six no tags", dumpFrame({ six: true, slot: 2, nm: "SIX" }));
add("dump track extras", dumpFrame({ six: true, flags: 0x0c, slot: 0, track: 2, tags: 3,
  extras: [0x01, 0x02, 0x03, 0x00, 0x05, 0x1f, 0x09, 0x02] }));
add("dump track extras flags", dumpFrame({ six: true, flags: 0x0c, track: 5, tags: 0, extras: [0, 0, 9, 1, 0, 0x0a, 0, 5] }));
add("dump track no extras", dumpFrame({ six: false, flags: 0x0c, track: 1 }));
add("dump own bits", dumpFrame({ flags: 0x08 | 0x20 | 0x02, slot: 77 }));
add("store not dump", dumpFrame({ flags: 0x00 }));
// ACK
add("ack", fz(0x52, [0x14, 0, ...r14(3), ...r14(1000)]));
add("ack in use", fz(0x52, [0x14, 10, ...r14(2), ...r14(16383)]));
add("ack status 13", fz(0x52, [0x31, 13, 0, 1, 0, 2]));
add("ack short", fz(0x52, [0x14, 0, 1]));
// STATUS
const statusBody = ({ flags = 0, sel = null }) => [1, 6, flags, 2, 17, 8, ...r14(143), ...r14(40), 3, ...r14(5000), ...r14(300),
  ...r14(512), ...r14(20), ...r14(64), 0x24, ...(sel !== null ? [sel] : [])];
add("status no sel", fz(0x42, statusBody({ flags: 0x05 })));
add("status sel", fz(0x42, statusBody({ flags: 0x7f, sel: 3 })));
add("status all off", fz(0x42, statusBody({ flags: 0, sel: 0 })));
add("status short", fz(0x42, [1, 6, 0, 0]));
// USERS
add("users", fz(0x57, [0, ...r14(42), 3, 0, ...r14(16), 4, ...r14(300), 7, ...r14(1), 2]));
add("users none", fz(0x57, [2, ...r14(128), 0, 0]));
add("users odd source", fz(0x57, [9, ...r14(1), 1, 2, 0, 5, 6]));
add("users minimal", fz(0x57, [0, 0, 1, 0]));
// PROJECTS
const proj = (fl, nm, pages) => [fl, ...name12(nm), ...r14(pages)];
add("projects 8", fz(0x63, [2, 8, proj(1, "NOIR", 12), proj(0, "", 0), proj(3, "CURRENT ONE", 300), proj(5, "DEMO", 1),
  proj(1, "P5", 2), proj(1, "Grand Piano X", 16383), proj(0, "EMPTY", 0), proj(7, "ALL  ", 9)].flat()));
add("projects 2 of 8", fz(0x63, [0, 8, ...proj(3, "A", 1), ...proj(1, "B", 2)]));
add("projects none", fz(0x63, [0, 0]));
// bad ones
const good = fz(0x41, [1, ...ascii("FIZZ 1")]);
const badSum = good.slice(); badSum[badSum.length - 2] ^= 1; add("bad checksum", badSum);
const wrongHdr = good.slice(); wrongHdr[2] = 0x47; add("wrong header", wrongHdr);
add("no f7", good.slice(0, -1));
add("too short", [0xf0, 0x7d, 0x46, 0x5a, 0x01, 0xf7]);
add("unknown kind", fz(0x70, [1, 2]));
add("empty", []);

const parsers = frames.map(({ label, f }) => ({
  label, in: f,
  parseReply: js(sx.parseReply(f)), parseLabels: js(sx.parseLabels(f)), parseList: js(sx.parseList(f)),
  parseDump: js(sx.parseDump(f)), parseAck: js(sx.parseAck(f)), parseStatus: js(sx.parseStatus(f)),
  parseUsers: js(sx.parseUsers(f)), parseProjects: js(sx.parseProjects(f)),
}));

// track / restore frames from the dumps
const reframes = [];
for (const { label, f } of frames.filter((x) => x.label.startsWith("dump"))) {
  reframes.push({ label, in: f, trackFrame: arr(sx.trackFrame(f)), restore: arr(sx.restoreFrame(f)),
    restore0: arr(sx.restoreFrame(f, { slot: 0 })), restore200: arr(sx.restoreFrame(f, { slot: 200 })) });
}
const usb = [sx.identity(), sx.statusRequest(), Uint8Array.from(frames[0].f), sx.storeSound({ blob: blobs.b96 }),
  Uint8Array.from([0xf0, 0xf7]), Uint8Array.from([0xf0, 1, 0xf7]), Uint8Array.from([0xf0, 1, 2, 3])]
  .map((f) => ({ in: arr(f), out: sx.usbPackets(f) }));

// ---- packing
const pack = [];
for (let n = 0; n <= 40; n++) {
  const b = bytes(n);
  const g = sx.pack7(b), s = fm1.pack7(Array.from(b));
  pack.push({ in: arr(b), grouped: g, groupedBack: arr(sx.unpack7(g)), stream: s, streamBack: fm1.unpack7(s) });
}
const unpackOdd = [];
for (let n = 0; n <= 20; n++) {
  const b = bytes(n, 128);
  unpackOdd.push({ in: arr(b), grouped: arr(sx.unpack7(b)), stream: fm1.unpack7(arr(b)) });
}
const tagMasks = [0, 5, 127, 128, 0x1ff, -1, [], ["bass"], ["BASS", "keys", "Pluck", "pad", "LEAD", "drums", "fx"], ["x", "FX", "FX"]]
  .map((tags) => ({ tags, mask: sx.fizzTagMask(tags) }));

// ---- crc32
const c1 = bytes(1000), c2 = bytes(333);
const crcs = [
  { in: [], out: crc32(new Uint8Array(0)) },
  { in: ascii("123456789"), out: crc32(Uint8Array.from(ascii("123456789"))) },
  { in: arr(c1), out: crc32(c1) },
  { in: arr(c2), c0: crc32(c1), out: crc32(c2, crc32(c1)) },
  { in: new Array(256).fill(0xff), out: crc32(new Uint8Array(256).fill(0xff)) },
];

// ---- identity (the update handshake's reply: 27 bytes of text)
const idFrame = (txt, n = 27) => fm1.frame(0x11, [...ascii(txt), ...new Array(27).fill(0)].slice(0, n));
const idCases = [
  ["fizz", idFrame("FM-1_46001100")], ["fizz 2", idFrame("FM-1_46120009")], ["ota", idFrame("ota-FM1_015")],
  ["ota upper", idFrame("OTA-x_1")], ["stock", idFrame("FM-1_016")], ["choralroot", idFrame("FM-1_90026")],
  ["open", idFrame("FM1_OFFICIAL_OPEN")], ["community", idFrame("FM1_COMMUNITY_12")], ["community bad", idFrame("FM1_COMMUNITY_")],
  ["no underscore", idFrame("FM-1")], ["letters", idFrame("FM-1_46x")], ["two underscores", idFrame("A_B_1")],
  ["empty", idFrame("")], ["full 27", idFrame("ABCDEFGHIJKLMNOPQRSTUVWXY_1")], ["short payload", idFrame("FM-1_46001100", 26)],
  ["other cmd", fm1.frame(0x12, [...ascii("FM-1_46001100"), ...new Array(14).fill(0)])],
  ["nul inside", idFrame("FM\u0000-1_4")], ["digits 15", idFrame("M_123456789012345")],
];
const identity = idCases.map(([label, f]) => {
  const id = fm1x.__parseIdentity(f);
  return { label, in: f, out: id ? { text: id.text, model: id.model, version: id.version, open: !!id.open,
    isOTA: fm1.IS_OTA(id), isFizz: !fm1.IS_OTA(id) && id.text.startsWith("FM-1_46") } : null };
});

// ---- update frames, read requests and responses
const updateFrames = [[0x11, []], [0x30, [1, 2, 3]], [0x46, arr(bytes(40))], [0x49, arr(bytes(300))]]
  .map(([cmd, payload]) => ({ cmd, payload, out: fm1.frame(cmd, payload) }));
const le32 = (v) => [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff];
const responses = [];
for (const addr of [0, 0x1234, 0x48e00, 0xe0000000, 0xf0000000]) for (const n of [0, 1, 7, 512]) for (const fl of [0, 1]) {
  const data = arr(bytes(n));
  const out = fm1.response(addr, data, fl);
  responses.push({ addr, data, fl, out, parsed: js(fm1.parseRequest(out)) });
}
const requests = [];
for (const [fl, addr, len] of [[0, 0, 512], [1, 0x200, 512], [0, 0x48e00, 100], [3, 0xe0000000, 0], [0, 0xffffffff, 0xffff]]) {
  const f = fm1.frame(0x30, [fl, ...le32(addr), len & 0xff, (len >> 8) & 0xff, 0]);
  requests.push({ in: f, out: js(fm1.parseRequest(f)) });
}
{
  const f = fm1.frame(0x30, [0, 0, 0, 0, 0, 0, 2, 0]); const bad = f.slice(); bad[bad.length - 2] ^= 1;
  requests.push({ in: bad, out: js(fm1.parseRequest(bad)) });
  const long = fm1.frame(0x30, [0, 0, 0, 0, 0, 0, 2, 0, 9]);
  requests.push({ in: long, out: js(fm1.parseRequest(long)) });
  requests.push({ in: [0xf0, 0xf7], out: js(fm1.parseRequest([0xf0, 0xf7])) });
}

// ---- the bulk channel
const bulkOut = [["FZBI", []], ["FZBR", [0, 256]], ["FZBR", [0x48e00, 17]], ["FZSR", [299008, 0x1a2b3c4d]], ["FZBR", [0xffffffff, 0]]]
  .map(([tag, words]) => ({ tag, words, out: bulkFrame(tag, ...words) }));
const bulkReply = (tag, data) => fm1.frame(0x46, [...ascii(tag), ...data]);
const bulkIn = [
  bulkReply("FZBI", [...le32(0x97000), ...le32(299008), ...le32(0xdeadbeef)]),
  bulkReply("FZBD", [...le32(512), ...arr(bytes(256))]),
  bulkReply("FZOK", le32(1)), bulkReply("FZER", le32(12)), bulkReply("FZOK", []),
  fm1.frame(0x46, [1, 2, 3]), fm1.frame(0x47, [...ascii("FZOK"), 1, 0, 0, 0]),
];
{ const b = bulkReply("FZOK", le32(1)).slice(); b[b.length - 2] ^= 1; bulkIn.push(b); }
bulkIn.push([0xf0, 0xf7], sx.identity());
const bulk = { out: bulkOut, in: bulkIn.map((f) => ({ in: arr(f), out: js(parseBulk(f)) })) };

// ---- the writes' helpers (the iOS write layer): the unit's read requests (a restore's other side), digitone.js
// storeFrame (IMPORT's SEND, BACKUP's ADD), library.js sameSound (ADD's duplicates), the names as the pages clean them
const readRequests = [];
for (const [fl, addr, len] of [[0, 0, 512], [1, 0x200, 512], [0, 0x48e00, 100], [2, 0x48f00, 0x12345], [0, 0xffffffff, 0]]) {
  const f = fm1.frame(0x30, [fl, ...le32(addr), len & 0xff, (len >> 8) & 0xff, (len >> 16) & 0xff]);
  readRequests.push({ fl, addr, len, out: f, parsed: js(fm1.parseRequest(f)) });
}
const sound = (o = {}) => ({ name: "PLUCK", blob: blobs.b128, drive: 7, pan: -5, sends: { chorus: 10, delay: 20, reverb: 30 },
  fizzTags: ["PLUCK"], ...o });
const soundCases = [
  [{}, {}], [{}, { batch: true }], [{}, { pool: true }], [{}, { pool: true, batch: true, track: 2 }], [{}, { track: 0 }],
  [{}, { track: 5 }], [{}, { track: 6 }], [{}, { track: -1 }], [{ fizzTags: [] }, {}], [{ fizzTags: ["BASS", "fx", "nope"] }, {}],
  [{ blob: blobs.b96, drive: 0, pan: 0 }, {}], [{ name: "Grand Piano Extra", pan: 63, sends: { chorus: 127, delay: 0, reverb: 200 } }, { track: 3 }],
  [{ blob: blobs.bad }, {}],
];
const soundFrames = soundCases.map(([o, opts]) => {
  const s = sound(o);
  const args = { name: s.name, blob: arr(s.blob), drive: s.drive, pan: s.pan, chorus: s.sends.chorus, delay: s.sends.delay,
    reverb: s.sends.reverb, fizzTags: s.fizzTags, pool: !!opts.pool, batch: !!opts.batch, track: opts.track ?? -1 };
  try { return { args, out: arr(storeFrame(s, opts)) }; } catch (_) { return { args, throws: true }; }
});
const b96x = Uint8Array.from(blobs.b96); b96x[87] ^= 0x55;
const b96y = Uint8Array.from(blobs.b96); b96y[3] ^= 1;
const b128x = Uint8Array.from(blobs.b128); b128x[87] ^= 0x55;
const same = [["A", blobs.b96, "A", blobs.b96], ["A", blobs.b96, "B", blobs.b96], ["A", blobs.b96, "A", b96x],
  ["A", blobs.b96, "A", b96y], ["A", blobs.b128, "A", b128x], ["A", blobs.b96, "A", blobs.b128], ["", blobs.b128, "", blobs.b128]]
  .map(([an, ab, bn, bb]) => ({ a: { name: an, blob: arr(ab) }, b: { name: bn, blob: arr(bb) }, out: sameSound({ name: an, blob: ab }, { name: bn, blob: bb }) }));
const nameIns = ["", "noir", "  Sunday  ", "Grand Piano Extra Long", "Café ☕ bar", "tab\there", "a-b.c,d'e&f!g?h#i+j/k(l)m*n:o_p~q",
  "abcdefghijk lmn", "ÄÖÜ ß", "~~~~~~~~~~~~~~", " lead "];
// manager.js renderSel's name field (the line as it is there)
const presetClean = (s) => s.toUpperCase().replace(/[^\x20-\x7E]/g, "").trim().slice(0, 12);
const names2 = nameIns.map((s) => ({ in: s, preset: presetClean(s), project: cleanName(s) }));
const fzer = Object.fromEntries(Object.entries(FZER));
const writes = { readRequests, soundFrames, same, names: names2, fzer };

const vectors = {
  generator: "ios/FizzKit/Tests/gen_vectors.mjs",
  handshakeQuery: fm1x.__HS_QUERY, upgrade: fm1x.__UPGRADE, blobs,
  builders, parsers, reframes, usb, pack, unpackOdd, tagMasks, crc32: crcs, identity, updateFrames, responses, requests, bulk,
  writes,
};
const out = join(here, "FizzKitTests/Resources/vectors.json");
writeFileSync(out, JSON.stringify(vectors, replacer) + "\n");
console.log(`${out}: ${builders.length} builder cases, ${parsers.length} frames x 8 parsers, ${pack.length} packs, ` +
  `${identity.length} identities, ${responses.length + requests.length} requests, ${bulk.out.length + bulk.in.length} bulk, ` +
  `${readRequests.length + soundFrames.length + same.length + names2.length} write helpers`);
