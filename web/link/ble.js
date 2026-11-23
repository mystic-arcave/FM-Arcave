// SPDX-License-Identifier: GPL-3.0-only
// FM-1s over Bluetooth, straight from the page (Web Bluetooth): each one a port the Link pages treat as they treat a
// USB one. Both pages use it: the LOCAL hub and the ONLINE lobby. Three parts:
//   the packets    BLE-MIDI (MIDI Association, "MIDI over Bluetooth Low Energy" 1.0) to and from plain MIDI messages,
//                  the same rules as the FM-1's own codec (firmware/bt/ble/ble_midi.c); tests/ble_web_test.mjs holds
//                  the two against each other
//   BlePorts       the FM-1s chosen on this page: connect, listen, send in order, come back after a break
//   Twins          one FM-1 heard on two roads at once (USB and Bluetooth, or Bluetooth through the computer's own
//                  MIDI and through this page): only one of the two counts
// It runs in a page and in Node (the tests); nothing here touches the document except bleRows and bleSay at the end.

// the BLE-MIDI service and its one characteristic (as the FM-1's stack carries them: ble_host.h, ble_att.c)
export const MIDI_SERVICE = "03b80e5a-ede8-4b33-a751-6ce34ec4c700";
export const MIDI_IO = "7772e5db-3868-4112-a1a9-f2669d106bf3";
export const NAME_PREFIX = "FM-ARCAVE";                      // each FM-1 adds four characters of its own: "FM-ARCAVE 3F2A"
// what one write may hold: the smallest ATT MTU (23) less its three octets. A page is not told the MTU that was
// agreed, so nothing longer is ever sent: every Bluetooth stack takes this
export const BLE_CAP = 20;

const now = () => (globalThis.performance ? performance.now() : Date.now());
const dataLen = (st) => (st < 0xF0 ? ((st & 0xE0) === 0xC0 ? 1 : 2) : st === 0xF2 ? 2 : st === 0xF1 || st === 0xF3 ? 1 : 0);

