/* SPDX-License-Identifier: GPL-3.0-only */
/* MUSHROOM RACK (ac_room.h has the controls every instrument shares). A rack of 16 mushroom caps in a row, one to each
 * key of the bottom row. Spore pods bounce from cap to cap. The rack is silent: a sound comes only when a pod lands on
 * a cap, an acid bass note. The cap gives the pitch (a taller one is a higher note), the speed of the landing gives
 * the loudness and opens the filter. Each kind of cap is a different bounce:
 *
 *   (nothing)  a gap: the pod falls through to the net below, rests, and is thrown back up. Silent: a rest in the line
 *   ROUND      an ordinary bounce                        FLAT     a short low hop: quick notes, soft ones after it
 *   SPRING     a high bounce: slow, and loud after it    STICKY   the pod sticks a moment; the note slides from the last
 *   TWIN       sends the pod back the way it came
 *   SIZE: how wide and springy the cap is (a bigger one throws higher, so it takes longer). QUALITY: the squelch.
 *
 *   KNOB 1 SPRING   how high every cap throws: the pace of the whole line
 *   KNOB 2 TILT     which way the pods travel, and how far a bounce: in place, every other bounce, 1, 1 and 2, 2 caps
 *   KNOB 3 SIEVE    how far a landing is pulled onto the clock's sixteenths: the pod waits on the cap, squashed
 *   KNOB 4 PODS     how many                             SELECT ENDS   WRAP round, BOUNCE back, FALL off and drop in again
 *   PRESETS         the pods: SPORE (soft), ACORN (heavy: low bounces, hard accents), DROP (wet: every note slides),
 *                   TWINS (each landing echoes an octave up)
 *   SEQ             the reset: every so many sixteenths all pods are put back on their starting caps, so the line
 *                   repeats exactly: a riff. Held: KNOB 1 LENGTH, KNOB 2 START, KNOB 3 SPREAD, KNOB 4 ACCENT
 *   bottom row      drop a pod on that cap now           top row       the whole line up or down the scale
 *
 * The pods are real: a place and a speed each, under one gravity, stepped in clock ticks so that the line keeps its
 * shape at any tempo. A cap throws a pod just hard enough to come down on the cap TILT says. */
#define RAK_NP 4
#define RAK_X0 15                                /* the middle of cap 1 */
#define RAK_SP 14                                /* from cap to cap */
#define RAK_TOP 148                              /* the top of a cap whose NOTE is 0; each step of the scale is a pixel taller */
#define RAK_PLANK 170
#define RAK_NET 189
#define RAK_CEIL 36                              /* nothing is thrown higher than this */
#define RAK_G 5689                               /* gravity: px Q16 a tick a tick (a bounce of 4 sixteenths rises 100 px) */
enum { RAK_GAP, RAK_ROUND, RAK_FLAT, RAK_SPRINGY, RAK_STICKY, RAK_TWIN, RAK_NK };
enum { RAK_FLY, RAK_WAIT, RAK_STICK, RAK_NETTED, RAK_REST };
static const char *const RAK_KINDS[RAK_NK] = {"A GAP", "ROUND", "FLAT", "SPRING", "STICKY", "TWIN"};
static const uint8_t RAK_SND[RAK_NK] = {SK_CLICK, SK_ACID, SK_ACID, SK_ACID, SK_SLIDE, SK_ACID};
static const char *const RAK_PODS[4] = {"SPORE", "ACORN", "DROP", "TWINS"};
static const char *const RAK_ALGOS[8] = {"YOUR OWN", "FIRST LINE", "RUNNER", "PING PONG", "SLIDER", "HOLES", "HEAVY SPRINGS", "STAIRS"};
static const char *const RAK_ENDS[3] = {"WRAP", "BOUNCE", "FALL"};
static const uint8_t RAK_LEN[8] = {4, 8, 12, 16, 24, 32, 48, 64};   /* the reset's LENGTH, in sixteenths */
static const uint8_t RAK_KDEF[4] = {4, 6, 8, 1}, RAK_SQDEF[4] = {3, 0, 4, 3};
static struct {
    struct { int32_t x, y, vx, vy, t, hit, glow; int8_t cap, dir; uint8_t st, par; } pod[RAK_NP];   /* px Q16, px Q16 a tick; t: ticks Q8 left to wait */
    uint8_t sq[RP_N];                            /* how squashed each cap is, of 255 */
    struct { int32_t t; uint8_t deg, vel, size, qual; } echo[4];   /* TWINS: the notes still to come */
    uint8_t n, hits, next, inited;
    int8_t tr;                                   /* the top row: the line moved by so many steps of the scale */
} rak;

