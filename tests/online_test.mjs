// SPDX-License-Identifier: GPL-3.0-only
// UNDERSONG online: copies of the browser build that meet only through a lobby server (server/node.mjs), each behind
// what the Link page's ONLINE tab runs (web/link/undersong.js, web/link/lobby.js), over real WebSockets with a delay
// put on every message, as between two houses. Unlike tests/duo_test.mjs this runs against the clock, since the
// network does: SPEED times faster than real time (2 by default), DELAY ms each way on top (40 by default, so 80 ms
// of the game's time), with some jitter that never reorders.
//   node tests/online_test.mjs              (after web/emu/build.sh, which also makes build/emu-catch)
//   SERVER=wss://fm-arcave-lobby.mystic-arcave.workers.dev DELAY=0 node tests/online_test.mjs    (the live one)
// One makes a lobby, a second joins it by its code; a wrong code and a fifth computer are refused. Both roll to the
// well, find each other through the lobby, and play TWIN ROAD and NOTE RALLY (whose note flies to the other side on
// a beat named in the message): both must agree on the score. The second one's connection breaks in the menu: it
// must join again by itself and keep its place. A third joins, the three play ECHO STEPS for three, then the third
// leaves the lobby in the middle of a game and the other two are stopped with the error that says who.
import { readFileSync } from "node:fs";
import { startServer } from "../server/node.mjs";
import { Gather } from "../web/link/undersong.js";
import { LobbyClient } from "../web/link/lobby.js";
const SPEED = +process.env.SPEED || 2, DELAY = process.env.DELAY != null ? +process.env.DELAY : 40, JITTER = DELAY / 4;
const fail = (m) => { console.error("FAIL", m); process.exit(1); };
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const save = (prog) => {                                    // the jukebox mended and `prog` chapters cleared (tests/duo_test.mjs)
  const b = new Uint8Array(192), v = new DataView(b.buffer);
  v.setUint32(0, 0x41434156, true); b[6] = 4; b[10] = 90; b[11] = 2; b[190] = prog;
  return b;
};
// SERVER=wss://... runs it against a deployed lobby server instead (the Cloudflare one: wss://fm-arcave-lobby.mystic-arcave.workers.dev)
const srv = process.env.SERVER ? null : await startServer(0, "127.0.0.1"), URL = process.env.SERVER || "ws://127.0.0.1:" + srv.port;
const NET = srv ? 3000 : 16000;                             // how long a lobby step may take: far away, a lobby's first wake is slower
const players = [];
let sent = 0, got = 0, playing = true;

