/* SPDX-License-Identifier: GPL-3.0-only */
/* HOME in a machine: how it is played, shown the way the course's controls are (ac_tutor.h). A loop of short steps,
 * each a little scene of the machine over a line drawing of the FM-1, the hands doing what the step says, and on the
 * FM-1's own screen the same scene, very small. The words are the machine's own help lines, one to a step, so every
 * language has them.
 *   MYSTIC RUN    a key: hop a crystal; the top row: a tall one; a block: the button that lights; KNOB 1: the tempo
 *   JUICE WORKS   keys under a tap: a glass; KNOB 1: pour; to the ticket's line; the same keys: serve; hold and press
 *                 under another tap: the glass moves
 *   NIGHT BLOOM   KNOB 1 turns the petal to a spore; each spore on its colour; SELECT and the second ring; ALGORITHM
 *                 and the third (an arrow to each knob as it joins, as in the game)
 * HOME again returns; REC goes back to the Arcave. */

#define MT_MAXS 5
typedef struct { uint8_t n; uint16_t ms[MT_MAXS]; int16_t text[MT_MAXS]; uint8_t line[MT_MAXS]; } mt_def_t;
static const mt_def_t MT_DEF[3] = {
    {4, {3200, 3200, 3600, 3600, 0}, {T_RUN_1, T_RUN_1, T_RUN_2, T_RUN_3, 0}, {0, 1, 0, 0, 0}},
    {5, {2400, 3600, 3000, 2400, 3800}, {T_TABLE_2, T_TABLE_1, T_TABLE_3, T_TABLE_2, T_TABLE_2}, {0, 0, 0, 1, 2}},
    {4, {3600, 3600, 3800, 3800, 0}, {T_BLOOM_1, T_BLOOM_2, T_BLOOM_1, T_BLOOM_3, 0}, {0, 0, 1, 0, 0}},
};
static const uint8_t TU_KN[8][2] = {{12, 13}, {27, 13}, {12, 29}, {27, 29}, {85, 14}, {102, 14}, {120, 14}, {137, 14}};
enum { MK_MASTER, MK_SELECT, MK_PRESETS, MK_ALGO, MK_K1, MK_K2, MK_K3, MK_K4 };   /* the knobs as drawn */
static int32_t mt_last;                          /* where in the loop the last tick was (for the sounds) */

static int32_t mt_loop(int w) { int32_t k, s = 0; for (k = 0; k < MT_DEF[w].n; k++) s += MT_DEF[w].ms[k]; return s; }
static void mt_where(int w, int32_t t, int32_t *step, int32_t *u)
{
    int32_t k, lt = t % mt_loop(w);
    for (k = 0; k < MT_DEF[w].n - 1 && lt >= MT_DEF[w].ms[k]; k++) lt -= MT_DEF[w].ms[k];
    *step = k; *u = lt;
}
static int32_t mt_in(int32_t u) { return u < 300 ? TU_SLIDE * (300 - u) / 300 * (300 - u) / 300 : 0; }   /* a hand coming in */
static int32_t mt_out(int32_t u, int32_t ms) { return u > ms - 300 ? TU_SLIDE * (u - ms + 300) / 300 * (u - ms + 300) / 300 : 0; }
static int32_t mt_ramp(int32_t u, int32_t a, int32_t b) { return ease8(iclamp((u - a) * 256 / imax(1, b - a), 0, 256)); }
/* a key's chromatic number (as the game counts them) from the row and its place in it */
static int mt_chrom(int top, int idx)
{
    int k, n = 0;
    for (k = 0; k < AC_NKEY; k++) if (KEY_IS_SHARP[k] == top) { if (n == idx) return k; n++; }
    return 0;
}

/* ---------------------------------------------------------------- the FM-1 and the hands */
enum { MTA_PINCH, MTA_THUMB, MTA_FINGER };
/* a hand: a pinch on knob a (f: turning), a thumb on key a (0..15 the bottom row, 16.. the top row's sharps; f: how far
 * down, 0..256), a finger on button a (the grid's 0..11; f as the thumb's). side: 1 the hand comes from the right, -1
 * the left (a pinch's side follows its knob). sl: how far out to the side it still is */
typedef struct { uint8_t kind, a; int8_t side; int32_t f, sl; } mt_act_t;
static void mt_keyxy(int a, int32_t *x, int32_t *y)
{
    if (a < 16) { *x = TU_DX + 14 + a * 8; *y = TU_DY + 75; }
    else { *x = TU_DX + 18 + TU_SHARP_AFTER[a - 16] * 8; *y = TU_DY + 60; }
}
static void mt_btnxy(int a, int32_t *x, int32_t *y) { *x = TU_DX + 85 + (a % 6) * 10; *y = TU_DY + 32 + (a / 6) * 10; }
/* lit: keys lit (bit a as in mt_act_t); age[a]-style: down keys (ms since they went down, -1 none) are asked of keyage();
 * blit, bdown: a button lit, a button down (-1 none) */
