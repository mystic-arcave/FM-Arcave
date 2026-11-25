// SPDX-License-Identifier: GPL-3.0-only
// The mystic machines' slots (firmware/src/arcave/ac_slots.h, mm.h; web/emu/machines.js, the MYSTIC MACHINES page's own
// module) without an FM-1: a copy of the browser build plays the FM-1's part over a made-up Web MIDI port, on one clock
// (as tests/save_test.mjs). The browser build has every loadable machine built in and a bank of plain bytes, so what
// is tested here is everything but an FM-1 running a machine from its flash.
//   node tests/machines_test.mjs            (after web/emu/build.sh; with build/machines/glowcaps.mmach from
//                                            tools/mm_build.py, the real file's addresses are checked too)
// A machine file read, moved to a slot and sealed, damaged ones refused; an FM-1's ledges read; GLOW CAPS loaded into
// the second ledge's slot in pieces, the FM-1 starting again with it there, a game of it played (caps sung by the keys
// the FM-1 lights, the hearts gone, its best score kept in the save); a longer file in many pieces; a piece damaged on
// the way; a machine for a newer firmware and one this build does not have: refused, the firmware's own machine back;
// the ledges' own machines chosen, the loaded one put back, a slot emptied; all of it still so after the FM-1 is off
// and on; a save played where its machines are not keeps what was kept for them; and the browser game's own bank.
// The music room's four places the same way: THE SPRING loaded into THE GEODE's place, played in the room (its bubbles
// pop and sound, a knob, a part of the loop); the firmware's own instrument back with its laws as they were left and
// the loaded one's kept beside it, also through a save played where the instrument is not; a machine sent to a place,
// an instrument for a newer firmware: refused.
import { readFileSync, existsSync } from "node:fs";
import { parseFile, forSlot, parseInfo, fits, Fm1Bank, BrowserBank, readBank, BANK_BYTES, BANK_KEY, ST, MAGIC, MAGIC_INST, RELOC, HEAD } from "../web/emu/machines.js";
import { crc32, SAVE_KEY, unb64 } from "../web/emu/saves.js";
const fail = (m) => { console.error("FAIL", m); process.exit(1); };
const ok = (m) => console.log("ok  ", m);
const WASM = readFileSync("build/emu/fm-arcave.wasm");
const SLOT = 0xa000, BASE = 0x02093000;

// a machine file as tools/mm_build.py writes one: the image (its header first), the places of its addresses, the tail
function file(img, rel = []) {
  const b = img.slice(), v = new DataView(b.buffer), t = new Uint8Array(b.length - 4);
  v.setUint32(12, b.length, true);
  t.set(b.subarray(0, 16)); t.set(b.subarray(HEAD), 16); v.setUint32(16, crc32(t), true);
  const out = new Uint8Array(b.length + 4 * rel.length + 12), o = new DataView(out.buffer);
  out.set(b);
  rel.forEach((w, i) => o.setUint32(b.length + 4 * i, w, true));
  o.setUint32(b.length + 4 * rel.length, rel.length, true); o.setUint32(b.length + 4 * rel.length + 4, RELOC, true); o.setUint32(b.length + 4 * rel.length + 8, b.length, true);
  return out;
}
// a header and no code (the browser game's copy), padded to n bytes
function twin(id, api = 1, n = 40, name = "GLOW CAPS") {
  const b = new Uint8Array(n), v = new DataView(b.buffer);
  v.setUint32(0, MAGIC, true); v.setUint16(4, api, true); v.setUint16(8, id, true); v.setUint16(10, 1, true);
  for (let i = 0; i < name.length; i++) b[20 + i] = name.charCodeAt(i);
  for (let i = 40; i < n; i++) b[i] = (i * 7 + 3) & 255;
  return b;
}

