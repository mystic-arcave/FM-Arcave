// SPDX-License-Identifier: GPL-3.0-only
// The game's save, backed up: the SAVES page (web/site/save) and the PLAY page keep copies of a save in this browser,
// hand them out as files and put them back on an FM-1 or into the browser's own game.
//   Backups      the copies kept in this browser (localStorage): list(), add(), remove(), note()
//   toFile(), fromFile()   a backup as a file (JSON, the save in base64, its CRC-32), and read back; a bare save is read too
//   Game         the browser build as the judge of a save: about() (whose it is, how far), upgrade() (an older
//                version's save made this version's, as the FM-1 itself would on reading it)
//   Fm1Save      an FM-1's save read and written over PLAY WITH MY FM-1's own link (remote.js, ac_remote.h): the FM-1
//                sends its save to whoever says hello, and keeps one sent to it. Nothing new on the FM-1's side, so it
//                works with every FM-ARCAVE that has PLAY WITH MY FM-1 (0.22 on)
//   fit()        what to send an FM-1 whose own save is so long: the backup as it is, upgraded, or the reason not
// A save says which version wrote it only by its length (each version's is longer: arcave.c), so that is all the page
// goes by too.
import { Fm1Remote } from "./remote.js";

export const MAGIC = 0x41434156;                  // "ACAV", the save's first four bytes
export const SAVE_KEY = "fm-arcave-emu-save";     // the browser game's own save (web/emu/index.html)
const LIST_KEY = "fm-arcave-backups";
export const AUTO_KEEP = 8;                       // the automatic copies kept: the newest eight
export const MOST = 60;                           // ... and no more than sixty in all (a save is 3 KB; a browser gives 5 MB)
const SAVE_MAX = 4084;                            // one flash sector less its header

export const b64 = (bytes) => { let s = ""; for (const b of bytes) s += String.fromCharCode(b); return btoa(s); };
export const unb64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));
export function crc32(bytes) {
  let c = 0xffffffff;
  for (const b of bytes) { c ^= b; for (let k = 0; k < 8; k++) c = c >>> 1 ^ (c & 1 ? 0xedb88320 : 0); }
  return (~c) >>> 0;
}
const hex8 = (n) => (n >>> 0).toString(16).padStart(8, "0");
const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
// a save at all: long enough, short enough, and it begins as one
export const looksLikeSave = (bytes) => bytes && bytes.length >= 28 && bytes.length <= SAVE_MAX &&
  (bytes[0] | bytes[1] << 8 | bytes[2] << 16 | bytes[3] << 24) >>> 0 === MAGIC;

// ---------------------------------------------------------------- the copies kept in this browser
// one: { id, made (ms), from ("fm1", "browser", "file"), auto (why it was made by itself, or ""), note, game (the page's
// version when it was made), data (base64), about (Game.about's answer then) }
export class Backups {
  constructor(storage, now = () => Date.now()) { this.s = storage; this.now = now; }
  list() {
    try {
      const l = JSON.parse(this.s.getItem(LIST_KEY) || "[]");
      return Array.isArray(l) ? l.filter((b) => b && typeof b.data === "string").sort((a, b) => b.made - a.made) : [];
    } catch (e) { return []; }
  }
  put(l) { this.s.setItem(LIST_KEY, JSON.stringify(l)); }   // (throws when the browser's storage is full or shut)
  get(id) { return this.list().find((b) => b.id === id) || null; }
  bytes(id) { const b = this.get(id); return b ? unb64(b.data) : null; }
  // a copy kept. An automatic one of a save already in the list is not kept again, nor one made by hand of a save
  // already kept by hand from the same place; the automatic ones beyond the newest AUTO_KEEP go. Returns { b, fresh }
  // (fresh false: it was there already), or throws "full" (MOST copies, none of them automatic)
  add(bytes, { from = "file", auto = "", note = "", game = "", about = null, made = 0 } = {}) {
    const l = this.list(), data = b64(bytes);
    const twin = l.find((b) => b.data === data && (auto || (!b.auto && b.from === from)));
    if (twin) return { b: twin, fresh: false };
    const b = { id: this.now().toString(36) + Math.random().toString(36).slice(2, 8), made: made || this.now(), from, auto, note, game, data, about };
    l.unshift(b); l.sort((x, y) => y.made - x.made);
    let autos = 0;
    const kept = l.filter((x) => !x.auto || autos++ < AUTO_KEEP || x === b);
    while (kept.length > MOST) {                   // (full: the oldest automatic copy makes room; none left: said)
      let i = kept.length - 1;
      while (i >= 0 && (!kept[i].auto || kept[i] === b)) i--;
      if (i < 0) throw new Error("full");
      kept.splice(i, 1);
    }
    this.put(kept);
    return { b, fresh: true };
  }
  remove(id) { this.put(this.list().filter((b) => b.id !== id)); }
  note(id, text) { const l = this.list(), b = l.find((x) => x.id === id); if (b) { b.note = String(text).slice(0, 60); this.put(l); } }
}

