// SPDX-License-Identifier: GPL-3.0-only
// PLAY WITH MY FM-1 (web/emu/remote.js, firmware/src/arcave/ac_remote.h) without an FM-1: one copy of the browser build plays
// the FM-1's part (it is the same code), another is the page's game, and the page's own remote.js is between them, over
// a made-up Web MIDI port that keeps the times messages are sent for. One clock for all of it.
//   node tests/remote_test.mjs            (after web/emu/build.sh)
// The FM-1's save comes over whole and the page's game starts from it; the FM-1's buttons, keys and knobs reach the
// page; the game's lights reach the FM-1; a save made in the page is kept on the FM-1; goodbye, and the FM-1 plays its
// own game from that save; then the page going quiet, a save of the wrong size, a damaged one; the pace both ways.
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { Fm1Remote, pack7 } from "../web/emu/remote.js";
const fail = (m) => { console.error("FAIL", m); process.exit(1); };
const ok = (m) => console.log("ok  ", m);
const OUT = "build/host"; mkdirSync(OUT, { recursive: true });
const WASM = readFileSync("build/emu/fm-arcave.wasm");

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
const lang = (f) => f.ex.web_scene() >> 16 & 255, scene = (f) => f.ex.web_scene() & 255, story = (f) => f.ex.web_scene() >> 8 & 255;
const remoteOn = (f) => f.ex.web_remote() & 1, remoteMsg = (f) => f.ex.web_remote() >> 1 & 7;
function shot(f, name) {
  const px = new Uint16Array(f.ex.memory.buffer, f.ex.web_screen(), 57600), o = Buffer.alloc(57600 * 3);
  for (let i = 0; i < 57600; i++) { const c = px[i]; o[i * 3] = (c >> 11) * 255 / 31; o[i * 3 + 1] = (c >> 5 & 63) * 255 / 63; o[i * 3 + 2] = (c & 31) * 255 / 31; }
  writeFileSync(`${OUT}/${name}.ppm`, Buffer.concat([Buffer.from("P6 240 240 255\n"), o]));
}

// the FM-1: a save from 0.19 with the jukebox mended (story 4), in English
const old = new Uint8Array(192), dv = new DataView(old.buffer);
dv.setUint32(0, 0x41434156, true); old[4] = 0; old[6] = 4; old[10] = 90; old[11] = 2;
const F = await fm(old);
let D = await fm(null);                            // the page's game, from this browser's (empty) save
let now = 0;
const toFm1 = [];                                  // [at, d1, d2], kept in the order sent
const perMs = new Map(), fm1PerMs = new Map();
const input = { name: "FM-1 MIDI", onmidimessage: null };
const output = { name: "FM-1 MIDI", send: (d, at) => {        // (as Web MIDI: at once, or at its time; in that order)
  const t = at === undefined ? now : at, i = toFm1.findIndex((m) => m[0] > t);
  toFm1.splice(i < 0 ? toFm1.length : i, 0, [t, d[0], d[1], d[2]]);
} };
const access = { inputs: new Map([["i", input]]), outputs: new Map([["o", output]]), onstatechange: null };
let saveIn = null, knobs = [], kept = [], states = [];
const page = {                                     // what web/emu/index.html does with the remote's news
  onState: (s, d) => states.push(d || s),
  onSave: (b) => { saveIn = b; },
  onInput: (b, k) => { D.ex.web_buttons(b); D.ex.web_keys(k); },
  onKnob: (id, n) => { knobs.push([id, n]); D.ex.web_enc(id, n); },
  onKept: (r) => kept.push(r),
};
let R = new Fm1Remote(access, { ...page, now: () => now });
let ticking = true, frameAt = 0, pageLen = 0, pageLights = true;
function run(ms) {                                 // a millisecond at a time: both FM-1s, the port, the page
  for (let t = 0; t < ms; t++) {
    now++;
    while (toFm1.length && toFm1[0][0] <= now) {
      const [, st, d1, d2] = toFm1.shift();
      F.ex.web_midi(st, d1, d2); if (d1 === 76) perMs.set(now, (perMs.get(now) || 0) + 1);
    }
    F.ex.web_render(44); D.ex.web_render(44);      // (a millisecond is 44.1 frames: near enough)
    if (now % 10 === 0) { F.ex.web_render(1); D.ex.web_render(1); }
    for (let v; (v = F.ex.web_midi_out()) >= 0;) {
      if ((v >> 8 & 127) === 76) fm1PerMs.set(now, (fm1PerMs.get(now) || 0) + 1);
      input.onmidimessage?.({ data: [v & 255, v >> 8 & 127, v >> 16 & 127], target: input });
    }
    while (D.ex.web_midi_out() >= 0);
    if (ticking && now % 50 === 0) R.tick();
    if (now - frameAt >= 33) {                     // the worklet's frame: the lights, and a save when one was made
      frameAt = now;
      if (pageLights) R.lights(leds(D));
      const g = D.ex.web_store_gen();
      if (g !== D.gen) { D.gen = g; const s = storeOf(D); if (R.state === "connected" && !R.leaving && s.length === pageLen) R.sendSave(s); }
    }
  }
}
const tap = (f, btn) => { f.ex.web_buttons(1 << btn); run(60); f.ex.web_buttons(0); run(240); };
const intoCave = (f, via) => { for (let t = 0; t < 30 && scene(f) !== 1; t++) { tap(via || f, 12); run(200); } return scene(f) === 1; };
const until = (what, cond, ms) => { for (let t = 0; t < ms; t += 10) { if (cond()) return; run(10); } fail(what); };

