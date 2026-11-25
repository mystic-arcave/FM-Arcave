// SPDX-License-Identifier: GPL-3.0-only
// The save, backed up (web/emu/saves.js, the SAVES page's own module) without an FM-1: a copy of the browser build plays
// the FM-1's part, over a made-up Web MIDI port, on one clock (as tests/remote_test.mjs).
//   node tests/save_test.mjs            (after web/emu/build.sh)
// What the game tells of a save; an older version's save brought up to date; a backup as a file and back, a damaged
// file refused; the list kept in the browser (the same save not twice, the automatic copies thinned); an FM-1's save
// read whole and the FM-1 itself again; a save put back on it, an older one brought up to date on the way, one it
// cannot take told apart beforehand and refused by the FM-1 itself; no FM-1 at all; and the FM-1's own spare copy:
// made at power-on, and what it starts from when its save is gone.
import { readFileSync } from "node:fs";
import { Backups, Game, Fm1Save, fit, toFile, fromFile, fileName, looksLikeSave, crc32, AUTO_KEEP, played } from "../web/emu/saves.js";
const fail = (m) => { console.error("FAIL", m); process.exit(1); };
const ok = (m) => console.log("ok  ", m);
const WASM = readFileSync("build/emu/fm-arcave.wasm");
const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);

async function fm(store, spare) {
  const { instance } = await WebAssembly.instantiate(WASM, {});
  const ex = instance.exports;
  if (ex._initialize) ex._initialize();
  if (store) { new Uint8Array(ex.memory.buffer, ex.web_store(), store.length).set(store); ex.web_store_set(store.length); }
  if (spare) { new Uint8Array(ex.memory.buffer, ex.web_spare(), spare.length).set(spare); ex.web_spare_set(spare.length); }
  ex.web_boot();
  return { ex };
}
const storeOf = (f) => new Uint8Array(f.ex.memory.buffer, f.ex.web_store(), f.ex.web_store_len()).slice();
const spareOf = (f) => new Uint8Array(f.ex.memory.buffer, f.ex.web_spare(), f.ex.web_spare_len()).slice();
const lang = (f) => f.ex.web_scene() >> 16 & 255, story = (f) => f.ex.web_scene() >> 8 & 255;
const remoteOn = (f) => f.ex.web_remote() & 1;
// a save of 0.19 (192 bytes): the language, how far the story is
const seed = (lg, st) => { const b = new Uint8Array(192), v = new DataView(b.buffer); v.setUint32(0, 0x41434156, true); b[4] = lg; b[6] = st; b[10] = 90; b[11] = 2; return b; };

// ---------------------------------------------------------------- the game as the judge of a save
const game = new Game(WASM);
const size = await game.saveSize();
const old = seed(0, 4), S1 = await game.upgrade(old), S2 = await game.upgrade(seed(1, 2));
if (!S1 || S1.length !== size || !S2 || S2.length !== size) fail("an older save was not brought up to this version's " + size + " bytes");
if (!same(await game.upgrade(S1), S1)) fail("a save of this version was changed by upgrade()");
const a0 = await game.about(old), a1 = await game.about(S1), a2 = await game.about(S2);
if (!a0.ok || !a0.older || a0.story !== 4 || !a1.ok || a1.older || a1.story !== 4 || a1.lang !== 0 || !a2.ok || a2.story !== 2 || a2.lang !== 1)
  fail("about(): " + JSON.stringify([a0, a1, a2]));
const junk = new Uint8Array(size).fill(7), long = new Uint8Array(size + 8); long.set(S1);
if ((await game.about(junk)).ok || (await game.about(new Uint8Array(0))).ok || looksLikeSave(junk)) fail("rubbish passed for a save");
if (!looksLikeSave(long) || !(await game.about(long)).newer || await game.upgrade(long)) fail("a save longer than this version's was not told apart");
const odd = S1.slice(0, size - 5);                 // (a length no version ever wrote)
if ((await game.about(odd)).ok || await game.upgrade(odd)) fail("a save of a length no version wrote was read");
ok(`what a save holds, read by the game: a 0.19 save of 192 bytes brought up to this version's ${size}; rubbish, a newer one and an odd length told apart`);

