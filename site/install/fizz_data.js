// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// What FIZZ's install page adds to fm1-base's installer (fm1lib.js), over the same Web MIDI link:
//
//   pushData(access, blob, {off, log, onProgress})  FIZZ's data blob (fizz-xdata.bin: its type and the TAP ape) into
//                                              the FM-1's data flash at off (XDATA_OFF; the Bluetooth package's two:
//                                              BLE_BLOBS). The update package does not carry it (fm1-base's loader
//                                              writes the program area only), so the page writes it after the
//                                              install, as firmware/tools/fizz_xdata.py push does: 46 "FZXD" len crc
//                                              off, then the unit's 512-byte read requests, then 46 "FZOK" or "FZER"
//                                              and a code (firmware/app/app.c app_usb_cmd). -> {written: bool}
//   readUnit(access)                           what the connected FM-1 runs: {kind: "fizz" | "stock" | "update" |
//                                              "other", id (its identity), version (FIZZ's own, from its SysEx
//                                              IDENTITY: "0.11.0-beta.1"), fromId (the version in the identity),
//                                              package ("usb" | "ble": FIZZ's STATUS; null when FIZZ did not answer)}
//                                              | null (no FM-1 answered)
//   versionOfId(id)                            "FM-1_46001100" -> "0.11.0"; "FM-1_4600" (a development build) -> ""
//   usbOnly(access)                            the access without the Bluetooth package's MIDI port ("FM-1 XXXX
//                                              Bluetooth"): an install and the data blobs go over USB only (FZER 15)
import { Updater, IS_OTA, IS_STOCK, frame, response, parseRequest, unpack7, fail } from "./fm1lib.js";
import { identity as fizzIdentity, parseReply, statusRequest } from "../js/fizz_sysex.js";

export const FIZZ_PREFIX = "FM-1_46";          // firmware/app/fm1.toml id_prefix
export const XDATA_OFF = 0xE5000;              // fm1-base plat.h PLAT_XDATA1_LO (fizz_xdata.py XDATA_OFF)
export const XDATA_CMD = 0x46;
// the Bluetooth package (firmware/BUDGET.md "What the other tools need"): its first blob after fm1-base's Bluetooth
// store object (0xE5000, two sectors), its second at the data region's first byte (where the USB package keeps MY
// PRESETS: writing it erases them). Written after the install, in this order (either works; the first can be FZOK 0)
export const BLE_XDATA_OFF = 0xE7000, BLE_XDATA2_OFF = 0x97000;
export const isBlePort = (name) => /\sbluetooth$/i.test(String(name || "").trim());
export function usbOnly(access) {
  const usb = (m) => new Map([...m].filter(([, x]) => !isBlePort(x.name)));
  return { get inputs() { return usb(access.inputs); }, get outputs() { return usb(access.outputs); } };
}
const IDLE_MS = 30000;                         // (fizz_xdata.py: 30 s without a request or an answer)
export const DATA_ERRORS = {                   // fizz_xdata.py's FZER codes
  1: "the data is another build's", 2: "FIZZ was already running", 3: "erasing the flash failed",
  4: "a read or a write failed", 5: "writing the data's trailer failed", 6: "the data did not check out after writing",
  15: "it needs the USB cable (not over Bluetooth)",
};

const le32 = (v) => [v & 0xFF, (v >>> 8) & 0xFF, (v >>> 16) & 0xFF, (v >>> 24) & 0xFF];
const isFizz = (id) => !IS_OTA(id) && id.text.startsWith(FIZZ_PREFIX);

// zlib's CRC-32 (fizz_xdata.py: zlib.crc32)
const CRC_T = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; }
  return t;
})();
export function crc32(bytes) {
  let c = 0xFFFFFFFF;
  for (let i = 0; i < bytes.length; i++) c = CRC_T[(c ^ bytes[i]) & 0xFF] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}

