// SPDX-License-Identifier: GPL-3.0-only
// The FM-1's panel, drawn to its real proportions (the 1000 x 624 layout the FM-1 emulators share): the screen,
// MASTER and the seven encoders, OCT- OCT+, the twelve buttons, two rows of pill keys with a slit light each.
// mountFm1(host, on) builds it inside `host` and reports what is touched:
//   on.buttons(mask)   bit = the firmware's BTN_ id        on.keys(mask)   bit 0 = F3 ... bit 26 = G5
//   on.enc(id, n)      the firmware's ENC_ id, n detents
// It returns { screen(fb565), leds(buttons, keys, dimKeys, breathing), key(code, down) -> handled, releaseAll(),
//   layout("fm1" | "computer"): which computer keys play it (computer: the two bottom rows of letters are its two rows
//   of keys, and every button, key and knob says which computer key is its own, under its name),
//   big(on): the screen as large as the window, with its lights in a strip below (on.bigOff() when it is left) }.
const NS = "http://www.w3.org/2000/svg";
const L = { w: 1000, h: 624, screen: [292, 44, 256],
  knobs: [[105, 100, 24, "MASTER", -1], [215, 100, 24, "SELECT", 0], [105, 205, 24, "PRESETS", 6], [215, 205, 24, "ALGORITHM", 1],
          [610, 100, 22, "KNOB 1", 2], [714, 100, 22, "KNOB 2", 3], [818, 100, 22, "KNOB 3", 4], [922, 100, 22, "KNOB 4", 5]],
  trays: [[50, 250, 220, 50], [568, 160, 400, 140]], oct: [[60, 258, 90, 34], [170, 258, 90, 34]],
  grid: [582, 174, 54, 48, 63, 62], keybed: [36, 332, 928, 256] };
// the buttons as printed, with the firmware's ids (BTN_OCT_DOWN 0, OCT_UP 1, FX 2, SEL 3, ENV 4, LFO 5, EDIT 6, GLO 7,
// HOME 8, SAVE 9, ARP 10, SEQ 11, PLAY 12, REC 13)
const ROWS = [[["FX", 2], ["SCL", 3], ["ENV", 4], ["LFO", 5], ["EDIT", 6], ["GLO", 7]],
              [["HOME", 8], ["SAVE", 9], ["ARP", 10], ["SEQ", 11], ["PLAY", 12], ["REC", 13]]];
const WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
const BLACK = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];
// the computer's keyboard: the bottom row is the naturals, the row above their sharps, Q to O the top nine keys
const KEYMAP = { KeyZ: 0, KeyX: 2, KeyC: 4, KeyV: 6, KeyB: 7, KeyN: 9, KeyM: 11, Comma: 12, Period: 14, Slash: 16,
  KeyS: 1, KeyD: 3, KeyF: 5, KeyH: 8, KeyJ: 10, KeyL: 13, Semicolon: 15, Quote: 17,
  KeyQ: 18, KeyW: 19, KeyE: 20, KeyR: 21, KeyT: 22, KeyY: 23, KeyU: 24, KeyI: 25, KeyO: 26 };
const BTNMAP = { Space: 12, Enter: 12, KeyG: 7, Escape: 7, KeyA: 8, Backspace: 8, KeyP: 13, BracketLeft: 0, BracketRight: 1,
  Digit5: 2, Digit6: 3, Digit7: 4, Digit8: 5, Digit9: 6, Digit0: 9, Minus: 10, Equal: 11 };   // FX SCL ENV LFO EDIT, SAVE ARP SEQ
// the "computer" layout: the computer's two bottom rows of letters are the FM-1's two rows of keys. A to ' are the
// eleven of the top row, in order; Z to / are ten of the sixteen below, spread along it, each under the key in the
// same place, so every colour lane of the games has its own (3, 2, 2, 3). The buttons on the rows above, where they
// sit on the FM-1: OCT- OCT+ at the left (Q W), its top row of buttons on 7 to =, its bottom row on U to ].
const SPREAD = [0, 2, 3, 5, 7, 8, 10, 12, 13, 15];               // (the white keys, by place, that Z to / play)
const KB_KEYS = {};
["KeyZ", "KeyX", "KeyC", "KeyV", "KeyB", "KeyN", "KeyM", "Comma", "Period", "Slash"].forEach((c, j) => { KB_KEYS[c] = WHITE[SPREAD[j]]; });
["KeyA", "KeyS", "KeyD", "KeyF", "KeyG", "KeyH", "KeyJ", "KeyK", "KeyL", "Semicolon", "Quote"].forEach((c, j) => { KB_KEYS[c] = BLACK[j]; });
const KB_BTNS = { KeyQ: 0, KeyW: 1, Digit7: 2, Digit8: 3, Digit9: 4, Digit0: 5, Minus: 6, Equal: 7,
  KeyU: 8, KeyI: 9, KeyO: 10, KeyP: 11, BracketLeft: 12, BracketRight: 13, Space: 12, Enter: 12, Escape: 7 };