// ---------------------------------------------------------------- a backup as a file
let clock = Date.UTC(2026, 10, 25, 9, 30);
const mem = new Map(), storage = { getItem: (k) => (mem.has(k) ? mem.get(k) : null), setItem: (k, v) => mem.set(k, String(v)) };
const list = new Backups(storage, () => clock);
const first = list.add(S1, { from: "fm1", about: { ...a1, name: "Mossy Rock" }, game: "0.28", note: "before the trip" });
if (!first.fresh || list.list().length !== 1) fail("a backup was not kept");
const text = toFile(first.b), back = fromFile(new TextEncoder().encode(text));
if (!same(back.bytes, S1) || back.made !== clock || back.note !== "before the trip" || back.game !== "0.28") fail("a backup did not come back from its file as it went: " + JSON.stringify({ ...back, bytes: back.bytes.length }));
if (!/^fm-arcave-save-mossy-rock-2026-11-25-\d{4}\.json$/.test(fileName(first.b))) fail("the file's name: " + fileName(first.b));
if (!same(fromFile(S2).bytes, S2)) fail("a bare save was not read as a file");
const o = JSON.parse(text);
const refused = (what, t) => { try { fromFile(new TextEncoder().encode(t)); } catch (e) { if (/damaged|not an FM-ARCAVE|newer/.test(e.message)) return; fail(what + ": " + e.message); } fail(what + " was read"); };
refused("a file with a byte changed", JSON.stringify({ ...o, data: o.data.slice(0, 40) + (o.data[40] === "A" ? "B" : "A") + o.data.slice(41) }));
refused("a file cut short", text.slice(0, text.length / 2));
refused("a file of something else", JSON.stringify({ hello: 1 }));
refused("a file of a format to come", JSON.stringify({ ...o, v: 2 }));
refused("a file whose checksum is another's", JSON.stringify({ ...o, crc32: crc32(S2).toString(16).padStart(8, "0") }));
ok("a backup as a file and back; a changed byte, a cut file, another format: refused");

// ---------------------------------------------------------------- the list kept in the browser
if (list.add(S1, { from: "fm1" }).fresh) fail("the same save from the same place was kept twice");
clock += 60000;
if (!list.add(S1, { from: "file" }).fresh) fail("the same save opened from a file was not kept beside the FM-1's");
if (list.add(S1, { from: "fm1", auto: "an FM-1 joined PLAY" }).fresh) fail("an automatic copy of a save already in the list was kept");
for (let i = 0; i < AUTO_KEEP + 5; i++) {
  clock += 60000;
  const s = S2.slice(); s[size - 1] = i; s[size - 2] = 200;   // (thirteen saves, each its own)
  if (!list.add(s, { from: "fm1", auto: "an FM-1 joined PLAY" }).fresh) fail("an automatic copy was not kept");
}
const l = list.list(), autos = l.filter((b) => b.auto);
if (autos.length !== AUTO_KEEP || l.length !== AUTO_KEEP + 2 || autos[0].made !== clock || l.some((b, i) => i && b.made > l[i - 1].made))
  fail(`the list: ${l.length} kept, ${autos.length} automatic`);
list.note(first.b.id, "x".repeat(100)); list.remove(l[l.length - 2].id);
if (list.get(first.b.id).note.length !== 60 || list.list().length !== AUTO_KEEP + 1 || !same(list.bytes(first.b.id), S1)) fail("a note, a delete");
mem.set("fm-arcave-backups", "{not json");
if (list.list().length !== 0 || !list.add(S1, { from: "fm1" }).fresh) fail("a spoiled list did not start afresh");
if (played(3 * 3600 + 12 * 60 + 5) !== "3 H 12 MIN" || played(40) !== "UNDER A MINUTE") fail("played()");
ok(`the list: the same save not twice, the newest ${AUTO_KEEP} automatic copies kept, those made by hand left alone, a spoiled list survived`);

// ---------------------------------------------------------------- an FM-1 on a made-up MIDI port
let now = 0;
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
let P = null, link = null;
function run(ms) {                                 // a millisecond at a time: the FM-1, the port, the page's 20 ms tick
  for (let t = 0; t < ms; t++) {
    now++;
    if (P && P.F) {
      while (P.toFm1.length && P.toFm1[0][0] <= now) { const [, st, d1, d2] = P.toFm1.shift(); P.F.ex.web_midi(st, d1, d2); }
      P.F.ex.web_render(44);
      if (now % 10 === 0) P.F.ex.web_render(1);
      for (let v; (v = P.F.ex.web_midi_out()) >= 0;) P.input.onmidimessage?.({ data: [v & 255, v >> 8 & 127, v >> 16 & 127], target: P.input });
    }
    if (link && now % 20 === 0) link.tick();
  }
}
async function settle(p, what, ms = 40000) {
  let done = false, val, err;
  p.then((v) => { done = true; val = v; }, (e) => { done = true; err = e; });
  for (let t = 0; t < ms && !done; t += 5) { run(5); await null; await null; await null; }
  if (!done) fail(what + ": nothing after " + ms + " ms");
  if (err) throw err;
  return val;
}
const open = (F) => { P = port(F); link = new Fm1Save(P.access, { now: () => now }); return link; };