run(1000); intoCave(F);
if (scene(F) !== 1 || story(F) !== 4) fail(`the FM-1 should be in the cave with the jukebox mended (scene ${scene(F)}, story ${story(F)})`);
if (scene(D) !== 0) fail("the page's game should be at the title");

// 1. the FM-1 found, its save across
R.start();
until("the FM-1 never sent its save", () => saveIn, 3000);
const size = D.ex.web_save_size();
if (!remoteOn(F)) fail("the FM-1 is not in remote mode");
if (saveIn.length !== size) fail(`the save is ${saveIn.length} bytes, the page's game has ${size}`);
if (new DataView(saveIn.buffer).getUint32(0, true) !== 0x41434156) fail("the save does not start with ACAV");
const fsave = storeOf(F);
if (fsave.length !== size || fsave.some((b, i) => b !== saveIn[i])) fail("the save that came is not the FM-1's");
{   // (the FM-1's millisecond and this test's drift a little apart: its pace is taken over the whole save)
  const ms = [...fm1PerMs.keys()], n = [...fm1PerMs.values()].reduce((a, v) => a + v, 0), span = Math.max(...ms) - Math.min(...ms) + 1;
  if (n / span > 4.1) fail(`the FM-1 sent its save faster than 4 a millisecond: ${n} in ${span} ms`);
  fm1PerMs.clear();
}
ok(`the FM-1's save over (${size} bytes, ${pack7(saveIn).length + 6} messages); the FM-1 shows its own screen`);
run(400); shot(F, "remote-fm1-screen");

// 2. the page's game started again from it
D = await fm(saveIn); pageLen = saveIn.length;
run(1000);
if (!intoCave(D, F) || story(D) !== 4) fail(`PLAY on the FM-1 should take the page's game, from the FM-1's save, into the cave (scene ${scene(D)}, story ${story(D)})`);
if (scene(F) !== 1 || !remoteOn(F)) fail("the FM-1's own game moved, or it left remote mode");
ok("the page's game starts from the FM-1's save; PLAY on the FM-1 takes it into the cave, the jukebox mended");

// 3. the FM-1's panel plays the page's game; the lights come back
F.ex.web_buttons(1 << 1); run(60); F.ex.web_buttons(0); run(300);   // OCT+: the next language
if (lang(D) !== 1) fail("OCT+ on the FM-1 did not change the page's language: " + lang(D));
F.ex.web_keys(1 << 5); run(30);
if (R.keys !== 1 << 5) fail("a key held on the FM-1 is not held in the page: " + R.keys);
F.ex.web_keys(0); run(30);
if (R.keys !== 0) fail("a key let go on the FM-1 is still held in the page");
F.ex.web_enc(2, 11); F.ex.web_enc(5, -3); run(30);
const turned = (id) => knobs.filter((k) => k[0] === id).reduce((a, k) => a + k[1], 0);
if (turned(2) !== 11 || turned(5) !== -3) fail("knobs: " + JSON.stringify(knobs));
run(500);
if (leds(F).join() !== leds(D).join()) fail(`the FM-1's lights ${leds(F)} are not the page's ${leds(D)}`);
// every light at once, in a pattern: buttons lit (not PLAY) and PLAY breathing, keys lit and glowing, four buttons breathing
const want = [(0x3fff & ~(1 << 12) & ~0xf0) | 1 << 15, 0x5555555, 0x2aaaaaa, 0xf0];
pageLights = false; R.lights(want); run(200);
if (leds(F).join() !== want.join()) fail(`the FM-1's lights ${leds(F).map((v) => v.toString(16))} are not ${want.map((v) => v.toString(16))}`);
pageLights = true; run(200);
if (leds(F).join() !== leds(D).join()) fail("the FM-1's lights did not follow the page's back");
ok("buttons, keys and knobs from the FM-1 reach the page; every light the page's game sets lights the FM-1");