static int32_t rak_cx(int s) { return RAK_X0 + s * RAK_SP; }
static int32_t rak_top(int s) { return RAK_TOP - imin(ri[RI_RACK].p[s].note, 20); }
static int32_t rak_hw(int s) { return 4 + (ri[RI_RACK].p[s].size & 7) * 3 / 8; }   /* half a cap's width */
static int rak_count(void) { return iclamp((ri_k(RI_RACK, 3) + 8) / 16, 0, RAK_NP); }
static int rak_kind(int s) { return imin(ri[RI_RACK].p[s].what, RAK_NK - 1); }
static int32_t rak_deg(int s)                    /* the cap's note, moved by the top row; under the bottom it comes round an octave up */
{
    int32_t d = ri[RI_RACK].p[s].note + rak.tr, n = RSC[rm.scale % RSC_N][0];
    while (d < 0) d += n;
    return d;
}
/* how long a landing waits for the clock: nothing if it is on a sixteenth or just after one, else SIEVE's share of the way to the next */
static int32_t rak_wait(void)
{
    uint32_t p = rm.tq8 % (TICKS_STEP << 8);
    if (p <= (6u << 8)) return 0;
    return (int32_t)((TICKS_STEP << 8) - p) * ri_k(RI_RACK, 2) / 128;
}
/* pod i put at its start: seat 1 on its cap, to sound at once (the reset); seat 0 dropped in from above */
static void rak_seed(int i, int seat)
{
    const rinst_t *r = &ri[RI_RACK];
    int s = (r->sq[1] + i * r->sq[2]) % RP_N;
    rak.pod[i].cap = (int8_t)s; rak.pod[i].dir = 1; rak.pod[i].par = 0; rak.pod[i].glow = 0;
    rak.pod[i].x = rak_cx(s) << 16; rak.pod[i].vx = rak.pod[i].vy = 0; rak.pod[i].hit = 533 << 8;
    if (seat) { rak.pod[i].y = rak_top(s) << 16; rak.pod[i].st = RAK_WAIT; rak.pod[i].t = r->sq[2] ? 0 : i * (int32_t)(TICKS_STEP << 8); }   /* all on one cap: a sixteenth apart */
    else { rak.pod[i].y = (RAK_CEIL + 2) << 16; rak.pod[i].st = RAK_FLY; }
}
static void rak_init(void)
{
    int i;
    if (rak.inited) return;                      /* (a new layout or other pods: the pods in the air stay where they are) */
    for (i = 0; i < RAK_NP; i++) rak_seed(i, 0);
    for (i = 0; i < RP_N; i++) rak.sq[i] = 0;
    for (i = 0; i < 4; i++) rak.echo[i].t = 0;
    rak.n = 0; rak.hits = 0; rak.next = 0; rak.tr = 0; rak.inited = 1;
}
static void rak_layout(int n)
{
#define RK(w_, z_, n_, q_) {RAK_##w_, z_, n_, q_}
    static const rpart_t L[7][RP_N] = {
        /* FIRST LINE: 32 sixteenths round */
        {RK(ROUND, 3, 0, 4), RK(FLAT, 3, 0, 3), RK(ROUND, 3, 5, 4), RK(SPRINGY, 3, 0, 5), RK(ROUND, 3, 0, 4), RK(FLAT, 3, 4, 3), RK(ROUND, 3, 5, 5), RK(SPRINGY, 3, 3, 4),
         RK(ROUND, 3, 0, 4), RK(FLAT, 3, 0, 3), RK(STICKY, 3, 7, 5), RK(SPRINGY, 3, 5, 6), RK(ROUND, 3, 0, 4), RK(FLAT, 3, 2, 3), RK(ROUND, 3, 4, 5), RK(SPRINGY, 3, 0, 4)},
        /* RUNNER: quick, 24 sixteenths round */
        {RK(FLAT, 3, 0, 4), RK(FLAT, 2, 0, 2), RK(ROUND, 3, 5, 5), RK(FLAT, 3, 0, 3), RK(FLAT, 2, 4, 4), RK(ROUND, 3, 0, 5), RK(FLAT, 3, 0, 2), RK(STICKY, 3, 5, 6),
         RK(FLAT, 3, 0, 4), RK(FLAT, 2, 2, 3), RK(ROUND, 3, 3, 5), RK(FLAT, 3, 0, 2), RK(ROUND, 3, 5, 6), RK(STICKY, 3, 7, 6), RK(ROUND, 3, 4, 4), RK(ROUND, 3, 0, 5)},
        /* PING PONG: two twins; a pod runs to and fro between them, and a second line waits past them for a second pod */
        {RK(ROUND, 3, 0, 4), RK(FLAT, 3, 0, 3), RK(FLAT, 3, 5, 5), RK(ROUND, 3, 3, 4), RK(TWIN, 3, 7, 6), RK(FLAT, 3, 4, 3), RK(ROUND, 4, 0, 5), RK(GAP, 3, 0, 3),
         RK(STICKY, 3, 5, 5), RK(FLAT, 3, 2, 3), RK(GAP, 3, 0, 3), RK(ROUND, 4, 0, 4), RK(TWIN, 3, 5, 6), RK(FLAT, 3, 4, 3), RK(ROUND, 3, 2, 5), RK(FLAT, 3, 0, 4)},
        /* SLIDER: sticky caps, every other note slides */
        {RK(STICKY, 3, 0, 5), RK(FLAT, 3, 0, 3), RK(STICKY, 3, 5, 6), RK(FLAT, 3, 0, 3), RK(STICKY, 3, 3, 5), RK(ROUND, 3, 4, 4), RK(STICKY, 4, 7, 7), RK(FLAT, 3, 5, 3),
         RK(STICKY, 3, 0, 5), RK(FLAT, 3, 0, 3), RK(STICKY, 3, 4, 6), RK(ROUND, 3, 2, 4), RK(STICKY, 3, 5, 6), RK(FLAT, 3, 0, 3), RK(STICKY, 4, 3, 7), RK(ROUND, 3, 0, 4)},
        /* HOLES: gaps, so rests */
        {RK(ROUND, 3, 0, 5), RK(FLAT, 3, 0, 3), RK(GAP, 3, 0, 3), RK(ROUND, 3, 5, 5), RK(FLAT, 3, 0, 3), RK(FLAT, 3, 4, 4), RK(GAP, 3, 0, 3), RK(ROUND, 3, 0, 5),
         RK(STICKY, 3, 3, 6), RK(FLAT, 3, 0, 3), RK(GAP, 3, 0, 3), RK(SPRINGY, 3, 0, 6), RK(ROUND, 3, 5, 4), RK(GAP, 3, 0, 3), RK(FLAT, 3, 4, 4), RK(ROUND, 3, 0, 5)},
        /* HEAVY SPRINGS: big caps, an octave lower and slow, quick little ones between */
        {RK(SPRINGY, 5, 0, 5), RK(FLAT, 2, 5, 3), RK(FLAT, 2, 5, 4), RK(SPRINGY, 5, 3, 6), RK(ROUND, 3, 5, 4), RK(FLAT, 2, 9, 3), RK(SPRINGY, 6, 0, 7), RK(ROUND, 3, 4, 4),
         RK(SPRINGY, 5, 0, 5), RK(FLAT, 2, 5, 3), RK(STICKY, 3, 7, 6), RK(SPRINGY, 5, 4, 6), RK(ROUND, 3, 5, 4), RK(FLAT, 2, 10, 3), RK(SPRINGY, 6, 2, 7), RK(ROUND, 3, 0, 4)},
        /* STAIRS: up the scale to a twin at the top, and down again */
        {RK(ROUND, 3, 0, 4), RK(FLAT, 3, 1, 3), RK(FLAT, 3, 2, 3), RK(ROUND, 3, 3, 4), RK(FLAT, 3, 4, 3), RK(FLAT, 3, 5, 4), RK(ROUND, 3, 6, 5), RK(FLAT, 3, 7, 3),
         RK(FLAT, 3, 8, 4), RK(ROUND, 3, 9, 5), RK(FLAT, 3, 10, 4), RK(FLAT, 3, 11, 5), RK(STICKY, 3, 12, 6), RK(FLAT, 3, 13, 5), RK(ROUND, 3, 14, 6), RK(TWIN, 4, 15, 7)},
    };
#undef RK
    int s;
    n = iclamp(n, 1, 7) - 1;
    for (s = 0; s < RP_N; s++) ri[RI_RACK].p[s] = L[n][s];
}
/* cap s sounds: by a pod's landing (how fast it came down) or by hand */
static void rak_sound(int s, int32_t vel, int accent)
{
    rinst_t *r = &ri[RI_RACK];
    const rpart_t *p = &r->p[s];
    int32_t deg = rak_deg(s), qual = p->qual + (accent ? 2 : 0);
    int k;
    if (!p->what) return;
    vel = iclamp(vel, 30, 255);
    r->fl[s] = 255; rak.sq[s] = (uint8_t)iclamp(100 + vel * 2 / 3, rak.sq[s], 255);
    rm_hit(RI_RACK, r->preset == 2 ? SK_SLIDE : RAK_SND[rak_kind(s)], deg, vel, p->size, qual);
    if (r->preset == 3)                          /* TWINS: the same note an octave up, a sixteenth later, softer */
        for (k = 0; k < 4; k++) if (rak.echo[k].t <= 0) {
            rak.echo[k].t = (int32_t)(TICKS_STEP << 8); rak.echo[k].deg = (uint8_t)iclamp(deg + RSC[rm.scale % RSC_N][0], 0, 40);
            rak.echo[k].vel = (uint8_t)(vel * 5 / 8); rak.echo[k].size = (uint8_t)imin(p->size, 4); rak.echo[k].qual = (uint8_t)imin(qual, 7);
            break;
        }
}
/* pod i is thrown: from the cap it sits on, or from the net. Up as hard as the cap's kind, its size and SPRING say;
 * along just fast enough to come down on the cap TILT says */