// one computer: an FM-1 (the build whose notes play themselves) and the Online page's bridge to the lobby
async function computer(name, prog) {
  const { instance } = await WebAssembly.instantiate(readFileSync("build/emu-catch/fm-arcave.wasm"), {});
  const ex = instance.exports;
  if (ex._initialize) ex._initialize();
  const s = save(prog);
  new Uint8Array(ex.memory.buffer, ex.web_store(), s.length).set(s); ex.web_store_set(s.length);
  ex.web_boot();
  const p = { name, ex, g: new Gather(), inbox: [], last: 0, on: true, states: [] };
  p.lobby = new LobbyClient(URL, {
    state: (st, why) => p.states.push(st + (why ? ":" + why : "")),
    message: (f, d) => {                                    // in order, late by DELAY (and a little jitter)
      const at = Math.max(p.last, performance.now() + DELAY + Math.random() * JITTER); p.last = at;
      p.inbox.push([at, d]); got++;
    },
  });
  p.lobby.info = () => ({ kind: "emu", well: (ex.web_scene() & 255) === 5 });
  players.push(p);
  return p;
}
// the emulators, against the clock: each renders what is due, its messages go out through its Gather, as the page's do
let emuMs = 0;
const t0 = performance.now();
function step() {
  const due = (performance.now() - t0) * SPEED;
  while (emuMs < due) {
    for (const p of players) {
      if (!p.on) continue;
      while (p.inbox.length && p.inbox[0][0] <= performance.now()) { const [, d] = p.inbox.shift(); for (let i = 0; i < d.length; i += 2) p.ex.web_midi(0xBF, d[i], d[i + 1]); }
      p.ex.web_render(128);
      for (let v; (v = p.ex.web_midi_out()) >= 0;) if ((v & 255) === 0xBF) { const d = p.g.take(v >> 8 & 127, v >> 16 & 127); if (d && p.lobby.send(d)) sent++; }
    }
    emuMs += 128 / 44.1;
  }
}
const ticker = setInterval(step, 4);
const run = async (ms) => { const end = emuMs + ms; while (emuMs < end) await wait(5); };
const st = (p) => { const d = p.ex.web_duo(); return { state: d & 15, seat: d >> 4 & 3, peer: d >> 6 & 1, game: d >> 8 & 15, prog: d >> 12 & 15, done: d >> 16 & 1, stars: d >> 17 & 3, np: d >> 20 & 3 }; };
const scene = (p) => p.ex.web_scene() & 255;
const tap = async (p, btn, ms = 120) => { p.ex.web_buttons(1 << btn); await run(60); p.ex.web_buttons(0); await run(ms); };
const until = async (what, cond, ms, who = players) => {
  for (let t = 0; t < ms; t += 100) { if (cond()) return; await run(100); }
  fail(what + " (" + who.map((p) => p.name + " " + JSON.stringify(st(p))).join(", ") + ")");
};
async function toWell(p) {                                  // PLAY past the title, roll right pressing PLAY until the well takes Rock
  for (let t = 0; t < 40000 && scene(p) !== 1; t += 500) { p.ex.web_buttons(1 << 12); await run(80); p.ex.web_buttons(0); await run(420); }
  if (scene(p) !== 1) fail(p.name + " never reached the cave");
  for (let t = 0; t < 30000 && scene(p) !== 5; t += 160) { p.ex.web_enc(2, 1); await run(80); p.ex.web_buttons(1 << 12); await run(40); p.ex.web_buttons(0); await run(40); }
  if (scene(p) !== 5) fail(p.name + " never found the well");
}
const ticks = (ps) => { const t = ps.map((p) => p.ex.web_tick()); return Math.max(...t) - Math.min(...t); };
// the delay one way, in the game's ticks (96 a beat), at the tempo playing: how far behind a follower may run
// (plus the network's own: half of each one's measured round trip to the server)
const lagTicks = (p) => Math.ceil((DELAY + JITTER + 10 + players.reduce((m, q) => Math.max(m, q.lobby.rtt), 0)) * SPEED * p.ex.web_bpm_q8() / 256 * 96 / 60000);
async function game(ps, g) {                                // from the list: line g, all ready, played out; all agree
  const [a] = ps;
  for (let i = 0; i < g; i++) { a.ex.web_enc(3, 1); await run(200); }
  await tap(a, 12, 500);
  await until("the card did not open on all", () => ps.every((p) => st(p).state === 2 && st(p).game === g), 4000);
  for (const p of ps) await tap(p, 12, 300);
  await until("the game did not start on all", () => ps.every((p) => st(p).state === 4), 15000);
  const apart = ticks(ps);
  await until("no result on all", () => ps.every((p) => st(p).done), 150000);
  await run(400);
  const sc = ps.map((p) => p.ex.web_duo_score(-1)), stars = ps.map((p) => st(p).stars);
  console.log(`  game ${g} for ${ps.length}: scores ${sc.join(", ")}, stars ${stars.join(", ")}, clocks ${apart} ticks apart at the start`);
  if (new Set(sc).size !== 1) fail("they disagree on the score");
  if (stars.some((s) => s < 3)) fail("too few stars for players who never miss: the delay cost notes");
  for (const p of ps) await tap(p, 12, 300);
  await until("not back at the list", () => ps.every((p) => st(p).state === 1), 4000);
  // the list takes PLAY only once each friend's "I am here" (every 300 ms, and then the delay) says they are at it too
  await run(300 + 2 * DELAY * SPEED + 300);
}

console.log(`lobby server on ${URL}; ${SPEED}x real time, ${DELAY} ms each way (${DELAY * SPEED} ms of the game's time)`);
// ---- the lobby
const A = await computer("A", 4), B = await computer("B", 4);
A.lobby.create("ben");
await until("A got no lobby", () => A.lobby.status === "in", NET, []);
const X = new LobbyClient(URL, { state: (s, w) => { X.last = s + ":" + w; } }); X.join("ZZZZZZ", "x");
B.lobby.join(A.lobby.code.toLowerCase().replace(/(...)/, "$1 "), "friend");   // typed as people type it
await until("B did not get in", () => B.lobby.status === "in" && A.lobby.members.length === 2, NET, []);
await until("a wrong code was not refused", () => X.last === "error:no-lobby", NET, []);
console.log(`lobby ${A.lobby.code}: ${A.lobby.members.map((m) => m.name).join(", ")}; a wrong code: refused`);

