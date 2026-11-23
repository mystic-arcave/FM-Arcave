// SPDX-License-Identifier: GPL-3.0-only
// PLAY WITH MY FM-1: a real FM-1, plugged in, as the controller of the game in this page. The FM-1 sends the save it holds,
// then its buttons, keys and knobs; the page sends it the game's lights and every save the game makes, which the FM-1
// keeps. Plain control changes on MIDI channel 16, as firmware/src/arcave/ac_remote.h describes them.
//   const r = new Fm1Remote(midiAccess, { onState, onSave, onInput, onKnob, onKept });  r.start(); ... r.stop();
//   r.tick() about every 20 ms (hello, the save by Bluetooth, noticing the FM-1 gone); r.lights([lit, keys, dim, breath]);
//   r.sendSave(bytes)
// By Bluetooth (web/link/ble.js's BlePorts): new Fm1Remote(bleAccess(port), ...), and the port's messages to access.heard.
// Everything that takes time comes from opts.now (performance.now in a browser), so a test can run it on its own clock.
const CH16 = 0xbf, HELLO = 78, HERE = 70, BTN = 71, KEY = 72, KNOB = 73, LEN_HI = 74, LEN_LO = 75, DATA = 76, SUM = 77,
  KEPT = 79, LIGHTS = 80;
const HELLO_MS = 500, ABSENT_MS = 4000, LOST_MS = 2500, GAP_MS = 0.5;   // the save: two messages a millisecond
const CHUNK = 42;                                 // by Bluetooth: seven packets of the save at a time (six to a packet)

export function pack7(bytes) {                    // seven bytes at a time: their top bits, then each byte's low seven
  const out = [];
  for (let i = 0; i < bytes.length; i += 7) {
    let h = 0;
    for (let k = 0; k < 7 && i + k < bytes.length; k++) h |= (bytes[i + k] >> 7) << k;
    out.push(h);
    for (let k = 0; k < 7 && i + k < bytes.length; k++) out.push(bytes[i + k] & 127);
  }
  return out;
}

// one FM-1 over Bluetooth (a port of web/link/ble.js's BlePorts) as Web MIDI's access to it: one input, one output, one
// name. The output also says how much is still waiting to be written (backlog), which paces a save (Fm1Remote.pump)
export function bleAccess(port) {
  const input = { name: port.name, onmidimessage: null };
  const output = {
    name: port.name,
    send: (d) => port.send(d),
    backlog: () => (port.state === "on" ? port.out.q.length + (port.out.busy ? 1 : 0) : Infinity),
  };
  return {
    inputs: new Map([[port.id, input]]), outputs: new Map([[port.id, output]]), onstatechange: null,
    heard: (d, t) => input.onmidimessage?.({ data: d, target: input, timeStamp: t }),
  };
}

export class Fm1Remote {
  constructor(access, opts = {}) {
    this.access = access; this.o = opts;
    this.now = opts.now || (() => performance.now());
    this.state = "off"; this.input = null; this.output = null;
    this.nonce = 1 + Math.floor(Math.random() * 127);
    this.btn = 0; this.keys = 0; this.lit = new Array(13).fill(-1);
    this.rx = null; this.sendAt = 0; this.txq = []; this.lastHello = 0; this.lastHere = 0; this.started = 0;
    this.onMsg = (e) => this.msg(e);
  }
  set(s, detail) { if (s !== this.state || detail) { this.state = s; this.o.onState?.(s, detail); } }
  outs() { return this.output ? [this.output] : [...this.access.outputs.values()]; }
  cc(d1, d2, at) { for (const o of this.outs()) at ? o.send([CH16, d1, d2 & 127], at) : o.send([CH16, d1, d2 & 127]); }
  listen() { for (const i of this.access.inputs.values()) if (!this.input || i === this.input) i.onmidimessage = this.onMsg; }

