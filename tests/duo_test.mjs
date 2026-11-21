// SPDX-License-Identifier: GPL-3.0-only
// UNDERSONG, the game for two, played by two copies of the browser build wired to each other as the Link page wires
// two FM-1s: every control change on channel 16 that one sends, the other gets. Nothing else is shared.
//   node tests/duo_test.mjs            (after web/emu/build.sh, and the same build with -DAC_TEST_CATCH=1 in build/emu-catch)
// Part 1, the real build: one FM-1 has been on for an hour and a quarter longer; both roll to the well, find each other, take
// seats, and play chapters 1 and 2 with keys pressed on every half beat in all four lanes. Both must end on the same
// score and the same chapter. Part 2, the build whose notes play themselves: chapters 3 and 4, the finale, ROCK-OFF,
// REC in the middle of a game, and one player going back up to the Arcave.
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
const fail = (m) => { console.error("FAIL", m); process.exit(1); };
const OUT = "build/host"; mkdirSync(OUT, { recursive: true });
const save = (prog) => {                                    // a save with the jukebox mended and `prog` chapters cleared
  const b = new Uint8Array(192), v = new DataView(b.buffer);
  v.setUint32(0, 0x41434156, true); b[6] = 4; b[10] = 90; b[11] = 2; b[190] = prog;
  return b;
};
async function fm(path, store, name) {
  const { instance } = await WebAssembly.instantiate(readFileSync(path), {});
  const ex = instance.exports;
  if (ex._initialize) ex._initialize();
  new Uint8Array(ex.memory.buffer, ex.web_store(), store.length).set(store); ex.web_store_set(store.length);
  ex.web_boot();
  return { ex, name, keysUp: 0, last8: -1 };
}
let A, B, mash = false, sent = 0;
const pump = (a, b) => { for (let v; (v = a.ex.web_midi_out()) >= 0;) if ((v & 255) === 0xBF) { b.ex.web_midi(v & 255, v >> 8 & 127, v >> 16 & 127); sent++; } };
function run(ms, only) {
  for (let i = 0; i < ms * 441 / 1280; i++) {
    for (const f of only ? [only] : [A, B]) {
      f.ex.web_render(128);
      if (mash && !only) {                                  // a key in each lane on every half beat, let go soon after
        const h = Math.floor((f.ex.web_tick() + 3) / 48);
        if (h !== f.last8) { f.last8 = h; f.ex.web_keys(1 << 1 | 1 << 9 | 1 << 16 | 1 << 24); f.keysUp = 12; }
        else if (f.keysUp && !--f.keysUp) f.ex.web_keys(0);
      }
    }
    if (!only) { pump(A, B); pump(B, A); }
  }
}
const tap = (f, btn, ms = 120) => { f.ex.web_buttons(1 << btn); run(60); f.ex.web_buttons(0); run(ms); };
const st = (f) => { const d = f.ex.web_duo(); return { state: d & 15, seat: d >> 4 & 1, peer: d >> 5 & 1, game: d >> 8 & 15, prog: d >> 12 & 15, done: d >> 16 & 1, stars: d >> 17 & 3 }; };
const scene = (f) => f.ex.web_scene() & 255;
let shots = [];
function shot(f, label) {
  const px = new Uint16Array(f.ex.memory.buffer, f.ex.web_screen(), 57600), o = Buffer.alloc(57600 * 3);
  for (let i = 0; i < 57600; i++) { const c = px[i]; o[i * 3] = (c >> 11) * 255 / 31; o[i * 3 + 1] = (c >> 5 & 63) * 255 / 63; o[i * 3 + 2] = (c & 31) * 255 / 31; }
  const name = `${OUT}/duotest-${String(shots.length).padStart(2, "0")}-${label}-${f.name}.ppm`;
  writeFileSync(name, Buffer.concat([Buffer.from("P6 240 240 255\n"), o])); shots.push(name);
}
const both = (label) => { shot(A, label); shot(B, label); };
const until = (what, cond, ms) => { for (let t = 0; t < ms; t += 100) { if (cond()) return; run(100); } fail(what + " (A " + JSON.stringify(st(A)) + ", B " + JSON.stringify(st(B)) + ")"); };
async function toWell(f) {                                  // PLAY past the title, then roll right, pressing PLAY, until the well takes Rock
  for (let t = 0; t < 40000 && scene(f) !== 1; t += 500) { f.ex.web_buttons(1 << 12); run(80, f); f.ex.web_buttons(0); run(420, f); }
  if (scene(f) !== 1) fail(f.name + " never reached the cave");
  for (let t = 0; t < 30000 && scene(f) !== 5; t += 160) { f.ex.web_enc(2, 1); run(80, f); f.ex.web_buttons(1 << 12); run(40, f); f.ex.web_buttons(0); run(40, f); }
  if (scene(f) !== 5) fail(f.name + " never found the well");
}
async function chapter(game, wantStars) {                   // from the menu: open what is selected, both ready, play it out
  tap(A, 12, 400);
  until("the card did not open on both", () => st(A).state === 2 && st(B).state === 2 && st(B).game === game, 3000);
  if (st(A).prog < 4 && game < 4) { shot(B, "story"); tap(A, 12); tap(B, 12); }   // the story's page first
  shot(A, "how"); tap(A, 12, 300); shot(B, "oneready"); tap(B, 12, 300);
  until("the count did not start on both", () => st(A).state >= 3 && st(B).state >= 3, 3000);
  shot(A, "count");
  until("the game did not start on both", () => st(A).state === 4 && st(B).state === 4, 12000);
  const d = Math.abs(A.ex.web_tick() - B.ex.web_tick());
  if (d > 3) fail("clocks " + d + " ticks apart in " + game);
  run(9000); both("play"); run(9000); both("play2");
  until("no result on both", () => st(A).done && st(B).done, 90000);
  run(300); both("result");
  const a = A.ex.web_duo_score(-1), b = B.ex.web_duo_score(-1), sa = st(A), sb = st(B);
  console.log(`  game ${game}: A ${a} (${sa.stars} stars, chapters ${sa.prog}), B ${b} (${sb.stars} stars, chapters ${sb.prog}), clocks ${d} ticks apart`);
  if (game !== 4 && a !== b) fail("the two disagree on the score");
  if (game === 4 && (A.ex.web_duo_score(0) !== B.ex.web_duo_score(0) || A.ex.web_duo_score(1) !== B.ex.web_duo_score(1))) fail("the two disagree on ROCK-OFF");
  if (wantStars && (sa.stars < wantStars || sb.stars < wantStars)) fail("too few stars");
  tap(A, 12, 300); tap(B, 12, 300);
  return [a, b];
}

