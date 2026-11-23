// SPDX-License-Identifier: GPL-3.0-only
// The Online page's side of a lobby (server/lobby.mjs has the wire): make one or join one by its code, pass UNDERSONG's
// messages, keep a round trip, and come back by itself if the connection breaks. Runs in a page and in Node (tests).
//
// The lobby server this site uses: server/worker.mjs on Cloudflare. ?server=wss://... on the Online page overrides it
// (server/node.mjs on one computer: ?server=ws://localhost:8787), and a join link carries that on.
export const SERVER = "wss://fm-arcave-lobby.mystic-arcave.workers.dev";

const TRIES = [500, 1000, 2000, 4000, 8000, 8000, 8000];     // after a broken connection: wait, try again, give up
export class LobbyClient {
  // on: { state(s, why), roster(members, you), message(from, d) }; state: "connecting", "in", "again", "out", "error"
  constructor(server, on) {
    this.server = String(server).replace(/\/+$/, ""); this.on = on;
    this.ws = null; this.code = null; this.you = 0; this.name = "PLAYER"; this.rtt = 0; this.members = [];
    this.token = Math.random().toString(36).slice(2) + Date.now().toString(36);
    this.info = () => ({ kind: "", well: false });             // what plays on this computer: the page sets it
    this.pings = new Map(); this.n = 0; this.timer = 0; this.tries = 0; this.leaving = false; this.sent = 0; this.got = 0;
  }
  create(name) { this.name = name; this.code = null; this.tries = 0; this.open("/new"); }
  join(code, name) { this.name = name; this.code = code; this.tries = 0; this.open("/join/" + encodeURIComponent(code)); }
  leave() {
    this.leaving = true; clearInterval(this.timer); clearTimeout(this.again);
    if (this.ws) try { this.ws.close(1000, "bye"); } catch (e) {}
    this.ws = null; this.code = null; this.members = []; this.state("out");
  }
  // control changes for everyone else in the lobby: [cc, v, cc, v, ...]
  send(d) {
    if (!this.ws || this.ws.readyState !== 1 || !d.length) return false;
    this.ws.send(JSON.stringify({ t: "m", d })); this.sent++; return true;
  }
  state(s, why) { this.status = s; this.on.state?.(s, why); }

  open(path) {
    this.leaving = false; clearInterval(this.timer); clearTimeout(this.again);
    if (!this.tries) this.state("connecting");
    let ws;
    try { ws = new WebSocket(this.server + path); } catch (e) { this.state("error", "server"); return; }
    this.ws = ws;
    let refused = false, wasIn = false;
    ws.onopen = () => {
      ws.send(JSON.stringify({ t: "hi", name: this.name, token: this.token }));
      this.timer = setInterval(() => this.ping(), 2000); this.ping();
    };
    ws.onmessage = (e) => {
      if (ws !== this.ws) return;
      let o; try { o = JSON.parse(e.data); } catch (x) { return; }
      if (o.t === "m") { this.got++; this.on.message?.(o.f, o.d); }
      else if (o.t === "pong") { const t = this.pings.get(o.n); if (t != null) { this.pings.delete(o.n); this.rtt = now() - t; } }
      else if (o.t === "roster") { this.members = o.members; this.on.roster?.(o.members, this.you); }
      else if (o.t === "lobby") { this.code = o.code; this.you = o.you; this.tries = 0; wasIn = true; this.state("in"); }
      else if (o.t === "err") { refused = true; this.code = o.why === "full" ? this.code : null; this.state("error", o.why); }
    };
    ws.onclose = () => {
      if (ws !== this.ws) return;
      clearInterval(this.timer); this.ws = null;
      if (this.leaving || refused) return;
      if (!wasIn && !this.code) { this.state("error", "server"); return; }   // never got in, nothing to go back to
      if (this.tries >= TRIES.length) { this.code = null; this.state("error", "lost"); return; }
      this.state("again");
      this.again = setTimeout(() => this.open("/join/" + encodeURIComponent(this.code)), TRIES[this.tries++]);
    };
    ws.onerror = () => {};
  }
  ping() {
    if (!this.ws || this.ws.readyState !== 1) return;
    const n = ++this.n & 1023, i = this.info();
    this.pings.set(n, now()); if (this.pings.size > 8) this.pings.delete(this.pings.keys().next().value);
    this.ws.send(JSON.stringify({ t: "ping", n, rtt: Math.round(this.rtt), kind: i.kind, well: i.well }));
  }
}
const now = () => (globalThis.performance ? performance.now() : Date.now());
