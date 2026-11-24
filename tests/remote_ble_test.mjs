// SPDX-License-Identifier: GPL-3.0-only
// PLAY WITH MY FM-1 (BLUETOOTH) without an FM-1: as tests/remote_test.mjs, but the two are joined by a made-up Bluetooth
// link as slow as a computer gives: a connection event every 15 ms, packets of 20 octets (the smallest MTU), two
// notifications an event from the FM-1 (firmware/bt/ble/ble_cfg.h, BLE_MIDI_PER_EVENT) out of its queue of 64 (a full
// queue drops, as bt_oq_put does), one write an event from the page. Both ways go through the real BLE-MIDI packets
// (web/link/ble.js), the page's side through the real write queue (Outbox) and remote.js's bleAccess.
//   node tests/remote_ble_test.mjs        (after web/emu/build.sh)
// The FM-1's save comes over whole with nothing dropped, paced by the room in its queue (bt_tx_room); the panel and the
// lights; the page's save kept on the FM-1 with nothing dropped and no hello missed; goodbye.
import { readFileSync } from "node:fs";
import { Fm1Remote, bleAccess } from "../web/emu/remote.js";
import { blePack, BleDecoder, Outbox } from "../web/link/ble.js";
const fail = (m) => { console.error("FAIL", m); process.exit(1); };
const ok = (m) => console.log("ok  ", m);
const WASM = readFileSync("build/emu/fm-arcave.wasm");
const IV = 15, CAP = 20, NOTIFY = 2, WRITES = 1, BT_OQ = 64;

async function fm(store) {
  const { instance } = await WebAssembly.instantiate(WASM, {});
  const ex = instance.exports;
  if (ex._initialize) ex._initialize();
  if (store) { new Uint8Array(ex.memory.buffer, ex.web_store(), store.length).set(store); ex.web_store_set(store.length); }
  ex.web_boot();
  return { ex, gen: ex.web_store_gen() };
}
const storeOf = (f) => new Uint8Array(f.ex.memory.buffer, f.ex.web_store(), f.ex.web_store_len()).slice();
const leds = (f) => [f.ex.web_lit_buttons(), f.ex.web_lit_keys(), f.ex.web_dim_keys(), f.ex.web_breath_buttons()];
const lang = (f) => f.ex.web_scene() >> 16 & 255;
const remoteOn = (f) => f.ex.web_remote() & 1;
const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);

const old = new Uint8Array(192), dv = new DataView(old.buffer);   // the FM-1: a save from 0.19, the jukebox mended
dv.setUint32(0, 0x41434156, true); old[4] = 0; old[6] = 4; old[10] = 90; old[11] = 2;
const F = await fm(old);
let D = await fm(null);
let now = 0;

// the link. The FM-1's side: what midi_send gives Bluetooth waits in a queue of 64, two packets go each event
const btq = [];                                    // groups of one message, as blePack takes them
let btDropped = 0, btMax = 0;
const toPage = new BleDecoder(), toFm1 = new BleDecoder();
// the page's side: a BlePorts port as ble.js makes one, its writes taken one an event
const writes = [];
const port = { id: "ble:test", name: "FM-ARCAVE 3F2A", kind: "ble", state: "on" };
port.out = new Outbox((pkt) => new Promise((res) => writes.push({ pkt, res })));
port.send = (bytes) => { if (port.state === "on") port.out.send(bytes); };
const acc = bleAccess(port);
let fm1In = 0;

let saveIn = null, kept = [], states = [];
const R = new Fm1Remote(acc, {
  now: () => now,
  onState: (s, d) => states.push(d || s),
  onSave: (b) => { saveIn = b; },
  onInput: (b, k) => { D.ex.web_buttons(b); D.ex.web_keys(k); },
  onKnob: (id, n) => D.ex.web_enc(id, n),
  onKept: (r) => kept.push(r),
});
let frameAt = 0, pageLen = 0;
const yieldNow = () => new Promise((r) => setImmediate(r));
async function run(ms) {
  for (let t = 0; t < ms; t++) {
    now++;
    if (now % IV === 0) {                          // a connection event
      for (let k = 0; k < NOTIFY; k++) {
        const pkt = blePack(btq, now, CAP);
        if (!pkt) break;
        for (const m of toPage.take(pkt)) acc.heard(m.d, now);
      }
      for (let k = 0; k < WRITES && writes.length; k++) {
        const w = writes.shift();
        for (const m of toFm1.take(w.pkt)) { F.ex.web_midi(m.d[0], m.d[1], m.d[2]); fm1In++; }
        w.res();
      }
    }
    F.ex.web_bt_room(BT_OQ - btq.length);
    F.ex.web_render(44); D.ex.web_render(44);
    if (now % 10 === 0) { F.ex.web_render(1); D.ex.web_render(1); }
    for (let v; (v = F.ex.web_midi_out()) >= 0;) {
      if (btq.length < BT_OQ) btq.push([[v & 255, v >> 8 & 127, v >> 16 & 127]]); else btDropped++;
    }
    btMax = Math.max(btMax, btq.length);
    while (D.ex.web_midi_out() >= 0);
    if (now % 20 === 0) R.tick();
    if (now - frameAt >= 33) {
      frameAt = now;
      R.lights(leds(D));
      const g = D.ex.web_store_gen();
      if (g !== D.gen) { D.gen = g; const s = storeOf(D); if (R.state === "connected" && !R.leaving && s.length === pageLen) R.sendSave(s); }
    }
    await yieldNow();                              // (the write queue's promises)
  }
}
const until = async (what, cond, ms) => { for (let t = 0; t < ms; t += 10) { if (cond()) return t; await run(10); } fail(what); };