// ---------------------------------------------------------------- the files
const caps = parseFile(file(twin(16)));
if (caps.id !== 16 || caps.api !== 1 || caps.code || caps.img.length !== 40) fail("a machine file was not read: " + JSON.stringify({ ...caps, img: 0 }));
{   // addresses: at a word and at a halfword, moved by a slot's distance times the slot; the seal made again
  const img = twin(77, 1, 256), v = new DataView(img.buffer);
  v.setUint32(100, BASE + 0x40, true); v.setUint32(150, BASE + 0x80, true);
  const f = parseFile(file(img, [100, 150])), m = forSlot(f, 2, SLOT), w = new DataView(m.buffer);
  const again = parseFile(file(m, [100, 150]));
  if (w.getUint32(100, true) !== BASE + 0x40 + 2 * SLOT || w.getUint32(150, true) !== BASE + 0x80 + 2 * SLOT || !f.code || again.img.length !== 256)
    fail("a machine was not moved to its slot");
  if (new DataView(forSlot(f, 0, SLOT).buffer).getUint32(100, true) !== BASE + 0x40) fail("slot 0 moved a machine");
}
const refused = (what, bytes) => { try { parseFile(bytes); } catch (e) { if (/not a mystic machine|damaged/.test(e.message)) return; fail(what + ": " + e.message); } fail(what + " was read"); };
{ const f = file(twin(16)); f[25] ^= 1; refused("a file with a byte changed", f); }
refused("a file cut short", file(twin(16)).slice(0, 30));
refused("a file of something else", new Uint8Array(64).fill(9));
{ const f = file(twin(16, 1, 64), [200]); refused("a file with an address outside it", f); }
let real = null;
if (existsSync("build/machines/glowcaps.mmach")) {
  real = parseFile(readFileSync("build/machines/glowcaps.mmach"));
  const a = new DataView(real.img.buffer), b = new DataView(forSlot(real, 1, SLOT).buffer);
  if (real.code) {
    if (real.id !== 16 || real.rel.length < 6) fail("the built GLOW CAPS: number " + real.id + ", " + real.rel.length + " addresses");
    for (const w of real.rel) {
      const x = a.getUint32(w, true);
      if (x < BASE || x >= BASE + real.img.length || b.getUint32(w, true) !== x + SLOT) fail("the built GLOW CAPS: the address at " + w + " is not one inside it");
    }
    for (const w of [24, 28, 32, 36, 40, 44]) if (!real.rel.includes(w)) fail("the built GLOW CAPS: its header's address at " + w + " is not listed");
  }
}
ok("machine files: read, moved to a slot (addresses at words and halfwords) and sealed; a changed byte, a cut file, an address outside: refused"
  + (real && real.code ? `; the built GLOW CAPS: ${real.img.length} bytes, ${real.rel.length} addresses, each inside it` : ""));

// ---------------------------------------------------------------- an FM-1 on a made-up MIDI port
async function fm(store, bank) {
  const { instance } = await WebAssembly.instantiate(WASM, {});
  const ex = instance.exports;
  if (ex._initialize) ex._initialize();
  if (store) { new Uint8Array(ex.memory.buffer, ex.web_store(), store.length).set(store); ex.web_store_set(store.length); }
  if (bank) new Uint8Array(ex.memory.buffer, ex.web_bank(), bank.length).set(bank);
  ex.web_boot();
  return { ex };
}
const storeOf = (f) => new Uint8Array(f.ex.memory.buffer, f.ex.web_store(), f.ex.web_store_len()).slice();
const bankOf = (f) => new Uint8Array(f.ex.memory.buffer, f.ex.web_bank(), f.ex.web_bank_len()).slice();
const ledge = (f, i) => { const v = f.ex.web_ledge(i); return { own: v & 3, loaded: !!(v & 0x80), st: v >> 8 & 255, id: v >>> 16 }; };
const mach = (f) => { const v = f.ex.web_mach(); return { which: v & 15, state: v >> 4 & 15, in: v >> 8 & 1, slot: v >> 12 & 15, help: v >> 16 & 1 }; };
const seed = () => { const b = new Uint8Array(192), v = new DataView(b.buffer); v.setUint32(0, 0x41434156, true); b[6] = 4; b[10] = 90; b[11] = 2; return b; };
let now = 0, P = null, link = null, spoil = 0;
function port(F) {
  const toFm1 = [], input = { name: "FM-1 MIDI", onmidimessage: null };
  const output = { name: "FM-1 MIDI", send: (d, at) => {
    for (let k = 0; k + 2 < d.length || k === 0; k += 3) {
      const t = at === undefined ? now : at, i = toFm1.findIndex((m) => m[0] > t);
      toFm1.splice(i < 0 ? toFm1.length : i, 0, [t, d[k], d[k + 1], d[k + 2]]);
    }
  } };
  return { F, toFm1, input, access: { inputs: new Map([["i", input]]), outputs: new Map([["o", output]]), onstatechange: null } };
}
function run(ms) {
  for (let t = 0; t < ms; t++) {
    now++;
    while (P.toFm1.length && P.toFm1[0][0] <= now) {
      const [, st, d1, d2] = P.toFm1.shift();
      if (spoil && d1 === 76 && !--spoil) { P.F.ex.web_midi(st, d1, d2 ^ 1); continue; }   // (one byte of a piece spoiled on the way)
      P.F.ex.web_midi(st, d1, d2);
    }
    P.F.ex.web_render(44);
    if (now % 10 === 0) P.F.ex.web_render(1);
    for (let v; (v = P.F.ex.web_midi_out()) >= 0;) P.input.onmidimessage?.({ data: [v & 255, v >> 8 & 127, v >> 16 & 127], target: P.input });
    if (link && now % 20 === 0) link.tick();
  }
}
async function settle(p, what, ms = 60000) {
  let done = false, val, err;
  p.then((v) => { done = true; val = v; }, (e) => { done = true; err = e; });
  for (let t = 0; t < ms && !done; t += 5) { run(5); await null; await null; await null; }
  if (!done) fail(what + ": nothing after " + ms + " ms");
  if (err) throw err;
  return val;
}
const open = async (F) => { P = port(F); link = new Fm1Bank(P.access, { now: () => now }); return (await settle(link.open(), "opening the FM-1's bank")).info; };
const shut = () => { link.close(); run(500); link = null; };

