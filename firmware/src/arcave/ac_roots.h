/* SPDX-License-Identifier: GPL-3.0-only */
/* ROOTS (ac_room.h has the controls every instrument shares). Sixteen roots hang from the roof of the cave, one to
 * each key of the bottom row; each is a chain of joints with a pod at its tip. The roots are silent: a sound comes
 * only when a pod is struck (a key, the beetle) or two neighbouring pods knock together, and the harder the knock
 * the louder. So the wind is the random generator, and what hangs where turns it into a pattern.
 *
 *   NOTHING  a bare root, silent                     POD     a dry shaker
 *   NUT      a tuned woodblock                       BELL    it rings
 *   GOURD    a low drum                              HUSK    it snaps like a snare
 *   (SIZE: the pod's weight and girth; a heavy one swings slower, is moved less by the wind and knocks harder)
 *
 *   KNOB 1 WIND     how strong                       KNOB 2 GUST    steady, or a gust every 4 bars, 2 bars, bar,
 *   KNOB 3 LENGTH   long roots swing slower and                     half bar, beat, half beat, quarter beat
 *                   reach their neighbours           KNOB 4 STIFF   how soon a root comes to rest
 *   SELECT LEAN     which way the wind blows: 0 hard left .. 4 to and fro .. 8 hard right
 *   PRESETS         the weather: BREEZE (an uneven wind, soft swells on the clock), PULSE (on each gust every fourth
 *                   root is blown into the one beside it, another four each time: a pattern four gusts long),
 *                   WAVE (a gust that travels along the roots, four of them to a gust), STORM (gusts at random)
 *   SEQ             the beetle: it flies from root to root in time and taps each pod it lands on, which sets the pod
 *                   swinging, so its neighbours may rattle after it. Held: KNOB 1 PACE (half, quarter, eighth,
 *                   sixteenth, thirty-second), KNOB 2 PATH (forward, back, to and fro, random), KNOB 3 FORCE,
 *                   KNOB 4 STRIDE (1, 3, 5, 7 roots a step)
 *   bottom row      pluck that root                  top row        a puff of wind from that place along the roof */
