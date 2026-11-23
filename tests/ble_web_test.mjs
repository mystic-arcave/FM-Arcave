// SPDX-License-Identifier: GPL-3.0-only
// Bluetooth on the Link pages (web/link/ble.js), without an FM-1 and without a radio:
//   node tests/ble_web_test.mjs
// 1. the packets: BLE-MIDI to and from plain MIDI (round trips, running status with and without its time octet,
//    several messages in a packet, the time's high part stepping, a 20-octet packet's cut, real time, SysEx left out)
// 2. the same packets through the FM-1's own codec (firmware/bt/ble/ble_midi.c, or BLE_MIDI_C=path to one), when it
//    and a C compiler are there: what the page packs the firmware reads, and what the firmware packs the page reads
// 3. the LOCAL page's own script, run here behind a made-up document, navigator.bluetooth and Web MIDI, with made-up
//    FM-1s: two by Bluetooth both take the page's clock, six control changes in one write of 20 octets and never
//    two writes at once; an UNDERSONG message from one reaches the other whole, in order, once; a broken link comes
//    back by itself; an FM-1 in by USB too is counted once and never hears itself; two USB ports of one name whose
//    outputs are listed the other way round are put right by the ping
// 4. the ONLINE page's own script the same way, against the lobby server (server/node.mjs): two FM-1s by Bluetooth
//    here and a friend far away all hear each other
// It runs against the clock (about 25 s): Bluetooth's delays are part of what is tried.
import { readFileSync, writeFileSync, mkdirSync, existsSync } from "node:fs";
import { execFileSync, spawnSync } from "node:child_process";
import { fileURLToPath, pathToFileURL } from "node:url";
import { dirname, join } from "node:path";
import { startServer } from "../server/node.mjs";
import { LobbyClient } from "../web/link/lobby.js";
import { MIDI_SERVICE, MIDI_IO, NAME_PREFIX, BLE_CAP, midiMessages, blePack, bleEncode, BleDecoder, Twins } from "../web/link/ble.js";
import { outFor, portBase } from "../web/link/undersong.js";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), ".."), OUT = join(ROOT, "build", "host");
mkdirSync(OUT, { recursive: true });
const fail = (m) => { console.error("FAIL", m); process.exit(1); };
const ok = (what, cond, extra = "") => { if (!cond) fail(what + (extra ? ": " + extra : "")); };
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const until = async (what, cond, ms, say = () => "") => { for (let t = 0; t < ms; t += 25) { if (cond()) return; await wait(25); } fail(what + " " + say()); };
const hex = (a) => [...a].map((x) => x.toString(16).padStart(2, "0")).join("");
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
const flat = (ms) => ms.flatMap((m) => m.d ?? m);
const cc = (...p) => { const b = []; for (let i = 0; i + 1 < p.length; i += 2) b.push(0xBF, p[i], p[i + 1]); return b; };

