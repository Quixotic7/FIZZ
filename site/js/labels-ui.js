// SPDX-License-Identifier: GPL-3.0-only
// FIZZ's live labels on the drawn panel (js/labels.js gives them): a layer of text over the device canvas, under each
// knob, on each button and on each key, placed by the panel's own geometry (panel.js GEOM) and sized with it. A label
// that changed glows for a moment, so holding SHIFT visibly flips the panel. The computer key that presses a control
// shows beside its label only while the pointer is over it. Too small to read (a phone's strip): that group is left out.
import { GEOM, B, E, KM, NB, NE, NKEY, hintText, labelToMatrix, roleToMatrix } from "./panel.js";
import { css } from "./theme.js";

const MIN_PX = 7;                       // smaller than this on screen: not drawn
const GLOW_MS = 700;

export class LabelsLayer {
  // wrap: the element the canvas sits in (#device-wrap); canvas: the device canvas; device: its DeviceView
  constructor({ wrap, canvas, device }) {
    this.wrap = wrap; this.canvas = canvas; this.device = device;
    this.on = true; this.labels = null;
    this.el = document.createElement("div");
    this.el.className = "labels";
    this.el.setAttribute("aria-hidden", "true");
    wrap.append(this.el);
    const mk = (cls, hint) => {
      const d = document.createElement("div");
      d.className = "lbl " + cls;
      const t = document.createElement("span"); t.className = "t";
      d.append(t);
      if (hint) { const k = document.createElement("kbd"); k.textContent = hint; d.append(k); }
      this.el.append(d);
      return d;
    };
    // (the knobs' computer keys: Page Up / Down pick SELECT or KNOB 1..4, the arrows turn it; ALGORITHM, PRESETS and
    // MASTER have none)
    this.knobs = Array.from({ length: NE }, (_, r) => mk("knob", r === E.SELECT || (r >= E.K1 && r <= E.K4) ? "PgUp/Dn ↑↓" : ""));
    this.btns = Array.from({ length: NB }, (_, b) => mk(b >= B.OCTDN ? "btn oct" : "btn", hintText(KM.BTN, b)));
    this.keys = Array.from({ length: NKEY }, (_, k) => mk("key", hintText(KM.KEY, k)));
    this.timers = new Map();
    // the computer key, on hover only
    let hover = null;
    const setHover = (d) => { if (hover === d) return; if (hover) hover.classList.remove("hover"); hover = d; if (d) d.classList.add("hover"); };
    canvas.addEventListener("pointermove", (e) => {
      if (e.pointerType !== "mouse") return;
      const r = canvas.getBoundingClientRect(), u = device.toUnits(e.clientX - r.left, e.clientY - r.top), h = device.hit(u.x, u.y);
      setHover(!h ? null : h.kind === KM.KEY ? this.keys[h.idx] : h.kind === KM.BTN ? this.btns[h.idx] : h.kind === KM.SELECT ? this.knobs[h.idx] : null);
    });
    canvas.addEventListener("pointerleave", () => setHover(null));
    this.show(true);
  }

  setTheme(pal) {
    this.el.style.setProperty("--lbl-plate", css(pal.label));
    this.el.style.setProperty("--lbl-cap", css(pal.capText));
    this.el.style.setProperty("--lbl-key", css(pal.noteText));
  }
  // labels: js/labels.js's shape (matrix orders), or null (none yet: the printed names)
  set(labels) {
    const first = !this.labels;
    this.labels = labels;
    if (!labels) { this.render(); return; }
    const put = (d, s) => {
      const t = d.firstChild, v = String(s || "");
      if (t.textContent === v) return;
      t.textContent = v;
      d.classList.toggle("none", !v);
      d.classList.toggle("long", v.length > 6);
      if (!first && v) {                      // (a glow, restarted on each change)
        d.classList.remove("glow"); void d.offsetWidth; d.classList.add("glow");
        clearTimeout(this.timers.get(d));
        this.timers.set(d, setTimeout(() => d.classList.remove("glow"), GLOW_MS));
      }
    };
    for (let r = 0; r < NE; r++) put(this.knobs[r], r === E.MASTER ? labels.master : labels.enc[roleToMatrix(r)]);
    for (let b = 0; b < NB; b++) put(this.btns[b], labels.buttons[labelToMatrix(b)]);
    for (let k = 0; k < NKEY; k++) put(this.keys[k], labels.keys[k]);
    this.render();
  }
  show(on) { this.on = !!on; this.render(); }
  get visible() { return this.on && !!this.labels; }
  render() {
    const vis = this.visible;
    this.el.hidden = !vis;
    this.device.setLabelled(vis);
    if (vis) this.layout();
  }
  // place every label (CSS pixels of the canvas) and drop the groups too small to read
  layout() {
    if (!this.visible) return;
    const c = this.canvas, u = this.device.unitPx();
    Object.assign(this.el.style, { left: c.offsetLeft + "px", top: c.offsetTop + "px", width: c.clientWidth + "px", height: c.clientHeight + "px" });
    const place = (d, x, y, w, h, size) => {
      const p = this.device.toCss(x, y);
      Object.assign(d.style, { left: p.x + "px", top: p.y + "px", width: w * u + "px", height: h * u + "px", fontSize: size * u + "px" });
    };
    const fits = (size) => size * u >= MIN_PX;
    const kOk = fits(9), bOk = fits(7.5), keyOk = fits(7);
    this.el.classList.toggle("no-knobs", !kOk);
    this.el.classList.toggle("no-btns", !bOk);
    this.el.classList.toggle("no-keys", !keyOk);
    for (let r = 0; r < NE; r++) place(this.knobs[r], GEOM.ENC_CX[r] - 40, GEOM.ENC_CY[r] + GEOM.ENC_R + 3, 80, 12, 9);
    for (let b = 0; b < NB; b++) { const r = GEOM.btns[b]; place(this.btns[b], r.x, r.y, r.w, r.h, b >= B.OCTDN ? 6.5 : 7.5); }
    for (let k = 0; k < NKEY; k++) { const r = GEOM.keys[k]; place(this.keys[k], r.x, r.y + 48, r.w, 26, 7); }
    // the device view hides its own names only where labels are drawn in their place
    this.device.labelGroups = { btns: bOk, keys: keyOk };
  }
}
