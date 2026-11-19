/* SPDX-License-Identifier: GPL-3.0-only */
/* HOME on the course: the controls, shown rather than told. A loop of five short steps, each a little scene with
 * Rock above a line drawing of the FM-1 and the two hands that hold it:
 *   roll        the left hand turns PRESETS: Rock rolls
 *   hop         the right thumb, holding the bottom right corner, on a bottom key: a hop
 *   big hop     the same thumb on a top key: a big hop
 *   the jump    rolling over the white keys in a hill, a top key: Rock is thrown from the crest, lands on a glowing
 *               bounce mushroom and is thrown again: BONUS
 *   or          the other grip, last and once: the right hand on KNOB 4, the left thumb on the keys
 * The device and the hands are lit lines on the dark, as the Hat and Rock are. The knob hand pinches its knob and
 * twists it, lets go and twists again; the thumb waits just off its key, comes down on it (a ring goes out from under
 * it, three small strokes stand over the key) and lifts. The knob hand comes in alone for the first
 * step, the key hand with the second; both slide out and in again when the grip changes.
 * The words are the controls sheet's own ("BIG HOP = TOP KEYS": the word large over the scene, the rest
 * under the scene, over the device), so every language has them. The keys of the row being shown light up on the
 * FM-1 itself. The first step has a line of its own under its word: TURN KNOB TO ROLL. */

typedef struct { uint8_t grip, act; uint16_t ms; uint16_t tap[2]; uint8_t row[2]; } tu_step_t;
enum { TA_ROLL, TA_HOP, TA_JUMP, TA_SWAP };      /* tap: when the thumb presses (0: it does not); row: 1 bottom, 2 top */
#define TU_T_KEY 1500                            /* the jump: when the key goes down, Rock leaves the crest, lands, is gone */
#define TU_T_OFF 1800
#define TU_T_LAND 2600
#define TU_T_OUT 3300
static const tu_step_t TU_STEP[5] = {
    {0, TA_ROLL, 2400, {0, 0}, {0, 0}},
    {0, TA_HOP, 1800, {550, 1300}, {1, 1}},   /* (later: the hand at the keys comes in first) */
    {0, TA_HOP, 1800, {300, 1050}, {2, 2}},
    {0, TA_JUMP, 4600, {TU_T_KEY, 0}, {2, 0}},
    {1, TA_SWAP, 3800, {1000, 2300}, {1, 2}},
};
#define TU_N 5
#define TU_LOOP 14400
#define TU_REACH 550                             /* how long before a press on the other row the thumb moves over */
#define TU_SLIDE 80                              /* the hands come in from the sides, and go out, when the grip changes */
#define TU_GY 68                                 /* the scene: its ground, and where it ends */
#define TU_SCENE_H 79
#define TU_JX 198                                /* the jump: the bounce mushroom, the crest */
#define TU_JY 60
#define TU_CREST 120
/* the FM-1, 150 x 90 at (45, 134), under the words, from a photograph of one */
#define TU_DX 45
#define TU_DY 134
static const uint8_t TU_SHARP_AFTER[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};   /* the white key each top key follows */
static int32_t tu_last;                          /* where in the loop the last tick was (for the sounds) */
/* every tutorial (this one and each machine's, ac_mtutor.h) plays each step's scene TU_HOLD times before it moves on,
 * so there is time to read; any knob steps on or back (sheet_tick) */
#define TU_HOLD 2

/* where t falls: the step, how far into its scene (u, the scene playing again on each pass), how many loops so far;
 * ur: how far into the step itself */
static void tu_where_r(int32_t t, int32_t *step, int32_t *u, int32_t *loops, int32_t *ur)
{
    int32_t k, lt = t % (TU_LOOP * TU_HOLD);
    *loops = t / (TU_LOOP * TU_HOLD);
    for (k = 0; k < TU_N - 1 && lt >= TU_STEP[k].ms * TU_HOLD; k++) lt -= TU_STEP[k].ms * TU_HOLD;
    *step = k; *ur = lt; *u = lt % TU_STEP[k].ms;
}
static void tu_where(int32_t t, int32_t *step, int32_t *u, int32_t *loops) { int32_t ur; tu_where_r(t, step, u, loops, &ur); }
/* the marks under a tutorial's scene, one a step: the step playing fills as it goes (how long until the next), and
 * arrows either side say any knob steps on or back (bright a moment after a knob has turned) */
