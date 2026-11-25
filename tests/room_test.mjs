// SPDX-License-Identifier: GPL-3.0-only
// The music room with friends (firmware/src/arcave/ac_roomnet.h): four copies of the browser build wired to each other
// as the Link page wires FM-1s (what web/link/undersong.js gathers from one, the others get; DELAY ms each way).
//   node tests/room_test.mjs [DELAY]          (after web/emu/build.sh)
// One is in its room first and changes it; the others come in one by one and are sent that room whole. Then: one each
// on an instrument (a second player is refused), a knob, the pool's mute, the quick mix's solo from a third, REC and
// keys onto a loop all four end up with, the host going dark without the room changing hands badly, and one player
// turning TOGETHER off: their own room again, the rest undisturbed. After every step every FM-1 must hold the same room.
// Last, two FM-1s with an instrument LOADED into a place (THE SPRING in THE GEODE's: ac_slots.h): beside friends who
// have the firmware's own four they step out and have their own rooms, the friends undisturbed; with those gone the
// two find each other and share a room, the loaded instrument's knob and all.
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { Gather, passes } from "../web/link/undersong.js";
import { crc32 } from "../web/emu/saves.js";
const fail = (m) => { console.error("FAIL", m); process.exit(1); };
const OUT = "build/host"; mkdirSync(OUT, { recursive: true });
const DELAY = +(process.argv[2] || 0);                      // ms each way
const save = () => { const b = new Uint8Array(192), v = new DataView(b.buffer); v.setUint32(0, 0x41434156, true); b[6] = 4; b[10] = 90; b[11] = 2; return b; };
let now = 0;
const fms = [];
async function fm(name, warm) {
  const { instance } = await WebAssembly.instantiate(readFileSync("build/emu/fm-arcave.wasm"), {});
  const ex = instance.exports, st = save();
  if (ex._initialize) ex._initialize();
  new Uint8Array(ex.memory.buffer, ex.web_store(), st.length).set(st); ex.web_store_set(st.length);
  ex.web_boot();
  const f = { ex, name, q: [], on: true, g: new Gather() };
  for (let i = 0; i < warm * 441 / 1280; i++) ex.web_render(128);   // (each has been on for a different while)
  ex.web_to_room();
  fms.push(f);
  return f;
}
let sent = 0;
function step() {                                           // 128 samples: about 2.9 ms
  now += 128000 / 44100;
  for (const f of fms) if (f.on) f.ex.web_render(128);
  for (const f of fms) if (f.on) for (let v; (v = f.ex.web_midi_out()) >= 0;) if ((v & 255) === 0xBF) {
    const cc = v >> 8 & 127, x = v >> 16 & 127;             // as the Link page does: gathered (undersong.js), each piece whole
    if (!passes(cc, x) && !(cc >= 102 && cc <= 106)) continue;
    const d = f.g.take(cc, x);
    if (d) for (const g of fms) if (g !== f && g.on) { g.q.push([now + DELAY, d]); sent += d.length / 2; }
  }
  for (const f of fms) while (f.q.length && f.q[0][0] <= now) { const d = f.q.shift()[1]; for (let i = 0; i < d.length; i += 2) f.ex.web_midi(0xBF, d[i], d[i + 1]); }
}
const run = (ms) => { for (let i = 0; i < ms * 441 / 1280; i++) step(); };
const tap = (f, btn, ms = 200) => { f.ex.web_buttons(1 << btn); run(80); f.ex.web_buttons(0); run(ms); };
const key = (f, k, ms = 150) => { f.ex.web_keys(1 << k); run(80); f.ex.web_keys(0); run(ms); };
const R_LOW = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
const st = (f) => { const d = f.ex.web_room(); return { seat: d & 7, np: d >> 3 & 7, tuning: d >> 6 & 3, st: d >> 8 & 15, focus: d >> 12 & 255, guest: d >> 20 & 1, tag: d >> 21 & 127, vw: d >>> 28 & 7 }; };
const sum = (f) => f.ex.web_room_sum() >>> 0, byte = (f, i) => f.ex.web_room_byte(i);
const live = () => fms.filter((f) => f.on);
const same = () => live().every((f) => sum(f) === sum(live()[0]));
const until = (what, cond, ms) => { for (let t = 0; t < ms; t += 50) { if (cond()) return t; run(50); } fail(what + "  " + live().map((f) => f.name + JSON.stringify(st(f)) + " " + sum(f).toString(16)).join("  ")); };
const agree = (what, ms = 4000) => until("not the same room after " + what, same, ms);
let shots = 0;
function shot(f, label) {
  const px = new Uint16Array(f.ex.memory.buffer, f.ex.web_screen(), 57600), o = Buffer.alloc(57600 * 3);
  for (let i = 0; i < 57600; i++) { const c = px[i]; o[i * 3] = (c >> 11) * 255 / 31; o[i * 3 + 1] = (c >> 5 & 63) * 255 / 63; o[i * 3 + 2] = (c & 31) * 255 / 31; }
  writeFileSync(`${OUT}/roomtest-${String(shots++).padStart(2, "0")}-${label}-${f.name}.ppm`, Buffer.concat([Buffer.from("P6 240 240 255\n"), o]));
}
function open(f, btn, who) {                                // its button; the first time, HOME ends the lesson that starts
  tap(f, btn, 600);
  tap(f, 8, 300);
  if (st(f).focus !== who) fail(`${f.name} did not get instrument ${who}: ${JSON.stringify(st(f))}`);
}

