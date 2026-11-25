// SPDX-License-Identifier: GPL-3.0-only
// The mystic machines' slots and the music room's, from a page: the MACHINES page (web/site/machines) reads which
// machine stands on each of the Arcave's three ledges and which instrument in each of the music room's four places,
// loads another into a ledge's or a place's slot of the bank, and puts the firmware's own back.
// firmware/src/arcave/mm.h says what a loaded machine is, mi.h a loaded instrument, ac_slots.h what the FM-1 does with
// one, tools/mm_build.py makes the files.
//   parseFile(bytes)        a .mmach or .minst file: { kind: "machine" | "instrument", img, rel, api, id, ver, code }
//                           (throws an Error that can be shown)
//   forSlot(file, slot)     the machine as that slot of the bank holds it: its addresses moved there, its size and
//                           CRC-32 sealed in. The bank's slots: the ledges' three, then the places' four
//   parseInfo(bytes)        the bank's state as the FM-1 tells it (ac_slots.h mb_info): .slots the ledges, .places the room's
//   Fm1Bank                 an FM-1's bank over PLAY WITH MY FM-1's link (remote.js; an FM-ARCAVE with the slots answers
//                           protocol 3): open(), load(slot, file), choose(slot, which), empty(slot), close(); for the
//                           room: loadPlace(place, file), choosePlace(place, loaded), emptyPlace(place)
//   BrowserBank             the same for the browser's own game: a copy of the game in this page does the work, and the
//                           save and the bank go into this browser's storage, where the PLAY page finds them
// A machine goes over in pieces of 1 KiB, each answered before the next: an FM-1 writes each to its flash as it comes.
import { Fm1Remote } from "./remote.js";
import { crc32, b64, unb64, SAVE_KEY, looksLikeSave } from "./saves.js";

export const MAGIC = 0x48434d4d, MAGIC_INST = 0x534e494d, RELOC = 0x4c524d4d, HEAD = 20, PIECE = 1024;
export const BANK_SLOTS = 7;                      // the ledges' three, then the music room's four places
export const BANK_BYTES = BANK_SLOTS * 0xa000;    // slots of 40 KiB (mm.h)
export const BANK_KEY = "fm-arcave-emu-bank";     // the browser game's bank: a JSON list, a slot's image in base64 each ("" none)
export const OWN = ["CRYSTAL ROAD", "JUICE WORKS", "NIGHT BLOOM"];   // the firmware's own three, by number
export const OWN_INST = ["ROOTS", "THE GEODE", "MUSHROOM RACK", "CRYSTAL SEAM"];   // the firmware's own instrument of each place
export const ST = { EMPTY: 0, OK: 1, OTHER: 2, BAD: 3 };
// what an answer to a command means (ac_slots.h mb_cmd)
export const WHY = ["", "The FM-1 could not do that just now.", "It came through damaged, or it is not the kind of thing that slot holds. Nothing stands in that slot now; the firmware's own is there instead.",
  "The FM-1's flash would not take it.", "This file is whole, but it is not one this FM-1's FM-ARCAVE can run."];