static void tu_pips(int32_t n, int32_t step, int32_t prog256, int32_t y, int32_t knob_ago, uint32_t tm)
{
    int32_t k, x0 = 120 - n * 8, hot = knob_ago >= 0 && knob_ago < 500;
    for (k = 0; k < n; k++) {
        int32_t x = x0 + k * 16;
        grect(x, y - 1, 14, 3, k < step ? th->pad : th->mute);   /* (done: lit; to come: dim; this one fills, white) */
        if (k == step) grect(x, y - 1, 14 * iclamp(prog256, 0, 256) / 256, 3, th->text);
    }
    galpha(hot ? 256 : 110 + ipulse(tm, 1800) * 70 / 256);
    gtextr(x0 - 6, y - 2, "\xE2\x86\x90", hot ? th->text : th->pad, 1);
    gtext(x0 + n * 16 + 4, y - 2, "\xE2\x86\x92", hot ? th->text : th->pad, 1);
    galpha(256);
}
/* the press nearest u in a step: which row, and how long ago it went down (negative: not yet; -1000: none) */
static int32_t tu_tap(int32_t step, int32_t u, int32_t *row)
{
    const tu_step_t *s = &TU_STEP[step];
    int k = s->tap[1] && u >= (int32_t)s->tap[1] - TU_REACH ? 1 : 0;
    *row = s->row[k];
    return s->tap[k] ? u - (int32_t)s->tap[k] : -1000;
}
static void tutor_tick(int32_t dt)
{
    int32_t step, u, loops, lt = sh.t % (TU_LOOP * TU_HOLD), a = tu_last, k, j, row, age, base = 0, r;
    uint32_t rowmask = 0;
    (void)dt;
    tu_where(sh.t, &step, &u, &loops);
    if (lt < a) a = -1;                          /* (the loop came round) */
#define TU_AT(ms) (a < (ms) && lt >= (ms))
    for (k = 0; k < TU_N; k++) {
        const tu_step_t *s = &TU_STEP[k];
        for (r = 0; r < TU_HOLD; r++, base += s->ms) {   /* (each pass of the step's scene) */
            for (j = 0; j < 2; j++)              /* a press: the key's own note, as on the course */
                if (s->tap[j] && TU_AT(base + s->tap[j])) sfx(key_note(s->row[j] == 2 ? 22 : 23), 0, 600, W_TRI, 2800, 6, 200);
            if (s->act == TA_JUMP) {
                if (TU_AT(base + TU_T_OFF)) snd_whoosh();
                if (TU_AT(base + TU_T_LAND)) { sfx(67, 0, 800, W_SINE, 3400, 4, 220); sfx(79, 60, 600, W_SINE, 1800, 4, 220); snd_chime(1); }
            }
        }
    }
#undef TU_AT
    tu_last = lt;
    /* on the FM-1 itself: the row being shown glows, and lights as the key goes down */
    age = tu_tap(step, u, &row);
    if (row)
        for (k = 0; k < AC_NKEY; k++) if (KEY_IS_SHARP[k] == (row == 2)) rowmask |= 1u << k;
    led_key_dim_want |= rowmask;
    if (age >= 0 && age < 220) led_key_want |= rowmask;
}

/* the hands, as icons: a closed hand and the one or two digits doing the work, each a few round-ended pieces drawn as
 * one shape (one pixel larger in the line's colour, then in the dark: one outline round all of it, hiding what the
 * hand is over), and the knuckles of the closed fingers as short lines. One hand pinches the knob it turns; the other,
 * at the side of the keys, is a thumb pointing in from a closed hand. */
typedef struct { int32_t x0, y0, r0, x1, y1, r1; } tu_seg_t;
static void tu_seg(const tu_seg_t *q, int32_t g, col_t c)   /* a tapering bar, round at both ends, g larger */
{
    int32_t dx = q->x1 - q->x0, dy = q->y1 - q->y0, L = imax(1, (int32_t)isqrt((uint32_t)(dx * dx + dy * dy)));
    int32_t r0 = q->r0 + g, r1 = q->r1 + g, p[8];
    int32_t nx0 = (-dy * r0 * 256 / L + 128) >> 8, ny0 = (dx * r0 * 256 / L + 128) >> 8;
    int32_t nx1 = (-dy * r1 * 256 / L + 128) >> 8, ny1 = (dx * r1 * 256 / L + 128) >> 8;
    gdisc(q->x0, q->y0, r0, c); gdisc(q->x1, q->y1, r1, c);
    p[0] = q->x0 + nx0; p[1] = q->y0 + ny0; p[2] = q->x1 + nx1; p[3] = q->y1 + ny1;
    p[4] = q->x1 - nx1; p[5] = q->y1 - ny1; p[6] = q->x0 - nx0; p[7] = q->y0 - ny0;
    gpoly(p, 4, c);
}
/* an icon: pieces {x0, y0, r0, x1, y1, r1} and lines {x0, y0, x1, y1} around (0, 0), mirrored when s is -1, turned by
 * a (65536 a turn), drawn at (ox, oy), its outline al bright (256: fully) */
