// SPDX-License-Identifier: GPL-3.0-only
// A FIZZ backup played in the browser's FIZZ (the owner's request, 2026-10-26: FIZZ with RemoteFM1, for videos): the
// FM-1's store from a .fizzbak (or a raw dump, firmware/tools/fizz_xdata.py backup) written again as the 1 MiB flash
// the player's FIZZ keeps (site/manager/fizzstore.js convertImage: the same records, the host's layout). The Manager
// opens the player with ?backup=<id>, the id of a backup it keeps in this browser (IndexedDB "fizz-manager").
//
//   await backupFlash(bytes, fileName) -> {flash (1 MiB), name, projects: [name], presets: n} | throws Error
//   await managerBackup(id)            -> the Manager's record {id, name, bytes, date, ..} | null
//   isBackupFile(bytes)                -> a .fizzbak (ZIP) or an FM-1 store dump
import { readBackup } from "../manager/fizzbak.js";
import { convertImage, layoutOf, LAYOUTS } from "../manager/fizzstore.js";

export const isBackupFile = (b) => (b.length > 4 && b[0] === 0x50 && b[1] === 0x4B && b[2] === 3 && b[3] === 4) ||
  b.length === LAYOUTS.device.size;

// the save's name: the file's, without its extension (the Saves list keeps names up to 40 characters)
export const saveName = (file) => (String(file || "FIZZ backup").replace(/\.(fizzbak|bin)$/i, "").trim() || "FIZZ backup").slice(0, 40);

export async function backupFlash(bytes, file = "") {
  if (!(bytes instanceof Uint8Array)) bytes = new Uint8Array(bytes);
  const b = await readBackup(bytes, file);
  if (!b.image) throw new Error(`${file || "This backup"} holds presets only: add them with Import sounds.`);
  if (!layoutOf(b.image.length)) throw new Error(`${file || "This backup"}: not a FIZZ store.`);
  const flash = convertImage(b.image, "host");
  return { flash, name: saveName(file), projects: b.summary.projects.map((p) => p.name), presets: b.summary.presets.length, tracks: b.summary.tracks };
}

// the Manager's backups (site/manager/manager.js DB): opened as the Manager opens it, so a page that runs first
// makes the same two stores
export function managerBackup(id) {
  return new Promise((resolve) => {
    try {
      const r = indexedDB.open("fizz-manager", 1);
      r.onupgradeneeded = () => { r.result.createObjectStore("backups", { keyPath: "id", autoIncrement: true }); r.result.createObjectStore("kv"); };
      r.onerror = () => resolve(null);
      r.onsuccess = () => {
        try {
          const t = r.result.transaction("backups", "readonly"), g = t.objectStore("backups").get(id);
          g.onsuccess = () => resolve(g.result || null);
          g.onerror = () => resolve(null);
        } catch (_) { resolve(null); }
      };
    } catch (_) { resolve(null); }
  });
}
