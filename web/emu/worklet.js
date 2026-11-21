// SPDX-License-Identifier: GPL-3.0-only
// FM-ARCAVE's emulator, the audio side: the WebAssembly FM-1 runs here, clocked by the sound the browser asks for.
// It gets { type: "load", wasm, store } and answers "ready"; then "buttons", "keys", "enc", "midi" come in and
// "frame" (the screen when it changed, the lights, MIDI out, the settings when they were saved) goes out, and "midi"
// (MIDI out between two frames, so a link to another FM-1 is not held up by the picture).
class FmArcave extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null; this.draws = -1; this.gen = 0; this.count = 0; this.hidden = false;
    this.port.onmessage = (e) => this.on(e.data);
  }
  async on(m) {
    if (m.type === "load") {
      try {
        const { instance } = await WebAssembly.instantiate(m.wasm, {});
        const ex = instance.exports;
        if (ex._initialize) ex._initialize();
        if (m.store && m.store.length) {
          new Uint8Array(ex.memory.buffer, ex.web_store(), m.store.length).set(m.store);
          ex.web_store_set(m.store.length);
        }
        ex.web_boot();
        this.gen = ex.web_store_gen();
        this.ex = ex;
        this.port.postMessage({ type: "ready" });
      } catch (err) { this.port.postMessage({ type: "error", text: String(err) }); }
      return;
    }
    const ex = this.ex;
    if (!ex) return;
    if (m.type === "buttons") ex.web_buttons(m.mask >>> 0);
    else if (m.type === "keys") ex.web_keys(m.mask >>> 0);
    else if (m.type === "enc") ex.web_enc(m.id, m.n);
    else if (m.type === "midi") ex.web_midi(m.st, m.d1, m.d2);
    else if (m.type === "hidden") this.hidden = m.on;
  }
  process(_in, out) {
    const ex = this.ex, o = out[0];
    if (!ex) return true;
    const n = o[0].length;
    ex.web_render(n);
    o[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));
    if (o[1]) o[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));
    this.count += n;
    if (this.count < (this.hidden ? 11025 : 1470)) {             // between pictures: MIDI out at once (UNDERSONG's link, the clock)
      const out = [];
      for (let v; (v = ex.web_midi_out()) >= 0;) out.push(v);
      if (out.length) this.port.postMessage({ type: "midi", midi: out });
    }
    if (this.count >= (this.hidden ? 11025 : 1470)) {            // 30 times a second; 4 when the tab is hidden
      this.count = 0;
      const msg = { type: "frame", leds: [ex.web_lit_buttons(), ex.web_lit_keys(), ex.web_dim_keys(), ex.web_breath_buttons()], midi: [],
                    clock: ex.web_clock_q8(), bpm: ex.web_bpm_q8(), state: ex.web_scene() };
      for (let v; (v = ex.web_midi_out()) >= 0;) msg.midi.push(v);
      const d = ex.web_screen_draws();
      const tr = [];
      if (d !== this.draws) {
        this.draws = d;
        msg.fb = new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240).slice();
        tr.push(msg.fb.buffer);
      }
      const g = ex.web_store_gen();
      if (g !== this.gen) { this.gen = g; msg.store = new Uint8Array(ex.memory.buffer, ex.web_store(), ex.web_store_len()).slice(); }
      this.port.postMessage(msg, tr);
    }
    return true;
  }
}
registerProcessor("fm-arcave", FmArcave);