static void tu_icon(const int8_t (*pc)[6], int32_t n, const int8_t (*ln)[4], int32_t m, int32_t ox, int32_t oy, int32_t s, int32_t a, int32_t al)
{
    int32_t co = icos(a), si = isin(a), g, k, j;
    tu_seg_t q[6];
#define TU_TX(x, y) (ox + ((s * (x) * co - (y) * si + 8192) >> 14))
#define TU_TY(x, y) (oy + ((s * (x) * si + (y) * co + 8192) >> 14))
    for (k = 0; k < n; k++) {
        q[k].x0 = TU_TX(pc[k][0], pc[k][1]); q[k].y0 = TU_TY(pc[k][0], pc[k][1]); q[k].r0 = pc[k][2];
        q[k].x1 = TU_TX(pc[k][3], pc[k][4]); q[k].y1 = TU_TY(pc[k][3], pc[k][4]); q[k].r1 = pc[k][5];
    }
    for (g = 1; g >= 0; g--) { galpha(g ? al : 256); for (k = 0; k < n; k++) tu_seg(&q[k], g, g ? th->text : th->bg); }
    galpha(150);
    for (j = 0; j < m; j++) gline(TU_TX(ln[j][0], ln[j][1]), TU_TY(ln[j][0], ln[j][1]), TU_TX(ln[j][2], ln[j][3]), TU_TY(ln[j][2], ln[j][3]), th->text);
    galpha(256);
#undef TU_TX
#undef TU_TY
}
/* the pinch, round the knob's middle, as an OK sign tilted a little: the hand to the left of the knob (s = 1), the
 * first finger curling over the top of it and the thumb round under it, their tips meeting on its far side; the other
 * three fingers stand up behind the first, dimmer */
static const int8_t TU_PINCH[5][6] = {{-20, 1, 6, -19, 5, 6}, {-15, -4, 3, -8, -9, 3}, {-8, -9, 3, 0, -7, 3},
                                     {-15, 8, 3, -8, 10, 3}, {-8, 10, 3, 1, 6, 3}};   /* palm; finger; thumb */
static const int8_t TU_PINCH_B[3][6] = {{-22, -3, 2, -23, -10, 2}, {-18, -4, 2, -17, -13, 2}, {-14, -5, 2, -11, -13, 2}};
#define TU_TILT 2500
/* the thumb, round its tip, the hand to the right of it (s = 1): pointing left from a closed hand */
static const int8_t TU_TAP[2][6] = {{0, 0, 3, 13, -1, 4}, {19, 4, 7, 21, 7, 7}};
static const int8_t TU_TAP_L[3][4] = {{13, 5, 17, 5}, {13, 8, 17, 8}, {14, 11, 17, 11}};
static void tu_rbox(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, col_t c)   /* an outline, its corners cut */
{
    ghl(x + r, x + w - r, y, c); ghl(x + r, x + w - r, y + h - 1, c);
    grect(x, y + r, 1, h - 2 * r, c); grect(x + w - 1, y + r, 1, h - 2 * r, c);
    gline(x, y + r, x + r, y, c); gline(x + w - 1 - r, y, x + w - 1, y + r, c);
    gline(x, y + h - 1 - r, x + r, y + h - 1, c); gline(x + w - 1 - r, y + h - 1, x + w - 1, y + h - 1 - r, c);
}
/* a key: an outline with its slit; lit when its row is the one being shown, filled with the step's colour while it
 * is down (age: ms since it went down) */
static void tu_key(int32_t x, int32_t y, int32_t lit, int32_t age, col_t c)
{
    galpha(lit ? 256 : 120);
    tu_rbox(x, y, 6, 13, 1, lit ? th->pad : th->rim);
    grect(x + 2, y + 3, 1, 6, lit ? th->pad : th->rim);
    if (age >= 0 && age < 420) { galpha(imax(0, 256 - imax(0, age - 180) * 256 / 240)); grect(x + 1, y + 1, 4, 11, c); }
    galpha(256);
}
/* how far the thumb is down on its key, 0..256, from how long ago the key went down: it comes in, holds, lifts */
static int32_t tu_press(int32_t age)
{
    if (age < -220 || age >= 420) return 0;
    if (age < 0) { int32_t e = (age + 220) * 256 / 220; return e * e / 256; }
    if (age < 150) return 256;
    return 256 - (age - 150) * 256 / 270;
}
/* the device and the hands. grip 0: the left hand on PRESETS, the right thumb on the keys; 1: the right hand on KNOB 4,
 * the left thumb. turn: the knob is being turned; row: the row the thumb is over (rf: how far it has come there from
 * the other, 0..256); age: how long ago it pressed; slide, tslide: how far the knob hand and the key hand are out to the sides; c: the step's colour */