static void mt_device(uint32_t lit, int32_t (*keyage)(int), int blit, int bdown, const mt_act_t *act, int n, col_t c, uint32_t tm)
{
    int32_t x0 = TU_DX, y0 = TU_DY, k, kturn = -1;
    for (k = 0; k < n; k++) if (act[k].kind == MTA_PINCH) kturn = act[k].a;
    tu_rbox(x0, y0, 150, 90, 4, th->rim);
    tu_rbox(x0 + 39, y0 + 7, 35, 35, 2, th->rim);
    galpha(120);
    gbox(x0 + 12, y0 + 41, 9, 5, th->rim); gbox(x0 + 22, y0 + 41, 9, 5, th->rim);
    tu_rbox(x0 + 6, y0 + 51, 138, 35, 3, th->rim);
    galpha(256);
    for (k = 0; k < 12; k++) {                   /* the buttons: one lit, one going down */
        int32_t bx = x0 + 81 + (k % 6) * 10, by = y0 + 28 + (k / 6) * 10;
        galpha(k == blit || k == bdown ? 256 : 120);
        gbox(bx, by, 8, 8, k == blit ? c : th->rim);
        if (k == bdown) grect(bx + 1, by + 1, 6, 6, th->pad);
        else if (k == blit && ipulse(tm, 600) > 128) grect(bx + 2, by + 2, 4, 4, c);
    }
    for (k = 0; k < 8; k++) {                    /* the knobs: the one in the fingers is drawn after the hand */
        int32_t a = -9000 + k * 5000;
        if (k == kturn) continue;
        galpha(120);
        gcirc(x0 + TU_KN[k][0], y0 + TU_KN[k][1], 4, th->rim);
        gline(x0 + TU_KN[k][0], y0 + TU_KN[k][1], x0 + TU_KN[k][0] + ((3 * icos(a)) >> 14), y0 + TU_KN[k][1] + ((3 * isin(a)) >> 14), th->rim);
    }
    galpha(256);
    for (k = 0; k < 16; k++) tu_key(x0 + 11 + k * 8, y0 + 69, (int)(lit >> k & 1u), keyage(k), c);
    for (k = 0; k < 11; k++) tu_key(x0 + 15 + TU_SHARP_AFTER[k] * 8, y0 + 54, (int)(lit >> (16 + k) & 1u), keyage(16 + k), c);
    for (k = 0; k < n; k++) {
        const mt_act_t *h = &act[k];
        if (h->kind == MTA_PINCH) {              /* the OK-sign pinch, twisting while it turns, its knob in its fingers */
            int32_t kx = x0 + TU_KN[h->a][0], ky = y0 + TU_KN[h->a][1], s = h->a >= MK_K1 ? -1 : 1, da = 0, ka = -9000, j;
            if (h->f) {
                int32_t ph = (int32_t)(tm % 700u), nn = (int32_t)(tm / 700u), e;
                if (ph < 480) { e = ph * 256 / 480; e = e * (512 - e) / 256; da = 5000 * e / 256; ka += nn * 5000 + da; }
                else { da = 5000 * (700 - ph) / 220; ka += nn * 5000 + 5000; }
            }
            for (j = 0; j < 3; j++) tu_icon(&TU_PINCH_B[j], 1, 0, 0, kx - s * h->sl, ky, s, da - s * TU_TILT, 130);
            tu_icon(&TU_PINCH[0], 1, 0, 0, kx - s * h->sl, ky, s, da - s * TU_TILT, 256);
            tu_icon(&TU_PINCH[3], 2, 0, 0, kx - s * h->sl, ky, s, da - s * TU_TILT, 256);
            tu_icon(&TU_PINCH[1], 2, 0, 0, kx - s * h->sl, ky, s, da - s * TU_TILT, 256);
            gdisc(kx, ky, 4, th->bg); gcirc(kx, ky, 4, h->f ? c : th->rim);
            gline(kx, ky, kx + ((3 * icos(ka)) >> 14), ky + ((3 * isin(ka)) >> 14), h->f ? c : th->rim);
        } else if (h->kind == MTA_THUMB) {       /* the thumbs-up hand, its thumb coming down on a key */
            int32_t px, py, s = h->side;
            mt_keyxy(h->a, &px, &py);
            tu_icon(TU_TAP, 2, TU_TAP_L, 3, px + s * (1 + 7 * (256 - h->f) / 256) + s * h->sl, py + 4 * (256 - h->f) / 256, s, 0, 256);
            if (h->f > 200) { galpha((h->f - 200) * 4); gcirc(px, py, 10, c); galpha(256); }
        } else {                                 /* the same hand on a button: the thumb comes in along the row and down on it */
            int32_t px, py, s = h->side;
            mt_btnxy(h->a, &px, &py);
            tu_icon(TU_TAP, 2, TU_TAP_L, 3, px + s * (1 + 7 * (256 - h->f) / 256) + s * h->sl, py + 3 * (256 - h->f) / 256, s, 0, 256);
            if (h->f > 200) { galpha((h->f - 200) * 4); gcirc(px, py, 9, c); galpha(256); }
        }
    }
}
/* the FM-1's own screen: the scene above, a third of its size (the brightest of each three by three, so thin lines
 * stay), the middle 99 columns of it from cx */
