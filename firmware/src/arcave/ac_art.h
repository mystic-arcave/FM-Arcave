/* SPDX-License-Identifier: GPL-3.0-only */
/* FM ARCAVE's two looks and the things drawn everywhere: the sky, a mushroom, the rock, the Hat. */

typedef struct {
    col_t bg, ground, rim, bands[5], far1, far2, far_stem, rock, rock_rim, eye, pupil, moss, text, caps[5], stem,
          mount, mrim, pad, mute, hat, hat_eye, hat_pupil, hat_rim, panel;
    uint8_t fill, dark;
} look_t;
static const look_t LOOKS[2] = {
    {   /* Black Opal: dark, lit lines on black */
        HEXC(0x000000), HEXC(0x000000), HEXC(0x7fe6d2),
        {HEXC(0x18969f), HEXC(0x5668e6), HEXC(0x963ede), HEXC(0xe248c8), HEXC(0x7fe6d2)},
        HEXC(0x0f3138), HEXC(0x23296a), HEXC(0x3a4650), HEXC(0x0c1216), HEXC(0x7fe6d2), HEXC(0x7fe6d2), HEXC(0x000000),
        HEXC(0x18969f), HEXC(0xffffff),
        {HEXC(0xe248c8), HEXC(0x963ede), HEXC(0x18c0c8), HEXC(0x7fe6d2), HEXC(0x5668e6)},
        HEXC(0xb9c6cc), HEXC(0x000000), HEXC(0x963ede), HEXC(0xffffff), HEXC(0x8e99a6),
        HEXC(0x000000), HEXC(0x7fe6d2), HEXC(0x000000), HEXC(0x7fe6d2), HEXC(0x0b1418), 0, 1,
    },
    {   /* Slow Glass: light, filled shapes under a dusk sky */
        HEXC(0x0a0920), HEXC(0x0a0920), HEXC(0xe9e2d0),
        {HEXC(0x6a4cc8), HEXC(0x4a30a8), HEXC(0x34209a), HEXC(0x241a6e), HEXC(0x1a1650)},
        HEXC(0x3a2a9c), HEXC(0x231a72), HEXC(0x8a7ad0), HEXC(0xe9e2d0), 0, HEXC(0x0a0920), HEXC(0xe9e2d0),
        HEXC(0xf2b45a), HEXC(0xe9e2d0),
        {HEXC(0xf2b45a), HEXC(0xe9e2d0), HEXC(0xf08a6a), HEXC(0xc8a0f0), HEXC(0xf2b45a)},
        HEXC(0xe9e2d0), HEXC(0x06061a), HEXC(0xf2b45a), HEXC(0xf2b45a), HEXC(0x9a94c8),
        HEXC(0xe9e2d0), HEXC(0x0a0920), HEXC(0xf2b45a), 0, HEXC(0x14123c), 1, 0,
    },
};
static const look_t *th = &LOOKS[0];
static uint8_t look_i;

/* sin(time * rate + phase): rate in tenths of a radian a second, phase in angle units -> Q14 */
static int32_t tsin(uint32_t ms, uint32_t rate10, int32_t phase)
{
    return isin((int32_t)(((uint64_t)ms * (rate10 * 1068u)) >> 10) + phase);
}

static void sky_draw(void)
{
    static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    static const col_t GLASS[8] = {AC_RGB(10, 12, 52), AC_RGB(20, 18, 92), AC_RGB(44, 28, 140), AC_RGB(82, 48, 172),
                                   AC_RGB(132, 72, 186), AC_RGB(190, 100, 172), AC_RGB(232, 140, 136), AC_RGB(242, 180, 90)};
    uint32_t i, x, y;
    if (th->dark) {
        gclear(0);
        for (i = 0; i < 46u; i++) {
            uint32_t h = ihash(i + 7u);
            fb_px[(ihash(i + 90u) * 150u >> 16) * AC_W + (ihash(i) * 240u >> 16)] =
                h > 45875u ? HEXC(0x7fe6d2) : h > 19660u ? HEXC(0x8a94a0) : 0xFFFFu;
        }
        return;
    }
    for (y = 0; y < AC_H; y++) {
        uint32_t f = AC_SKYF[y], k = f >> 8, fr = f & 255u;
        col_t *p = &fb_px[y * AC_W];
        for (x = 0; x < AC_W; x++) p[x] = GLASS[k + (fr > BAYER[(y & 3u) * 4u + (x & 3u)] * 16u + 8u ? 1u : 0u)];
    }
}

