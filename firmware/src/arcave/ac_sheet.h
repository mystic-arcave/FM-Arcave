/* SPDX-License-Identifier: GPL-3.0-only */
/* Sheets laid over a paused game, and the FX button's effects.
 *   controls   HOME on the course: what rolls and what jumps. HOME in a machine: how that machine is played
 *              (REC from there goes back to the Arcave). HOME again returns.
 *   scores     EDIT anywhere, or the board in the Arcave: the best score of each machine, the time played, the
 *              secrets found. EDIT, PLAY or HOME closes.
 *   FX         one of a dozen pairs of picture and sound, a second long, over whatever is on the screen. */

enum { SH_NONE, SH_INTRO, SH_MACH, SH_SCORES };
enum { SA_NONE, SA_CLOSED, SA_LEAVE };
static struct { uint8_t on; int32_t t; } sh;
static uint32_t play_s, play_ms;                 /* the time played: whole seconds (kept in the save) and the rest */

static void sheet_open(int kind) { sh.on = (uint8_t)kind; sh.t = 0; snd_ui(74); sy_rumble_want = 0; }
static int sheet_tick(int32_t dt)
{
    sh.t += dt;
    if (sh.on == SH_SCORES) {
        if (BDOWN(BTN_EDIT) || BDOWN(BTN_PLAY) || BDOWN(BTN_HOME)) { sh.on = SH_NONE; snd_ui(67); return SA_CLOSED; }
        led_btn_want |= 1u << BTN_EDIT;
        return SA_NONE;
    }
    if (BDOWN(BTN_HOME)) { sh.on = SH_NONE; snd_ui(67); return SA_CLOSED; }
    if (sh.on == SH_MACH && BDOWN(BTN_REC)) { sh.on = SH_NONE; return SA_LEAVE; }
    led_breathe_want |= 1u << BTN_HOME;
    return SA_NONE;
}
/* lines one under another, centred, as large as the sheet has room for. ids: a text, -1 half a line of air,
 * -2 - k the k-th extra string; the colours go with them */
static void sheet_lines(const int *ids, const col_t *cols, int32_t n, const char *const *extra, int32_t y0, int32_t y1)
{
    int32_t a = ts(1, 3), k, h, y;
    for (;; a--) {
        h = 0;
        for (k = 0; k < n; k++) h += ids[k] == -1 ? TLH(a) / 2 : gblock_h(228, ids[k] <= -2 ? extra[-2 - ids[k]] : TR(ids[k]), a) + 2;
        if (h <= y1 - y0 || a == 1) break;
    }
    y = y0 + imax(0, (y1 - y0 - h) / 2);
    for (k = 0; k < n; k++) {
        if (ids[k] == -1) { y += TLH(a) / 2; continue; }
        y += gblock(6, y, 228, ids[k] <= -2 ? extra[-2 - ids[k]] : TR(ids[k]), cols[k], a, 1) + 2;
    }
}
static void sheet_row(int32_t y, const char *lab, const char *val, col_t c)
{
    int32_t sc = ts(1, 3);
    while (sc > 1 && gtextw(lab, sc) + gtextw(val, sc) + 12 > 220) sc--;
    gtext(10, y + (TLH(ts(1, 3)) - 5 * sc) / 2, lab, th->text, sc);
    gtextr(230, y + (TLH(ts(1, 3)) - 5 * sc) / 2, val, c, sc);
}
static void sheet_draw(void)
{
    gveil(th->bg, 28);
    if (sh.on == SH_INTRO) {
        static const int IDS[8] = {T_HELP_ROLL, -1, T_HELP_LOW, T_HELP_HIGH, -1, T_HELP_PADS, -1, T_HELP_RETURN};
        col_t cols[8] = {th->text, 0, th->text, th->text, 0, th->text, 0, th->pad};
        sheet_lines(IDS, cols, 8, 0, 6, 234);
    } else if (sh.on == SH_MACH) {
        static const int T1[3] = {T_RUN_1, T_TABLE_1, T_BLOOM_1};
        int ids[8]; col_t cols[8]; const char *ex[1];
        ex[0] = MACH_NAME[mc.which];
        ids[0] = -2; cols[0] = th->caps[mc.which * 2 % 5]; ids[1] = -1; cols[1] = 0;
        ids[2] = T1[mc.which]; ids[3] = T1[mc.which] + 1; ids[4] = T1[mc.which] + 2; cols[2] = cols[3] = cols[4] = th->text;
        ids[5] = -1; cols[5] = 0; ids[6] = T_HELP_RETURN; cols[6] = th->pad; ids[7] = T_REC_BACK; cols[7] = th->mute;
        sheet_lines(ids, cols, 8, ex, 4, 236);
    } else {
        char b[5][16];
        int32_t st = ts(2, 4), rh = TLH(ts(1, 3)) + 4, y, used, t = (int32_t)play_s, n;
        used = gfit(120, 6 + 2 * st, 232, TR(T_SCORES), th->caps[3], st);
        y = 6 + 2 * st + 5 * used + 10;
        rh = imin(rh, (238 - TLH(ts(1, 2)) - 6 - y) / 6);   /* six rows above the foot */
        num_str(b[0], (int32_t)best[0]); num_str(b[1], (int32_t)best[2]);
        sheet_row(y, MACH_NAME[0], b[0], th->caps[0]); y += rh;
        sheet_row(y, MACH_NAME[2], b[1], th->caps[4]); y += rh;
        num_str(b[3], (int32_t)best[1]);
        sheet_row(y, MACH_NAME[1], b[3], th->caps[2]); y += rh;
        if (race_best_cs) race_str(b[4], race_best_cs); else { b[4][0] = '-'; b[4][1] = 0; }
        sheet_row(y, TR(T_RACE), b[4], th->caps[3]); y += rh + 2;
        num_str(b[2], t / 3600); for (n = 0; b[2][n]; n++) ;
        b[2][n++] = ':'; b[2][n++] = (char)('0' + t / 600 % 6); b[2][n++] = (char)('0' + t / 60 % 10);
        b[2][n++] = ':'; b[2][n++] = (char)('0' + t / 10 % 6); b[2][n++] = (char)('0' + t % 10); b[2][n] = 0;
        sheet_row(y, TR(T_PLAYTIME), b[2], th->pad); y += rh;
        b[3][0] = (char)('0' + (cv.secrets & 1u) + (cv.secrets >> 1 & 1u) + (cv.secrets >> 2 & 1u)); b[3][1] = '/'; b[3][2] = '3'; b[3][3] = 0;
        sheet_row(y, TR(T_SECRETS), b[3], th->pad);
        gfit(120, 238 - TLH(ts(1, 2)) + 2 * ts(1, 2), 232, TR(T_SCORES_CLOSE), th->mute, ts(1, 2));
    }
}