static void rak_launch(int i, int from_net)
{
    static const uint8_t TB[RAK_NK] = {24, 32, 16, 48, 16, 32};   /* a bounce's length in sixteenths Q4, cap to cap on a level */
    static const uint8_t PM[4] = {64, 48, 64, 64};                /* (an acorn is heavy: it does not rise as far) */
    const rinst_t *r = &ri[RI_RACK];
    int s = rak.pod[i].cap, kind = from_net ? RAK_GAP : rak_kind(s), h, stepn, sgn, tgt, tc;
    int32_t T, v8, y0, yt, dy, disc, tq4, hmax;
    if (!from_net) rak.pod[i].y = rak_top(s) << 16;
    y0 = rak.pod[i].y >> 16;
    h = (ri_k(RI_RACK, 1) + 8) / 16 - 4;         /* TILT in half caps a bounce, -4..4 */
    stepn = (iabs(h) + (rak.pod[i].par & 1)) / 2; rak.pod[i].par++;
    sgn = (h < 0 ? -1 : 1) * rak.pod[i].dir;
    if (from_net && !stepn) stepn = 1;           /* (never left bouncing on the net) */
    tgt = s + sgn * stepn; tc = tgt;
    if (r->sel == 0) tc = (tgt + RP_N) % RP_N;   /* WRAP: it flies out at one end and in at the other */
    else if (r->sel == 1) {                      /* BOUNCE: back from the end */
        if (tgt < 0) { tgt = -tgt; rak.pod[i].dir = (int8_t)-rak.pod[i].dir; }
        else if (tgt > RP_N - 1) { tgt = 2 * (RP_N - 1) - tgt; rak.pod[i].dir = (int8_t)-rak.pod[i].dir; }
        tc = tgt;
    } else if (tgt < 0 || tgt > RP_N - 1) tc = -1;   /* FALL: over the end */
    T = TB[kind] + (from_net ? 0 : ((r->p[s].size & 7) - 3) * 4) - 3;   /* (a little short of the sixteenth, so that the sieve has something to hold) */
    T = iclamp(T * (32 + ri_k(RI_RACK, 0) / 2) / 64 * PM[r->preset & 3u] / 64, 8, 64);
    v8 = (RAK_G * T * 3 / 4) >> 8;               /* up, px Q8 a tick: half the bounce's length times gravity */
    hmax = imax(2, y0 - RAK_CEIL);
    if (v8 * v8 > 2 * RAK_G * hmax) v8 = (int32_t)isqrt((uint32_t)(2 * RAK_G * hmax));
    yt = tc >= 0 && r->p[tc].what ? rak_top(tc) : RAK_NET;
    dy = yt - y0;
    disc = v8 * v8 + 2 * RAK_G * dy;
    if (disc < 2 * RAK_G * 3) {                  /* the cap it is bound for stands higher than this throw: thrown just over it */
        v8 = (int32_t)isqrt((uint32_t)(2 * RAK_G * imax(1, 3 - dy)));
        disc = imax(0, v8 * v8 + 2 * RAK_G * dy);
    }
    tq4 = imax(16, (v8 + (int32_t)isqrt((uint32_t)disc)) * 4096 / RAK_G);   /* how long it will be in the air, ticks Q4 */
    rak.pod[i].vx = (((rak_cx(tgt) << 16) - rak.pod[i].x) / tq4) * 16;
    rak.pod[i].vy = -(v8 << 8);
    rak.pod[i].st = RAK_FLY;
}
/* pod i's wait on its cap is over: the note, and on it goes */
static void rak_fire(int i)
{
    static const uint8_t PV[4] = {208, 255, 240, 224};
    const rinst_t *r = &ri[RI_RACK];
    int s = rak.pod[i].cap, kind = rak_kind(s), acc = 0;
    int32_t vel = (40 + (rak.pod[i].hit >> 8) * 190 / 1000) * PV[r->preset & 3u] / 256;
    if (!kind) { rak.pod[i].st = RAK_FLY; rak.pod[i].vx = rak.pod[i].vy = 0; return; }   /* the cap is gone from under it */
    rak.hits++;
    if (r->sq[3] && rak.hits % r->sq[3] == 0) { acc = 1; vel += r->preset == 1 ? 110 : 70; }
    if (r->preset == 1) vel += 30;
    rak_sound(s, vel, acc);
    rak.pod[i].glow = acc ? 260 : 160;
    if (kind == RAK_TWIN) rak.pod[i].dir = (int8_t)-rak.pod[i].dir;
    if (!r->run) { rak.pod[i].st = RAK_REST; return; }
    if (kind == RAK_STICKY) { rak.pod[i].st = RAK_STICK; rak.pod[i].t = 20 * 256 * (32 + ri_k(RI_RACK, 0) / 2) / 64; }
    else rak_launch(i, 0);
}
static void rak_step(void)                       /* 10 ms */
{
    rinst_t *r = &ri[RI_RACK];
    int i, s, n, edit = rm.edit && rm.focus == RI_RACK;
    int32_t dtq = iclamp((int32_t)(rm.tq8 - rm.ptq8), 1, 2048);
    const int32_t xl = (RAK_X0 - RAK_SP / 2) << 16, xr = (RAK_X0 + (RP_N - 1) * RAK_SP + RAK_SP / 2) << 16;
    if (!rak.inited) rak_init();
    for (s = 0; s < RP_N; s++) rak.sq[s] = (uint8_t)(rak.sq[s] > 10 ? rak.sq[s] - 10 : 0);
    for (i = 0; i < RAK_NP; i++) if (rak.pod[i].glow > 0) rak.pod[i].glow -= 10;
    if (edit && !r->run) return;                 /* open for planting: the world holds still */
    n = rak_count();
    for (i = rak.n; i < n; i++) rak_seed(i, 0);  /* a new pod drops in over its starting cap */
    rak.n = (uint8_t)n;
    for (i = 0; i < 4; i++) if (rak.echo[i].t > 0) {
        rak.echo[i].t -= dtq;
        if (rak.echo[i].t <= 0) rm_hit(RI_RACK, SK_ACID, rak.echo[i].deg, rak.echo[i].vel, rak.echo[i].size, rak.echo[i].qual);
    }
    if (r->seq && r->run && rm_cross((uint32_t)RAK_LEN[r->sq[0] & 7u] * TICKS_STEP, 0, 0)) {   /* the reset: every pod back on its starting cap */
        for (i = 0; i < n; i++) rak_seed(i, 1);
        rak.hits = 0;
    }
    for (i = 0; i < n; i++) {
        int32_t py, xp, yp;
        if (rak.pod[i].st != RAK_FLY) {
            s = rak.pod[i].cap;
            if (rak.pod[i].st == RAK_NETTED) {
                rak.pod[i].t -= dtq;
                if (rak.pod[i].t <= 0 && r->run) rak_launch(i, 1);
                continue;
            }
            rak.pod[i].y = rak_top(s) << 16;
            if (rak.pod[i].st != RAK_REST) rak.sq[s] = (uint8_t)imax(rak.sq[s], 200);   /* a pod sits on it */
            if (!r->run) { rak.pod[i].st = RAK_REST; continue; }                       /* PLAY off: they come to rest */
            if (rak.pod[i].st == RAK_REST) { rak.pod[i].st = RAK_WAIT; rak.pod[i].t = rak_wait(); }
            rak.pod[i].t -= dtq;
            if (rak.pod[i].t > 0) continue;
            if (rak.pod[i].st == RAK_WAIT) rak_fire(i); else rak_launch(i, 0);
            continue;
        }
        py = rak.pod[i].y >> 16;
        rak.pod[i].vy += (RAK_G * dtq) >> 8;
        rak.pod[i].vy = imin(rak.pod[i].vy, 400000);
        rak.pod[i].x += ((rak.pod[i].vx >> 4) * dtq) >> 4;
        rak.pod[i].y += ((rak.pod[i].vy >> 4) * dtq) >> 4;
        if (r->sel == 0) { if (rak.pod[i].x < xl) rak.pod[i].x += (RP_N * RAK_SP) << 16; else if (rak.pod[i].x >= xr) rak.pod[i].x -= (RP_N * RAK_SP) << 16; }
        else if (r->sel == 1 && (rak.pod[i].x < xl || rak.pod[i].x >= xr)) {   /* (the ends were closed while it was on its way out) */
            rak.pod[i].x = iclamp(rak.pod[i].x, xl, xr - 1); rak.pod[i].vx = -rak.pod[i].vx; rak.pod[i].dir = (int8_t)-rak.pod[i].dir;
        }
        xp = rak.pod[i].x >> 16; yp = rak.pod[i].y >> 16;
        if (yp > 215 || xp < -40 || xp > AC_W + 40) { rak_seed(i, 0); continue; }   /* fallen off: in again from above */
        if (rak.pod[i].vy <= 0 || xp < RAK_X0 - RAK_SP / 2) continue;
        s = (xp - RAK_X0 + RAK_SP / 2) / RAK_SP;
        if (s > RP_N - 1) continue;
        if (r->p[s].what && iabs(xp - rak_cx(s)) <= rak_hw(s) + 1 && yp >= rak_top(s) && py <= rak_top(s) + 2) {   /* down onto a cap */
            rak.pod[i].cap = (int8_t)s; rak.pod[i].x = rak_cx(s) << 16; rak.pod[i].y = rak_top(s) << 16;
            rak.pod[i].hit = rak.pod[i].vy; rak.pod[i].vx = rak.pod[i].vy = 0;
            rak.sq[s] = (uint8_t)imax(rak.sq[s], 200);
            if (r->run) { rak.pod[i].st = RAK_WAIT; rak.pod[i].t = rak_wait(); } else rak.pod[i].st = RAK_REST;
        } else if (yp >= RAK_NET - 2) {          /* into the net: a rest */
            rak.pod[i].cap = (int8_t)s; rak.pod[i].y = (RAK_NET - 2) << 16; rak.pod[i].vx = rak.pod[i].vy = 0;
            rak.pod[i].st = RAK_NETTED; rak.pod[i].t = 8 << 8;
        }
    }
}
static void rak_key(int i, int vel)              /* a pod dropped on that cap now; with no pods about, the cap played by hand */
{
    const rinst_t *r = &ri[RI_RACK];
    int j;
    if (!rak.inited) rak_init();
    i = iclamp(i, 0, RP_N - 1);
    if (!rak.n) { rak_sound(i, vel, 0); return; }
    j = rak.next % rak.n; rak.next = (uint8_t)((j + 1) % RAK_NP);
    rak.pod[j].cap = (int8_t)i; rak.pod[j].x = rak_cx(i) << 16; rak.pod[j].y = rak_top(i) << 16; rak.pod[j].vx = rak.pod[j].vy = 0;
    rak.pod[j].hit = imax(0, vel - 40) * 1000 / 190 * 256;
    if (r->p[i].what) rak_fire(j);
    else { rak.pod[j].st = RAK_FLY; rak.pod[j].y = (rak_top(i) - 10) << 16; }   /* over a gap: down it goes */
}
static void rak_top_key(int i) { rak.tr = (int8_t)(iclamp(i, 0, 10) - 5); rm_say_n("LINE", rak.tr); }
static void rak_play(int on) { if (!on) sfx_ex(45, 0, 260, W_SAW, 1200, 2, 60, 0, 0, 65200); else snd_ui(72); }

