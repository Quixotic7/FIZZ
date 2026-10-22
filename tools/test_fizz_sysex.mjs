// SPDX-License-Identifier: GPL-3.0-only
// FIZZ P1.5x: tools/fizz_sysex.mjs's STORE frames as FIZZ reads them (core/tests/test_sysex --feed): a preset's pan
// and sends kept LOAD or not, the optional tags byte (a mask or FIZZ's names; 0 or none: FIZZ's classifier), the
// library's 256 places, and a frame's bytes where docs/SYSEX.md puts them.
//
//   node tools/test_fizz_sysex.mjs TEST_SYSEX
import { writeFileSync, mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { execFileSync } from "node:child_process";
import { storeSound, parseReply, fizzTagMask, FIZZ_TAGS } from "./fizz_sysex.mjs";

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
console.log(`test_fizz_sysex: ${checks} checks, ${fails} failed`);
process.exit(fails ? 1 : 0);