// ---- two, through the lobby
await toWell(A); await run(1500);
if (st(A).state !== 0 || st(A).peer) fail("A should be waiting alone");
await toWell(B);
await until("the two did not find each other", () => [A, B].every((p) => st(p).state === 1 && st(p).peer), 6000);
// (two numbers the same: each draws again, and the old one is still heard for 2.5 s, so for a moment the two may
// see the room differently; the seats must part once it has gone)
await until("both took the same seat", () => st(A).seat !== st(B).seat, 4000, [A, B]);
await run(3000);
const lag = lagTicks(A);
console.log(`found each other: seats ${st(A).seat}, ${st(B).seat}; clocks ${ticks([A, B])} ticks apart (the delay allows ${lag})`);
if (ticks([A, B]) > lag) fail("the follower's clock is further behind than the delay");
await until("the lobby does not show both in the well", () => A.lobby.members.length === 2 && A.lobby.members.every((m) => m.well), 5000, []);
await game([A, B], 1);                                     // TWIN ROAD
await game([A, B], 3);                                     // NOTE RALLY

// ---- B's connection breaks in the menu: it joins again by itself, in its old place
const bId = B.lobby.you;
if (srv) srv.lobbies.get(A.lobby.code).members.find((m) => m.id === bId).sock.kill();   // a connection cut without a word
else B.lobby.ws.close();                                    // (far away: the socket closes under the page)
await until("B did not notice", () => B.states.includes("again"), NET, []);
await until("B did not join again", () => B.lobby.status === "in" && A.lobby.members.length === 2, NET, []);
if (B.lobby.you !== bId) fail("B came back under another number");
await run(2000);
if (st(A).state !== 1 || st(B).state !== 1) fail("the broken connection ended the game for them");
console.log(`B's connection broke and came back (${B.states.join(" > ")}): still at the list together`);

// ---- a third, a fourth, and a fifth that is turned away
const C = await computer("C", 4);
C.lobby.join(A.lobby.code, "third");
await until("C did not get in", () => C.lobby.status === "in", NET, []);
// (a lobby holds four computers, for the music room: a fourth with nothing playing is let in, a fifth is turned away)
const D = new LobbyClient(URL, { state: (s, w) => { D.last = s + ":" + w; } }); D.join(A.lobby.code, "fourth");
await until("a fourth computer was not let in", () => D.status === "in", NET, []);
const E = new LobbyClient(URL, { state: (s, w) => { E.last = s + ":" + w; } }); E.join(A.lobby.code, "fifth");
await until("a fifth computer was let in", () => E.last === "error:full", NET, []);
D.leave();
// C was switched on later than A, but rolls to the well exactly as A did: in lockstep it would draw A's number at the
// door, and draw again in step with it. People are never that exact; as in tests/duo_test.mjs, C waits a little first
await run(2300);
await toWell(C);
await until("the three did not find each other", () => [A, B, C].every((p) => st(p).state === 1 && st(p).np === 3), 6000);
if (new Set([A, B, C].map((p) => st(p).seat)).size !== 3) fail("two took the same seat");
await run(3000);
console.log(`three: seats ${[A, B, C].map((p) => st(p).seat).join(", ")}, clocks ${ticks([A, B, C])} ticks apart; a fifth: refused`);
await game([A, B, C], 0);                                  // ECHO STEPS for three

// ---- C leaves the lobby in the middle of a game
await tap(A, 12, 500);
await until("the card did not open on all three", () => [A, B, C].every((p) => st(p).state === 2), 4000);
for (const p of [A, B, C]) await tap(p, 12, 300);
await until("the last game did not start", () => [A, B, C].every((p) => st(p).state === 4), 15000);
await run(3000);
C.lobby.leave(); C.on = false;
await until("the two left were not stopped with the error", () => st(A).state === 7 && st(B).state === 7, 6000, [A, B]);
await until("C is still in the lobby", () => A.lobby.members.length === 2, NET, []);
console.log("C left in the middle of a game: the two were stopped with the error");

clearInterval(ticker);
for (const p of players) p.lobby.leave();
if (srv) await srv.close();
console.log(`ok (${sent} messages sent, ${got} received)`);
process.exit(0);