// ---------------------------------------------------------------- 1. the packets
{
  const dec = new BleDecoder(), all = (pkts) => pkts.flatMap((p) => dec.take(p));
  const six = cc(102, 25, 103, 0, 104, 3, 105, 77, 106, 5, 107, 8);
  let p = bleEncode(six, 1000);
  ok("six control changes are one packet of 20 octets", p.length === 1 && p[0].length === 20, hex(p[0] || []));
  ok("its header and time octets say 1000 ms", p[0][0] === (0x80 | 1000 >> 7) && p[0][1] === (0x80 | (1000 & 127)));
  ok("they come back the same, each at 1000 ms", same(flat(all(p)), six) && dec.take(p[0]).every((m) => m.ts === 1000));
  p = bleEncode(cc(118, 1, 115, 2, 116, 3, 117, 20), 5);
  ok("an UNDERSONG message of four is one packet", p.length === 1 && p[0].length === 14 && same(flat(all(p)), cc(118, 1, 115, 2, 116, 3, 117, 20)));
  const seven = cc(1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7);
  p = bleEncode(seven, 8191);
  ok("seven are cut into two packets, none over 20 octets, the order kept", p.length === 2 && p.every((x) => x.length <= BLE_CAP) && same(flat(all(p)), seven), p.map(hex).join(" "));
  ok("a time of 8191 ms is the last before it starts again", dec.take(p[0])[0].ts === 8191 && dec.take(bleEncode(cc(1, 1), 8192)[0])[0].ts === 0);
  // two sends that wait together: whole sends stay whole (4 + 4 do not fit: the second leads the next packet)
  const q = [midiMessages(cc(118, 1, 115, 2, 116, 3, 117, 4)), midiMessages(cc(118, 5, 115, 6, 116, 7, 117, 8))];
  const a = blePack(q, 0), b = blePack(q, 0);
  ok("two messages of four: one packet each, never one cut in the middle", a.length === 14 && b.length === 14 && !blePack(q, 0));
  const mixed = [0x90, 60, 100, 0x90, 64, 90, 0xF8, 0x80, 60, 0, 0xC3, 9, 0xE0, 1, 2, 0xFA];
  ok("notes, a program change, a bend and real time go and come back", same(flat(all(bleEncode(mixed, 77))), mixed));
  ok("SysEx and what is not a message are not sent", same(flat(all(bleEncode([0xF0, 1, 2, 3, 0xF7, 5, 0xBF, 7, 9, 0xBF, 1], 0))), cc(7, 9)));
  // as other senders write them
  dec.reset();
  ok("running status without a time octet", same(flat(dec.take([0x80, 0x85, 0xBF, 1, 2, 3, 4, 5, 6])), cc(1, 2, 3, 4, 5, 6)));
  ok("running status with one", same(flat(dec.take([0x80, 0x85, 0xBF, 1, 2, 0x86, 3, 4])), cc(1, 2, 3, 4)));
  let m = dec.take([0x81, 0xFE, 0x90, 1, 2, 0x82, 0x90, 3, 4]);
  ok("a lower time octet: the high part went one up", m[0].ts === (1 << 7 | 126) && m[1].ts === (2 << 7 | 2));
  m = dec.take([0xBF, 0xFF, 0x90, 1, 2, 0x80, 3, 4]);
  ok("and round from the top", m[0].ts === 8191 && m[1].ts === 0);
  ok("real time in the middle of a message", same(flat(dec.take([0x80, 0x80, 0xBF, 7, 0x80, 0xF8, 9])), [0xF8, 0xBF, 7, 9]));
  ok("a SysEx over two packets is followed and left out; what follows it is read",
    dec.take([0x80, 0x80, 0xF0, 1, 2, 3]).length === 0 && same(flat(dec.take([0x80, 4, 5, 0x81, 0xF7, 0x82, 0xBF, 1, 2])), cc(1, 2)));
  ok("a packet without a header is ignored", dec.take([0x3F, 0xBF, 1, 2]).length === 0 && dec.take([]).length === 0);
  ok("a message cut at a packet's end is lost, the next packet read", dec.take([0x80, 0x80, 0xBF, 1]).length === 0 && same(flat(dec.take([0x80, 0x80, 0xBF, 3, 4])), cc(3, 4)));
  let rnd = 12345; const r = (n) => (rnd = (rnd * 1103515245 + 12345) >>> 0, (rnd >>> 8) % n);
  dec.reset();
  for (let i = 0; i < 3000; i++) {                           // random channel messages and real time, random packet sizes
    const bytes = [];
    for (let k = 1 + r(9); k > 0; k--) {
      const kind = r(8);
      if (kind === 0) bytes.push(0xF8 + [0, 2, 3, 4][r(4)]);
      else { const st = 0x80 + r(7) * 16 + (r(3) ? 15 : r(16)); bytes.push(st, r(128)); if ((st & 0xE0) !== 0xC0) bytes.push(r(128)); }
    }
    const pk = bleEncode(bytes, r(100000), 20 + r(3) * 40);
    if (!same(flat(all(pk)), bytes)) fail("a random round trip came back different: " + hex(bytes));
  }
  console.log("packets: six control changes in 20 octets, cuts, times, running status, real time, SysEx left out, 3000 random round trips");
}