const F = await fm(seed());
P = port(F); run(1200);
let info = await open(F);
if (info.api !== 1 || info.nslot !== 3 || info.slotBytes !== SLOT || !info.runs || !info.bank
  || info.slots.map((s) => s.title).join() !== "CRYSTAL ROAD,JUICE WORKS,NIGHT BLOOM" || info.slots.some((s) => s.loaded || s.st !== ST.EMPTY))
  fail("a new FM-1's ledges: " + JSON.stringify(info));
if (fits(caps, info, false) || !/no code/.test(fits(caps, info, true))) fail("fits(): the browser game's copy of a machine");
ok("an FM-1's ledges read: CRYSTAL ROAD, JUICE WORKS, NIGHT BLOOM, three empty slots of 40 KiB");

let seen = 0, t0 = now;
let r = await settle(link.load(1, caps, (n) => { seen = n; }), "loading GLOW CAPS");
info = link.info;
if (r !== 0 || seen !== 40 || !info.slots[1].loaded || info.slots[1].st !== ST.OK || info.slots[1].id !== 16 || info.slots[1].title !== "GLOW CAPS" || info.slots[0].title !== "CRYSTAL ROAD")
  fail(`GLOW CAPS loaded into the second ledge: answer ${r}, ` + JSON.stringify(info.slots[1]));
shut();
if (F.ex.web_remote() & 1 || !ledge(F, 1).loaded || ledge(F, 1).id !== 16 || ledge(F, 0).loaded) fail("after the page let go the FM-1's ledges are not as it left them");
ok(`GLOW CAPS loaded into the second ledge's slot in ${now - t0} ms; the FM-1 its own game again, the machine on its ledge`);