const MAPS = { fm1: [KEYMAP, BTNMAP], computer: [KB_KEYS, KB_BTNS] };
const CAP = { Comma: ",", Period: ".", Slash: "/", Semicolon: ";", Quote: "'", Minus: "-", Equal: "=", BracketLeft: "[",
  BracketRight: "]", Space: "SPACE", Enter: "ENTER", Escape: "ESC", Backspace: "BKSP" };
const capOf = (code) => CAP[code] || code.replace(/^(Key|Digit)/, "");
// the knobs' own keys, the same in both layouts
const KNOB_CAP = { 0: "↑ ↓", 6: "⇧ ↑ ↓", 1: "⇧ ← →", 2: "1", 3: "2", 4: "3", 5: "4" };
const BTN_NAMES = ["OCT -", "OCT +", "FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"];

function el(tag, attrs, parent) {
  const e = document.createElementNS(NS, tag);
  for (const k in attrs) e.setAttribute(k, attrs[k]);
  if (parent) parent.appendChild(e);
  return e;
}

export function mountFm1(host, on) {
  host.classList.add("fm1");
  const svg = el("svg", { viewBox: `0 0 ${L.w} ${L.h}`, role: "img", "aria-label": "FM-1" }, host);
  const defs = el("defs", {}, svg);
  defs.innerHTML = `<linearGradient id="fmBody" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#23262b"/><stop offset="1" stop-color="#15171a"/></linearGradient>
    <linearGradient id="fmKnob" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3a3e45"/><stop offset="1" stop-color="#1c1e22"/></linearGradient>
    <linearGradient id="fmKeyW" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f0f1ee"/><stop offset="1" stop-color="#cfd2ce"/></linearGradient>
    <linearGradient id="fmKeyB" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2a2d32"/><stop offset="1" stop-color="#17191c"/></linearGradient>`;
  el("rect", { x: 2, y: 2, width: L.w - 4, height: L.h - 4, rx: 40, fill: "url(#fmBody)", stroke: "#34383f", "stroke-width": 2 }, svg);
  for (const t of L.trays) el("rect", { x: t[0], y: t[1], width: t[2], height: t[3], rx: 14, fill: "#101214", stroke: "#2a2d33" }, svg);
  el("rect", { x: L.keybed[0], y: L.keybed[1], width: L.keybed[2], height: L.keybed[3], rx: 18, fill: "#0c0d0f", stroke: "#2a2d33" }, svg);
  el("rect", { x: L.screen[0] - 14, y: L.screen[1] - 14, width: L.screen[2] + 28, height: L.screen[2] + 28, rx: 18, fill: "#050506", stroke: "#2a2d33" }, svg);

  // the screen: a canvas laid over the bezel
  const cv = document.createElement("canvas");
  cv.width = cv.height = 240; cv.className = "fm1-screen";
  cv.style.left = (L.screen[0] / L.w * 100) + "%"; cv.style.top = (L.screen[1] / L.h * 100) + "%";
  cv.style.width = (L.screen[2] / L.w * 100) + "%"; cv.style.height = (L.screen[2] / L.h * 100) + "%";
  host.appendChild(cv);
  const ctx = cv.getContext("2d"), img = ctx.createImageData(240, 240);

  let btnMask = 0, keyMask = 0, activeKnob = 2;
  const btnEls = {}, keyEls = [], knobEls = {}, knobAng = {};
  const setBtn = (id, down) => { const m = down ? btnMask | (1 << id) : btnMask & ~(1 << id); if (m !== btnMask) { btnMask = m; on.buttons(m); } if (btnEls[id]) btnEls[id].classList.toggle("down", down); };
  const setKey = (k, down) => { const m = down ? keyMask | (1 << k) : keyMask & ~(1 << k); if (m !== keyMask) { keyMask = m; on.keys(m); } keyEls[k].classList.toggle("down", down); };
  // (the strip's own elements are made further down; set...() only ever runs after that)
  const turn = (id, n) => {
    if (id < 0 || !n) return;
    on.enc(id, n);
    knobAng[id] = (knobAng[id] || 0) + n * 15;
    knobEls[id].tick.setAttribute("transform", `rotate(${knobAng[id]} ${knobEls[id].x} ${knobEls[id].y})`);
  };
  const press = (node, down, up) => {
    node.addEventListener("pointerdown", (e) => { e.preventDefault(); node.setPointerCapture(e.pointerId); down(e); });
    const end = (e) => { if (node.hasPointerCapture(e.pointerId)) node.releasePointerCapture(e.pointerId); up(e); };
    node.addEventListener("pointerup", end); node.addEventListener("pointercancel", end);
  };

  for (const k of L.knobs) {
    const g = el("g", { class: "fm1-knob" + (k[4] < 0 ? " master" : "") }, svg);
    el("circle", { cx: k[0], cy: k[1], r: k[2] + 7, fill: "#0e0f11", stroke: "#2a2d33" }, g);
    el("circle", { cx: k[0], cy: k[1], r: k[2], fill: "url(#fmKnob)", stroke: "#4a4f57" }, g);
    const tick = el("rect", { x: k[0] - 1.5, y: k[1] - k[2] + 3, width: 3, height: k[2] * 0.55, rx: 1.5, fill: "#e6e9ec" }, g);
    const t = el("text", { x: k[0], y: k[1] + k[2] + 26, "text-anchor": "middle", class: "fm1-label" }, g); t.textContent = k[3];
    if (k[4] >= 0) { const c = el("text", { x: k[0], y: k[1] + k[2] + 45, "text-anchor": "middle", class: "fm1-cap" }, g); c.textContent = KNOB_CAP[k[4]]; }
    const ring = el("circle", { cx: k[0], cy: k[1], r: k[2] + 11, fill: "none", stroke: "#7fe6d2", "stroke-width": 2, class: "fm1-active" }, g);
    if (k[4] < 0) continue;
    knobEls[k[4]] = { tick, x: k[0], y: k[1], ring };
    let last = null, acc = 0;
    g.addEventListener("pointerdown", (e) => { e.preventDefault(); g.setPointerCapture(e.pointerId); last = [e.clientX, e.clientY]; acc = 0; pick(k[4]); });
    g.addEventListener("pointermove", (e) => {
      if (!last) return;
      acc += (e.clientX - last[0]) - (e.clientY - last[1]); last = [e.clientX, e.clientY];
      const n = Math.trunc(acc / 9); if (n) { acc -= n * 9; turn(k[4], n); }
    });
    const up = () => { last = null; }; g.addEventListener("pointerup", up); g.addEventListener("pointercancel", up);
    g.addEventListener("wheel", (e) => { e.preventDefault(); turn(k[4], (e.deltaY || e.deltaX) > 0 ? -1 : 1); pick(k[4]); }, { passive: false });
  }
  function pick(id) { activeKnob = id; for (const i in knobEls) knobEls[i].ring.classList.toggle("on", +i === id); }
  pick(2);

  const btnText = {};
  const mkBtn = (x, y, w, h, label, id) => {
    const g = el("g", { class: "fm1-btn" + (id === 13 ? " rec" : "") }, svg);
    el("rect", { x, y, width: w, height: h, rx: 8, fill: "#1b1d21", stroke: "#3a3e45" }, g);
    el("rect", { x: x + 4, y: y + 4, width: w - 8, height: 5, rx: 2.5, class: "fm1-lamp" }, g);
    const t = el("text", { x: x + w / 2, y: y + h / 2 + 9, "text-anchor": "middle", class: "fm1-label" }, g); t.textContent = label;
    // its computer key, drawn as a keycap: under the name on the grid, beside it on the low OCT buttons
    const side = h < 40, cx = side ? x + w - 22 : x + w / 2, cy = side ? y + 12 : y + h - 21;
    const box = el("rect", { x: cx - 13, y: cy, width: 26, height: 18, rx: 4, class: "fm1-capbox" }, g);
    const c = el("text", { x: cx, y: cy + 14, "text-anchor": "middle", class: "fm1-cap key" }, g);
    btnText[id] = { t, c, box, x0: x + w / 2, x1: side ? x + w / 2 - 14 : x + w / 2, y0: y + h / 2 + 9, y1: side ? y + h / 2 + 9 : y + 24 };
    btnEls[id] = g;
    press(g, () => setBtn(id, true), () => setBtn(id, false));
  };
  mkBtn(...L.oct[0], "OCT -", 0); mkBtn(...L.oct[1], "OCT +", 1);
  ROWS.forEach((row, r) => row.forEach((b, c) => mkBtn(L.grid[0] + c * L.grid[4], L.grid[1] + r * L.grid[5], L.grid[2], L.grid[3], b[0], b[1])));

  // the keys: naturals below, sharps above and between them
  const kb = L.keybed, kw = (kb[2] - 24) / 16, x0 = kb[0] + 12, keyCap = [];
  const mkKey = (k, x, y, w, h, black) => {
    const g = el("g", { class: "fm1-key" + (black ? " black" : "") }, svg);
    el("rect", { x, y, width: w, height: h, rx: w / 2, fill: black ? "url(#fmKeyB)" : "url(#fmKeyW)", stroke: black ? "#3a3e45" : "#9a9d98" }, g);
    el("rect", { x: x + w / 2 - 2.5, y: y + 16, width: 5, height: 26, rx: 2.5, class: "fm1-slit" }, g);
    keyCap[k] = el("text", { x: x + w / 2, y: y + h - 16, "text-anchor": "middle", class: "fm1-kcap" + (black ? " black" : "") }, g);
    keyEls[k] = g;
    press(g, () => setKey(k, true), () => setKey(k, false));
  };
  WHITE.forEach((k, i) => mkKey(k, x0 + i * kw + 4, kb[1] + 134, kw - 8, 108, false));
  BLACK.forEach((k) => { const i = WHITE.indexOf(k + 1); mkKey(k, x0 + i * kw - (kw - 8) / 2, kb[1] + 14, kw - 8, 108, true); });

  // the big screen's strip: the keys' lights in their two rows, and the buttons' (pressable, like the panel)
  const strip = document.createElement("div"); strip.className = "fm1-strip"; host.appendChild(strip);
  const rowT = document.createElement("div"), rowW = document.createElement("div"), rowB = document.createElement("div");
  rowT.className = "fm1-srow"; rowW.className = "fm1-srow"; rowB.className = "fm1-sbtns"; strip.append(rowT, rowW, rowB);
  const sKeys = [], sBtns = [];
  const mkS = (row, cls, id, isKey) => {
    const s = document.createElement("span"); s.className = cls; s.innerHTML = "<i></i><b></b>"; row.appendChild(s);
    press(s, () => (isKey ? setKey(id, true) : setBtn(id, true)), () => (isKey ? setKey(id, false) : setBtn(id, false)));
    return s;
  };
  WHITE.forEach((k, i) => { sKeys[k] = mkS(rowW, "fm1-sk", k, true); sKeys[k].style.gridColumn = i + 1; });
  BLACK.forEach((k) => { const i = WHITE.indexOf(k + 1); sKeys[k] = mkS(rowT, "fm1-sk black", k, true); sKeys[k].style.gridColumn = i + 1; });
  for (let id = 0; id < 14; id++) { sBtns[id] = mkS(rowB, "fm1-sb" + (id === 13 ? " rec" : ""), id, false); sBtns[id].firstChild.nextSibling.dataset.name = BTN_NAMES[id]; }
  const exit = document.createElement("button"); exit.className = "fm1-exit"; exit.textContent = "EXIT BIG SCREEN"; host.appendChild(exit);
  exit.addEventListener("click", () => { exit.blur(); host.classList.remove("big"); on.bigOff?.(); });

  let maps = MAPS.fm1;
  function layout(mode) {
    maps = MAPS[mode] || MAPS.fm1;
    const kc = mode === "computer";
    host.classList.toggle("kbd", kc);
    const keyCode = {}, btnCode = {};
    for (const c in maps[0]) if (!(maps[0][c] in keyCode)) keyCode[maps[0][c]] = c;
    for (const c in maps[1]) if (!(maps[1][c] in btnCode)) btnCode[maps[1][c]] = c;
    for (let k = 0; k < 27; k++) {
      const t = kc && k in keyCode ? capOf(keyCode[k]) : "";
      keyCap[k].textContent = t; sKeys[k].lastChild.textContent = t;
    }
    for (const id in btnText) {
      const b = btnText[id], t = kc && id in btnCode ? capOf(btnCode[id]) : "";
      b.c.textContent = t; b.t.setAttribute("y", kc ? b.y1 : b.y0); b.t.setAttribute("x", kc ? b.x1 : b.x0);
      b.box.style.display = t ? "" : "none";
      sBtns[id].lastChild.textContent = t;
    }
  }
  layout("fm1");

  return {
    layout,
    big(onOff) { host.classList.toggle("big", !!onOff); },
    screen(fb) {
      const d = img.data;
      for (let i = 0, o = 0; i < 57600; i++, o += 4) {
        const c = fb[i];
        d[o] = (c >> 11) * 255 / 31; d[o + 1] = ((c >> 5) & 63) * 255 / 63; d[o + 2] = (c & 31) * 255 / 31; d[o + 3] = 255;
      }
      ctx.putImageData(img, 0, 0);
    },
    leds(b, k, dim, br = 0) {
      for (const id in btnEls) { btnEls[id].classList.toggle("lit", !!(b >> id & 1)); if (+id !== 12) btnEls[id].classList.toggle("pulse", !!(br >> id & 1)); }
      btnEls[12].classList.toggle("green", !!(b >> 14 & 1));          // PLAY's green light: lit, or breathing
      btnEls[12].classList.toggle("breathe", !!(b >> 15 & 1));
      keyEls.forEach((g, i) => {
        const lit = !!(k >> i & 1), dm = !!(dim >> i & 1) && !lit;
        g.classList.toggle("lit", lit); g.classList.toggle("dim", dm);
        sKeys[i].classList.toggle("lit", lit); sKeys[i].classList.toggle("dim", dm);
      });
      for (let id = 0; id < 14; id++) {
        sBtns[id].classList.toggle("lit", !!(b >> id & 1) || (id === 12 && !!(b >> 14 & 1)));
        sBtns[id].classList.toggle("pulse", id === 12 ? !!(b >> 15 & 1) && !(b >> 14 & 1) : !!(br >> id & 1));
        sBtns[id].classList.toggle("green", id === 12);
      }
    },
    // the computer's keyboard; returns true when the key was one of ours
    key(e, down) {
      if (e.metaKey || e.ctrlKey || e.altKey) return false;
      const c = e.code;
      if (c in maps[0]) { if (!e.repeat) setKey(maps[0][c], down); return true; }
      if (c in maps[1]) { if (!e.repeat) setBtn(maps[1][c], down); return true; }
      if (!down) return c.startsWith("Arrow") || /^Digit[1-4]$/.test(c);
      if (/^Digit[1-4]$/.test(c)) { pick(+c[5] + 1); return true; }
      if (c === "ArrowLeft" || c === "ArrowRight") { turn(e.shiftKey ? 1 : activeKnob, c === "ArrowRight" ? 1 : -1); return true; }
      if (c === "ArrowUp" || c === "ArrowDown") { turn(e.shiftKey ? 6 : 0, c === "ArrowDown" ? 1 : -1); return true; }
      return false;
    },
    releaseAll() { for (let i = 0; i < 27; i++) if (keyMask >> i & 1) setKey(i, false); for (let i = 0; i < 14; i++) if (btnMask >> i & 1) setBtn(i, false); },
  };
}