static void tu_device(int grip, int turn, int32_t row, int32_t rf, int32_t age, int32_t slide, int32_t tslide, col_t c, uint32_t tm)
{
    static const uint8_t KN[8][2] = {{12, 13}, {27, 13}, {12, 29}, {27, 29}, {85, 14}, {102, 14}, {120, 14}, {137, 14}};
    int32_t x0 = TU_DX, y0 = TU_DY, k, kn = grip ? 7 : 2, kx, ky, hw = grip ? 1 : 13, hs = grip ? 1 : 12, s = grip ? -1 : 1;
    int32_t da = 0, ka = -9000, kp[2][2], r, px, py, f, tx, ty;
    tu_rbox(x0, y0, 150, 90, 4, th->rim);
    /* the screen (the game on it is drawn by tu_mini) */
    tu_rbox(x0 + 39, y0 + 7, 35, 35, 2, th->rim);
    galpha(120);
    /* OCT- and OCT+, the two rows of buttons, the keys' bed */
    gbox(x0 + 12, y0 + 41, 9, 5, th->rim); gbox(x0 + 22, y0 + 41, 9, 5, th->rim);
    for (k = 0; k < 12; k++) gbox(x0 + 81 + (k % 6) * 10, y0 + 28 + (k / 6) * 10, 8, 8, th->rim);
    tu_rbox(x0 + 6, y0 + 51, 138, 35, 3, th->rim);
    for (k = 0; k < 8; k++) {                    /* the knobs: the one in the fingers is drawn after the hand */
        int32_t a = -9000 + k * 5000;
        if (k == kn) continue;
        gcirc(x0 + KN[k][0], y0 + KN[k][1], 4, th->rim);
        gline(x0 + KN[k][0], y0 + KN[k][1], x0 + KN[k][0] + ((3 * icos(a)) >> 14), y0 + KN[k][1] + ((3 * isin(a)) >> 14), th->rim);
    }
    galpha(256);
    /* the keys: sixteen below, eleven above in threes and twos; the row the thumb is over is lit */
    for (k = 0; k < 16; k++) tu_key(x0 + 11 + k * 8, y0 + 69, row == 1, row == 1 && k == hw ? age : -1, c);
    for (k = 0; k < 11; k++) tu_key(x0 + 15 + TU_SHARP_AFTER[k] * 8, y0 + 54, row == 2, row == 2 && TU_SHARP_AFTER[k] == hs ? age : -1, c);
    /* the hand on the knob: it twists the knob a little way, lets go and takes a new hold, again and again */
    kx = x0 + KN[kn][0]; ky = y0 + KN[kn][1];
    if (turn) {
        int32_t ph = (int32_t)(tm % 700u), n = (int32_t)(tm / 700u), e;
        if (ph < 480) { e = ph * 256 / 480; e = e * (512 - e) / 256; da = 5000 * e / 256; ka += n * 5000 + da; }
        else { da = 5000 * (700 - ph) / 220; ka += n * 5000 + 5000; }   /* let go, turn back, take hold again */
    }
    {   /* each part with its own outline, back to front: the three fingers behind, the palm, the thumb, the finger */
        int32_t a = da - s * TU_TILT, hx = kx - s * slide;
        for (k = 0; k < 3; k++) tu_icon(&TU_PINCH_B[k], 1, 0, 0, hx, ky, s, a, 130);
        tu_icon(&TU_PINCH[0], 1, 0, 0, hx, ky, s, a, 256);
        tu_icon(&TU_PINCH[3], 2, 0, 0, hx, ky, s, a, 256);
        tu_icon(&TU_PINCH[1], 2, 0, 0, hx, ky, s, a, 256);
    }
    gdisc(kx, ky, 4, th->bg); gcirc(kx, ky, 4, turn ? c : th->rim);
    gline(kx, ky, kx + ((3 * icos(ka)) >> 14), ky + ((3 * isin(ka)) >> 14), turn ? c : th->rim);
    /* the hand at the corner: its thumb waits just off the key, comes down on it and lifts */
    kp[0][0] = x0 + 11 + hw * 8 + 3; kp[0][1] = y0 + 69 + 6;
    kp[1][0] = x0 + 15 + hs * 8 + 3; kp[1][1] = y0 + 54 + 6;
    r = row == 2 ? 1 : 0;
    px = kp[1 - r][0] + (kp[r][0] - kp[1 - r][0]) * rf / 256; py = kp[1 - r][1] + (kp[r][1] - kp[1 - r][1]) * rf / 256;
    f = tu_press(age);
    tx = px + s * (1 + 7 * (256 - f) / 256); ty = py + 4 * (256 - f) / 256;
    tu_icon(TU_TAP, 2, TU_TAP_L, 3, tx + s * tslide, ty, s, 0, 256);
    if (age >= 0 && age < 420) {                 /* the press: a ring out from under the thumb, three strokes over the key */
        int32_t fa = imax(0, 256 - imax(0, age - 180) * 256 / 240), rr = 8 + age * 12 / 420;
        galpha(imax(0, 256 - age * 256 / 420)); gcirc(px, py, rr, c);
        galpha(fa);
        for (k = -1; k <= 1; k++) gline(px + k * 4, py - 9 - age / 60, px + k * 6, py - 12 - age / 60, th->pad);
        galpha(256);
    }
}