// ---------------------------------------------------------------- the packets
// plain MIDI octets (as Web MIDI's send takes them: whole messages, one after another) -> messages. Channel messages
// and real time (F8 FA FB FC ...) are kept; anything else is left out: the FM-1 takes nothing else over Bluetooth
export function midiMessages(bytes) {
  const out = [];
  for (let i = 0; i < bytes.length;) {
    const st = bytes[i];
    if (st >= 0xF8) { out.push([st]); i++; continue; }
    if (st < 0x80 || st >= 0xF0) { i++; continue; }
    const n = dataLen(st), m = [st];
    if (i + n >= bytes.length) break;                        // cut short
    for (let k = 1; k <= n; k++) m.push(bytes[i + k]);
    if (m.some((b, k) => k && (b & 0x80))) { i++; continue; }   // a status where data should be: not a message
    out.push(m);
    i += n + 1;
  }
  return out;
}
// one packet from the front of a queue of groups (a group: the messages of one send, which belong together). Whole
// groups go in while they fit; a group longer than a packet is cut between two messages. What went in is taken off
// the queue. A packet: a header (10hhhhhh, the time's bits 12..7), then for each message a time octet (1lllllll)
// and the message; a status the same as the one before is left out (running status), its time octet kept: six
// control changes on one channel are 20 octets, one write. Every message in a packet carries the packet's time
export function blePack(queue, tMs, cap = BLE_CAP) {
  const t = Math.floor(tMs) & 0x1FFF, lo = 0x80 | (t & 127), p = [0x80 | (t >> 7)];
  let rs = 0, any = false;
  const size = (m, r) => (m[0] >= 0xF8 ? 2 : (m[0] === r ? 1 : 2) + m.length - 1);
  while (queue.length) {
    const g = queue[0];
    let need = 0, r = rs;
    for (const m of g) { need += size(m, r); if (m[0] < 0xF8) r = m[0]; }
    const whole = p.length + need <= cap;
    if (!whole && any) break;                                // the next group whole in the next packet
    while (g.length) {
      const m = g[0];
      if (p.length + size(m, rs) > cap) break;
      p.push(lo);
      if (m[0] >= 0xF8) p.push(m[0]);
      else { if (m[0] !== rs) p.push(m[0]); for (let k = 1; k < m.length; k++) p.push(m[k] & 127); rs = m[0]; }
      g.shift(); any = true;
    }
    if (g.length) break;                                     // (cut: the rest of it leads the next packet)
    queue.shift();
  }
  return any ? Uint8Array.from(p) : null;
}
// plain MIDI octets -> the packets that carry them, in order
export function bleEncode(bytes, tMs = now(), cap = BLE_CAP) {
  const q = [midiMessages(bytes)], out = [];
  if (!q[0].length) return out;
  for (let p; (p = blePack(q, tMs, cap));) out.push(p);
  return out;
}
// packets -> messages, one packet at a time; what must outlast a packet is kept (an open SysEx, the running status).
// take(packet) gives [{ d: [status, data...], ts }], ts the 13-bit time in milliseconds the sender gave it. SysEx is
// followed to its end and left out
export class BleDecoder {
  constructor() { this.reset(); }
  reset() { this.rs = 0; this.cur = 0; this.need = 0; this.d = []; this.sysex = false; }
  take(p) {
    const out = [];
    if (!p || p.length < 1 || (p[0] & 0xC0) !== 0x80) return out;
    let hi = p[0] & 0x3F, lo = -1, ts = hi << 7, afterTs = false;
    for (let i = 1; i < p.length; i++) {
      const b = p[i];
      if (!(b & 0x80)) {                                     // data
        afterTs = false;
        if (this.sysex) continue;
        if (!this.cur) { if (!this.rs) continue; this.cur = this.rs; this.need = dataLen(this.rs); this.d = []; }
        this.d.push(b);
        if (this.d.length === this.need) this.emit(out, ts);
      } else if (!afterTs) {                                 // a time octet: a lower one than the last means the high part went up
        if (lo >= 0 && (b & 127) < lo) hi = (hi + 1) & 0x3F;
        lo = b & 127; ts = hi << 7 | lo; afterTs = true;
      } else {                                               // a status
        afterTs = false;
        if (b >= 0xF8) { out.push({ d: [b], ts }); continue; }   // real time: anywhere, nothing else changes
        if (this.sysex) { this.sysex = false; if (b === 0xF7) continue; }
        this.d = [];
        if (b === 0xF0) { this.rs = this.cur = 0; this.sysex = true; continue; }
        if (b === 0xF7 || b === 0xF4 || b === 0xF5) { this.rs = this.cur = 0; continue; }
        this.cur = b; this.need = dataLen(b); this.rs = b < 0xF0 ? b : 0;
        if (!this.need) this.emit(out, ts);
      }
    }
    if (!this.sysex) { this.cur = 0; this.d = []; }          // a message cut at the packet's end is lost
    return out;
  }
  emit(out, ts) {
    out.push({ d: [this.cur, ...this.d], ts });
    this.d = [];
    if (this.cur >= 0xF0) this.cur = 0;                      // system common: no running status
  }
}