// ---------------------------------------------------------------- 2. against the FM-1's own codec
{
  const src = process.env.BLE_MIDI_C || join(ROOT, "firmware", "bt", "ble", "ble_midi.c");
  const ccBin = process.env.CC || "cc";
  if (!existsSync(src)) console.log("-- the FM-1's codec (firmware/bt/ble/ble_midi.c) is not in this tree: BLE_MIDI_C=path tries the page's packets against it");
  else if (spawnSync(ccBin, ["--version"]).status !== 0) console.log("-- no C compiler: the page's packets were not tried against the FM-1's codec");
  else {
    const c = join(OUT, "ble_web_codec.c"), bin = join(OUT, "ble_web_codec");
    writeFileSync(c, `#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "${src.replace(/\\/g, "/")}"
/* lines in: "D hex" a packet to read; "E cap t n pkt..." USB-MIDI events to pack. Lines out: "M pkt ts", "P hex", "." */
static void sink(void *ctx, uint32_t pkt, uint16_t ts) { (void)ctx; printf("M %08x %u\\n", (unsigned)pkt, (unsigned)ts); }
int main(void)
{
    static char line[8192];
    struct ble_midi_dec d;
    struct ble_midi_enc e;
    ble_midi_dec_reset(&d);
    ble_midi_enc_reset(&e);
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] == 'D') {
            uint8_t p[600]; unsigned n = 0, v; const char *s = line + 2;
            while (sscanf(s, "%2x", &v) == 1 && n < sizeof p) { p[n++] = (uint8_t)v; s += 2; }
            ble_midi_dec(&d, p, (uint16_t)n, sink, 0);
        } else if (line[0] == 'E') {
            unsigned cap, t, n, i, pos = 0, used; uint8_t buf[600]; const char *s = line + 2;
            sscanf(s, "%u %u %u%n", &cap, &t, &n, &pos); s += pos;
            ble_midi_enc_begin(&e, buf, (uint16_t)cap);
            for (i = 0; i < n; i++) {
                unsigned pkt; int r;
                sscanf(s, "%x%n", &pkt, &used); s += used;
                r = ble_midi_enc_add(&e, pkt, t);
                if (r == 0) {
                    unsigned k, len = ble_midi_enc_len(&e);
                    printf("P "); for (k = 0; k < len; k++) printf("%02x", buf[k]); printf("\\n");
                    ble_midi_enc_begin(&e, buf, (uint16_t)cap);
                    ble_midi_enc_add(&e, pkt, t);
                }
            }
            { unsigned k, len = ble_midi_enc_len(&e); if (len) { printf("P "); for (k = 0; k < len; k++) printf("%02x", buf[k]); printf("\\n"); } }
        }
        printf(".\\n");
    }
    return 0;
}
`);
    execFileSync(ccBin, ["-O1", "-w", "-o", bin, c]);
    const usb = (m) => ((m[0] >= 0xF8 ? 0xF : m[0] >> 4) | m[0] << 8 | (m[1] || 0) << 16 | (m[2] || 0) << 24) >>> 0;
    const lines = [], want = [];
    let rnd = 777; const r = (n) => (rnd = (rnd * 1103515245 + 12345) >>> 0, (rnd >>> 8) % n);
    const sets = [cc(102, 25, 103, 0, 104, 3, 105, 77, 106, 5, 107, 8), cc(118, 1, 115, 2, 116, 3, 117, 20), [0xF8, 0xBF, 108, 9, 0xFA]];
    for (let i = 0; i < 400; i++) {
      const bytes = [];
      for (let k = 1 + r(10); k > 0; k--) { if (!r(6)) bytes.push([0xF8, 0xFA, 0xFB, 0xFC][r(4)]); else { const st = 0x80 + r(7) * 16 + 15; bytes.push(st, r(128)); if ((st & 0xE0) !== 0xC0) bytes.push(r(128)); } }
      sets.push(bytes);
    }
    for (const bytes of sets) {                              // the page packs, the FM-1 reads
      const t = r(60000), ms = midiMessages(bytes);
      for (const p of bleEncode(bytes, t)) lines.push("D " + hex(p));
      want.push(ms.map((m) => "M " + usb(m).toString(16).padStart(8, "0") + " " + (t & 0x1FFF)));
    }
    let outp = execFileSync(bin, { input: lines.join("\n") + "\n" }).toString().split("\n");
    const gotM = outp.filter((l) => l[0] === "M"), wantM = want.flat();
    ok("what the page packs, the FM-1's codec reads: the same messages at the same times", same(gotM, wantM), gotM.length + " of " + wantM.length);
    const dec = new BleDecoder();                            // the FM-1 packs, the page reads
    let n = 0;
    for (const bytes of sets) {
      const ms = midiMessages(bytes), t = r(60000), cap = [20, 20, 61, 244][r(4)];
      outp = execFileSync(bin, { input: `E ${cap} ${t} ${ms.length} ${ms.map((m) => usb(m).toString(16)).join(" ")}\n` }).toString().split("\n");
      const back = outp.filter((l) => l[0] === "P").flatMap((l) => dec.take(Uint8Array.from(l.slice(2).match(/../g).map((x) => parseInt(x, 16)))));
      if (!same(flat(back), bytes) || back.some((m) => m.ts !== (t & 0x1FFF))) fail("what the FM-1's codec packs, the page read differently: " + hex(bytes));
      n += back.length;
    }
    console.log(`the FM-1's own codec (${src.replace(ROOT + "/", "")}): ${wantM.length} messages from the page read by it, ${n} packed by it read by the page`);
  }
}