export function parseFile(buf) {
  const u = buf instanceof Uint8Array ? buf : new Uint8Array(buf), v = new DataView(u.buffer, u.byteOffset, u.byteLength);
  const no = (m) => { throw new Error(m); };
  if (u.length < HEAD + 12) no("This is not a mystic machine file.");
  const n = v.getUint32(u.length - 12, true), magic = v.getUint32(u.length - 8, true), len = v.getUint32(u.length - 4, true);
  const first = v.getUint32(0, true), kind = first === MAGIC ? "machine" : first === MAGIC_INST ? "instrument" : "";
  if (magic !== RELOC || len + 4 * n + 12 !== u.length || len < HEAD || len % 4 || !kind) no("This is not a mystic machine file.");
  const img = new Uint8Array(len), rel = [];    // (a copy of its own, plain bytes whatever kind came in)
  img.set(u.subarray(0, len));
  for (let i = 0; i < n; i++) { const w = v.getUint32(len + 4 * i, true); if (w % 2 || w + 4 > len || w < HEAD) no("This machine file is damaged."); rel.push(w); }
  if (v.getUint32(12, true) !== len || v.getUint32(16, true) !== sealOf(img)) no("This machine file is damaged.");
  return { kind, img, rel, api: v.getUint16(4, true), state: v.getUint16(6, true), id: v.getUint16(8, true), ver: v.getUint16(10, true), code: n > 0 };
}
const sealOf = (img) => { const b = new Uint8Array(img.length - 4); b.set(img.subarray(0, 16)); b.set(img.subarray(HEAD), 16); return crc32(b); };
export function forSlot(file, slot, slotBytes) {
  const b = file.img.slice(), v = new DataView(b.buffer);
  if (b.length > slotBytes) throw new Error("This machine is larger than a slot.");
  for (const w of file.rel) v.setUint32(w, (v.getUint32(w, true) + slot * slotBytes) >>> 0, true);
  v.setUint32(12, b.length, true);
  v.setUint32(16, sealOf(b), true);
  return b;
}
export function parseInfo(u) {
  if (!u || u.length < 16 || u[0] !== 2 || u.length < 16 + 32 * (u[2] + u[7])) throw new Error("The FM-1's answer about its machines made no sense.");
  const o = { api: u[1], nslot: u[2], slotBytes: u[3] * 1024, stateMax: u[4] * 256, runs: !!u[5], bank: !!u[6],
              nplace: u[7], iapi: u[8], istateMax: u[9] * 256, slots: [], places: [] };
  for (let i = 0; i < o.nslot + o.nplace; i++) {
    const q = u.subarray(16 + 32 * i, 48 + 32 * i);
    let name = ""; for (let k = 10; k < 30 && q[k]; k++) name += String.fromCharCode(q[k]);
    const s = { own: q[0] & 3, loaded: !!(q[0] & 0x80), st: q[1], id: q[2] | q[3] << 8, ver: q[4] | q[5] << 8, len: q[6] | q[7] << 8 | q[8] << 16, name };
    // what stands there: the loaded one, when it is asked for, whole and may be run; else the firmware's own
    s.shows = s.loaded && s.st === ST.OK && o.runs ? "loaded" : "own";
    if (i < o.nslot) { s.title = s.shows === "loaded" ? s.name : OWN[s.own] || OWN[0]; o.slots.push(s); }
    else { s.own = i - o.nslot; s.title = s.shows === "loaded" ? s.name : OWN_INST[s.own] || ""; o.places.push(s); }
  }
  return o;
}
// a machine or an instrument may go onto this FM-1 (or game): "" yes, else the reason
export function fits(file, info, fm1) {
  const inst = file.kind === "instrument", what = inst ? "instrument" : "machine";
  if (inst && !info.nplace) return "This " + (fm1 ? "FM-1's FM-ARCAVE" : "page") + " has no slots for instruments yet.";
  if (file.api > (inst ? info.iapi : info.api)) return "This " + what + " needs a newer FM-ARCAVE than this " + (fm1 ? "FM-1 has: update it on the INSTALL page first." : "page has.");
  if (fm1 && !file.code) return "This file is the browser game's copy of the " + what + ": it holds no code for an FM-1.";
  if (file.img.length > info.slotBytes || file.state > (inst ? info.istateMax : info.stateMax)) return "This " + what + " is too large for a slot.";
  return "";
}

// ---------------------------------------------------------------- an FM-1's bank
// const b = new Fm1Bank(access, { now });  setInterval(() => b.tick(), 20);
//   await b.open()               { info, save }; rejects "absent", "lost", "timeout", or "old" (an FM-ARCAVE without slots)
//   await b.load(slot, file, onProgress)   0 done, else mb_cmd's answer (WHY); onProgress(sent, of) in bytes
//   await b.choose(slot, which)  which 0..2: the firmware's own machine for that ledge; 3: the loaded one again
//   await b.empty(slot)
//   await b.loadPlace(place, file, onProgress), b.choosePlace(place, loaded), b.emptyPlace(place)   the same for the
//                                music room's places 0..3: an instrument into the place's slot; the firmware's own
//                                instrument there (loaded false) or the loaded one again; the slot emptied
//   b.close()                    goodbye: the FM-1 starts again as after a power-on, with its ledges as they now are
export class Fm1Bank {
  constructor(access, opts = {}) {
    this.now = opts.now || (() => performance.now());
    this.want = null; this.until = 0; this.info = null; this.save = null;
    this.r = new Fm1Remote(access, {
      now: this.now,
      onState: (s, d) => { opts.onState?.(s, d); if ((s === "absent" || s === "lost") && this.want) this.fail(s); },
      onSave: (bytes) => { this.save = bytes; const w = this.want; if (w) { this.want = null; w.res(bytes); } },
    });
  }
  fail(why) { const w = this.want; if (w) { this.want = null; w.rej(new Error(why)); } }
  async open(ms = 30000) {
    await new Promise((res, rej) => { this.want = { res, rej }; this.until = this.now() + ms; this.r.start(); });
    if (this.r.proto < 3) { this.r.stop(); throw new Error("old"); }
    this.info = parseInfo(await this.r.bankInfo());
    return { info: this.info, save: this.save };
  }
  async refresh() { this.info = parseInfo(await this.r.bankInfo()); return this.info; }
  // into bank slot b: the command that empties it, the pieces, the command that has it checked
  async send(b, file, begin, commit, onProgress) {
    const img = forSlot(file, b, this.info.slotBytes);
    let r = await this.r.bank(begin);
    if (r) return r;
    for (let off = 0; off < img.length; off += PIECE) {
      const piece = img.subarray(off, Math.min(img.length, off + PIECE));
      let kept = 0;
      for (let tries = 0; tries < 3 && kept !== 1; tries++) kept = await this.r.sendPiece(1 + b, piece);   // (damaged on the way: the FM-1 kept none of it, the same again)
      if (kept !== 1) return 2;
      onProgress?.(off + piece.length, img.length);
    }
    r = await this.r.bank(commit);
    await this.refresh();
    return r;
  }
  load(slot, file, onProgress) { return this.send(slot, file, 16 + slot, 32 + slot, onProgress); }
  async choose(slot, which) { const r = await this.r.bank(48 + 4 * slot + which); await this.refresh(); return r; }
  async empty(slot) { const r = await this.r.bank(64 + slot); await this.refresh(); return r; }
  loadPlace(place, file, onProgress) { return this.send(this.info.nslot + place, file, 80 + place, 84 + place, onProgress); }
  async choosePlace(place, loaded) { const r = await this.r.bank(96 + 2 * place + (loaded ? 1 : 0)); await this.refresh(); return r; }
  async emptyPlace(place) { const r = await this.r.bank(88 + place); await this.refresh(); return r; }
  close() { this.fail("lost"); this.r.stop(); }
  tick() { this.r.tick(); if (this.want && this.now() > this.until) { this.r.stop(); this.fail("timeout"); } }
  get state() { return this.r.state; }
}

