/* SPDX-License-Identifier: GPL-3.0-only */
/* UNDERSONG's ONE VOICE (2026-11-25, built from the preview in docs/undersong-proposals: proto/um_voice.h). A game for
 * two, on the list beside the story's games. Included by ac_duo.h.
 *
 *   the game     the two players are one creature, a Hum, on its way home down a tunnel. Neither has a rock to move:
 *                one of them is how high the Hum floats (a key held: the four colours of the keys, low to high; no key
 *                and it lies flat on the floor), the other is its shape (a knob: round, square, spiky). Walls of crystal
 *                come one after another, each with a hole: how high (its rim in the colour of the keys that sing it
 *                there) and what shape. Both right as the wall arrives and it rings, and a light joins the Hum's tail;
 *                either wrong and it bonks, and the wall crumbles. After the eighth wall the rocks change sides, and jobs.
 *   how it is told   a wall is a note for both (du_n: lane its height, hid its shape). Each FM-1 judges its own
 *                player's half as the wall arrives and says so (DO_HIT / DO_MISS, as for any note); the wall waits at
 *                the Hum the moment that takes, then rings or breaks. The key held and the knob go to the other screen
 *                as ever (CC 112-114), so both see one Hum.
 *   the opening  a little story, the first time (PLAY skips it): a pool, a drop, a ripple that lifts off the water and
 *                opens two eyes. It cannot hold itself; Pink's note steadies how high it is, Teal's its edge. Its
 *                family calls from down the tunnel, behind the walls.
 *   not yet      for three; a lesson of its own (the first walls of each half say what is yours); its best is kept in
 *                the one spare byte the save had beside UNDERSONG's chapters, in twenties. */

#define HM_N 40                                  /* the corners of its outline */
#define HM_VX 120                                /* where the tunnel runs to */
#define HM_VY 100
#define HM_WALLS 16
#define HM_HALF 8                                /* after this many the rocks swap */
#define HM_SPEED 3                               /* how fast the tunnel comes: depth a tick */
#define HM_SWAP0 (136 * 24 + 96)                 /* the swap: from a beat after the eighth wall, a bar long */
#define HM_SWAP1 (HM_SWAP0 + 384)
static const int16_t HM_LY[4] = {166, 130, 94, 58};   /* the four heights where the Hum is, low to high */
static const uint16_t HM_AT[HM_WALLS] = {32, 56, 72, 88, 104, 116, 128, 136, 176, 192, 208, 220, 232, 244, 252, 260};   /* when
                                                    each wall arrives (sixteenths): closer together as each half goes on */
static const uint8_t HM_K[4] = {3, 10, 16, 23};  /* (tests) a key of each colour */
static const col_t HM_GLOW = AC_RGB(255, 206, 96), HM_BODY = AC_RGB(255, 230, 140), HM_CORE = AC_RGB(255, 252, 226), HM_INK = AC_RGB(74, 40, 10);
static struct {
    int8_t key[2];                               /* each seat's key that counts: the newest still held (-1: none) */
    uint32_t prev[2];
    uint8_t jn, res, kept, res_ok, role, sl, ss, init;   /* walls judged here, walls settled, rung; the last one rang;
                                                    who is how high; what was last heard (lane + 1, shape) */
    int8_t bot;                                  /* (tests) the key a seat that plays by itself holds */
    uint32_t res_ms, draw_ms;
    int32_t y, shape, flat;                      /* the Hum as drawn: y (Q8), shape 0..512, how flat 0..256 */
} hm;
static int32_t hm_xy[(HM_N + 6) * 2];            /* (an outline, or a wall with its hole: not on the stack) */

/* (kept out of line, as dw_ramp is: an opening's times folded into constants look like ROM addresses to the build) */
__attribute__((noinline)) static int32_t hm_fade(int32_t t, int32_t a, int32_t b) { return iclamp((t - a) * 256 / imax(1, b - a), 0, 256); }
__attribute__((noinline)) static int32_t hm_hop(int32_t t, int32_t at, int32_t len) { return dw_hop(t, at, len); }
static int32_t hm_lerp(int32_t a, int32_t b, int32_t f) { return a + (b - a) * f / 256; }
static void hm_spark(int32_t x, int32_t y, int32_t r, col_t c)
{
    gline(x - r, y, x + r, y, c); gline(x, y - r, x, y + r, c);
    if (r > 2) { gp(x - 1, y - 1, c); gp(x + 1, y - 1, c); gp(x - 1, y + 1, c); gp(x + 1, y + 1, c); }
}
static void hm_dark(int32_t a) { if (a > 0) { galpha(a); grect(0, 0, AC_W, AC_H, AC_RGB(0, 0, 0)); galpha(256); } }

/* ---------------------------------------------------------------- the Hum */
/* its outline about (cx, cy): radius r, shape 0 round .. 256 square .. 512 spiky (five points), turned by rot, squashed
 * by sx, sy (Q8). A hole in a wall is the same outline, so a shape fits its hole exactly */