const F = await fm(S1);
run(1500);
if (lang(F) !== 0 || story(F) !== 4 || remoteOn(F)) fail("the FM-1 did not start from its save");
let f = open(F);
const got = await settle(f.read(), "reading the FM-1's save");
if (!same(got, storeOf(F)) || got.length !== size || !remoteOn(F)) fail("the FM-1's save did not come over whole");
const ag = await game.about(got);
if (!ag.ok || ag.story !== 4 || ag.lang !== 0) fail("the save read from the FM-1: " + JSON.stringify(ag));
f.close(); run(400);
if (remoteOn(F) || !same(storeOf(F), got)) fail("after the backup the FM-1 is not itself again, its save untouched");
ok(`BACK UP MY FM-1: its save of ${got.length} bytes read whole in ${now - 1500} ms, the FM-1 its own game again, nothing on it changed`);

// a save put back: read what is there (to keep it first), then send, then goodbye
f = open(F);
const cur = await settle(f.read(), "reading before a put-back");
let r = await fit(S2, cur.length, game);
if (r.why || !same(r.bytes, S2)) fail("fit(): a save of the FM-1's own size is sent as it is");
let how = await settle(f.write(r.bytes), "putting a save back");
run(600);
if (how !== "kept" || !same(storeOf(F), S2) || remoteOn(F) || lang(F) !== 1 || story(F) !== 2) fail(`a save put back: ${how}, language ${lang(F)}, story ${story(F)}`);
ok("PUT BACK: another save kept by the FM-1, which starts again from it (its language and its story)");

f = open(F);
await settle(f.read(), "reading before an older save");
r = await fit(seed(2, 4), size, game);
if (r.why || !r.upgraded || r.bytes.length !== size) fail("fit(): an older save was not brought up to date: " + JSON.stringify(r.why));
how = await settle(f.write(r.bytes), "putting an older save back");
run(600);
if (how !== "kept" || lang(F) !== 2 || story(F) !== 4 || storeOf(F).length !== size) fail(`an older save put back: ${how}, language ${lang(F)}, story ${story(F)}`);
ok("PUT BACK, a backup from 0.19: brought up to this version on the way, kept");

if ((await fit(long, size, game)).why !== "update" || (await fit(S1, size - 100, game)).why !== "update" || (await fit(old, size - 100, game)).why !== "update"
  || (await fit(old, size + 100, game)).why !== "page" || (await fit(junk, size, game)).why !== "damaged" || (await fit(odd, size, game)).why !== "damaged")
  fail("fit(): the saves an FM-1 cannot take are not told apart");
f = open(F);
const before = await settle(f.read(), "reading before a save of the wrong size");
how = await settle(f.write(S1.slice(0, size - 8)), "a save of the wrong size");
run(600);
if (how !== "refused" || !same(storeOf(F), before) || remoteOn(F) || lang(F) !== 2) fail(`a save of the wrong size: ${how}`);
ok("a save the FM-1 cannot take: told beforehand (newer than the FM-1, an FM-1 newer than the page, damaged), and refused by the FM-1 itself with its own save untouched");

P = port(null); link = new Fm1Save(P.access, { now: () => now });
const t0 = now;
let said = "";
try { await settle(link.read(), "no FM-1"); } catch (e) { said = e.message; }
if (said !== "absent" || now - t0 > 5000) fail(`no FM-1 at all: "${said}" after ${now - t0} ms`);
link = null;
ok(`no FM-1 at all: said so after ${now - t0} ms`);

// ---------------------------------------------------------------- the FM-1's own spare copy
if (!same(spareOf(F), storeOf(F))) fail("the FM-1 has no spare copy of the save it started from");
const G = await fm(null, S2);                      // (the save gone, cut short by the power: only the spare is whole)
P = port(G); run(100);
if (lang(G) !== 1 || story(G) !== 2 || G.ex.web_about(5) !== 1) fail(`with its save gone the FM-1 did not start from its spare: language ${lang(G)}, story ${story(G)}`);
run(2500);
const again = storeOf(G);
if (again.length !== size || (await game.about(again)).story !== 2) fail("the FM-1 did not write its save again from the spare");
const H = await fm(null, null);
if (story(H) !== 0 || H.ex.web_about(5) || H.ex.web_about(0)) fail("with no save and no spare the FM-1 did not start a new game");
const J = await fm(S1, S2);
if (lang(J) !== 0 || J.ex.web_about(5) || !same(spareOf(J), S1)) fail("a whole save did not win over an older spare, or the spare was not brought up to it");
ok("the FM-1's spare copy: made from the save it starts from, started from when the save is gone (and the save written again), never in a whole save's way");
console.log("save_test: passed");
