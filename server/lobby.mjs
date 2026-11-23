// SPDX-License-Identifier: GPL-3.0-only
// FM-ARCAVE's online lobbies: the rules, shared by the Node server (server/node.mjs) and the Cloudflare Worker
// (server/worker.mjs). A lobby is private: nobody can list lobbies, one is found only by its code. Up to three
// computers meet in one, and what each sends (UNDERSONG's control changes, "m") goes to the others as it comes.
// Nothing here knows the game: the Link page on each computer passes the messages to its FM-1s and emulators.
//
// The wire, JSON text frames over a WebSocket at /new (make a lobby) or /join/CODE:
//   server -> you   {t:"lobby", code, you}               the answer to your "hi": you are in; you: your number here
//                   {t:"roster", code, members:[{id, name, kind, rtt, well}]}   whenever anything about them changes
//                   {t:"m", f, d:[cc, v, cc, v, ...]}    control changes on channel 16 from member f
//                   {t:"pong", n}                        the answer to your ping
//                   {t:"err", why}                       no-lobby, full, bad-code, busy; the socket is closed after it
//   you -> server   {t:"hi", name, token}                first: who you are (token: the same on a reconnect)
//                   {t:"m", d:[...]}                     to everyone else in the lobby
//                   {t:"ping", n, rtt, kind, well}       every two seconds: your last round trip, what plays here
//                                                        ("usb", "emu", "usb+emu" or ""), whether one is in the well
export const ALPHA = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";  // no I, O, 0 or 1: a code is read aloud and typed
export const LEN = 6;                                      // 32^6, about a billion
export const MAX = 3;                                      // UNDERSONG is for two or three
export const IDLE_MS = 15000;                              // a member not heard from for this long is gone
export const KEEP_MS = 30 * 60000;                         // an empty lobby stays open this long, for whoever comes back
const MAX_TEXT = 4096, MAX_PER_SEC = 240;

export function newCode() {
  const b = new Uint8Array(LEN); crypto.getRandomValues(b);
  return Array.from(b, (x) => ALPHA[x & 31]).join("");
}
export function cleanCode(s) {
  const c = String(s || "").toUpperCase().replace(/[^A-Z0-9]/g, "");
  return c.length === LEN && [...c].every((x) => ALPHA.includes(x)) ? c : null;
}
export function cleanName(s) {
  return String(s || "").toUpperCase().replace(/[^A-Z0-9 .!?-]/g, "").replace(/\s+/g, " ").trim().slice(0, 12) || "PLAYER";
}
const KINDS = new Set(["", "usb", "emu", "usb+emu"]);

export class Lobby {
  constructor(code, now = Date.now()) { this.code = code; this.members = []; this.next = 1; this.emptySince = now; this.swept = now; this.gone = new Map(); }

  // a socket wants in. sock: { send(text), close(code, reason) }. It is let in at its "hi": the same computer coming
  // back after its connection broke then takes its own place, even in a full lobby
  add(sock, now = Date.now()) {
    this.sweep(now);
    const m = { id: 0, sock, name: "PLAYER", token: "", kind: "", rtt: 0, well: false, seen: now, win: now, n: 0 };
    setTimeout(() => { if (!m.id) sock.close(4001, "no hi"); }, 10000);
    return m;
  }

  // a text frame from a member
  text(m, s, now = Date.now()) {
    if (typeof s !== "string" || s.length > MAX_TEXT) return;
    if (now - m.win >= 1000) { m.win = now; m.n = 0; }
    if (++m.n > MAX_PER_SEC) return;                      // a flood is dropped, not passed on
    m.seen = now;
    if (now - this.swept > 5000) this.sweep(now);
    let o; try { o = JSON.parse(s); } catch (e) { return; }
    if (!o || typeof o !== "object") return;
    if (!m.id) { if (o.t === "hi") this.hi(m, o, now); return; }
    if (o.t === "m") {
      const d = o.d;
      if (!Array.isArray(d) || !d.length || d.length > 64 || d.length & 1 || !d.every((x) => Number.isInteger(x) && x >= 0 && x < 128)) return;
      const out = JSON.stringify({ t: "m", f: m.id, d });
      for (const x of this.members) if (x !== m) x.sock.send(out);
    } else if (o.t === "ping") {
      tell(m.sock, { t: "pong", n: Number.isInteger(o.n) ? o.n : 0 });
      const rtt = Math.max(0, Math.min(9999, Math.round(+o.rtt || 0))), kind = KINDS.has(o.kind) ? o.kind : "", well = !!o.well;
      if (rtt !== m.rtt || kind !== m.kind || well !== m.well) { m.rtt = rtt; m.kind = kind; m.well = well; this.roster(); }
    }
  }

  hi(m, o, now) {
    m.name = cleanName(o.name);
    m.token = typeof o.token === "string" ? o.token.slice(0, 40) : "";
    // the same computer back after its connection broke: it takes its old place and number, and the old socket goes
    const old = m.token ? this.members.findIndex((x) => x.token === m.token) : -1;
    if (old >= 0) {
      const x = this.members[old]; m.id = x.id; m.seen = now; this.members[old] = m; x.sock.close(4000, "replaced");
    } else {
      if (this.members.length >= MAX) { tell(m.sock, { t: "err", why: "full" }); m.sock.close(4003, "full"); return; }
      const g = this.gone.get(m.token);                  // its old socket already closed: its old number still
      m.id = g && now - g.at < IDLE_MS * 4 ? g.id : this.next++; m.seen = now;
      this.gone.delete(m.token);
      this.members.push(m);
    }
    tell(m.sock, { t: "lobby", code: this.code, you: m.id });
    this.roster();
  }

  drop(m, now = Date.now(), quiet = false) {
    const i = this.members.indexOf(m);
    if (i < 0) return;
    this.members.splice(i, 1);
    if (m.token) { this.gone.set(m.token, { id: m.id, at: now }); if (this.gone.size > 16) this.gone.delete(this.gone.keys().next().value); }
    if (!this.members.length) this.emptySince = now;
    if (!quiet) this.roster();
  }

  sweep(now = Date.now()) {
    this.swept = now;
    for (const m of [...this.members]) if (now - m.seen > IDLE_MS) { this.drop(m, now); m.sock.close(4008, "silent"); }
  }

  // an empty lobby nobody has come back to in KEEP_MS is closed
  done(now = Date.now()) { return !this.members.length && now - this.emptySince > KEEP_MS; }

  roster() {
    const s = JSON.stringify({ t: "roster", code: this.code, members: this.members.map((m) => ({ id: m.id, name: m.name, kind: m.kind, rtt: m.rtt, well: m.well })) });
    for (const m of this.members) m.sock.send(s);
  }
}

export function tell(sock, o) { sock.send(JSON.stringify(o)); }