await run(1000);
// 1. the FM-1's save, across a slow link, nothing dropped
R.start();
const tSave = await until("the FM-1's save never came over Bluetooth", () => saveIn, 15000);
const fsave = storeOf(F);
if (!remoteOn(F)) fail("the FM-1 is not in remote mode");
if (!same(saveIn, fsave)) fail("the save that came is not the FM-1's");
if (btDropped) fail(`the FM-1's Bluetooth queue overflowed: ${btDropped} messages dropped (the most waiting: ${btMax})`);
ok(`the FM-1's save over Bluetooth (${saveIn.length} bytes in ${(tSave / 1000).toFixed(1)} s), its queue never full (at most ${btMax} of ${BT_OQ} waiting)`);

// 2. the page's game from it; the panel and the lights
D = await fm(saveIn); pageLen = saveIn.length; frameAt = now;
await run(1000);
F.ex.web_buttons(1 << 1); await run(60); F.ex.web_buttons(0); await run(400);   // OCT+: the next language
if (lang(D) !== 1) fail("OCT+ on the FM-1 did not change the page's language: " + lang(D));
F.ex.web_keys(1 << 5); await run(60);
if (R.keys !== 1 << 5) fail("a key held on the FM-1 is not held in the page: " + R.keys);
F.ex.web_keys(0); await run(60);
if (R.keys !== 0) fail("a key let go on the FM-1 is still held in the page");
await run(600);
if (leds(F).join() !== leds(D).join()) fail(`the FM-1's lights ${leds(F)} are not the page's ${leds(D)}`);
ok("buttons and keys from the FM-1 reach the page by Bluetooth; the page's lights reach the FM-1");

// 3. the page's save kept on the FM-1, nothing dropped, no hello missed while it went
const at = now;
await until("the page's save was never kept on the FM-1", () => kept.includes(1), 30000);
if (!same(storeOf(F), storeOf(D))) fail("the FM-1 does not hold the page's save");
if (port.out.dropped) fail(`the page's write queue dropped ${port.out.dropped} sends`);
if (states.includes("lost") || states.includes("damaged")) fail("the link was taken for lost, or a save for damaged: " + states.join(" "));
if (!remoteOn(F)) fail("the FM-1 left remote mode while the page's save came (a hello missed)");
if (btDropped) fail(`the FM-1's Bluetooth queue overflowed: ${btDropped} dropped`);
ok(`the page's save kept on the FM-1 by Bluetooth, byte for byte (${((now - at) / 1000).toFixed(1)} s, one 20-octet write an event)`);

// 4. let go straight after playing (the page's button): the game saved at once, kept by Bluetooth, then the goodbye
F.ex.web_buttons(1 << 1); await run(60); F.ex.web_buttons(0); await run(150);   // OCT+ again, not yet saved
if (lang(D) !== 2) fail("OCT+ on the FM-1 did not change the page's language again: " + lang(D));
D.ex.web_save_now(); D.gen = D.ex.web_store_gen();
const t0 = now, leaving = R.leave(storeOf(D));
await until("letting go never finished", () => !R.leaving, 30000);
const how = await leaving;
if (how !== "kept") fail("letting go: the last save was " + how);
await run(500);
if (remoteOn(F)) fail("the goodbye did not reach the FM-1");
if (lang(F) !== 2) fail("the FM-1 did not keep what was played just before letting go (language " + lang(F) + ")");
ok(`let go straight after playing: the last save kept by Bluetooth (${((now - t0) / 1000).toFixed(1)} s), then goodbye; the FM-1 is itself again`);
console.log("remote by Bluetooth: all passed");