static void mt_mini(int32_t cx)
{
    int32_t x, y, i, j, sx0 = cx - 49, dx = TU_DX + 40, dy = TU_DY + 11;
    for (y = 0; y < TU_SCENE_H / 3; y++)
        for (x = 0; x < 33; x++) {
            col_t best = 0; int32_t bl = -1;
            for (j = 0; j < 3; j++)
                for (i = 0; i < 3; i++) {
                    int32_t sx = sx0 + x * 3 + i, sy = y * 3 + j, l;
                    col_t v;
                    if (sx < 0 || sx >= AC_W) continue;
                    v = fb_px[sy * AC_W + sx];
                    l = ((v >> 11) & 31) * 2 + ((v >> 5) & 63) + (v & 31) * 2;
                    if (l > bl) { bl = l; best = v; }
                }
            fb_px[(dy + y) * AC_W + dx + x] = best;
        }
}

/* ---------------------------------------------------------------- the three scenes. Each draws its step at u and says
 * which hands are where (act, returns how many), which keys are lit, which button */
static int32_t mt_age[27];                       /* how long ago each key went down in this frame's step (-1: not) */
static int32_t mt_keyage(int a) { return mt_age[a]; }
static void mt_press(int a, int32_t u, int32_t at) { if (u >= at && u - at < 420) mt_age[a] = u - at; }