// ---------------------------------------------------------------- pairing by name (undersong.js)
{
  const port = (id, name) => ({ id, name });
  const mk = (ins, outs) => ({ inputs: new Map(ins.map((p) => [p.id, p])), outputs: new Map(outs.map((p) => [p.id, p])) });
  const i1 = port("i1", "FM-ARCAVE"), i2 = port("i2", "FM-ARCAVE"), o1 = port("o1", "FM-ARCAVE"), o2 = port("o2", "FM-ARCAVE");
  let m = mk([i1, i2], [o1, o2]);
  ok("two ports of one name: the n-th input with the n-th output", outFor(m, i1) === o1 && outFor(m, i2) === o2);
  ok("a pair a ping has shown is kept, and its output is no other input's", outFor(m, i1, new Map([["i2", o1]])) === o2 && outFor(m, i2, new Map([["i2", o1]])) === o1);
  const b1 = port("b1", "FM-ARCAVE 3F2A Bluetooth"), b2 = port("b2", "FM-ARCAVE 91C0 Bluetooth"), c1 = port("c1", "FM-ARCAVE 3F2A Bluetooth"), c2 = port("c2", "FM-ARCAVE 91C0 Bluetooth");
  m = mk([b1, b2, i1], [c2, o1, c1]);
  ok("two Bluetooth FM-1s through the computer's MIDI: each with its own four characters", outFor(m, b1) === c1 && outFor(m, b2) === c2 && outFor(m, i1) === o1);
  const w1 = port("w1", "FM-ARCAVE 3F2A (Bluetooth MIDI IN)"), w2 = port("w2", "FM-ARCAVE 3F2A (Bluetooth MIDI OUT)"), w3 = port("w3", "MIDIIN2 (FM-ARCAVE)"), w4 = port("w4", "MIDIOUT2 (FM-ARCAVE)");
  m = mk([w1, w3], [w4, w2]);
  ok("names that differ only by IN and OUT still pair", outFor(m, w1) === w2 && outFor(m, w3) === w4 && portBase(w1.name) === portBase(w2.name));
  ok("no output of that name: none", outFor(mk([i1], [c1]), i1) === null);
}