// ---------------------------------------------------------------- the FM-1s on this page
// what waits to go to one FM-1. A Bluetooth write takes until the next meeting of the two radios, and a second one
// begun before it ends is refused: so they go one at a time, and what was said meanwhile goes together in the next
// (whole sends kept whole). A link that takes nothing for a long time loses its oldest, never its order
export class Outbox {
  constructor(write) { this.write = write; this.q = []; this.busy = false; this.due = false; this.sent = 0; this.dropped = 0; }
  send(bytes) {
    const g = midiMessages(bytes);
    if (!g.length) return;
    this.q.push(g);
    while (this.q.length > 96) { this.q.shift(); this.dropped++; }
    if (!this.due && !this.busy) { this.due = true; queueMicrotask(() => { this.due = false; this.pump(); }); }
  }
  clear() { this.q.length = 0; }
  async pump() {
    if (this.busy) return;
    this.busy = true;
    try {
      for (let p; (p = blePack(this.q, now()));) { await this.write(p); this.sent++; }
    } catch (e) { this.q.length = 0; }                       // (the link broke under it: what waited is stale by the time it is back)
    this.busy = false;
  }
}
const TRIES = [500, 1000, 2000, 4000, 8000];                 // after a broken link: wait, try again; the last one for as long as it takes
export class BlePorts {
  // on: { message(port, [status, d1, d2], t), change() }. A port: { id, name, kind: "ble", state, send(bytes) };
  // state: "connecting", "on", "lost" (trying again by itself)
  constructor(on = {}) { this.on = on; this.ports = new Map(); }
  get supported() { return !!globalThis.navigator?.bluetooth?.requestDevice; }
  // false when the computer's Bluetooth is off or it has none (true when the browser cannot say)
  async available() {
    if (!this.supported) return false;
    try { return navigator.bluetooth.getAvailability ? await navigator.bluetooth.getAvailability() : true; } catch (e) { return true; }
  }
  // the browser's chooser, from a click (one FM-1 a click: the browser asks each time). every: any Bluetooth MIDI
  // device, for an FM-1 the computer still lists under an older name. Gives the port; null when nothing was chosen;
  // throws when the FM-1 chosen could not be reached
  async add(every = false) {
    const filters = every ? [{ services: [MIDI_SERVICE] }] : [{ services: [MIDI_SERVICE], namePrefix: NAME_PREFIX }, { namePrefix: NAME_PREFIX }];
    let dev;
    try { dev = await navigator.bluetooth.requestDevice({ filters, optionalServices: [MIDI_SERVICE] }); }
    catch (e) { if (e && (e.name === "NotFoundError" || e.name === "AbortError")) return null; throw e; }
    const id = "ble:" + dev.id;
    let p = this.ports.get(id);
    if (!p) {
      p = { id, name: dev.name || "FM-1", kind: "ble", state: "connecting", dev, ch: null, tries: 0, gone: false, linking: false, dec: new BleDecoder(), got: 0 };
      p.out = new Outbox((pkt) => {
        if (!p.ch || p.state !== "on") throw new Error("not connected");
        return p.ch.writeValueWithoutResponse ? p.ch.writeValueWithoutResponse(pkt) : p.ch.writeValue(pkt);
      });
      p.send = (bytes) => { if (p.state === "on") p.out.send(bytes); };
      p.heard = (e) => {
        const v = e.target.value, t = e.timeStamp > 0 && now() - e.timeStamp < 1000 && e.timeStamp <= now() ? e.timeStamp : now();
        for (const m of p.dec.take(new Uint8Array(v.buffer, v.byteOffset, v.byteLength))) { p.got++; this.on.message?.(p, m.d, t); }
      };
      p.dropped = () => { if (!p.gone) this.lost(p); };
      dev.addEventListener("gattserverdisconnected", p.dropped);
      this.ports.set(id, p);
    }
    try { await this.link(p); }
    catch (e) { if (p.state !== "on") this.remove(p); throw e; }
    return p;
  }
  async link(p) {
    if (p.linking || p.gone) return;
    p.linking = true; p.state = "connecting"; this.on.change?.();
    try {
      const server = await p.dev.gatt.connect();
      const ch = await (await server.getPrimaryService(MIDI_SERVICE)).getCharacteristic(MIDI_IO);
      if (p.ch) try { p.ch.removeEventListener("characteristicvaluechanged", p.heard); } catch (e) {}
      ch.addEventListener("characteristicvaluechanged", p.heard);
      await ch.startNotifications();
      if (p.gone) { try { p.dev.gatt.disconnect(); } catch (e) {} return; }
      p.ch = ch; p.dec.reset(); p.out.clear(); p.tries = 0; p.state = "on";
    } finally { p.linking = false; this.on.change?.(); }
  }
  lost(p) {
    p.state = "lost"; p.out.clear(); this.on.change?.();
    clearTimeout(p.again);
    p.again = setTimeout(() => {
      if (p.gone) return;
      this.link(p).catch(() => { if (!p.gone && p.state !== "on") this.lost(p); });
    }, TRIES[Math.min(p.tries++, TRIES.length - 1)]);
  }
  remove(p) {
    p.gone = true; clearTimeout(p.again); p.out.clear();
    try { p.dev.removeEventListener("gattserverdisconnected", p.dropped); } catch (e) {}
    try { if (p.dev.gatt.connected) p.dev.gatt.disconnect(); } catch (e) {}
    this.ports.delete(p.id); p.state = "gone"; this.on.change?.();
  }
}

