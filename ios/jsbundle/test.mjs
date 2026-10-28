// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The bundler proved right: FizzData.js evaluated in a bare Node vm context (no TextEncoder / TextDecoder, atob /
// btoa, DecompressionStream, Blob: as a JSContext; its polyfills and the routed inflate run) against entry.js and the
// site modules imported directly, on fixed synthetic inputs: byte-equal files, equal results, equal error messages.
//   node ios/jsbundle/test.mjs            (from the worktree root; after node ios/jsbundle/build.mjs)
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import vm from "node:vm";
import { inflateRawSync } from "node:zlib";
import * as E from "./entry.js";
import * as S from "./synth.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const BUNDLE = join(here, "../FizzKit/Sources/FizzData/Resources/FizzData.js");

new Function(S.UTC_DATES)();                              // (both realms: Date's local getters are UTC ones)
const ctx = vm.createContext({});
vm.runInContext(S.UTC_DATES, ctx);
const VU8 = vm.runInContext("Uint8Array", ctx);
for (const g of ["TextEncoder", "TextDecoder", "atob", "btoa", "DecompressionStream", "Blob", "Response", "setTimeout"])
  if (vm.runInContext(`typeof ${g}`, ctx) !== "undefined") throw new Error(`the vm context has ${g}: not a JSContext's`);
let inflates = 0;
ctx.__fizzInflateRaw = (b) => { inflates++; const r = inflateRawSync(b), u = new VU8(r.length); u.set(r); return u; };
const logs = [];
ctx.console = { log: (...a) => logs.push(a.join(" ")) };
const t0 = performance.now();
vm.runInContext(readFileSync(BUNDLE, "utf8"), ctx, { filename: "FizzData.js" });
const loadMs = performance.now() - t0;
const F = ctx.FizzData;

// inputs into the vm's realm (makeFizzbak and storeSound check instanceof Uint8Array)
const intoVm = (x) => ArrayBuffer.isView(x) ? VU8.from(x) : Array.isArray(x) ? x.map(intoVm)
  : x && typeof x === "object" && !(x instanceof Date) ? Object.fromEntries(Object.entries(x).map(([k, v]) => [k, intoVm(v)])) : x;
const plain = (x) => JSON.stringify(E.toPlain(x));
let n = 0, bad = 0;
function check(name, a, b) {
  n++;
  const pa = plain(a), pb = plain(b);
  if (pa !== pb) { bad++; console.log(`✘ ${name}\n  esm:    ${pa.slice(0, 400)}\n  bundle: ${pb.slice(0, 400)}`); }
}
const sameBytes = (name, a, b) => { n++; if (a.length !== b.length || a.some((x, i) => x !== b[i])) { bad++; console.log(`✘ ${name}: ${a.length} / ${b.length} bytes differ`); } };
async function both(name, fn, args) {               // fn(api, args) on both sides: equal results, or equal errors
  const run = async (api, a) => { try { return { ok: await fn(api, a) }; } catch (e) { return { error: String(e && e.message) }; } };
  const a = await run(E, args), b = await run(F, intoVm(args));
  check(name, a, b);
  return [a.ok, b.ok];
}

// ---- the polyfills against Node's own
const strs = ["", "FIZZ", "héllo ☃ 𝄞", "\ud800x", "x\udc00", "\ud83d", "€".repeat(30000), String.fromCharCode(...Array.from({ length: 256 }, (_, i) => i))];
for (const s of strs) check(`TextEncoder ${JSON.stringify(s.slice(0, 12))}`, new TextEncoder().encode(s), vm.runInContext("(s) => new TextEncoder().encode(s)", ctx)(s));
const byteCases = [[], [0x41], [0xEF, 0xBB, 0xBF, 0x41], [0xC0, 0x80], [0xED, 0xA0, 0x80], [0xF4, 0x90, 0x80, 0x80], [0xE2, 0x82], [0xE2, 0x82, 0x41],
  [0xF0, 0x9F, 0x98, 0x80], [0x80, 0xBF], [0xFF, 0xFE], [0xE2, 0x82, 0xAC], Array.from(new TextEncoder().encode("ok ☃ 𝄞 ".repeat(5000)))];