// ---------------------------------------------------------------- made-up FM-1s, Bluetooth and MIDI
const MOD = 1 << 21, wrap = (d) => ((d % MOD) + MOD + MOD / 2) % MOD - MOD / 2, rate = (bpm) => bpm * 384 / 60000;
class Fm1 {
  constructor(name, num) {
    this.name = name; this.num = num; this.well = false; this.nonce = 1 + Math.floor(Math.random() * 126);
    this.bpm = 80 + Math.random() * 60; this.t0 = performance.now(); this.pos0 = Math.floor(Math.random() * MOD);
    this.roads = []; this.got = []; this.acc = { tempo: 0, pos: 0 }; this.asks = []; this.follows = 0;
    this.timers = [setInterval(() => { if (this.num) this.report(); }, 500), setInterval(() => { if (this.well) this.say(cc(110, this.nonce)); }, 300)];
  }
  pos(t = performance.now()) { return ((this.pos0 + (t - this.t0) * rate(this.bpm)) % MOD + MOD) % MOD; }
  report() { const t = Math.round(this.bpm * 32), p = Math.floor(this.pos()); this.say(cc(102, t >> 7, 103, t & 127, 104, p >> 14 & 127, 105, p >> 7 & 127, 106, p & 127, 107, this.num)); }
  say(bytes) { for (const r of this.roads) r.fromDevice(bytes); }
  take(m, road) {                                           // a message from the hub, as the firmware takes it (ac_sync.h)
    const [st, c, v] = m, now = performance.now();
    if (st !== 0xBF) return;
    this.got.push({ c, v, t: now, road });
    const a = this.acc;
    if (c === 102) a.tempo = v << 7; else if (c === 103) a.tempo |= v; else if (c === 104) a.pos = v << 14; else if (c === 105) a.pos |= v << 7; else if (c === 106) a.pos |= v;
    else if (c === 107 && (v & 8) && (v & 7) !== 5) {       // "follow this": of four in a row the one asking for the most is trusted
      this.asks.push(wrap(a.pos - this.pos(now)));
      if (this.asks.length >= 4) { const d = Math.max(...this.asks); this.asks = []; this.pos0 = this.pos(now) + d; this.t0 = now; this.bpm = a.tempo / 32; this.follows++; }
    } else if (c === 108 && this.num) this.say(cc(109, v));
  }
  stop() { this.timers.forEach(clearInterval); }
}
// a Bluetooth road: a GATT server with the BLE-MIDI service. A write lands at the radios' next meeting (2 to 12 ms)
// and a second one begun before that is refused, as a browser refuses it; what the FM-1 says goes out at a meeting
// too, packed as its firmware packs it (several messages a packet, running status)
const chooser = [];
function bleRoad(fm) {
  const ev = {}, devEv = {}, dec = new BleDecoder(), sub = (o) => ({ addEventListener(t, f) { (o[t] ||= new Set()).add(f); }, removeEventListener(t, f) { o[t]?.delete(f); } });
  const road = { kind: "ble", writes: [], clashes: 0, long: 0, connects: 0, connected: false, notifying: false, busy: false, queue: [], due: false, unreachable: false };
  const ch = {
    ...sub(ev), value: null,
    async startNotifications() { road.notifying = true; return ch; },
    writeValueWithoutResponse(buf) {
      if (!road.connected) return Promise.reject(Object.assign(new Error("gone"), { name: "NetworkError" }));
      if (road.busy) { road.clashes++; return Promise.reject(Object.assign(new Error("GATT operation already in progress."), { name: "NetworkError" })); }
      const p = Uint8Array.from(buf);
      if (p.length > BLE_CAP) road.long++;
      road.busy = true; road.writes.push({ p, t: performance.now() });
      return new Promise((done) => setTimeout(() => { road.busy = false; for (const m of dec.take(p)) fm.take(m.d, road); done(); }, 2 + Math.random() * 10));
    },
  };
  const server = { async getPrimaryService(u) { ok("the service asked for is BLE-MIDI's", u === MIDI_SERVICE); return { async getCharacteristic(c) { ok("the characteristic is MIDI I/O", c === MIDI_IO); return ch; } }; } };
  road.dev = {
    ...sub(devEv), id: "dev-" + fm.name, name: fm.name,
    gatt: { connected: false, async connect() { await wait(15); if (road.unreachable) throw Object.assign(new Error("out of reach"), { name: "NetworkError" }); road.connected = road.dev.gatt.connected = true; road.connects++; return server; }, disconnect() { road.drop(); } },
  };
  road.drop = () => { if (!road.connected) return; road.connected = road.dev.gatt.connected = false; road.notifying = false; road.queue = []; for (const f of devEv.gattserverdisconnected || []) f({ target: road.dev }); };
  road.fromDevice = (bytes) => {
    if (!road.connected || !road.notifying) return;
    road.queue.push(...bytes);
    if (road.due) return;
    road.due = true;
    setTimeout(() => {
      road.due = false;
      const b = road.queue; road.queue = [];
      if (!road.connected) return;
      for (const p of bleEncode(b, performance.now(), [20, 61, 244][Math.floor(Math.random() * 3)]))
        for (const f of ev.characteristicvaluechanged || []) f({ target: { value: new DataView(p.buffer, p.byteOffset, p.byteLength) }, timeStamp: performance.now() });
    }, 2 + Math.random() * 10);
  };
  fm.roads.push(road);
  return road;
}
const midiAccess = { inputs: new Map(), outputs: new Map(), onstatechange: null };
let portN = 0;
function midiRoad(fm, name, outFirst = false) {              // a USB road: Web MIDI's two ports, a message an event, a millisecond each way
  const n = ++portN, road = { kind: "midi", sent: 0 };
  road.input = { id: "in" + n, name, type: "input", onmidimessage: null };
  road.output = { id: "out" + n, name, type: "output", send(bytes) { road.sent++; const ms = midiMessages(bytes); setTimeout(() => { for (const m of ms) fm.take(m, road); }, 1); } };
  road.fromDevice = (bytes) => { for (const m of midiMessages(bytes)) setTimeout(() => road.input.onmidimessage?.({ data: Uint8Array.from(m), timeStamp: performance.now() }), 1); };
  midiAccess.inputs.set(road.input.id, road.input);
  if (outFirst) midiAccess.outputs = new Map([[road.output.id, road.output], ...midiAccess.outputs]); else midiAccess.outputs.set(road.output.id, road.output);
  fm.roads.push(road);
  midiAccess.onstatechange?.({});
  return road;
}
// a page's document: what the pages' scripts ask of one
function fakeDocument() {
  const byId = new Map();
  const el = (id) => {
    const kids = new Map(), e = {
      id, textContent: "", value: id === "leader" ? "page" : "", hidden: false, disabled: false, className: "", children: [], on: {}, html: "",
      classList: { add() {}, remove() {}, toggle() {} },
      addEventListener(t, f) { (e.on[t] ||= []).push(f); }, setAttribute() {}, focus() {}, insertAdjacentHTML() {},
      querySelector(s) { if (!kids.has(s)) kids.set(s, el(s)); return kids.get(s); }, querySelectorAll() { return []; },
      appendChild(c) { e.children.push(c); return c; },
      click() { return Promise.all((e.on.click || []).map((f) => f({}))); },
    };
    Object.defineProperty(e, "innerHTML", { get: () => e.html, set: (v) => { e.html = v; e.children.length = 0; } });
    return e;
  };
  return { getElementById(id) { if (!byId.has(id)) byId.set(id, el(id)); return byId.get(id); }, createElement: (t) => el(t) };
}
// a page's module script, its imports pointed at the files, run here
async function page(file, search = "") {
  const html = readFileSync(join(ROOT, "web", "link", file), "utf8"), dir = dirname(join(ROOT, "web", "link", file));
  const src = html.slice(html.indexOf('<script type="module">') + 22, html.lastIndexOf("</script>"))
    .replace(/from "(\.[^"]+)"/g, (_, rel) => 'from "' + pathToFileURL(join(dir, rel)).href + '"');
  const out = join(OUT, "ble_web_" + file.replace(/\W+/g, "_") + ".mjs");
  writeFileSync(out, src);
  globalThis.document = fakeDocument();
  globalThis.location = { search, hash: "", origin: "http://test", pathname: "/link/" };
  await import(pathToFileURL(out).href + "?" + Date.now());
  return globalThis.document;
}
globalThis.window = globalThis;
globalThis.addEventListener = () => {};
globalThis.history = { replaceState() {} };
globalThis.localStorage = { getItem: () => null, setItem() {} };
let asked = null;
Object.defineProperty(globalThis, "navigator", { configurable: true, value: {
  requestMIDIAccess: async () => midiAccess,
  bluetooth: {
    getAvailability: async () => true,
    async requestDevice(o) { asked = o; const r = chooser.shift(); if (!r) throw Object.assign(new Error("cancelled"), { name: "NotFoundError" }); return r.dev; },
  },
} });

