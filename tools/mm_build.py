#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The mystic machines and the music room's instruments that are loaded, not built in: each firmware/machines/NAME.c
and firmware/instruments/NAME.c as a file the MACHINES page sends to an FM-1, and the catalogue the page shows.

    tools/mm_build.py [--browser-only]   -> build/machines/NAME.mmach, NAME.minst, build/machines/catalog.json

(An instrument is built exactly as a machine is, and what follows holds for both; its header is mi.h's.)

A machine is compiled alone (it reaches the firmware only through mm_api_t: firmware/src/arcave/mm.h) and linked for
slot 0 of the machine bank (firmware/machine.ld), its header first. It is linked a second time two slots on: every
32-bit value that moved by exactly that much and points into the machine is an address in it, at whatever halfword it
sits (pi32v2 loads a 32-bit constant with a 48-bit instruction, the value at its second halfword). The file lists
those places, and the page adds the slot's distance to each before sending, so one file goes into any slot. Anything
else that moved (an address split across instructions) fails the build: such a machine could only run in slot 0.

    NAME.mmach   the image as slot 0 holds it (its size and CRC-32 written into its header), then the places (u32
                 each), their count, "MMRL", the image's length (u32 each, little-endian)

--browser-only (or no JieLi toolchain): the files hold a header and no code. The browser game takes them (it has every
machine built in and only asks which one a slot holds); an FM-1 refuses them, and the catalogue says so ("fm1": false).

MACHINES and INSTRUMENTS below are the catalogue: add yours there (docs/MACHINES.md)."""
import json
import os
import re
import struct
import sys
import zlib
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "tools"))
FW, OUT = SRC / "firmware", SRC / "build"
MM_H = (FW / "src" / "arcave" / "mm.h").read_text() + (FW / "src" / "arcave" / "mi.h").read_text()


def mm(name):
    m = re.search(rf"#define {name} (0x[0-9A-Fa-f]+|\d+)u", MM_H)
    if not m:
        raise SystemExit(f"mm.h, mi.h: no {name}")
    return int(m[1], 0)


API, MAGIC, NSLOT, SLOT, BASE, STATE_MAX, HEAD = (mm(n) for n in
                                                  ("MM_API", "MM_MAGIC", "MM_NSLOT", "MM_SLOT", "MM_BASE", "MM_STATE_MAX", "MM_HEAD"))
MOVE = (NSLOT - 1) * SLOT                      # the second link: two slots on
I_API, I_MAGIC, I_NSLOT, I_STATE_MAX = (mm(n) for n in ("MI_API", "MI_MAGIC", "MI_NSLOT", "MI_STATE_MAX"))
# what differs between the two kinds: the folder, the file's ending, the object's name, the first word, the API, the
# most RAM, and the places in the header (after its first twenty bytes) that must hold addresses inside the file
KINDS = {
    "machine": ("machines", "mmach", "mm_machine_", MAGIC, API, STATE_MAX, {HEAD + 4 + 4 * k for k in range(6)}),
    "instrument": ("instruments", "minst", "mi_instrument_", I_MAGIC, I_API, I_STATE_MAX, {HEAD + 4 * k for k in range(7)}),
}
RELOC = 0x4C524D4D                             # "MMRL"

# the catalogue: (file name, its number, its name, its version, a line about it)
MACHINES = [
    ("glowcaps", 16, "GLOW CAPS", 1,
     "Sixteen caps, one for each key of the bottom row. A cap glows before its turn and a ring closes on it: press its "
     "key as they meet and it sings its note of the tune. On the beat, then faster."),
]
INSTRUMENTS = [
    ("spring", 16, "THE SPRING", 1,
     "Warm water over sixteen vents. A vent lets a bubble go when its pressure is up; the bubble is silent until it "
     "meets the surface and pops. A current carries bubbles into each other, lily pads hold them for the beat, and a "
     "frog jumps in on time."),
]


def sealed(img):
    """the image with its size and its CRC-32 (of all but the CRC's own four bytes) written into its header"""
    b = bytearray(img)
    struct.pack_into("<I", b, 12, len(b))
    struct.pack_into("<I", b, 16, zlib.crc32(bytes(b[:16]) + bytes(b[20:])) & 0xFFFFFFFF)
    return bytes(b)


def as_file(img, rel):
    return sealed(img) + struct.pack(f"<{len(rel)}I", *rel) + struct.pack("<III", len(rel), RELOC, len(img))


def build_one(kind, name, mid, title, ver):
    import build as B                         # (tools/build.py: the toolchain's runner and the app's flags)
    folder, ext, sym, MAGIC, API, STATE_MAX, want = KINDS[kind]
    o = OUT / "machines" / f"{name}.o"
    B.tc("cc", *B.CFLAGS, "-fdata-sections", "-ffreestanding", "-DMM_MODULE=1", "-Ifirmware/src", "-c",
         FW / folder / f"{name}.c", "-o", o)
    img = []
    for k, base in enumerate((BASE, BASE + MOVE)):
        ld = (FW / "machine.ld").read_text()
        if ld.count("ORIGIN = 0x02093000") != 1 or ld.count("LENGTH = 0xA000") != 1 or BASE != 0x02093000 or SLOT != 0xA000:
            raise SystemExit("machine.ld and mm.h disagree about the bank (MM_BASE, MM_SLOT)")
        (OUT / "machines" / f"machine_{k}.ld").write_text(ld.replace("ORIGIN = 0x02093000", f"ORIGIN = 0x{base:08X}"))
        elf, out = OUT / "machines" / f"{name}_{k}.elf", OUT / "machines" / f"{name}_{k}.bin"
        # (a call to anything outside the file fails the link here: nothing else is linked in)
        B.tc("pi32v2/bin/ld", "--gc-sections", "-e", f"{sym}{name}", "-T", OUT / "machines" / f"machine_{k}.ld", o, "-o", elf)
        for ln in B.tc("common/bin/objdump", "-h", elf).splitlines():
            f = ln.split()
            if len(f) > 3 and f[1].startswith((".data", ".sdata", ".bss", ".sbss")) and int(f[2], 16):
                raise SystemExit(f"{kind} {name}: {f[1]} of {int(f[2], 16)} B: a machine keeps no variables of its own "
                                 f"(its RAM is st, as large as its header's state says)")
        B.tc("common/bin/objcopy", "-O", "binary", "-j", ".machine", elf, out)
        img.append(out.read_bytes())
    b, m = img
    if len(b) > SLOT or len(b) < HEAD + 28 or struct.unpack_from("<I", b)[0] != MAGIC:
        raise SystemExit(f"{kind} {name}: {len(b)} B (a slot holds {SLOT}), or its header is not first")
    if len(m) != len(b) or len(b) % 4:
        raise SystemExit(f"{kind} {name}: linked elsewhere it is {len(m)} B, not {len(b)}")
    api, state, hid, hver = struct.unpack_from("<HHHH", b, 4)
    if api != API or hid != mid or hver != ver or not 0 < state <= STATE_MAX:
        raise SystemExit(f"{kind} {name}: its header says API {api}, number {hid}, version {hver}, {state} B of RAM; "
                         f"the catalogue (tools/mm_build.py) says number {mid}, version {ver}; API {API}, RAM up to {STATE_MAX}")
    rel, at = [], 0
    for i in range(len(b)):
        if i < at or b[i] == m[i]:
            continue
        lo = max(at, i - 3)
        for w in range(lo + (lo & 1), i + 1, 2):
            if w + 4 > len(b):
                continue
            x, y = struct.unpack_from("<I", b, w)[0], struct.unpack_from("<I", m, w)[0]
            if (y - x) & 0xFFFFFFFF == MOVE and BASE <= x < BASE + len(b):
                rel.append(w)
                at = w + 4
                break
        else:
            dis = [ln for ln in B.tc("common/bin/objdump", "-d", OUT / "machines" / f"{name}_0.elf").splitlines()
                   if (a := re.match(r"\s*([0-9a-f]+):", ln)) and abs(int(a[1], 16) - BASE - i) <= 12]
            raise SystemExit(f"{kind} {name}: the byte at {i:#x} moved, in no address inside the machine (one split "
                             f"across instructions?): it could only run in slot 0\n" + "\n".join(dis))
    # the header's own first addresses (a machine's name, help, sign, start, tick, draw; an instrument's three names and
    # four knobs) are among them, or it is not one
    if not want <= set(rel):
        raise SystemExit(f"{kind} {name}: its header's addresses are not all inside it ({sorted(want - set(rel))})")
    (OUT / "machines" / f"{name}.{ext}").write_bytes(as_file(b, rel))
    return len(b), len(rel), state


def twin_one(kind, name, mid, title, ver):
    """a header and no code: the browser game's (it has the machine built in)"""
    folder, ext, sym, MAGIC, API, STATE_MAX, want = KINDS[kind]
    b = struct.pack("<IHHHHII", MAGIC, API, 0, mid, ver, 0, 0) + title.encode()[:20].ljust(20, b"\0")
    (OUT / "machines" / f"{name}.{ext}").write_bytes(as_file(b, []))
    return len(b)


def main():
    (OUT / "machines").mkdir(parents=True, exist_ok=True)
    tcdir = os.environ.get("JIELI_TOOLCHAIN", str(Path.home() / ".jieli" / "toolchain"))
    os.environ.setdefault("JIELI_TOOLCHAIN", tcdir)
    fm1 = "--browser-only" not in sys.argv and (Path(tcdir) / "pi32v2" / "bin" / "clang").exists()
    cats = {}
    for kind, entries in (("machine", MACHINES), ("instrument", INSTRUMENTS)):
        cat = cats[kind] = []
        for name, mid, title, ver, about in entries:
            if not re.fullmatch(r"[A-Z0-9 ']{1,20}", title) or not 16 <= mid < 32768:
                raise SystemExit(f"{kind} {name}: a name of 1 to 20 capitals, a number from 16 up")
            if fm1:
                n, nrel, state = build_one(kind, name, mid, title, ver)
                print(f"{kind:<8} {name}: {n} B of a slot's {SLOT}, {nrel} addresses, {state} B of RAM")
            else:
                n = twin_one(kind, name, mid, title, ver)
                print(f"{kind:<8} {name}: for the browser game only ({n} B, no code: no FM-1 toolchain)")
            cat.append({"id": mid, "name": title, "ver": ver, "file": f"{name}.{KINDS[kind][1]}", "bytes": n, "fm1": fm1, "about": about})
    (OUT / "machines" / "catalog.json").write_text(json.dumps(
        {"api": API, "slots": NSLOT, "slot": SLOT, "machines": cats["machine"],
         "iapi": I_API, "places": I_NSLOT, "instruments": cats["instrument"]}, indent=1) + "\n")
    print(f"machines {OUT / 'machines' / 'catalog.json'} ({len(cats['machine'])} machines, API {API}; "
          f"{len(cats['instrument'])} instruments, API {I_API})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
