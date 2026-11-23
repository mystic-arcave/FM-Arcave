// SPDX-License-Identifier: GPL-3.0-only
// UNDERSONG's messages as a relay passes them on (docs/LINK.md): control changes 110 to 118 on channel 16, and the
// clock of the FM-1 that leads the game (102 to 106, then 107 with 13). One Gather for each FM-1 (or emulator) heard.
// Both Link pages use it: the LOCAL one between FM-1s on this computer, the ONLINE one between computers.
export class Gather {
  constructor() { this.tempo = 0; this.pos = 0; this.held = null; }
  // one control change from this FM-1: what goes on now, as [cc, v, cc, v, ...], or null. A message with numbers
  // (118, 115, 116, then its 117) goes on in one piece, so two players' messages never mix on the way to a third
  take(cc, v) {
    if (cc >= 110 && cc <= 118) {
      if (cc === 115 || cc === 116 || cc === 118) { (this.held ||= []).push(cc, v); if (this.held.length > 16) this.held.splice(0, 2); return null; }
      if (cc === 117 && this.held) { const o = this.held; this.held = null; o.push(cc, v); return o; }
      return [cc, v];
    }
    if (cc === 102) this.tempo = v << 7;
    else if (cc === 103) this.tempo |= v;
    else if (cc === 104) this.pos = v << 14;
    else if (cc === 105) this.pos |= v << 7;
    else if (cc === 106) this.pos |= v;
    else if (cc === 107 && v === 13) {                    // the game's leader says where its clock is: all of it, in one piece
      const t = this.tempo, p = this.pos;
      return [102, t >> 7 & 127, 103, t & 127, 104, p >> 14 & 127, 105, p >> 7 & 127, 106, p & 127, 107, 13];
    }
    return null;
  }
}
// "I am here" (110): an FM-1 that sends it is down the well, in UNDERSONG
export const HELLO = 110;

// a port's name without what tells an input from an output ("MIDIIN2 (FM-ARCAVE)", "FM-ARCAVE 3F2A Bluetooth MIDI
// OUT"): some systems name the two ends of one device differently
export const portBase = (name) => String(name || "").toLowerCase().replace(/\bmidi\s*(in|out)\s*\d*/g, " ")
  .replace(/\b(in|out|input|output)\b/g, " ").replace(/[()[\]]/g, " ").replace(/\s+/g, " ").trim();
// the MIDI output that belongs to an input. Two FM-1s by USB carry the same port name: the n-th input of a name goes
// with the n-th output of that name (two by Bluetooth through the computer's own MIDI differ by their four
// characters). Where no output has the input's exact name, the names are compared without their IN and OUT.
// proven (the LOCAL hub's: input id -> output): pairs a ping's answer has shown; they are kept, and their outputs
// are no other input's
export function outFor(midi, input, proven) {
  const all = [...midi.outputs.values()], hit = proven?.get(input.id);
  if (hit && all.includes(hit)) return hit;
  const taken = new Set();
  if (proven) for (const [id, o] of proven) if (id !== input.id) taken.add(o);
  for (const same of [(x) => x.name === input.name, (x) => portBase(x.name) === portBase(input.name)]) {
    const ins = [...midi.inputs.values()].filter((x) => same(x) && !proven?.has(x.id)), outs = all.filter((x) => same(x) && !taken.has(x));
    if (outs.length) return outs[Math.max(0, ins.indexOf(input))] || outs[0];
  }
  return null;
}
// one Link page at a time in a browser passes UNDERSONG on: two would pass every message twice. The LOCAL hub and the
// ONLINE lobby both hold this lock while they run; fn(true) once it is ours, fn(false) when another tab has it.
// The lock is let go by calling what this returns
export function holdTheLink(fn) {
  if (!globalThis.navigator?.locks) { fn(true); return () => {}; }
  let let_go = null;
  const gone = new Promise((r) => { let_go = r; });
  navigator.locks.request("fm-arcave-link-hub", { ifAvailable: true }, (lock) => { fn(!!lock); return lock ? gone : null; });
  return () => let_go();
}