// a game of it: up to the ledge, PLAY, the keys the FM-1 lights pressed as they light, then none until the hearts are gone
P = port(F);
F.ex.web_to_ledge(1); run(300);
if (mach(F).which !== 3 || !mach(F).in || mach(F).state !== 0 || mach(F).slot !== 1) fail("up to the second ledge: " + JSON.stringify(mach(F)));
F.ex.web_buttons(1 << 8); run(60); F.ex.web_buttons(0); run(200);
if (!mach(F).help) fail("HOME did not open the machine's own words");
F.ex.web_buttons(1 << 8); run(60); F.ex.web_buttons(0); run(200);
F.ex.web_buttons(1 << 12); run(60); F.ex.web_buttons(0); run(100);
if (mach(F).state !== 1 || mach(F).help) fail("PLAY did not start GLOW CAPS: " + JSON.stringify(mach(F)));
for (let t = 0; t < 12000 && F.ex.web_mach_score() < 60; t += 20) {
  const lit = F.ex.web_lit_keys();
  if (lit) { F.ex.web_keys(lit); run(20); F.ex.web_keys(0); run(100); } else run(20);
}
const score = F.ex.web_mach_score();
if (score < 60) fail("the keys the FM-1 lit, pressed as they lit, did not sing the caps: " + score);
for (let t = 0; t < 40000 && mach(F).state === 1; t += 100) run(100);
if (mach(F).state !== 2) fail("the hearts never ran out");
run(4000);
const bestAt = F.ex.web_save_part(0) + 4 + 16 * 1 + 4;
const best = (s) => new DataView(s.buffer).getUint32(bestAt, true);
if (best(storeOf(F)) !== score) fail(`GLOW CAPS' best score is not in the save: ${best(storeOf(F))}, played ${score}`);
for (let k = 0; k < 3 && mach(F).in; k++) { F.ex.web_buttons(1 << 13); run(60); F.ex.web_buttons(0); run(900); }   // (REC: past the tickets if they are still coming, then out)
if (mach(F).in) fail("REC did not go back to the Arcave from GLOW CAPS");
ok(`a game of GLOW CAPS on its ledge: HOME's page, ${score} points from the keys the FM-1 lit, the hearts gone, its best kept in the save`);

// a save played where its machines are not (the browser, with PLAY WITH MY FM-1) keeps what was kept for them
{
  const G = await fm(storeOf(F), null);
  if (ledge(G, 1).st !== ST.EMPTY || !ledge(G, 1).loaded) fail("the save's ledges without its machines");
  G.ex.web_save_now();
  if (best(storeOf(G)) !== score) fail("a save played where its machine is not lost the machine's best score");
  G.ex.web_to_ledge(1);
  if ((G.ex.web_mach() & 15) !== 1) fail("a ledge whose loaded machine is not there did not show the firmware's own (JUICE WORKS)");
}
ok("the same save where the machine is not: the ledge shows the firmware's own again, what was kept for the machine is still kept");

// more of the bank
info = await open(F);
if (!info.slots[1].loaded || info.slots[1].title !== "GLOW CAPS") fail("the FM-1 forgot its loaded machine");
const long = parseFile(file(twin(16, 1, 5200)));
t0 = now; seen = 0;
r = await settle(link.load(0, long, (n) => { seen = n; }), "a machine of six pieces");
if (r !== 0 || seen !== 5200 || link.info.slots[0].len !== 5200 || link.info.slots[0].title !== "GLOW CAPS") fail(`a machine of six pieces: answer ${r}, ${seen} bytes`);
const inBank = bankOf(F).subarray(0, 5200);
if (!inBank.every((v, i) => v === forSlot(long, 0, SLOT)[i])) fail("a machine of six pieces is not in the bank as it was sent");
ok(`a machine of 5200 bytes in six pieces, each answered before the next: ${now - t0} ms, in the bank whole`);

spoil = 700;                                       // a byte of the first piece spoiled on the way: the FM-1 keeps none of it, the page sends it again
r = await settle(link.load(2, long), "a piece damaged on the way");
if (r !== 0 || spoil || link.info.slots[2].st !== ST.OK) fail("a piece damaged on the way was not sent again: answer " + r);
r = await settle(link.r.bank(16 + 2), "emptying a slot");
await settle(link.r.sendPiece(1 + 2, forSlot(caps, 2, SLOT).map((v, i) => (i === 30 ? v ^ 1 : v))), "a machine with a byte wrong");
r = await settle(link.r.bank(32 + 2), "checking it");
info = await settle(link.refresh(), "the bank's state");
if (r !== 2 || info.slots[2].st !== ST.BAD || info.slots[2].loaded || info.slots[2].title !== "NIGHT BLOOM") fail(`a machine that came whole but wrong: answer ${r}, ` + JSON.stringify(info.slots[2]));
const newer = parseFile(file(twin(16, 2))), unknown = parseFile(file(twin(999)));
if (!/newer FM-ARCAVE/.test(fits(newer, info, false))) fail("fits(): a machine for a newer firmware");
r = await settle(link.load(2, newer), "a machine for a newer firmware");
if (r !== 4 || link.info.slots[2].st !== ST.OTHER || link.info.slots[2].title !== "NIGHT BLOOM") fail("a machine for a newer firmware: answer " + r);
r = await settle(link.load(2, unknown), "a machine this build does not have");
if (r !== 4 || link.info.slots[2].id !== 999 || link.info.slots[2].title !== "NIGHT BLOOM") fail("a machine this build does not have: answer " + r);
ok("a piece damaged on the way: sent again; a machine with a byte wrong, one for a newer firmware, one this build has not: refused, the ledge keeps the firmware's own");