static void mt_crystal(int32_t x, int32_t g, int tall, int32_t beat)   /* the game's own crystal (mr_crystal), larger */
{
    int32_t hh = tall ? 40 : 20, k, xy[6];
    galpha(90 + beat * 70 / 256); gdisc(x, g - hh / 2, hh / 2 + 9, th->caps[0]); galpha(256);
    for (k = 0; k < (tall ? 3 : 2); k++) {
        xy[0] = x - 11 + k * 7; xy[1] = g; xy[2] = x - 5 + k * 7 + (k - 1); xy[3] = g - hh + iabs(k - 1) * 6; xy[4] = x + 1 + k * 7; xy[5] = g;
        gpoly(xy, 3, th->text);
    }
    for (k = 0; k < (tall ? 3 : 2); k++) {
        xy[0] = x - 11 + k * 7; xy[1] = g; xy[2] = x - 5 + k * 7 + (k - 1); xy[3] = g - hh + iabs(k - 1) * 6; xy[4] = x + 1 + k * 7; xy[5] = g;
        gpolyline(xy, 3, 1, th->caps[0]);
    }
}
static int mt_run(int32_t step, int32_t u, uint32_t tm, mt_act_t *act, uint32_t *lit, int *blit, int *bdown)
{
    static const int16_t TA[2][2] = {{1000, 2400}, {1700, 0}};   /* when a crystal reaches Rock: steps 0 and 1 */
    int32_t g = 70, rx = 60, v = 90, s, k, hop = 0, bpm = 110, n = 0, beat;
    /* how far the ground has gone by: steady, then faster as KNOB 1 turns the tempo up */
    if (step == 3) { int32_t f = mt_ramp(u, 400, 2800); v = 90 + 60 * f / 256; bpm = 110 + 30 * f / 256; }
    s = (int32_t)(tm * 9u / 100u) + (step == 3 ? 60 * u / 1000 * mt_ramp(u, 400, 2800) / 256 : 0);
    beat = 256 - imin(256, (int32_t)((tm * (uint32_t)bpm / 60u) % 1000u) * 256 / 400);
    galpha(th->dark ? 70 : 90);                 /* mushrooms that swell on the beat: dim, far back */
    for (k = -1; k < 5; k++) {
        int32_t wx = (s / 70 + k) * 70, x = wx - s + 20;
        mush_draw(x, g, 120 + (int32_t)(ihash((uint32_t)(wx / 70)) >> 11) + beat * 16 / 256, th->caps[ihash((uint32_t)(wx / 70 + 3)) * 5u >> 16], th->stem, beat);
    }
    galpha(256);
    galpha(140 + beat * 116 / 256); ghl(0, 240, g, th->rim); galpha(256);
    for (k = 0; k < 240; k += 4) if (((k + s) / 4) % 3) { galpha(110); gp(k, g + 4, th->bands[0]); gp(k + 2, g + 8, th->bands[1]); }
    galpha(256);
    if (step <= 1) {                             /* crystals, and a hop over each, the key going down a quarter second before */
        for (k = 0; k < 2; k++) {
            int32_t ta = TA[step][k], d, x;
            if (!ta) continue;
            x = rx + (ta - u) * v / 1000;
            if (x > -20 && x < 260) mt_crystal(x, g, step == 1, beat);
            d = u - (ta - 250);
            if (d >= 0 && d < 500) hop = (step == 1 ? 50 : 28) * 4 * d / 500 * (500 - d) / 500;
            mt_press(step ? 16 + 6 : 9, u, ta - 250);
        }
        act[n].kind = MTA_THUMB; act[n].a = (uint8_t)(step ? 16 + 6 : 9); act[n].side = 1;
        act[n].f = tu_press(u - (TA[step][step == 0 && u >= 1450 ? 1 : 0] - 250)); act[n].sl = step == 0 ? mt_in(u) : 0; n++;
        if (step == 1) act[0].sl = mt_out(u, MT_DEF[0].ms[1]);
        *lit = step ? 0x7FFu << 16 : 0xFFFFu;
    } else if (step == 2) {                      /* a block: its button lights; pressed, it bursts before it reaches Rock */
        int32_t ta = 2600, x = rx + (ta - u) * v / 1000, at = 1500;
        if (u < at) {
            grect(x - 24, g - 34, 48, 34, th->panel); gbox(x - 24, g - 34, 48, 34, th->pad); gbox(x - 22, g - 32, 44, 30, th->caps[2]);
            gfit(x, g - 22, 40, "ENV", th->text, 2);
            *blit = 2;
        } else if (u < at + 500) {               /* (bits of it flying) */
            int32_t q = u - at;
            galpha(256 - q * 256 / 500);
            for (k = 0; k < 8; k++) grect(x + ((q * 40 / 500 * icos(k * 8192)) >> 14), g - 13 + ((q * 30 / 500 * isin(k * 8192)) >> 14), 3, 3, th->caps[k % 5]);
            gfit(x, g - 40 - q / 25, 80, TR(T_GOOD), th->pad, 2);
            galpha(256);
        }
        if (u >= at && u - at < 160) *bdown = 2;
        act[n].kind = MTA_FINGER; act[n].a = 2; act[n].side = 1; act[n].f = tu_press(u - at);
        act[n].sl = imax(mt_in(u), mt_out(u, MT_DEF[0].ms[2])); n++;
    } else {                                     /* KNOB 1: the tempo, the number going up */
        char b[12]; int32_t m = 0;
        num_str(b, bpm); while (b[m]) m++;
        b[m++] = ' '; b[m++] = 'B'; b[m++] = 'P'; b[m++] = 'M'; b[m] = 0;
        gtextc(120, 34, b, mt_ramp(u, 400, 2800) > 0 && u < 2900 ? th->pad : th->mute, 2);
        act[n].kind = MTA_PINCH; act[n].a = MK_K1; act[n].side = 1; act[n].f = u > 400 && u < 2800; act[n].sl = imax(mt_in(u), mt_out(u, MT_DEF[0].ms[3])); n++;
    }
    rock_draw(rx, g - 13 - hop, 360, s * 1304, (tm % 3100u) < 150u ? 40 : 256, th->rock, th->rock_rim, th->dark, th->moss, th->eye, th->pupil);
    return (int)n;
}

