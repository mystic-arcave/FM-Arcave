#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The intro course of FM ARCAVE as tables: firmware/gen/arcave_course.h.

The land and everything on it were tuned in floating point in the browser prototype
(mystic-arcave-intro.html). The FM-1 has no FPU, so the shape is worked out here, with the
prototype's own formulas, and the firmware reads it from flash.

    python3 tools/gen_course.py
"""
import math
from pathlib import Path

TAU = math.pi * 2


def sm(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def hash_(i):
    s = math.sin(i * 127.1 + 11.7) * 43758.5453
    return s - math.floor(s)


Y0, XS, L, SL = 300, 130, 15800, 0.062
XE = L - 260
XSTOP = L - 4
WALLX, LEDGE, XMIN, LOOPX, LOOPR, CARDX, CAMMIN = 18, Y0 - 58, -690, -626, 32, -377, -770


def base(x):
    if x < 18:
        u = 18 - x
        return (Y0 - 58 + 150 * sm(60, 320, u) - 62 * sm(470, 600, u)
                + 4 * math.sin(u / 21) * sm(80, 200, u) * (1 - sm(300, 340, u)))
    y = (Y0 + 90 * sm(XS, XS + 120, x) + 110 * sm(L * .3, L * .3 + 170, x)
         + 130 * sm(L * .58, L * .58 + 170, x) + 150 * sm(L * .84, L * .84 + 190, x))
    u = max(0, x - (XS + 60))
    run = XE - 200 - (XS + 60)
    if u <= run:
        y += SL * u
    else:
        w = min(u - run, 200)
        y += SL * run + SL * (w - w * w / 400)
    return y


def Y(x):
    e = sm(XS + 120, XS + 300, x) * (1 - sm(XE - 260, XE - 40, x))
    if e <= 0:
        return base(x)
    A = (23 + 10 * math.sin(x / 300 + 1)) * (1 + .2 * sm(L * .45, L * .62, x))
    ridge = 1.5 * math.sin(x / 9) * sm(L * .2, L * .27, x) * (1 - sm(L * .46, L * .54, x))
    swoop = 18 * math.sin(x / 190) * sm(L * .7, L * .8, x)
    return base(x) + e * (A * math.sin(x / 40) + 2 * math.sin(x / 15 + 2) + 9 * math.sin(x / 97) + ridge + swoop)


def dY(x):
    return max(-3, min(3, Y(x + .5) - Y(x - .5)))


# mushrooms on the course
SHROOMS = []
x, i = 520, 0
while x < L - 30:
    p = (x - 520) / (L - 520)
    dens = .03 + .9 * p * p
    if hash_(i) < dens:
        SHROOMS.append((x + hash_(i + 500) * 14, .16 + .3 * hash_(i + 900) + .2 * p * hash_(i + 40),
                        int(hash_(i + 300) * 5), hash_(i + 77) * TAU))
    x += 22
    i += 1
for i, m in enumerate([[-66, .5, 0], [-52, .3, 2], [-40, .42, 1], [-30, .26, 3], [-20, .36, 4], [-10, .22, 0]]):
    SHROOMS.append((L + m[0] + 4, m[1], m[2], float(i)))

# launch pads
PADS = []
x = XS + 420
while x < XE - 320:
    if Y(x) < Y(x - 2) and Y(x) <= Y(x + 2) and Y(x - 40) - Y(x) > 5:
        f = x + 20
        while f < x + 400 and Y(f + 2) >= Y(f):
            f += 2
        PADS.append((x - 24, f))
        x += 120
    x += 2

# in the sky
SKYOBJ = []   # type 0 spore, 1 jelly, 2 ring
for i, (px, far) in enumerate(PADS):
    gy = Y(far)
    for k in range(1, 5):
        SKYOBJ.append((0, px + k * (far - px + 60) / 5, Y(px) - 20 - 50 * math.sin(math.pi * k / 5) + k * 4, (i + k) % 5))
    if i % 3 == 1:
        bx = far + 10
        for j, q in enumerate([[0, 95], [120, 175], [245, 250]]):
            SKYOBJ.append((1 if j < 2 else 2, bx + q[0], gy - q[1], (i + j) % 5))
        for k in range(5):
            SKYOBJ.append((0, bx + 60 + k * 70, gy - 140 - k * 45, (i + k) % 5))

# on the ground: 0 pal, 1 crystal, 2 gate, 3 lamp, 4 sign
PROPS = []
x, i = 900, 0
while x < XE - 200:
    p, h, px = x / L, hash_(i + 2000), x + hash_(i + 2100) * 50
    if h < .07:
        PROPS.append((0, px, int(hash_(i + 5) * 5), int((.7 + .5 * hash_(i + 6)) * 256)))
    elif .2 < p < .56 and h < .42:
        PROPS.append((1, px, int(hash_(i + 8) * 5), 2 + int(hash_(i + 7) * 3)))
    elif .45 < p < .8 and h > .9:
        PROPS.append((2, px, int(hash_(i + 9) * 5), 0))
    elif p > .7 and .6 < h < .76:
        PROPS.append((3, px, int(hash_(i + 10) * 5), 0))
    x += 70
    i += 1
for n, q in enumerate([.35, .63, .91]):
    PROPS.append((4, q * L, 0, n))
PROPS.sort(key=lambda o: o[1])

POOLS = []
x = L * .6
while x < XE - 300:
    if Y(x) > Y(x - 2) and Y(x) >= Y(x + 2):
        lv = Y(x) - 7
        a = b = x
        while Y(a) > lv and a > x - 90:
            a -= 2
        while Y(b) > lv and b < x + 90:
            b += 2
        POOLS.append((a, b, lv, len(POOLS) % 5))
        x += 100
    x += 2

for i, m in enumerate([[-330, .5, 0], [-352, .3, 2], [-404, .36, 1], [-428, .6, 3], [-448, .28, 4], [-560, .3, 0], [-676, .4, 2]]):
    SHROOMS.append((m[0], m[1], m[2], float(i)))
SHROOMS.sort(key=lambda m: m[0])
SKYOBJ.sort(key=lambda o: o[1])

HX0, HX1 = -780, L + 60
ht = [round(Y(x) * 16) for x in range(HX0, HX1 + 1)]
assert max(ht) < 32767 and min(ht) > 0, (min(ht), max(ht))

out = []
w = out.append
w("/* SPDX-License-Identifier: GPL-3.0-only */")
w("/* FM ARCAVE's intro course. Written by tools/gen_course.py: do not edit. */")
w(f"#define CO_L {L}")
w(f"#define CO_XS {XS}")
w(f"#define CO_XE {XE}")
w(f"#define CO_XSTOP {XSTOP}")
w(f"#define CO_WALLX {WALLX}")
w(f"#define CO_LEDGE {LEDGE}")
w(f"#define CO_XMIN {XMIN}")
w(f"#define CO_LOOPX {LOOPX}")
w(f"#define CO_LOOPR {LOOPR}")
w(f"#define CO_CARDX {CARDX}")
w(f"#define CO_CAMMIN {CAMMIN}")
w(f"#define CO_HX0 ({HX0})")
w(f"#define CO_HN {len(ht)}")
w(f"#define CO_YEND_Q4 {round(Y(L) * 16)}")
w("static const int16_t CO_HT[CO_HN] = {   /* the land's height at every pixel, in 1/16 px */")
for k in range(0, len(ht), 16):
    w("    " + ",".join(str(v) for v in ht[k:k + 16]) + ",")
w("};")
w("typedef struct { int16_t x; uint8_t s, c, ph; } co_shroom_t;   /* s: size * 256, ph: phase, 256 = a turn */")
w(f"#define CO_NSHROOM {len(SHROOMS)}")
w("static const co_shroom_t CO_SHROOM[CO_NSHROOM] = {")
for m in SHROOMS:
    w(f"    {{{round(m[0])},{min(255, round(m[1] * 256))},{m[2]},{round((m[3] % TAU) / TAU * 256) & 255}}},")
w("};")
w("typedef struct { int16_t x, far; } co_pad_t;")
w(f"#define CO_NPAD {len(PADS)}")
w("static const co_pad_t CO_PAD[CO_NPAD] = {")
for p in PADS:
    w(f"    {{{round(p[0])},{round(p[1])}}},")
w("};")
w("typedef struct { int16_t x, y; uint8_t t, c; } co_sky_t;   /* t: 0 spore, 1 jelly, 2 ring */")
w(f"#define CO_NSKY {len(SKYOBJ)}")
w("static const co_sky_t CO_SKY[CO_NSKY] = {")
for o in SKYOBJ:
    w(f"    {{{round(o[1])},{round(o[2])},{o[0]},{o[3]}}},")
w("};")
w("typedef struct { int16_t x; uint8_t t, c; uint16_t n; } co_prop_t;   /* t: 0 pal (n: size*256), 1 crystal (n), 2 gate, 3 lamp, 4 sign (n: which) */")
w(f"#define CO_NPROP {len(PROPS)}")
w("static const co_prop_t CO_PROP[CO_NPROP] = {")
for o in PROPS:
    w(f"    {{{round(o[1])},{o[0]},{o[2]},{o[3]}}},")
w("};")
w("typedef struct { int16_t a, b, lv; uint8_t c; } co_pool_t;   /* lv in 1/16 px */")
w(f"#define CO_NPOOL {len(POOLS)}")
w("static const co_pool_t CO_POOL[CO_NPOOL] = {")
for o in POOLS:
    w(f"    {{{round(o[0])},{round(o[1])},{round(o[2] * 16)},{o[3]}}},")
w("};")

dst = Path(__file__).resolve().parent.parent / "firmware" / "gen" / "arcave_course.h"
dst.write_text("\n".join(out) + "\n")
print(f"{dst}: land {len(ht)} px, {len(SHROOMS)} mushrooms, {len(PADS)} pads, {len(SKYOBJ)} sky, {len(PROPS)} props, {len(POOLS)} pools")