r = await settle(link.choose(2, 0), "choosing");
if (r !== 0 || link.info.slots[2].title !== "CRYSTAL ROAD") fail("the third ledge was not given CRYSTAL ROAD");
r = await settle(link.choose(1, 2), "choosing");
if (r !== 0 || link.info.slots[1].title !== "NIGHT BLOOM" || link.info.slots[1].loaded || link.info.slots[1].st !== ST.OK) fail("the second ledge was not given NIGHT BLOOM, its loaded machine kept in the slot");
r = await settle(link.choose(1, 3), "the loaded machine again");
if (r !== 0 || link.info.slots[1].title !== "GLOW CAPS" || link.info.slots[1].own !== 2) fail("the loaded machine was not put back on the second ledge");
r = await settle(link.empty(0), "emptying");
if (r !== 0 || link.info.slots[0].st !== ST.EMPTY || link.info.slots[0].title !== "CRYSTAL ROAD") fail("the first slot was not emptied");
if (await settle(link.choose(0, 3), "a loaded machine where there is none") !== 1) fail("an empty slot's machine was put on its ledge");
shut();
const H = await fm(storeOf(F), bankOf(F));         // (off and on: the save and the flash are what is left)
if (ledge(H, 1).st !== ST.OK || !ledge(H, 1).loaded || ledge(H, 1).own !== 2 || ledge(H, 2).own !== 0 || ledge(H, 0).st !== ST.EMPTY || best(storeOf(H)) !== score)
  fail("off and on again, the ledges are not as they were left: " + JSON.stringify([0, 1, 2].map((i) => ledge(H, i))));
ok("the ledges' own machines chosen, the loaded one put back, a slot emptied; all of it still so after the FM-1 is off and on");

// ---------------------------------------------------------------- the music room's four places
const inst = (id, api = 1, n = 40, name = "THE SPRING") => { const b = twin(id, api, n, name); new DataView(b.buffer).setUint32(0, MAGIC_INST, true); return b; };
const spring = parseFile(file(inst(16)));
if (spring.kind !== "instrument" || caps.kind !== "machine" || spring.id !== 16) fail("the kind of a file: " + spring.kind + ", " + caps.kind);
let realI = null;
if (existsSync("build/machines/spring.minst")) {
  realI = parseFile(readFileSync("build/machines/spring.minst"));
  if (realI.code) {
    const a = new DataView(realI.img.buffer), b = new DataView(forSlot(realI, 4, SLOT).buffer);   // (bank slot 4: THE GEODE's place)
    if (realI.kind !== "instrument" || realI.id !== 16) fail("the built THE SPRING: " + realI.kind + " number " + realI.id);
    for (const w of realI.rel) {
      const x = a.getUint32(w, true);
      if (x < BASE || x >= BASE + realI.img.length || b.getUint32(w, true) !== x + 4 * SLOT) fail("the built THE SPRING: the address at " + w + " is not one inside it");
    }
    for (const w of [20, 24, 28, 32, 36, 40, 44]) if (!realI.rel.includes(w)) fail("the built THE SPRING: its header's address at " + w + " is not listed");
  }
}
const place = (f, j) => { const v = f.ex.web_place(j); return { loaded: !!(v & 0x80), st: v >> 8 & 255, id: v >> 16 & 0x7fff, on: v >>> 31 }; };
const rk = (f, j, i = 0) => { const v = f.ex.web_room_k(j, i); return { k: v & 255, algo: v >> 8 & 255, nkind: v >> 16 & 255, pulse: v >>> 24 }; };
const tap = (f, b) => { f.ex.web_buttons(1 << b); run(60); f.ex.web_buttons(0); run(120); };
const toRoom = (f) => { P = port(f); f.ex.web_to_room(); run(1500); };