static void hm_outline(int32_t *xy, int32_t cx, int32_t cy, int32_t r, int32_t shape, int32_t rot, int32_t sx, int32_t sy)
{
    int32_t k;
    shape = iclamp(shape, 0, 512);
    for (k = 0; k < HM_N; k++) {
        int32_t a = k * 65536 / HM_N, ph = (a * 5) & 65535;
        int32_t tri = ph < 32768 ? 256 - ph / 64 : -256 + (ph - 32768) / 64, f, rr;
        int32_t sq = 218 * 16384 / imax(iabs(icos(a)), iabs(isin(a))), star = 256 + 100 * tri / 256;   /* each as a part of r, Q8 */
        f = shape <= 256 ? 256 + (sq - 256) * shape / 256 : sq + (star - sq) * (shape - 256) / 256;
        rr = r * f;                              /* Q8 */
        xy[2 * k] = cx + ((((icos(a + rot - 16384) >> 6) * rr) >> 8) * sx >> 16);
        xy[2 * k + 1] = cy + ((((isin(a + rot - 16384) >> 6) * rr) >> 8) * sy >> 16);
    }
}
/* face: 0 seen from behind, 1 its face. mood: 0 calm, 1 happy, 2 sad, 3 dizzy, 4 wide-eyed. look: its eyes aside, px */
static void hm_hum(int32_t x, int32_t y, int32_t r, int32_t shape, int32_t rot, int32_t sx, int32_t sy, int face, int mood, int32_t look, uint32_t tm)
{
    int32_t *xy = hm_xy, g = 30 + ipulse(tm, 700) * 30 / 256, k;
    int32_t ex = imax(2, r * sx / 256 * 34 / 100), ey = y - r * sy / 256 * 12 / 100, er = imax(1, r / 7), my = y + r * sy / 256 * 30 / 100;
    galpha(g); gellf(x, y, (r + 9) * sx / 256, (r + 9) * sy / 256, HM_GLOW, 0);
    galpha(g + 30); gellf(x, y, (r + 4) * sx / 256, (r + 4) * sy / 256, HM_GLOW, 0); galpha(256);
    hm_outline(xy, x, y, r, shape, rot, sx, sy); gpoly(xy, HM_N, HM_BODY);
    galpha(170); gpolyline(xy, HM_N, 1, AC_RGB(255, 255, 255)); galpha(256);
    hm_outline(xy, x, y, r * 5 / 8, shape, rot, sx, sy); gpoly(xy, HM_N, HM_CORE);
    if (!face) return;
    x += look;
    for (k = -1; k <= 1; k += 2) {
        int32_t px = x + k * ex;
        if (mood == 1) garc(px, ey + 1, er + 1, er + 1, 32768, 65536, HM_INK, r > 14);          /* shut, happy */
        else if (mood == 3) { gline(px - er, ey - er, px + er, ey + er, HM_INK); gline(px - er, ey + er, px + er, ey - er, HM_INK); }
        else if ((tm % 2900u) < 110u) ghl(px - er, px + er + 1, ey, HM_INK);
        else if (mood == 4) { gdisc(px, ey, er + 2, AC_RGB(255, 255, 255)); gdisc(px, ey, imax(1, er - 1), HM_INK); }
        else { gdisc(px, ey, er, HM_INK); gp(px - er / 2, ey - er / 2, AC_RGB(255, 255, 255)); }
        if (mood == 2) gline(px - k * (er + 2), ey - er - 4, px + k * er, ey - er - 1, HM_INK);  /* sad brows */
        if (mood == 1 && r > 12) { galpha(120); gellf(px + k * er, ey + er + 3, er + 1, imax(1, er / 2), AC_RGB(255, 120, 150), 0); galpha(256); }
    }
    if (mood == 1) garc(x, my - 2, imax(2, r / 4), imax(2, r / 5), 0, 32768, HM_INK, r > 14);
    else if (mood == 2) garc(x, my + 2, imax(2, r / 5), imax(1, r / 7), 32768, 65536, HM_INK, 0);
    else if (mood == 3) { gline(x - 4, my, x - 2, my - 2, HM_INK); gline(x - 2, my - 2, x + 1, my + 1, HM_INK); gline(x + 1, my + 1, x + 4, my - 1, HM_INK); }
    else if (mood == 4) gdisc(x, my, imax(1, r / 8), HM_INK);
    else ghl(x - 2, x + 3, my, HM_INK);
    if (mood == 2) { int32_t d = (int32_t)(tm % 1200u); galpha(256 - d * 256 / 1200); gdisc(x + ex + er + 1, ey + 3 + d / 100, 1, AC_RGB(150, 210, 255)); galpha(256); }
}

/* ---------------------------------------------------------------- the tunnel, coming at you */
/* depth z: 256 is where the Hum is; a thing at z is drawn at 65536 / z (Q8) of its size there */
static int32_t hm_s(int32_t z) { return 65536 / imax(z, 40); }
static void hm_tunnel(int32_t run, int32_t home, uint32_t tm)
{
    static const col_t BG = AC_RGB(3, 5, 14);
    int32_t k, xy[8], m0 = ((-run) % 200 + 200) % 200;
    gclear(BG);
    xy[0] = HM_VX - 10; xy[1] = HM_VY - 8; xy[2] = HM_VX - 10; xy[3] = HM_VY + 8; xy[4] = -54; xy[5] = 240; xy[6] = -54; xy[7] = -37;
    gpoly(xy, 4, AC_RGB(6, 10, 24));             /* its sides, */
    xy[0] = HM_VX + 10; xy[2] = HM_VX + 10; xy[4] = 294; xy[6] = 294; gpoly(xy, 4, AC_RGB(6, 10, 24));
    xy[0] = HM_VX - 10; xy[1] = HM_VY + 8; xy[2] = HM_VX + 10; xy[3] = HM_VY + 8; xy[4] = 294; xy[5] = 240; xy[6] = -54; xy[7] = 240;
    gpoly(xy, 4, AC_RGB(9, 16, 34));             /* and floor */
    for (k = 13; k >= 0; k--) {                  /* its ribs, crystals at their feet and teeth in the roof: the far ones first */
        int32_t z = 140 + m0 + k * 200, s = hm_s(z), fog = iclamp((z - 200) * 256 / 2500, 0, 256);
        int32_t x0 = HM_VX - 124 * s / 256, x1 = HM_VX + 124 * s / 256, y0 = HM_VY - 98 * s / 256, y1 = HM_VY + 100 * s / 256;
        uint32_t h = ihash((uint32_t)((run + m0 + k * 200) / 200) * 31u + 7u);
        col_t c = dw_mix(AC_RGB(52, 80, 136), BG, fog), cc = dw_mix(th->caps[h % 5u], BG, iclamp(fog + 60, 0, 256));
        gbox(x0, y0, x1 - x0, y1 - y0, c);
        if (s > 150) gbox(x0 + 1, y0 + 1, x1 - x0 - 2, y1 - y0 - 2, c);
        ghl(x0, x1, y1, dw_mix(AC_RGB(90, 130, 190), BG, fog));
        xy[0] = x0; xy[1] = y1; xy[2] = x0 + 14 * s / 256; xy[3] = y1; xy[4] = x0 + 5 * s / 256; xy[5] = y1 - (18 + (int32_t)(h >> 4) % 16) * s / 256; gpoly(xy, 3, cc);
        xy[0] = x1; xy[2] = x1 - 14 * s / 256; xy[4] = x1 - 5 * s / 256; xy[5] = y1 - (16 + (int32_t)(h >> 8) % 18) * s / 256; gpoly(xy, 3, cc);
        xy[0] = HM_VX - (80 - (int32_t)(h >> 6) % 160) * s / 256; xy[1] = y0; xy[2] = xy[0] + 16 * s / 256; xy[3] = y0; xy[4] = xy[0] + 8 * s / 256; xy[5] = y0 + (14 + (int32_t)(h >> 10) % 16) * s / 256;
        gpoly(xy, 3, dw_mix(AC_RGB(26, 42, 76), BG, fog));
    }
    galpha(home * 60 / 256); gdisc(HM_VX, HM_VY, 30, HM_GLOW);            /* the far end: home, a warm light */
    galpha(home * 110 / 256); gdisc(HM_VX, HM_VY, 17, HM_GLOW);
    galpha(home); gdisc(HM_VX, HM_VY, 8, HM_BODY);
    for (k = 0; k < 5; k++) {                    /* the others, tiny, bobbing in it */
        int32_t a = (int32_t)(tm * 8u) + k * 13107;
        gdisc(HM_VX + (icos(a) * 6 >> 14), HM_VY + (isin(a * 2 + k * 9000) * 3 >> 14), 1, HM_CORE);
    }
    galpha(256);
}
/* a wall of crystal at depth z, its hole at height lane, of shape (0 round, 256 square, 512 spiky); a: how solid.
 * One shape with the hole in it: the frame and the hole's outline joined by a seam the fill cancels */