// ---------------------------------------------------------------- one FM-1 on two roads
// An FM-1 plugged in by USB with its Bluetooth on, or one the computer has connected as a MIDI device and this page
// too, says everything twice: it would count as two players, and be sent its own messages back. Its two roads are
// told by what comes down them: the whole clock an FM-1 reports (tempo, position, who) and its "I am here" in the
// well are its own, and no two FM-1s say the same ones at the same moment again and again. Four the same in a row
// on two ports: one FM-1. The port of the lower rank is then the shadow: what it says is left out, nothing is sent
// to it. Three in a row with no match (the other road went quiet: a cable pulled) and it counts for itself again
const TW_WINDOW = 400, TW_ON = 4, TW_OFF = 3;
export class Twins {
  constructor() { this.p = new Map(); this.pairs = new Map(); this.n = 0; }
  port(id, rank) {
    let P = this.p.get(id);
    if (!P) { P = { id, rank, order: this.n++, tempo: 0, pos: 0, recs: [] }; this.p.set(id, P); }
    return P;
  }
  pair(a, b) {
    const k = a < b ? a + "\n" + b : b + "\n" + a;
    let x = this.pairs.get(k);
    if (!x) { x = { a, b, streak: 0, miss: 0, on: false }; this.pairs.set(k, x); }
    return x;
  }
  // a control change on channel 16 heard on a port, at t. rank: which of two roads to one FM-1 is kept (the higher;
  // the same: the one heard first). Returns true when this port is a shadow: leave the message out
  take(id, rank, cc, v, t) {
    const P = this.port(id, rank);
    let s = null;
    if (cc === 102) P.tempo = v << 7;
    else if (cc === 103) P.tempo |= v;
    else if (cc === 104) P.pos = v << 14;
    else if (cc === 105) P.pos |= v << 7;
    else if (cc === 106) P.pos |= v;
    else if (cc === 107) s = "c" + P.tempo + ":" + P.pos + ":" + v;
    else if (cc === 110) s = "h" + v;
    if (s) {
      this.expire(t);
      const r = { s, t, m: new Set() };
      for (const Q of this.p.values()) {
        if (Q === P) continue;
        const o = Q.recs.find((x) => x.s === s && t - x.t <= TW_WINDOW && !x.m.has(id));
        if (!o) continue;
        o.m.add(id); r.m.add(Q.id);
        const x = this.pair(id, Q.id);
        x.miss = 0; if (++x.streak >= TW_ON) x.on = true;
      }
      P.recs.push(r);
    }
    return this.shadowOf(id) != null;
  }
  expire(t) {
    for (const X of this.p.values())
      while (X.recs.length && t - X.recs[0].t > 2 * TW_WINDOW) {
        const r = X.recs.shift();
        for (const x of this.pairs.values()) {
          if (x.a !== X.id && x.b !== X.id) continue;
          if (r.m.has(x.a === X.id ? x.b : x.a) || !x.streak) continue;
          if (++x.miss >= TW_OFF) { x.streak = 0; x.miss = 0; x.on = false; }
        }
      }
  }
  same(a, b) { return a !== b && !!this.pairs.get(a < b ? a + "\n" + b : b + "\n" + a)?.on; }
  // the port that is kept in this one's place, or null when this one counts
  shadowOf(id) {
    const P = this.p.get(id);
    if (!P) return null;
    let best = null;
    for (const x of this.pairs.values()) {
      if (!x.on || (x.a !== id && x.b !== id)) continue;
      const Q = this.p.get(x.a === id ? x.b : x.a);
      if (Q && (Q.rank > P.rank || (Q.rank === P.rank && Q.order < P.order)) && (!best || Q.rank > best.rank || (Q.rank === best.rank && Q.order < best.order))) best = Q;
    }
    return best ? best.id : null;
  }
  forget(id) {
    this.p.delete(id);
    for (const [k, x] of this.pairs) if (x.a === id || x.b === id) this.pairs.delete(k);
  }
}
// a port's rank for Twins: a cable before the air; of two Bluetooth roads the computer's own MIDI before this page's
export const rankOf = (port) => (port.kind === "ble" ? 1 : /bluetooth|\bble\b/i.test(port.name || "") ? 2 : 3);