/* the ground of the jump's scene under x: flat, a hill with the pad on its way up, a hollow beyond */
static int32_t tu_hill(int32_t x)
{
    if (x < 50) return TU_GY;
    if (x < TU_CREST) return TU_GY - 20 * smooth8(50, TU_CREST, x) / 256;
    if (x < 160) return TU_GY - 20 + 24 * smooth8(TU_CREST, 160, x) / 256;
    return TU_GY + 4;
}
static void tu_ground(int32_t x, int32_t y0, int32_t y1)
{
    int32_t k;
    gline(x, y0, x + 4, y1, th->rim);
    for (k = 0; k < 2; k++)
        if (((x / 4 + k * 3) % (5 + k * 2)) < 3 + k) { galpha(150 - k * 50); gline(x, y0 + 3 + k * 4, x + 4, y1 + 3 + k * 4, th->bands[k]); galpha(256); }
}
static void tu_rock(int32_t x, int32_t y, int32_t ang, uint32_t tm)
{
    rock_draw(x, y, 256, ang, (tm % 3100u) < 150u ? 30 : 256, th->rock, th->rock_rim, th->dark, th->moss, th->eye, th->pupil);
}
/* a roll, hop or other-grip step: how far Rock has rolled (s, in the scene's pixels: only the steps with a knob turning
 * move it) and how high it is hopping (a hop with each press: small from the bottom row, big from the top) */
static void tu_roll_state(int32_t step, int32_t u, int32_t loops, int32_t *s, int32_t *hop)
{
    const tu_step_t *st = &TU_STEP[step];
    int32_t k, roll = loops * 6200 + (step > 0 ? 2400 : 0) + (st->act == TA_ROLL || st->act == TA_SWAP ? u : 0);
    *s = roll * 46 / 1000; *hop = 0;
    for (k = 0; k < 2; k++) {
        int32_t d = u - (int32_t)st->tap[k], len = st->row[k] == 2 ? 640 : 480, h = st->row[k] == 2 ? 26 : 12;
        if (st->tap[k] && d >= 0 && d < len) *hop = 4 * h * d / len * (len - d) / len;
    }
}
/* the jump: where Rock is (rx, ry, its middle) and how far it has turned */
static void tu_jump_pos(int32_t u, int32_t *rx, int32_t *ry)
{
    int32_t sq = u >= TU_T_LAND && u < TU_T_LAND + 250 ? 256 - (u - TU_T_LAND) * 256 / 250 : 0;
    if (u < TU_T_OFF) {                          /* rolling up, faster and faster */
        int32_t f = u * 256 / TU_T_OFF;
        *rx = 10 + 110 * (f + f * f / 256) / 512; *ry = tu_hill(*rx) - 9;
    } else if (u < TU_T_LAND) {                  /* thrown from the crest */
        int32_t f = (u - TU_T_OFF) * 256 / (TU_T_LAND - TU_T_OFF);
        *rx = TU_CREST + (TU_JX - TU_CREST) * f / 256; *ry = 39 + (TU_JY - 18 - 39) * f / 256 - 4 * 24 * f / 256 * (256 - f) / 256;
    } else {                                     /* and thrown again by the mushroom */
        int32_t f = imin(256, (u - TU_T_LAND) * 256 / (TU_T_OUT - TU_T_LAND));
        *rx = TU_JX + 56 * f / 256; *ry = TU_JY - 18 + 3 * sq / 256 - 12 * f / 256 - 4 * 22 * f / 256 * (256 - f) / 256;
    }
}
/* the scene of a roll, hop or other-grip step: Rock in the middle, the ground and its mushrooms going by as it rolls */
static void tu_scene_roll(int32_t step, int32_t u, int32_t loops, uint32_t tm)
{
    int32_t k, x, s, hop, n0;
    tu_roll_state(step, u, loops, &s, &hop);
#define TU_G(wx) (TU_GY + (isin((wx) * 700) * 3 >> 14))
    n0 = (s - 40) / 78;
    for (k = n0; k < n0 + 5; k++) {              /* mushrooms along the way, glowing */
        int32_t wx = k * 78 + 30, sx = wx - s;
        if (k < 0 || sx < -30 || sx > 270) continue;
        mush_draw(sx, TU_G(wx) + 1, 100 + (int32_t)(ihash((uint32_t)k + 40u) % 70u), th->caps[k % 5], th->stem, 128 + (tsin(tm, 20, k * 9000) >> 7));
    }
    for (x = 0; x < 240; x += 4) tu_ground(x, TU_G(s + x), TU_G(s + x + 4));
    tu_rock(120, TU_G(s + 120) - 9 - hop, s * 1304, tm);
#undef TU_G
}
/* the scene of the jump */
static void tu_scene_jump(int32_t u, uint32_t tm)
{
    int32_t x, k, rx, ry, ang, sq = u >= TU_T_LAND && u < TU_T_LAND + 250 ? 256 - (u - TU_T_LAND) * 256 / 250 : 0;
    col_t jc = th->caps[0];
    for (x = 0; x < 240; x += 4) tu_ground(x, tu_hill(x), tu_hill(x + 4));
    mush_draw(22, TU_GY + 1, 120, th->caps[2], th->stem, 128 + (tsin(tm, 20, 0) >> 7));
    mush_draw(232, TU_GY + 5, 90, th->caps[4], th->stem, 128 + (tsin(tm, 20, 20000) >> 7));
    for (x = 84; x < 109; x++) {                 /* the white keys in the hill; the one under Rock goes down */
        int32_t key = (x - 84) / 5, sink = 0;
        if ((x - 84) % 5 == 4) continue;
        if (u < TU_T_OFF) { int32_t rxn = 10 + 110 * (u * 256 / TU_T_OFF + (u * 256 / TU_T_OFF) * (u * 256 / TU_T_OFF) / 256) / 512; if (iabs(rxn - (86 + key * 5)) < 5) sink = 2; }
        grect(x, tu_hill(x) - 2 + sink, 1, 7, th->pad);
    }
    {   /* the bounce mushroom: a jelly cap, glowing, squashed as it throws */
        int32_t jrx = 15 * (256 + sq * 3 / 10) / 256, jry = 10 * (256 - sq * 4 / 10) / 256, jy = TU_JY + 3 * sq / 256;
        galpha(51 + sq / 3); gdisc(TU_JX, jy, 22, jc); galpha(256);
        gellf(TU_JX, jy, jrx, jry, jc, 1);
        for (k = -2; k <= 2; k++) {
            int32_t tx = TU_JX + jrx * k / 3, sw = tsin(tm, 30, k * 10430) * 3 >> 14;
            gline(tx, jy, tx + sw, jy + 7 * jry / 10, jc); gline(tx + sw, jy + 7 * jry / 10, tx, jy + (12 + (tsin(tm, 20, k * 10430) >> 13)) * jry / 10, jc);
        }
        grect(TU_JX - 7, jy - 6, 2, 2, th->stem); grect(TU_JX + 3, jy - 8, 2, 2, th->stem);
    }
    tu_jump_pos(u, &rx, &ry); ang = rx * 1304;
    if (u < TU_T_OUT) tu_rock(rx, ry, ang, tm);
    if (u >= TU_T_LAND) {                        /* rings from the mushroom, and the word the course gives for it */
        int32_t a = imin(256, (u - TU_T_LAND) * 256 / 700), sc = ts(2, 3);
        for (k = 0; k < 3; k++) { galpha(imax(0, 256 - a) * (256 - k * 64) / 256); gcirc(TU_JX, TU_JY - 8, 8 + k * 7 + a * 40 / 256, th->caps[(k + 1) % 5]); }
        galpha(u > 4000 ? imax(0, (4300 - u) * 256 / 300) : imin(256, (u - TU_T_LAND) * 256 / 150));
        gfit(70, 6 + 2 * sc, 130, TR(T_BONUS), th->pad, sc);
        galpha(256);
    }
}