#define ROO_NJ 5                                 /* joints under the roof; the last one carries the pod */
#define ROO_TIP (ROO_NJ - 1)
#define ROO_Y0 19                                /* the roof */
#define ROO_X(s_) (15 + (s_) * 14)
enum { ROO_BARE, ROO_POD, ROO_NUT, ROO_BELL, ROO_GOURD, ROO_HUSK, ROO_NK };
static const char *const ROO_KINDS[ROO_NK] = {"NOTHING", "POD", "NUT", "BELL", "GOURD", "HUSK"};
static const uint8_t ROO_SND[ROO_NK] = {SK_NONE, SK_POD, SK_NUT, SK_BELL, SK_GOURD, SK_HUSK};
static const char *const ROO_WEATHER[4] = {"BREEZE", "PULSE", "WAVE", "STORM"};
static const char *const ROO_ALGOS[8] = {"YOUR OWN", "FIRST KIT", "SHAKERS", "WOOD", "CHIMES", "DRUM CIRCLE", "TWO HALVES", "ALL OF IT"};
static const uint8_t ROO_KDEF[4] = {4, 5, 4, 4}, ROO_SQDEF[4] = {3, 0, 5, 0};
static const uint16_t ROO_PER[8] = {0, 1536, 768, 384, 192, 96, 48, 24};   /* GUST: ticks between gusts (0: steady) */
static const uint16_t ROO_PACE[5] = {192, 96, 48, 24, 12};                 /* the beetle's step, in ticks */
static struct {
    int32_t x[RP_N][ROO_NJ], y[RP_N][ROO_NJ], px[RP_N][ROO_NJ], py[RP_N][ROO_NJ];   /* Q8 px; and where each was a step ago */
    int16_t g[RP_N];                             /* the gust on each root just now (sideways, Q8 px a step a step), dying away */
    uint8_t cool[RP_N];                          /* ms since pods s and s + 1 last knocked */
    int32_t w, wd, drift, lk;                    /* the wind, coming up and dropping; what the streaks show; how far they have gone; LENGTH as the roots have got to it (x8) */
    uint32_t rnd, bi;                            /* its own dice; the beetle's step of the clock */
    int32_t bt, puff_x, puff_t;                  /* ms since the beetle tapped; a puff from the top row */
    uint16_t side;                               /* which way each root was last plucked */
    uint8_t wpos, bat, bon, inited;              /* WAVE: the root the front is at; the beetle's root; it has landed once */
} roo;
static uint32_t roo_rnd(void) { roo.rnd ^= roo.rnd << 13; roo.rnd ^= roo.rnd >> 17; roo.rnd ^= roo.rnd << 5; return roo.rnd; }
static col_t roo_col(int k) { return k == ROO_POD ? th->caps[2] : k == ROO_NUT ? th->caps[4] : k == ROO_BELL ? th->caps[3] : k == ROO_GOURD ? th->caps[1] : k == ROO_HUSK ? th->caps[0] : th->mute; }
static int32_t roo_len(int s)                    /* a root's length, px */
{
    static const int8_t PROF[RP_N] = {0, 2, 4, 3, 1, -2, -4, -3, 0, 3, 4, 2, -1, -3, -4, -2};
    return 40 + roo.lk * 85 / 1024 + PROF[s];
}
static int32_t roo_mass(int s) { const rpart_t *p = &ri[RI_ROOTS].p[s]; return p->what ? 4 + (p->size & 7) : 3; }
static int32_t roo_rad(int s) { const rpart_t *p = &ri[RI_ROOTS].p[s]; return p->what ? (6 + (p->size & 7)) * 128 : 0; }   /* Q8 px */
static void roo_rest(void)                       /* every root hanging straight and still */
{
    int s, j;
    for (s = 0; s < RP_N; s++) {
        int32_t seg = roo_len(s) * 256 / ROO_NJ;
        for (j = 0; j < ROO_NJ; j++) { roo.x[s][j] = roo.px[s][j] = Q8(ROO_X(s)); roo.y[s][j] = roo.py[s][j] = Q8(ROO_Y0) + seg * (j + 1); }
        roo.g[s] = 0; roo.cool[s] = 250;
    }
    roo.w = 0; roo.wd = 0;
}
static void roo_init(void)
{
    if (roo.inited) return;                      /* (a new layout: the roots hang on as they were) */
    roo.rnd = 0x9E3779B9u; roo.lk = ri_k(RI_ROOTS, 2) * 8; roo.bon = 0; roo.bt = 9999; roo.puff_t = 9999;
    roo_rest();
    roo.inited = 1;
}
static void roo_layout(int n)
{
    rinst_t *r = &ri[RI_ROOTS];
    rpart_t *p = r->p;
    int s, any = 0;
    /* The room as new (ri_defaults has wiped the player's own layout and not yet filled it): the beetle is awake, so
     * that ROOTS plays a pattern from the start. The framework has no default for SEQ; this is the one place that
     * can tell. A saved room puts its own SEQ back afterwards. */
    for (s = 0; s < (int)sizeof r->mine; s++) any |= ((const uint8_t *)r->mine)[s];
    if (!any && n == 1) r->seq = 1;
    for (s = 0; s < RP_N; s++) { p[s].what = ROO_BARE; p[s].size = 3; p[s].note = (uint8_t)(s % 5); p[s].qual = 3; }
#define RP(s_, w_, z_, n_, q_) (p[s_].what = (w_), p[s_].size = (z_), p[s_].note = (n_), p[s_].qual = (q_))
    switch (n) {
    case 1:                                      /* a kit: with the beetle at sixteenths, one bar of it */
        RP(0, ROO_GOURD, 6, 0, 3); RP(2, ROO_POD, 2, 0, 3); RP(3, ROO_POD, 1, 0, 6); RP(4, ROO_HUSK, 4, 2, 3); RP(6, ROO_POD, 2, 0, 3); RP(7, ROO_NUT, 3, 4, 4);
        RP(8, ROO_GOURD, 5, 0, 2); RP(9, ROO_GOURD, 3, 3, 4); RP(10, ROO_POD, 2, 0, 4); RP(12, ROO_HUSK, 4, 2, 4); RP(13, ROO_POD, 1, 0, 6); RP(14, ROO_NUT, 2, 6, 4); RP(15, ROO_BELL, 2, 4, 2);
        break;
    case 2: for (s = 0; s < RP_N; s++) if (s % 4 != 1) RP(s, ROO_POD, (s * 3) % 6, 0, (s * 5) % 8); break;
    case 3: for (s = 0; s < RP_N; s++) if (s % 3 != 2) RP(s, ROO_NUT, 1 + (s * 2) % 5, (s * 3) % 10, 2 + s % 5); break;
    case 4: for (s = 0; s < RP_N; s++) { if (s & 1) RP(s, ROO_POD, 1, 0, 6); else RP(s, ROO_BELL, 1 + (s / 2) % 4, (s / 2 * 2) % 9, (s / 2) % 6); } break;
    case 5: for (s = 0; s < RP_N; s++) if (s % 4 != 3) RP(s, s % 4 == 1 ? ROO_HUSK : ROO_GOURD, 3 + (s * 3) % 5, (s / 4) * 2 % 6, 2 + s % 4); break;
    case 6: for (s = 0; s < 8; s++) { RP(s, s % 3 == 1 ? ROO_HUSK : ROO_GOURD, 4 + s % 3, s % 4, 3); if (s != 3) RP(s + 8, s & 1 ? ROO_BELL : ROO_NUT, 2 + s % 3, 2 + s, 3 + s % 3); } break;
    default: for (s = 0; s < RP_N; s++) RP(s, 1 + s % 5, 1 + (s * 3) % 6, (s * 2) % 10, (s * 5) % 8); break;
    }
#undef RP
}
static void roo_gust(int s, int32_t v) { roo.g[s] = (int16_t)iclamp(roo.g[s] + v, -180, 180); }
static void roo_shove(int s, int32_t vx, int32_t vy)   /* the pod of root s thrown: Q8 px a step, for a pod of middling weight */
{
    int32_t m = imax(roo_mass(s), 4);
    roo.px[s][ROO_TIP] -= vx * 8 / m; roo.py[s][ROO_TIP] -= vy * 8 / m;
}
static int roo_bpos(uint32_t idx)                /* the root the beetle is on at step idx of the clock */
{
    const rinst_t *r = &ri[RI_ROOTS];
    uint32_t st = 1u + 2u * (r->sq[3] & 3u), k;
    switch (r->sq[1] & 3u) {
    case 0: return (int)((idx * st) & 15u);
    case 1: return (int)((0u - idx * st) & 15u);
    case 2: k = idx % 30u; return (int)(((k < 16u ? k : 30u - k) * st) & 15u);
    default: return (int)(ihash(idx * 2654435761u + 17u) & 15u);
    }
}
static void roo_step(void)                       /* 10 ms */
{
    static const uint8_t STEADY[4] = {4, 2, 2, 4}, TURB[4] = {3, 1, 1, 4};
    rinst_t *r = &ri[RI_ROOTS];
    int s, j, it, still = rm.edit && rm.focus == RI_ROOTS && !r->run, blow = r->run && !still, pre = r->preset & 3;
    int32_t lean = iclamp((int32_t)r->sel - 4, -4, 4), per = ROO_PER[iclamp((ri_k(RI_ROOTS, 1) + 8) / 16, 0, 7)], k3 = ri_k(RI_ROOTS, 3);
    int32_t damp = still ? 170 : 3 + k3 * k3 * 90 / 16384 + (r->run ? 0 : 26), sum = 0;
    uint32_t tick = rm.tq8 >> 8, idx = 0;
    int32_t ph1 = (int32_t)((tick % 192u) * 65536u / 192u), ph2 = (int32_t)((tick % 128u) * 512u);
    roo.w += iclamp((blow ? ri_k(RI_ROOTS, 0) : 0) - roo.w, -2, 2);   /* the wind comes up and drops, it is not switched */
    if (still) roo.w = 0;
    roo.lk += iclamp(ri_k(RI_ROOTS, 2) * 8 - roo.lk, -12, 12);   /* the roots grow and shrink, they do not jump */
    if (roo.bt < 9000) roo.bt += 10;
    if (roo.puff_t < 9000) roo.puff_t += 10;
    /* the gusts, tied to the clock */
    if (blow && roo.w > 0) {
        int32_t A = roo.w, dir;
        if (pre == 2) {                          /* WAVE: the front goes along the roots, four of them to a gust (steady: a sixteenth each) */
            uint32_t stp = per ? (uint32_t)per / 4u : 24u, pos = (tick / stp) & 15u;
            dir = lean > 0 ? 1 : lean < 0 ? -1 : ((tick / (stp * 16u)) & 1u ? -1 : 1);
            while (roo.wpos != pos) { roo.wpos = (uint8_t)((roo.wpos + 1u) & 15u); roo_gust(dir > 0 ? roo.wpos : 15 - roo.wpos, dir * A * 3 / 4); }
        } else if ((per && rm_cross((uint32_t)per, 0, &idx)) || (pre == 3 && !per && roo_rnd() % 70u == 0u)) {
            if (!per) idx = roo_rnd();
            dir = lean > 0 ? 1 : lean < 0 ? -1 : (idx & 1u ? -1 : 1);
            for (s = 0; s < RP_N; s++) {
                uint32_t h = roo_rnd();
                if (pre == 0) roo_gust(s, dir * A * (int32_t)(30u + h % 60u) / 140);        /* a soft swell, uneven */
                else if (pre == 1) { if ((((uint32_t)s + idx) & 3u) == 0u) roo_gust(s, dir * A * 3 / 4); }   /* every fourth root, another four each gust: each runs into the one beside it */
                else if ((h & 3u) == 0u) roo_gust(s, ((h >> 2 & 3u) ? dir : -dir) * A * (int32_t)(30u + (h >> 8) % 60u) / 100);
            }
        }
        if (pre == 3 && roo_rnd() % 60u == 0u) { uint32_t h = roo_rnd(); roo_gust((int)(h & 15u), (h & 16u ? 1 : -1) * A * (int32_t)(40u + (h >> 8) % 50u) / 100); }
    }
    /* every joint: where it was going, less what the root's stiffness takes; its weight; the wind */
    for (s = 0; s < RP_N; s++) {
        const rpart_t *p = &r->p[s];
        int32_t m = roo_mass(s), x0 = Q8(ROO_X(s)), aw;
        aw = roo.w * lean * STEADY[pre] * 6 / 512
           + (((isin(ph1 - s * 9000) + isin(ph2 + s * 5000) / 2) * roo.w) / 16384) * TURB[pre] * 12 / 512
           + roo.g[s];
        if (still) aw = 0;
        sum += aw;
        aw = aw * 8 / m;                         /* a light pod is blown further */
        roo.g[s] = (int16_t)(roo.g[s] * 7 / 8);
        for (j = 0; j < ROO_NJ; j++) {
            int tip = j == ROO_TIP;
            int32_t vx = roo.x[s][j] - roo.px[s][j], vy = roo.y[s][j] - roo.py[s][j], fx;
            vx = iclamp(vx - vx * damp / 1024, -1536, 1536); vy = iclamp(vy - vy * damp / 1024, -1536, 1536);
            fx = (tip ? aw : aw * (j + 1) / (2 * ROO_NJ)) - (roo.x[s][j] - x0) * k3 / 16384;   /* a stiff root springs back to where it hangs */
            roo.px[s][j] = roo.x[s][j]; roo.py[s][j] = roo.y[s][j];
            roo.x[s][j] += vx + fx; roo.y[s][j] += vy + (tip && p->what ? 72 - (p->size & 7) * 4 : 56);   /* (a heavy pod swings slower) */
        }
    }
    roo.wd = sum / RP_N; roo.drift = (roo.drift + roo.wd) % (280 * 64);
    /* the roots keep their length: each link pulled back to it, the pod giving way least */
    for (it = 0; it < 3; it++)
        for (s = 0; s < RP_N; s++) {
            int32_t seg = roo_len(s) * 256 / ROO_NJ, wb_pod = 512 / (2 + roo_mass(s));
            for (j = 0; j < ROO_NJ; j++) {
                int32_t ax = j ? roo.x[s][j - 1] : Q8(ROO_X(s)), ay = j ? roo.y[s][j - 1] : Q8(ROO_Y0);
                int32_t dx = roo.x[s][j] - ax, dy = roo.y[s][j] - ay, d, e, cx, cy, wb;
                d = (int32_t)isqrt((uint32_t)((dx >> 2) * (dx >> 2) + (dy >> 2) * (dy >> 2))) << 2;
                if (d < 16) continue;
                e = d - seg; cx = dx * 256 / d * e / 256; cy = dy * 256 / d * e / 256;
                wb = !j ? 256 : j == ROO_TIP && r->p[s].what ? wb_pod : 128;
                roo.x[s][j] -= cx * wb / 256; roo.y[s][j] -= cy * wb / 256;
                if (j) { roo.x[s][j - 1] += cx * (256 - wb) / 256; roo.y[s][j - 1] += cy * (256 - wb) / 256; }
            }
        }
    for (s = 0; s < RP_N; s++)
        for (j = 0; j < ROO_NJ; j++) { roo.x[s][j] = iclamp(roo.x[s][j], Q8(3), Q8(236)); roo.y[s][j] = iclamp(roo.y[s][j], Q8(ROO_Y0), Q8(186)); }
    /* neighbouring pods that meet: they knock, and both sound */
    for (s = 0; s < RP_N - 1; s++) {
        int32_t *xa = &roo.x[s][ROO_TIP], *ya = &roo.y[s][ROO_TIP], *xb = &roo.x[s + 1][ROO_TIP], *yb = &roo.y[s + 1][ROO_TIP];
        int32_t *pxa = &roo.px[s][ROO_TIP], *pya = &roo.py[s][ROO_TIP], *pxb = &roo.px[s + 1][ROO_TIP], *pyb = &roo.py[s + 1][ROO_TIP];
        int32_t dx = *xb - *xa, dy = *yb - *ya, rs = roo_rad(s) + roo_rad(s + 1), ma = roo_mass(s), mb = roo_mass(s + 1), d, nx, ny, rv, e, ea, eb;
        if (roo.cool[s] < 250) roo.cool[s] += 10;
        if (!r->p[s].what || !r->p[s + 1].what || iabs(dx) >= rs || iabs(dy) >= rs) continue;
        d = (int32_t)isqrt((uint32_t)(dx * dx + dy * dy));
        if (d >= rs) continue;
        if (d < 16) { dx = 256; dy = 0; d = 256; }   /* one on top of the other: apart sideways */
        nx = dx * 256 / d; ny = dy * 256 / d;
        rv = (((*xb - *pxb) - (*xa - *pxa)) * nx + ((*yb - *pyb) - (*ya - *pya)) * ny) / 256;   /* how fast they close (under 0) */
        e = rs - d; ea = e * mb / (ma + mb); eb = e - ea;
        *xa -= nx * ea / 256; *ya -= ny * ea / 256; *pxa -= nx * ea / 256; *pya -= ny * ea / 256;
        *xb += nx * eb / 256; *yb += ny * eb / 256; *pxb += nx * eb / 256; *pyb += ny * eb / 256;
        if (rv < 0) {
            int32_t jn = -rv * 371 / 256, ja = jn * mb / (ma + mb), jb = jn - ja;   /* they spring apart, the light one more */
            *pxa += nx * ja / 256; *pya += ny * ja / 256; *pxb -= nx * jb / 256; *pyb -= ny * jb / 256;
            if (-rv > 44 && roo.cool[s] >= 70 && !still) {
                int32_t vel = 26 + -rv * (ma + mb) / 36;
                ri_strike(RI_ROOTS, s, iclamp(vel * 2 * mb / (ma + mb), 30, 255));       /* (struck by a heavy one: louder) */
                ri_strike(RI_ROOTS, s + 1, iclamp(vel * 2 * ma / (ma + mb), 30, 255));
                roo.cool[s] = 0;
            }
        }
    }
    /* the beetle: on each of its steps of the clock it lands on the next root and taps what hangs there */
    if (r->seq && blow && rm_cross(ROO_PACE[imin(r->sq[0], 4)], 0, &idx)) {
        int at = roo_bpos(idx), from = roo.bon ? roo.bat : at;
        roo.bi = idx; roo.bat = (uint8_t)at; roo.bon = 1; roo.bt = 0;
        if (r->p[at].what) ri_strike(RI_ROOTS, at, 60 + r->sq[2] * 22);
        roo_shove(at, (at < from ? -1 : 1) * (55 + r->sq[2] * 28), 60 + r->sq[2] * 12);
    }
    if (!r->seq) roo.bon = 0;
}
static void roo_key(int i, int vel)              /* a pluck: the pod pulled aside and let go */
{
    i = iclamp(i, 0, RP_N - 1);
    if (ri[RI_ROOTS].p[i].what) ri_strike(RI_ROOTS, i, vel);
    else ri[RI_ROOTS].fl[i] = 160;
    roo.side ^= (uint16_t)(1u << i);
    roo_shove(i, (roo.side >> i & 1u ? 1 : -1) * vel * 2, 0);
}
static void roo_top(int i)                       /* a puff from that place along the roof: the roots near it blown apart */
{
    int32_t x0 = 15 + iclamp(i, 0, 10) * 21, s;
    for (s = 0; s < RP_N; s++) {
        int32_t dx = ROO_X(s) - x0, k = 56 - iabs(dx);
        if (k > 0) roo_gust((int)s, (dx > 0 || (!dx && (s & 1)) ? 1 : -1) * k * 3 / 2);
    }
    roo.puff_x = x0; roo.puff_t = 0;
    sfx(96, 0, 50, W_NOISE, 500, 0, 40);
}
static void roo_play(int on) { if (!on) sfx_ex(50, 0, 300, W_SAW, 1200, 2, 60, 0, 0, 65200); else snd_ui(72); }

