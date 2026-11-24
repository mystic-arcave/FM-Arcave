/* SPDX-License-Identifier: GPL-3.0-only */
/* What the profile looks like (ac_prof.h has the catalogue and says how to add to it): Rock in the stone, hat and face
 * he wears; the sparks his keys throw off and the trail he leaves; how he jumps; the strip of tickets out of a machine. */

/* ---------------------------------------------------------------- a place on Rock
 * Points are in tenths of his units (he is 18 units wide, 15 tall; (0, 0) his middle, y down), turned and enlarged
 * with him, so everything he wears rolls with him. */
typedef struct { int32_t x, y, z, c, s; } rk_t;
static void rk_pt(const rk_t *k, int32_t px, int32_t py, int32_t *ox, int32_t *oy)
{
    *ox = k->x + ((((px * k->c - py * k->s) / 10) >> 6) * k->z >> 16);
    *oy = k->y + ((((px * k->s + py * k->c) / 10) >> 6) * k->z >> 16);
}
static int32_t rk_len(const rk_t *k, int32_t v) { return imax(1, v * k->z / 2560); }
static void rk_disc(const rk_t *k, int32_t px, int32_t py, int32_t r, col_t c)
{
    int32_t x, y;
    rk_pt(k, px, py, &x, &y); gdisc(x, y, rk_len(k, r), c);
}
static void rk_line(const rk_t *k, int32_t x0, int32_t y0, int32_t x1, int32_t y1, col_t c)   /* thicker as he is larger */
{
    int32_t ax, ay, bx, by;
    rk_pt(k, x0, y0, &ax, &ay); rk_pt(k, x1, y1, &bx, &by);
    if (k->z >= 420) { gline2(ax, ay, bx, by, c); if (k->z >= 760) gline2(ax + 1, ay + 1, bx + 1, by + 1, c); }
    else gline(ax, ay, bx, by, c);
}
/* a shape of up to 12 corners: filled, and outlined if line is not 0 */
static void rk_poly(const rk_t *k, const int16_t *p, int32_t n, col_t fill, col_t line)
{
    int32_t xy[24], i;
    for (i = 0; i < n && i < 12; i++) rk_pt(k, p[2 * i], p[2 * i + 1], &xy[2 * i], &xy[2 * i + 1]);
    gpoly(xy, n, fill);
    if (line) gpolyline(xy, n, 1, line);
}
static void rk_path(const rk_t *k, const int16_t *p, int32_t n, col_t c)   /* an open line through n points */
{
    int32_t i;
    for (i = 0; i + 1 < n; i++) rk_line(k, p[2 * i], p[2 * i + 1], p[2 * i + 2], p[2 * i + 3], c);
}