/* the FM-1's own screen: the game as it plays on it, very small. A third of the scene's size, following Rock: the
 * ground in hills, the glowing mushrooms, Rock rolling as the knob turns and hopping as the thumb presses; in the jump,
 * the hill with its white keys, the throw from the crest and the bounce mushroom */
#define TU_MW 33
static int32_t tu_mwx, tu_mwy;
static void tu_mp(int32_t x, int32_t y, col_t c)
{
    if (x >= 0 && x < TU_MW && y >= 1 && y < TU_MW) gp(tu_mwx + x, tu_mwy + y, c);
}
static void tu_mrock(int32_t x, int32_t y, int32_t ang)   /* Rock, five pixels across, a fleck of moss going round */
{
    int32_t i, j;
    for (j = -2; j <= 2; j++) for (i = -2; i <= 2; i++) if (i * i + j * j <= 5) tu_mp(x + i, y + j, th->rock_rim);
    tu_mp(x + ((icos(ang) + 8192) >> 14), y + ((isin(ang) + 8192) >> 14), th->moss);
}
static void tu_mmush(int32_t x, int32_t gy, col_t cap, int32_t glow)   /* a mushroom: a cap of three over a stem */
{
    galpha(glow / 3); tu_mp(x - 1, gy - 4, cap); tu_mp(x + 1, gy - 4, cap); tu_mp(x, gy - 5, cap); galpha(256);
    tu_mp(x, gy - 1, th->stem); tu_mp(x - 1, gy - 2, cap); tu_mp(x, gy - 2, cap); tu_mp(x + 1, gy - 2, cap); tu_mp(x, gy - 3, cap);
}
static int32_t tu_mground(int32_t wx)            /* the hills of the little screen, in its pixels from its ground line */
{
    return (isin(wx * 160) * 3 >> 14) + (isin(wx * 67 + 9000) * 2 >> 14);
}
static void tu_mini(int32_t step, int32_t u, int32_t loops, uint32_t tm)
{
    const tu_step_t *st = &TU_STEP[step];
    int32_t c, k, wx, gy, cx, rx, ry, h;
    tu_mwx = TU_DX + 40; tu_mwy = TU_DY + 8;
    for (k = 0; k < 4; k++) tu_mp((int32_t)(ihash((uint32_t)k * 5u + 1u) % 33u), 2 + (int32_t)(ihash((uint32_t)k * 11u + 2u) % 9u), th->mute);
    if (st->act != TA_JUMP) {
        int32_t s, hop;
        tu_roll_state(step, u, loops, &s, &hop);
        cx = s + 120;                            /* Rock stays in the middle; the world goes by */
        for (c = 0; c < TU_MW; c++) {
            wx = cx + (c - 16) * 3; gy = 22 - tu_mground(wx);
            tu_mp(c, gy, th->rim);
            galpha(90); if ((wx / 3 + c) % 3) tu_mp(c, gy + 2, th->bands[0]);
            galpha(50); if ((wx / 3 + c) % 4 < 2) tu_mp(c, gy + 4, th->bands[1]);
            galpha(256);
        }
        for (k = (cx - 60) / 78 - 1; k <= (cx + 60) / 78 + 1; k++) {
            int32_t mx = 16 + (k * 78 + 30 - cx) / 3;
            if (k < 0) continue;
            tu_mmush(mx, 22 - tu_mground(k * 78 + 30), th->caps[k % 5], 128 + (tsin(tm, 20, k * 9000) >> 7));
        }
        tu_mrock(16, 22 - tu_mground(cx) - 3 - hop / 3, s * 1304);
        return;
    }
    tu_jump_pos(u, &rx, &ry);
    cx = iclamp(rx, 60, 180);                    /* the jump: the view follows Rock up the hill and over */
    for (c = 0; c < TU_MW; c++) {
        wx = cx + (c - 16) * 3; gy = 22 + (tu_hill(wx) - TU_GY) / 3;
        tu_mp(c, gy, th->rim);
        galpha(90); if ((wx / 3 + c) % 3) tu_mp(c, gy + 2, th->bands[0]);
        galpha(256);
        if (wx >= 84 && wx < 109) tu_mp(c, gy - 1, th->pad);   /* the white keys in the hill */
    }
    {   /* the bounce mushroom: a jelly cap of five, glowing, with its strands */
        int32_t mx = 16 + (TU_JX - cx) / 3, my = 22 + (TU_JY - TU_GY) / 3, sq = u >= TU_T_LAND && u < TU_T_LAND + 250;
        galpha(70); for (k = -3; k <= 3; k++) tu_mp(mx + k, my - 2, th->caps[0]); galpha(256);
        for (k = -2; k <= 2; k++) tu_mp(mx + k, my - 1 + sq, th->caps[0]);
        for (k = -1; k <= 1; k++) tu_mp(mx + k, my - 2 + sq, th->caps[0]);
        for (k = -2; k <= 2; k += 2) { tu_mp(mx + k, my + sq, th->caps[0]); tu_mp(mx + k + ((tm / 200u) & 1u), my + 1 + sq, th->caps[0]); }
        if (u >= TU_T_LAND) {                    /* a ring going out from it */
            int32_t a = imin(256, (u - TU_T_LAND) * 256 / 600);
            h = 3 + a * 7 / 256;
            galpha(256 - a);
            for (k = 0; k < 24; k++) tu_mp(mx + ((h * icos(k * 2731) + 8192) >> 14), my - 2 + ((h * isin(k * 2731) + 8192) >> 14), th->caps[1]);
            galpha(256);
        }
    }
    if (u < TU_T_OUT) tu_mrock(16 + (rx - cx) / 3, 22 + (ry - TU_GY) / 3, rx * 1304);
}