/* what hangs on a root, at (x, y): r its girth in px; hot: its flash */
static void roo_pod(int32_t x, int32_t y, int32_t r, int kind, int qual, int32_t hot)
{
    col_t c = hot > 150 ? th->text : roo_col(kind);
    int32_t k, xy[12];
    switch (kind) {
    case ROO_POD:                                /* a long seed pod, its seam down the middle */
        gellf(x, y + 1, r * 3 / 4 + 1, r + 2, c, 0);
        gline(x, y - r + 1, x, y + r, th->bg); gline(x, y + r + 3, x, y + r + 4, c);
        if (qual >= 4) { gp(x - 2, y - 1, th->bg); gp(x + 2, y + 2, th->bg); }
        break;
    case ROO_NUT:                                /* round, under a cap */
        gdisc(x, y + 1, r, c);
        gellf(x, y - 1, r + 1, r * 2 / 3 + 1, th->stem, 1); ghl(x - r - 1, x + r + 2, y - 1, th->bg);
        gp(x, y + r + 2, c);
        break;
    case ROO_BELL:                               /* flared, a clapper under it */
        xy[0] = x - 1; xy[1] = y - r; xy[2] = x + 1; xy[3] = y - r; xy[4] = x + r - 1; xy[5] = y + r - 2;
        xy[6] = x + r + 1; xy[7] = y + r; xy[8] = x - r - 1; xy[9] = y + r; xy[10] = x - r + 1; xy[11] = y + r - 2;
        galpha(90 + hot * 160 / 256); gpoly(xy, 6, c); galpha(256); gpolyline(xy, 6, 1, c);
        gdisc(x, y + r + 2, 1, th->text);
        break;
    case ROO_GOURD:                              /* a neck over a belly */
        gdisc(x, y - r * 2 / 3, imax(1, r / 2), c); gdisc(x, y + 2, r + 1, c);
        galpha(120); garc(x, y + 2, r - 1, r - 1, 36000, 52000, th->bg, 0); galpha(256);
        break;
    default:                                     /* a husk: burst open, all points */
        for (k = 0; k < 8; k++) gline(x, y, x + (((r + 2) * icos(k * 8192 + 2000)) >> 14), y + (((r + 2) * isin(k * 8192 + 2000)) >> 14), c);
        gdisc(x, y, imax(1, r - 2), c); gdisc(x, y, imax(0, r - 4), th->bg);
        break;
    }
    if (hot > 20) { galpha(hot); gcirc(x, y, r + 3 + (255 - hot) / 36, th->text); galpha(256); }
}
static void roo_beetle(int32_t x, int32_t y, int fly, int dir)
{
    int32_t f = (int32_t)(rm.t / 40u) & 1;
    gellf(x, y, 4, 3, th->pad, 0); gdisc(x + dir * 4, y, 1, th->pad);
    gline(x, y - 2, x, y + 3, th->bg);
    gline(x + dir * 5, y - 1, x + dir * 7, y - 3, th->pad);
    if (fly) { gline(x - 1, y - 3, x - 5, y - 6 + f * 3, th->text); gline(x + 1, y - 3, x + 5, y - 6 + f * 3, th->text); }
    else { gline(x - 3, y - 2, x - 5, y - 4, th->pad); gline(x + 3, y - 2, x + 5, y - 4, th->pad); gline(x - 4, y + 1, x - 6, y + 2, th->pad); gline(x + 4, y + 1, x + 6, y + 2, th->pad); }
}
static void roo_draw(int edit)
{
    static const int8_t GN[8] = {2, -1, -2, 1, 0, 2, -2, 1};   /* a root is not straight */
    static const char *const RATE[8] = {"STEADY WIND", "A GUST EVERY 4 BARS", "A GUST EVERY 2 BARS", "A GUST EVERY BAR", "A GUST EVERY HALF BAR",
                                        "A GUST EVERY BEAT", "A GUST EVERY HALF BEAT", "A GUST EVERY QUARTER BEAT"};
    const rinst_t *r = &ri[RI_ROOTS];
    int s, j, k;
    int32_t wd = roo.wd, sg = wd < 0 ? -1 : 1, x, y;
    /* the roof */
    for (k = 0; k < 20; k++) gline(k * 12, 15 + (int32_t)(ihash((uint32_t)k * 7u + 3u) % 3u), k * 12 + 12, 15 + (int32_t)(ihash((uint32_t)k * 7u + 10u) % 3u), th->mute);
    /* the wind: streaks that go with it, and a mark where a gust is on a root */
    if (iabs(wd) > 2) {
        int32_t len = imin(30, 8 + iabs(wd)), a = imin(210, 70 + iabs(wd) * 8);
        galpha(a);
        for (k = 0; k < 6; k++) {
            x = ((roo.drift >> 6) + k * 97) % 280; if (x < 0) x += 280;
            x -= 20; y = 30 + k * 25 + (k * 37) % 11;
            gline(x, y, x + sg * len, y, th->mute); gline(x + sg * 3, y + 2, x + sg * (3 + len / 2), y + 2, th->mute);
        }
        galpha(256);
    }
    for (s = 0; s < RP_N; s++) {
        int32_t g = roo.g[s], gs = g < 0 ? -1 : 1;
        if (iabs(g) < 14) continue;
        x = roo.x[s][2] >> 8; y = roo.y[s][2] >> 8;
        galpha(imin(256, iabs(g) * 3));
        gline(x - gs * 4, y - 3, x - gs * (6 + iabs(g) / 12), y - 3, th->text); gline(x - gs * 5, y + 1, x - gs * (9 + iabs(g) / 9), y + 1, th->text);
        galpha(256);
    }
    if (roo.puff_t < 300) { galpha(256 - roo.puff_t * 256 / 300); garc(roo.puff_x, ROO_Y0 - 2, 6 + roo.puff_t / 6, 4 + roo.puff_t / 8, 0, 32768, th->text, 0); galpha(256); }
    /* the roots, and what hangs on them */
    for (s = 0; s < RP_N; s++) {
        const rpart_t *p = &r->p[s];
        int32_t hot = r->fl[s], qx = ROO_X(s), qy = ROO_Y0, nx, ny;
        gdisc(qx, ROO_Y0 - 1, 2, th->stem);
        for (j = 0; j < ROO_NJ; j++) {
            nx = (roo.x[s][j] >> 8) + (j < ROO_TIP ? GN[(j + s * 3) & 7] : 0); ny = roo.y[s][j] >> 8;
            if (j < 3) gline2(qx, qy, nx, ny, th->stem); else gline(qx, qy, nx, ny, th->stem);
            if (hot > 40) { galpha(hot); gline(qx, qy, nx, ny, th->text); galpha(256); }
            if (j == 1 || j == 3) gline(nx, ny, nx + (((s + j) & 1) ? 3 : -3), ny + 3, th->stem);   /* a rootlet */
            qx = nx; qy = ny;
        }
        if (p->what) roo_pod(qx, qy + 1, roo_rad(s) >> 8, p->what, p->qual, hot);
        else gdisc(qx, qy, hot > 60 ? 1 : 0, th->stem);
        if (edit) {
            char n[3];
            int on = r->hand >> s & 1;
            n[0] = s >= 9 ? '1' : (char)('1' + s); n[1] = s >= 9 ? (char)('0' + s - 9) : 0; n[2] = 0;
            gtextc(ROO_X(s), 190, n, on ? th->text : th->mute, 1);
            if (on) { gbox(ROO_X(s) - 7, 187, 14, 11, th->text); gcirc(qx, qy + 1, (roo_rad(s) >> 8) + 5, th->text); gline(ROO_X(s), 22, ROO_X(s), 26, th->text); }
        }
    }
    /* the beetle: it sits under what it has tapped, then flies to the next */
    if (r->seq && roo.bon) {
        int32_t per = ROO_PACE[imin(r->sq[0], 4)], f = r->run ? (int32_t)((rm.tq8 >> 8) % (uint32_t)per) * 256 / per : 0, h = f < 90 ? 0 : (f - 90) * 256 / 166;
        int a = roo.bat, b = roo_bpos(roo.bi + 1u);
        int32_t xa = roo.x[a][ROO_TIP] >> 8, ya = (roo.y[a][ROO_TIP] >> 8) + (roo_rad(a) >> 8) + 6, xb = roo.x[b][ROO_TIP] >> 8, yb = (roo.y[b][ROO_TIP] >> 8) + (roo_rad(b) >> 8) + 6;
        h = ease8(iclamp(h, 0, 256));
        x = xa + (xb - xa) * h / 256; y = ya + (yb - ya) * h / 256 + ((isin(h * 128) * (6 + iabs(xb - xa) / 8)) >> 14);
        if (roo.bt < 120) { galpha(256 - roo.bt * 2); gcirc(xa, ya - 3, 5 + roo.bt / 12, th->pad); galpha(256); }
        roo_beetle(x, imin(y, 194), h > 0 && h < 256, xb < xa ? -1 : 1);
    }
    if (edit) {
        if ((r->hand & (r->hand - 1)) == 0) {    /* one root in hand: what hangs on it */
            char nb[6];
            s = 0;
            while (s < RP_N - 1 && !(r->hand >> s & 1u)) s++;
            gtextc(120, 160, ROO_KINDS[r->p[s].what % ROO_NK], roo_col(r->p[s].what), 2);
            rm_note_name(nb, rm_note(RI_ROOTS, r->p[s].note));
            gtextc(120, 177, nb, th->text, 1);
        }
    } else {
        int32_t lean = iclamp((int32_t)r->sel - 4, -4, 4), ax = 214, l = 4 + iabs(lean) * 4;
        gtext(4, 190, !r->run || !r->k[0] ? "NO WIND" : RATE[iclamp((ri_k(RI_ROOTS, 1) + 8) / 16, 0, 7)], th->mute, 1);
        if (lean >= 0) { gline(ax, 193, ax + l, 193, th->mute); gline(ax + l, 193, ax + l - 3, 190, th->mute); gline(ax + l, 193, ax + l - 3, 196, th->mute); }   /* which way it blows, and how much to one side */
        if (lean <= 0) { gline(ax, 193, ax - l, 193, th->mute); gline(ax - l, 193, ax - l + 3, 190, th->mute); gline(ax - l, 193, ax - l + 3, 196, th->mute); }
    }
}