static void mt_tank(int32_t x, int k, int valve)
{
    col_t c = JW_COL[k];
    char nm[2] = {(char)('1' + k), 0};
    int32_t i;
    galpha(60); gdisc(x, 12, 16, c); galpha(256);
    gellf(x, 14, 13, 10, th->bg, 1); grect(x - 13, 14, 27, 6, th->bg);
    galpha(190); gellf(x, 14, 11, 8, c, 1); grect(x - 11, 14, 23, 4, c); galpha(256);
    garc(x, 14, 13, 10, 32768, 65536, th->text, 0); gline(x - 13, 14, x - 13, 20, th->text); gline(x + 13, 14, x + 13, 20, th->text); ghl(x - 13, x + 14, 20, th->text);
    gtextc(x + 1, 9, nm, th->bg, 1);
    grect(x - 2, 21, 5, 5, th->panel); gbox(x - 2, 21, 5, 5, th->text);
    for (i = 0; i < 3; i++) grect(x + 6 + i * 4, 22, 2, 4, i < valve ? c : th->mute);
}
static int mt_juice(int32_t step, int32_t u, uint32_t tm, mt_act_t *act, uint32_t *lit, int *blit, int *bdown)
{
    static const int32_t BX[2] = {84, 156};
    int32_t floor = 76, n = 0, k, valve = 0, gx = BX[0], has = 1, gy = floor, galp = 256;
    jw_glass_t g, tk;
    (void)blit; (void)bdown;
    g.n = 0; tk.n = 1; tk.c[0] = 0; tk.v[0] = Q8(3);
    /* the glass: none, then one; filling; full to the ticket's line; served; a new one moved over */
    if (step == 0) has = u >= 700;
    else if (step == 1) {
        valve = u < 300 ? 0 : u < 450 ? 1 : u < 600 ? 2 : u < 2700 ? 3 : u < 2850 ? 2 : u < 3000 ? 1 : 0;
        if (u > 700) { g.n = 1; g.c[0] = 0; g.v[0] = Q8(3) * imin(2000, u - 700) / 2000; }
    } else if (step == 2) { g.n = 1; g.c[0] = 0; g.v[0] = Q8(3); }
    else if (step == 3) {
        g.n = 1; g.c[0] = 0; g.v[0] = Q8(3);
        if (u >= 700) { int32_t q = imin(500, u - 700); gy = floor - q * 40 / 500; galp = 256 - q * 256 / 500; }
    } else {
        has = u >= 200;
        if (u >= 1500) gx = BX[0] + (BX[1] - BX[0]) * mt_ramp(u, 1500, 1900) / 256;
    }
    ghl(0, 240, 3, th->rim);
    for (k = 0; k < 2; k++) { grect(BX[k] - 1, 3, 3, 3, th->rim); mt_tank(BX[k], k, k == 0 ? valve : 0); }
    if (valve) {                                 /* the stream, down to the glass */
        int32_t top = floor - 1 - (has ? jw_total(&g) * JW_UPX / 256 : 0), y;
        for (y = 26; y < top; y += 4) grect(BX[0] - (valve + 1) / 2 + ((tsin(tm, 90, y * 900) > 9000) ? 1 : 0), y, valve + 1, imin(4, top - y), JW_COL[0]);
    }
    ghl(0, 240, floor + 1, th->rim);
    for (k = 0; k < 2; k++) if (!(has && iabs(gx - BX[k]) < 4)) { galpha(70); gbox(BX[k] - 9, floor - 3, 19, 3, th->mute); galpha(256); }
    if (has) {
        int32_t pop = step == 0 ? mt_ramp(u, 700, 900) : step == 4 ? mt_ramp(u, 200, 400) : 256;
        galpha(galp * pop / 256); jw_glass_draw(gx, gy, 8, &g, step == 2 ? th->pad : th->text); galpha(256);
    }
    if (step < 3 || (step == 3 && u < 1200)) {   /* the ticket: three parts of the yellow */
        int32_t tx = 214, hl = step == 2 && ipulse(tm, 700) > 128;
        galpha(step == 3 ? 256 - imin(256, imax(0, u - 700) * 256 / 500) : 256);
        gline(tx, 3, tx, 10, th->mute);
        grect(tx - 11, 10, 23, 58, th->panel); gbox(tx - 11, 10, 23, 58, hl ? th->pad : th->text);
        jw_glass_draw(tx, 62, 8, &tk, th->text);   /* (the same glass as the one being filled) */
        galpha(256);
        if (step == 2) {                         /* the glass and the ticket: the same line */
            int32_t ly = floor - 1 - 3 * JW_UPX, ty = 61 - 3 * JW_UPX, x;
            for (x = gx + 11; x < tx - 13; x += 4) gp(x, ly + (ty - ly) * (x - gx - 11) / (tx - 24 - gx), th->pad);
        }
    }
    if (step == 3 && u >= 700 && u < 1700) { galpha(256 - imax(0, u - 1300) * 256 / 400); gfit(120, 44 - (u - 700) / 60, 120, TR(T_GOOD), th->pad, 2); galpha(256); }
    /* the hands: the left thumb under tap 1 (keys 0-3); the right hand on KNOB 1; at the end the right thumb under tap 2 */
    act[n].kind = MTA_THUMB; act[n].a = 2; act[n].side = -1;
    act[n].f = step == 0 ? tu_press(u - 700) : step == 3 ? tu_press(u - 700) : step == 4 ? (u < 600 ? tu_press(u - 600) : u < 2600 ? 256 : tu_press(u - 2600 + 150)) : 0;
    act[n].sl = step == 0 ? mt_in(u) : step == 4 ? mt_out(u, MT_DEF[1].ms[4]) : 0; n++;
    if (step == 0) mt_press(2, u, 700);
    if (step == 3) mt_press(2, u, 700);
    if (step == 4 && u >= 600 && u < 2600) mt_age[2] = 100;
    if (step >= 1 && step <= 3) {
        act[n].kind = MTA_PINCH; act[n].a = MK_K1; act[n].side = 1; act[n].f = step == 1 && ((u > 300 && u < 650) || (u > 2700 && u < 3050));
        act[n].sl = step == 1 ? mt_in(u) : step == 3 ? mt_out(u, MT_DEF[1].ms[3]) : 0; n++;
    }
    if (step == 4) {
        act[n].kind = MTA_THUMB; act[n].a = 5; act[n].side = 1; act[n].f = tu_press(u - 1500); act[n].sl = imax(mt_in(u - 700), mt_out(u, MT_DEF[1].ms[4])); n++;
        mt_press(5, u, 1500);
    }
    *lit = step == 4 && u >= 600 ? 0xFFu : 0x0Fu;
    return (int)n;
}