// ---------------------------------------------------------------- a backup as a file
export function toFile(b) {
  const bytes = unb64(b.data);
  return JSON.stringify({
    format: "fm-arcave-save", v: 1, made: new Date(b.made).toISOString(), from: b.from, note: b.note || "", game: b.game || "",
    about: b.about || null, bytes: bytes.length, crc32: hex8(crc32(bytes)), data: b.data,
  }, null, 1) + "\n";
}
export function fileName(b) {
  const who = ((b.about && b.about.name) || "rock").toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "") || "rock";
  const d = new Date(b.made), p = (n) => String(n).padStart(2, "0");
  return `fm-arcave-save-${who}-${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}.json`;
}
// a file's bytes: { bytes, made, from, note, game }, or an Error whose message can be shown as it is
export function fromFile(buf) {
  const u = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
  if (looksLikeSave(u) && u[0] !== 0x7b) return { bytes: u.slice(), made: 0, from: "file", note: "", game: "" };   // (a bare save)
  let o;
  try { o = JSON.parse(new TextDecoder().decode(u)); } catch (e) { throw new Error("This is not an FM-ARCAVE save file."); }
  if (!o || o.format !== "fm-arcave-save" || typeof o.data !== "string") throw new Error("This is not an FM-ARCAVE save file.");
  if (o.v !== 1) throw new Error("This save file is from a newer FM-ARCAVE site than this page.");
  let bytes;
  try { bytes = unb64(o.data); } catch (e) { throw new Error("This save file is damaged."); }
  if (bytes.length !== o.bytes || hex8(crc32(bytes)) !== o.crc32 || !looksLikeSave(bytes)) throw new Error("This save file is damaged.");
  const made = Date.parse(o.made);
  return { bytes, made: Number.isFinite(made) ? made : 0, from: "file", note: typeof o.note === "string" ? o.note.slice(0, 60) : "", game: typeof o.game === "string" ? o.game.slice(0, 12) : "" };
}

// ---------------------------------------------------------------- the browser build as the judge of a save
export class Game {
  constructor(wasm) { this.wasm = wasm; this.mod = null; this.size = 0; }
  async fm(store) {
    this.mod = this.mod || await WebAssembly.compile(this.wasm);
    const ex = (await WebAssembly.instantiate(this.mod, {})).exports;
    if (ex._initialize) ex._initialize();
    if (store) { new Uint8Array(ex.memory.buffer, ex.web_store(), store.length).set(store); ex.web_store_set(store.length); }
    ex.web_boot();
    this.size = ex.web_save_size();
    return ex;
  }
  async saveSize() { if (!this.size) await this.fm(null); return this.size; }
  // what a save holds, read by the game itself: { ok, story (0 to 4), lang, name, played (seconds), tickets, crystals,
  // well (UNDERSONG's chapters), size, newer (longer than this version's: unread) }. ok false: this version reads nothing in it
  async about(bytes) {
    const size = await this.saveSize();
    if (!looksLikeSave(bytes)) return { ok: false, size: bytes ? bytes.length : 0 };
    if (bytes.length > size) return { ok: false, newer: true, size: bytes.length };
    const ex = await this.fm(bytes);
    if (!ex.web_about || !ex.web_about(0)) return { ok: false, size: bytes.length };
    let name = "";
    for (let i = 0; i < 16; i++) { const c = ex.web_about(16 + i); if (!c) break; name += String.fromCharCode(c); }
    const s = ex.web_scene();
    return { ok: true, size: bytes.length, older: bytes.length < size, story: s >> 8 & 255, lang: s >> 16 & 255, name,
      played: ex.web_about(1), tickets: ex.web_about(2), crystals: ex.web_about(3), well: ex.web_about(4) };
  }
  // an older version's save as this version's: the game reads it as the FM-1 would, and saves. null: it reads nothing in it
  async upgrade(bytes) {
    const size = await this.saveSize();
    if (!looksLikeSave(bytes) || bytes.length > size) return null;
    if (bytes.length === size) return bytes.slice();
    const ex = await this.fm(bytes);
    if (!ex.web_about || !ex.web_about(0)) return null;
    ex.web_save_now();
    const out = new Uint8Array(ex.memory.buffer, ex.web_store(), ex.web_store_len()).slice();
    return out.length === size ? out : null;
  }
}