// THE GEODE as the player has it: KNOB 1 turned
toRoom(F); F.ex.web_room_open(1); run(300);
const g0 = rk(F, 1).k;
F.ex.web_enc(2, g0 >= 6 ? -2 : 2); run(200);
const gk = rk(F, 1).k;
if (rk(F, 1).nkind !== 5 || gk === g0) fail("THE GEODE in its place, KNOB 1 turned: " + JSON.stringify(rk(F, 1)));
F.ex.web_save_now();
info = await open(F);
if (info.nplace !== 4 || info.iapi !== 1 || info.places.map((s) => s.title).join() !== "ROOTS,THE GEODE,MUSHROOM RACK,CRYSTAL SEAM" || info.places.some((s) => s.loaded || s.st !== ST.EMPTY))
  fail("a new FM-1's music room: " + JSON.stringify(info.places));
if (fits(spring, info, false) || !/no code/.test(fits(spring, info, true))) fail("fits(): the browser game's copy of an instrument");
r = await settle(link.loadPlace(1, spring), "loading THE SPRING");
if (r !== 0 || !link.info.places[1].loaded || link.info.places[1].st !== ST.OK || link.info.places[1].title !== "THE SPRING" || link.info.places[0].title !== "ROOTS" || link.info.slots[1].title !== "GLOW CAPS")
  fail(`THE SPRING loaded into THE GEODE's place: answer ${r}, ` + JSON.stringify(link.info.places[1]));
shut();
if (!place(F, 1).on || place(F, 1).id !== 16 || place(F, 0).on) fail("after the page let go, the room's places are not as it left them: " + JSON.stringify(place(F, 1)));
ok("the music room's places read: ROOTS, THE GEODE, MUSHROOM RACK, CRYSTAL SEAM; THE SPRING loaded into THE GEODE's place" + (realI && realI.code ? `; the built THE SPRING: ${realI.img.length} bytes, ${realI.rel.length} addresses, each inside it` : ""));

// played: in the room it runs by itself (its vents let bubbles go, they pop, a sound), a knob is its own law, REC takes its hits
toRoom(F); F.ex.web_room_open(1); run(300);
if (rk(F, 1).nkind !== 6 || rk(F, 1).k !== 4) fail("THE SPRING as new in its place: " + JSON.stringify(rk(F, 1)));
let loud = 0;
for (let t = 0; t < 6000; t += 50) { run(50); loud = Math.max(loud, rk(F, 1).pulse); }
if (!loud) fail("THE SPRING made no sound in six seconds");
F.ex.web_enc(2, 2); run(200);
if (rk(F, 1).k !== 6) fail("KNOB 1 did not turn THE SPRING's heat");
tap(F, 13); run(5000); tap(F, 13);
const hits = F.ex.web_room_loop(1);
if (!hits) fail("REC took none of THE SPRING's hits onto the loop");
F.ex.web_save_now();
ok(`THE SPRING played in the room: its bubbles pop and sound, KNOB 1 is its heat, ${hits} of its hits on the loop`);

// the save where the instrument is not: THE GEODE as it was left; and back where it is: THE SPRING as it was left
{
  const G = await fm(storeOf(F), null);
  if (place(G, 1).on || !place(G, 1).loaded || place(G, 1).st !== ST.EMPTY) fail("the save's places without its instrument");
  toRoom(G);
  if (rk(G, 1).nkind !== 5 || rk(G, 1).k !== gk || G.ex.web_room_loop(1)) fail("where THE SPRING is not, THE GEODE is not as it was left: " + JSON.stringify(rk(G, 1)) + ", loop " + G.ex.web_room_loop(1));
  G.ex.web_save_now();
  const K = await fm(storeOf(G), bankOf(F));
  toRoom(K);
  if (!place(K, 1).on || rk(K, 1).nkind !== 6 || rk(K, 1).k !== 6) fail("back where THE SPRING is, it is not as it was left: " + JSON.stringify(rk(K, 1)));
}
ok("the same save where the instrument is not: THE GEODE with its laws as they were left; back again: THE SPRING with its own");