/* one cap, its top at y (already squashed by q px) */
static void rak_cap_draw(int32_t cx, int32_t y, int kind, int32_t hw, int32_t q, int32_t qual, col_t c, uint32_t seed)
{
    int32_t ry, k, yy;
    hw += q / 2;
    switch (kind) {
    case RAK_FLAT:                               /* a low wide plate */
        ghl(cx - hw, cx + hw + 1, y, c); grect(cx - hw - 1, y + 1, 2 * hw + 3, 2, c);
        ghl(cx - hw + 1, cx + hw, y + 3, th->stem);
        grect(cx - 1, y + 4, 2, RAK_PLANK - y - 4, th->stem);
        break;
    case RAK_SPRINGY:                            /* a tall cap on a coiled stem */
        ry = imax(3, hw + 1 - q);
        gellf(cx, y + ry, hw, ry, c, 1);
        ghl(cx - hw + 1, cx + hw, y + ry + 1, th->stem);
        for (yy = y + ry + 2, k = 0; yy < RAK_PLANK; yy += 3, k++) {
            int32_t y2 = imin(RAK_PLANK, yy + 3);
            gline(cx + (k & 1 ? 2 : -2), yy, cx + (k & 1 ? -2 : 2), y2, th->text);
        }
        break;
    case RAK_STICKY:                             /* a dome that drips */
        ry = imax(2, hw - 1 - q);
        gellf(cx, y + ry, hw, ry, c, 1);
        grect(cx - 1, y + ry + 1, 2, RAK_PLANK - y - ry - 1, th->stem);
        for (k = -1; k <= 1; k++) {
            int32_t l = 2 + ((tsin(rm.t, 16, (int32_t)seed * 9000 + k * 17000) + 16384) * 3 >> 15), dx = k * (hw - 1);
            grect(cx + dx, y + ry + 1, 1, l, c); gp(cx + dx, y + ry + 1 + l, th->text);
        }
        break;
    case RAK_TWIN: {                             /* two little caps on a forked stem */
        int32_t r2 = imax(2, hw / 2 + 1), fy;
        ry = imax(2, r2 - q / 2); fy = imin(RAK_PLANK, y + ry + 6);
        gellf(cx - r2, y + ry, r2, ry, c, 1); gellf(cx + r2, y + ry, r2, ry, c, 1);
        gline(cx - r2, y + ry + 1, cx, fy, th->stem); gline(cx + r2, y + ry + 1, cx, fy, th->stem);
        grect(cx - 1, fy, 2, RAK_PLANK - fy, th->stem);
        break; }
    default:                                     /* ROUND */
        ry = imax(2, hw - q);
        gellf(cx, y + ry, hw, ry, c, 1);
        ghl(cx - hw + 2, cx + hw - 1, y + ry + 1, th->stem);
        grect(cx - 1, y + ry + 2, 2, RAK_PLANK - y - ry - 2, th->stem);
        break;
    }
    if (qual >= 4) gp(cx - 1, y + 1, th->text);  /* a squelchy one glistens */
    if (qual >= 6) gp(cx + 1, y + 2, th->text);
}
static void rak_draw(int edit)
{
    static const uint8_t PR[4] = {2, 3, 2, 2};
    const rinst_t *r = &ri[RI_RACK];
    col_t c = ri_col(RI_RACK);
    int s, i, n = rak.n, still = edit && !r->run, h = (ri_k(RI_RACK, 1) + 8) / 16 - 4;
    int32_t x, k;
    char nb[8];
    if (!rak.inited) rak_init();
    /* the rack: legs, the net under it, what the ends do */
    gline(4, RAK_PLANK, 4, RAK_NET + 3, th->stem); gline(AC_W - 5, RAK_PLANK, AC_W - 5, RAK_NET + 3, th->stem);
    for (x = 6; x < AC_W - 6; x += 3) gp(x, RAK_NET + ((x / 3) & 1), th->mute);
    if (r->sel == 1) { grect(2, RAK_TOP - 44, 2, RAK_PLANK - RAK_TOP + 44, th->rim); grect(AC_W - 4, RAK_TOP - 44, 2, RAK_PLANK - RAK_TOP + 44, th->rim); }
    else if (r->sel == 0)
        for (k = 0; k < 2; k++) {
            int32_t ax = k ? AC_W - 3 : 2, ay = RAK_TOP - 16, d = k ? -1 : 1;
            gline(ax, ay, ax + 2 * d, ay - 2, th->mute); gline(ax, ay, ax + 2 * d, ay + 2, th->mute);
            gline(ax, ay + 7, ax + 2 * d, ay + 5, th->mute); gline(ax, ay + 7, ax + 2 * d, ay + 9, th->mute);
        }
    for (s = 0; s < RP_N; s++) {
        const rpart_t *p = &r->p[s];
        int32_t cx = rak_cx(s), hot = r->fl[s], q = rak.sq[s] * 4 / 256, y = rak_top(s) + q;
        if (p->what) {
            grect(cx - 7, RAK_PLANK, RAK_SP, 2, th->stem);
            if (hot) { galpha(hot * 150 / 256); gdisc(cx, y + 2, rak_hw(s) + 5, c); galpha(256); }
            rak_cap_draw(cx, y, p->what, rak_hw(s), q, p->qual, hot > 120 ? th->text : c, (uint32_t)s);
            if (hot > 60) { galpha(hot); gcirc(cx, y + 1, rak_hw(s) + 3 + (255 - hot) / 24, th->text); galpha(256); }
        } else if (edit) { gp(cx - 2, RAK_PLANK, th->mute); gp(cx, RAK_PLANK, th->mute); gp(cx + 2, RAK_PLANK, th->mute); }
        if (edit) {
            int on = r->hand >> s & 1;
            nb[0] = s >= 9 ? '1' : (char)('1' + s); nb[1] = s >= 9 ? (char)('0' + s - 9) : 0; nb[2] = 0;
            gtextc(cx + 1, RAK_PLANK + 8, nb, on ? th->text : th->mute, 1);
            if (on) gbox(cx - 7, RAK_TOP - 28, RAK_SP + 1, RAK_PLANK - RAK_TOP + 46, th->text);
        }
    }
    /* where the pods start: the first one's cap marked whole, the others hollow */
    for (i = imax(n, 1) - 1; i >= 0; i--) {
        int32_t cx = rak_cx((r->sq[1] + i * r->sq[2]) % RP_N), xy[6];
        xy[0] = cx - 3; xy[1] = RAK_PLANK + 6; xy[2] = cx; xy[3] = RAK_PLANK + 3; xy[4] = cx + 3; xy[5] = RAK_PLANK + 6;
        if (i == 0) gpoly(xy, 3, th->pad); else gpolyline(xy, 3, 0, th->mute);
    }
    for (i = 0; i < n; i++) {
        int32_t px = rak.pod[i].x >> 16, py = (rak.pod[i].y >> 16), pr = PR[r->preset & 3u], gl = rak.pod[i].glow;
        col_t pc = gl > 60 ? th->text : th->rock;
        if (rak.pod[i].st == RAK_FLY) {          /* a tail, the way it came */
            galpha(90); gline(px, py - pr, px - (rak.pod[i].vx >> 13), py - pr - (rak.pod[i].vy >> 14), th->text); galpha(256);
        } else if (rak.pod[i].st != RAK_NETTED) py += rak.sq[rak.pod[i].cap] * 4 / 256;
        py -= pr;
        if (still) galpha(150);
        if (r->preset == 3) { gdisc(px - 2, py - 1, 2, pc); gcirc(px - 2, py - 1, 2, th->text); gdisc(px + 3, py - 1, 2, pc); gcirc(px + 3, py - 1, 2, th->text); }   /* two, joined */
        else {
            gdisc(px, py, pr, pc); gcirc(px, py, pr, th->text);
            if (r->preset == 1) ghl(px - pr, px + pr + 1, py - 1, th->stem);                    /* the acorn's cup */
            if (r->preset == 2) { gline(px, py - pr, px, py - pr - 3, th->text); gp(px - 1, py - 1, th->bg); }   /* the drop's tail */
        }
        galpha(256);
        if (gl > 0) { galpha(imin(256, gl)); gcirc(px, py, pr + 2 + (260 - gl) / 30, th->text); galpha(256); }
    }
    /* the reset: a bar that fills until the pods go back */
    if (r->seq && !still) {
        uint32_t per = (uint32_t)RAK_LEN[r->sq[0] & 7u] * TICKS_STEP, ph = (rm.tq8 >> 8) % per;
        int32_t nq = RAK_LEN[r->sq[0] & 7u], bw = 150;
        gbox(44, 27, bw + 2, 5, th->mute);
        grect(45, 28, (int32_t)(ph * (uint32_t)bw / per), 3, ph < TICKS_STEP ? th->text : c);
        for (k = 4; k < nq; k += 4) gp(45 + k * bw / nq, 31, th->text);
        gtext(4, 26, "RESET", th->mute, 1);
        nb[0] = nq >= 10 ? (char)('0' + nq / 10) : (char)('0' + nq); nb[1] = nq >= 10 ? (char)('0' + nq % 10) : 0; nb[2] = 0;
        x = gtext(200, 26, nb, th->text, 1); gtext(x + 2, 26, "/16", th->mute, 1);
    }
    if (still) {
        if ((r->hand & (r->hand - 1)) == 0 && r->hand) {   /* one cap in hand: what it is */
            for (s = 0; s < RP_N - 1 && !(r->hand >> s & 1u); s++) ;
            gtextc(120, 40, RAK_KINDS[r->p[s].what], r->p[s].what ? c : th->mute, 2);
            rm_note_name(nb, rm_note(RI_RACK, rak_deg(s)));
            gtextc(120, 60, nb, th->text, 1);
        }
        return;
    }
    /* the ends, the tilt as arrows (a dim one: every other bounce), how far the top row has moved the line */
    x = gtext(4, 16, "ENDS", th->mute, 1); x = gtext(x + 4, 16, RAK_ENDS[imin(r->sel, 2)], th->text, 1) + 10;
    if (!h) gtext(x, 16, "\xE2\x86\x91", th->text, 1);
    else {
        const char *a = h < 0 ? "\xE2\x86\x90" : "\xE2\x86\x92";
        for (k = 0; k < iabs(h) / 2; k++) x = gtext(x, 16, a, th->text, 1) + 1;
        if (iabs(h) & 1) gtext(x, 16, a, th->mute, 1);
    }
    for (k = 0; k < 11; k++) {
        if (k == rak.tr + 5) grect(180 + k * 5, 16, 3, 5, c);
        else grect(180 + k * 5, k == 5 ? 17 : 18, 3, k == 5 ? 3 : 1, th->mute);
    }
    if (rak.tr) {
        int32_t v = iabs(rak.tr);
        nb[0] = rak.tr < 0 ? '-' : '+'; nb[1] = (char)('0' + v); nb[2] = 0;
        gtextr(176, 16, nb, th->text, 1);
    }
}