/* a mushroom standing at (x, y), s = size Q8 (256: a 34 px cap), glow 0..256 */
static void mush_draw(int32_t x, int32_t y, int32_t s, col_t cap, col_t stem, int32_t glow)
{
    uint32_t a0 = ga;
#define MS(v) (((v) * s) / 256)
    if (glow > 0 && a0 >= 8u) {
        ga = (a0 * (uint32_t)(26 + (26 * glow >> 8))) >> 8; gdisc(x, y + MS(-16), MS(30), cap);
        ga = (a0 * (uint32_t)(41 + (31 * glow >> 8))) >> 8; gdisc(x, y + MS(-16), MS(21), cap);
        ga = a0;
    }
    grect(x + MS(-3), y + MS(-16), imax(1, MS(6)), imax(1, MS(17)), stem);
    gellf(x, y + MS(-15), imax(1, MS(17)), imax(1, MS(13)), cap, 1);
    if (s >= 96) {
        grect(x + MS(-9), y + MS(-21), imax(1, MS(3)), imax(1, MS(3)), stem);
        grect(x + MS(3), y + MS(-24), imax(1, MS(3)), imax(1, MS(3)), stem);
        grect(x + MS(8), y + MS(-19), imax(1, MS(2)), imax(1, MS(2)), stem);
    }
#undef MS
}

/* the rock: lumpy, flat-bottomed when upright */
static const int8_t ROCK_P[9][2] = {{-8, 5}, {-9, -1}, {-6, -6}, {-1, -8}, {5, -7}, {9, -2}, {8, 5}, {3, 7}, {-4, 7}};
#define ROCK_RM Q8(7.6)
static int32_t rock_rad(int32_t a)               /* how far the rock reaches down when turned by a -> Q8 */
{
    int32_t s = isin(a), c = icos(a), m = 0, i;
    for (i = 0; i < 9; i++) {
        int32_t v = (ROCK_P[i][0] * s + ROCK_P[i][1] * c) >> 6;
        if (v > m) m = v;
    }
    return m;
}
/* at screen (x, y), zoom z (Q8), turned by a; eyes 0..256 (how open) */
static void rock_draw(int32_t x, int32_t y, int32_t z, int32_t a, int32_t eyes, col_t body, col_t rim, int has_rim,
                      col_t moss, col_t eye, col_t pupil)
{
    static const int8_t MOSS[4][3] = {{-50, -70, 26}, {-10, -86, 30}, {30, -80, 26}, {60, -64, 20}};   /* tenths */
    int32_t s = isin(a), c = icos(a), xy[18], i, e;
#define RX(px, py) (x + ((((px) * c - (py) * s) >> 6) * z >> 16))
#define RY(px, py) (y + ((((px) * s + (py) * c) >> 6) * z >> 16))
    for (i = 0; i < 4; i++)
        gdisc(x + ((((MOSS[i][0] * c - MOSS[i][1] * s) / 10) >> 6) * z >> 16), y + ((((MOSS[i][0] * s + MOSS[i][1] * c) / 10) >> 6) * z >> 16),
              imax(1, (MOSS[i][2] * z) / 2560), moss);
    for (i = 0; i < 9; i++) { xy[2 * i] = RX(ROCK_P[i][0], ROCK_P[i][1]); xy[2 * i + 1] = RY(ROCK_P[i][0], ROCK_P[i][1]); }
    gpoly(xy, 9, body);
    if (has_rim) gpolyline(xy, 9, 1, rim);
    if (eyes >= 15)
        for (e = -1; e <= 1; e += 2) {
            int32_t ex = x + ((((e * 34 * c + 10 * s) / 10) >> 6) * z >> 16), ey = y + ((((e * 34 * s - 10 * c) / 10) >> 6) * z >> 16);
            gellf(ex, ey, imax(1, (23 * z) / 2560), imax(1, (imax(40, 200 * eyes / 256) * z) / 25600), eye, 0);
            if (eyes > 102 && z >= 200)
                gellf(ex + (4 * z) / 2560, ey, imax(1, (8 * z) / 2560), imax(1, (130 * eyes / 256 * z) / 25600), pupil, 0);
        }
#undef RX
#undef RY
}

