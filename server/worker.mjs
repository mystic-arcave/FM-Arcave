// SPDX-License-Identifier: GPL-3.0-only
// FM-ARCAVE's lobby server as a Cloudflare Worker: one Durable Object per lobby, so the two or three computers in a
// lobby always meet in the same place. The rules are server/lobby.mjs, the same as on Node (server/node.mjs).
//   cd server && npx wrangler deploy          (wrangler.toml beside this file)
// The address it prints, with wss:// in front, goes into web/link/lobby.js (SERVER).
import { Lobby, newCode, cleanCode, tell, KEEP_MS } from "./lobby.mjs";

// a socket that is told why and closed: for a code that is not a lobby
function refuse(why) {
  const [client, server] = Object.values(new WebSocketPair());
  server.accept(); tell(server, { t: "err", why }); server.close(4004, why);
  return new Response(null, { status: 101, webSocket: client });
}

export default {
  async fetch(req, env) {
    const path = new URL(req.url).pathname;
    if ((req.headers.get("Upgrade") || "").toLowerCase() !== "websocket")
      return new Response("FM-ARCAVE lobbies. Connect from the Link page's ONLINE tab.\n", { headers: { "content-type": "text/plain; charset=utf-8", "access-control-allow-origin": "*" } });
    if (path === "/new") {
      for (let i = 0; i < 4; i++) {                         // a code still in use is drawn again
        const code = newCode();
        const r = await env.LOBBY.get(env.LOBBY.idFromName(code)).fetch(new Request("https://lobby/new/" + code, req));
        if (r.status !== 409) return r;
      }
      return refuse("busy");
    }
    const m = path.match(/^\/join\/([^/]+)$/), code = m && cleanCode(decodeURIComponent(m[1]));
    if (!code) return refuse("bad-code");
    return env.LOBBY.get(env.LOBBY.idFromName(code)).fetch(new Request("https://lobby/join/" + code, req));
  },
};

export class LobbyRoom {
  constructor(state) { this.state = state; this.lobby = null; }

  async fetch(req) {
    const [, verb, code] = new URL(req.url).pathname.split("/");
    const last = await this.state.storage.get("last");     // when this lobby was last made or left: it outlives the object
    const now = Date.now(), alive = last != null && (this.lobby?.members.length || now - last < KEEP_MS);
    if (verb === "new") {
      if (alive) return new Response(null, { status: 409 });
      this.lobby = new Lobby(code, now);
      await this.state.storage.put("last", now);
      await this.state.storage.setAlarm(now + KEEP_MS);
    } else if (!alive) return refuse("no-lobby");
    this.lobby ||= new Lobby(code, now);
    const lobby = this.lobby;
    const [client, server] = Object.values(new WebSocketPair());
    server.accept();
    const sock = { send: (t) => { try { server.send(t); } catch (e) {} }, close: (c, w) => { try { server.close(c, w); } catch (e) {} } };
    const mem = lobby.add(sock, now);
    if (mem) {
      server.addEventListener("message", (e) => lobby.text(mem, typeof e.data === "string" ? e.data : "", Date.now()));
      let gone = false;
      const bye = () => {
        if (gone) return; gone = true;
        lobby.drop(mem, Date.now());
        if (!lobby.members.length) { const t = Date.now(); this.state.storage.put("last", t); this.state.storage.setAlarm(t + KEEP_MS); }
      };
      // a page that closes its side is answered, or it waits for the close to finish
      server.addEventListener("close", (e) => { bye(); try { server.close(e.code === 1005 ? 1000 : e.code, "bye"); } catch (x) {} });
      server.addEventListener("error", bye);
    }
    return new Response(null, { status: 101, webSocket: client });
  }

  async alarm() {
    const now = Date.now();
    if (this.lobby?.members.length) { this.lobby.sweep(now); await this.state.storage.setAlarm(now + KEEP_MS); return; }
    const last = await this.state.storage.get("last");
    if (last == null || now - last >= KEEP_MS - 1000) { this.lobby = null; await this.state.storage.deleteAll(); }
    else await this.state.storage.setAlarm(last + KEEP_MS);
  }
}