for (const c of byteCases) {
  check(`TextDecoder [${c.slice(0, 6).join(",")}]`, new TextDecoder().decode(Uint8Array.from(c)), vm.runInContext("(b) => new TextDecoder().decode(b)", ctx)(VU8.from(c)));
  check(`TextDecoder ignoreBOM [${c.slice(0, 6).join(",")}]`, new TextDecoder("utf-8", { ignoreBOM: true }).decode(Uint8Array.from(c)),
    vm.runInContext("(b) => new TextDecoder('utf-8', { ignoreBOM: true }).decode(b)", ctx)(VU8.from(c)));
}
const r = S.rng(7);
for (let len = 0; len < 40; len++) {
  const s = String.fromCharCode(...Array.from({ length: len * 7 }, () => Math.floor(r() * 256)));
  check(`btoa ${len * 7}`, btoa(s), ctx.btoa(s));
  check(`atob ${len * 7}`, atob(btoa(s)), ctx.atob(btoa(s)));
}
for (const s of ["QUJD", "QUJ", "QU", "Q", "QQ==", "QQ=", " Q U J D \n", "QUJD=", "QU=D", "Q@", "QR==", "QUJD\tRA=="]) {
  const t = (f) => { try { return f(s); } catch (e) { return `throws ${e.name}`; } };
  check(`atob ${JSON.stringify(s)}`, t(atob), t(ctx.atob));
}
for (const s of ["ok", "é", "☃"]) { const t = (f) => { try { return f(s); } catch (e) { return `throws ${e.name}`; } }; check(`btoa ${s}`, t(btoa), t(ctx.btoa)); }

// ---- crc32, hex8, file names, sound keys
for (let len = 0; len < 300; len += 37) { const b = Uint8Array.from({ length: len }, () => Math.floor(r() * 256)); await both(`crc32 ${len}`, (A, x) => A.hex8(A.crc32(x)), b); }
for (const kind of ["full", "presets", "other"]) await both(`backupFileName ${kind}`, (A, d) => A.backupFileName(d, kind), S.DATE);
for (const s of S.SOUNDS) await both(`soundKey ${s.name}`, (A, x) => A.soundKey(x), s);
check("layoutOf", [E.layoutOf(299008), E.layoutOf(1 << 20), E.layoutOf(5)], [F.layoutOf(299008), F.layoutOf(1 << 20), F.layoutOf(5)]);

// ---- sounds
for (const f of S.FRAMES) {
  await both("soundFromFrame", (A, x) => A.soundFromFrame(x), f);
  await both("soundFromStoreFrame", (A, x) => A.soundFromStoreFrame(x), f);
}
await both("soundFromStoreFrame (not a frame)", (A, x) => A.soundFromStoreFrame(x), Uint8Array.of(1, 2, 3));
for (const s of [...S.SOUNDS, S.V5]) {
  const [text] = await both(`soundToFizzsnd ${s.name}`, (A, x) => A.soundToFizzsnd(x, { exported: "2026-10-27" }), s);
  await both(`parseFizzsnd ${s.name}`, (A, t) => A.parseFizzsnd(t, "x.fizzsnd"), text);
  await both(`parseFizzsnd bytes ${s.name}`, (A, t) => A.parseFizzsnd(new TextEncoder().encode(t), "x.fizzsnd"), text);
}
await both("parseFizzsnd (bad)", (A, t) => A.parseFizzsnd(t), '{"format":"fizz-sound","version":1,"name":"X","patch":{"blob":"!!","blobVersion":6}}');
await both("sameSound", (A, [a, b, c]) => [A.sameSound(a, b), A.sameSound(a, c)], [S.SOUNDS[0], { ...S.SOUNDS[0] }, S.SOUNDS[1]]);
const [libE, libF] = await both("makeFizzlib", (A, x) => A.makeFizzlib(x, { name: "SYN", author: "test", fizz: S.FIZZ, date: S.DATE }), [...S.SOUNDS, S.V5]);
sameBytes("makeFizzlib bytes", libE, libF);
await both("parseFizzlib", (A, b) => A.parseFizzlib(b, "syn.fizzlib"), libE);