// ---- part 1
A = await fm("build/emu/fm-arcave.wasm", save(0), "A"); B = await fm("build/emu/fm-arcave.wasm", save(0), "B");
run(75 * 60 * 1000, A);                                     // A has been on for an hour and a quarter
await toWell(A); run(1500, A); shot(A, "alone");
if (st(A).state !== 0 || st(A).peer) fail("A should be waiting alone");
await toWell(B);
until("the two did not find each other", () => st(A).state === 1 && st(B).state === 1 && st(A).peer && st(B).peer, 4000);
if (st(A).seat === st(B).seat) fail("both took the same seat");
run(2500);
console.log(`found each other: A seat ${st(A).seat}, B seat ${st(B).seat}; clocks ${A.ex.web_tick()} and ${B.ex.web_tick()} (they were an hour and a quarter apart)`);
if (Math.abs(A.ex.web_tick() - B.ex.web_tick()) > 3) fail("the clocks did not come together in whole");
both("menu");
B.ex.web_keys(1 << 20); run(200); shot(A, "friendkey"); B.ex.web_keys(0); run(100);
mash = true;
await chapter(0, 2); if (st(A).prog !== 1 || st(B).prog !== 1) fail("chapter 1 not kept on both");
await chapter(1, 2); if (st(A).prog !== 2 || st(B).prog !== 2) fail("chapter 2 not kept on both");
mash = false; both("menu2");

// ---- part 2
A = await fm("build/emu-catch/fm-arcave.wasm", save(2), "A"); B = await fm("build/emu-catch/fm-arcave.wasm", save(2), "B");
run(4000, A);
await toWell(A); await toWell(B);
until("the two did not find each other again", () => st(A).state === 1 && st(B).state === 1, 4000);
run(2500);
await chapter(2, 3); await chapter(3, 3);
until("no finale", () => st(A).state === 6 && st(B).state === 6, 3000);
both("finale"); run(1500); tap(A, 12, 300); tap(B, 12, 300);
if (st(A).prog !== 4 || st(B).prog !== 4) fail("the story is not finished on both");
both("games");
for (let i = 0; i < 4; i++) { B.ex.web_enc(3, 1); run(150); }   // B turns to ROCK-OFF: A's list follows
shot(A, "picked");
await chapter(4, 0);
tap(B, 12, 400);                                            // ROCK-OFF again, and REC in the middle of it
tap(A, 12, 300); tap(B, 12, 300);
until("the second ROCK-OFF did not start", () => st(A).state === 4 && st(B).state === 4, 15000);
run(3000); tap(B, 13, 400);
if (st(A).state !== 1 || st(B).state !== 1) fail("REC did not take both back to the list");
tap(B, 13, 3200);                                           // B goes back up to the Arcave
if (scene(B) !== 1) fail("B did not leave");
if (st(A).state !== 0) fail("A was not told its friend left");
shot(A, "left");
writeFileSync(`${OUT}/duotest-shots.txt`, shots.join("\n") + "\n");
console.log(`ok (${sent} messages passed, ${shots.length} pictures in ${OUT})`);