/* ---------------------------------------------------------------- the stone */
typedef struct { col_t body, rim, moss, eye, pupil; } pf_body_t;   /* body 0: the look's own stone; moss 0: its own moss */
static const pf_body_t PF_BODY[10] = {
    {0, 0, 0, 0, 0},
    {0, 0, HEXC(0x5fd06a), 0, 0},                                                     /* MOSSY */
    {HEXC(0xd8352a), HEXC(0xfff4e8), HEXC(0xfff4e8), HEXC(0xfff4e8), HEXC(0x1a0a08)}, /* TOADSTOOL */
    {HEXC(0xe8e6e0), HEXC(0x9aa0a8), HEXC(0x9aa0a8), HEXC(0x20242c), HEXC(0xe8e6e0)}, /* MARBLE */
    {HEXC(0xf4f4f0), HEXC(0x30303a), HEXC(0x30303a), HEXC(0x101014), HEXC(0xf4f4f0)}, /* ZEBRA */
    {HEXC(0x2a0a06), HEXC(0xff6a1a), HEXC(0xff9a2a), HEXC(0xffd060), HEXC(0x2a0a06)}, /* LAVA */
    {HEXC(0xbfe8ff), HEXC(0xffffff), HEXC(0xffffff), HEXC(0x14365a), HEXC(0xbfe8ff)}, /* ICE */
    {HEXC(0xf2c230), HEXC(0xfff0a0), HEXC(0xfff0a0), HEXC(0x4a2c00), HEXC(0xf2c230)}, /* GOLD */
    {HEXC(0x1a1040), HEXC(0x8a6cff), HEXC(0xff7ad8), HEXC(0xffffff), HEXC(0x1a1040)}, /* GALAXY */
    {HEXC(0x202028), HEXC(0xffffff), HEXC(0xffffff), HEXC(0xffffff), HEXC(0x000000)}, /* DISCO */
};
static void deco_body(const rk_t *k, int kind, col_t moss, uint32_t tm)   /* its pattern, over the stone and under the eyes */
{
    static const int16_t SPOT[5][3] = {{-45, -30, 16}, {10, -50, 14}, {50, -5, 18}, {-10, 40, 13}, {-62, 22, 10}};
    static const int16_t VEIN1[8] = {-70, -20, -20, 10, 10, -5, 60, 30}, VEIN2[6] = {-30, -60, 0, -30, 34, -44};
    static const int16_t STRIPE[4][4] = {{-60, -42, -50, 48}, {-28, -60, -16, 60}, {8, -66, 20, 60}, {44, -58, 54, 44}};
    static const int16_t CRACK1[12] = {-70, 10, -40, -10, -25, 20, 5, 0, 20, 35, 55, 15}, CRACK2[6] = {0, -66, -10, -35, 15, -20};
    static const int16_t FACET[4][4] = {{-58, -56, -10, 0}, {-10, 0, -38, 58}, {-10, 0, 48, -64}, {-10, 0, 74, 36}};
    static const int8_t STAR[7][2] = {{-55, -30}, {-20, 30}, {5, -45}, {40, 10}, {60, -25}, {-45, 35}, {20, 45}};
    uint32_t a0 = ga;
    int32_t i, x, y;
    switch (kind) {
    case 1:                                      /* MOSSY: tufts over the top as well */
        rk_disc(k, -34, -58, 22, moss); rk_disc(k, 18, -66, 26, moss); rk_disc(k, 62, -34, 17, moss); rk_disc(k, -70, -10, 13, moss);
        break;
    case 2: for (i = 0; i < 5; i++) rk_disc(k, SPOT[i][0], SPOT[i][1], SPOT[i][2], HEXC(0xfff4e8)); break;
    case 3: rk_path(k, VEIN1, 4, HEXC(0x9aa0a8)); rk_path(k, VEIN2, 3, HEXC(0xb8bcc4)); break;
    case 4: for (i = 0; i < 4; i++) { rk_line(k, STRIPE[i][0], STRIPE[i][1], STRIPE[i][2], STRIPE[i][3], HEXC(0x101014)); rk_line(k, STRIPE[i][0] + 6, STRIPE[i][1], STRIPE[i][2] + 6, STRIPE[i][3], HEXC(0x101014)); } break;
    case 5: {                                    /* LAVA: the cracks glow and dim */
        col_t c = mix565(HEXC(0xff5a10), HEXC(0xffe060), (uint32_t)(ipulse(tm, 1500) >> 3));
        rk_path(k, CRACK1, 6, c); rk_path(k, CRACK2, 3, c);
        break;
    }
    case 6:
        for (i = 0; i < 4; i++) rk_line(k, FACET[i][0], FACET[i][1], FACET[i][2], FACET[i][3], HEXC(0x7fbfe8));
        rk_disc(k, -40, -30, 7, HEXC(0xffffff));
        break;
    case 7: {                                    /* GOLD: a glint goes across */
        int32_t t = (int32_t)(tm % 2400u);
        if (t < 900) { int32_t px = -50 + t * 100 / 900; rk_line(k, px + 12, -42, px - 12, 34, HEXC(0xffffff)); }
        break;
    }
    case 8:                                      /* GALAXY: its stars, each to its own time */
        for (i = 0; i < 7; i++) {
            ga = (a0 * (uint32_t)(60 + ipulse(tm + (uint32_t)i * 700u, 900 + (uint32_t)i * 170u) * 196 / 256)) >> 8;
            rk_pt(k, STAR[i][0], STAR[i][1], &x, &y);
            gdisc(x, y, k->z >= 500 ? 1 : 0, i % 3 == 0 ? HEXC(0xff7ad8) : i % 3 == 1 ? HEXC(0x7fe6ff) : HEXC(0xffffff));
        }
        ga = a0;
        break;
    case 9:                                      /* DISCO: tiles of the look's five colours, changing */
        for (i = 0; i < 20; i++) {
            int32_t gx = i % 5 - 2, gy = i / 5 - 2, px = gx * 27 - 4, py = gy * 25 + 8;
            int16_t q[8];
            if (iabs(px) + iabs(py) > 96) continue;
            if (ihash((uint32_t)i * 77u + tm / 240u) % 3u == 0u) continue;
            q[0] = (int16_t)(px - 11); q[1] = (int16_t)(py - 10); q[2] = (int16_t)(px + 11); q[3] = (int16_t)(py - 10);
            q[4] = (int16_t)(px + 11); q[5] = (int16_t)(py + 10); q[6] = (int16_t)(px - 11); q[7] = (int16_t)(py + 10);
            rk_poly(k, q, 4, th->caps[ihash((uint32_t)i * 31u + tm / 240u) % 5u], 0);
        }
        break;
    }
}