static void mt_ring(int32_t cx, int32_t cy, int32_t r, int32_t ang, col_t c, int32_t hit, int32_t a)
{
    galpha(96 * a / 256); garc(cx, cy, r, r, 0, 65536, c, 1);
    galpha(a);
    garc(cx, cy, r - 1, r - 1, ang - NB_ARC, ang + NB_ARC, hit ? th->text : c, 1);
    garc(cx, cy, r + 1, r + 1, ang - NB_ARC, ang + NB_ARC, hit ? th->text : c, 1);
    garc(cx, cy, r + 3, r + 3, ang - NB_ARC + 900, ang + NB_ARC - 900, c, 1);
    galpha(256);
}
static void mt_spore(int32_t cx, int32_t cy, int32_t r, int32_t ang, col_t c)
{
    int32_t x = cx + ((r * icos(ang)) >> 14), y = cy + ((r * isin(ang)) >> 14);
    galpha(110); gdisc(x, y, 6, c); galpha(256); gdisc(x, y, 2, c);
}
/* a spore coming in from far to ring r, slowing a little near it (as in the game), arriving at t1 */
static int32_t mt_in_r(int32_t u, int32_t t0, int32_t t1, int32_t r)
{
    int32_t q = iclamp(t1 - u, 0, t1 - t0) * 256 / (t1 - t0);
    return r + (130 - r) * (3 * q + q * q / 256) / 1024;
}
static int mt_bloom(int32_t step, int32_t u, uint32_t tm, mt_act_t *act, uint32_t *lit, int *blit, int *bdown)
{
    static const int16_t R[3] = {14, 25, 36};
    int32_t cx = 120, cy = 40, n = 0, k, a1 = -16384, a2 = -16384, a3 = -16384, w2 = 0, w3 = 0, hit1 = 0, hit2 = 0, hit3 = 0, bloomed = 0;
    (void)blit; (void)bdown;
    /* where the petals are: turned toward each spore before it arrives */
    if (step == 0) a1 = -16384 + (8000 + 16384) * mt_ramp(u, 500, 2200) / 256;
    else if (step == 1) a1 = 8000 + (30000 - 8000) * mt_ramp(u, 300, 1100) / 256 + (45536 - 30000) * mt_ramp(u, 1700, 2700) / 256;
    else a1 = 45536;
    if (step >= 2) { w2 = step == 2 ? mt_ramp(u, 0, 600) : 256; a2 = step == 2 ? -16384 + (20000 + 16384) * mt_ramp(u, 800, 2400) / 256 : 20000; }
    if (step == 3) { w3 = mt_ramp(u, 0, 600); a3 = -16384 + (-16384 + 40000) * mt_ramp(u, 800, 2400) / 256; }
    galpha(140); for (k = 0; k < 18; k++) gp((int32_t)(ihash((uint32_t)k * 7u + 1u) % 240u), (int32_t)(ihash((uint32_t)k * 3u + 9u) % 76u), th->mute); galpha(256);
    /* the garden: a flower for each spore caught so far in the loop */
    bloomed = step == 0 ? (u >= 2600) : step == 1 ? 1 + (u >= 1300) + (u >= 3000) : step == 2 ? 3 + (u >= 3000) : 4 + (u >= 3000);
    for (k = 0; k < bloomed; k++) {
        int32_t x = k < 3 ? 14 + k * 18 : 190 + (k - 3) * 18, y = k & 1 ? 66 : 14;
        col_t c = NB_COL[k == 3 ? 1 : k == 4 ? 2 : 0];
        galpha(60); gdisc(x, y, 6, c); galpha(256);
        gp(x - 3, y, c); gp(x + 3, y, c); gp(x, y - 3, c); gp(x, y + 3, c); grect(x - 1, y - 1, 3, 3, c);
    }
    /* the spores and their catches */
    if (step == 0) { if (u < 2600) mt_spore(cx, cy, mt_in_r(u, 200, 2600, R[0]), 8000, NB_COL[0]); else hit1 = u < 2850; }
    if (step == 1) {
        if (u < 1300) mt_spore(cx, cy, mt_in_r(u, -800, 1300, R[0]), 30000, NB_COL[0]); else if (u < 1550) hit1 = 1;
        if (u < 3000) mt_spore(cx, cy, mt_in_r(u, 800, 3000, R[0]), 45536, NB_COL[0]); else if (u < 3250) hit1 = 1;
    }
    if (step == 2) { if (u < 3000) { if (u > 700) mt_spore(cx, cy, mt_in_r(u, 700, 3000, R[1]), 20000, NB_COL[1]); } else hit2 = u < 3250; }
    if (step == 3) { if (u < 3000) { if (u > 700) mt_spore(cx, cy, mt_in_r(u, 700, 3000, R[2]), 40000, NB_COL[2]); } else hit3 = u < 3250; }
    mt_ring(cx, cy, R[0], a1, NB_COL[0], hit1, 256);
    if (hit1) { galpha(160); gdisc(cx + ((R[0] * icos(a1)) >> 14), cy + ((R[0] * isin(a1)) >> 14), 8, NB_COL[0]); galpha(256); }
    if (w2) {
        mt_ring(cx, cy, R[1], a2, NB_COL[1], hit2, w2);
        if (hit2) { galpha(160); gdisc(cx + ((R[1] * icos(a2)) >> 14), cy + ((R[1] * isin(a2)) >> 14), 8, NB_COL[1]); galpha(256); }
    }
    if (w3) {
        mt_ring(cx, cy, R[2], a3, NB_COL[2], hit3, w3);
        if (hit3) { galpha(160); gdisc(cx + ((R[2] * icos(a3)) >> 14), cy + ((R[2] * isin(a3)) >> 14), 8, NB_COL[2]); galpha(256); }
    }
    for (k = 0; k < 5; k++) {                    /* the flower in the middle */
        int32_t a = k * 13107 + (int32_t)(tm * 2u);
        gdisc(cx + ((6 * icos(a)) >> 14), cy + ((6 * isin(a)) >> 14), 3, th->caps[0]);
    }
    gdisc(cx, cy, 3, th->pad);
    /* the arrow to the knob that joins, as in the game: KNOB 1 at the start, SELECT, ALGORITHM */
    {
        int32_t ta = step == 0 ? imin(256, imax(0, 1800 - u) * 256 / 300) : (step == 2 || step == 3) ? imin(mt_ramp(u, 0, 300), imax(0, 2600 - u) * 256 / 300) : 0;
        if (step == 0) nb_tag(228, 10, -8192, 192, 18, 0, ta, tm, 1);
        if (step == 2) nb_tag(12, 10, -24576, 36, 22, 1, ta, tm, 1);
        if (step == 3) nb_tag(12, 70, 24576, 40, 50, 2, ta, tm, 1);
    }
    /* the hands: the right hand on KNOB 1 throughout; the left on SELECT for the second ring, ALGORITHM for the third */
    act[n].kind = MTA_PINCH; act[n].a = MK_K1; act[n].side = 1;
    act[n].f = (step == 0 && u > 500 && u < 2200) || (step == 1 && ((u > 300 && u < 1100) || (u > 1700 && u < 2700)));
    act[n].sl = step == 0 ? mt_in(u) : step == 3 ? mt_out(u, MT_DEF[2].ms[3]) : 0; n++;
    if (step == 2) { act[n].kind = MTA_PINCH; act[n].a = MK_SELECT; act[n].side = -1; act[n].f = u > 800 && u < 2400; act[n].sl = imax(mt_in(u - 300), mt_out(u, MT_DEF[2].ms[2])); n++; }
    if (step == 3) { act[n].kind = MTA_PINCH; act[n].a = MK_ALGO; act[n].side = -1; act[n].f = u > 800 && u < 2400; act[n].sl = imax(mt_in(u - 300), mt_out(u, MT_DEF[2].ms[3])); n++; }
    *lit = 0;
    return (int)n;
}

