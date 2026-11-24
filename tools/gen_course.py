#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The intro course of FM-ARCAVE as tables: firmware/gen/arcave_course.h.

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


Y0, XS, SL = 300, 130, 0.062
L_LONG, L_SHORT = 15800, 7900   # the race back keeps the first length; the opening roll is half of it
L = XE = XSTOP = 0
HX0 = -780
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


def build(length):
    """One course of that length: the same land to the left of the start, the same four steps, in proportion."""
    global L, XE, XSTOP
    L = length
    XE = L - 260
    XSTOP = L - 4
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

    HX1 = L + 60
    ht = [round(Y(x) * 16) for x in range(HX0, HX1 + 1)]
    assert max(ht) < 32767 and min(ht) > 0, (min(ht), max(ht))

    return dict(L=L, XE=XE, XSTOP=XSTOP, ht=ht, yend=round(Y(L) * 16), shrooms=SHROOMS, pads=PADS, sky=SKYOBJ, props=PROPS,
                pools=POOLS)


SHORT = build(L_SHORT)
LONG = build(L_LONG)    # (only to make the race from: its land, pads, sky and pools mirrored, its mushrooms and props kept)


def build_race(c):
    """The race back: out of the Arcave's upper door (on the right, at the top) downhill all the way to where the game
    began. As long as the first build's course. Its land is the long course's turned round (what went down to the right
    goes down to the left), with a flat stretch at the door for the start line; at its left end the opening area exactly
    as it is, only lower. Over the finish (x = 74) the opening course comes back in its place, behind the
    congratulations, so the two ends must look the same: they do, moved down by D."""
    lm = XE                                     # (Y is the long course's still: L, XE are the long one's)
    m = lm + XS                                 # the mirror: x on the race is m - x on the long course
    d = Y(lm) - Y0                              # how much lower the opening area sits at the race's end

    def R(x):
        if x < XS:
            return Y(x) + d                     # (left of XS the long course's land is the short one's)
        if x <= lm:
            return Y(m - x)
        return Y0
    ht = [round(R(x) * 16) for x in range(HX0, L + 61)]
    assert max(ht) < 32767 and min(ht) > 0, (min(ht), max(ht))
    pads = sorted(((m - px, m - far) for px, far in c["pads"]), key=lambda q: q[0])
    sky = sorted(((t, m - x, y, col) for t, x, y, col in c["sky"]), key=lambda o: o[1])
    pools = sorted(((m - b, m - a, lv, col) for a, b, lv, col in c["pools"]), key=lambda o: o[0])
    return dict(L=L, XE=XE, XSTOP=XSTOP, ht=ht, yend=round(R(L) * 16), shrooms=c["shrooms"], pads=pads, sky=sky,
                props=c["props"], pools=pools, d=d)


RACE = build_race(LONG)