/* ---------------------------------------------------------------- what he wears on top */
static void deco_hat(const rk_t *k0, int kind, col_t line, uint32_t tm)   /* line: an outline for the dark shapes (0: none) */
{
    rk_t kk = *k0;
    const rk_t *k = &kk;
    static const int16_t PARTY[6] = {-46, -66, 44, -70, 4, -172};
    static const int16_t CAP[14] = {-74, -60, -64, -100, -36, -128, 0, -138, 36, -128, 64, -100, 74, -62};
    static const int16_t BAND[14] = {-88, -6, -86, -58, -56, -98, 0, -112, 56, -98, 86, -58, 88, -6};
    static const int16_t BRIM[8] = {-64, -68, 64, -70, 64, -84, -64, -82}, CROWN_T[8] = {-40, -82, 40, -84, 38, -154, -38, -152};
    static const int16_t TBAND[8] = {-40, -94, 40, -96, 40, -108, -40, -106};
    static const int16_t DOME[12] = {-48, -68, -40, -98, -16, -114, 16, -114, 40, -98, 48, -70};
    static const int16_t WIZ[14] = {-58, -64, 56, -70, 30, -84, 20, -132, 42, -176, 6, -146, -24, -86};
    static const int16_t KING[14] = {-48, -66, 48, -70, 54, -124, 28, -100, 2, -132, -24, -100, -52, -120};
    int32_t i, x, y;
    /* (hats are drawn a third larger than they are measured, about where they sit on his head: they read from afar) */
    rk_pt(k0, 0, -70, &x, &y);
    kk.z = k0->z * 4 / 3;
    rk_pt(&kk, 0, -70, &i, &kk.y);               /* (where that point would be at the larger size ...) */
    kk.x = k0->x + x - i; kk.y = k0->y + y - kk.y;   /* (... and the whole moved so that it stays put) */
    switch (kind) {
    case 1:                                      /* SPROUT */
        rk_line(k, 8, -76, 12, -118, HEXC(0x3fae52));
        rk_disc(k, -2, -124, 15, HEXC(0x5fd06a)); rk_disc(k, 26, -128, 15, HEXC(0x5fd06a));
        break;
    case 2:                                      /* PARTY HAT */
        rk_poly(k, PARTY, 3, HEXC(0xff5cc0), line);
        rk_line(k, -22, -100, 28, -104, HEXC(0xffe060)); rk_line(k, -8, -136, 16, -138, HEXC(0x60d0ff));
        rk_disc(k, 4, -174, 16, HEXC(0xffe060));
        break;
    case 3:                                      /* FLOWER */
        rk_line(k, 0, -76, 0, -112, HEXC(0x3fae52));
        for (i = 0; i < 5; i++) rk_disc(k, (isin(i * 13107) * 22) >> 14, -132 - ((icos(i * 13107) * 22) >> 14), 15, HEXC(0xff7ab8));
        rk_disc(k, 0, -132, 12, HEXC(0xffe060));
        break;
    case 4:                                      /* MUSHROOM CAP */
        rk_poly(k, CAP, 7, HEXC(0xd8352a), line);
        rk_disc(k, -34, -96, 12, HEXC(0xfff4e8)); rk_disc(k, 8, -116, 11, HEXC(0xfff4e8)); rk_disc(k, 44, -90, 9, HEXC(0xfff4e8));
        break;
    case 5:                                      /* HEADPHONES */
        rk_path(k, BAND, 7, HEXC(0xd0d4dc));
        rk_disc(k, -92, 4, 30, th->caps[0]); rk_disc(k, 92, 4, 30, th->caps[0]);
        break;
    case 6:                                      /* TOP HAT */
        rk_poly(k, BRIM, 4, HEXC(0x14141c), line); rk_poly(k, CROWN_T, 4, HEXC(0x14141c), line);
        rk_poly(k, TBAND, 4, HEXC(0xc03040), 0);
        break;
    case 7: {                                    /* PROPELLER: it turns */
        int32_t w = (icos((int32_t)(tm * 90u)) * 46) >> 14;
        rk_poly(k, DOME, 6, HEXC(0x3a7cff), line);
        rk_line(k, -16, -114, -16, -70, HEXC(0xffd040)); rk_line(k, 16, -114, 16, -70, HEXC(0xffd040));
        rk_line(k, 0, -114, 0, -134, HEXC(0xd0d4dc));
        rk_line(k, -w, -136, w, -136, HEXC(0xff5050));
        rk_disc(k, 0, -136, 7, HEXC(0xffd040));
        break;
    }
    case 8:                                      /* HALO: it stays level over him, however he rolls */
        x = k->x; y = k->y - rk_len(k, 128) + (tsin(tm, 30, 0) >> 13);
        garc(x, y, rk_len(k, 62), imax(2, rk_len(k, 20)), 0, 65536, HEXC(0xffe880), k->z >= 420);
        break;
    case 9:                                      /* WIZARD HAT: a little of the Hat himself */
        rk_poly(k, WIZ, 7, HEXC(0x5a3cc8), line ? line : HEXC(0xb9a0ff));
        rk_disc(k, 14, -110, 9, HEXC(0xffe060));
        break;
    case 10:                                     /* CROWN */
        rk_poly(k, KING, 7, HEXC(0xf2c230), HEXC(0xfff0a0));
        rk_disc(k, 2, -90, 9, HEXC(0xff4060)); rk_disc(k, -30, -84, 7, HEXC(0x40a0ff)); rk_disc(k, 32, -86, 7, HEXC(0x40d080));
        break;
    }
}
/* round his eyes (they are at (-34, -10) and (34, -10), about 23 wide) */
static void deco_face(const rk_t *k, int kind, col_t ink, col_t line)   /* ink: his eyes' colour */
{
    /* MOUSTACHE: the left half, bushy (the lower edge in tufts, the end drooping); the right is the same mirrored */
    static const int16_t TACHE[24] = {0, 20, -30, 12, -56, 16, -74, 28, -86, 46, -70, 46, -60, 56, -48, 48, -36, 57, -24, 49,
                                      -12, 55, 0, 46};
    static const int16_t SH_L[8] = {-66, -30, -6, -30, -12, 8, -58, 8}, SH_R[8] = {6, -30, 66, -30, 58, 8, 12, 8};
    /* SUNGLASSES: a round lens over each eye, a sunset in it (plum, pink, orange: the two lower bands are the lens cut
     * across), in a white frame. The left lens; the right is the same moved 68 along */
    static const int16_t SUN[24] = {-62, -18, -58, -30, -48, -38, -34, -40, -20, -38, -10, -30, -6, -18, -8, -4, -18, 8, -34, 12,
                                    -50, 8, -60, -4};
    static const int16_t SUN_MID[12] = {-7, -14, -8, -4, -11, 0, -57, 0, -60, -4, -61, -14}, SUN_LOW[10] = {-11, 0, -18, 8, -34, 12, -50, 8, -57, 0};
    uint32_t a0 = ga;
    int32_t x, y, e;
    switch (kind) {
    case 1:                                      /* BLUSH */
        ga = a0 * 170u >> 8; rk_disc(k, -60, 20, 13, HEXC(0xff7a9a)); rk_disc(k, 60, 20, 13, HEXC(0xff7a9a)); ga = a0;
        break;
    case 2:                                      /* BROWS: he means it */
        rk_line(k, -62, -46, -14, -32, ink); rk_line(k, 62, -46, 14, -32, ink);
        break;
    case 3:                                      /* GLASSES */
        for (e = -1; e <= 1; e += 2) { rk_pt(k, e * 34, -10, &x, &y); garc(x, y, rk_len(k, 30), rk_len(k, 30), 0, 65536, HEXC(0xffd040), k->z >= 420); }
        rk_line(k, -6, -12, 6, -12, HEXC(0xffd040));
        break;
    case 4: {                                    /* MOUSTACHE: big and bushy, and no mouth under it */
        int16_t p[24];
        int32_t j;
        for (e = -1; e <= 1; e += 2) {
            for (j = 0; j < 12; j++) { p[2 * j] = (int16_t)(TACHE[2 * j] * e); p[2 * j + 1] = TACHE[2 * j + 1]; }
            rk_poly(k, p, 12, HEXC(0x6a3a18), 0);   /* no outline: drawn round, it read as a mouth */
            rk_disc(k, e * -16, 24, 11, HEXC(0x6a3a18));   /* tufts along the top, to make it bushy */
            rk_disc(k, e * -40, 22, 12, HEXC(0x6a3a18));
            rk_disc(k, e * -64, 30, 11, HEXC(0x6a3a18));
            rk_line(k, e * -10, 20, e * -14, 34, HEXC(0x9a6234));   /* a few lighter hairs, fanning out */
            rk_line(k, e * -34, 16, e * -42, 32, HEXC(0x9a6234));
            rk_line(k, e * -58, 24, e * -70, 38, HEXC(0x9a6234));
            rk_line(k, e * -24, 36, e * -30, 48, HEXC(0x4a2610));   /* and darker ones lower down */
            rk_line(k, e * -50, 38, e * -58, 50, HEXC(0x4a2610));
        }
        break;
    }
    case 5:                                      /* MONOCLE */
        rk_pt(k, 34, -10, &x, &y); garc(x, y, rk_len(k, 32), rk_len(k, 32), 0, 65536, HEXC(0xffd040), k->z >= 420);
        rk_line(k, 56, 14, 74, 62, HEXC(0xffd040));
        break;
    case 6:                                      /* EYEPATCH */
        rk_line(k, -92, -34, 70, -58, line ? line : HEXC(0x14141c)); rk_disc(k, -34, -10, 30, line ? HEXC(0x243840) : HEXC(0x14141c));
        if (line) { rk_pt(k, -34, -10, &x, &y); garc(x, y, rk_len(k, 30), rk_len(k, 30), 0, 65536, line, 0); }
        break;
    case 7:                                      /* SHADES */
        rk_poly(k, SH_L, 4, HEXC(0x0c0c14), line); rk_poly(k, SH_R, 4, HEXC(0x0c0c14), line);
        rk_line(k, -6, -26, 6, -26, line ? line : HEXC(0x0c0c14));
        rk_line(k, -54, -22, -40, -8, HEXC(0xffffff)); rk_line(k, 18, -22, 32, -8, HEXC(0xffffff));
        break;
    case 8:                                      /* SUNGLASSES */
        for (e = 0; e < 2; e++) {
            int16_t p[26];
            int32_t j, dx = e * 68;
            for (j = 0; j < 12; j++) { p[2 * j] = (int16_t)(SUN[2 * j] + dx); p[2 * j + 1] = SUN[2 * j + 1]; }
            rk_poly(k, p, 12, HEXC(0x3a1458), 0);
            for (j = 0; j < 6; j++) { p[2 * j] = (int16_t)(SUN_MID[2 * j] + dx); p[2 * j + 1] = SUN_MID[2 * j + 1]; }
            rk_poly(k, p, 6, HEXC(0xff3c8c), 0);
            for (j = 0; j < 5; j++) { p[2 * j] = (int16_t)(SUN_LOW[2 * j] + dx); p[2 * j + 1] = SUN_LOW[2 * j + 1]; }
            rk_poly(k, p, 5, HEXC(0xff9a2a), 0);
            for (j = 0; j < 12; j++) { p[2 * j] = (int16_t)(SUN[2 * j] + dx); p[2 * j + 1] = SUN[2 * j + 1]; }
            p[24] = p[0]; p[25] = p[1];
            rk_path(k, p, 13, HEXC(0xffffff));   /* the frame, closed */
            rk_line(k, -50 + dx, -30, -40 + dx, -34, HEXC(0xffffff));   /* a glint */
        }
        rk_line(k, -6, -24, 6, -24, HEXC(0xffffff));   /* the bridge, and the arms back over his sides */
        rk_line(k, -62, -20, -84, -26, HEXC(0xffffff)); rk_line(k, 62, -20, 84, -26, HEXC(0xffffff));
        break;
    }
}
/* Rock in what eq says he wears (eq: stone, hat, face, trail), at screen (x, y), zoom z, turned by a; rimc: his rim in
   this colour instead (UNDERSONG: each player's own), 0 the stone's own */
