// SPDX-License-Identifier: GPL-3.0-only
// The Digitone II's USB audio as an input: the device list, a level meter, and recordings (mono: the mean of the
// channels; no echo cancelling, no noise suppression, no automatic gain).
export async function audioInputs() {
  try {                                                  // (labels need a permission first)
    const s = await navigator.mediaDevices.getUserMedia({ audio: true });
    s.getTracks().forEach((t) => t.stop());
  } catch (_) { /* refused: the list has no names */ }
  return (await navigator.mediaDevices.enumerateDevices()).filter((d) => d.kind === "audioinput");
}

export class Recorder {
  // deviceId: a real input; makeStream(ctx): a stream instead (the mock)
  async open({ deviceId, makeStream } = {}) {
    this.close();
    this.ctx = new AudioContext();
    await this.ctx.resume().catch(() => {});
    this.stream = makeStream ? makeStream(this.ctx) : await navigator.mediaDevices.getUserMedia({ audio: {
      deviceId: deviceId ? { exact: deviceId } : undefined, echoCancellation: false, noiseSuppression: false,
      autoGainControl: false, channelCount: 2 } });
    this.src = this.ctx.createMediaStreamSource(this.stream);
    this.an = this.ctx.createAnalyser();
    this.an.fftSize = 2048;
    this.proc = this.ctx.createScriptProcessor(2048, 2, 1);
    this.mute = this.ctx.createGain();
    this.mute.gain.value = 0;
    this.chunks = null;
    this.proc.onaudioprocess = (e) => {
      if (!this.chunks) return;
      const b = e.inputBuffer, n = b.length, ch = b.numberOfChannels, m = new Float32Array(n);
      for (let c = 0; c < ch; c++) { const d = b.getChannelData(c); for (let i = 0; i < n; i++) m[i] += d[i] / ch; }
      this.chunks.push(m);
    };
    this.src.connect(this.an);
    this.src.connect(this.proc);
    this.proc.connect(this.mute);
    this.mute.connect(this.ctx.destination);
    this.buf = new Float32Array(this.an.fftSize);
    return this;
  }
  get rate() { return this.ctx ? this.ctx.sampleRate : 48000; }
  get open_() { return !!this.ctx; }
  peakDb() {
    if (!this.an) return -Infinity;
    this.an.getFloatTimeDomainData(this.buf);
    let p = 0;
    for (const x of this.buf) p = Math.max(p, Math.abs(x));
    return p > 0 ? 20 * Math.log10(p) : -Infinity;
  }
  start() { this.chunks = []; }
  stop() {
    const c = this.chunks || [];
    this.chunks = null;
    const out = new Float32Array(c.reduce((a, b) => a + b.length, 0));
    let o = 0;
    for (const b of c) { out.set(b, o); o += b.length; }
    return out;
  }
  close() {
    if (this.stream) this.stream.getTracks().forEach((t) => t.stop());
    if (this.ctx) this.ctx.close().catch(() => {});
    this.ctx = this.stream = this.src = this.an = this.proc = null;
  }
}