export const FM1_CSS = `
.fm1{position:relative;width:100%;aspect-ratio:1000/624;user-select:none;-webkit-user-select:none;touch-action:none}
.fm1 svg{display:block;width:100%;height:100%}
.fm1-screen{position:absolute;image-rendering:pixelated;background:#000;border-radius:2px}
.fm1-label{font:400 13px/1 "Silkscreen",ui-monospace,Menlo,monospace;fill:#7d8590;letter-spacing:.04em;pointer-events:none}
.fm1-knob,.fm1-btn,.fm1-key{cursor:pointer}
.fm1-knob.master{cursor:default;opacity:.7}
.fm1-active{opacity:0;transition:opacity .15s}
.fm1-active.on{opacity:.55}
.fm1-lamp{fill:#2a2d33}
.fm1-btn.lit .fm1-lamp{fill:#fff1d6;filter:drop-shadow(0 0 6px #fff1d6)}
.fm1-btn.green .fm1-lamp{fill:#5fe08a;filter:drop-shadow(0 0 6px #5fe08a)}
.fm1-btn.breathe .fm1-lamp{fill:#5fe08a;filter:drop-shadow(0 0 6px #5fe08a);animation:fm1breath 2.25s ease-in-out infinite}
@keyframes fm1breath{0%,100%{opacity:.05}50%{opacity:1}}
.fm1-btn.pulse .fm1-lamp{fill:#fff1d6;filter:drop-shadow(0 0 6px #fff1d6);animation:fm1breath 2.25s ease-in-out infinite}
.fm1-btn.down rect:first-child{fill:#2a2d33}
.fm1-slit{fill:#7d776a;opacity:.35}
.fm1-key.black .fm1-slit{fill:#55524a}
.fm1-key.dim .fm1-slit{fill:#fff1d6;opacity:.45}
.fm1-key.lit .fm1-slit{fill:#fff1d6;opacity:1;filter:drop-shadow(0 0 7px #fff1d6)}
.fm1-key.down rect:first-child{transform:translateY(3px);filter:brightness(.82)}
.fm1-btn.rec.lit .fm1-lamp,.fm1-btn.rec.pulse .fm1-lamp{fill:#ff4d4d;filter:drop-shadow(0 0 6px #ff4d4d)}
.fm1-cap,.fm1-kcap,.fm1-capbox{display:none;pointer-events:none}
.fm1-cap{font:400 15px/1 "Silkscreen",ui-monospace,Menlo,monospace;fill:#7fe6d2;letter-spacing:.04em}
.fm1-cap.key{font-size:14px;fill:#0b0c0e}
.fm1-capbox{fill:#7fe6d2}
.fm1-kcap{font:400 22px/1 "Silkscreen",ui-monospace,Menlo,monospace;fill:#2a2e35}
.fm1-kcap.black{fill:#e6e9ec}
.fm1.kbd .fm1-cap,.fm1.kbd .fm1-kcap,.fm1.kbd .fm1-capbox{display:inline}
.fm1-strip,.fm1-exit{display:none}
.fm1.big{position:fixed;inset:0;z-index:100;width:auto;height:auto;aspect-ratio:auto;background:#000;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:14px;padding:12px;box-sizing:border-box}
.fm1.big svg{display:none}
.fm1.big .fm1-screen{position:static!important;width:min(calc(100vw - 24px),calc(100vh - 170px))!important;height:auto!important;aspect-ratio:1;border-radius:0}
.fm1.big .fm1-strip{display:grid;gap:5px;width:min(calc(100vw - 24px),780px)}
.fm1.big .fm1-exit{display:block;position:absolute;top:12px;right:12px;font:400 11px/1 "Silkscreen",ui-monospace,monospace;letter-spacing:.08em;color:#7fe6d2;background:#000;border:1px solid #7fe6d2;padding:9px 12px;cursor:pointer}
.fm1-srow{display:grid;grid-template-columns:repeat(16,1fr);gap:4px;height:26px}
.fm1-sbtns{display:grid;grid-template-columns:repeat(14,1fr);gap:4px;margin-top:4px}
.fm1-sk,.fm1-sb{position:relative;display:flex;align-items:center;justify-content:center;border-radius:13px;background:#d9dbd7;cursor:pointer;font:400 10px/1 "Silkscreen",ui-monospace,monospace;color:#3b4048}
.fm1-sk.black{background:#24272c;color:#c9ccd1;transform:translateX(-50%);margin:0 6px}
.fm1-sk i{position:absolute;left:50%;top:5px;width:4px;height:7px;margin-left:-2px;border-radius:2px;background:#7d776a;opacity:.35}
.fm1-sk b{font-weight:400;margin-top:9px}
.fm1-sk.dim i{background:#fff1d6;opacity:.5}
.fm1-sk.lit i{background:#fff1d6;opacity:1;box-shadow:0 0 7px #fff1d6}
.fm1-sb{flex-direction:column;gap:3px;border-radius:6px;background:#1b1d21;border:1px solid #3a3e45;color:#7fe6d2;padding:4px 0 5px;min-height:34px}
.fm1-sb i{width:70%;height:4px;border-radius:2px;background:#2a2d33}
.fm1-sb b{font-weight:400}
.fm1-sb b::before{content:attr(data-name);display:block;color:#7d8590;margin-bottom:3px}
.fm1-sb.lit i,.fm1-sb.pulse i{background:#fff1d6;box-shadow:0 0 6px #fff1d6}
.fm1-sb.pulse i{animation:fm1breath 2.25s ease-in-out infinite}
.fm1-sb.green.lit i,.fm1-sb.green.pulse i{background:#5fe08a;box-shadow:0 0 6px #5fe08a}
.fm1-sb.rec.lit i,.fm1-sb.rec.pulse i{background:#ff4d4d;box-shadow:0 0 6px #ff4d4d}
`;
