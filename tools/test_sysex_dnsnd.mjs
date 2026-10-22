// SPDX-License-Identifier: GPL-3.0-only
// P1.5r: Digitone .dnsnd presets into FIZZ over SysEx, end to end: ChoralRoot's reader (../ChoralRootFM1/web:
// parseDnsnd, dnsndToQuad, soundFromDnsnd's name) builds each preset's FM TONE patch, fizz_sysex.mjs its STORE SOUND
// frame, and FIZZ itself (test_sysex --feed, the core as the glue drives it) stores it: its reply must be OK and the
// sound stored must be the reader's values, all 73, its DRIVE and its name. The presets are the owner's own, read from
// their folder for this test only (never copied into this tree).
//
//   node tools/test_sysex_dnsnd.mjs DIR TEST_SYSEX [N]     the first N presets of DIR (default 12)
import { readFileSync, readdirSync, writeFileSync, mkdtempSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { execFileSync } from "node:child_process";
import { parseDnsnd, dnsndToQuad } from "../../ChoralRootFM1/web/fm1dnsnd.js";
import { soundFromDnsnd } from "../../ChoralRootFM1/web/fm1sounds.js";
import { storeSound, identity, parseReply, usbPackets, unpack7, pack7 } from "./fizz_sysex.mjs";

const [dir, exe, nArg] = process.argv.slice(2);
if (!dir || !exe) { console.error("usage: node test_sysex_dnsnd.mjs DIR TEST_SYSEX [N]"); process.exit(2); }
const files = readdirSync(dir).filter((f) => f.endsWith(".dnsnd")).sort().slice(0, Number(nArg) || 12);
const tmp = mkdtempSync(join(tmpdir(), "fizz-sx-"));
let fails = 0, checks = 0;
const check = (ok, msg) => { checks++; if (!ok) { fails++; console.log(`  FAIL ${msg}`); } };

{   // the host side's own pieces
  const b = Uint8Array.from({ length: 96 }, (_, i) => (i * 37 + 11) & 255);
  check(Array.from(unpack7(pack7(b))).join() === Array.from(b).join() && pack7(b).length === 110, "pack7 / unpack7");
  const id = identity();
  check(id.join() === [0xf0, 0x7d, 0x46, 0x5a, 1, 1, 0x7f, 0xf7].join(), "identity()'s bytes");
  const pk = usbPackets(id);
  check(pk.length === 3 && (pk[2] & 15) === 6 && (pk[0] & 15) === 4, "usbPackets: CIN 4 4 6");
}
for (const f of files) {
  const parsed = await parseDnsnd(new Uint8Array(readFileSync(join(dir, f))));
  const q = dnsndToQuad(parsed), s = soundFromDnsnd(parsed);
  const msg = storeSound({ name: s.name, blob: q.blob, drive: q.drive });
  const syx = join(tmp, "one.syx");
  writeFileSync(syx, msg);
  const out = execFileSync(exe, ["--feed", syx], { encoding: "utf8" });
  const reply = parseReply(Uint8Array.from(out.match(/^reply(.*)$/m)[1].trim().split(/\s+/).map((h) => parseInt(h, 16))));
  const name = (out.match(/^name "(.*)"$/m) || [])[1];
  const vals = ((out.match(/^values (.*)$/m) || [])[1] || "").split(" ").map(Number);
  const want = [...q.values, Math.max(0, Math.min(127, q.drive))];
  const same = vals.length === 74 && want.every((v, i) => v === vals[i]);
  check(reply && reply.kind === "store" && reply.status === 0 && !reply.pool && reply.slot === 1,
        `${f}: the reply ${JSON.stringify(reply)}`);
  check(name === s.name.slice(0, 12).replace(/\s+$/, ""), `${f}: the name "${name}" (the reader's "${s.name}")`);
  check(same, `${f}: the values stored = the reader's (${vals.length}; first difference at ${want.findIndex((v, i) => v !== vals[i])})`);
  console.log(`  ${f.padEnd(28)} -> LIBRARY ${reply && reply.slot}: "${name}", DRIVE ${q.drive}, ${msg.length} bytes`);
}
console.log(`test_sysex_dnsnd: ${files.length} presets, ${checks} checks, ${fails} failed`);
process.exit(fails ? 1 : 0);
