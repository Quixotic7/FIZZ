// SPDX-License-Identifier: GPL-3.0-only
// The manager's IMPORT tab (docs/WEB-MANAGER-PLAN.md §7, shot 5): Digitone .dnsnd and Digitone II .dn2pst / .dn2snd
// files read and converted in this browser (js/digitone.js: ChoralRoot's reader, FIZZ's dn2pst.js; nothing is
// uploaded), each sound with a 10-segment bar of how much carried over (green exact, yellow small losses, orange
// approximations), its tags; a sound that is not FM TONE greyed. SEND stores the ticked ones on the FM-1 with STORE /
// STORE6 into MY PRESETS (POOL: also referenced in the project's pool; TRACK: the first also loaded onto that track).
// The Digitone tags FIZZ has no tag for are kept as web tags (on this computer and in backups). .syx is not read (the
// owner's decision: only the formats that already convert).
import { readFiles, storeFrame } from "../js/digitone.js";
import { sameSound, soundFromStoreFrame } from "../js/library.js";
import { FIZZ_TAGS } from "../js/fizz_sysex.js";
import { soundKey } from "./fizzbak.js";
import { esc, refusal } from "./ui.js";

export const IMPORT_FILES = /\.(dnsnd|dn2pst|dn2snd)$/i;

export function mountImport(ctx) {
  const { S, $, say, toast, refreshRows, loadDumps, log } = ctx;
  const I = { items: [], dest: "mine", track: 0, sending: false };
  const can = () => !!(S.link && !S.busy && S.unit && S.unit.version && !I.sending);
  const fid = (s) => (s.notes.length ? Math.max(3, 10 - s.notes.length) : 10);
  const fidC = (n) => (n >= 10 ? "var(--green)" : n >= 8 ? "var(--yellow)" : "var(--orange)");

  async function add(files) {
    const list = [];
    for (const f of files) {
      if (!IMPORT_FILES.test(f.name)) { say(`${esc(f.name)}: not a Digitone sound (.dnsnd, .dn2pst, .dn2snd)`, "--orange"); continue; }
      list.push({ name: f.name, bytes: new Uint8Array(await f.arrayBuffer()) });
    }
    const got = await readFiles(list);
    const have = S.unit && S.unit.dumps ? [...S.unit.dumps.values()].map(soundFromStoreFrame) : [];
    for (const g of got) {
      const dup = g.ok && have.some((h) => sameSound(h, g.sound));
      I.items.push({ ...g, on: g.ok && !dup, dup, result: null });
    }
    log(`import: ${got.length} read (${got.filter((g) => !g.ok).length} refused)`);
    render();
  }
  function render() {
    const n = I.items.filter((x) => x.on).length;
    $("i-count").textContent = `${I.items.length} sound${I.items.length === 1 ? "" : "s"}`;
    $("i-rows").innerHTML = I.items.map((it, k) => {
      if (!it.ok) {
        const why = (it.error || "").replace(/^Not importable: /, "");
        return `<div class="ir no"><span class="chk" aria-hidden="true"></span><span class="src ${/dn2/i.test(it.file) ? "dn2" : "dn1"}">${/dn2/i.test(it.file) ? "DN II" : "DN1"}</span>` +
          `<span class="nm">${esc(it.file.replace(/\.[^.]+$/, ""))}</span><span class="fid">${"<i></i>".repeat(10)}</span><span class="note">${esc(why.slice(0, 60))}</span><span></span></div>`;
      }
      const s = it.sound, f = fid(s), web = (s.tags || []).map((t) => String(t).toUpperCase()).filter((t) => !s.fizzTags.includes(t));
      const res = it.result ? (it.result.ok ? `<span class="okmark">✓ ${it.result.pool ? "POOL" : "MY"} ${it.result.slot}</span>` : `<span class="note bad">${esc(it.result.error)}</span>`) : "";
      return `<div class="ir ${it.on ? "" : "off"}" data-k="${k}"><button class="chk ${it.on ? "on" : ""}" data-k="${k}" aria-pressed="${it.on}" aria-label="Send ${esc(s.name)}">${it.on ? '<svg viewBox="0 0 12 12"><path d="M2 6l3 3 5-6"/></svg>' : ""}</button>` +
        `<span class="src ${s.dn2 ? "dn2" : "dn1"}">${s.dn2 ? "DN II" : "DN1"}</span><span class="nm">${esc(s.name)}</span>` +
        `<span class="fid" title="${esc(s.notes.join("\n") || "exact")}">${Array.from({ length: 10 }, (_, j) => `<i style="background:${j < f ? fidC(f) : "#24242b"}"></i>`).join("")}</span>` +
        `<span class="note ${s.notes.length ? "lost" : ""}">${it.dup ? "already in MY PRESETS" : esc((s.notes[0] || "exact").slice(0, 44))}</span>` +
        `<span class="tg">${res || [...s.fizzTags.map((t) => `<span class="tag t-${t}">${t}</span>`), ...web.slice(0, 2).map((t) => `<span class="tag web">${esc(t)}</span>`)].join("")}</span></div>`;
    }).join("");
    const mine = S.unit ? S.unit.rows.filter((r) => r.source === "mine").map((r) => r.slot) : [];
    const free = []; for (let s = 1; s <= 256 && free.length < n; s++) if (!mine.includes(s)) free.push(s);
    const run = free.length > 1 && free[free.length - 1] - free[0] === free.length - 1;
    const where = !free.length ? "" : free.length === 1 ? free[0] : run ? `${free[0]}–${free[free.length - 1]}` : `${free.slice(0, 3).join(", ")}${free.length > 3 ? " …" : ""}`;
    $("i-foot").innerHTML = n ? `<b>${n}</b> to ${I.dest === "pool" ? "MY PRESETS and the POOL" : `MY PRESETS ${where}`}${I.dest === "track" ? ` · the first onto TRACK ${I.track + 1}` : ""}` : "";
    $("i-send").textContent = `↓ SEND ${n} TO FM-1`;
    $("i-send").disabled = !n || !can();
    $("i-clear").hidden = !I.items.length;
    $("i-dest").querySelectorAll("button").forEach((b) => b.classList.toggle("on", b.dataset.d === I.dest));
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
          const web = (s.tags || []).map((t) => String(t).toUpperCase()).filter((t) => !FIZZ_TAGS.includes(t) && !s.fizzTags.includes(t));
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
    if (!b || I.sending) return;
    const it = I.items[+b.dataset.k];
    if (it && it.ok) { it.on = !it.on; render(); }
  });
  $("i-dest").addEventListener("click", (ev) => { const b = ev.target.closest("button[data-d]"); if (b) { I.dest = b.dataset.d; render(); } });
  $("i-track").addEventListener("change", (ev) => { I.track = +ev.target.value; render(); });
  $("i-send").addEventListener("click", send);
  $("i-clear").addEventListener("click", () => { I.items.length = 0; render(); });
  return { render, add, state: I, send };
}
