// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// FIZZ P1.5x: tools/fizz_sysex.mjs's STORE frames as FIZZ reads them (core/tests/test_sysex --feed): a preset's pan
// and sends kept LOAD or not, E1's STORE6 (the version-6 blob), the optional tags byte (a mask or FIZZ's names; 0 or none: FIZZ's classifier), the
// library's 256 places, and a frame's bytes where docs/SYSEX.md puts them.
//
//   node tools/test_fizz_sysex.mjs TEST_SYSEX
import { writeFileSync, mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { execFileSync } from "node:child_process";
import { storeSound, parseReply, fizzTagMask, FIZZ_TAGS, labelsRequest, parseLabels, LABELS_N, listRequest, parseList, dumpRequest,
  parseDump, restoreFrame, parseReply as reply2 } from "./fizz_sysex.mjs";

const [exe] = process.argv.slice(2);
if (!exe) { console.error("usage: node test_fizz_sysex.mjs TEST_SYSEX"); process.exit(2); }
const tmp = mkdtempSync(join(tmpdir(), "fizz-sx5x-"));
let fails = 0, checks = 0;
const check = (ok, msg) => { checks++; if (!ok) { fails++; console.log(`  FAIL ${msg}`); } };
// a valid FM TONE blob: 'Q' 5, every value at its minimum (FIZZ's quad_pack of the minima: all zero bytes)
const blob = new Uint8Array(96);
blob[0] = 0x51, blob[1] = 5;
const feed = (frame) => {
  const syx = join(tmp, "one.syx");
  writeFileSync(syx, frame);
  const out = execFileSync(exe, ["--feed", syx], { encoding: "utf8" });
  const r = out.match(/^reply(.*)$/m);
  return { reply: r && parseReply(Uint8Array.from(r[1].trim().split(/\s+/).map((h) => parseInt(h, 16)))),
           mix: ((out.match(/^mix (.*)$/m) || [])[1] || "").split(" ").map(Number),
           tags: Number((out.match(/^tags (.*)$/m) || [])[1]) };
};

{   // the bytes
  const f = storeSound({ name: "MIX", blob, pan: -20, chorus: 11, delay: 22, reverb: 33, tags: ["PAD", "fx"] });
  check(f.length === 141 && f[5] === 0x10, `with tags: 141 bytes (${f.length})`);
  check(f[6 + 17] === 44 && f[6 + 18] === 11 && f[6 + 19] === 22 && f[6 + 20] === 33 && f[6 + 21] === 0x0f,
        "pan + 64, chorus, delay, reverb, mixmask 0F (no LOAD)");
  check(f[6 + 132] === ((1 << 3) | (1 << 6)), `the tags byte after the patch: PAD | FX (${f[6 + 132]})`);
  check(storeSound({ name: "NONE", blob }).length === 140, "no tags: 140 bytes, as protocol 1 had them");
  check(fizzTagMask(["BASS", "drums", "?"]) === 0x21 && fizzTagMask(0x1ff) === 0x7f && FIZZ_TAGS.length === 7, "fizzTagMask");
  let threw = false;
  try { storeSound({ blob, pool: true, slot: 200 }); } catch (_) { threw = true; }
  check(threw && storeSound({ blob, slot: 200 }).length === 140, "LIBRARY slots to 256, POOL to 128");
}
{   // as FIZZ reads them
  let r = feed(storeSound({ name: "MIX", blob, pan: -20, chorus: 11, delay: 22, reverb: 33, tags: ["PAD", "FX"] }));
  check(r.reply && r.reply.status === 0 && r.mix.join() === "-20,11,22,33" && r.tags === 72,
        `stored: its mix ${r.mix}, its tags ${r.tags} (PAD | FX = 72)`);
  r = feed(storeSound({ name: "PART", blob, reverb: 50 }));
  check(r.reply && r.reply.status === 0 && r.mix.join() === "0,0,0,50", `only the reverb given: ${r.mix}`);
  r = feed(storeSound({ name: "LOADED", blob, track: 2, pan: 10, chorus: 1, delay: 2, reverb: 3, tags: 1 }));
  check(r.reply && r.reply.status === 0 && r.mix.join() === "10,1,2,3" && r.tags === 1, `LOAD too: ${r.mix}, tags ${r.tags}`);
  r = feed(storeSound({ name: "ZERO TAGS", blob, tags: 0 }));
  check(r.reply && r.reply.status === 0 && r.tags > 0, `tags 0: FIZZ's classifier (${r.tags})`);
  r = feed(storeSound({ name: "POOLED", blob, pool: true, pan: 5, reverb: 9, tags: ["LEAD"] }));
  check(r.reply && r.reply.status === 0 && r.reply.pool && r.mix.join() === "5,0,0,9" && r.tags === 16,
        `POOL: its preset's mix ${r.mix} and tags ${r.tags} (P1.5w: a reference to a new MY preset)`);
}
{   // E1: STORE6, the 128-byte version-6 blob (FIZZ's values 74..: AMOD 1, HOLD 50, PORT ON, PTIM 40, DRIVE AT PRE)
  const b6 = new Uint8Array(128);
  b6[0] = 0x51, b6[1] = 6;
  b6[88] = 1, b6[89] = 50, b6[90] = 1, b6[91] = 40, b6[93] = 1;
  const f = storeSound({ name: "SIX", blob: b6, drive: 30, tags: 1 });
  check(f[5] === 0x11 && f.length === 6 + 22 + 147 + 1 + 2, `a 128-byte blob: STORE6, ${f.length} bytes`);
  const out = execFileSync(exe, ["--feed", (writeFileSync(join(tmp, "six.syx"), f), join(tmp, "six.syx"))], { encoding: "utf8" });
  const r = parseReply(Uint8Array.from(out.match(/^reply(.*)$/m)[1].trim().split(/\s+/).map((h) => parseInt(h, 16))));
  const v = ((out.match(/^values (.*)$/m) || [])[1] || "").split(" ").map(Number);
  check(r && r.status === 0 && v[73] === 30 && v[74] === 1 && v[75] === 50 && v[76] === 1 && v[77] === 40 && v[79] === 1,
        `STORE6 stored: DRIVE ${v[73]}, AMOD ${v[74]} HOLD ${v[75]} PORT ${v[76]} PTIM ${v[77]} ROUTE ${v[79]}`);
  const b5 = blob.slice(); b5[1] = 6;
  const r5 = feed(storeSound({ name: "SHORT SIX", blob: b5 }));
  check(r5.reply && r5.reply.status !== 0, `a version-6 blob in a STORE (96 bytes): refused (${r5.reply && r5.reply.statusName})`);
}
{   // P1.6l: LABELS through FIZZ (--session: awake, the step view): the step view's labels, parsed
  const req = labelsRequest({ subscribe: true });
  check(req.length === 9 && req[5] === 0x20 && req[6] === 1, `labelsRequest: 9 bytes, 20 01 (${Array.from(req)})`);
  writeFileSync(join(tmp, "labels.syx"), req);
  const out = execFileSync(exe, ["--session", join(tmp, "labels.syx")], { encoding: "utf8" });
  const line = (out.match(/^reply(.*)$/m) || [])[1] || "";
  const l = parseLabels(Uint8Array.from(line.trim().split(/\s+/).map((h) => parseInt(h, 16))));
  check(l && l.list.length === LABELS_N && l.white[0] === "STEP 1" && l.black[1] === "VEL" && l.knobs.SELECT === "PAGE" &&
        l.buttons.PLAY === "PLAY" && l.knobs.MASTER === "VOLUME" && l.list.every((x) => x.length <= 10),
        `parseLabels: 49, W1 ${l && l.white[0]}, B2 ${l && l.black[1]}, SELECT ${l && l.knobs.SELECT}`);
  check(parseReply(Uint8Array.from(line.trim().split(/\s+/).map((h) => parseInt(h, 16)))).kind === "labels", "parseReply: labels");
}
{   // P1.6m: two presets stored (one STORE6), LIST, DUMP each, then stored back into a fresh FIZZ: the same
  const b6 = new Uint8Array(128);
  b6[0] = 0x51, b6[1] = 6, b6[88] = 1, b6[90] = 1;
  const a1 = storeSound({ name: "EXPORT ONE", blob, slot: 3, pan: -7, chorus: 5, delay: 6, reverb: 7, tags: ["PAD"] });
  const a2 = storeSound({ name: "EXPORT SIX", blob: b6, slot: 9, drive: 40, reverb: 99, tags: ["LEAD", "FX"] });
  const syx = (frames) => { const p = join(tmp, "s.syx"); writeFileSync(p, Uint8Array.from(frames.flatMap((f) => Array.from(f)))); return p; };
  const replies = (frames) => execFileSync(exe, ["--session", syx(frames)], { encoding: "utf8" }).split("\n")
    .filter((l) => l.startsWith("reply")).map((l) => Uint8Array.from(l.slice(5).trim().split(/\s+/).map((h) => parseInt(h, 16))));
  const r = replies([a1, a2, listRequest(), dumpRequest({ source: "mine", slot: 3 }), dumpRequest({ source: "mine", slot: 9 }),
                     dumpRequest({ source: "factory", slot: 1 }), dumpRequest({ source: "mine", slot: 0 })]);
  const list = r.map((x) => parseList(x)).filter(Boolean);
  const dumps = r.map((x) => parseDump(x)).filter(Boolean);
  check(list.length === 1 && list[0].last && list[0].total === 2 && list[0].entries.map((e) => `${e.slot} ${e.name}`).join() ===
        "3 EXPORT ONE,9 EXPORT SIX" && list[0].entries[0].imported, `LIST: ${JSON.stringify(list[0] && list[0].entries)}`);
  check(dumps.length === 5 && dumps[0].name === "EXPORT ONE" && !dumps[0].six && dumps[0].pan === -7 && dumps[0].reverb === 7 &&
        dumps[0].tags === 8 && dumps[1].six && dumps[1].drive === 40 && dumps[1].blob[88] === 1 && dumps[1].tags === 80 &&
        dumps[2].slot === 0 && dumps[3].name === "EXPORT ONE" && dumps[4].name === "EXPORT SIX",
        `DUMP: the two, the factory's (slot 0: a new one), all (${dumps.map((d) => d.name).join(", ")})`);
  const back = replies([restoreFrame(dumps[0].frame), restoreFrame(dumps[1].frame), dumpRequest({ source: "mine", slot: 3 }),
                        dumpRequest({ source: "mine", slot: 9 }), restoreFrame(dumps[0].frame).slice(0, 0)]);
  const again = back.map((x) => parseDump(x)).filter(Boolean);
  const same = (x, y) => x && y && x.name === y.name && x.pan === y.pan && x.chorus === y.chorus && x.delay === y.delay &&
    x.reverb === y.reverb && x.tags === y.tags && x.drive === y.drive && x.blob.join() === y.blob.join();
  check(back.filter((x) => reply2(x) && reply2(x).kind === "store" && reply2(x).status === 0).length === 2 &&
        again.length === 2 && same(again[0], dumps[0]) && same(again[1], dumps[1]),
        "restoreFrame: stored back into a fresh FIZZ, its dumps the same as the first ones");
}
console.log(`test_fizz_sysex: ${checks} checks, ${fails} failed`);
process.exit(fails ? 1 : 0);