static void hm_wall(int32_t z, int lane, int32_t shape, int32_t a, int32_t flash, uint32_t tm)
{
    int32_t s = hm_s(z), fog = iclamp((z - 256) * 256 / 1500, 0, 256), *xy = hm_xy;
    int32_t x0 = HM_VX - 124 * s / 256, x1 = HM_VX + 124 * s / 256, y0 = HM_VY - 98 * s / 256, y1 = HM_VY + 100 * s / 256;
    int32_t hy = HM_VY + (HM_LY[lane & 3] - HM_VY) * s / 256, hr = 31 * s / 256;
    col_t c = dw_mix(AC_RGB(44, 84, 150), AC_RGB(10, 18, 42), fog), rim = dw_mix(GH_ZONE[lane & 3], AC_RGB(10, 18, 42), fog / 2);
    xy[0] = x0; xy[1] = y0; xy[2] = x1; xy[3] = y0; xy[4] = x1; xy[5] = y1; xy[6] = x0; xy[7] = y1; xy[8] = x0; xy[9] = y0;
    hm_outline(&xy[10], HM_VX, hy, hr, shape, 0, 256, 256);
    xy[10 + HM_N * 2] = xy[10]; xy[11 + HM_N * 2] = xy[11];
    galpha(a * (180 - fog * 80 / 256) / 256); gpoly(xy, HM_N + 6, c);
    if (flash > 0) { galpha(flash * a / 256 * 80 / 256); gpoly(xy, HM_N + 6, AC_RGB(255, 255, 255)); }
    galpha(a * 150 / 256); gbox(x0, y0, x1 - x0, y1 - y0, dw_mix(AC_RGB(150, 200, 255), AC_RGB(10, 18, 42), fog));
    if (s > 40) {                                /* facets: a lighter corner top left, a darker one bottom right */
        int32_t t3[6];
        t3[0] = x0; t3[1] = y0; t3[2] = x0 + 60 * s / 256; t3[3] = y0; t3[4] = x0; t3[5] = y0 + 70 * s / 256;
        galpha(a * 40 / 256); gpoly(t3, 3, AC_RGB(190, 226, 255));
        t3[0] = x1; t3[1] = y1; t3[2] = x1 - 70 * s / 256; t3[3] = y1; t3[4] = x1; t3[5] = y1 - 50 * s / 256;
        galpha(a * 70 / 256); gpoly(t3, 3, AC_RGB(6, 10, 30));
    }
    galpha(a);
    gpolyline(&xy[10], HM_N, 1, rim);
    if (hr > 8) { hm_outline(&xy[10], HM_VX, hy, hr + 1, shape, 0, 256, 256); gpolyline(&xy[10], HM_N, 1, rim); }
    if (hr > 14) {
        galpha(a * (60 + ipulse(tm, 600) * 60 / 256) / 256);
        hm_outline(&xy[10], HM_VX, hy, hr + 3, shape, 0, 256, 256); gpolyline(&xy[10], HM_N, 1, rim);
    }
    galpha(256);
}
/* a wall that did not let it through: its pieces, q ms after */
static void hm_shards(int lane, int32_t q)
{
    int32_t k, xy[6];
    if (q < 0 || q > 900) return;
    galpha(256 - q * 256 / 900);
    for (k = 0; k < 16; k++) {
        uint32_t h = ihash((uint32_t)k * 77u + 5u);
        int32_t a = k * 4096 + (int32_t)(h % 3000u), d = 30 + (int32_t)(h >> 4) % 70 + q * (60 + (int32_t)(h >> 8) % 90) / 400;
        int32_t x = HM_VX + (icos(a) * d >> 14), y = HM_LY[lane & 3] + (isin(a) * d >> 14) + q * q / 2600, sz = 5 + (int32_t)(h >> 10) % 9;
        xy[0] = x; xy[1] = y - sz; xy[2] = x + sz; xy[3] = y + sz / 2; xy[4] = x - sz / 2; xy[5] = y + sz;
        gpoly(xy, 3, k % 3 ? AC_RGB(44, 84, 150) : AC_RGB(120, 170, 230));
    }
    galpha(256);
}
/* the two things carved into the tunnel where the Hum is: on the left four lamps, low to high, in the colours of the
 * keys (the one being sung lit); on the right the three shapes, round, square, spiky (the one it has lit). mine: the
 * side that is this player's to do (0 left, 1 right) breathes */
