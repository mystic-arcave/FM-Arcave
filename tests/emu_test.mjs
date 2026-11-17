// SPDX-License-Identifier: GPL-3.0-only
// The browser build, run in Node: it must load with no imports, draw the title, make sound, and take PLAY.
//   node tests/emu_test.mjs            (after web/emu/build.sh)
import { readFileSync } from "node:fs";
const { instance } = await WebAssembly.instantiate(readFileSync("build/emu/fm-arcave.wasm"), {});
const ex = instance.exports;
if (ex._initialize) ex._initialize();
ex.web_boot();
const run = (ms) => { let peak = 0; for (let i = 0; i < ms * 441 / 1280; i++) { ex.web_render(128); const l = new Float32Array(ex.memory.buffer, ex.web_out_l(), 128); for (const v of l) peak = Math.max(peak, Math.abs(v)); } return peak; };
const lit = () => new Uint16Array(ex.memory.buffer, ex.web_screen(), 57600).reduce((n, v) => n + (v ? 1 : 0), 0);
const fail = (m) => { console.error("FAIL", m); process.exit(1); };
let peak = run(4000);
if (lit() < 800) fail("the title drew " + lit() + " pixels");
if (peak < 0.01) fail("the title is silent");
console.log("title: " + lit() + " pixels lit, peak " + peak.toFixed(3));
ex.web_buttons(1 << 12); run(100); ex.web_buttons(0); run(3000);   // PLAY: on to the wide shot
if ((ex.web_scene() & 255) !== 0) fail("left the intro too early");
ex.web_buttons(1 << 12); run(100); ex.web_buttons(0); run(4500);   // PLAY: wake up Rock
ex.web_enc(2, 8); run(300); ex.web_enc(1, 8); run(300); ex.web_enc(6, 8); run(1500);
console.log("rolling: " + lit() + " pixels lit, clock " + (ex.web_clock_q8() >>> 8) + " ticks at " + (ex.web_bpm_q8() >> 8) + " BPM");
if ((ex.web_clock_q8() >>> 8) < 1000) fail("the clock is not running");
// ARCAVE LINK: a beacon from a hub pulls the clock to it
ex.web_buttons(1 << 7); run(100); ex.web_buttons(0); run(200);                        // GLO
ex.web_enc(0, 1); run(100); ex.web_enc(0, 1); run(100); ex.web_enc(6, 1); run(200);   // LINK: 1
ex.web_buttons(1 << 7); run(100); ex.web_buttons(0); run(200);
const beacon = (ticksQ2, bpm) => { const t = bpm * 32; for (const [c, v] of [[102, t >> 7], [103, t & 127], [104, ticksQ2 >> 14 & 127], [105, ticksQ2 >> 7 & 127], [106, ticksQ2 & 127], [107, 8]]) ex.web_midi(0xBF, c, v); };
let hub = ((ex.web_clock_q8() >>> 6) + 4 * 200) & 0x1FFFFF;                            // a hub 200 ticks ahead, at 100 BPM
for (let i = 0; i < 16; i++) { beacon(hub, 100); run(500); hub = (hub + Math.round(0.5 * 100 / 60 * 96 * 4)) & 0x1FFFFF; }
const err = ((ex.web_clock_q8() >>> 6) - hub + 0x300000) % 0x200000 - 0x100000;
console.log("link: " + (ex.web_bpm_q8() >> 8) + " BPM, " + (err / 4).toFixed(2) + " ticks from the hub's clock");
if ((ex.web_bpm_q8() >> 8) !== 100) fail("the tempo did not follow");
if (Math.abs(err) > 8) fail("the clock did not follow");
let out = 0; for (let v; (v = ex.web_midi_out()) >= 0;) out++;
if (out < 6) fail("no beacons went out");
console.log("ok");