// ---------------------------------------------------------------- 3. the LOCAL page
{
  const doc = await page("index.html"), $ = (id) => doc.getElementById(id), L = globalThis.__link;
  const A = new Fm1("FM-ARCAVE 3F2A", 1), B = new Fm1("FM-ARCAVE 91C0", 2), ra = bleRoad(A), rb = bleRoad(B);
  await wait(30);
  ok("with Web Bluetooth there, the button is not greyed", !$("bt").disabled);
  chooser.push(ra); await $("bt").click();
  ok("the chooser is asked for BLE-MIDI devices named FM-ARCAVE...", asked.filters.some((f) => f.namePrefix === NAME_PREFIX && f.services?.[0] === MIDI_SERVICE) && asked.optionalServices.includes(MIDI_SERVICE));
  chooser.push(rb); await $("bt").click();
  ok("two FM-1s by Bluetooth at once, both connected", L.ble.ports.size === 2 && [...L.ble.ports.values()].every((p) => p.state === "on"));
  await $("bt").click();                                    // the chooser closed with nothing picked
  ok("nothing chosen: said, and the list of every Bluetooth MIDI device offered", /NOTHING CHOSEN/.test($("btsay").textContent) && !$("btall").hidden && L.ble.ports.size === 2);
  chooser.push(ra); await $("bt").click();
  ok("the same FM-1 picked again is not a second one", L.ble.ports.size === 2);
  await $("go").click();
  ok("the hub runs", $("go").textContent === "STOP THE HUB");
  // both take the page's clock
  await until("the two Bluetooth FM-1s did not both report and get pinged", () => L.devs.size === 2 && [...L.devs.values()].every((d) => d.rtts.length >= 3), 9000, () => JSON.stringify([...L.devs.values()].map((d) => [d.key, d.rtts.length])));
  await until("they were not both told the clock four times", () => A.follows >= 2 && B.follows >= 2, 6000);
  const pagePos = (t) => (L.page.pos0 + (t - L.page.t0) * rate(L.page.bpm)) % MOD, now = performance.now();
  const offA = wrap(A.pos(now) - pagePos(now)) / rate(100), offB = wrap(B.pos(now) - pagePos(now)) / rate(100);
  const dl = [...L.devs.values()].map((d) => d.delay.toFixed(1));
  console.log(`LOCAL: two FM-1s by Bluetooth follow the page: ${offA.toFixed(1)} and ${offB.toFixed(1)} ms from its clock at 100 BPM; delays measured ${dl.join(", ")} ms (each way 2 to 12)`);
  ok("both are within 15 ms of the page's clock, at its tempo", Math.abs(offA) < 15 && Math.abs(offB) < 15 && Math.abs(A.bpm - 100) < 0.1 && Math.abs(B.bpm - 100) < 0.1);
  ok("the delay measured is a Bluetooth one, not the 2 ms a cable starts from", [...L.devs.values()].every((d) => d.ble && d.delay >= 2 && d.delay < 14));
  const clock = ra.writes.filter((w) => w.p.length === 20);
  ok("the clock's six control changes go in one write of 20 octets", clock.length >= 8 && same(flat(new BleDecoder().take(clock[0].p)).filter((_, i) => i % 3 === 1), [102, 103, 104, 105, 106, 107]));
  ok("no write over 20 octets, never two writes at once", ra.long + rb.long === 0 && ra.clashes + rb.clashes === 0, `${ra.long + rb.long} long, ${ra.clashes + rb.clashes} at once`);
  // UNDERSONG between the two
  A.well = B.well = true;
  await until("the two were not both in UNDERSONG's relay", () => L.duo.size === 2, 3000);
  A.got = []; B.got = [];
  const w0 = rb.writes.length;
  A.say(cc(118, 3, 115, 40, 116, 41, 117, 7)); A.say(cc(112, 5)); A.say(cc(118, 4, 115, 50, 116, 51, 117, 7));
  await wait(400);
  const us = (f) => f.got.filter((m) => m.c >= 111 && m.c <= 118).map((m) => [m.c, m.v]).flat();
  ok("an UNDERSONG message from one reaches the other whole, in order, once", same(us(B), [118, 3, 115, 40, 116, 41, 117, 7, 112, 5, 118, 4, 115, 50, 116, 51, 117, 7]), JSON.stringify(us(B)));
  ok("and never comes back to the one who said it", us(A).length === 0);
  const whole = rb.writes.slice(w0).map((w) => flat(new BleDecoder().take(w.p)).filter((_, i) => i % 3 === 1));
  ok("each message of four in one write, never cut", whole.filter((w) => w.includes(117)).every((w) => { const i = w.indexOf(118); return i >= 0 && same(w.slice(i, i + 4), [118, 115, 116, 117]); }), JSON.stringify(whole));
  // the leader's clock of the game (107 with 13) goes on in one piece too
  A.say(cc(102, 25, 103, 0, 104, 1, 105, 2, 106, 3, 107, 13));
  await wait(200);
  ok("the game leader's clock is passed on in one piece", same(B.got.filter((m) => m.c >= 102 && m.c <= 107 && B.got.indexOf(m) >= B.got.findIndex((x) => x.c === 107 && x.v === 13) - 5).slice(0, 6).map((m) => m.c), [102, 103, 104, 105, 106, 107]));
  console.log("LOCAL: UNDERSONG's messages pass between two Bluetooth FM-1s whole, in order, once");
  // a link breaks and comes back
  const pa = L.ble.ports.get("ble:dev-" + A.name), before = ra.connects;
  ra.drop();
  ok("a broken link is shown", pa.state === "lost");
  await until("the link did not come back by itself", () => pa.state === "on" && ra.connects === before + 1, 4000);
  const n0 = A.follows;
  await until("the FM-1 back on its link was not told the clock again", () => A.follows > n0, 5000);
  console.log("LOCAL: a broken Bluetooth link comes back by itself, and the clock with it");
  // the same FM-1 by USB too: counted once, on its cable
  const ua = midiRoad(A, "FM-ARCAVE");
  await until("an FM-1 in by USB and by Bluetooth was not seen as one", () => L.twins.shadowOf(pa.id) === ua.input.id, 6000);
  await wait(600);
  const liveDevs = [...L.devs.values()].filter((d) => d.num === 1);
  ok("it is counted once, on its cable", liveDevs.length === 1 && liveDevs[0].inId === ua.input.id && !L.duo.has(pa.id) && L.duo.has(ua.input.id), JSON.stringify([...L.devs.keys()]));
  A.got = []; B.got = [];
  B.say(cc(118, 9, 115, 1, 116, 2, 117, 7)); A.say(cc(118, 8, 115, 3, 116, 4, 117, 7));
  await wait(400);
  ok("it hears its friend once (by the cable), and never itself", same(us(A), [118, 9, 115, 1, 116, 2, 117, 7]) && A.got.filter((m) => m.c === 117).every((m) => m.road === ua) && same(us(B), [118, 8, 115, 3, 116, 4, 117, 7]), JSON.stringify([us(A), us(B)]));
  const wr = ra.writes.length; await wait(800);
  ok("nothing more is sent down its Bluetooth link", ra.writes.length === wr);
  ok("the page says so", /SAME FM-1/.test(doc.getElementById("bts").children[0]?.querySelector(".st").textContent || ""), doc.getElementById("bts").children.map((c) => c.querySelector(".st").textContent).join(" | "));
  console.log("LOCAL: an FM-1 in by USB and by Bluetooth is counted once, hears its friends once and never itself");
  // the cable pulled: its Bluetooth link counts again
  midiAccess.inputs.delete(ua.input.id); midiAccess.outputs.delete(ua.output.id); A.roads = A.roads.filter((r) => r !== ua); midiAccess.onstatechange?.({});
  await until("with its cable pulled the FM-1 did not count by Bluetooth again", () => L.twins.shadowOf(pa.id) == null && [...L.devs.values()].some((d) => d.inId === pa.id && performance.now() - d.seen < 1000), 8000);
  console.log("LOCAL: its cable pulled, it is in by Bluetooth again");
  // two USB FM-1s of one name, their outputs listed the other way round
  const C = new Fm1("C", 3), D = new Fm1("D", 4), rc = midiRoad(C, "FM-ARCAVE"), rd = midiRoad(D, "FM-ARCAVE", true);
  await until("the two USB FM-1s did not report", () => L.devs.has(rc.input.id + "|3") && L.devs.has(rd.input.id + "|4"), 3000);
  const dc = L.devs.get(rc.input.id + "|3"), dd = L.devs.get(rd.input.id + "|4");
  ok("(by their names alone they are paired the wrong way round)", dc.out === rd.output && dd.out === rc.output);
  await until("a ping's answer did not put the pairs right", () => dc.out === rc.output && dd.out === rd.output && dc.rtts.length && dd.rtts.length, 9000, () => JSON.stringify([dc.out.id, dd.out.id, rc.output.id, rd.output.id]));
  console.log("LOCAL: two USB ports of one name, outputs listed the other way round: put right by the ping's answer");
  await $("go").click();
  ok("the hub stops", $("go").textContent === "START THE HUB");
  for (const p of [...L.ble.ports.values()]) L.ble.remove(p);
  ok("removed FM-1s are let go", L.ble.ports.size === 0 && !ra.connected && !rb.connected);
  for (const f of [A, B, C, D]) f.stop();
  midiAccess.inputs.clear(); midiAccess.outputs.clear();
}