// ---- the store
const [imgE, imgF] = await both("demoStore", (A, x) => A.demoStore(x), { presets: S.SOUNDS, projects: S.PROJECTS });
sameBytes("demoStore bytes", imgE, imgF);
await both("demoStore (a v5 blob)", (A, x) => A.demoStore(x), { presets: [S.V5] });
await both("demoStore host", (A, x) => A.demoStore(x), { presets: S.SOUNDS.slice(0, 2), projects: ["ONE"], layout: "host" });
await both("readRegion", (A, x) => A.readRegion(x), imgE);
await both("readRegion (blank)", (A, x) => A.readRegion(x), new Uint8Array(299008).fill(0xFF));
await both("readRegion (not a store)", (A, x) => A.readRegion(x), new Uint8Array(299008));
await both("readRegion (a size)", (A, x) => A.readRegion(x), new Uint8Array(100));
await both("writeStore", (A, x) => { const img = new Uint8Array(8 * 4096); const n = A.writeStore(img, 0, 8, x.map((b, i) => ({ key: 0x05000000 + i, bytes: b }))); return [n, img]; },
  [new Uint8Array(10), new Uint8Array(240), new Uint8Array(241), new Uint8Array(1000)]);
await both("writeStore (full)", (A, x) => A.writeStore(new Uint8Array(8 * 4096), 0, 8, [{ key: 1, bytes: x }]), new Uint8Array(240 * 40));
await both("buildImage", (A, x) => A.buildImage("device", { lib: [{ key: 0x05000000, bytes: x }] }), new Uint8Array(151));

// ---- the backup file
const full = { kind: "full", image: imgE, region: S.REGION, crc: null, frames: S.FRAMES, fizz: S.FIZZ, unit: S.UNIT,
  webTags: { [E.soundKey(S.SOUNDS[0])]: ["SHIMMER", "DARK"], [E.soundKey(S.SOUNDS[2])]: [], deadbeef: ["UNUSED"] }, stats: S.STATS, date: S.DATE };
const [fullE, fullF] = await both("makeUnitFizzbak full", (A, x) => A.makeUnitFizzbak(x), full);
sameBytes("makeUnitFizzbak full bytes", fullE, fullF);
const [presE, presF] = await both("makeUnitFizzbak presets", (A, x) => A.makeUnitFizzbak(x), { ...full, kind: "presets", stats: null });
sameBytes("makeUnitFizzbak presets bytes", presE, presF);
const [noDumpE] = await both("makeUnitFizzbak (no DUMP: the image's presets)", (A, x) => A.makeUnitFizzbak(x), { ...full, frames: null });
await both("makeUnitFizzbak (a bad frame)", (A, x) => A.makeUnitFizzbak(x), { ...full, frames: [Uint8Array.of(0xF0, 0xF7)] });
await both("makeFizzbak (no image)", (A, x) => A.makeFizzbak(x), { kind: "full", presets: S.SOUNDS });
const [directE, directF] = await both("makeFizzbak", (A, x) => A.makeFizzbak(x), { kind: "full", image: imgE, region: S.REGION, presets: S.SOUNDS, fizz: S.FIZZ, unit: S.UNIT, date: S.DATE });
sameBytes("makeFizzbak bytes", directE, directF);

const defl = S.deflatedZip(S.storedMembers(libE));
const damaged = fullE.slice(); damaged[fullE.length >> 1] ^= 0x55;
const notZip = new Uint8Array(1000);
for (const [name, bytes, file] of [["full", fullE, "a.fizzbak"], ["presets", presE, "p.fizzbak"], ["no DUMP", noDumpE, "n.fizzbak"],
  ["fizzlib", libE, "l.fizzlib"], ["deflated fizzlib", defl, "d.fizzlib"], ["bin", imgE, "store.bin"], ["damaged", damaged, "x.fizzbak"],
  ["not a backup", notZip, "x.bin"], ["cut short", fullE.subarray(0, 5000), "c.fizzbak"]])
  await both(`readBackup ${name}`, (A, b) => A.readBackup(b, file), bytes);
const deflBefore = inflates;
await F.readBackup(intoVm(defl), "d.fizzlib");
n++; if (inflates <= deflBefore) { bad++; console.log("✘ the deflated members did not go through __fizzInflateRaw"); }

console.log(`${n - bad} / ${n} equal (the bundle: ${(readFileSync(BUNDLE).length / 1024).toFixed(0)} KB, loaded in ${loadMs.toFixed(1)} ms)` +
  (logs.length ? `; console: ${logs.join(" | ")}` : ""));
process.exit(bad ? 1 : 0);