/* the Hat: a wizard's hat with one eye, and no body under it */
static int16_t HAT_XY[40][2], HAT_CURL[25][2];
static int32_t hat_n;
static void hat_init(void)
{
    static const int16_t Q[4][6] = {{-52, 2, -34, -9, -20, -8}, {-20, -8, -10, -42, 6, -66}, {6, -66, 18, -40, 22, -8}, {22, -8, 38, -9, 52, 2}};
    static const int8_t ZIG[8][2] = {{41, 10}, {30, 6}, {20, 12}, {8, 8}, {-4, 13}, {-16, 8}, {-28, 12}, {-41, 7}};
    int32_t i, k;
    hat_n = 0;
    for (i = 0; i < 4; i++)
        for (k = 0; k < 6; k++) {                /* a curve as six short lines */
            int32_t t = k * 256 / 6, u = 256 - t;
            HAT_XY[hat_n][0] = (int16_t)((u * u * Q[i][0] + 2 * u * t * Q[i][2] + t * t * Q[i][4]) >> 16);
            HAT_XY[hat_n][1] = (int16_t)((u * u * Q[i][1] + 2 * u * t * Q[i][3] + t * t * Q[i][5]) >> 16);
            hat_n++;
        }
    HAT_XY[hat_n][0] = 52; HAT_XY[hat_n][1] = 2; hat_n++;
    for (i = 0; i < 8; i++) { HAT_XY[hat_n][0] = ZIG[i][0]; HAT_XY[hat_n][1] = ZIG[i][1]; hat_n++; }
    for (k = 0; k <= 24; k++) {                  /* the curl at the tip */
        int32_t a = 32768 + k * 57344 / 24, rr = 11 * 24 - k * 8;
        HAT_CURL[k][0] = (int16_t)(17 + ((rr * icos(a)) >> 14) / 24);
        HAT_CURL[k][1] = (int16_t)(-64 + ((rr * isin(a)) >> 14) / 24);
    }
}
/* s: size Q8; open, draw: 0..256; glance: -256..256; has_rim: drawn as a line (Black Opal) */
static void hat_draw(int32_t x, int32_t y, int32_t s, col_t col, col_t eye, col_t pupil, int32_t open, int32_t glance,
                     int has_rim, col_t rim, int32_t draw)
{
    int32_t xy[80], i, n;
    uint32_t a0 = ga;
#define HS(v) (((v) * s) / 256)
    for (i = 0; i < hat_n; i++) { xy[2 * i] = x + HS(HAT_XY[i][0]); xy[2 * i + 1] = y + HS(HAT_XY[i][1]); }
    if (!has_rim) ga = (a0 * (uint32_t)draw) >> 8;
    if (has_rim != 2) gpoly(xy, hat_n, col);
    if (!has_rim) {
        for (i = 0; i <= 24; i++) gdisc(x + HS(HAT_CURL[i][0]), y + HS(HAT_CURL[i][1]), imax(1, HS(3)), col);
        ga = a0;
    } else {
        n = (hat_n + 1) * draw / 256;            /* the line draws itself */
        for (i = 0; i < n && i < hat_n; i++) {
            int32_t j = (i + 1) % hat_n;
            gline(xy[2 * i], xy[2 * i + 1], xy[2 * j], xy[2 * j + 1], rim);
        }
        n = 24 * draw / 256;
        for (i = 0; i < n; i++)
            gline(x + HS(HAT_CURL[i][0]), y + HS(HAT_CURL[i][1]), x + HS(HAT_CURL[i + 1][0]), y + HS(HAT_CURL[i + 1][1]), rim);
    }
    if (open > 0) {                              /* an almond eye */
        int32_t k, up = 13 * open / 256, dn = 10 * open / 256;
        for (k = -15; k <= 15; k++) {
            int32_t f = 225 - k * k, y0 = -3 - up * f / 450, y1 = -3 + dn * f / 450, yy;
            for (yy = HS(y0 * 4) / 4; yy <= HS(y1 * 4) / 4; yy++) ghl(x + HS(k), x + HS(k + 1) + (s > 256), y + yy, eye);
        }
        if (open > 90) gellf(x + HS(glance * 6 / 256), y + HS(-4), imax(1, HS(9) / 2), imax(1, HS(6 * open / 256)), pupil, 0);
    }
#undef HS
}