static void deco_rock_ex(int32_t x, int32_t y, int32_t z, int32_t a, int32_t eyes, const uint8_t *eq, uint32_t tm, col_t rimc)
{
    const pf_body_t *b = &PF_BODY[eq[0] < 10u ? eq[0] : 0u];
    rk_t k;
    col_t body = b->body ? b->body : th->rock, rim = rimc ? rimc : b->rim ? b->rim : th->rock_rim, moss = b->moss ? b->moss : rimc ? rimc : th->moss;
    col_t eye = b->eye ? b->eye : th->eye, pupil = b->eye ? b->pupil : th->pupil, line = th->dark ? th->rock_rim : 0;
    k.x = x; k.y = y; k.z = z; k.c = icos(a); k.s = isin(a);
    rock_draw(x, y, z, a, 0, body, rim, b->body || rimc ? 1 : th->dark, moss, eye, pupil);
    if (eq[0]) deco_body(&k, eq[0], moss, tm);
    rock_eyes(x, y, z, a, eyes, eye, pupil);
    if (eq[2]) deco_face(&k, eq[2], eye, line);
    if (eq[1]) deco_hat(&k, eq[1], line, tm);
}
static void deco_rock(int32_t x, int32_t y, int32_t z, int32_t a, int32_t eyes, const uint8_t *eq, uint32_t tm) { deco_rock_ex(x, y, z, a, eyes, eq, tm, 0); }