const A = await fm("A", 5000);
run(2500);
if (st(A).np !== 1 || st(A).tuning || st(A).guest) fail("alone, A is not simply in its own room: " + JSON.stringify(st(A)));
const REP = byte(A, 9001), N = byte(A, 9002), MIX = 4 * REP;
open(A, 2, 0);                                              // A, alone: ROOTS, the wind up, back; the geode muted in the quick mix
A.ex.web_enc(2, 3); run(300);
tap(A, 8, 500);
A.ex.web_buttons(1 << 7); run(200); key(A, R_LOW[4]); A.ex.web_buttons(0); run(300);
if (!byte(A, MIX + 8 + 6)) fail("the quick mix did not mute the geode on A");
const wind = byte(A, 128);
const s0 = sum(A);

const B = await fm("B", 9000);
until("B was not sent A's room", () => st(B).tag === st(A).tag && !st(B).tuning && st(B).guest === 1 && same(), 15000);
if (sum(A) !== s0) fail("A's own room changed when B came in");
if (st(A).seat !== 0 || st(B).seat !== 1 || byte(B, 128) !== wind || !byte(B, MIX + 8 + 6)) fail("B has not A's room: " + JSON.stringify([st(A), st(B)]));
shot(B, "tuned");
const C = await fm("C", 14000);
run(700); shot(C, "tuning");
const D = await fm("D", 20000);
const tIn = until("C and D were not sent the room", () => live().every((f) => st(f).np === 4 && !st(f).tuning && st(f).tag === st(A).tag) && same(), 25000);
if ([A, B, C, D].map((f) => st(f).seat).join("") !== "0123") fail("seats: " + [A, B, C, D].map((f) => st(f).seat));
console.log(`four in one room (C and D tuned in ${tIn} ms after D came in; ${N} shared bytes)`);
shot(A, "four");