// ---------------------------------------------------------------- 4. the ONLINE page
{
  const srv = await startServer(0, "127.0.0.1"), URL = "ws://127.0.0.1:" + srv.port;
  const doc = await page("online/index.html", "?server=" + encodeURIComponent(URL)), $ = (id) => doc.getElementById(id), O = globalThis.__online;
  const A = new Fm1("FM-ARCAVE 3F2A", 0), B = new Fm1("FM-ARCAVE 91C0", 0), ra = bleRoad(A), rb = bleRoad(B);
  await wait(30);
  chooser.push(ra); await $("bt").click();
  chooser.push(rb); await $("bt").click();
  ok("ONLINE: two FM-1s by Bluetooth connected", O.ble.ports.size === 2 && [...O.ble.ports.values()].every((p) => p.state === "on"));
  await $("make").click();
  await until("the page got no lobby", () => O.lobby.status === "in", 3000);
  const far = [], F = new LobbyClient(URL, { message: (f, d) => far.push(d) });
  F.join(O.lobby.code, "far");
  await until("the friend far away did not get in", () => F.status === "in", 3000);
  A.well = B.well = true;
  await until("the two were not seen in the well", () => [...O.here.values()].filter((s) => s.kind === "ble" && performance.now() - s.hello < 1500).length === 2, 3000);
  A.got = []; B.got = []; far.length = 0;
  A.say(cc(118, 3, 115, 40, 116, 41, 117, 7));
  await wait(500);
  const us = (f) => f.got.filter((m) => m.c >= 111 && m.c <= 118).map((m) => [m.c, m.v]).flat();
  ok("ONLINE: what one says reaches the other FM-1 here, whole, once", same(us(B), [118, 3, 115, 40, 116, 41, 117, 7]) && us(A).length === 0, JSON.stringify([us(A), us(B)]));
  ok("ONLINE: and the lobby, in one piece", far.some((d) => same(d, [118, 3, 115, 40, 116, 41, 117, 7])), JSON.stringify(far));
  ok("ONLINE: the lobby is told an FM-1 plays here, in the well", (() => { const i = O.lobby.info(); return i.kind === "usb" && i.well; })());
  A.got = []; B.got = [];
  F.send([118, 1, 115, 2, 116, 3, 117, 9]);
  await wait(500);
  ok("ONLINE: what the friend far away says reaches both FM-1s, whole, once", same(us(A), [118, 1, 115, 2, 116, 3, 117, 9]) && same(us(B), [118, 1, 115, 2, 116, 3, 117, 9]), JSON.stringify([us(A), us(B)]));
  ok("ONLINE: no write over 20 octets, never two at once", ra.long + rb.long === 0 && ra.clashes + rb.clashes === 0);
  console.log("ONLINE: two FM-1s by Bluetooth and a friend through the lobby server all hear each other, whole, once");
  O.lobby.leave(); F.leave();
  for (const f of [A, B]) f.stop();
  await srv.close();
}

// ---------------------------------------------------------------- no Web Bluetooth: said plainly
{
  Object.defineProperty(globalThis, "navigator", { configurable: true, value: { requestMIDIAccess: async () => midiAccess } });
  const doc = await page("index.html");
  await wait(30);
  ok("without Web Bluetooth the button is greyed and the page says which browsers have it", doc.getElementById("bt").disabled && /CHROME OR EDGE/.test(doc.getElementById("btsay").textContent) && /SAFARI AND FIREFOX/.test(doc.getElementById("btsay").textContent));
  await doc.getElementById("go").click();
  ok("the hub still runs on MIDI alone", doc.getElementById("go").textContent === "STOP THE HUB");
  console.log("a browser without Web Bluetooth: the page says so, and the hub runs on MIDI");
}
// (Twins alone: two different FM-1s are never taken for one)
{
  const t = new Twins();
  let twin = false;
  for (let i = 0; i < 40; i++) { const now = i * 300; for (const [id, n] of [["a", 7], ["b", 8]]) twin ||= t.take(id, 1, 110, n, now + (id === "b" ? 20 : 0)); }
  ok("two FM-1s with their own numbers are two", !twin && !t.same("a", "b"));
}
console.log("ok");
process.exit(0);
