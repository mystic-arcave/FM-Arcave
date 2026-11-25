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
L_LONG = 15800   # the race back keeps the first length (the opening roll's: L_SHORT, below)
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


# The opening roll (Ben, 2026-11-25): its first stretch smooth, two long runs downhill each into a ramp, in place of the
# bumps; then, a little longer, more room for the roll's music to build on the way down; then the bumps after it lower
# at first and growing back to their full height towards the end, so the land goes gradually from smooth to bumpy.
# The short course is the old bumpy one (L_OLD long) with the smooth opening, D px longer, in place of its first
# OPEN_X1_OLD px; from there on it is the old land moved D on, its bumps CALM as tall at first (full again by 82%).
L_OLD, D = 9875, 1400
OPEN_X1_OLD = 3582                               # (where the opening meets the old land: in its valley)
OPEN_X1 = OPEN_X1_OLD + D
L_SHORT = L_OLD + D
CALM = .7
DY = 119                                         # (and the land after the opening this much lower: room for the landings)
# (old x, new x): how the old opening's things are moved over the new one (its run-ins took the extra length)
STRETCH = [(XS, XS), (640, 1240), (1340, 1940), (2350, 3150), (2600, 4000), (OPEN_X1_OLD, OPEN_X1)]
# points (x, y, slope) of a smooth curve: a long gentle start steepening into a kicker 30 px tall (its lip at 1600), a
# landing, a long gentle climb that takes off some of the speed, a run down into a second kicker (its lip at 4020), a
# landing, and down to the valley at OPEN_X1 (its last point is worked out from the land there). Each landing was fitted
# to Rock's flight off its kicker in the game itself (the host build, a trace of the knob held): a short flat top, then
# a slope that meets a flight a little flatter than his at its own slope (0.5) and one a little higher at its own
# (0.8), so that the flights in between come down without a thud. (Rock is lumpy: his take-off varies a little.)
OPEN_KNOTS = [(130, 300, 0), (700, 312, 0.04), (1050, 335, 0.12), (1240, 362, 0.38), (1390, 425, 0.55),   # the first run, its kicker
              (1500, 455, 0), (1600, 426, -0.5),
              (1636, 428, 0), (1660, 428, 0), (1684, 428, 0), (1708, 428, 0), (1732, 428, 0), (1754, 430.8, 0.5),   # its landing
              (1834, 467.1, 0.879), (1884, 511, 0.879), (2124, 612.5, 0.04),
              (2524, 557.5, 0), (2950, 562.5, 0.03), (3300, 577.5, 0.08), (3550, 602.5, 0.15), (3690, 626.5, 0.3),   # the climb, the second run, its kicker
              (3800, 648.5, 0.25), (3920, 658.5, 0), (4020, 632.5, -0.42),
              (4056, 634.5, 0), (4080, 634.5, 0), (4104, 634.5, 0), (4128, 634.5, 0), (4152, 634.5, 0),   # its landing
              (4176, 634.5, 0), (4200, 636.5, 0.398), (4221, 645.9, 0.5), (4311, 683.9, 0.8), (4361, 723.9, 0.8),
              (4601, 816.6, 0.04)]

# where each of the roll's 28 places (ac_rollmus.h: a bar of its piece each) begins, the last number its end: an equal
# time each for a roll with the knob held (measured in the host build), so that a fast stretch does not hurry the
# music on; then evened out within each part of the piece, which begin where they did: FIRST LIGHT at the start, THE
# VALLEY off the second kicker, THE POOLS where the pools of light begin, THE CLIMB
PLACE_X = [130, 454, 804, 1213, 1708, 2271, 2856, 3442, 4035, 4685, 5283, 5768, 6153, 6505, 6851, 7195, 7545, 7743, 8010,
           8329, 8653, 8934, 9198, 9479, 9827, 10219, 10582, 10901, 11211]

def stretch(x):
    if x < XS:
        return x
    if x >= OPEN_X1_OLD:
        return x + D
    for (a0, b0), (a1, b1) in zip(STRETCH, STRETCH[1:]):
        if a0 <= x <= a1:
            return b0 + (x - a0) * (b1 - b0) / (a1 - a0)


def calm(xo):                                    # (from 15% as tall where the opening ends, CALM by 900 px on, full by 82%)
    return (1 - (1 - CALM) * (1 - sm(.5 * L_OLD, .82 * L_OLD, xo))) * (.15 + .85 * sm(OPEN_X1_OLD, OPEN_X1_OLD + 900, xo))


def Y_calm(xo):                                  # (the old land at old x, its bumps calmed)
    b = base(xo)
    return b + calm(xo) * (Y_bumpy(xo) - b)