open(B, 3, 1);                                              // B takes THE GEODE; C wants it too and is told no
run(400);
tap(C, 3, 900);
if (st(C).focus !== 255) fail("C got the geode while B was on it: " + JSON.stringify(st(C)));
shot(C, "refused");
const w0 = byte(A, REP + 129);
B.ex.web_enc(3, 2); run(100);                               // B: KNOB 2, the stones' weight
agree("B's knob");
if (byte(A, REP + 129) === w0) fail("B's knob did not reach A");
open(C, 5, 2);                                              // C: MUSHROOM RACK
C.ex.web_enc(1, 1);                                         // a ready-made line: sixteen caps change at once
agree("C's layout", 8000);
tap(D, 4, 700);                                             // D: THE POOL. ROOTS muted there
if (st(D).focus !== 4) fail("D did not get the pool");
key(D, R_LOW[0]);
agree("D's mute");
if (!byte(A, MIX + 6)) fail("D's mute of ROOTS did not reach A");
shot(D, "pool");
A.ex.web_buttons(1 << 7); run(200); key(A, R_LOW[13]); A.ex.web_buttons(0); run(200);   // A, loose in the room: the quick mix, SEAM alone
agree("A's solo");
if (!byte(D, MIX + 24 + 7)) fail("A's solo of SEAM did not reach D");
A.ex.web_buttons(1 << 7); run(200); key(A, R_LOW[13]); key(A, R_LOW[0]); key(A, R_LOW[4]); A.ex.web_buttons(0); run(200);   // (and everything heard again)
agree("A's quick mix");

tap(B, 13, 300);                                            // B: REC, a few of the geode's parts by hand, REC
until("REC did not reach everyone", () => live().every((f) => byte(f, N - 1) === 1), 3000);
for (const k of [5, 6, 7, 8, 9]) key(B, R_LOW[k], 250);
run(2500);
tap(B, 13, 300);
agree("the recording", 6000);
const loop = byte(A, 9000);
if (!loop || live().some((f) => byte(f, 9000) !== loop)) fail("the loop: " + live().map((f) => byte(f, 9000)));
console.log(`a loop of ${loop} hits on all four`);
const ticks = live().map((f) => f.ex.web_tick());
if (Math.max(...ticks) - Math.min(...ticks) > 12 + DELAY / 4) fail("the clocks: " + ticks);

// ---- watching. A, loose in the room, holds GLO and taps SCL: onto THE GEODE, which B plays, so A watches it
A.ex.web_buttons(1 << 7); run(150); A.ex.web_buttons(1 << 7 | 1 << 3); run(80); A.ex.web_buttons(1 << 7); run(150); A.ex.web_buttons(0); run(600);
if (st(A).focus !== 1 || st(A).vw !== 2 || st(A).st !== 0 || st(B).focus !== 1 || st(B).st !== 2) fail("A is not watching B's geode: " + JSON.stringify([st(A), st(B)]));
const w1 = byte(B, REP + 129);
A.ex.web_enc(3, 2); run(400);                               // A's knob, watching: nothing
agree("A's knob, watching");
if (byte(B, REP + 129) !== w1) fail("A's knob reached the geode while A only watched it");
shot(A, "watching"); shot(B, "watched");
A.ex.web_buttons(3); run(80); A.ex.web_buttons(0); run(300);   // OCT- and OCT+ by both: they swap
B.ex.web_buttons(3); run(80); B.ex.web_buttons(0);
until("A and B did not swap", () => st(A).vw === 0 && st(A).st === 2 && st(B).vw === 2 && st(B).st === 0, 3000);
run(1200);
if (st(A).st !== 2 || st(B).vw !== 2) fail("the swap did not hold: " + JSON.stringify([st(A), st(B)]));
A.ex.web_enc(3, -2); run(200);
agree("A's knob, playing");
if (byte(B, REP + 129) === w1) fail("A's knob did not reach B after the swap");
shot(A, "swapped"); shot(B, "swapped");
tap(A, 8, 400);                                             // A goes back into the room: the geode is B's again
until("B did not get the geode back when A left it", () => st(A).focus === 255 && st(B).vw === 0 && st(B).st === 2, 4000);
console.log("A watched B's geode (its knob did nothing), they swapped with OCT- and OCT+, and A leaving gave it back to B");

A.on = false;                                               // the host goes dark: B's seat is 0 now, nobody is sent anything again
until("the three did not close ranks", () => live().every((f) => st(f).np === 3), 6000);
if (st(B).seat !== 0 || live().some((f) => st(f).tuning)) fail("after the host left: " + live().map((f) => JSON.stringify(st(f))));
B.ex.web_enc(2, -2); run(100);
agree("B's knob, the host gone");