  start() {
    this.input = null; this.output = null; this.btn = 0; this.keys = 0; this.lit.fill(-1); this.rx = null; this.txq = [];
    this.started = this.now(); this.lastHere = 0; this.lastHello = -1e9;
    this.access.onstatechange = () => this.listen();
    this.listen();
    this.set("searching");
    this.tick();
  }
  stop() {
    if (this.state === "off") return;
    this.cc(HELLO, 0);                           // goodbye: the FM-1 is itself again
    for (const i of this.access.inputs.values()) if (i.onmidimessage === this.onMsg) i.onmidimessage = null;
    this.access.onstatechange = null;
    this.releaseAll(); this.txq = [];
    this.set("off");
  }
  tick() {
    if (this.state === "off" || this.state === "absent") return;
    const t = this.now();
    this.pump();
    if (t - this.lastHello >= HELLO_MS) { this.lastHello = t; this.cc(HELLO, this.nonce); }
    if (this.state === "searching" && t - this.started > ABSENT_MS) { this.stop(); this.set("absent"); }
    else if (this.state !== "searching" && t - this.lastHere > LOST_MS) {
      for (const i of this.access.inputs.values()) if (i.onmidimessage === this.onMsg) i.onmidimessage = null;
      this.releaseAll(); this.txq = []; this.set("lost"); this.state = "off";
    }
  }
  releaseAll() {
    if (this.btn || this.keys) { this.btn = 0; this.keys = 0; this.o.onInput?.(0, 0); }
  }
  // the game's lights (the worklet's frame.leds), only the groups that changed
  lights(leds) {
    if (this.state !== "connected") return;
    const [lit, keys, dim, br] = leds, g = [lit, lit >>> 7, lit >>> 14, keys, keys >>> 7, keys >>> 14, keys >>> 21,
      dim, dim >>> 7, dim >>> 14, dim >>> 21, br, br >>> 7];
    for (let i = 0; i < 13; i++) { const v = g[i] & 127; if (v !== this.lit[i]) { this.lit[i] = v; this.cc(LIGHTS + i, v); } }
  }
  // a save of the game's, to be kept on the FM-1: paced, so the FM-1's ring of 64 coming in is never overrun
  sendSave(bytes) {
    if (this.state !== "connected") return;
    let sum = 0; for (const b of bytes) sum += b;
    const m = [[LEN_HI, bytes.length >> 7], [LEN_LO, bytes.length], ...pack7(bytes).map((v) => [DATA, v]),
      [SUM, (sum >> 7) & 127], [SUM, sum & 127]];
    if (this.outs().some((o) => o.backlog)) {     // Bluetooth: a newer save takes the place of one not yet all out (the
      this.txq = m; this.pump();                  // FM-1 drops a save cut short by the next one's length)
      return;
    }
    let at = Math.max(this.sendAt, this.now());
    for (const [d1, d2] of m) { at += GAP_MS; this.cc(d1, d2, at); }
    this.sendAt = at;
  }
  // by Bluetooth: the save a few packets at a time, the next only once the last is all but written, so a hello or a
  // light never waits long behind it and nothing piles up to be dropped
  pump() {
    if (!this.txq.length || this.state !== "connected") return;
    const o = this.outs()[0];
    while (this.txq.length && o.backlog() < 2) {
      const b = [];
      for (const [d1, d2] of this.txq.splice(0, CHUNK)) b.push(CH16, d1, d2 & 127);
      o.send(b);
    }
  }

  msg(e) {
    const d = e.data;
    if (d.length < 3 || d[0] !== CH16 || d[1] < HERE || d[1] > KEPT) return;
    const [, c, v] = d;
    if (!this.input) {                           // the first FM-1 to answer is the one; its output has its name
      if (c !== HERE) return;
      this.input = e.target || e.currentTarget || null;
      const named = this.input && [...this.access.outputs.values()].find((o) => o.name === this.input.name);
      if (named) this.output = named;
      for (const i of this.access.inputs.values()) if (i !== this.input && i.onmidimessage === this.onMsg) i.onmidimessage = null;
    }
    if (c === HERE) {
      this.lastHere = this.now();
      if (this.state === "searching") this.set("loading");
      return;
    }
    if (this.state === "searching") return;
    if (c === BTN || c === KEY) {
      const bit = 1 << (v & 63), down = v & 64;
      if (c === BTN) this.btn = down ? this.btn | bit : this.btn & ~bit;
      else this.keys = down ? this.keys | bit : this.keys & ~bit;
      this.o.onInput?.(this.btn >>> 0, this.keys >>> 0);
    } else if (c === KNOB) this.o.onKnob?.(v >> 4, (v & 15) - 8);
    else if (c === LEN_HI) this.rx = { hi: v, len: -1 };
    else if (c === LEN_LO && this.rx) Object.assign(this.rx, { len: this.rx.hi << 7 | v, bytes: [], k: 0, hdr: 0, sum: 0, ck: -1 });
    else if (c === DATA && this.rx && this.rx.len >= 0) {
      const r = this.rx;
      if (r.k === 0) r.hdr = v;
      else { const b = v | ((r.hdr >> (r.k - 1)) & 1) << 7; r.bytes.push(b); r.sum += b; }
      if (++r.k === 8 || (r.k > 1 && r.bytes.length >= r.len)) r.k = 0;
    } else if (c === SUM && this.rx && this.rx.len >= 0) {
      const r = this.rx;
      if (r.ck < 0) { r.ck = v << 7; return; }
      r.ck |= v; this.rx = null;
      if (r.bytes.length !== r.len || (r.sum & 0x3fff) !== r.ck) {   // damaged on the way: a new number has it sent again
        this.nonce = this.nonce % 127 + 1; this.lastHello = -1e9; this.set(this.state, "damaged"); this.tick(); return;
      }
      this.set("connected");
      this.o.onSave?.(Uint8Array.from(r.bytes));
    } else if (c === KEPT) this.o.onKept?.(v);
  }
}