/* ---- its lesson: the roots themselves, played by the lesson */
static void rol_reset(void)
{
    rinst_t *r = &ri[RI_ROOTS];
    int i;
    ri_load_algo(RI_ROOTS, 1);
    for (i = 0; i < 4; i++) { r->k[i] = ROO_KDEF[i]; r->lfo[i] = 0; r->env[i] = 4; r->sq[i] = ROO_SQDEF[i]; }
    r->k[0] = 0; r->seq = 0; r->arp = 0; r->run = 1; r->sel = 6; r->preset = 0; r->oct = 0;
    rm.edit = 0; rm.menu = 0;
    roo_rest();
}
static void rol_say(const char *s) { rt.label = s; rt.label_t = 1000; }
static void rol_1(int32_t t, int32_t dt)
{
    static const uint8_t K[5] = {2, 7, 12, 4, 15};
    int i;
    if (RT_FIRST) rol_reset();
    rt.icon = RTI_LOW;
    for (i = 0; i < 5; i++) if (RT_AT(900 + i * 850)) roo_key(K[i], 200);
}
static void rol_2(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_ROOTS];
    int i;
    if (RT_FIRST) {
        for (i = 0; i < RP_N; i++) r->p[i].what = ROO_BARE;
        for (i = 0; i < 5; i++) { rpart_t *p = &r->p[1 + i * 3]; p->what = (uint8_t)(1 + i); p->size = 4; p->note = (uint8_t)(i == 3 ? 0 : i * 2); p->qual = 3; }
        r->k[2] = 1; roo.lk = ri_k(RI_ROOTS, 2) * 8; roo_rest();
    }
    rt.icon = RTI_LOW;
    for (i = 0; i < 5; i++) if (RT_AT(700 + i * 1100)) { roo_key(1 + i * 3, 200); rol_say(ROO_KINDS[1 + i]); }
}
static void rol_3(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_ROOTS];
    if (RT_FIRST) { ri_load_algo(RI_ROOTS, 7); r->k[0] = 3; r->k[1] = 0; r->k[2] = 6; }
    rt.icon = t < 3600 ? RTI_K1 : RTI_K2;
    if (RT_AT(1200)) r->k[0] = 5;
    if (RT_AT(2200)) r->k[0] = 7;
    if (RT_AT(3800)) { r->k[1] = 5; r->preset = 1; }
}
static void rol_4(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_ROOTS];
    if (RT_FIRST) { r->k[0] = 6; r->k[1] = 5; r->preset = 1; }
    rt.icon = t < 1700 ? RTI_K3 : t < 3200 ? RTI_K4 : t < 4700 ? RTI_SELECT : RTI_PRESETS;
    if (RT_AT(300)) r->k[2] = 2;
    if (RT_AT(1000)) r->k[2] = 8;
    if (RT_AT(1900)) r->k[3] = 7;
    if (RT_AT(2600)) r->k[3] = 1;
    if (RT_AT(3400)) r->sel = 1;
    if (RT_AT(4100)) r->sel = 7;
    if (RT_AT(4900)) { r->preset = 2; r->k[1] = 4; rol_say(ROO_WEATHER[2]); }
}
static void rol_5(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_ROOTS];
    if (RT_FIRST) { ri_load_algo(RI_ROOTS, 1); r->k[0] = 1; r->k[1] = 5; r->k[2] = 4; r->k[3] = 4; r->sel = 6; r->preset = 0; r->seq = 1; }
    rt.icon = t < 3600 ? RTI_BTN + BTN_SEQ : RTI_K4;
    if (RT_AT(4000)) r->sq[3] = 1;
}
static void rol_6(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_ROOTS];
    if (RT_FIRST) { r->seq = 0; r->sq[3] = 0; rm.edit = 1; r->run = 0; r->hand = 1u << 2; }
    rt.icon = t < 1400 ? RTI_BTN + BTN_EDIT : t < 2200 ? RTI_LOW : t < 3200 ? RTI_K1 : t < 4700 ? RTI_K2 : t < 6200 ? RTI_K3 : RTI_K4;
    if (RT_AT(1500)) { r->hand = 1u << 11; ri[RI_ROOTS].fl[11] = 200; }
    if (RT_AT(2400)) { r->p[11].what = ROO_BELL; r->p[11].size = 1; r->p[11].note = 2; r->p[11].qual = 3; ri_strike(RI_ROOTS, 11, 150); }
    if (RT_AT(3400) || RT_AT(3800) || RT_AT(4200)) { r->p[11].size += 2; ri_strike(RI_ROOTS, 11, 150); }
    if (RT_AT(4900) || RT_AT(5300) || RT_AT(5700)) { r->p[11].note += 2; ri_strike(RI_ROOTS, 11, 150); }
    if (RT_AT(6400) || RT_AT(6900)) { r->p[11].qual += 2; ri_strike(RI_ROOTS, 11, 150); }
}
static void rol_7(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_ROOTS];
    if (RT_FIRST) { rm.edit = 0; r->run = 1; r->k[0] = 0; r->hand = 1; }
    rt.icon = t < 2600 ? RTI_TOP : t < 3700 ? RTI_BTN + BTN_REC : t < 4800 ? RTI_BTN + BTN_SAVE : RTI_BTN + BTN_HOME;
    if (RT_AT(600)) roo_top(2);
    if (RT_AT(1500)) roo_top(8);
}
static const rt_step_t ROO_LESSON[7] = {
    {5500, {"SIXTEEN ROOTS HANG FROM THE ROOF, MOST WITH A POD AT THE TIP. A POD SOUNDS ONLY WHEN SOMETHING STRIKES IT. THE BOTTOM ROW PLUCKS THE ROOTS.",
            "SECHZEHN WURZELN HÄNGEN VON DER DECKE, DIE MEISTEN MIT EINER KAPSEL AN DER SPITZE. EINE KAPSEL KLINGT NUR, WENN ETWAS SIE TRIFFT. DIE UNTERE REIHE ZUPFT DIE WURZELN.",
            "SEKSTEN RØDDER HÆNGER FRA LOFTET, DE FLESTE MED EN KAPSEL I SPIDSEN. EN KAPSEL LYDER KUN, NÅR NOGET RAMMER DEN. DEN NEDERSTE RÆKKE KNIPSER RØDDERNE.",
            "SEIZE RACINES PENDENT DU PLAFOND, LA PLUPART AVEC UNE COSSE AU BOUT. UNE COSSE NE SONNE QUE SI QUELQUE CHOSE LA FRAPPE. LA RANGÉE DU BAS PINCE LES RACINES.",
            "DIECISÉIS RAÍCES CUELGAN DEL TECHO, CASI TODAS CON UNA VAINA EN LA PUNTA. UNA VAINA SOLO SUENA CUANDO ALGO LA GOLPEA. LA FILA DE ABAJO PULSA LAS RAÍCES.",
            "TAVANDAN ON ALTI KÖK SARKAR, ÇOĞUNUN UCUNDA BİR KOZA VAR. KOZA ANCAK BİR ŞEY ONA ÇARPINCA SES VERİR. ALT SIRA KÖKLERİ ÇEKİP BIRAKIR.",
            "ZESTIEN WORTELS HANGEN AAN HET PLAFOND, DE MEESTE MET EEN PEUL AAN DE PUNT. EEN PEUL KLINKT ALLEEN ALS IETS HEM RAAKT. DE ONDERSTE RIJ TOKKELT DE WORTELS."}, rol_1},
    {6500, {"FIVE THINGS CAN HANG THERE. POD IS A DRY SHAKER. NUT IS A WOODBLOCK. BELL RINGS. GOURD IS A LOW DRUM. HUSK SNAPS LIKE A SNARE.",
            "FÜNF DINGE KÖNNEN DORT HÄNGEN. POD IST EINE TROCKENE RASSEL. NUT IST EIN HOLZBLOCK. BELL LÄUTET. GOURD IST EINE TIEFE TROMMEL. HUSK KNALLT WIE EINE SNARE.",
            "FEM TING KAN HÆNGE DER. POD ER EN TØR RASLE. NUT ER EN TRÆBLOK. BELL RINGER. GOURD ER EN DYB TROMME. HUSK SMÆLDER SOM EN LILLETROMME.",
            "CINQ CHOSES PEUVENT Y PENDRE. POD EST UN HOCHET SEC. NUT EST UN WOODBLOCK. BELL TINTE. GOURD EST UN TAMBOUR GRAVE. HUSK CLAQUE COMME UNE CAISSE CLAIRE.",
            "CINCO COSAS PUEDEN COLGAR AHÍ. POD ES UNA MARACA SECA. NUT ES UNA CAJA CHINA. BELL TAÑE. GOURD ES UN TAMBOR GRAVE. HUSK RESTALLA COMO UNA CAJA.",
            "ORADA BEŞ ŞEY ASILI OLABİLİR. POD KURU BİR MARAKAS. NUT BİR TAHTA BLOK. BELL ÇINLAR. GOURD KALIN BİR DAVUL. HUSK TRAMPET GİBİ ŞAKLAR.",
            "VIJF DINGEN KUNNEN DAAR HANGEN. POD IS EEN DROGE SCHUDDER. NUT IS EEN HOUTBLOK. BELL LUIDT. GOURD IS EEN LAGE TROM. HUSK KNALT ALS EEN SNAARTROM."}, rol_2},
    {6500, {"KNOB 1 IS THE WIND. PODS THAT KNOCK INTO EACH OTHER SOUND TOO: THE HARDER THE KNOCK, THE LOUDER. KNOB 2: HOW OFTEN THE WIND GUSTS, IN TIME WITH THE CLOCK.",
            "KNOB 1 IST DER WIND. KAPSELN, DIE ANEINANDERSTOSSEN, KLINGEN AUCH: JE HÄRTER DER STOSS, DESTO LAUTER. KNOB 2: WIE OFT EINE BÖE KOMMT, IM TAKT DER UHR.",
            "KNOB 1 ER VINDEN. KAPSLER, DER STØDER MOD HINANDEN, LYDER OGSÅ: JO HÅRDERE STØD, JO HØJERE. KNOB 2: HVOR TIT DER KOMMER ET VINDSTØD, I TAKT MED URET.",
            "KNOB 1 EST LE VENT. LES COSSES QUI SE COGNENT SONNENT AUSSI : PLUS LE CHOC EST DUR, PLUS LE SON EST FORT. KNOB 2 : LA FRÉQUENCE DES RAFALES, EN MESURE AVEC L'HORLOGE.",
            "KNOB 1 ES EL VIENTO. LAS VAINAS QUE CHOCAN ENTRE SÍ TAMBIÉN SUENAN: CUANTO MÁS FUERTE EL GOLPE, MÁS ALTO. KNOB 2: CADA CUÁNTO LLEGA UNA RÁFAGA, AL COMPÁS DEL RELOJ.",
            "KNOB 1 RÜZGAR. BİRBİRİNE ÇARPAN KOZALAR DA SES VERİR: ÇARPMA NE KADAR SERTSE SES O KADAR YÜKSEK. KNOB 2: RÜZGAR NE SIKLIKTA ESER, SAATLE AYNI TEMPODA.",
            "KNOB 1 IS DE WIND. PEULEN DIE TEGEN ELKAAR STOTEN KLINKEN OOK: HOE HARDER DE STOOT, HOE LUIDER. KNOB 2: HOE VAAK ER EEN WINDSTOOT KOMT, IN DE MAAT VAN DE KLOK."}, rol_3},
    {6000, {"KNOB 3: LONGER ROOTS SWING SLOWER AND REACH THEIR NEIGHBOURS. KNOB 4: STIFF ROOTS COME TO REST SOONER. SELECT: WHICH WAY THE WIND LEANS. PRESETS: THE WEATHER.",
            "KNOB 3: LÄNGERE WURZELN SCHWINGEN LANGSAMER UND ERREICHEN IHRE NACHBARN. KNOB 4: STEIFE WURZELN KOMMEN FRÜHER ZUR RUHE. SELECT: WOHIN DER WIND DRÜCKT. PRESETS: DAS WETTER.",
            "KNOB 3: LÆNGERE RØDDER SVINGER LANGSOMMERE OG NÅR DERES NABOER. KNOB 4: STIVE RØDDER FALDER HURTIGERE TIL RO. SELECT: HVILKEN VEJ VINDEN BLÆSER. PRESETS: VEJRET.",
            "KNOB 3 : LES RACINES LONGUES OSCILLENT PLUS LENTEMENT ET TOUCHENT LEURS VOISINES. KNOB 4 : LES RACINES RAIDES S'ARRÊTENT PLUS VITE. SELECT : DE QUEL CÔTÉ LE VENT POUSSE. PRESETS : LE TEMPS QU'IL FAIT.",
            "KNOB 3: LAS RAÍCES LARGAS SE MECEN MÁS DESPACIO Y ALCANZAN A SUS VECINAS. KNOB 4: LAS RAÍCES RÍGIDAS SE AQUIETAN ANTES. SELECT: HACIA DÓNDE EMPUJA EL VIENTO. PRESETS: EL TIEMPO QUE HACE.",
            "KNOB 3: UZUN KÖKLER DAHA YAVAŞ SALLANIR VE KOMŞULARINA ULAŞIR. KNOB 4: SERT KÖKLER DAHA ÇABUK DURUR. SELECT: RÜZGARIN YÖNÜ. PRESETS: HAVA DURUMU.",
            "KNOB 3: LANGERE WORTELS ZWAAIEN TRAGER EN RAKEN HUN BUREN. KNOB 4: STIJVE WORTELS KOMEN EERDER TOT RUST. SELECT: WELKE KANT DE WIND OP DUWT. PRESETS: HET WEER."}, rol_4},
    {7000, {"SEQ WAKES THE BEETLE. IT GOES FROM ROOT TO ROOT IN TIME AND TAPS EVERY POD IT LANDS ON. HOLD SEQ AND TURN KNOB 1 TO 4: ITS PACE, PATH, FORCE AND STRIDE.",
            "SEQ WECKT DEN KÄFER. ER FLIEGT IM TAKT VON WURZEL ZU WURZEL UND KLOPFT AUF JEDE KAPSEL, AUF DER ER LANDET. HALTE SEQ UND DREH KNOB 1 BIS 4: SEIN TEMPO, SEIN WEG, SEINE KRAFT, SEIN SCHRITT.",
            "SEQ VÆKKER BILLEN. DEN FLYVER I TAKT FRA ROD TIL ROD OG BANKER PÅ HVER KAPSEL, DEN LANDER PÅ. HOLD SEQ OG DREJ KNOB 1 TIL 4: DENS TEMPO, VEJ, KRAFT OG SKRIDT.",
            "SEQ RÉVEILLE LE SCARABÉE. IL VA DE RACINE EN RACINE EN MESURE ET TAPE CHAQUE COSSE OÙ IL SE POSE. TIENS SEQ ET TOURNE KNOB 1 À 4 : SON ALLURE, SON CHEMIN, SA FORCE, SON PAS.",
            "SEQ DESPIERTA AL ESCARABAJO. VA DE RAÍZ EN RAÍZ A COMPÁS Y GOLPEA CADA VAINA EN LA QUE SE POSA. MANTÉN SEQ Y GIRA KNOB 1 A 4: SU RITMO, SU CAMINO, SU FUERZA Y SU ZANCADA.",
            "SEQ BÖCEĞİ UYANDIRIR. TEMPOYLA KÖKTEN KÖKE UÇAR VE KONDUĞU HER KOZAYA VURUR. SEQ'İ BASILI TUT VE KNOB 1-4'Ü ÇEVİR: HIZI, YOLU, GÜCÜ VE ADIMI.",
            "SEQ WEKT DE KEVER. HIJ VLIEGT IN DE MAAT VAN WORTEL NAAR WORTEL EN TIKT OP ELKE PEUL WAAR HIJ LANDT. HOUD SEQ VAST EN DRAAI KNOB 1 TOT 4: ZIJN TEMPO, PAD, KRACHT EN STAP."}, rol_5},
    {8000, {"EDIT STILLS THE ROOTS. HOLD A KEY OF THE BOTTOM ROW: THAT ROOT IS IN HAND. KNOB 1: WHAT HANGS THERE. KNOB 2: ITS WEIGHT. KNOB 3: ITS NOTE. KNOB 4: ITS QUALITY.",
            "EDIT HÄLT DIE WURZELN AN. HALTE EINE TASTE DER UNTEREN REIHE: DIESE WURZEL IST IN DER HAND. KNOB 1: WAS DORT HÄNGT. KNOB 2: SEIN GEWICHT. KNOB 3: SEIN TON. KNOB 4: SEINE KLANGFARBE.",
            "EDIT STANDSER RØDDERNE. HOLD EN TAST I DEN NEDERSTE RÆKKE: DEN ROD ER I HÅNDEN. KNOB 1: HVAD DER HÆNGER DER. KNOB 2: DENS VÆGT. KNOB 3: DENS TONE. KNOB 4: DENS KLANG.",
            "EDIT IMMOBILISE LES RACINES. TIENS UNE TOUCHE DE LA RANGÉE DU BAS : CETTE RACINE EST EN MAIN. KNOB 1 : CE QUI Y PEND. KNOB 2 : SON POIDS. KNOB 3 : SA NOTE. KNOB 4 : SON TIMBRE.",
            "EDIT AQUIETA LAS RAÍCES. MANTÉN UNA TECLA DE LA FILA DE ABAJO: ESA RAÍZ ESTÁ EN LA MANO. KNOB 1: QUÉ CUELGA AHÍ. KNOB 2: SU PESO. KNOB 3: SU NOTA. KNOB 4: SU TIMBRE.",
            "EDIT KÖKLERİ DURDURUR. ALT SIRADAN BİR TUŞU BASILI TUT: O KÖK ELİNDE. KNOB 1: ORADA NE ASILI. KNOB 2: AĞIRLIĞI. KNOB 3: NOTASI. KNOB 4: TINISI.",
            "EDIT ZET DE WORTELS STIL. HOUD EEN TOETS VAN DE ONDERSTE RIJ VAST: DIE WORTEL HEB JE IN DE HAND. KNOB 1: WAT DAAR HANGT. KNOB 2: ZIJN GEWICHT. KNOB 3: ZIJN NOOT. KNOB 4: ZIJN KLANKKLEUR."}, rol_6},
    {6000, {"THE TOP ROW BLOWS A PUFF OF WIND FROM THAT PLACE ALONG THE ROOF. REC RECORDS WHAT SOUNDS ONTO THE LOOP. SAVE KEEPS THE ROOM. HOME GOES BACK TO IT. NOW YOU!",
            "DIE OBERE REIHE BLÄST EINEN WINDSTOSS VON DIESER STELLE AN DER DECKE. REC NIMMT AUF, WAS KLINGT. SAVE SICHERT DEN RAUM. HOME FÜHRT DORTHIN ZURÜCK. JETZT DU!",
            "DEN ØVERSTE RÆKKE PUSTER ET VINDSTØD FRA DET STED LANGS LOFTET. REC OPTAGER DET, DER LYDER, PÅ LOOPET. SAVE GEMMER RUMMET. HOME GÅR TILBAGE TIL DET. NU DIG!",
            "LA RANGÉE DU HAUT SOUFFLE UNE RAFALE DEPUIS CET ENDROIT DU PLAFOND. REC ENREGISTRE CE QUI SONNE SUR LA BOUCLE. SAVE GARDE LA SALLE. HOME Y RETOURNE. À TOI !",
            "LA FILA DE ARRIBA SOPLA UNA RÁFAGA DESDE ESE PUNTO DEL TECHO. REC GRABA LO QUE SUENA EN EL BUCLE. SAVE GUARDA LA SALA. HOME VUELVE A ELLA. ¡AHORA TÚ!",
            "ÜST SIRA TAVANIN O YERİNDEN BİR RÜZGAR ÜFLER. REC ÇALANI DÖNGÜYE KAYDEDER. SAVE ODAYI SAKLAR. HOME ODAYA DÖNER. ŞİMDİ SEN!",
            "DE BOVENSTE RIJ BLAAST EEN WINDSTOOT VANAF DIE PLEK LANGS HET PLAFOND. REC NEEMT OP WAT KLINKT, OP DE LOOP. SAVE BEWAART DE KAMER. HOME GAAT ERNAAR TERUG. NU JIJ!"}, rol_7},
};
static const rdesc_t ROOTS_DESC = {
    "ROOTS", {"WIND", "GUST", "LENGTH", "STIFF"}, {8, 7, 8, 8}, {4, 5, 4, 4}, {0, 0, 0, 0},
    "LEAN", 8, 6,
    "SEQ: THE BEETLE", {"PACE", "PATH", "FORCE", "STRIDE"}, {4, 3, 8, 3}, {3, 0, 5, 0},
    ROO_KINDS, ROO_NK, ROO_SND, ROO_WEATHER, 4, ROO_ALGOS, 8,
    roo_init, roo_step, roo_draw, roo_key, roo_top, roo_layout, roo_play, ROO_LESSON, 7,
};