tap(D, 8, 500);                                             // D: out of the pool, the settings, MUSIC ROOM, TOGETHER off
tap(D, 7, 400); tap(D, 12, 400);
for (let i = 0; i < 6; i++) { D.ex.web_enc(0, 1); run(120); }
const before = sum(B);
tap(D, 12, 600);
until("D is not alone in its own room", () => st(D).np === 1 && !st(D).guest, 4000);
if (sum(D) === before) fail("D kept the friends' room after turning TOGETHER off");
shot(D, "alone");
until("B and C did not close ranks", () => st(B).np === 2 && st(C).np === 2, 6000);
if (sum(B) !== before || sum(C) !== before) fail("the room changed when D left it");

// ---- other instruments. THE SPRING into THE GEODE's place, as the MACHINES page does it for the browser's game
function loadSpring(f) {
  const img = new Uint8Array(40), v = new DataView(img.buffer), t = new Uint8Array(36);
  v.setUint32(0, 0x534e494d, true); v.setUint16(4, 1, true); v.setUint16(8, 16, true); v.setUint16(10, 1, true); v.setUint32(12, 40, true);
  t.set(img.subarray(0, 16)); t.set(img.subarray(20), 16); v.setUint32(16, crc32(t), true);
  if (f.ex.web_bank_cmd(80 + 1)) fail("the slot of THE GEODE's place was not emptied");
  new Uint8Array(f.ex.memory.buffer, f.ex.web_bank() + 4 * 0xa000, 40).set(img);
  if (f.ex.web_bank_cmd(84 + 1) || !(f.ex.web_place(1) >>> 31)) fail("THE SPRING was not taken into THE GEODE's place");
}
const E = await fm("E", 3000); loadSpring(E);
until("E, with other instruments, did not step out", () => st(E).seat === 4 && !st(E).guest && !st(E).tuning, 8000);
until("B and C did not close ranks after E", () => st(B).np === 2 && st(C).np === 2, 8000);
if (sum(B) !== before || sum(C) !== before || sum(E) === before) fail("the room changed when an FM-1 with other instruments came by");
const G2 = await fm("G", 7000); loadSpring(G2);
until("G, with other instruments, did not step out", () => st(G2).seat === 4 && !st(G2).tuning, 8000);
run(1500);
if (sum(B) !== before || sum(C) !== before || st(B).np !== 2) fail("the room changed when a second FM-1 with other instruments came by");
console.log("two FM-1s with THE SPRING loaded stepped out beside friends with the firmware's four: their own rooms, the friends' room as it was");
B.on = false; C.on = false;                                 // the friends gone: the two with the same instruments find each other
until("E and G did not find each other", () => st(E).np === 2 && st(G2).np === 2 && st(E).seat < 4 && st(G2).seat < 4 && !st(E).tuning && !st(G2).tuning && st(E).tag && st(E).tag === st(G2).tag, 15000);
until("E and G do not hold the same room", () => sum(E) === sum(G2), 6000);
open(E, 3, 1);                                              // E on THE SPRING (SCL, THE GEODE's button): its heat up
E.ex.web_enc(2, 3); run(300);
until("G did not hear E's knob on THE SPRING", () => sum(E) === sum(G2) && byte(G2, REP + 128) === byte(E, REP + 128), 5000);
if (byte(E, REP + 128) !== 7) fail("KNOB 1 on THE SPRING: " + byte(E, REP + 128));
tap(G2, 3, 900);                                            // G wants it too: E has it
if (st(G2).focus === 1) fail("two players on THE SPRING");
shot(E, "spring");
console.log("with the friends gone the two share one room: THE SPRING in its place on both, a knob heard, one player on it");
console.log(`room_test: passed (${sent} control changes passed on, ${DELAY} ms each way)`);