/* ---- its lesson: the rack itself, played by the lesson */
static void rak_lreset(void)
{
    rinst_t *r = &ri[RI_RACK];
    int i;
    ri_load_algo(RI_RACK, 1);
    r->seq = 0; r->arp = 0; r->run = 1; r->sel = 0; r->preset = 0; r->oct = 0;
    for (i = 0; i < 4; i++) { r->k[i] = RAK_KDEF[i]; r->lfo[i] = 0; r->env[i] = 4; r->sq[i] = RAK_SQDEF[i]; }
    rm.edit = 0; rm.menu = 0;
    rak.tr = 0; rak.n = 0; rak.hits = 0;         /* (the pods drop in afresh) */
}
static void rak_l1(int32_t t, int32_t dt) { if (RT_FIRST) rak_lreset(); rt.icon = RTI_NONE; }
static void rak_l2(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_RACK];
    int i;
    if (RT_FIRST) {
        r->k[1] = 4;                             /* no tilt: the pod stays on the cap it is dropped on */
        for (i = 0; i < 5; i++) { r->p[5 + i].what = (uint8_t)(1 + i); r->p[5 + i].size = 3; r->p[5 + i].note = (uint8_t)(i * 2); r->p[5 + i].qual = 4; }
    }
    rt.icon = RTI_LOW;
    for (i = 0; i < 5; i++) if (RT_AT(500 + i * 1500)) {
        if (i == 4) r->k[1] = 6;                 /* the twin: it travels, to be sent back */
        rak_key(5 + i, 200); rt.label = RAK_KINDS[1 + i]; rt.label_t = 1000;
    }
}
static void rak_l3(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_RACK];
    if (RT_FIRST) { ri_load_algo(RI_RACK, 1); r->k[1] = 6; }
    rt.icon = t < 3600 ? RTI_K1 : RTI_K2;
    if (RT_AT(700)) r->k[0] = 8;
    if (RT_AT(2200)) r->k[0] = 0;
    if (RT_AT(3600)) { r->k[0] = 4; r->k[1] = 8; }
    if (RT_AT(5200)) r->k[1] = 2;
}
static void rak_l4(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_RACK];
    if (RT_FIRST) { r->k[1] = 6; r->k[2] = 0; }
    rt.icon = t < 3000 ? RTI_K3 : t < 5600 ? RTI_K4 : RTI_SELECT;
    if (RT_AT(1800)) r->k[2] = 8;
    if (RT_AT(3200)) r->k[3] = 2;
    if (RT_AT(4400)) r->k[3] = 3;
    if (RT_AT(5800)) r->sel = 1;
}
static void rak_l5(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_RACK];
    if (RT_FIRST) { r->sel = 0; r->k[3] = 2; r->seq = 1; r->sq[0] = 1; }
    rt.icon = t < 3000 ? RTI_BTN + BTN_SEQ : RTI_K1;
    if (RT_AT(3400)) r->sq[0] = 0;
    if (RT_AT(5600)) r->sq[0] = 3;
}
static void rak_l6(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_RACK];
    if (RT_FIRST) r->sq[0] = 3;
    rt.icon = t < 2500 ? RTI_LOW : t < 5200 ? RTI_TOP : RTI_PRESETS;
    if (RT_AT(600)) rak_key(12, 200);
    if (RT_AT(1500)) rak_key(3, 200);
    if (RT_AT(2700)) rak.tr = 2;
    if (RT_AT(3600)) rak.tr = 3;
    if (RT_AT(4500)) rak.tr = 0;
    if (RT_AT(5300)) r->preset = 3;
}
static void rak_l7(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_RACK];
    rpart_t *p = &r->p[6];
    if (RT_FIRST) { rm.edit = 1; r->run = 0; r->hand = 1u << 2; r->preset = 0; r->seq = 0; rak.tr = 0; }
    rt.icon = t < 1400 ? RTI_BTN + BTN_EDIT : t < 2200 ? RTI_LOW : t < 3200 ? RTI_K1 : t < 4700 ? RTI_K2 : t < 6200 ? RTI_K3 : RTI_K4;
    if (RT_AT(1500)) { r->hand = 1u << 6; ri_strike(RI_RACK, 6, 150); }
    if (RT_AT(2400)) { p->what = RAK_SPRINGY; p->size = 1; p->qual = 1; ri_strike(RI_RACK, 6, 150); }
    if (RT_AT(3400) || RT_AT(3800) || RT_AT(4200)) { p->size = (uint8_t)imin(7, p->size + 2); ri_strike(RI_RACK, 6, 150); }
    if (RT_AT(4900) || RT_AT(5300) || RT_AT(5700)) { p->note = (uint8_t)imin(20, p->note + 2); ri_strike(RI_RACK, 6, 150); }
    if (RT_AT(6400) || RT_AT(6900)) { p->qual = (uint8_t)imin(7, p->qual + 3); ri_strike(RI_RACK, 6, 150); }
}
static void rak_l8(int32_t t, int32_t dt)
{
    if (RT_FIRST) { rm.edit = 0; ri[RI_RACK].run = 1; }
    rt.icon = t < 1800 ? RTI_BTN + BTN_REC : t < 3400 ? RTI_BTN + BTN_SAVE : RTI_BTN + BTN_HOME;
}
static const rt_step_t RAK_LESSON[8] = {
    {6000, {"A POD BOUNCES ALONG A RACK OF MUSHROOM CAPS. EVERY LANDING IS AN ACID BASS NOTE. THE CAP GIVES THE PITCH, THE FALL GIVES THE FORCE.",
            "EINE KAPSEL HÜPFT ÜBER EIN REGAL VOLLER PILZHÜTE. JEDE LANDUNG IST EIN ACID-BASSTON. DER HUT GIBT DIE TONHÖHE, DER FALL DIE WUCHT.",
            "EN KAPSEL HOPPER HEN OVER EN HYLDE MED SVAMPEHATTE. HVER LANDING ER EN ACID-BASTONE. HATTEN GIVER TONEN, FALDET GIVER KRAFTEN.",
            "UNE CAPSULE REBONDIT SUR UNE RANGÉE DE CHAPEAUX DE CHAMPIGNONS. CHAQUE CHUTE EST UNE NOTE DE BASSE ACID. LE CHAPEAU DONNE LA HAUTEUR, LA CHUTE DONNE LA FORCE.",
            "UNA CÁPSULA REBOTA POR UNA FILA DE SOMBREROS DE SETA. CADA CAÍDA ES UNA NOTA DE BAJO ACID. EL SOMBRERO DA LA ALTURA, LA CAÍDA DA LA FUERZA.",
            "BİR KAPSÜL MANTAR ŞAPKALARI RAFINDA ZIPLAR. HER İNİŞ BİR ACID BAS NOTASIDIR. ŞAPKA PERDEYİ, DÜŞÜŞ GÜCÜ VERİR.",
            "EEN PEUL STUITERT OVER EEN REK MET PADDENSTOELHOEDEN. ELKE LANDING IS EEN ACID-BASNOOT. DE HOED GEEFT DE TOON, DE VAL GEEFT DE KRACHT."}, rak_l1},
    {8200, {"FIVE CAPS, FIVE BOUNCES. ROUND: EVEN. FLAT: QUICK AND LOW. SPRING: HIGH, SLOW, LOUD. STICKY: THE NOTE SLIDES. TWIN: SENDS THE POD BACK. A GAP IS A REST.",
            "FÜNF HÜTE, FÜNF SPRÜNGE. ROUND: GLEICHMÄSSIG. FLAT: SCHNELL UND FLACH. SPRING: HOCH, LANGSAM, LAUT. STICKY: DER TON GLEITET. TWIN: SCHICKT DIE KAPSEL ZURÜCK. EINE LÜCKE IST EINE PAUSE.",
            "FEM HATTE, FEM HOP. ROUND: JÆVNT. FLAT: HURTIGT OG LAVT. SPRING: HØJT, LANGSOMT, KRAFTIGT. STICKY: TONEN GLIDER. TWIN: SENDER KAPSLEN TILBAGE. ET HUL ER EN PAUSE.",
            "CINQ CHAPEAUX, CINQ REBONDS. ROUND : RÉGULIER. FLAT : VIF ET BAS. SPRING : HAUT, LENT, FORT. STICKY : LA NOTE GLISSE. TWIN : RENVOIE LA CAPSULE. UN TROU EST UN SILENCE.",
            "CINCO SOMBREROS, CINCO REBOTES. ROUND: REGULAR. FLAT: RÁPIDO Y BAJO. SPRING: ALTO, LENTO, FUERTE. STICKY: LA NOTA SE DESLIZA. TWIN: DEVUELVE LA CÁPSULA. UN HUECO ES UN SILENCIO.",
            "BEŞ ŞAPKA, BEŞ SEKME. ROUND: DÜZENLİ. FLAT: HIZLI VE ALÇAK. SPRING: YÜKSEK, YAVAŞ, GÜR. STICKY: NOTA KAYAR. TWIN: KAPSÜLÜ GERİ YOLLAR. BOŞLUK BİR SUSTUR.",
            "VIJF HOEDEN, VIJF STUITEN. ROUND: GELIJKMATIG. FLAT: SNEL EN LAAG. SPRING: HOOG, TRAAG, LUID. STICKY: DE NOOT GLIJDT. TWIN: STUURT DE PEUL TERUG. EEN GAT IS EEN RUST."}, rak_l2},
    {7000, {"KNOB 1 SPRING: HOW HIGH EVERY CAP THROWS, SO HOW FAST THE LINE RUNS. KNOB 2 TILT: WHICH WAY THE PODS TRAVEL, AND HOW FAR EACH BOUNCE.",
            "KNOB 1 SPRING: WIE HOCH JEDER HUT WIRFT, ALSO WIE SCHNELL DIE LINIE LÄUFT. KNOB 2 TILT: WOHIN DIE KAPSELN WANDERN UND WIE WEIT JE SPRUNG.",
            "KNOB 1 SPRING: HVOR HØJT HVER HAT KASTER, ALTSÅ HVOR HURTIGT LINJEN LØBER. KNOB 2 TILT: HVILKEN VEJ KAPSLERNE VANDRER, OG HVOR LANGT I HVERT HOP.",
            "KNOB 1 SPRING : À QUELLE HAUTEUR CHAQUE CHAPEAU LANCE, DONC LA VITESSE DE LA LIGNE. KNOB 2 TILT : DE QUEL CÔTÉ VONT LES CAPSULES, ET JUSQU'OÙ À CHAQUE REBOND.",
            "KNOB 1 SPRING: LO ALTO QUE LANZA CADA SOMBRERO, O SEA LO RÁPIDO QUE VA LA LÍNEA. KNOB 2 TILT: HACIA DÓNDE VIAJAN LAS CÁPSULAS Y CUÁNTO EN CADA REBOTE.",
            "KNOB 1 SPRING: HER ŞAPKA NE KADAR YÜKSEĞE ATAR, YANİ ÇİZGİ NE KADAR HIZLI AKAR. KNOB 2 TILT: KAPSÜLLER NE YÖNE GİDER VE HER SEKMEDE NE KADAR.",
            "KNOB 1 SPRING: HOE HOOG ELKE HOED GOOIT, DUS HOE SNEL DE LIJN LOOPT. KNOB 2 TILT: WELKE KANT DE PEULEN OP GAAN, EN HOE VER PER STUIT."}, rak_l3},
    {7400, {"KNOB 3 SIEVE: AT ZERO THE TIMING IS RAW PHYSICS. TURNED UP, EVERY LANDING WAITS FOR THE CLOCK. KNOB 4: MORE PODS. SELECT: WHAT THE ENDS OF THE RACK DO.",
            "KNOB 3 SIEVE: BEI NULL IST DAS TIMING ROHE PHYSIK. AUFGEDREHT WARTET JEDE LANDUNG AUF DEN TAKT. KNOB 4: MEHR KAPSELN. SELECT: WAS DIE ENDEN DES REGALS TUN.",
            "KNOB 3 SIEVE: PÅ NUL ER TIMINGEN RÅ FYSIK. SKRUET OP VENTER HVER LANDING PÅ TAKTEN. KNOB 4: FLERE KAPSLER. SELECT: HVAD HYLDENS ENDER GØR.",
            "KNOB 3 SIEVE : À ZÉRO, LE RYTHME EST DE LA PHYSIQUE BRUTE. PLUS HAUT, CHAQUE CHUTE ATTEND L'HORLOGE. KNOB 4 : PLUS DE CAPSULES. SELECT : CE QUE FONT LES BOUTS DE LA RANGÉE.",
            "KNOB 3 SIEVE: EN CERO, EL RITMO ES FÍSICA PURA. MÁS ARRIBA, CADA CAÍDA ESPERA AL RELOJ. KNOB 4: MÁS CÁPSULAS. SELECT: QUÉ HACEN LOS EXTREMOS DE LA FILA.",
            "KNOB 3 SIEVE: SIFIRDA ZAMANLAMA HAM FİZİKTİR. AÇILINCA HER İNİŞ SAATİ BEKLER. KNOB 4: DAHA ÇOK KAPSÜL. SELECT: RAFIN UÇLARI NE YAPAR.",
            "KNOB 3 SIEVE: OP NUL IS DE TIMING PURE NATUURKUNDE. HOGER WACHT ELKE LANDING OP DE KLOK. KNOB 4: MEER PEULEN. SELECT: WAT DE UITEINDEN VAN HET REK DOEN."}, rak_l4},
    {7600, {"SEQ IS THE RESET: EVERY SO MANY STEPS THE PODS GO BACK TO THEIR START, SO THE LINE REPEATS: A RIFF. HOLD SEQ AND TURN KNOB 1 TO 4: LENGTH, START, SPREAD, ACCENT.",
            "SEQ IST DER RESET: ALLE PAAR SCHRITTE GEHEN DIE KAPSELN ZURÜCK AN DEN START, DIE LINIE WIEDERHOLT SICH: EIN RIFF. SEQ HALTEN UND KNOB 1 BIS 4 DREHEN: LENGTH, START, SPREAD, ACCENT.",
            "SEQ ER RESET: MED FASTE MELLEMRUM GÅR KAPSLERNE TILBAGE TIL START, SÅ LINJEN GENTAGER SIG: ET RIFF. HOLD SEQ OG DREJ KNOB 1 TIL 4: LENGTH, START, SPREAD, ACCENT.",
            "SEQ EST LE RESET : TOUS LES TANT DE PAS, LES CAPSULES REVIENNENT AU DÉPART ET LA LIGNE SE RÉPÈTE : UN RIFF. TIENS SEQ ET TOURNE KNOB 1 À 4 : LENGTH, START, SPREAD, ACCENT.",
            "SEQ ES EL RESET: CADA CIERTOS PASOS LAS CÁPSULAS VUELVEN AL INICIO Y LA LÍNEA SE REPITE: UN RIFF. MANTÉN SEQ Y GIRA KNOB 1 A 4: LENGTH, START, SPREAD, ACCENT.",
            "SEQ RESET'TİR: BELLİ ADIMLARDA KAPSÜLLER BAŞA DÖNER, ÇİZGİ TEKRARLANIR: BİR RIFF. SEQ'İ TUT VE KNOB 1-4'Ü ÇEVİR: LENGTH, START, SPREAD, ACCENT.",
            "SEQ IS DE RESET: OM DE ZOVEEL STAPPEN GAAN DE PEULEN TERUG NAAR HUN START, DUS DE LIJN HERHAALT: EEN RIFF. HOUD SEQ VAST EN DRAAI KNOB 1 TOT 4: LENGTH, START, SPREAD, ACCENT."}, rak_l5},
    {7000, {"THE BOTTOM ROW DROPS A POD ON A CAP. THE TOP ROW MOVES THE WHOLE LINE UP OR DOWN THE SCALE. PRESETS CHANGES THE PODS.",
            "DIE UNTERE REIHE WIRFT EINE KAPSEL AUF EINEN HUT. DIE OBERE REIHE SCHIEBT DIE GANZE LINIE DIE TONLEITER HINAUF ODER HINAB. PRESETS WECHSELT DIE KAPSELN.",
            "DEN NEDERSTE RÆKKE SLIPPER EN KAPSEL PÅ EN HAT. DEN ØVERSTE RÆKKE FLYTTER HELE LINJEN OP ELLER NED AD SKALAEN. PRESETS SKIFTER KAPSLERNE.",
            "LA RANGÉE DU BAS LÂCHE UNE CAPSULE SUR UN CHAPEAU. LA RANGÉE DU HAUT DÉPLACE TOUTE LA LIGNE DANS LA GAMME. PRESETS CHANGE LES CAPSULES.",
            "LA FILA DE ABAJO SUELTA UNA CÁPSULA SOBRE UN SOMBRERO. LA FILA DE ARRIBA MUEVE TODA LA LÍNEA POR LA ESCALA. PRESETS CAMBIA LAS CÁPSULAS.",
            "ALT SIRA BİR ŞAPKAYA KAPSÜL BIRAKIR. ÜST SIRA BÜTÜN ÇİZGİYİ GAMDA YUKARI YA DA AŞAĞI TAŞIR. PRESETS KAPSÜLLERİ DEĞİŞTİRİR.",
            "DE ONDERSTE RIJ LAAT EEN PEUL OP EEN HOED VALLEN. DE BOVENSTE RIJ SCHUIFT DE HELE LIJN OMHOOG OF OMLAAG DOOR DE TOONLADDER. PRESETS WISSELT DE PEULEN."}, rak_l6},
    {8000, {"EDIT STOPS THE RACK. HOLD A KEY OF THE BOTTOM ROW: THAT CAP IS IN HAND. KNOB 1: ITS KIND. KNOB 2: SIZE. KNOB 3: NOTE. KNOB 4: SQUELCH. ALGORITHM: READY-MADE LINES.",
            "EDIT HÄLT DAS REGAL AN. HALTE EINE TASTE DER UNTEREN REIHE: DIESER HUT IST IN DER HAND. KNOB 1: SEINE ART. KNOB 2: GRÖSSE. KNOB 3: TON. KNOB 4: SCHMATZEN. ALGORITHM: FERTIGE LINIEN.",
            "EDIT STOPPER HYLDEN. HOLD EN TANGENT I NEDERSTE RÆKKE: DEN HAT ER I HÅNDEN. KNOB 1: DENS SLAGS. KNOB 2: STØRRELSE. KNOB 3: TONE. KNOB 4: SMASK. ALGORITHM: FÆRDIGE LINJER.",
            "EDIT ARRÊTE LA RANGÉE. TIENS UNE TOUCHE DU BAS : CE CHAPEAU EST EN MAIN. KNOB 1 : SON GENRE. KNOB 2 : TAILLE. KNOB 3 : NOTE. KNOB 4 : SON GRAS. ALGORITHM : LIGNES TOUTES FAITES.",
            "EDIT DETIENE LA FILA. MANTÉN UNA TECLA DE ABAJO: ESE SOMBRERO ESTÁ EN LA MANO. KNOB 1: SU TIPO. KNOB 2: TAMAÑO. KNOB 3: NOTA. KNOB 4: CHAPOTEO. ALGORITHM: LÍNEAS YA HECHAS.",
            "EDIT RAFI DURDURUR. ALT SIRADAN BİR TUŞU TUT: O ŞAPKA ELİNDE. KNOB 1: TÜRÜ. KNOB 2: BOYU. KNOB 3: NOTASI. KNOB 4: VICIKLIĞI. ALGORITHM: HAZIR ÇİZGİLER.",
            "EDIT ZET HET REK STIL. HOUD EEN TOETS VAN DE ONDERSTE RIJ VAST: DIE HOED HEB JE IN DE HAND. KNOB 1: ZIJN SOORT. KNOB 2: GROOTTE. KNOB 3: NOOT. KNOB 4: SOPPIGHEID. ALGORITHM: KANT-EN-KLARE LIJNEN."}, rak_l7},
    {5500, {"REC RECORDS WHAT SOUNDS ONTO THE LOOP. SAVE KEEPS THE ROOM. HOME GOES BACK TO IT. HOLD LFO, ENV OR FX AND TURN A KNOB FOR MORE. NOW YOU!",
            "REC NIMMT AUF, WAS KLINGT. SAVE SICHERT DEN RAUM. HOME FÜHRT ZURÜCK. HALTE LFO, ENV ODER FX UND DREH EINEN KNOB FÜR MEHR. JETZT DU!",
            "REC OPTAGER DET, DER LYDER, PÅ LOOPET. SAVE GEMMER RUMMET. HOME GÅR TILBAGE. HOLD LFO, ENV ELLER FX OG DREJ EN KNOB FOR MERE. NU DIG!",
            "REC ENREGISTRE CE QUI SONNE SUR LA BOUCLE. SAVE GARDE LA SALLE. HOME Y RETOURNE. TIENS LFO, ENV OU FX ET TOURNE UN KNOB POUR ALLER PLUS LOIN. À TOI !",
            "REC GRABA LO QUE SUENA EN EL BUCLE. SAVE GUARDA LA SALA. HOME VUELVE A ELLA. MANTÉN LFO, ENV O FX Y GIRA UN KNOB PARA MÁS. ¡TE TOCA!",
            "REC ÇALANI DÖNGÜYE KAYDEDER. SAVE ODAYI SAKLAR. HOME ODAYA DÖNER. DAHASI İÇİN LFO, ENV YA DA FX'İ TUT VE BİR KNOB ÇEVİR. SIRA SENDE!",
            "REC NEEMT OP WAT KLINKT, OP DE LOOP. SAVE BEWAART DE KAMER. HOME GAAT TERUG. HOUD LFO, ENV OF FX VAST EN DRAAI AAN EEN KNOB VOOR MEER. NU JIJ!"}, rak_l8},
};
static const rdesc_t RACK_DESC = {
    "MUSHROOM RACK", {"SPRING", "TILT", "SIEVE", "PODS"}, {8, 8, 8, 4}, {4, 6, 8, 1}, {0, 1, 0, 0},
    "ENDS", 2, 0,
    "SEQ: THE RESET", {"LENGTH", "START", "SPREAD", "ACCENT"}, {7, 15, 8, 8}, {3, 0, 4, 3},
    RAK_KINDS, RAK_NK, RAK_SND, RAK_PODS, 4, RAK_ALGOS, 8,
    rak_init, rak_step, rak_draw, rak_key, rak_top_key, rak_layout, rak_play, RAK_LESSON, 8,
};