static void hm_marks(int lane, int shape_i, int mine, uint32_t tm)
{
    int32_t k, *xy = hm_xy, br = ipulse(tm, 900) * 70 / 256;
    for (k = 0; k < 4; k++) {
        int32_t y = HM_LY[k], d[8];
        d[0] = 9; d[1] = y - 7; d[2] = 14; d[3] = y; d[4] = 9; d[5] = y + 7; d[6] = 4; d[7] = y;
        if (k == lane) { galpha(70 + ipulse(tm, 500) * 50 / 256); gdisc(9, y, 11, GH_ZONE[k]); galpha(256); }
        galpha(k == lane ? 256 : 80 + (mine == 0 ? br : 0)); gpoly(d, 4, GH_ZONE[k]); galpha(256);
    }
    for (k = 0; k < 3; k++) {
        int32_t y = 70 + k * 42;
        if (k == shape_i) { galpha(60 + ipulse(tm, 500) * 50 / 256); gdisc(230, y, 12, th->text); galpha(256); }
        hm_outline(xy, 230, y, 7, k * 256, 0, 256, 256);
        galpha(k == shape_i ? 256 : 90 + (mine == 1 ? br : 0)); gpolyline(xy, HM_N, 1, k == shape_i ? AC_RGB(255, 255, 255) : AC_RGB(120, 150, 190)); galpha(256);
    }
}
/* a rock's voice going up to the Hum: a string of lights in its colour, climbing */
static void hm_thread(int32_t x0, int32_t y0, int32_t x1, int32_t y1, col_t c, uint32_t tm)
{
    int32_t k;
    for (k = 0; k < 9; k++) {
        int32_t f = (k * 256 / 9 + (int32_t)((tm / 5u) % 28u)) % 256, g = 256 - f;
        int32_t x = (g * g * x0 + 2 * g * f * x0 + f * f * x1) >> 16, y = (g * g * y0 + 2 * g * f * y1 + f * f * y1) >> 16;
        galpha(60 + f * 150 / 256); gdisc(x, y, f > 200 ? 0 : 1, c);
    }
    galpha(256);
}

/* ---------------------------------------------------------------- the game */
static int hm_high(int32_t now) { return now >= (HM_SWAP0 + HM_SWAP1) / 2; }   /* the seat that is how high it is: 0, then 1 */
static int hm_lane(int s) { return hm.key[s & 1] >= 0 ? KEY_ZONE[hm.key[s & 1]] : -1; }
static int32_t hm_pitch(int lane) { return mus_ladder(&SONGS[DU_TRACK[DM_HUM]], 5 + (lane & 3) * 2) + 12; }
/* a seat's keys as they are now: the one that counts is the newest still held */
static void hm_follow(int s, uint32_t held)
{
    uint32_t nw = held & ~hm.prev[s];
    int k;
    hm.prev[s] = held;
    if (nw) { for (k = 0; k < AC_NKEY; k++) if (nw >> k & 1u) hm.key[s] = (int8_t)k; }
    else if (hm.key[s] >= 0 && !(held >> hm.key[s] & 1u)) {
        hm.key[s] = -1;
        for (k = 0; k < AC_NKEY; k++) if (held >> k & 1u) hm.key[s] = (int8_t)k;
    }
}
static void hm_keys(void)
{
    int me = du.seat & 1;
    hm_follow(me, in.keys & 0x7FFFFFFu); hm_follow(!me, du.pkeys[!me]);
#ifdef AC_TEST_CATCH
    hm.key[me] = hm.bot;
#endif
}
/* its walls: the same on both FM-1s, all from the seed. The first only asks for a height (the Hum starts round), the
 * second for both; after that the height always changes, the shape two times in three */
