// SPDX-License-Identifier: GPL-3.0-only
// FM-ARCAVE's lobby server on Node, with nothing to install: for trying the Online page on one computer, for the tests,
// and for anyone who would rather run the lobbies on their own machine than on Cloudflare (server/worker.mjs).
//   node server/node.mjs [PORT]          (8787 by default; the Online page then takes ?server=ws://localhost:8787)
// Behind a proxy that ends TLS (Caddy, nginx, a tunnel), pages on https reach it as wss://.
import { createServer } from "node:http";
import { createHash } from "node:crypto";
import { pathToFileURL } from "node:url";
import { Lobby, newCode, cleanCode, tell } from "./lobby.mjs";

const GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// a WebSocket on a raw socket: text frames in and out, ping, close. Frames from a browser are masked; ours are not
function wsAccept(req, socket, onText, onClose) {
  const key = req.headers["sec-websocket-key"];
  if (!key || (req.headers.upgrade || "").toLowerCase() !== "websocket") { socket.end("HTTP/1.1 400 Bad Request\r\n\r\n"); return null; }
  socket.write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "
    + createHash("sha1").update(key + GUID).digest("base64") + "\r\n\r\n");
  socket.setNoDelay(true);                                // a key press goes out at once, not with the next one
  let buf = Buffer.alloc(0), parts = [], open = true;
  const frame = (op, body) => {
    const n = body.length, head = n < 126 ? Buffer.from([0x80 | op, n]) : n < 65536 ? Buffer.from([0x80 | op, 126, n >> 8, n & 255]) : null;
    if (!head || !open) return;
    try { socket.write(Buffer.concat([head, body])); } catch (e) {}
  };
  const ws = {
    send: (text) => frame(1, Buffer.from(text)),
    close: (code = 1000, why = "") => {
      if (!open) return;
      const b = Buffer.alloc(2 + Buffer.byteLength(why)); b.writeUInt16BE(code); b.write(why, 2);
      frame(8, b); open = false; socket.end(); finish();
    },
    kill: () => { open = false; socket.destroy(); finish(); },   // for tests: a connection that breaks without a word
  };
  let finished = false;
  const finish = () => { if (!finished) { finished = true; onClose(); } };
  socket.on("data", (chunk) => {
    buf = Buffer.concat([buf, chunk]);
    for (;;) {
      if (buf.length < 2) return;
      const fin = buf[0] & 0x80, op = buf[0] & 15, masked = buf[1] & 0x80;
      let n = buf[1] & 127, at = 2;
      if (n === 126) { if (buf.length < 4) return; n = buf.readUInt16BE(2); at = 4; }
      else if (n === 127) { ws.close(1009, "too big"); return; }
      if (n > 65536) { ws.close(1009, "too big"); return; }
      if (buf.length < at + (masked ? 4 : 0) + n) return;
      const mask = masked ? buf.subarray(at, at + 4) : null; at += masked ? 4 : 0;
      const body = Buffer.from(buf.subarray(at, at + n)); buf = buf.subarray(at + n);
      if (mask) for (let i = 0; i < n; i++) body[i] ^= mask[i & 3];
      if (op === 8) { ws.close(1000); return; }
      if (op === 9) { frame(10, body); continue; }
      if (op === 10) continue;
      if (op === 1 || op === 2 || op === 0) {
        parts.push(body);
        if (parts.reduce((s, p) => s + p.length, 0) > 65536) { ws.close(1009, "too big"); return; }
        if (fin) { const all = Buffer.concat(parts); parts = []; if (op !== 2) onText(all.toString("utf8")); }
      }
    }
  });
  socket.on("close", finish);
  socket.on("error", () => { open = false; finish(); });
  return ws;
}

export function startServer(port = 8787, host = "0.0.0.0") {
  const lobbies = new Map();
  const sockets = new Set();
  const server = createServer((req, res) => {
    res.writeHead(200, { "content-type": "text/plain; charset=utf-8", "access-control-allow-origin": "*" });
    res.end(`FM-ARCAVE lobbies: ${lobbies.size} open. Connect from the Link page's ONLINE tab.\n`);
  });
  server.on("upgrade", (req, socket) => {
    const path = new URL(req.url, "http://x").pathname;
    let lobby = null, why = null;
    if (path === "/new") {
      let code; do code = newCode(); while (lobbies.has(code));
      lobby = new Lobby(code); lobbies.set(code, lobby);
    } else {
      const m = path.match(/^\/join\/([^/]+)$/), code = m && cleanCode(decodeURIComponent(m[1]));
      if (!code) why = "bad-code"; else if (!(lobby = lobbies.get(code))) why = "no-lobby";
    }
    let mem = null;
    const ws = wsAccept(req, socket, (t) => { if (mem) lobby.text(mem, t); }, () => { sockets.delete(ws); if (mem) lobby.drop(mem); });
    if (!ws) return;
    sockets.add(ws);
    if (why) { tell(ws, { t: "err", why }); ws.close(4004, why); return; }
    mem = lobby.add(ws);
  });
  const timer = setInterval(() => {
    const now = Date.now();
    for (const [code, l] of lobbies) { l.sweep(now); if (l.done(now)) lobbies.delete(code); }
  }, 5000);
  return new Promise((ok) => server.listen(port, host, () => ok({
    port: server.address().port, lobbies, sockets,
    close: () => new Promise((done) => { clearInterval(timer); for (const s of sockets) s.kill(); server.close(() => done()); }),
  })));
}

if (import.meta.url === pathToFileURL(process.argv[1] || "").href) {
  const s = await startServer(+process.argv[2] || 8787);
  console.log(`FM-ARCAVE lobbies on ws://localhost:${s.port} (the Online page: ?server=ws://localhost:${s.port})`);
}
