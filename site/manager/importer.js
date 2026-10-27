// SPDX-License-Identifier: GPL-3.0-only
// The manager's IMPORT tab (docs/WEB-MANAGER-PLAN.md §7, shot 5): FIZZ's own files (a .fizzlib, as EXPORT ALL writes
// it, .fizzsnd, a .fizzbak's presets: the owner's fix of 2026-10-25) and Digitone .dnsnd and Digitone II .dn2pst /
// .dn2snd files read and converted in this browser (js/digitone.js: ChoralRoot's reader, FIZZ's dn2pst.js; nothing is
// uploaded), each sound with a 10-segment bar of how much carried over (green exact, yellow small losses, orange
// approximations), its tags; a sound that is not FM TONE greyed; one already in MY PRESETS (the same name and patch),
// or twice in the files, flagged and not ticked. SEND stores the ticked ones on the FM-1 with STORE /
// STORE6 into MY PRESETS (POOL: also referenced in the project's pool; TRACK: the first also loaded onto that track).
// TRACK only with the header's TRACK switch on (P2-B). The Digitone tags FIZZ has no tag for are kept as web tags (on this computer and in backups). .syx is not read (the
// owner's decision: only the formats that already convert).
import { readFiles, storeFrame } from "../js/digitone.js";
import { sameSound, soundFromStoreFrame } from "../js/library.js";
import { FIZZ_TAGS } from "../js/fizz_sysex.js";
import { soundKey, readBackup } from "./fizzbak.js";
import { esc, refusal, midiChip } from "./ui.js";
import { isMidiBlob } from "./sound.js";

export const IMPORT_FILES = /\.(fizzlib|fizzsnd|fizzbak|dnsnd|dn2pst|dn2snd)$/i;

// (no DOM: the tests) files [{name, bytes}] -> [{file, ok, sound, error, web}]: a .fizzbak's presets (its web tags
// with them), a .fizzlib's, a .fizzsnd, the Digitone's converted
export async function readImport(files) {
  const out = [];
  for (const f of files) {
    if (/\.fizzbak$/i.test(f.name)) {
      try {
        const b = await readBackup(f.bytes, f.name);
        if (!b.presets.length) out.push({ file: f.name, ok: false, error: "no presets in this backup" });
        for (const s of b.presets) out.push({ file: f.name, ok: true, sound: s, web: (b.web && b.web.tags && b.web.tags[soundKey(s)]) || null });
      } catch (e) { out.push({ file: f.name, ok: false, error: e.message || String(e) }); }
      continue;
    }
    out.push(...(await readFiles([f])));
  }
  return out;
}
// each item's duplicate: "mine" (the same name and patch in MY PRESETS: have), "list" (earlier in the list), or ""
export function markDuplicates(items, have = []) {
  const seen = [];
  for (const it of items) {
    it.dup = "";
    if (!it.ok) continue;
    if (have.some((h) => sameSound(h, it.sound))) it.dup = "mine";
    else if (seen.some((s) => sameSound(s, it.sound))) it.dup = "list";
    seen.push(it.sound);
  }
  return items;
}