out = []
w = out.append
w("/* SPDX-License-Identifier: GPL-3.0-only */")
w("/* FM-ARCAVE's intro courses: the opening roll (short) and the race back (the first build's length, downhill to the")
w(" * left all the way, ending in the opening area set CO_RACE_D px lower).")
w(" * Written by tools/gen_course.py: do not edit. */")
w(f"#define CO_XS {XS}")
w(f"#define CO_WALLX {WALLX}")
w(f"#define CO_LEDGE {LEDGE}")
w(f"#define CO_XMIN {XMIN}")
w(f"#define CO_LOOPX {LOOPX}")
w(f"#define CO_LOOPR {LOOPR}")
w(f"#define CO_CARDX {CARDX}")
w(f"#define CO_CAMMIN {CAMMIN}")
w(f"#define CO_HX0 ({HX0})")
w(f"#define CO_RACE_D {round(RACE['d'])}")
w("typedef struct { int16_t x; uint8_t s, c, ph; } co_shroom_t;   /* s: size * 256, ph: phase, 256 = a turn */")
w("typedef struct { int16_t x, far; } co_pad_t;")
w("typedef struct { int16_t x, y; uint8_t t, c; } co_sky_t;   /* t: 0 spore, 1 jelly, 2 ring */")
w("typedef struct { int16_t x, vx, vy; } co_step_t;   /* a booster at the foot of a big step: where, and its throw */")
w("typedef struct { int16_t x; uint8_t t, c; uint16_t n; } co_prop_t;   /* t: 0 pal (n: size*256), 1 crystal (n), 2 gate, 3 lamp, 4 sign (n: which) */")
w("typedef struct { int16_t a, b, lv; uint8_t c; } co_pool_t;   /* lv in 1/16 px */")
for sfx, c in (("S", SHORT), ("R", RACE)):
    ht = c["ht"]
    w(f"static const int16_t CO_HT_{sfx}[{len(ht)}] = {{   /* the land's height at every pixel, in 1/16 px */")
    for k in range(0, len(ht), 16):
        w("    " + ",".join(str(v) for v in ht[k:k + 16]) + ",")
    w("};")
    w(f"static const co_shroom_t CO_SHROOM_{sfx}[{len(c['shrooms'])}] = {{")
    for m in c["shrooms"]:
        w(f"    {{{round(m[0])},{min(255, round(m[1] * 256))},{m[2]},{round((m[3] % TAU) / TAU * 256) & 255}}},")
    w("};")
    w(f"static const co_pad_t CO_PAD_{sfx}[{len(c['pads'])}] = {{")
    for p in c["pads"]:
        w(f"    {{{round(p[0])},{round(p[1])}}},")
    w("};")
    w(f"static const co_sky_t CO_SKY_{sfx}[{len(c['sky'])}] = {{")
    for o in c["sky"]:
        w(f"    {{{round(o[1])},{round(o[2])},{o[0]},{o[3]}}},")
    w("};")
    w(f"static const co_prop_t CO_PROP_{sfx}[{len(c['props'])}] = {{")
    for o in c["props"]:
        w(f"    {{{round(o[1])},{o[0]},{o[2]},{o[3]}}},")
    w("};")
    w(f"static const co_pool_t CO_POOL_{sfx}[{len(c['pools'])}] = {{")
    for o in c["pools"]:
        w(f"    {{{round(o[0])},{round(o[1])},{round(o[2] * 16)},{o[3]}}},")
    w("};")
w(f"#define CO_MAXPAD {max(len(SHORT['pads']), len(RACE['pads']))}")
w(f"#define CO_MAXSKY {max(len(SHORT['sky']), len(RACE['sky']))}")
w("typedef struct {")
w("    int32_t l, xe, xstop, hn, yend_q4;")
w("    const int16_t *ht;")
w("    const co_shroom_t *shroom; const co_pad_t *pad; const co_sky_t *sky; const co_prop_t *prop; const co_pool_t *pool;")
w("    int32_t nshroom, npad, nsky, nprop, npool;")
w("} course_t;")
for sfx, name, c in (("S", "CO_SHORT", SHORT), ("R", "CO_RACE", RACE)):
    w(f"static const course_t {name} = {{ {c['L']}, {c['XE']}, {c['XSTOP']}, {len(c['ht'])}, {c['yend']}, CO_HT_{sfx},")
    w(f"    CO_SHROOM_{sfx}, CO_PAD_{sfx}, CO_SKY_{sfx}, CO_PROP_{sfx}, CO_POOL_{sfx},")
    w(f"    {len(c['shrooms'])}, {len(c['pads'])}, {len(c['sky'])}, {len(c['props'])}, {len(c['pools'])} }};")

dst = Path(__file__).resolve().parent.parent / "firmware" / "gen" / "arcave_course.h"
dst.write_text("\n".join(out) + "\n")
for name, c in (("short", SHORT), ("race", RACE)):
    print(f"{name}: land {len(c['ht'])} px, {len(c['shrooms'])} mushrooms, {len(c['pads'])} pads, {len(c['sky'])} sky, {len(c['props'])} props, "
          f"{len(c['pools'])} pools")
print(f"race: the opening area {RACE['d']:.0f} px lower; land {min(RACE['ht']) / 16:.0f}..{max(RACE['ht']) / 16:.0f}")