def open_y(x):
    for (x0, y0, s0), (x1, y1, s1) in zip(OPEN_KNOTS, OPEN_KNOTS[1:]):
        if x0 <= x <= x1:
            h = x1 - x0
            t = (x - x0) / h
            return ((2 * t ** 3 - 3 * t * t + 1) * y0 + (t ** 3 - 2 * t * t + t) * h * s0 + (3 * t * t - 2 * t ** 3) * y1
                    + (t ** 3 - t * t) * h * s1)


def Y(x):
    return Y_bumpy(x)


def Y_bumpy(x):
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


def build_short():
    """The opening roll: the old bumpy course's things moved over the new land (STRETCH), its land smooth up to OPEN_X1
    and the old one calmed after it. Before OPEN_X1 the old pads stay (moved) but for those a kicker needs clear, the
    old sky objects stay as high over the new land as they were over the old, and each kicker's lip has a pad with its
    usual arc of four spores; after it, pads, sky and pools are found on the calmed land as build() finds them."""
    global OPEN_KNOTS
    ref = build(L_OLD)                           # (and L, XE are the old course's from here: Y_bumpy, base read them)
    xo1 = OPEN_X1_OLD
    OPEN_KNOTS = OPEN_KNOTS[:] + [(OPEN_X1, Y_calm(xo1) + DY, Y_calm(xo1 + .5) - Y_calm(xo1 - .5))]

    def Yn(x):
        if x < XS:
            return Y_bumpy(x)
        if x <= OPEN_X1:
            w = sm(OPEN_X1 - 8, OPEN_X1, x)
            return open_y(x) * (1 - w) + (Y_calm(x - D) + DY) * w
        return Y_calm(x - D) + DY
    Ln, XEn = L_SHORT, ref["XE"] + D
    lips = []
    x = XS + 420
    while x < OPEN_X1:
        if Yn(x) < Yn(x - 2) and Yn(x) <= Yn(x + 2) and Yn(x - 40) - Yn(x) > 5:
            lips.append((x - 24, x + 300))      # (its far end fixed: the flight off a kicker is the same whatever is under it)
            x += 120
        x += 2
    # the bumps' own pads in the opening stay where they were, keys set into the smooth slope (a hop on one earns its
    # part of the music); not those in a kicker's run-in, flight or landing (its lip has its own)
    keep = [(stretch(a), stretch(b)) for a, b in ref["pads"] if a < xo1 and not (830 < a < 1260 or 2600 < a < 3170)]
    sky = [(t, stretch(x), y + Yn(stretch(x)) - Y_bumpy(x), col) for t, x, y, col in ref["sky"] if x < xo1
           and not any(px - 40 < stretch(x) < px + 520 for px, far in lips)]   # (none over a kicker's flight but its own arc)
    for i, (px, far) in enumerate(lips):         # (higher than a pad's: over a kicker's own flight, for a pad jump off it to take)
        for k in range(1, 5):
            sky.append((0, px + k * (far - px + 60) / 5, Yn(px) - 50 - 50 * math.sin(math.pi * k / 5) + k * 4, (i + k) % 5))
    after = []
    x = OPEN_X1 + 40
    while x < XEn - 320:
        if Yn(x) < Yn(x - 2) and Yn(x) <= Yn(x + 2) and Yn(x - 40) - Yn(x) > 5:
            f = x + 20
            while f < x + 400 and Yn(f + 2) >= Yn(f):
                f += 2
            after.append((x - 24, f))
            x += 120
        x += 2
    i0 = len([p for p in ref["pads"] if p[0] < xo1])   # (counted on from the old course's, so the jellies and rings fall as they did)
    for j, (px, far) in enumerate(after):
        i = i0 + j
        gy = Yn(far)
        for k in range(1, 5):
            sky.append((0, px + k * (far - px + 60) / 5, Yn(px) - 20 - 50 * math.sin(math.pi * k / 5) + k * 4, (i + k) % 5))
        if i % 3 == 1:
            bx = far + 10
            for jj, q in enumerate([[0, 95], [120, 175], [245, 250]]):
                sky.append((1 if jj < 2 else 2, bx + q[0], gy - q[1], (i + jj) % 5))
            for k in range(5):
                sky.append((0, bx + 60 + k * 70, gy - 140 - k * 45, (i + k) % 5))
    pools = []
    x = L_OLD * .6 + D
    while x < XEn - 300:
        if Yn(x) > Yn(x - 2) and Yn(x) >= Yn(x + 2):
            lv = Yn(x) - 7
            a = b = x
            while Yn(a) > lv and a > x - 90:
                a -= 2
            while Yn(b) > lv and b < x + 90:
                b += 2
            pools.append((a, b, lv, len(pools) % 5))
            x += 100
        x += 2
    ht = [round(Yn(x) * 16) for x in range(HX0, Ln + 61)]
    assert max(ht) < 32767 and min(ht) > 0, (min(ht), max(ht))
    return dict(L=Ln, XE=XEn, XSTOP=ref["XSTOP"] + D, ht=ht, yend=round(Yn(Ln) * 16),
                shrooms=sorted(((stretch(m[0]),) + tuple(m[1:]) for m in ref["shrooms"]), key=lambda m: m[0]),
                pads=sorted(lips + keep) + after, sky=sorted(sky, key=lambda o: o[1]),
                props=sorted(((o[0], stretch(o[1])) + tuple(o[2:]) for o in ref["props"]), key=lambda o: o[1]), pools=pools,
                Yn=Yn)