// the unit's answer to FZXD: F0 pack7(00 59 46 08 00 00 tag[4] code[4] ~sum) F7 -> {tag, code} | null
export function parseDataReply(pkt) {
  if (pkt.length < 3 || pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const u = unpack7(pkt.slice(1, -1));
  if (u.length < 15 || u[0] !== 0 || u[1] !== 0x59 || u[2] !== XDATA_CMD || u[3] !== 8) return null;
  let s = 0;
  for (let i = 6; i < 14; i++) s += u[i];
  if ((~s & 0xFF) !== u[14]) return null;
  const tag = String.fromCharCode(...u.slice(6, 10));
  if (tag !== "FZOK" && tag !== "FZER") return null;
  return { tag, code: (u[10] | (u[11] << 8) | (u[12] << 16) | (u[13] << 24)) >>> 0 };
}

export async function pushData(access, blob, { off = XDATA_OFF, log = () => {}, onProgress = () => {}, waitMs = 40000 } = {}) {
  const up = new Updater(usbOnly(access), log);
  const dev = await up.waitFor(isFizz, waitMs);
  if (!dev) throw fail("nofizz", "no FM-1 running FIZZ answered");
  const link = dev.link;
  try {
    const crc = crc32(blob);
    log(`data: ${dev.id.text}, ${blob.length} B, CRC-32 ${crc.toString(16).padStart(8, "0")}, to flash 0x${off.toString(16)}`);
    link.drain();
    if (!link.send(frame(XDATA_CMD, [..."FZXD"].map((c) => c.charCodeAt(0)).concat(le32(blob.length), le32(crc), le32(off))))) {
      throw fail("lost", "the FM-1 was disconnected");
    }
    let last = Date.now(), served = 0;
    for (;;) {
      const p = await link.read(1000);
      if (!p) {
        if (link.lost) throw fail("lost", `the FM-1 was disconnected after ${served} reads`);
        if (Date.now() - last > IDLE_MS) throw fail("datastopped", `FIZZ stopped asking after ${served} reads`);
        continue;
      }
      const r = parseRequest(p);
      if (r) {
        last = Date.now();
        if (r.len > 512 || r.addr + r.len > blob.length) throw fail("badreq", `FIZZ asked for ${r.addr.toString(16)}+${r.len}, outside its data`);
        if (!link.send(response(r.addr, Array.from(blob.subarray(r.addr, r.addr + r.len)), r.fl))) {
          throw fail("lost", `the FM-1 was disconnected after ${served} reads`);
        }
        served++;
        onProgress(Math.min(r.addr + r.len, blob.length) / blob.length);
        continue;
      }
      const a = parseDataReply(p);
      if (!a) continue;
      if (a.tag === "FZOK") { log(`data: FZOK ${a.code} after ${served} reads`); onProgress(1); return { written: a.code !== 0 }; }
      throw fail("dataerr", `FZER ${a.code}`, DATA_ERRORS[a.code] || `code ${a.code}`);
    }
  } finally {
    link.close();
  }
}

export function versionOfId(id) {
  const m = new RegExp("^" + FIZZ_PREFIX + "(\\d\\d)(\\d\\d)(\\d\\d)$").exec(id || "");
  return m ? `${+m[1]}.${+m[2]}.${+m[3]}` : "";
}

// FIZZ's own IDENTITY (7D 46 5A 01 01): "FIZZ <APP_VERSION>"; null when FIZZ does not answer (no data yet, safe mode: SAVE held at power-on)
async function fizzVersion(link) {
  link.drain();
  if (!link.send(Array.from(fizzIdentity()))) return null;
  const end = Date.now() + 1500;
  while (Date.now() < end) {
    const p = await link.read(end - Date.now());
    if (!p) break;
    const r = parseReply(p);
    if (r && r.kind === "identity") return r.version.replace(/^FIZZ\s*/, "");
  }
  return null;
}

// FIZZ's STATUS (7D 46 5A 01 02 -> 42): its package ("usb" without the package bytes, a FIZZ before them too) | null
async function fizzPackage(link) {
  link.drain();
  if (!link.send(Array.from(statusRequest()))) return null;
  const end = Date.now() + 1500;
  while (Date.now() < end) {
    const p = await link.read(end - Date.now());
    if (!p) break;
    const r = parseReply(p);
    if (r && r.kind === "status") return r.package || "usb";
  }
  return null;
}

export async function readUnit(access) {
  const up = new Updater(usbOnly(access));
  const dev = await up.find(() => true);
  if (!dev) return null;
  try {
    const id = dev.id.text;
    if (IS_OTA(dev.id)) return { kind: "update", id };
    if (isFizz(dev.id)) {
      const version = await fizzVersion(dev.link);
      return { kind: "fizz", id, fromId: versionOfId(id), version, package: version ? await fizzPackage(dev.link) : null };
    }
    if (dev.id.open || IS_STOCK(id)) return { kind: "stock", id, open: !!dev.id.open };
    return { kind: "other", id };
  } finally {
    dev.link.close();
  }
}