// ---------------------------------------------------------------- the browser game's bank
// the same answers from a copy of the game run in this page; every change goes into the browser's storage at once
export class BrowserBank {
  constructor(wasm, storage) { this.wasm = wasm; this.s = storage; this.ex = null; this.info = null; }
  mem(ptr, n) { return new Uint8Array(this.ex.memory.buffer, ptr, n); }
  async open() {
    const ex = (await WebAssembly.instantiate(this.wasm, {})).instance.exports;
    if (ex._initialize) ex._initialize();
    if (!ex.web_bank_cmd) throw new Error("old");
    this.ex = ex;
    let save = null;
    try { const t = this.s.getItem(SAVE_KEY); if (t) { const b = unb64(t); if (looksLikeSave(b)) save = b; } } catch (e) {}
    if (save) { this.mem(ex.web_store(), save.length).set(save); ex.web_store_set(save.length); }
    const bank = readBank(this.s, ex.web_bank_len());
    if (bank) this.mem(ex.web_bank(), bank.length).set(bank);
    ex.web_boot();
    return { info: this.refresh(), save };
  }
  refresh() { this.info = parseInfo(this.mem(this.ex.web_bank_info(), 16 + 32 * BANK_SLOTS).slice()); return this.info; }
  keep() {
    const ex = this.ex, n = this.info.slotBytes, list = [];
    ex.web_save_now();
    this.s.setItem(SAVE_KEY, b64(this.mem(ex.web_store(), ex.web_store_len())));
    [...this.info.slots, ...this.info.places].forEach((s, i) => list.push(s.len ? b64(this.mem(ex.web_bank() + i * n, s.len)) : ""));
    this.s.setItem(BANK_KEY, JSON.stringify(list));
  }
  async send(b, file, begin, commit, onProgress) {
    const img = forSlot(file, b, this.info.slotBytes), ex = this.ex;
    let r = ex.web_bank_cmd(begin);
    if (r) return r;
    this.mem(ex.web_bank() + b * this.info.slotBytes, img.length).set(img);
    onProgress?.(img.length, img.length);
    r = ex.web_bank_cmd(commit);
    this.refresh(); this.keep();
    return r;
  }
  load(slot, file, onProgress) { return this.send(slot, file, 16 + slot, 32 + slot, onProgress); }
  async choose(slot, which) { const r = this.ex.web_bank_cmd(48 + 4 * slot + which); this.refresh(); this.keep(); return r; }
  async empty(slot) { const r = this.ex.web_bank_cmd(64 + slot); this.refresh(); this.keep(); return r; }
  loadPlace(place, file, onProgress) { return this.send(this.info.nslot + place, file, 80 + place, 84 + place, onProgress); }
  async choosePlace(place, loaded) { const r = this.ex.web_bank_cmd(96 + 2 * place + (loaded ? 1 : 0)); this.refresh(); this.keep(); return r; }
  async emptyPlace(place) { const r = this.ex.web_bank_cmd(88 + place); this.refresh(); this.keep(); return r; }
  close() { this.ex = null; }
  tick() {}
}
// the browser game's bank as the game wants it at its start: every slot's bytes, 0xFF where nothing is (null: none kept)
export function readBank(storage, total, slots = BANK_SLOTS) {
  let list;
  try { list = JSON.parse(storage.getItem(BANK_KEY) || "null"); } catch (e) { return null; }
  if (!Array.isArray(list)) return null;
  const out = new Uint8Array(total).fill(255), n = total / slots;
  list.slice(0, slots).forEach((t, i) => { try { if (t) { const b = unb64(t); if (b.length <= n) out.set(b, i * n); } } catch (e) {} });
  return out;
}