export function mountImport(ctx) {
  const { S, $, say, toast, refreshRows, loadDumps, log } = ctx;
  const I = { items: [], dest: "mine", track: 0, sending: false, cur: undefined };   // (cur: the row heard)
  const can = () => !!(S.link && !S.busy && S.unit && S.unit.version && !I.sending);
  const fid = (s) => (s.notes.length ? Math.max(3, 10 - s.notes.length) : 10);
  const fidC = (n) => (n >= 10 ? "var(--green)" : n >= 8 ? "var(--yellow)" : "var(--orange)");

  async function add(files) {
    const list = [];
    for (const f of files) {
      if (!IMPORT_FILES.test(f.name)) { say(`${esc(f.name)}: not a sound file (.fizzlib, .fizzsnd, .fizzbak, .dnsnd, .dn2pst, .dn2snd)`, "--orange"); continue; }
      list.push({ name: f.name, bytes: new Uint8Array(await f.arrayBuffer()) });
    }
    const got = await readImport(list);
    if (S.link && S.unit && !S.unit.dumps && loadDumps) await loadDumps();   // (MY PRESETS' sounds: the duplicates)
    const have = S.unit && S.unit.dumps ? [...S.unit.dumps.values()].map(soundFromStoreFrame) : [];
    const fresh = got.map((g) => ({ ...g, on: false, dup: "", result: null }));
    markDuplicates([...I.items.filter((x) => !x.result || !x.result.ok), ...fresh], have);
    for (const it of fresh) it.on = it.ok && !it.dup;
    I.items.push(...fresh);
    log(`import: ${got.length} read (${got.filter((g) => !g.ok).length} refused, ${fresh.filter((x) => x.dup).length} already there)`);
    render();
  }
  function render() {
    const n = I.items.filter((x) => x.on).length;
    $("i-count").textContent = `${I.items.length} sound${I.items.length === 1 ? "" : "s"}`;
    $("i-rows").innerHTML = I.items.map((it, k) => {
      if (!it.ok) {
        const why = (it.error || "").replace(/^Not importable: /, "");
        const fz = /\.fizz/i.test(it.file);
        return `<div class="ir no"><span class="chk" aria-hidden="true"></span><span class="src ${fz ? "fz" : /dn2/i.test(it.file) ? "dn2" : "dn1"}">${fz ? "FIZZ" : /dn2/i.test(it.file) ? "DN II" : "DN1"}</span>` +
          `<span class="nm">${esc(it.file.replace(/\.[^.]+$/, ""))}</span><span class="fid">${"<i></i>".repeat(10)}</span><span class="note">${esc(why.slice(0, 60))}</span><span></span></div>`;
      }
      const s = it.sound, f = fid(s), web = (s.tags || []).map((t) => String(t).toUpperCase()).filter((t) => !s.fizzTags.includes(t));
      const res = it.result ? (it.result.ok ? `<span class="okmark">✓ ${it.result.pool ? "POOL" : "MY"} ${it.result.slot}</span>` : `<span class="note bad">${esc(it.result.error)}</span>`) : "";
      return `<div class="ir ${it.on ? "" : "off"}${I.cur === k ? " cur" : ""}" data-k="${k}"><button class="chk ${it.on ? "on" : ""}" data-k="${k}" aria-pressed="${it.on}" aria-label="Send ${esc(s.name)}">${it.on ? '<svg viewBox="0 0 12 12"><path d="M2 6l3 3 5-6"/></svg>' : ""}</button>` +
        `<span class="src ${s.fizz ? "fz" : s.dn2 ? "dn2" : "dn1"}">${s.fizz ? "FIZZ" : s.dn2 ? "DN II" : "DN1"}</span><span class="nm">${esc(s.name)}</span>` +
        `<span class="fid" title="${esc(s.notes.join("\n") || "exact")}">${Array.from({ length: 10 }, (_, j) => `<i style="background:${j < f ? fidC(f) : "#24242b"}"></i>`).join("")}</span>` +
        `<span class="note ${it.dup ? "dup" : s.notes.length ? "lost" : ""}">${it.dup === "mine" ? "already in MY PRESETS" : it.dup === "list" ? "twice in these files" : esc((s.notes[0] || "exact").slice(0, 44))}</span>` +
        `<span class="tg">${res || [midiChip(isMidiBlob(s.blob)), ...s.fizzTags.map((t) => `<span class="tag t-${t}">${t}</span>`), ...web.slice(0, 2).map((t) => `<span class="tag web">${esc(t)}</span>`)].join("")}</span></div>`;
    }).join("");
    const mine = S.unit ? S.unit.rows.filter((r) => r.source === "mine").map((r) => r.slot) : [];
    const free = []; for (let s = 1; s <= 256 && free.length < n; s++) if (!mine.includes(s)) free.push(s);
    const run = free.length > 1 && free[free.length - 1] - free[0] === free.length - 1;
    const where = !free.length ? "" : free.length === 1 ? free[0] : run ? `${free[0]}–${free[free.length - 1]}` : `${free.slice(0, 3).join(", ")}${free.length > 3 ? " …" : ""}`;
    $("i-foot").innerHTML = n ? `<b>${n}</b> to ${I.dest === "pool" ? "MY PRESETS and the POOL" : `MY PRESETS ${where}`}${I.dest === "track" ? ` · the first onto TRACK ${I.track + 1}` : ""}` : "";
    $("i-send").textContent = `↓ SEND ${n} TO FM-1`;
    $("i-send").disabled = !n || !can();
    $("i-clear").hidden = !I.items.length;
    // (P2-B: TRACK only with the header's TRACK switch on: off, the Manager leaves the open project's tracks alone)
    const trk = !ctx.trackOn || ctx.trackOn();
    if (!trk && I.dest === "track") I.dest = "mine";
    $("i-dest").querySelectorAll("button").forEach((b) => {
      b.classList.toggle("on", b.dataset.d === I.dest);
      if (b.dataset.d === "track") { b.disabled = !trk; b.title = trk ? "" : "Turn TRACK on (in the header) to load onto a track"; }
    });
    $("i-track").hidden = I.dest !== "track";
  }
  async function send() {
    if (!can()) return;
    const go = I.items.filter((x) => x.on && x.ok);
    if (!go.length) return;
    I.sending = true; render();
    say(""); let n = 0, full = false;
    try {
      for (let i = 0; i < go.length; i++) {
        const it = go[i], s = it.sound;
        $("i-send").textContent = `↓ ${i + 1} / ${go.length}`;
        const f = storeFrame(s, { pool: I.dest === "pool", batch: i > 0, track: I.dest === "track" && i === 0 ? I.track : -1 });
        const r = await S.link.store(f);
        if (r && r.kind === "store" && r.status === 0) {
          it.result = { ok: true, slot: r.slot, pool: r.pool };
          it.on = false; n++;
          const web = [...(s.tags || []), ...(it.web || [])].map((t) => String(t).toUpperCase()).filter((t) => !FIZZ_TAGS.includes(t) && !s.fizzTags.includes(t));
          if (web.length) S.web.tags[soundKey(s)] = [...new Set([...(S.web.tags[soundKey(s)] || []), ...web])];
        } else {
          it.result = { ok: false, error: r && r.status === 1 ? "FULL" : refusal(r).replace(/<[^>]+>/g, "") };
          if (r && r.status === 1) { full = true; break; }
        }
        render();
      }
    } catch (e) { say(`Import stopped: ${esc(e.message)}`, "--red"); } finally { I.sending = false; }
    if (n) { ctx.saveWeb(); toast(`+${n} on the FM-1${full ? " · FULL" : ""}`, full ? "--orange" : "--violet"); }
    if (S.unit) S.unit.dumps = null;
    await refreshRows();
    loadDumps();
    render();
  }

  const drop = $("i-drop");
  $("i-file").addEventListener("change", async (ev) => { const f = [...ev.target.files]; ev.target.value = ""; if (f.length) await add(f); });
  ["dragenter", "dragover"].forEach((t) => drop.addEventListener(t, (ev) => { ev.preventDefault(); drop.classList.add("over"); }));
  ["dragleave", "drop"].forEach((t) => drop.addEventListener(t, () => drop.classList.remove("over")));
  drop.addEventListener("drop", async (ev) => { ev.preventDefault(); await add([...ev.dataTransfer.files]); });
  $("i-rows").addEventListener("click", (ev) => {
    const b = ev.target.closest(".chk[data-k]");
    if (b) {
      if (I.sending) return;
      const it = I.items[+b.dataset.k];
      if (it && it.ok) { it.on = !it.on; render(); }
      return;
    }
    const r = ev.target.closest(".ir[data-k]");         // (the owner, 2026-10-27: a row picked is heard on the keys)
    if (r) hear(+r.dataset.k);
  });
  function hear(k) {
    const it = I.items[k];
    if (!it || !it.ok) return;
    I.cur = k;
    $("i-rows").querySelectorAll(".ir.cur").forEach((x) => x.classList.remove("cur"));
    const el = $("i-rows").querySelector(`.ir[data-k="${k}"]`);
    if (el) { el.classList.add("cur"); el.scrollIntoView({ block: "nearest" }); }
    if (ctx.previewLib) ctx.previewLib(it.sound, it.sound.name);
  }
  // up / down: the next sound heard; SPACE ticks it (a big dump looked through, some of it brought in)
  document.addEventListener("keydown", (ev) => {
    if ($("tab-import").hidden || ev.metaKey || ev.ctrlKey || ev.altKey || (ev.target && /^(INPUT|TEXTAREA|SELECT)$/.test(ev.target.tagName))) return;
    const d = { ArrowDown: 1, ArrowUp: -1 }[ev.key];
    if (d) {
      ev.preventDefault();
      let k = I.cur ?? -1;
      do k += d; while (k >= 0 && k < I.items.length && !I.items[k].ok);
      if (k >= 0 && k < I.items.length) hear(k);
      return;
    }
    if (ev.key === " " && I.cur !== undefined && I.items[I.cur] && I.items[I.cur].ok && !I.sending) {
      ev.preventDefault();
      const it = I.items[I.cur]; it.on = !it.on; render();
    }
  });
  $("i-dest").addEventListener("click", (ev) => { const b = ev.target.closest("button[data-d]"); if (b) { I.dest = b.dataset.d; render(); } });
  $("i-track").addEventListener("change", (ev) => { I.track = +ev.target.value; render(); });
  $("i-send").addEventListener("click", send);
  $("i-clear").addEventListener("click", () => { I.items.length = 0; I.cur = undefined; render(); });
  return { render, add, state: I, send };
}