/* ---------------------------------------------------------------- sparks: what the keys throw off, what he leaves behind
 * A small pool of them, in the world's own places (Q8), so they stay where they were let go as the camera moves. */
enum { PX_DOT, PX_RING, PX_NOTE, PX_STAR, PX_BUB, PX_HEART, PX_ZAP, PX_FLAME, PX_DUST, PX_BOW };
#define PFX_N 40
typedef struct { int32_t x, y, vx, vy; int16_t life, max; uint8_t kind, seed; col_t col; } pfx_t;
static pfx_t pfx[PFX_N];
static void pfx_add(int kind, int32_t x, int32_t y, int32_t vx, int32_t vy, int32_t life, col_t col)   /* v: pixels a second */
{
    static uint8_t next;
    pfx_t *p = &pfx[next % PFX_N];
    int32_t i;
    for (i = 0; i < PFX_N; i++) if (pfx[(next + i) % PFX_N].life <= 0) { p = &pfx[(next + i) % PFX_N]; break; }
    next = (uint8_t)((uint32_t)(p - pfx) + 1u);
    p->kind = (uint8_t)kind; p->x = x; p->y = y; p->vx = vx * 256; p->vy = vy * 256;
    p->life = p->max = (int16_t)life; p->col = col; p->seed = (uint8_t)ac_rnd();
}
static void pfx_clear(void) { int32_t i; for (i = 0; i < PFX_N; i++) pfx[i].life = 0; }
static void pfx_tick(int32_t dt)
{
    int32_t i;
    for (i = 0; i < PFX_N; i++) {
        pfx_t *p = &pfx[i];
        if (p->life <= 0) continue;
        p->life = (int16_t)(p->life - dt);
        p->x += p->vx * dt / 1000; p->y += p->vy * dt / 1000;
        if (p->kind == PX_DOT || p->kind == PX_STAR) p->vy += 420 * 256 * dt / 1000;   /* these fall */
        else if (p->kind == PX_DUST) { p->vx -= p->vx * imin(dt, 200) / 300; p->vy -= p->vy * imin(dt, 200) / 300; }
    }
}
/* drawn through a camera at (camx, camy) (Q8) with zoom z (Q8, 256: one to one) */
static void pfx_draw(int32_t camx, int32_t camy, int32_t z)
{
    static const col_t RB[6] = {AC_RGB(255, 60, 60), AC_RGB(255, 170, 40), AC_RGB(255, 235, 60), AC_RGB(60, 220, 100), AC_RGB(60, 140, 255), AC_RGB(190, 90, 255)};
    uint32_t a0 = ga;
    int32_t i, j;
    for (i = 0; i < PFX_N; i++) {
        const pfx_t *p = &pfx[i];
        int32_t x, y, q, al;
        if (p->life <= 0) continue;
        x = ((p->x - camx) * z) >> 16; y = ((p->y - camy) * z) >> 16;
        if (x < -20 || x > AC_W + 20 || y < -20 || y > AC_H + 20) continue;
        q = 256 - p->life * 256 / imax(1, p->max);           /* how far through its time */
        al = q < 180 ? 256 : (256 - q) * 256 / 76;
        ga = (a0 * (uint32_t)iclamp(al, 0, 256)) >> 8;
        switch (p->kind) {
        case PX_DOT: grect(x - 1, y - 1, q < 128 ? 3 : 2, q < 128 ? 3 : 2, p->col); break;
        case PX_RING: ga = (a0 * (uint32_t)(256 - q)) >> 8; gcirc(x, y, 4 + q * 18 / 256, p->col); gcirc(x, y, 3 + q * 18 / 256, p->col); break;
        case PX_NOTE: gtext(x - 3, y - 4, "\xE2\x99\xAA", p->col, 1 + (p->seed & 1)); break;
        case PX_STAR: ghl(x - 3, x + 4, y, p->col); grect(x, y - 3, 1, 7, p->col); grect(x - 1, y - 1, 3, 3, p->col); break;
        case PX_BUB: gcirc(x + (tsin((uint32_t)(p->max - p->life), 60, p->seed * 256) >> 12), y, 2 + (p->seed & 1) + q / 128, p->col); break;
        case PX_HEART: gtext(x - 3, y - 3, "\xE2\x99\xA5", p->col, 1 + (p->seed & 1)); break;
        case PX_ZAP: {
            int32_t px = x, py = y, dx = (p->seed & 1) ? 5 : -5;
            for (j = 0; j < 4; j++) { int32_t nx = px + dx + (int32_t)(ihash(p->seed + (uint32_t)j) % 5u) - 2, ny = py - 6; gline2(px, py, nx, ny, j & 1 ? HEXC(0xffffff) : p->col); px = nx; py = ny; dx = -dx; }
            break;
        }
        case PX_FLAME: gdisc(x, y, q < 110 ? 2 : 1, q < 80 ? HEXC(0xffe060) : q < 170 ? HEXC(0xff8a20) : HEXC(0xd03018)); break;
        case PX_DUST: ga = (a0 * (uint32_t)((256 - q) * 150 / 256)) >> 8; gdisc(x, y, 1 + q / 90, p->col); break;
        case PX_BOW: ga = (a0 * (uint32_t)(256 - q)) >> 8; for (j = 0; j < 6; j++) grect(x - 2, y - 6 + j * 2, 4, 2, RB[j]); break;
        }
    }
    ga = a0;
}
/* a key of colour `zone` pressed with Rock at (x, y) (the world's, Q8): what that colour wears flies off him */
static void deco_fx_kind(int kind, col_t c, int32_t x, int32_t y)
{
    int32_t i, a;
    switch (kind) {
    case 1: for (i = 0; i < 8; i++) { a = i * 8192 + (int32_t)(ac_rnd() % 4000u); pfx_add(PX_DOT, x, y, (icos(a) * 100) >> 14, ((isin(a) * 100) >> 14) - 60, 380, c); } break;
    case 2: pfx_add(PX_RING, x, y, 0, 0, 420, c); break;
    case 3: for (i = 0; i < 2; i++) pfx_add(PX_NOTE, x + Q8(i * 12 - 6), y - Q8(8), i ? 22 : -22, -52, 850, c); break;
    case 4: for (i = 0; i < 5; i++) { a = i * 13107 + (int32_t)(ac_rnd() % 6000u); pfx_add(PX_STAR, x, y - Q8(4), (icos(a) * 70) >> 14, ((isin(a) * 70) >> 14) - 90, 600, c); } break;
    case 5: for (i = 0; i < 4; i++) pfx_add(PX_BUB, x + Q8((int32_t)(ac_rnd() % 17u) - 8), y, 0, -36 - (int32_t)(ac_rnd() % 30u), 950, c); break;
    case 6: for (i = 0; i < 2; i++) pfx_add(PX_HEART, x + Q8(i * 14 - 7), y - Q8(8), i ? 16 : -16, -46, 850, c); break;
    case 7: for (i = 0; i < 3; i++) pfx_add(PX_ZAP, x + Q8(i * 10 - 10), y - Q8(8), 0, 0, 200, c); break;
    }
}
static void deco_key_fx(int zone, int32_t x, int32_t y) { deco_fx_kind(pf.zone[zone & 3][1], pf_zone_col(zone, fm1_ms), x, y); }
/* rolling on the ground at (x, y foot) with speed vx (Q8 px/s): what he leaves behind, a piece every few pixels */
static int32_t deco_trail_px;
static void deco_trail(int32_t x, int32_t yfoot, int32_t vx, int32_t dt)
{
    int kind = pf.eq[3];
    int32_t back = vx > 0 ? -1 : 1;
    if (!kind || iabs(vx) < Q8(24)) return;
    deco_trail_px += iabs(vx) * dt / 1000;
    if (deco_trail_px < Q8(kind == 6 ? 4 : 9)) return;
    deco_trail_px = 0;
    x += back * Q8(7);
    switch (kind) {
    case 1: pfx_add(PX_DUST, x, yfoot - Q8(2), back * 26, -18, 480, th->mute); break;
    case 2: pfx_add(PX_BUB, x, yfoot - Q8(6), back * 8, -30, 900, HEXC(0x7fe6ff)); break;
    case 3: pfx_add(PX_NOTE, x, yfoot - Q8(10), back * 10, -34, 800, th->caps[(fm1_ms / 200u) % 5u]); break;
    case 4: pfx_add(ac_rnd() & 1u ? PX_STAR : PX_DOT, x + Q8((int32_t)(ac_rnd() % 9u) - 4), yfoot - Q8(4 + (int32_t)(ac_rnd() % 12u)), back * 12, -70, 520, ac_rnd() & 1u ? HEXC(0xffffff) : HEXC(0xffe060)); break;
    case 5: pfx_add(PX_FLAME, x, yfoot - Q8(3), back * 14, -44, 420, 0); break;
    case 6: pfx_add(PX_BOW, x, yfoot - Q8(8), 0, 0, 620, 0); break;
    }
}