// 4. the page's save kept on the FM-1
until("the language change was never kept on the FM-1", () => kept.includes(1), 6000);
const dsave = storeOf(D), fnow = storeOf(F);
if (fnow.length !== dsave.length || fnow.some((b, i) => b !== dsave[i])) fail("the FM-1 does not hold the page's save");
if ([...perMs.values()].some((n) => n > 2)) fail("the page sent more than 2 of its save in a millisecond: " + Math.max(...perMs.values()));
ok("the page's save is kept on the FM-1, byte for byte (two messages a millisecond)");
run(200); shot(F, "remote-fm1-saved");

// 5. let go straight after playing (as the page's button does): the game saved at once, kept, then the goodbye. Without
// the wait, the goodbye cuts the save on its way and what was just played is lost
F.ex.web_buttons(1 << 1); run(60); F.ex.web_buttons(0); run(150);   // OCT+ again: a third language, not yet saved
if (lang(D) !== 2) fail("OCT+ on the FM-1 did not change the page's language again: " + lang(D));
D.ex.web_save_now(); D.gen = D.ex.web_store_gen();
const leaving = R.leave(storeOf(D));
until("letting go never finished", () => !R.leaving, 10000);
const how = await leaving;
if (how !== "kept") fail("letting go: the last save was " + how);
run(3500);
if (remoteOn(F)) fail("the FM-1 is still in remote mode after goodbye");
if (lang(F) !== 2 || story(F) !== 4 || scene(F) !== 0) fail(`the FM-1 should be at its title, in the language just chosen in the page (lang ${lang(F)}, story ${story(F)}, scene ${scene(F)})`);
if (!intoCave(F)) fail("the FM-1's own game does not go on from its title");
ok("let go straight after playing: the last save kept, then goodbye (its title, in the language just chosen; PLAY: the cave)");

// 6. the page goes quiet: the FM-1 is itself again by itself
saveIn = null; R = new Fm1Remote(access, { ...page, now: () => now }); R.start();
until("the FM-1 never answered again", () => saveIn, 3000);
ticking = false; run(3400);
if (remoteOn(F)) fail("the FM-1 stayed in remote mode with no hello for 3.4 s");
if (scene(F) !== 0) fail("the FM-1 should be back in its own game, at its title");
ok("no hello for three seconds: the FM-1 goes back to its own game");

// 7. a save of another size, and a damaged one
ticking = true; saveIn = null; kept = []; R = new Fm1Remote(access, { ...page, now: () => now }); R.start();
until("the FM-1 never answered a third time", () => saveIn, 3000);
R.sendSave(new Uint8Array(size - 8).fill(7)); run(2000);
if (!kept.includes(2) || remoteMsg(F) !== 2) fail("a save of the wrong size: " + kept + ", msg " + remoteMsg(F));
const before = F.ex.web_store_gen();
const bad = storeOf(D); let sum = 0; for (const b of bad) sum += b; sum += 1;
const raw = [[74, bad.length >> 7], [75, bad.length & 127], ...pack7(bad).map((v) => [76, v]), [77, sum >> 7 & 127], [77, sum & 127]];
raw.forEach(([c, v], i) => output.send([0xbf, c, v], now + 1 + i / 2));
run(2000);
if (!kept.includes(3) || F.ex.web_store_gen() !== before) fail("a damaged save: " + kept);
run(300); shot(F, "remote-fm1-update");
ok("a save of another size is refused (UPDATE THIS FM-1 on its screen); a damaged one is refused and not written");
R.stop(); run(500);
console.log("remote: all passed; the FM-1's screens in build/host/remote-fm1-*.ppm");