SHORT = build_short()
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
    # the land's height at every pixel, in 1/16 px, kept in half the room: every sixteenth height whole (CO_HA), the
    # fifteen after it as steps of a byte each from the one before (CO_HD); ac_intro.h's land_h adds them up. The land
    # has one wall (a step of many pixels): the sixteens are counted from CO hoff pixels before the land's start, so
    # that the wall falls between two of them. Not a value changes: checked here against the plain list
    ht = c["ht"]
    walls = [i for i in range(1, len(ht)) if not -128 <= ht[i] - ht[i - 1] <= 127]
    off = (16 - walls[0] % 16) % 16 if walls else 0
    assert all((i + off) % 16 == 0 for i in walls), f"the land's walls do not fall on one grid of sixteen: {walls}"
    hp = [ht[0]] * off + ht
    hp += [hp[-1]] * (-len(hp) % 16)
    ha = [hp[k] for k in range(0, len(hp), 16)]
    hd = [hp[k + n] - hp[k + n - 1] for k in range(0, len(hp), 16) for n in range(1, 16)]
    assert all(-128 <= d <= 127 for d in hd)
    for i in range(len(ht)):                     # (as land_h reads it)
        k, n = divmod(i + off, 16)
        assert ha[k] + sum(hd[k * 15:k * 15 + n]) == ht[i], i
    c["hoff"] = off
    w(f"static const int16_t CO_HA_{sfx}[{len(ha)}] = {{   /* the land's height at every sixteenth pixel, in 1/16 px */")
    for k in range(0, len(ha), 16):
        w("    " + ",".join(str(v) for v in ha[k:k + 16]) + ",")
    w("};")
    w(f"static const int8_t CO_HD_{sfx}[{len(hd)}] = {{   /* ... and the fifteen pixels after each: its step from the pixel before */")
    for k in range(0, len(hd), 30):
        w("    " + ",".join(str(v) for v in hd[k:k + 30]) + ",")
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
w("    const int16_t *ha; const int8_t *hd; int32_t hoff;   /* the land's heights: every sixteenth, the steps between, where the sixteens start */")
w("    const co_shroom_t *shroom; const co_pad_t *pad; const co_sky_t *sky; const co_prop_t *prop; const co_pool_t *pool;")
w("    int32_t nshroom, npad, nsky, nprop, npool;")
w("} course_t;")
assert len(PLACE_X) == 29 and PLACE_X[0] == XS and PLACE_X[-1] == SHORT["L"] - 64 and PLACE_X == sorted(PLACE_X)
w(f"static const int16_t CO_PLACEX[{len(PLACE_X)}] = {{ {', '.join(str(v) for v in PLACE_X)} }};   /* the opening roll: where each place of its piece begins (ac_rollmus.h) */")
for sfx, name, c in (("S", "CO_SHORT", SHORT), ("R", "CO_RACE", RACE)):
    w(f"static const course_t {name} = {{ {c['L']}, {c['XE']}, {c['XSTOP']}, {len(c['ht'])}, {c['yend']}, CO_HA_{sfx}, CO_HD_{sfx}, {c['hoff']},")
    w(f"    CO_SHROOM_{sfx}, CO_PAD_{sfx}, CO_SKY_{sfx}, CO_PROP_{sfx}, CO_POOL_{sfx},")
    w(f"    {len(c['shrooms'])}, {len(c['pads'])}, {len(c['sky'])}, {len(c['props'])}, {len(c['pools'])} }};")

dst = Path(__file__).resolve().parent.parent / "firmware" / "gen" / "arcave_course.h"
dst.write_text("\n".join(out) + "\n")
for name, c in (("short", SHORT), ("race", RACE)):
    print(f"{name}: land {len(c['ht'])} px, {len(c['shrooms'])} mushrooms, {len(c['pads'])} pads, {len(c['sky'])} sky, {len(c['props'])} props, "
          f"{len(c['pools'])} pools")
print(f"race: the opening area {RACE['d']:.0f} px lower; land {min(RACE['ht']) / 16:.0f}..{max(RACE['ht']) / 16:.0f}")