info = await open(F);
r = await settle(link.choosePlace(1, false), "the firmware's own instrument");
if (r !== 0 || link.info.places[1].title !== "THE GEODE" || link.info.places[1].loaded || link.info.places[1].st !== ST.OK) fail("THE GEODE was not put back in its place, the loaded instrument kept in the slot");
r = await settle(link.choosePlace(1, true), "the loaded instrument again");
if (r !== 0 || link.info.places[1].title !== "THE SPRING") fail("THE SPRING was not put back");
r = await settle(link.loadPlace(2, caps), "a machine sent to a place of the room");
if (r !== 2 || link.info.places[2].title !== "MUSHROOM RACK" || link.info.places[2].loaded) fail("a machine was taken for an instrument: answer " + r);
const newerI = parseFile(file(inst(16, 2)));
if (!/newer FM-ARCAVE/.test(fits(newerI, link.info, false))) fail("fits(): an instrument for a newer firmware");
r = await settle(link.loadPlace(2, newerI), "an instrument for a newer firmware");
if (r !== 4 || link.info.places[2].st !== ST.OTHER || link.info.places[2].title !== "MUSHROOM RACK") fail("an instrument for a newer firmware: answer " + r);
r = await settle(link.loadPlace(2, parseFile(file(inst(999)))), "an instrument this build does not have");
if (r !== 4 || link.info.places[2].title !== "MUSHROOM RACK") fail("an instrument this build does not have: answer " + r);
if (await settle(link.choosePlace(0, true), "a loaded instrument where there is none") !== 1) fail("an empty slot's instrument was put in its place");
r = await settle(link.emptyPlace(1), "emptying a place's slot");
if (r !== 0 || link.info.places[1].st !== ST.EMPTY || link.info.places[1].title !== "THE GEODE") fail("THE GEODE's place was not emptied");
shut();
toRoom(F);
if (place(F, 1).on || rk(F, 1).nkind !== 5 || rk(F, 1).k !== gk) fail("with its slot emptied, THE GEODE is not as it was left: " + JSON.stringify(rk(F, 1)));
ok("the firmware's own instrument chosen, the loaded one put back; a machine sent to a place, an instrument for a newer firmware, one this build has not: refused; the slot emptied, THE GEODE as it was left");

// ---------------------------------------------------------------- the browser game's own bank
const mem = new Map(), storage = { getItem: (k) => (mem.has(k) ? mem.get(k) : null), setItem: (k, v) => mem.set(k, String(v)) };
const B = new BrowserBank(WASM, storage);
info = (await B.open()).info;
if (info.slots.some((s) => s.loaded) || fits(caps, info, false)) fail("the browser game's ledges, new");
r = await B.load(2, caps);
if (r !== 0 || B.info.slots[2].title !== "GLOW CAPS" || !mem.has(SAVE_KEY) || JSON.parse(mem.get(BANK_KEY))[2] === "") fail("GLOW CAPS into the browser game's third ledge: " + r);
await B.choose(0, 1);
const J = await fm(unb64(mem.get(SAVE_KEY)), readBank(storage, BANK_BYTES));   // (the PLAY page, starting from what the browser kept)
if (!ledge(J, 2).loaded || ledge(J, 2).st !== ST.OK || ledge(J, 0).own !== 1) fail("the PLAY page does not find the browser game's ledges as the page left them");
J.ex.web_to_ledge(2);
if ((J.ex.web_mach() & 15) !== 3) fail("the browser game's third ledge is not GLOW CAPS");
await B.empty(2);
if (JSON.parse(mem.get(BANK_KEY))[2] !== "" || B.info.slots[2].title !== "NIGHT BLOOM") fail("the browser game's slot was not emptied");
r = await B.loadPlace(3, spring);
if (r !== 0 || B.info.places[3].title !== "THE SPRING" || JSON.parse(mem.get(BANK_KEY))[6] === "") fail("THE SPRING into the browser game's fourth place: " + r);
const L = await fm(unb64(mem.get(SAVE_KEY)), readBank(storage, BANK_BYTES));
if (!place(L, 3).on || place(L, 2).on) fail("the PLAY page does not find the browser game's instrument in its place");
toRoom(L);
if (rk(L, 3).nkind !== 6) fail("the browser game's fourth place is not THE SPRING");
await B.choosePlace(3, false);
if (B.info.places[3].title !== "CRYSTAL SEAM" || B.info.places[3].st !== ST.OK) fail("the browser game's place was not given its own instrument back");
ok("the browser game's own ledges and places: a machine and an instrument loaded and kept in the browser's storage, found by the PLAY page, put away again");
console.log("machines_test: passed");