/* ---------------------------------------------------------------- how he jumps
 * The jump itself is the same for everyone (how high, how far); what the key's colour wears is how it looks. */
static struct { int32_t spin, grow, t, hx[3], hy[3], ha[3]; uint8_t style, on, hn; } pj;
static void deco_jump_style(int style, int zone, int32_t x, int32_t y)
{
    int32_t i;
    pj.style = (uint8_t)style; pj.t = 0; pj.on = 1; pj.hn = 0;
    if (pj.style == 6) for (i = 0; i < 6; i++) pfx_add(PX_STAR, x, y, (icos(i * 10922) * 90) >> 14, ((isin(i * 10922) * 60) >> 14) - 40, 600, pf_zone_col(zone, fm1_ms + (uint32_t)i * 110u));
    else if (pj.style == 4) pfx_add(PX_RING, x, y + Q8(6), 0, 0, 380, pf_zone_col(zone, fm1_ms));
}
static void deco_jump(int zone, int32_t x, int32_t y)   /* Rock leaves the ground at (x, y) (Q8) by a key of this colour */
{
    deco_jump_style(pf.zone[zone & 3][3], zone, x, y);
}
static void deco_jump_tick(int32_t dt, int grounded, int32_t x, int32_t y)
{
    if (pj.on) {
        pj.t += dt;
        switch (pj.style) {
        case 1: pj.spin += dt * 65536 / 520; break;                       /* FLIP: over forwards */
        case 2: pj.spin -= dt * 65536 / 520; break;                       /* BACKFLIP */
        case 3: pj.spin += dt * 65536 / 240; break;                       /* TWIRL: a blur */
        case 4: pj.grow = isin(imin(pj.t, 700) * 32768 / 700) * 110 >> 14; break;   /* BIG AIR: larger at the top */
        case 6: pj.spin += dt * 65536 / 380; if ((pj.t / 70) != ((pj.t - dt) / 70)) pfx_add(PX_STAR, x, y, 0, 20, 420, pf_zone_col((int)(pj.t / 70) & 3, fm1_ms)); break;   /* STAR JUMP */
        }
        if (grounded && pj.t > 140) { pj.on = 0; pj.grow = 0; }
    } else if (pj.spin) {                        /* landed mid-turn: round to upright, quickly */
        int32_t s = pj.spin & 0xFFFF;
        if (s > 32768) s -= 65536;
        s -= s * imin(dt, 90) / 90;
        pj.spin = iabs(s) < 600 ? 0 : s;
    }
}
/* the player's own Rock, as he is worn: standing in a picture ... */
static void prock_draw(int32_t x, int32_t y, int32_t z, int32_t a, int32_t eyes)
{
    deco_rock(x, y, z, a, eyes, pf.eq, fm1_ms);
}
/* ... and where he rolls and jumps (the cave, the course, the music room): with the jump he wears */
static void deco_rock_live(int32_t x, int32_t y, int32_t z, int32_t a, int32_t eyes, const uint8_t *eq)
{
    uint32_t a0 = ga;
    int32_t i;
    if (pj.on && pj.style == 5) {                /* GHOST: three of him behind, fainter the further back */
        for (i = imin(pj.hn, 3) - 1; i >= 0; i--) { ga = (a0 * (uint32_t)(90 - i * 24)) >> 8; deco_rock(pj.hx[i], pj.hy[i], z, pj.ha[i], 0, eq, fm1_ms); }
        ga = a0;
        pj.hx[2] = pj.hx[1]; pj.hy[2] = pj.hy[1]; pj.ha[2] = pj.ha[1]; pj.hx[1] = pj.hx[0]; pj.hy[1] = pj.hy[0]; pj.ha[1] = pj.ha[0];
        pj.hx[0] = x; pj.hy[0] = y; pj.ha[0] = a; if (pj.hn < 3) pj.hn++;
    }
    deco_rock(x, y, z * (256 + pj.grow) >> 8, a + pj.spin, eyes, eq, fm1_ms);
}
static void prock_live(int32_t x, int32_t y, int32_t z, int32_t a, int32_t eyes) { deco_rock_live(x, y, z, a, eyes, pf.eq); }