/* ---------------------------------------------------------------- the loop: sounds, lights, the page */
static void mtutor_tick(int32_t dt)
{
    int w = mc.which;
    int32_t loop = mt_loop(w), lt = sh.t % loop, a = mt_last, step, u, base, k;
    /* the moments that sound: {step, ms, what} - 0 a low key, 1 a high key, 2 a button, 3 a catch, 4 a glass, 5 served */
    static const int16_t EV[3][8][3] = {
        {{0, 750, 0}, {0, 2150, 0}, {1, 1450, 1}, {2, 1500, 2}, {-1, 0, 0}},
        {{0, 700, 4}, {3, 700, 5}, {4, 200, 4}, {4, 1500, 0}, {-1, 0, 0}},
        {{0, 2600, 3}, {1, 1300, 3}, {1, 3000, 3}, {2, 3000, 3}, {3, 3000, 3}, {-1, 0, 0}},
    };
    (void)dt;
    if (lt < a) a = -1;
    for (k = 0; k < 8 && EV[w][k][0] >= 0; k++) {
        int32_t s, at;
        for (s = 0, base = 0; s < EV[w][k][0]; s++) base += MT_DEF[w].ms[s];
        at = base + EV[w][k][1];
        if (a < at && lt >= at) switch (EV[w][k][2]) {
            case 0: sfx(key_note(5), 0, 500, W_TRI, 2800, 5, 180); break;
            case 1: sfx(key_note(18), 0, 500, W_TRI, 2800, 5, 180); break;
            case 2: sfx(67, 0, 300, W_SQUARE, 1800, 2, 140); sfx(96, 0, 120, W_NOISE, 1200, 1, 100); break;
            case 3: sfx(72 + SC_PEN[k % 5], 0, 600, W_SINE, 3000, 4, 170); break;
            case 4: sfx(90, 0, 160, W_SINE, 1800, 1, 160); break;
            default: snd_chime(1); break;
        }
    }
    mt_last = lt;
    mt_where(w, sh.t, &step, &u);
    (void)u; (void)step;
    led_breathe_want |= 1u << BTN_REC;
}
static void mtutor_draw(void)
{
    int w = mc.which;
    int32_t step, u, k, n = 0, sc, a;
    uint32_t tm = (uint32_t)sh.t, lit = 0, rowmask = 0;
    int blit = -1, bdown = -1;
    mt_act_t act[4];
    col_t c = th->caps[w * 2 % 5];
    mt_where(w, sh.t, &step, &u);
    for (k = 0; k < 27; k++) mt_age[k] = -1;
    grect(0, 0, AC_W, AC_H, th->bg);
    gclip_y1 = TU_SCENE_H;
    n = w == 0 ? mt_run(step, u, tm, act, &lit, &blit, &bdown) : w == 1 ? mt_juice(step, u, tm, act, &lit, &blit, &bdown) : mt_bloom(step, u, tm, act, &lit, &blit, &bdown);
    gclip_y1 = AC_H;
    mt_mini(w == 0 ? 100 : 120);
    if (u < 250) { galpha(256 - u * 256 / 250); grect(0, 0, AC_W, TU_SCENE_H, th->bg); galpha(256); }   /* each step fades in */
    /* under the scene, a mark for each step (the machine is the one just left: the scene is all its own) */
    (void)sc;
    for (k = 0; k < MT_DEF[w].n; k++) grect(120 - MT_DEF[w].n * 8 + k * 16, TU_SCENE_H + 1, 14, 2, k == step ? th->pad : th->mute);
    {   /* the step's line, under the scene, over the device */
        char buf[120];
        const char *s = TR(MT_DEF[w].text[step]);
        int32_t m = 0, ln = MT_DEF[w].line[step], h;
        while (ln > 0 && *s) { if (*s == '\n') ln--; s++; }
        while (*s && *s != '\n' && m < 118) buf[m++] = *s++;
        buf[m] = 0;
        a = ts(1, 2);
        if (gblock_h(232, buf, a) > 3 * TLH(a)) {   /* too long for three lines: its two sentences, one after the other */
            int32_t j;
            for (j = 0; buf[j] && !(buf[j] == '.' && buf[j + 1] == ' '); j++) ;
            if (buf[j]) {
                if (u < MT_DEF[w].ms[step] / 2) buf[j + 1] = 0;
                else { int32_t q = 0; j += 2; while (buf[j]) buf[q++] = buf[j++]; buf[q] = 0; }
            }
        }
        while (a > 1 && gblock_h(232, buf, a) > 3 * TLH(a)) a--;
        h = gblock_h(232, buf, a);
        galpha(imin(256, u * 256 / 250));
        gblock(4, TU_SCENE_H + 5 + imax(0, (3 * TLH(ts(1, 2)) - h) / 2), 232, buf, th->text, a, 1);
        galpha(256);
    }
    mt_device(lit, mt_keyage, blit, bdown, act, (int)n, c, tm);
    /* the lights on the FM-1 itself: the keys the step is about, and any going down */
    for (k = 0; k < 27; k++) {
        int top = k >= 16, idx = top ? k - 16 : k;
        if (top && idx > 10) continue;
        if (lit >> k & 1u) rowmask |= 1u << mt_chrom(top, idx);
        if (mt_age[k] >= 0 && mt_age[k] < 220) led_key_want |= 1u << mt_chrom(top, idx);
    }
    led_key_dim_want |= rowmask;
    if (bdown >= 0) led_btn_want |= 1u << (bdown + 2); else if (blit >= 0) led_btn_want |= 1u << (blit + 2);
    /* the foot: HOME back to the machine, and (taking turns with it) REC back to the Arcave */
    galpha(150 + ipulse(tm, 1400) * 106 / 256);
    gfit(120, 231, 232, TR((tm / 3000u) & 1u ? T_REC_BACK : T_HELP_RETURN), (tm / 3000u) & 1u ? th->mute : th->pad, 1);
    galpha(256);
}
