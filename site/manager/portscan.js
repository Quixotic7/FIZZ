// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// PORTS (the iOS app's PortReport and FirmwareName: ios/FizzKit/Sources/FizzUI/PortsSheet.swift): every MIDI port the
// browser sees, the in/out pairs asked who they are, and what answered. A port is asked gently: fm1-base's update
// handshake (the identity query, the one every installer sends first) once, a short wait, nothing else: no write,
// never the upgrade command (nothing that restarts a unit or puts it in its loader). Only ports with an FM-1's usual
// names are asked (fm1lib.js Updater.usual: other gear never is), and never the port of the unit in use (its link
// answers for it: skip()).
//   portsOf(access) -> [{ name, inn, out, ble }]           (the names of the inputs and outputs not disconnected)
//   firmwareOf(id) -> { fw: "FIZZ" | "REMOTEFM1" | "M-VAVE" | "M-VAVE OPEN" | "UPDATE MODE" | <the id>, version, fizz } | null
//   await probePort(access, name, ms) -> the id it answered ("FM-1_46001200") | null
//   const scan = new PortScan(access, { skip: () => ({ port, id, version } | null), ms, log }); await scan.run({ again })
//   scan.report() -> { at, ports: [{ name, inn, out, ble, asked, id, fw, version, fizz, current }], n: { in, out, asked, fizz } }
//   scan.forget(name) (a port gone: asked again when it comes back); reportText(report) (for the copy button)
import { Updater, unpack7 } from "../install/fm1lib.js";
import { isBlePort, FIZZ_PREFIX } from "./fizzlink.js";

const HS_QUERY = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];   // (fm1lib.js's: identity, cmd 0x11 back)
const live = (m) => [...m.values()].filter((p) => p.state !== "disconnected");
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

export function portsOf(access) {
  if (!access) return [];
  const ins = new Set(live(access.inputs).map((p) => p.name || "")), outs = new Set(live(access.outputs).map((p) => p.name || ""));
  return [...new Set([...ins, ...outs])].map((name) => ({ name, inn: ins.has(name), out: outs.has(name), ble: isBlePort(name) }));
}

// what firmware an update-handshake id belongs to (FirmwareName.of); FIZZ's releases are FM-1_46XXYYZZ (fm1.toml)
export function firmwareOf(id) {
  if (!id) return null;
  const s = String(id);
  if (s.startsWith(FIZZ_PREFIX)) {
    const m = /^FM-1_46(\d\d)(\d\d)(\d\d)$/.exec(s);
    return { fw: "FIZZ", version: m ? `${+m[1]}.${+m[2]}.${+m[3]}` : "", fizz: true };
  }
  const fw = s.startsWith("FM-1_52") ? "REMOTEFM1"
    : /^FM1_(OFFICIAL_OPEN|COMMUNITY)/.test(s) || /^FM-1_80000\d\d$/.test(s) ? "M-VAVE OPEN"
      : /^FM-1_0\d\d$/.test(s) ? "M-VAVE"
        : /^ota-/i.test(s) ? "UPDATE MODE" : s;
  return { fw, version: "", fizz: false };
}

// fm1-base's identity reply: F0 pack7(00 59 11 len16 00 text[27] ~sum) F7 -> its text | null
export function parseHandshake(p) {
  if (!p || p[0] !== 0xF0 || p[p.length - 1] !== 0xF7) return null;
  const d = unpack7(p.slice(1, -1));
  if (d.length !== 34 || d[0] !== 0 || d[1] !== 0x59 || d[2] !== 0x11) return null;
  const t = String.fromCharCode(...d.slice(6, 33)).replace(/\0+$/, "");
  return /^[\x20-\x7E]+$/.test(t) ? t : null;
}

// one port asked once: its input listened to for `ms`, the handshake sent to its output; the input's handler put back
export async function probePort(access, name, ms = 700) {
  const input = live(access.inputs).find((p) => p.name === name), output = live(access.outputs).find((p) => p.name === name);
  if (!input || !output) return null;
  try { await input.open(); await output.open(); } catch (_) { return null; }
  const prev = input.onmidimessage;
  let got = null, wake = null;
  input.onmidimessage = (ev) => {
    const id = parseHandshake(Array.from(ev.data || []));
    if (id && !got) { got = id; if (wake) wake(); }
  };
  try {
    output.send(HS_QUERY);
    if (!got) await Promise.race([new Promise((r) => { wake = r; }), sleep(ms)]);
  } catch (_) { /* (gone) */ }
  finally { input.onmidimessage = prev || null; }
  return got;
}

export class PortScan {
  constructor(access, { skip = () => null, ms = 700, log = () => {} } = {}) {
    Object.assign(this, { access, skip, ms, log, answers: new Map(), at: null, q: Promise.resolve(), running: false });
    this.up = new Updater(access);
  }
  usual(name) { return this.up.usual(name); }
  forget(name) { this.answers.delete(name); }
  // every pair with a usual name asked (again: those asked before too); one run at a time
  run({ again = false } = {}) {
    const go = async () => {
      this.running = true;
      try {
        const pairs = portsOf(this.access), names = new Set(pairs.map((p) => p.name));
        for (const n of [...this.answers.keys()]) if (!names.has(n)) this.answers.delete(n);
        for (const p of pairs) {
          if (!p.inn || !p.out || !this.usual(p.name)) continue;
          const cur = this.skip();
          if (cur && cur.port === p.name) continue;
          if (!again && this.answers.has(p.name)) continue;
          const id = await probePort(this.access, p.name, this.ms);
          this.answers.set(p.name, { id });
          if (id) this.log(`ports: "${p.name}" answered ${id}`);
        }
        this.at = new Date();
      } finally { this.running = false; }
    };
    const r = this.q.then(go, go);
    this.q = r.catch(() => {});
    return r;
  }
  report() {
    const cur = this.skip();
    const ports = portsOf(this.access).map((p) => {
      const mine = !!(cur && cur.id && cur.port === p.name), a = mine ? { id: cur.id } : this.answers.get(p.name);
      const f = a && a.id ? firmwareOf(a.id) : null;
      return { ...p, asked: !!a, id: (a && a.id) || null, fw: f ? f.fw : "", fizz: !!(f && f.fizz),
        version: mine && cur.version ? cur.version : f ? f.version : "", current: !!mine };
    });
    return { at: this.at, ports, n: { in: ports.filter((p) => p.inn).length, out: ports.filter((p) => p.out).length,
      asked: ports.filter((p) => p.asked).length, fizz: ports.filter((p) => p.fizz).length } };
  }
}

// the report as plain text (sent with a bug report, as the iOS sheet's share)
export function reportText(r) {
  const ln = [`FIZZ Manager: ports${r.at ? ` (${r.at.toISOString()})` : ""}`];
  for (const p of r.ports)
    ln.push(`${p.name}  ${p.inn ? "in" : "--"}/${p.out ? "out" : "---"}  ${p.ble ? "BLUETOOTH" : "USB"}  ${p.asked ? `asked: ${p.id ? `${p.id} ${p.fw}${p.version ? ` ${p.version}` : ""}` : "nothing"}` : "not asked"}`);
  return ln.join("\n");
}