/* ---------------------------------------------------------------- the tickets out of the machine */
static void dnum(char *b, int32_t v)             /* a number as text */
{
    char t[12]; int32_t n = 0, k = 0;
    if (v <= 0) t[n++] = '0';
    while (v > 0 && n < 10) { t[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) b[k++] = t[--n];
    b[k] = 0;
}
#define TIX_PAPER HEXC(0xf6e27a)
#define TIX_EDGE HEXC(0xc9a93a)
/* one ticket, w x h with its top left at (x, y): paper, a notch at each side, a mark in the middle */
static void tix_one(int32_t x, int32_t y, int32_t w, int32_t h, col_t mark)
{
    grect(x, y, w, h, TIX_PAPER);
    grect(x, y, w, 1, TIX_EDGE); grect(x, y + h - 1, w, 1, TIX_EDGE);
    if (h >= 9) { grect(x, y + h / 2 - 1, 2, 3, TIX_EDGE); grect(x + w - 2, y + h / 2 - 1, 2, 3, TIX_EDGE); }
    if (w >= 20) { gdisc(x + w / 2, y + h / 2, imax(1, h / 4), mark); grect(x + 5, y + h / 2, 4, 1, TIX_EDGE); grect(x + w - 9, y + h / 2, 4, 1, TIX_EDGE); }
    else grect(x + w / 2 - 1, y + h / 2 - 1, 2, 2, mark);
}
/* the pocket: a small ticket and how many there are, its right edge at xr; bump: a moment larger (0..256) */
static void tix_pocket(int32_t xr, int32_t y, uint32_t n, int32_t sc, col_t c)
{
    char b[12];
    int32_t w;
    dnum(b, (int32_t)n);
    w = gtextw(b, sc);
    gtext(xr - w, y, b, c, sc);
    tix_one(xr - w - 4 - 7 * sc, y - sc, 7 * sc, 5 * sc + sc, th->caps[0]);
}
static void tix_tick(int32_t dt)
{
    int32_t was, now, step, feed;
    if (!tix.on) return;
    step = tix_step_ms(); feed = tix_feed_ms();
    was = tix.t < 0 ? 0 : imin(tix.n, tix.t / step);
    tix.t += dt;
    now = tix.t < 0 ? 0 : imin(tix.n, tix.t / step);
    if (now > was) sfx(76 + (now % 4) * 3, 0, 36, W_SQUARE, 700, 1, 20);          /* each one clicks out */
    if (tix.t >= feed && tix.t - dt < feed) { sfx(84, 0, 90, W_SQUARE, 1300, 1, 60); sfx(91, 80, 90, W_SQUARE, 1300, 1, 60); sfx(96, 160, 700, W_SINE, 2600, 2, 200); }
    if (tix.t >= feed + TIX_TAIL) tix.on = 0;
}
static void tix_skip(void) { if (tix.on) tix.t = imax(tix.t, tix_feed_ms() + TIX_TAIL - 650); }   /* PLAY: straight to the sum */
static void tix_draw(void)
{
    char b[12];
    int32_t step, feed, e, len, u, i, y, al = 256, sx = 101, sw = 38, th_ = 15, top = 48, vis = 7 * 15;
    uint32_t tm = fm1_ms;
    if (!tix.on) return;
    step = tix_step_ms(); feed = tix_feed_ms();
    u = tix.t - feed;                            /* > 0: the feeding is over */
    if (u > TIX_TAIL - 250) al = imax(0, (TIX_TAIL - u) * 256 / 250);
    if (tix.t < 0) al = imax(0, 256 + tix.t * 256 / 350);
    gveil(th->bg, (uint32_t)(29 * al / 256));   /* (the card under it nearly gone: the strip and the count are what is read) */
    galpha(al);
    e = tix.t < 0 ? 0 : imin(tix.n, tix.t / step);
    len = tix.t < 0 ? 0 : imin(tix.n * th_, tix.t * th_ / step);   /* how much strip is out, in pixels */
    /* the machine's slot: a plate in its colour, the slit the strip comes out of */
    grect(70, 26, 100, 26, th->panel); gbox(70, 26, 100, 26, tix.col); gbox(71, 27, 98, 24, tix.col);
    grect(82, 43, 76, 5, 0); grect(82, 42, 76, 1, th->mute);
    gdisc(77, 32, 1, th->mute); gdisc(163, 32, 1, th->mute);
    gfit(120, 31, 70, TR(T_TICKETS), th->text, 1);
    if (u < 450) {                               /* the strip (until it is torn off) */
        int32_t fly = u > 0 ? ease8(u * 256 / 450) : 0, sh = imin(len, vis), folds = imin(10, (len - sh) / th_);
        if (fly) galpha(al * (256 - fly) / 256);
        gclip_y0 = top; gclip_y1 = top + sh + 1;
        for (i = 0; i * th_ < sh + 2 * th_; i++) {   /* (past the seventh it goes on sliding: into the pile) */
            y = top + sh - (i + 1) * th_ + (len > vis ? len % th_ : 0);
            tix_one(sx + (tsin(tm, 50, y * 500) >> 13) - fly * 60 / 256, y, sw, th_, tix.col);
        }
        gclip_y0 = 0; gclip_y1 = AC_H;
        for (i = 0; i < folds; i++)              /* what is past the seventh folds itself into a pile */
            tix_one(sx - 3 + ((i & 1) ? 6 : 0) - fly * 60 / 256, top + vis + 22 - i * 3, sw, 4, tix.col);
        if (len > vis) grect(sx + 2 - fly * 60 / 256, top + vis, sw - 4, 22 - imin(folds, 7) * 3, TIX_PAPER);
        galpha(al);
    }
    /* how many so far, large, to the right; the pocket to the left, filling */
    b[0] = '+'; dnum(b + 1, e);
    gtextc(196, 82 - (e && tix.t % step < 60 ? 2 : 0), b, th->text, 4);
    tix_pocket(84, 86, tix.before + (uint32_t)e, 2, th->pad);
    if (u > 300) {                               /* the sum, across the middle */
        int32_t a2 = imin(256, (u - 300) * 256 / 250), w;
        galpha(al * a2 / 256);
        grect(20, 142, 200, 56, th->panel); gbox(20, 142, 200, 56, tix.col);
        dnum(b, (int32_t)pf.tickets);
        w = gtextw(b, 4);
        tix_one(120 - (w + 36) / 2, 158, 28, 20, tix.col);
        gtext(120 - (w + 36) / 2 + 36, 160, b, th->text, 4);
    }
    galpha(256);
}