// what to send an FM-1 whose own save is fm1Len bytes long, to put the save `bytes` on it: { bytes }, or { why }:
//   "damaged"    not a save this page's version reads
//   "update"     the backup is from a newer FM-ARCAVE than the FM-1 runs: the FM-1 wants an update first
//   "page"       the FM-1 runs a newer FM-ARCAVE than this page, and the backup is older still: only the page's own
//                version can make it this FM-1's
export async function fit(bytes, fm1Len, game) {
  if (!looksLikeSave(bytes)) return { why: "damaged" };
  if (bytes.length === fm1Len) return { bytes };
  if (bytes.length > fm1Len) return { why: "update" };
  const size = await game.saveSize();
  if (fm1Len < size) return { why: "update" };
  if (fm1Len > size) return { why: "page" };
  const up = await game.upgrade(bytes);
  return up ? { bytes: up, upgraded: true } : { why: "damaged" };
}

// ---------------------------------------------------------------- an FM-1's save, read and written
// const f = new Fm1Save(access, { now });  setInterval(() => f.tick(), 20);
//   await f.read()        its save (a Uint8Array); the FM-1 shows its "played in a browser" screen from here on
//   await f.write(bytes)  "kept", "refused" (not its size), "timeout", "lost"; after it the FM-1 is itself again, from
//                         the save it now holds
//   f.close()             goodbye without writing: the FM-1 is itself again, its save untouched
// read() rejects with an Error whose message is "absent" (nothing answered), "lost" or "timeout"
export class Fm1Save {
  constructor(access, opts = {}) {
    this.now = opts.now || (() => performance.now());
    this.want = null; this.len = 0; this.until = 0; this.damaged = 0;
    this.r = new Fm1Remote(access, {
      now: this.now,
      onState: (s, detail) => {
        if (detail === "damaged") this.damaged++;
        opts.onState?.(s, detail);
        if ((s === "absent" || s === "lost") && this.want) this.fail(s);
      },
      onSave: (bytes) => { this.len = bytes.length; const w = this.want; if (w) { this.want = null; w.res(bytes); } },
    });
  }
  fail(why) { const w = this.want; if (w) { this.want = null; w.rej(new Error(why)); } }
  read(ms = 30000) {
    return new Promise((res, rej) => { this.want = { res, rej }; this.until = this.now() + ms; this.r.start(); });
  }
  async write(bytes, ms = 30000) {
    if (this.r.state !== "connected") return "lost";
    const how = await this.r.leave(bytes, ms);
    return how === "none" || how === "stopped" ? "lost" : how;
  }
  close() { this.fail("lost"); this.r.stop(); }
  tick() {
    this.r.tick();
    if (this.want && this.now() > this.until) { this.r.stop(); this.fail("timeout"); }
  }
  progress() { return this.r.progress(); }
  get state() { return this.r.state; }
}

// ---------------------------------------------------------------- how a save is told to a person
export const STORY = ["ON THE WAY DOWN", "IN THE CAVE", "MENDING THE JUKEBOX", "MENDING THE JUKEBOX", "THE ARCADE IS OPEN"];
export function played(s) {
  const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60);
  return h ? `${h} H ${m} MIN` : m ? `${m} MIN` : "UNDER A MINUTE";
}