// ---------------------------------------------------------------- on the page
const STATE = { connecting: "CONNECTING...", on: "CONNECTED", lost: "LINK LOST: TRYING AGAIN" };
// the Bluetooth FM-1s as rows in el (hidden while there are none). note(port): a line about one, or "" (the pages
// say there when it is a second road to an FM-1 already in)
const drawn = new WeakMap();
export function bleRows(el, ble, note = () => "") {
  const sig = [...ble.ports.values()].map((p) => p.id + "|" + p.state + "|" + note(p)).join("\n");
  el.hidden = ble.ports.size === 0;
  if (drawn.get(el) === sig) return;                         // (a row made anew under the pointer would lose its click)
  drawn.set(el, sig);
  el.innerHTML = "";
  for (const p of ble.ports.values()) {
    const extra = note(p), r = document.createElement("div");
    r.className = "bt " + (extra ? "twin" : p.state);
    r.innerHTML = '<span class="dot"></span><span class="nm"></span><span class="st"></span><button class="btn sm" type="button">REMOVE</button>';
    r.querySelector(".nm").textContent = p.name;
    r.querySelector(".st").textContent = extra || STATE[p.state] || "";
    r.querySelector("button").setAttribute("aria-label", "Remove " + p.name);
    r.querySelector("button").addEventListener("click", () => ble.remove(p));
    el.appendChild(r);
  }
}
// what to say when Bluetooth cannot be used here, or "" when it can
export async function bleSay(ble) {
  if (!ble.supported) return "THIS BROWSER HAS NO WEB BLUETOOTH: USE CHROME OR EDGE (SAFARI AND FIREFOX HAVE NONE). AN FM-1 THE COMPUTER ITSELF HAS CONNECTED AS A BLUETOOTH MIDI DEVICE STILL COMES IN AS A MIDI PORT.";
  if (!(await ble.available())) return "BLUETOOTH IS OFF ON THIS COMPUTER, OR IT HAS NONE.";
  return "";
}
// the ADD button's click: the chooser, the connection, and what went wrong in plain words. say(text); gives the port or null
export async function bleAdd(ble, say, every = false) {
  const no = await bleSay(ble);
  if (no) { say(no); return null; }
  say("PICK YOUR FM-1 IN THE BROWSER'S LIST...");
  try {
    const p = await ble.add(every);
    say(p ? p.name + ": CONNECTED BY BLUETOOTH." : "NOTHING CHOSEN. NOT IN THE LIST? SWITCH BLUETOOTH ON IN THE FM-1'S SETTINGS (GLO), AND CLOSE ANY OTHER APP OR PHONE THAT HOLDS IT.");
    return p;
  } catch (e) {
    say(e && e.name === "SecurityError" ? "THE BROWSER DID NOT ALLOW BLUETOOTH ON THIS PAGE."
      : "COULD NOT CONNECT. IS BLUETOOTH ON IN THE FM-1'S SETTINGS, AND IS IT FREE (AN FM-1 HOLDS ONE BLUETOOTH LINK AT A TIME)?");
    return null;
  }
}