static uint32_t hm_build(uint32_t seed)
{
    uint32_t i, lane = 1, shape = 0, h;
    for (i = 0; i < sizeof hm; i++) ((uint8_t *)&hm)[i] = 0;
    hm.key[0] = hm.key[1] = -1; hm.bot = -1;
    du.dial[0] = du.dial[1] = du.dial[2] = 0;
    for (i = 0; i < HM_WALLS; i++) {
        h = ihash(seed * 37u + i * 101u + 11u);
        if (i) { lane = (lane + 1u + h % 3u) & 3u; if (i == 1u || (h >> 4) % 3u) shape = (shape + 1u + (h >> 8) % 2u) % 3u; }
        du_add(HM_AT[i], (int)lane, 3, (int)shape, hm_pitch((int)lane));
    }
    return HM_AT[HM_WALLS - 1] + 26u;
}
static void hm_tick(int32_t now)
{
    int me = du.seat & 1, fr = !me, high = hm_high(now), lane, shape, s;
    hm_keys();
    if (hm.role != high) {                       /* the swap: whoever is its shape now takes it as it is */
        hm.role = (uint8_t)high;
        if (me != high) { du.dial[me] = (uint8_t)(du.dial[fr] % 3u); du_dial_tx(du.dial[me]); }
        sfx(79, 0, 120, W_TRI, 1800, 4, 100); sfx(84, 120, 120, W_TRI, 1800, 4, 100); sfx(91, 240, 300, W_TRI, 1800, 4, 100);
    }
    if (me != high && in.any_knob) {             /* yours is its shape: a knob, round and round */
        du.dial[me] = (uint8_t)((du.dial[me] % 3u + 3u + (in.any_knob > 0 ? 1u : 2u)) % 3u);
        du_dial_tx(du.dial[me]);
    }
    if (hm.jn < du.n) {                          /* (tests) a seat that plays by itself is ready for the next wall in good time */
        const du_note_t *n = &du_n[hm.jn];
        int hs = hm.jn >= HM_HALF;
        if (n->step * 24 - now < 200) for (s = 0; s < 2; s++) {
            int bot = 0;
#ifdef AC_TEST_CATCH
            if (s == me) bot = 1;
#endif
#ifdef AC_TEST_DUO
            if (s != me) bot = 1;
#endif
            if (!bot) continue;
            if (s == hs) { if (s == me) hm.bot = hm.key[me] = (int8_t)HM_K[n->lane & 3]; else du.pkeys[s] = 1u << HM_K[n->lane & 3]; }
            else if (du.dial[s] % 3u != n->hid) { du.dial[s] = n->hid; if (s == me) du_dial_tx(n->hid); }
        }
    }
    while (hm.jn < du.n && now >= du_n[hm.jn].step * 24) {   /* a wall is here: your half of it, right or wrong, and said */
        const du_note_t *n = &du_n[hm.jn];
        int i = hm.jn++, ok = me == (i >= HM_HALF) ? hm_lane(me) == n->lane : du.dial[me] % 3u == n->hid;
        du_set(i, me, ok ? 1 : 3);
        du_tx(ok ? DO_HIT : DO_MISS, (uint32_t)i & 127u, ok ? 1u : 0u, (uint32_t)i >> 7);
#ifdef AC_TEST_DUO
        du_set(i, fr, 1);
#endif
    }
    while (hm.res < hm.jn) {                     /* and your friend's half heard (half a beat with no word: it was wrong) */
        du_note_t *n = &du_n[hm.res];
        int ok;
        if (!n->st[fr]) { if (now < n->step * 24 + 48) break; n->st[fr] = 3; }
        ok = n->st[0] == 1u && n->st[1] == 1u;
        hm.res_ok = (uint8_t)ok; hm.res_ms = fm1_ms; hm.res++;
        if (ok) {
            hm.kept++;
            sfx(n->pitch, 0, 800, W_TRI, 2600, 6, 150); sfx(n->pitch + 7, 90, 700, W_TRI, 2000, 6, 150); sfx(n->pitch + 12, 180, 900, W_SINE, 2000, 6, 150);
        } else { du_sour(n->pitch - 12); sfx(40, 0, 180, W_SINE, 2600, 1, 20); sfx(88, 60, 50, W_NOISE, 700, 1, 30); }
#ifdef DINGHY_HOST
        printf("%6u ms  ONE VOICE: wall %d (height %d, shape %d) %s\n", fm1_ms, hm.res, n->lane, n->hid,
               ok ? "rings" : n->st[0] == 1u || n->st[1] == 1u ? "breaks: one half was wrong" : "breaks: both halves were wrong");
#endif
    }
    lane = hm_lane(high); shape = du.dial[!high] % 3u;
    if (lane + 1 != hm.sl) {                     /* it is heard: its note where it floats now; a sigh when it sinks */
        hm.sl = (uint8_t)(lane + 1);
        if (lane >= 0) sfx(hm_pitch(lane), 0, 300, W_SINE, high == me ? 1400 : 900, 12, 100);
        else sfx(45, 0, 160, W_SINE, 700, 20, 40);
    }
    if (shape != hm.ss) { hm.ss = (uint8_t)shape; sfx(88 + shape * 4, 0, 40, W_SQUARE, (!high) == me ? 600 : 350, 1, 0); }
    if (me == high) led_key_want |= 0x7FFFFFFu;  /* yours is how high: the keys, in their four colours */
}
static void hm_ease(int32_t *v, int32_t to, int32_t dms, int32_t tc)
{
    int32_t d = (to - *v) * imin(dms, tc) / tc;
    if (!d && to != *v) d = to > *v ? 1 : -1;
    *v += d;
}
static void hm_world(int32_t now)
{
    uint32_t tm = fm1_ms;
    int me = du.seat & 1, high, lane, sh_i, k, face = 0, mood = 1, dizzy = 0;
    int32_t dms = (int32_t)(tm - hm.draw_ms), q = hm.res ? (int32_t)(tm - hm.res_ms) : 100000, wig = 0, y, sx, sy, last = HM_AT[HM_WALLS - 1] * 24;
    int32_t sw = dw_ramp(now, HM_SWAP0, HM_SWAP1), x0 = 34 + 172 * sw / 256, x1 = 206 - 172 * sw / 256;   /* seat 0's rock, seat 1's */
    if (du.state == DUS_COUNT) hm_keys();        /* (counted in: a key held already lifts it) */
    high = hm_high(now); lane = hm_lane(high); sh_i = du.dial[!high] % 3u;
    hm.draw_ms = tm;
    if (!hm.init || dms < 0 || dms > 400) { hm.init = 1; dms = 400; }   /* (the first picture: there at once) */
    hm_ease(&hm.y, Q8(lane >= 0 ? HM_LY[lane] : 192), dms, 110);
    hm_ease(&hm.shape, sh_i * 256, dms, 80);
    hm_ease(&hm.flat, lane >= 0 ? 0 : 256, dms, 140);
    y = (hm.y >> 8) + (lane >= 0 ? tsin(tm, 30, 0) >> 13 : 0);
    sx = 256 + 144 * hm.flat / 256; sy = 256 - 166 * hm.flat / 256;
    if (hm.res && hm.res_ok && q >= 150 && q < 1000) face = 1;            /* rung: a glance back at you, beaming */
    if (hm.res && !hm.res_ok && q < 1500) { face = 1; mood = 3; dizzy = 1; wig = q < 500 ? (tsin(tm, 500, 0) >> 12) * (500 - q) / 500 : 0; }
    if (hm.flat > 128 && !dizzy) { face = 1; mood = 2; }                  /* nobody singing it up: flat on the floor, a tear */
    hm_tunnel(now * HM_SPEED, 60 + 190 * iclamp(now, 0, last) / last, tm);
    for (k = du.n - 1; k >= hm.jn; k--) {        /* the walls still to come, the far ones first */
        int32_t z = 256 + (du_n[k].step * 24 - now) * HM_SPEED;
        if (z > 256 && z < 2400) hm_wall(z, du_n[k].lane, du_n[k].hid * 256, z > 2000 ? (2400 - z) * 256 / 400 : 256, 0, tm);
    }
    hm_marks(lane, sh_i, me != high, tm);
    for (k = 0; k < hm.kept; k++) {              /* its tail: a light for every wall rung */
        int32_t ty = y + 26 + k * 7, tx = 120 + (tsin(tm + (uint32_t)k * 260u, 40, 0) * (4 + k) >> 14);
        galpha(70); gdisc(tx, ty, 5, th->caps[k % 5]); galpha(256); gdisc(tx, ty, 2, th->caps[k % 5]); gp(tx, ty, HM_CORE);
    }
    hm_hum(120 + wig, y, 22, hm.shape, dizzy ? wig * 300 : 0, sx, sy, face, mood, 0, tm);
    for (k = hm.res; k < hm.jn; k++)             /* a wall at the Hum, the moment before it is known how it went */
        hm_wall(256, du_n[k].lane, du_n[k].hid * 256, 256, 60, tm);
    if (hm.res && q < 1400) {                    /* the last one: through it, ringing; or broken against it */
        const du_note_t *n = &du_n[hm.res - 1];
        int32_t hy = HM_LY[n->lane & 3];
        if (hm.res_ok) {
            int32_t z = 256 - q * 2 / 5;
            if (z > 112) hm_wall(z, n->lane, n->hid * 256, (z - 112) * 170 / 144, q < 300 ? 220 - q * 220 / 300 : 0, tm);
            if (q < 900) {                       /* a ring in its colour, notes flying off */
                galpha(256 - q * 256 / 900);
                gcirc(120, hy, 30 + q / 10, GH_ZONE[n->lane & 3]); gcirc(120, hy, 31 + q / 10, GH_ZONE[n->lane & 3]);
                for (k = 0; k < 4; k++) gtextc(120 + (k * 2 - 3) * (14 + q / 18), hy - 24 - q / 14 + (k % 2) * 12, "\xE2\x99\xAA", th->caps[(hm.res + k) % 5], 2);
                galpha(256);
            }
        } else {
            if (q < 420) {                       /* it stops against it: cracks run out from the hole, then it falls */
                hm_wall(256, n->lane, n->hid * 256, 256, 200 - q / 2, tm);
                for (k = 0; k < 7; k++) {
                    int32_t a = k * 9362 + 3000, l = 34 + imin(q, 300) / 4 + k % 3 * 12, cx = 120 + (icos(a) * l >> 14), cy = hy + (isin(a) * l >> 14);
                    gline(120 + (icos(a) * 34 >> 14), hy + (isin(a) * 34 >> 14), cx, cy, AC_RGB(200, 230, 255));
                    gline(cx, cy, cx + (icos(a + 9000) * 10 >> 14), cy + (isin(a + 9000) * 10 >> 14), AC_RGB(200, 230, 255));
                }
            }
            hm_shards(n->lane, q - 380);
            if (q < 400) { galpha(256 - q * 256 / 400); hm_spark(120, y, 5 + q / 30, AC_RGB(255, 255, 255)); galpha(256); }
        }
    }
    if (now > last) {                            /* the last wall behind it: home, its light filling the tunnel */
        int32_t f = iclamp((now - last) * 256 / imax(du.end_tick - last, 1), 0, 256);
        galpha(f * 150 / 256); gdisc(HM_VX, HM_VY, 10 + 190 * f / 256, HM_GLOW); galpha(256);
        for (k = 0; k < 6; k++) {                /* and its family, out to meet it */
            int32_t a = (int32_t)(tm * 6u) + k * 10922, r = 20 + 60 * f / 256;
            if (f > 40) hm_hum(HM_VX + (icos(a) * r >> 14), HM_VY + (isin(a) * r * 2 / 3 >> 14), 3 + 7 * f / 256, (k % 3) * 256, 0, 256, 256, f > 150, 1, 0, tm);
        }
    }
    hm_thread(x0, 196, 110, y + 12, DU_COL[0], tm); hm_thread(x1, 196, 130, y + 12, DU_COL[1], tm);
    for (k = 0; k < 2; k++) {                    /* the two rocks: under the lamps whoever is how high, under the shapes the other */
        int in_swap = now >= HM_SWAP0 && now < HM_SWAP1;
        dw_rock(k ? x1 : x0, 212, k, 440, in_swap ? (now - HM_SWAP0) * (k ? -300 : 300) : 0, du.hop[k] > 0 ? (240 - du.hop[k]) * 256 / 240 : 0, tm);
    }
    gbox(70, 2, 100, 5, th->mute);               /* how much of the way is left */
    grect(72, 4, 96 * iclamp(du.end_tick - now, 0, du.end_tick) / imax(du.end_tick, 1), 1, th->pad);
    if (now >= HM_SWAP0 - 96 && now < HM_SWAP1) {   /* the swap, said large */
        galpha(hm_fade(now, HM_SWAP0 - 96, HM_SWAP0 - 48) - hm_fade(now, HM_SWAP1 - 72, HM_SWAP1));
        gfit(120, 122, 200, TR(T_HM_SWAP), th->pad, 4); galpha(256);
    } else if (hm.res < 2 || (hm.res >= HM_HALF && hm.res < HM_HALF + 2)) {   /* each half's first walls: what is yours */
        const char *s = TR(me == high ? T_HM_HIGH : T_HM_SHAPE);
        int32_t a = ts(1, 2);
        while (a > 1 && gtextw(s, a) > 220) a--;
        galpha(190); grect(0, 11, 240, 5 * a + 8, AC_RGB(3, 5, 14)); galpha(256);
        gfit(120, 15, 228, s, DU_COL[me], a);
    }
}