static void tutor_draw(void)
{
    static const int IDS[3] = {T_HELP_ROLL, T_HELP_LOW, T_HELP_HIGH};
    int32_t step, u, loops, k, n, sc, a, fade = 0, id, row, age, ur;
    uint32_t tm = (uint32_t)sh.t;
    const tu_step_t *st;
    const char *s, *rest;
    char word[40];
    col_t c;
    tu_where_r(sh.t, &step, &u, &loops, &ur);
    st = &TU_STEP[step];
    age = tu_tap(step, u, &row);
    c = st->act == TA_ROLL ? th->caps[0] : st->act == TA_JUMP ? th->pad : st->act == TA_SWAP ? th->caps[3] : th->caps[row == 2 ? 4 : 2];
    grect(0, 0, AC_W, AC_H, th->bg);
    for (k = 0; k < 22; k++) gp((int32_t)(ihash((uint32_t)k * 7u + 3u) % 240u), (int32_t)(ihash((uint32_t)k * 13u + 5u) % 60u), th->mute);
    gclip_y1 = TU_SCENE_H;
    if (st->act == TA_JUMP) tu_scene_jump(u, tm); else tu_scene_roll(step, u, loops, tm);
    gclip_y1 = AC_H;
    /* the jump's scene is another place: it fades in, and out again before the next step */
    if (st->act == TA_JUMP) fade = u < 250 ? 256 - u * 256 / 250 : u > st->ms - 300 ? 256 - (st->ms - u) * 256 / 300 : 0;
    else if (st->act == TA_SWAP && u < 250) fade = 256 - u * 256 / 250;
    if (fade > 0) { galpha(fade); grect(0, 0, AC_W, TU_SCENE_H, th->bg); galpha(256); }
    /* the word, large: what this step's control does. One size for the three of them, in every language */
    sc = ts(2, 3);
    for (id = 0; id < 3; id++) {
        s = TR(IDS[id]);
        for (n = 0; s[n] && !(s[n] == ' ' && s[n + 1] == '=') && n < 38; n++) word[n] = s[n];
        word[n] = 0;
        while (sc > 1 && gtextw(word, sc) > 228) sc--;
    }
    rest = TR(st->act == TA_JUMP ? T_HELP_PADS : T_HELP_SWAP);
    if (st->act == TA_ROLL || st->act == TA_HOP) {
        s = TR(IDS[st->act == TA_ROLL ? 0 : row]);
        for (n = 0; s[n] && !(s[n] == ' ' && s[n + 1] == '=') && n < 38; n++) word[n] = s[n];
        word[n] = 0;
        rest = s[n] ? s + n + 3 : s + n;
        if (st->act == TA_ROLL) rest = TR(T_HELP_TURN);   /* the first step says what to do, not which knobs */
        galpha(imin(256, u * 256 / 200));
        gtextc(120, 4 + 2 * sc, word, c, sc);
        galpha(256);
    }
    /* where in the loop it is: a mark for each step under the scene */
    tu_pips(TU_N, step, ur * 256 / (st->ms * TU_HOLD), TU_SCENE_H + 1, sh.t - sh.knob_t, tm);
    {   /* the thumb moves to the top row as a step changes to it, and in the last step before its second press; the
         * hands go out to the sides and come back as the grip changes; the knob hand comes in first, alone */
        int32_t rf = 256, sl = 0, st_ = 0, e;
        if (step == 2) rf = imin(256, u * 256 / 300);
        if (st->tap[1] && st->row[1] != st->row[0] && u >= (int32_t)st->tap[1] - TU_REACH) rf = imin(256, (u - (int32_t)st->tap[1] + TU_REACH) * 256 / 300);
        if ((step == 0 || step == 4) && u < 350) { e = 350 - u; sl = TU_SLIDE * e / 350 * e / 350; }
        if ((step == 3 || step == 4) && u > st->ms - 300) { e = u - (st->ms - 300); sl = TU_SLIDE * e / 300 * e / 300; }
        /* the hand at the keys: away while the first step only turns the knob, in as the second begins */
        st_ = step == 0 ? 3 * TU_SLIDE / 2 : step == 1 && u < 300 ? TU_SLIDE * (300 - u) / 300 * (300 - u) / 300 : sl;
        tu_device(st->grip, st->act == TA_ROLL || st->act == TA_SWAP || (st->act == TA_JUMP && u < TU_T_OFF), row, rf, age, sl, st_, c, tm);
        tu_mini(step, u, loops, tm);
    }
    /* under the scene, over the device: the rest of the line, in the size the settings ask for, on as many lines as it needs */
    {
        char buf[72];
        for (n = 0; rest[n] && n < 70; n++) buf[n] = rest[n] == '\n' ? ' ' : rest[n];
        buf[n] = 0;
        a = ts(1, 2);
        while (a > 1 && gblock_h(232, buf, a) > 3 * TLH(a)) a--;
        k = gblock_h(232, buf, a);
        gblock(4, TU_SCENE_H + 5 + imax(0, (3 * TLH(ts(1, 2)) - k) / 2), 232, buf, th->text, a, 1);
    }
    galpha(150 + ipulse(tm, 1400) * 106 / 256);  /* the foot: HOME back, and (taking turns with it) any knob steps */
    gfit(120, 231, 232, TR((tm / 3000u) & 1u ? T_HELP_KNOB : T_HELP_RETURN), (tm / 3000u) & 1u ? th->caps[3] : th->pad, 1);
    galpha(256);
}
/* any knob: the next step (or the one before), from its start; the sounds skipped over stay quiet */
static void tutor_step(int32_t d)
{
    int32_t step, u, loops, ur, k, base = 0, to, L = TU_LOOP * TU_HOLD;
    tu_where_r(sh.t, &step, &u, &loops, &ur);
    to = step + (d > 0 ? 1 : -1);
    if (to >= TU_N) { to = 0; loops++; }
    if (to < 0) { to = TU_N - 1; if (loops > 0) loops--; }
    for (k = 0; k < to; k++) base += TU_STEP[k].ms * TU_HOLD;
    sh.t = loops * L + base; sh.knob_t = sh.t; tu_last = base;
    snd_ui(d > 0 ? 79 : 72);
}