/* ---------------------------------------------------------------- FX: twelve of them */
#define FX_N 12
#define FX_MS 1000
static struct { int32_t t; uint8_t kind, on, last; uint32_t seed; } fxs;
static col_t fx_row[AC_W];

static void fx_trigger(void)
{
    static const uint8_t UP[6] = {60, 64, 67, 72, 76, 79};
    uint32_t i, k = ac_rnd() % FX_N;
    if (k == fxs.last) k = (k + 1u + ac_rnd() % (FX_N - 1u)) % FX_N;   /* never the same one twice running */
    fxs.kind = fxs.last = (uint8_t)k; fxs.t = 0; fxs.on = 1; fxs.seed = ac_rnd();
    switch (k) {
    case 0: for (i = 0; i < 6u; i++) sfx(UP[i] + 12, i * 60u, 220, W_SQUARE, 1500, 2, 120); break;                      /* inverted: a square run up */
    case 1: sfx_ex(69, 0, 900, W_TRI, 3400, 20, 160, 0, -40, 65480); sfx_ex(64, 0, 900, W_TRI, 2600, 20, 160, 0, 40, 65250); break;   /* a wave: two tones sliding apart */
    case 2: for (i = 0; i < 5u; i++) sfx(90 - (int32_t)i * 9, i * 150u, 120, W_NOISE, 2200, 1, 60); break;               /* mosaic: steps of noise */
    case 3: for (i = 0; i < 8u; i++) sfx(60 + SC_PEN[i % 5u] + 12 * (int32_t)(i / 5u), i * 70u, 500, W_SINE, 2400, 4, 200); break;   /* rainbow: the scale */
    case 4: sfx_ex(36, 0, 500, W_SINE, 7000, 2, 0, 0, 0, 65300); sfx(84, 0, 400, W_NOISE, 1800, 2, 80); break;           /* shake: a thump */
    case 5: sfx(60, 0, 1200, W_SINE, 2600, 30, 200); sfx(67, 0, 1200, W_SINE, 2600, 30, 200); sfx(76, 0, 1200, W_SINE, 2600, 30, 200); break;   /* mirror: a chord */
    case 6: sfx(84, 0, 1600, W_SINE, 3400, 2, 230); sfx(91, 0, 900, W_SINE, 1400, 2, 230); sfx(96, 30, 500, W_SINE, 900, 2, 230); break;      /* tint: a bell */
    case 7: snd_whoosh(); break;                                                                                         /* burst */
    case 8: sfx(48, 0, 700, W_SINE, 4200, 120, 100); sfx(48, 480, 700, W_SINE, 4200, 120, 100); break;                   /* a slow glow: two swells */
    case 9: sfx_ex(79, 0, 900, W_SAW, 2200, 4, 150, 0, 0, 65250); break;                                                 /* slide: a falling tone */
    case 10: for (i = 0; i < 4u; i++) sfx(88, i * 220u, 500, W_SINE, 2600 - i * 500u, 2, 240); break;                    /* rings: pings dying away */
    default: for (i = 0; i < 10u; i++) sfx(84 + SC_PEN[ihash(fxs.seed + i) % 5u] + 12 * (int32_t)(i & 1u), i * 80u, 300, W_TRI, 1600, 2, 180); break;   /* confetti: a sprinkle */
    }
}
static void fx_rotate_row(int32_t y, int32_t d)  /* a row moved d to the right, what leaves comes back in */
{
    col_t *p = &fb_px[y * AC_W];
    int32_t x;
    d = ((d % AC_W) + AC_W) % AC_W;
    if (!d) return;
    for (x = 0; x < AC_W; x++) fx_row[(x + d) % AC_W] = p[x];
    for (x = 0; x < AC_W; x++) p[x] = fx_row[x];
}
static void fx_apply(int32_t dt)                 /* after the scene is drawn, before the banner */
{
    int32_t t, p, q, x, y, k;
    if (!fxs.on) return;
    fxs.t += dt; t = fxs.t;
    if (t >= FX_MS) { fxs.on = 0; return; }
    p = t * 256 / FX_MS; q = 256 - p;            /* how far through, and how much is left */
    switch (fxs.kind) {
    case 0:                                      /* inverted, twice, slowly (never a strobe) */
        if ((t / 250) % 2 == 0) for (k = 0; k < AC_W * AC_H; k++) fb_px[k] = (col_t)~fb_px[k];
        break;
    case 1:                                      /* a wave through the rows */
        for (y = 0; y < AC_H; y++) fx_rotate_row(y, (isin(y * 900 + t * 90) * 14 >> 14) * q / 256);
        break;
    case 2: {                                    /* mosaic: coarse, then fine again */
        int32_t b = 2 + (p < 128 ? p : 256 - p) / 10;
        for (y = 0; y < AC_H; y++)
            for (x = 0; x < AC_W; x++) fb_px[y * AC_W + x] = fb_px[(y - y % b) * AC_W + (x - x % b)];
        break;
    }
    case 3:                                      /* bands of the five colours crossing */
        for (y = 0; y < AC_H; y++)
            for (x = 0; x < AC_W; x++) {
                col_t *px = &fb_px[y * AC_W + x];
                *px = mix565(*px, th->caps[((x + y + t / 3) / 28) % 5], (uint32_t)(14 * q / 256));
            }
        break;
    case 4:                                      /* a shake that settles */
        k = (int32_t)(ihash(fxs.seed + (uint32_t)(t / 40)) % 21u) - 10;
        for (y = 0; y < AC_H; y++) fx_rotate_row(y, k * q / 256);
        break;
    case 5:                                      /* a mirror down the middle */
        for (y = 0; y < AC_H; y++)
            for (x = 0; x < AC_W / 2; x++) fb_px[y * AC_W + AC_W - 1 - x] = fb_px[y * AC_W + x];
        break;
    case 6:                                      /* the colours, one after another, as a wash */
        gveil(th->caps[(t / 180) % 5], (uint32_t)(15 * q / 256));
        break;
    case 7:                                      /* a burst from the middle */
        for (k = 0; k < 28; k++) {
            int32_t a = (int32_t)ihash(fxs.seed + (uint32_t)k), r = 8 + (int32_t)(40 + ihash(fxs.seed + 99u + (uint32_t)k) % 120u) * p / 256;
            galpha(q); gdisc(120 + ((r * icos(a)) >> 14), 120 + ((r * isin(a)) >> 14), 2 + (k & 1), th->caps[k % 5]);
        }
        galpha(256);
        break;
    case 8:                                      /* two slow swells of light */
        gveil(th->pad, (uint32_t)(iabs(isin(t * 65536 / FX_MS)) * 14 >> 14));
        break;
    case 9:                                      /* the picture slides once round */
        k = AC_W * ease8(p) / 256;
        for (y = 0; y < AC_H; y++) fx_rotate_row(y, k);
        break;
    case 10:                                     /* rings going out */
        for (k = 0; k < 4; k++) {
            int32_t r = (p - k * 40) * 170 / 256;
            if (r > 2) { galpha(imax(0, q - k * 20)); garc(120, 120, r, r, 0, 65536, th->caps[k + 1], 1); }
        }
        galpha(256);
        break;
    default:                                     /* confetti falling */
        for (k = 0; k < 44; k++) {
            int32_t cx = (int32_t)(ihash(fxs.seed + (uint32_t)k) % 240u), sp = 180 + (int32_t)(ihash(fxs.seed + 50u + (uint32_t)k) % 200u);
            int32_t cy = -10 + sp * t / FX_MS + (isin(t * 200 + k * 9000) >> 12);
            galpha(imin(256, q * 2)); grect(cx + (isin(t * 120 + k * 5000) >> 11), cy, 3 + (k & 1), 3 + (k & 1), th->caps[k % 5]);
        }
        galpha(256);
        break;
    }
}