/* ---------------------------------------------------------------- the opening */
#define HM_INTRO_POOL 9600                       /* (the pool until then, the tunnel after) */
static void hm_pool(uint32_t tm)
{
    int32_t k, y, xy[10];
    for (y = 0; y < 240; y += 8) grect(0, y, 240, 8, dw_mix(AC_RGB(3, 5, 14), AC_RGB(10, 22, 40), y * 256 / 240));
    for (k = 0; k < 8; k++) {                    /* stalactites far off */
        int32_t x = 8 + k * 33 + (k * 17) % 11, h = 18 + (k * 29) % 34;
        xy[0] = x - 9; xy[1] = 0; xy[2] = x + 9; xy[3] = 0; xy[4] = x + 1; xy[5] = h; gpoly(xy, 3, AC_RGB(10, 18, 32));
    }
    for (k = 0; k < 22; k++) {                   /* glow-worms */
        galpha(60 + ipulse(tm + (uint32_t)k * 377u, 1900u + (uint32_t)k * 31u) * 190 / 256);
        gp((int32_t)(ihash((uint32_t)k * 13u) % 240u), 3 + (int32_t)(ihash((uint32_t)k * 7u) % 30u), k % 3 ? th->caps[3] : th->caps[2]);
    }
    galpha(256);
    xy[0] = 146; xy[1] = 0; xy[2] = 186; xy[3] = 0; xy[4] = 174; xy[5] = 34; xy[6] = 166; xy[7] = 60; xy[8] = 158; xy[9] = 30;
    gpoly(xy, 5, AC_RGB(22, 36, 60));            /* the one that drips */
    gline(166, 60, 174, 34, AC_RGB(60, 90, 130)); gline(174, 34, 186, 0, AC_RGB(60, 90, 130));
    grect(0, 180, 240, 60, AC_RGB(6, 22, 44));   /* the pool */
    ghl(0, 240, 180, AC_RGB(40, 90, 130));
    for (k = 0; k < 14; k++) {
        int32_t x = (int32_t)((ihash((uint32_t)k * 91u) + tm / 60u) % 260u) - 10, yy = 186 + (k * 37) % 50;
        galpha(50 + ipulse(tm + (uint32_t)k * 500u, 2400) * 60 / 256); ghl(x, x + 8 + k % 5, yy, AC_RGB(60, 120, 160));
    }
    galpha(256);
    xy[0] = 0; xy[1] = 172; xy[2] = 104; xy[3] = 172; xy[4] = 122; xy[5] = 184; xy[6] = 128; xy[7] = 240; xy[8] = 0; xy[9] = 240;
    gpoly(xy, 5, AC_RGB(16, 26, 36));            /* the shore */
    ghl(0, 105, 172, th->moss); ghl(0, 104, 173, AC_RGB(14, 80, 86));
    for (k = 4; k < 100; k += 7) { int32_t h = 2 + (int32_t)(ihash((uint32_t)k) % 4u); gline(k, 172, k - 1, 172 - h, th->moss); }
    mush_draw(14, 172, 150, th->caps[0], th->stem, 120 + ipulse(tm, 2600) * 100 / 256);
    mush_draw(100, 173, 90, th->caps[2], th->stem, 120 + ipulse(tm + 900u, 2600) * 100 / 256);
}
static void hm_ripple(int32_t x, int32_t q, int32_t life, int32_t reach)
{
    if (q < 0 || q > life) return;
    galpha(200 - q * 200 / life); garc(x, 181, 2 + q * reach / life, 1 + q * reach / life / 4, 0, 65536, AC_RGB(150, 210, 255), 0); galpha(256);
}
__attribute__((noinline)) static void hm_title(int32_t t, int32_t t0)
{
    int32_t a = hm_fade(t, t0, t0 + 300), y = 70 + 30 * (256 - dw_ramp(t, t0, t0 + 500)) / 256;
    if (t < t0) return;
    galpha(a * 3 / 4); grect(0, y - 14, AC_W, 64, th->bg); galpha(a);
    ghl(20, 220, y - 12, HM_BODY); ghl(20, 220, y + 48, HM_BODY);
    gfit(120, y, 228, DU_NAME[DM_HUM], HM_BODY, 4);
    gtextc(120, y + 36, "\xE2\x99\xAA", th->mute, 1);
    galpha(256);
}
static void hm_intro(int32_t t, uint32_t tm)
{
    if (t < HM_INTRO_POOL) {                     /* the pool */
        int32_t hx = 166, hy = 140, r = 13, shape = 0, sx = 256, sy = 256, rot = 0, mood = 0, look = 0, born = t >= 2100;
        int32_t pa = 0, ta = 0, ph = 0, th2 = 0, jit;
        hm_pool(tm);
        if (t >= 900 && t < 1500) gdisc(166, 61, 1 + (t - 900) / 300, AC_RGB(150, 210, 255));          /* the drop */
        if (t >= 1500 && t < 1900) { int32_t f = (t - 1500) * 256 / 400; gdisc(166, 62 + 118 * f / 256 * f / 256, 2, AC_RGB(150, 210, 255)); }
        hm_ripple(166, t - 1900, 1100, 44); hm_ripple(166, t - 2100, 1100, 36); hm_ripple(166, t - 2350, 1000, 26);
        if (t >= 3900 && t < 4200) ph = dw_hop(t, 3900, 300);
        if (t >= 3980 && t < 4280) th2 = dw_hop(t, 3980, 300);
        if (t >= 2100) { pa = 1500; ta = 1500; }  /* (they look over at it) */
        if (born) {
            jit = t < 3600 ? 0 : t < 7400 ? 256 : 256 - dw_ramp(t, 7400, 8000);
            shape = (256 + (tsin(tm, 170, 0) >> 6) + (tsin(tm, 71, 9000) >> 7)) * jit / 256;
            if (t < 3000) {                      /* a ripple lifts off the water */
                int32_t f = dw_ramp(t, 2100, 2900);
                hy = 180 - 40 * f / 256; r = 4 + 9 * f / 256; sy = 80 + 176 * f / 256;
            } else if (t < 3600) { mood = 4; look = t < 3300 ? -2 : 2; }
            else if (t < 4000) { hy = 140 - 62 * hm_fade(t, 3600, 4000) / 256; mood = 4; }      /* up it shoots */
            else if (t < 4600) {                 /* its head on the stalactite; down, flat */
                int32_t f = hm_fade(t, 4000, 4600);
                hx = 166 + 14 * f / 256; hy = 78 + 96 * f / 256 * f / 256; mood = 3; rot = (t - 4000) * 60;
            } else if (t < 5900) { hx = 180; hy = 176; sx = 400; sy = 90; mood = 2; shape = shape / 3; }
            else {                               /* Pink's note: up, and steady */
                int32_t f = dw_ramp(t, 5900, 6700);
                hx = 180 - 14 * f / 256; hy = 176 - 58 * f / 256; sx = 400 - 144 * f / 256; sy = 90 + 166 * f / 256;
                mood = t < 7400 ? 4 : 1;
                if (t >= 8100 && t < 8900) rot = (t - 8100) * 65536 / 800;
                if (t >= 8900) { mood = 4; look = 3; }
            }
        }
        if (t >= 5400 && t < 5700) ph = dw_hop(t, 5400, 300);
        if (t >= 7000 && t < 7300) th2 = dw_hop(t, 7000, 300);
        if (t >= 8900) { pa = 2600; ta = 2600; }
        dw_rock(40, 161, 0, 400, pa, ph, tm);
        dw_rock(78, 161, 1, 400, ta, th2, tm);
        if (t >= 5900) hm_thread(46, 148, hx - 8, hy + 6, DU_COL[0], tm);
        if (t >= 7500) hm_thread(84, 148, hx - 4, hy + 10, DU_COL[1], tm);
        if (t >= 5400 && t < 6100) { int32_t f = dw_ramp(t, 5400, 6100); gtextc(hm_lerp(46, 176, f), hm_lerp(142, 168, f) - (isin(f * 128) * 30 >> 14), "\xE2\x99\xAA", DU_COL[0], 2); }
        if (t >= 7000 && t < 7700) { int32_t f = dw_ramp(t, 7000, 7700); gtextc(hm_lerp(84, 166, f), hm_lerp(142, 118, f) - (isin(f * 128) * 24 >> 14), "\xE2\x99\xAA", DU_COL[1], 2); }
        if (born) hm_hum(hx, hy, r, shape, rot, sx, sy, t >= 2900, mood, look, tm);
        if (t >= 4000 && t < 4400) { galpha(256 - (t - 4000) * 256 / 400); hm_spark(168, 64, 3 + (t - 4000) / 50, AC_RGB(255, 255, 255)); galpha(256); }
        if (t >= 8100 && t < 8900) {             /* the twirl: sparkles round it */
            int32_t k;
            for (k = 0; k < 5; k++) { int32_t a = (t - 8100) * 90 + k * 13107; hm_spark(hx + (icos(a) * 24 >> 14), hy + (isin(a) * 24 >> 14), 2, HM_CORE); }
        }
        if (t >= 8700) {                         /* the chord from down the tunnel: a warm light from the right */
            int32_t k, a = hm_fade(t, 8700, 9300);
            for (k = 0; k < 6; k++) { galpha(a * (12 + k * 7) / 256 * (180 + ipulse(tm, 500) * 76 / 256) / 256); grect(200 + k * 7, 0, 7, 180, HM_GLOW); }
            galpha(256);
        }
        if (t < 2100) gzoom(110, 150, 256 + 60 * (256 - dw_ramp(t, 0, 1800)) / 256);
        else if (t < 5400) gzoom(122, 140, 256 + 52 * dw_ramp(t, 2100, 3000) / 256);
        else gzoom(122, 140, 308 - 52 * dw_ramp(t, 5400, 6200) / 256);
        hm_dark(256 - hm_fade(t, 0, 800)); hm_dark(hm_fade(t, 9200, 9600));
        dw_skip(tm);
        return;
    }
    {                                            /* down the tunnel: the others, far off behind the walls */
        int32_t q = t - HM_INTRO_POOL, hx = 120, hy = 186, r = 22, face = 0, mood = 0, f, wy = HM_VY + (HM_LY[2] - HM_VY) * hm_s(440) / 256, wr = 22 * hm_s(440) / 256;
        int32_t px = -24 + 62 * dw_ramp(q, 1700, 2600) / 256, tx = 264 - 62 * dw_ramp(q, 1800, 2700) / 256;
        hm_tunnel(0, 200 + ipulse(tm, 1000) * 56 / 256, tm);
        hm_wall(1180, 3, 256, 256, 0, tm); hm_wall(780, 1, 0, 256, 0, tm);
        if (q < 1100) { f = dw_ramp(q, 300, 1100); hy = 186 - (186 - wy) * f / 256; r = 22 - (22 - wr) * f / 256; }   /* in it darts */
        else if (q < 1700) { f = dw_ramp(q, 1100, 1700); hy = wy + (150 - wy) * f / 256; r = wr + (22 - wr) * f / 256; face = 1; mood = 3; }   /* bonk; back it comes */
        else { hy = 150 + (tsin(tm, 30, 0) >> 12); face = 1; mood = q < 2900 ? 2 : 1; }
        hm_wall(440, 2, 512, 256, q >= 1100 && q < 1500 ? 200 - (q - 1100) / 2 : 0, tm);
        hm_hum(hx, hy, r, 0, q >= 1100 && q < 1700 ? (q - 1100) * 50 : 0, 256, 256, face, mood, 0, tm);
        if (q >= 1100 && q < 1500) { galpha(256 - (q - 1100) * 256 / 400); hm_spark(120, wy, 4 + (q - 1100) / 40, AC_RGB(255, 255, 255)); galpha(256); }
        dw_rock(px, 212, 0, 440, q < 2600 ? q * 40 : 0, hm_hop(q, 2800, 300), tm);
        dw_rock(tx, 212, 1, 440, q < 2700 ? -q * 40 : 0, hm_hop(q, 2900, 300), tm);
        hm_dark(256 - hm_fade(q, 0, 500));
        hm_title(q, 3300);
        if (q < 3300) dw_skip(tm);
    }
}
static void hm_sounds(int32_t t0, int32_t t1)    /* the opening's sounds, as their moments pass */
{
    int k;
    if (dw_at(t0, t1, 1900)) { sfx(96, 0, 60, W_SINE, 1500, 1, 160); sfx(103, 50, 110, W_SINE, 900, 1, 160); }   /* plink */
    if (dw_at(t0, t1, 2200)) { sfx(72, 0, 700, W_SINE, 700, 300, 140); sfx(79, 250, 500, W_SINE, 500, 200, 140); }   /* a ripple that hums */
    for (k = 0; k < 4; k++) if (dw_at(t0, t1, 3600 + k * 80)) sfx(72 + k * 7, 0, 90, W_SINE, 900, 2, 60);   /* up it shoots */
    if (dw_at(t0, t1, 4000)) { sfx(45, 0, 150, W_SINE, 2600, 1, 20); sfx(88, 0, 40, W_NOISE, 700, 1, 20); }       /* its head */
    if (dw_at(t0, t1, 4600)) sfx(52, 0, 110, W_NOISE, 900, 2, 0);                                                /* flat */
    if (dw_at(t0, t1, 4900)) { sfx(67, 0, 240, W_SINE, 600, 20, 80); sfx(63, 260, 420, W_SINE, 600, 20, 80); }
    if (dw_at(t0, t1, 5400)) sfx(72, 0, 900, W_TRI, 2000, 20, 120);                                             /* Pink's note */
    if (dw_at(t0, t1, 5900)) sfx(84, 0, 700, W_SINE, 800, 120, 140);                                            /* and it rises to it */
    if (dw_at(t0, t1, 7000)) sfx(76, 0, 900, W_TRI, 2000, 20, 120);                                             /* Teal's */
    if (dw_at(t0, t1, 7500)) sfx(88, 0, 600, W_SINE, 800, 80, 140);                                             /* its edge settles */
    for (k = 0; k < 4; k++) if (dw_at(t0, t1, 8100 + k * 120)) sfx(84 + (k == 3 ? 12 : k == 2 ? 7 : k * 4), 0, 160, W_SINE, 800, 2, 140);   /* the twirl */
    if (dw_at(t0, t1, 8700)) { sfx(48, 0, 1800, W_TRI, 1700, 300, 180); sfx(55, 0, 1800, W_TRI, 1300, 300, 180); sfx(64, 0, 1800, W_SINE, 1100, 300, 180); }   /* its family, far off */
    for (k = 0; k < 3; k++) if (dw_at(t0, t1, HM_INTRO_POOL + 300 + k * 110)) sfx(79 + k * 5, 0, 80, W_SINE, 800, 2, 60);   /* in it darts */
    if (dw_at(t0, t1, HM_INTRO_POOL + 1100)) { sfx(43, 0, 170, W_SINE, 2600, 1, 20); sfx(88, 0, 40, W_NOISE, 700, 1, 20); }   /* bonk */
    if (dw_at(t0, t1, HM_INTRO_POOL + 2850)) { sfx(84, 0, 70, W_SQUARE, 500, 1, 0); sfx(88, 100, 70, W_SQUARE, 500, 1, 0); }   /* the rocks: in after it */
    if (dw_at(t0, t1, HM_INTRO_POOL + 3300)) snd_ui(79);
}
