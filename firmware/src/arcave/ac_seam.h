/* SPDX-License-Identifier: GPL-3.0-only */
/* CRYSTAL SEAM (ac_room.h has the controls every instrument shares). A seam of 16 crystals growing out of a slope of
 * rock, one to each key of the bottom row. A crystal grows; full-grown and under enough stress it breaks: it rings
 * its note, throws shards and starts again. A shard is a small body that falls: where it lands on another crystal,
 * that one rings softly and is stressed, so breaks set off breaks. A sound comes only from a break, a shard landing,
 * a tap or the drip.
 *
 *   QUARTZ    a clear ring; it cracks                    AMETHYST  a bell: tough, few heavy shards
 *   SALT      short and bright: brittle, many small      OPAL      a soft shimmer: no shards, its neighbours feel it
 *   STONE     never breaks: it only rings when a shard lands on it
 *   SIZE      how long it grows (2, 3, 4, 6, 8, 12, 16, 24 eighths at GROW's middle): every crystal is its own clock
 *   QUALITY   how brittle (and its sound)
 *
 *   KNOB 1 GROW     how fast everything grows (a quarter of the time .. four times it); tied to the clock
 *   KNOB 2 STRESS   how readily a full-grown crystal breaks by itself, on a sixteenth; at 0 only when hit or tapped
 *   KNOB 3 SHARDS   how many fly, and how far     KNOB 4 RING   how much a ringing crystal stirs its neighbours and kin
 *   SELECT FALL     the lean of the seam: the way the shards fall (7: level)
 *   PRESETS         the habit of the shards: CLEAR, FROST (they ring an octave up, and float), HEAVY (few, big, they
 *                   bounce, and ring the crystal itself), DUST (many, tiny)
 *   SEQ             the drip: a drop falls in time and taps the crystal under it (no break, but stress).
 *                   Held: KNOB 1 PACE, KNOB 2 PLACE, KNOB 3 FORCE, KNOB 4 WALK (it moves that many along each drop)
 *   bottom row      tap a crystal (a well-grown one under stress breaks)     top row   shake the rock there */
#define SEA_NSH 24
#define SEA_GY 152
enum { SEA_NONE, SEA_QUARTZ, SEA_AMETHYST, SEA_SALT, SEA_OPAL, SEA_STONE, SEA_NK };
static const char *const SEA_KINDS[SEA_NK] = {"NOTHING", "QUARTZ", "AMETHYST", "SALT", "OPAL", "STONE"};
static const uint8_t SEA_SND[SEA_NK] = {SK_CLICK, SK_CRYSTAL, SK_BELL, SK_SALT, SK_OPAL, SK_NUT};
static const char *const SEA_PRE[4] = {"CLEAR", "FROST", "HEAVY", "DUST"};
static const char *const SEA_ALGOS[8] = {"YOUR OWN", "FIRST", "STAIR", "BELL VEIN", "SALT PAN", "OPAL BED", "TEETH", "ALL OF IT"};
static const uint8_t SEA_SQDEF[4] = {5, 4, 5, 0};
static struct {
    struct { int32_t age; int16_t stress; uint16_t brk; uint8_t cool, jolt, sym, symv, glow; } c[RP_N];   /* age: clock ticks Q8 since it broke; brk: ms since */
    struct { int32_t x, y, vx, vy; uint16_t rot, life; int16_t vrot; uint8_t on, src, kind, bnc, big; } sh[SEA_NSH];   /* Q8 px; on 2: a spark going out */
    int32_t tok;                                 /* sounds the seam may still make just now */
    int32_t drip_t, drip_x, drip_y;              /* the last drop's splash */
    int32_t shake_t, shake_x;
    uint8_t tick, drip_n, inited;
} sea;

static int sea_what(int s) { int w = ri[RI_SEAM].p[s].what; return w < SEA_NK ? w : SEA_QUARTZ; }
static int32_t sea_lean(void) { return iclamp(ri[RI_SEAM].sel, 0, 14) - 7; }
static int32_t sea_gy(int32_t x) { return SEA_GY + (x - 120) * sea_lean() / 35; }   /* the rock's top at x */
static col_t sea_col(int w)
{
    return w == SEA_QUARTZ ? ri_col(RI_SEAM) : w == SEA_AMETHYST ? (th->dark ? th->caps[1] : th->caps[3]) : w == SEA_SALT ? th->text : w == SEA_OPAL ? th->caps[2] : th->mute;
}
/* where crystal s stands, and the way it points (an angle from straight up) */
static void sea_geo(int s, int32_t *bx, int32_t *by, int32_t *a)
{
    *bx = 15 + s * 14; *by = sea_gy(*bx);
    *a = sea_lean() * 280 + (int32_t)(ihash((uint32_t)s * 77u + 5u) % 2401u) - 1200;
}
static int32_t sea_per(int s)                    /* how long crystal s grows: clock ticks Q8, never 0 */
{
    static const uint8_t LEN[8] = {2, 3, 4, 6, 8, 12, 16, 24}, M[9] = {64, 48, 32, 24, 16, 12, 8, 6, 4};
    int32_t k = iclamp(ri_k(RI_SEAM, 0), 0, 128), i = k / 16, m = M[i] * 16 + (M[imin(8, i + 1)] - M[i]) * (k % 16);
    return LEN[ri[RI_SEAM].p[s].size & 7u] * 48 * m;
}
static int32_t sea_g(int s) { int32_t per = sea_per(s); return sea.c[s].age >= per ? 256 : sea.c[s].age / (per / 256); }   /* how far grown, of 256 */
static int32_t sea_thr(int s)                    /* the stress that breaks it */
{
    static const int16_t T[SEA_NK] = {9999, 150, 200, 90, 120, 9999};
    static const uint8_t Q[SEA_NK] = {0, 10, 12, 8, 8, 0};
    int w = sea_what(s);
    return T[w] - Q[w] * (ri[RI_SEAM].p[s].qual & 7);
}
static int32_t sea_hmax(int s)
{
    int32_t z = ri[RI_SEAM].p[s].size & 7;
    switch (sea_what(s)) {
    case SEA_QUARTZ: return 16 + z * 7;
    case SEA_AMETHYST: return 14 + z * 5;
    case SEA_SALT: return 12 + z * 5;
    case SEA_OPAL: return 12 + z * 4;
    case SEA_STONE: return 9 + z * 2;
    default: return 0;
    }
}
static int32_t sea_h(int s, int32_t g) { int32_t hm = sea_hmax(s); return sea_what(s) == SEA_STONE || !hm ? hm : 3 + (hm - 3) * g / 256; }
static int32_t sea_hw(int s)                     /* half its width */
{
    int32_t z = ri[RI_SEAM].p[s].size & 7;
    int w = sea_what(s);
    return w == SEA_OPAL ? 4 + z / 2 : w == SEA_STONE ? 6 : w == SEA_SALT ? 3 + z / 3 : 3 + z / 3;
}
static int sea_spend(int n) { if (sea.tok < n) return 0; sea.tok -= n; return 1; }
static void sea_add(int s, int32_t v)            /* stress onto crystal s */
{
    int w = sea_what(s);
    if (!w || w == SEA_STONE) return;
    sea.c[s].stress = (int16_t)imin(1000, sea.c[s].stress + v); sea.c[s].jolt = 1;
}
static void sea_clear(void)
{
    int i;
    for (i = 0; i < RP_N; i++) { sea.c[i].age = 0; sea.c[i].stress = 0; sea.c[i].brk = 9999; sea.c[i].cool = 99; sea.c[i].jolt = sea.c[i].sym = sea.c[i].symv = sea.c[i].glow = 0; }
    for (i = 0; i < SEA_NSH; i++) sea.sh[i].on = 0;
    sea.tok = 8; sea.drip_t = 0; sea.shake_t = 0; sea.drip_n = 0;
}
static void sea_init(void)
{
    if (!sea.inited) { sea_clear(); sea.inited = 1; }
}
static void sea_layout(int n)
{
    rpart_t *p = ri[RI_SEAM].p;
    int s;
    for (s = 0; s < RP_N; s++) { p[s].what = SEA_NONE; p[s].size = 3; p[s].note = (uint8_t)(s % 10); p[s].qual = 3; }
#define SP(s_, w_, z_, n_, q_) (p[s_].what = (w_), p[s_].size = (z_), p[s_].note = (n_), p[s_].qual = (q_))
    switch (n) {
    case 1:
        SP(0, SEA_QUARTZ, 4, 0, 3); SP(2, SEA_QUARTZ, 2, 2, 3); SP(3, SEA_SALT, 1, 7, 3); SP(4, SEA_AMETHYST, 5, 4, 3); SP(6, SEA_QUARTZ, 3, 5, 4);
        SP(7, SEA_STONE, 3, 0, 3); SP(8, SEA_QUARTZ, 2, 7, 3); SP(9, SEA_OPAL, 5, 2, 4); SP(11, SEA_QUARTZ, 1, 9, 3); SP(12, SEA_AMETHYST, 6, 1, 4);
        SP(13, SEA_SALT, 2, 10, 3); SP(15, SEA_QUARTZ, 4, 4, 3);
        break;
    case 2: for (s = 0; s < RP_N; s++) SP(s, SEA_QUARTZ, 1 + s % 4, s * 10 / 15, 2 + s % 4); break;   /* quartz up the scale, four sizes drifting */
    case 3: for (s = 0; s < RP_N; s += 2) { SP(s, SEA_AMETHYST, 3 + (s / 2) % 4, (s / 2 * 2) % 9, 1 + s % 5); if (s % 4 == 0) SP(s + 1, SEA_STONE, 2, (s / 2) % 5, 3); } break;
    case 4: for (s = 0; s < RP_N; s++) if (s % 5 != 4) SP(s, SEA_SALT, (s * 3) % 4, 5 + (s * 2) % 9, 2 + s % 5); else SP(s, SEA_QUARTZ, 5, s / 5 * 2, 3); break;
    case 5: for (s = 0; s < RP_N; s += 3) { SP(s, SEA_OPAL, 4 + s % 3, (s / 3 * 2) % 7, 2 + s / 3); if (s + 1 < RP_N) SP(s + 1, SEA_QUARTZ, 2 + s % 2, 4 + s / 3, 3); } break;
    case 6: for (s = 0; s < RP_N; s++) if (s & 1) SP(s, SEA_STONE, 1 + s % 5, (s * 3) % 10, 2 + s % 4); else if (s % 4 == 0) SP(s, s % 8 ? SEA_SALT : SEA_QUARTZ, 2 + s / 8, 5 + s / 4, 5); break;
    default: for (s = 0; s < RP_N; s++) SP(s, 1 + s % 5, 1 + (s * 3) % 6, (s * 2) % 11, (s * 5) % 8); break;
    }
#undef SP
}

/* n shards thrown up from (x, y) px; q: how hard, of 256 */
static void sea_throw(int32_t x, int32_t y, int n, int kind, int src, int32_t q)
{
    static const uint8_t BNC[4] = {1, 1, 2, 0};
    int32_t k3 = ri_k(RI_SEAM, 2), pre = ri[RI_SEAM].preset & 3, lean = sea_lean();
    int i, j;
    for (i = 0; i < n; i++) {
        int32_t vx, vy;
        for (j = 0; j < SEA_NSH && sea.sh[j].on; j++) ;
        if (j >= SEA_NSH) return;
        vx = ((int32_t)(ac_rnd() % 513u) - 256) * (48 + k3 * 2) / 256 + lean * 20;
        vy = -(240 + k3 * 2 + (int32_t)(ac_rnd() % 180u));
        if (pre == 2) vx = vx * 3 / 4;
        if (pre == 3) vy = vy * 3 / 4;
        sea.sh[j].x = Q8(x); sea.sh[j].y = Q8(y); sea.sh[j].vx = vx * q / 256; sea.sh[j].vy = vy * q / 256;
        sea.sh[j].rot = (uint16_t)ac_rnd(); sea.sh[j].vrot = (int16_t)((int32_t)(ac_rnd() % 4001u) - 2000); sea.sh[j].life = 0;
        sea.sh[j].on = 1; sea.sh[j].src = (uint8_t)src; sea.sh[j].kind = (uint8_t)kind; sea.sh[j].bnc = BNC[pre]; sea.sh[j].big = (uint8_t)(pre == 2 ? 2 : pre == 3 ? 0 : 1);
    }
}
/* crystal s rang: after a moment its neighbours and the notes akin to its own feel it (amt of 256), by KNOB 4 */
static void sea_ring(int s, int32_t amt)
{
    const rpart_t *p = ri[RI_SEAM].p;
    int32_t r4 = ri_k(RI_SEAM, 3), n = RSC[rm.scale % RSC_N][0], fifth = (n * 4 + 3) / 7;
    int j;
    if (r4 <= 0 || n <= 0) return;
    for (j = 0; j < RP_N; j++) {
        int32_t d = iabs(j - s), nd = iabs((int32_t)p[j].note - (int32_t)p[s].note) % n, w = d == 1 ? 256 : d == 2 ? 128 : 0, add;
        int k = sea_what(j);
        if (j == s || !k || k == SEA_STONE) continue;
        if (nd == 0) w = 256; else if (nd == fifth || nd == n - fifth) w = imax(w, 160);
        add = ((r4 * w >> 8) * amt >> 8) / 2;    /* up to 64: at KNOB 4's middle a chain dies out, high up it feeds itself */
        if (add < 4) continue;
        if (!sea.c[j].sym) sea.c[j].sym = (uint8_t)(6 + imin(d, 6) * 4);
        sea.c[j].symv = (uint8_t)imax(sea.c[j].symv, add);
    }
}
static void sea_break(int s, int32_t vel)
{
    static const uint8_t KN[SEA_NK] = {0, 16, 9, 26, 0, 0};   /* how many shards a kind throws, in sixteenths */
    rinst_t *r = &ri[RI_SEAM];
    const rpart_t *p = &r->p[s];
    int w = sea_what(s), pre = r->preset & 3, n, j;
    int32_t g = sea_g(s), bx, by, a, h = sea_h(s, g);
    if (!w || w == SEA_STONE) return;
    sea_geo(s, &bx, &by, &a);
    r->fl[s] = 255;
    rm_hit(RI_SEAM, w == SEA_QUARTZ ? SK_CRACK : SEA_SND[w], p->note, g >= 256 ? vel : vel * 2 / 3, p->size, p->qual);
    n = (int)(((ri_k(RI_SEAM, 2) + 15) / 24) * KN[w] / 16);
    if (ri_k(RI_SEAM, 2) > 0 && KN[w] && n < 1) n = 1;
    if (pre == 2) n = (n + 1) / 2; else if (pre == 3) n *= 2;
    if (g < 256) n = (n + 1) / 2;
    sea_throw(bx + ((h * isin(a)) >> 14), by - ((h * icos(a)) >> 14), n, w, s, w == SEA_SALT ? 170 : 256);
    if (w == SEA_OPAL)                           /* no shards: its neighbours feel it go */
        for (j = imax(0, s - 2); j <= imin(RP_N - 1, s + 2); j++) {
            int k = sea_what(j);
            if (j == s || !k || k == SEA_STONE) continue;
            if (!sea.c[j].sym) sea.c[j].sym = (uint8_t)(5 + iabs(j - s) * 5);
            sea.c[j].symv = (uint8_t)imax(sea.c[j].symv, 70 / iabs(j - s));
        }
    sea_ring(s, 256);
    sea.c[s].age = 0; sea.c[s].stress = 0; sea.c[s].brk = 0; sea.c[s].jolt = 0; sea.c[s].cool = 0;
}
/* crystal s tapped: by a key (hard: a well-grown one under stress breaks) or by the drip */
static void sea_tap(int s, int32_t vel, int32_t add, int hard)
{
    int w = sea_what(s);
    int32_t g;
    sea.tok = imax(0, sea.tok - 2);
    if (!w) { ri_strike(RI_SEAM, s, vel / 3); return; }
    if (w == SEA_STONE) { ri_strike(RI_SEAM, s, vel); sea_ring(s, 128); return; }
    g = sea_g(s);
    if (hard && g >= 192 && sea.c[s].stress + add >= sea_thr(s)) { sea_break(s, vel); return; }
    ri_strike(RI_SEAM, s, vel * (128 + g / 2) >> 8);
    sea.c[s].cool = 0;
    sea.c[s].stress = (int16_t)imin(1000, sea.c[s].stress + add);
    if (hard) sea.c[s].jolt = 1;
    sea_ring(s, 128);
}
/* shard i has landed on crystal j, `ac` px off its middle */
static void sea_land(int i, int j, int32_t ac)
{
    static const uint16_t PADD[4] = {256, 170, 512, 90};
    static const uint8_t REST[4] = {110, 90, 140, 0};
    rinst_t *r = &ri[RI_SEAM];
    const rpart_t *p = &r->p[j];
    int w = sea_what(j), pre = r->preset & 3, big = sea.sh[i].big;
    int32_t vel = iclamp(40 + (iabs(sea.sh[i].vy) + iabs(sea.sh[i].vx) / 2) / 6 + (big == 2 ? 40 : big == 0 ? -15 : 0), 30, 150);
    int32_t add = 30 * PADD[pre] / 256;
    if (sea.sh[i].kind == SEA_AMETHYST) add = add * 3 / 2; else if (sea.sh[i].kind == SEA_SALT) add = add * 2 / 3;
    if (w == SEA_STONE) { if (sea.c[j].cool >= 9 && sea_spend(1)) { ri_strike(RI_SEAM, j, vel); sea.c[j].cool = 0; } }
    else {
        sea_add(j, add);
        sea.c[j].glow = 200;
        if (sea.c[j].cool >= 9 && sea_spend(pre == 2 ? 2 : 1)) {
            sea.c[j].cool = 0;
            if (pre == 2) ri_strike(RI_SEAM, j, vel / 2 + 20);   /* a heavy one rings the crystal itself */
            else { r->fl[j] = 255; rm_hit(RI_SEAM, SK_SHARD, p->note + (pre == 1 ? RSC[rm.scale % RSC_N][0] : 0), vel, p->size / 2, 0); }
        }
    }
    if (sea.sh[i].bnc && w != SEA_OPAL) {        /* it glances off (opal swallows it) */
        sea.sh[i].bnc--; sea.sh[i].src = (uint8_t)j;
        sea.sh[i].vy = -(sea.sh[i].vy * REST[pre] >> 8) - 60;
        sea.sh[i].vx = sea.sh[i].vx / 2 + (ac >= 0 ? 1 : -1) * (50 + (int32_t)(ac_rnd() % 70u));
        sea.sh[i].y -= Q8(2);
    } else { sea.sh[i].on = 2; sea.sh[i].life = 0; }
}
static uint32_t sea_drip_per(void)
{
    static const uint8_t PACE[8] = {32, 16, 12, 8, 6, 4, 3, 2};
    return (uint32_t)PACE[ri[RI_SEAM].sq[0] & 7u] * TICKS_STEP;
}
static int sea_drip_at(void) { const rinst_t *r = &ri[RI_SEAM]; return (r->sq[1] + (r->sq[3] & 3) * sea.drip_n) % RP_N; }

static void sea_step(void)                       /* 10 ms */
{
    static const uint8_t GRAV[4] = {12, 7, 16, 9}, REST[4] = {110, 90, 140, 0};
    rinst_t *r = &ri[RI_SEAM];
    int s, i, j, nbrk = 0, pre = r->preset & 3, six;
    int32_t dtq = iclamp((int32_t)(rm.tq8 - rm.ptq8), 0, (int32_t)(TICKS_BEAT << 8)), lean = sea_lean(), k2 = ri_k(RI_SEAM, 1), r4 = ri_k(RI_SEAM, 3);
    if (!sea.inited) sea_init();
    if (rm.edit && rm.focus == RI_SEAM && !r->run) return;   /* open for planting: the world holds still */
    six = rm_cross(TICKS_STEP, 0, 0);
    sea.tick++;
    if (sea.tick % 5u == 0 && sea.tok < 8) sea.tok++;    /* at most some twenty sounds a second, eight at once */
    if (sea.drip_t > 0) sea.drip_t -= 10;
    if (sea.shake_t > 0) sea.shake_t -= 10;
    for (s = 0; s < RP_N; s++) {
        int w = sea_what(s), full;
        int32_t per, g, thr;
        if (sea.c[s].brk < 9000) sea.c[s].brk += 10;
        if (sea.c[s].cool < 250) sea.c[s].cool++;
        sea.c[s].glow = (uint8_t)(sea.c[s].glow > 8 ? sea.c[s].glow - 8 : 0);
        if (sea.tick % 3u == 0 && sea.c[s].stress > 0) sea.c[s].stress--;   /* stress eases: a third of a break's worth a second */
        if (!w || w == SEA_STONE) { sea.c[s].sym = sea.c[s].symv = 0; sea.c[s].jolt = 0; continue; }
        per = sea_per(s);
        if (r->run && sea.c[s].age < per) sea.c[s].age = imin(per, sea.c[s].age + dtq);
        full = sea.c[s].age + dtq + 256 >= per;
        if (r->run && six && full) { sea.c[s].stress = (int16_t)imin(1000, sea.c[s].stress + k2 * 2); sea.c[s].jolt = 1; }   /* a sixteenth: it strains by itself */
        g = sea_g(s);
        if (sea.c[s].sym && !--sea.c[s].sym) {   /* a neighbour's ring has reached it */
            int32_t v = sea.c[s].symv;
            sea.c[s].symv = 0; sea.c[s].glow = 255;
            sea_add(s, v);
            if (v >= 24 && g >= 64 && sea.c[s].cool >= 20 && sea.tok >= 4 && sea_spend(1)) {   /* a strong one is heard, when there is room */
                sea.c[s].cool = 0; r->fl[s] = 255;
                rm_hit(RI_SEAM, SK_SHARD, r->p[s].note, 40 + v, r->p[s].size, 0);
            }
        }
        if (sea.c[s].jolt) {
            thr = sea_thr(s);
            if ((full && sea.c[s].stress >= thr) || (g >= 128 && sea.c[s].stress >= thr * 2)) {
                if (nbrk < 3 && sea_spend(2)) { sea_break(s, 150 + imin(60, (sea.c[s].stress - thr) / 2)); nbrk++; }   /* (else it waits a moment: too much at once) */
            } else sea.c[s].jolt = 0;
        }
    }
    if (r->seq && r->run && rm_cross(sea_drip_per(), 0, 0)) {   /* the drip */
        int32_t bx, by, a, h;
        s = sea_drip_at();
        sea_geo(s, &bx, &by, &a); h = sea_h(s, sea_g(s));
        sea.drip_x = bx + ((h * isin(a)) >> 14); sea.drip_y = by - ((h * icos(a)) >> 14); sea.drip_t = 180;
        sea_tap(s, 60 + r->sq[2] * 22, r->sq[2] * 14, 0);
        sea.drip_n++;
    }
    for (i = 0; i < SEA_NSH; i++) {              /* the shards */
        int32_t px, py, gyv;
        if (!sea.sh[i].on) continue;
        if (sea.sh[i].on == 2) { if (++sea.sh[i].life > 12) sea.sh[i].on = 0; continue; }
        sea.sh[i].vy += GRAV[pre]; sea.sh[i].vx += lean;
        sea.sh[i].x += sea.sh[i].vx; sea.sh[i].y += sea.sh[i].vy; sea.sh[i].rot = (uint16_t)(sea.sh[i].rot + sea.sh[i].vrot);
        px = sea.sh[i].x >> 8; py = sea.sh[i].y >> 8;
        if (px < -6 || px > 245 || ++sea.sh[i].life > 600) { sea.sh[i].on = 0; continue; }
        if (sea.sh[i].vy > 0) {                  /* falling: onto a crystal? */
            int s0 = (int)((iclamp(px, 0, 239) - 8) / 14);
            for (j = s0 - 1; j <= s0 + 1; j++) {
                int32_t bx, by, a, ux, uy, al, ac, dx, dy;
                int w;
                if (j < 0 || j >= RP_N || j == sea.sh[i].src || !(w = sea_what(j))) continue;
                sea_geo(j, &bx, &by, &a); ux = isin(a); uy = -icos(a);
                dx = px - bx; dy = py - by;
                al = (dx * ux + dy * uy) >> 14; ac = (dy * ux - dx * uy) >> 14;
                if (al < 0 || al > sea_h(j, sea_g(j)) + 1 || iabs(ac) > sea_hw(j) + (w == SEA_AMETHYST ? 3 : 1)) continue;
                sea_land(i, j, ac);
                break;
            }
            if (j <= s0 + 1) continue;
        }
        gyv = sea_gy(iclamp(px, 0, 239));
        if (py >= gyv) {                         /* the rock: a bounce, or it lies there and is gone */
            if (sea.sh[i].bnc && sea.sh[i].vy > 150) {
                sea.sh[i].bnc--; sea.sh[i].y = Q8(gyv) - 1; sea.sh[i].vy = -(sea.sh[i].vy * REST[pre] >> 8); sea.sh[i].vx = sea.sh[i].vx * 3 / 4; sea.sh[i].src = 255;
            } else { sea.sh[i].on = 2; sea.sh[i].life = 0; sea.sh[i].y = Q8(gyv - 1); }
        }
    }
}
static void sea_key(int i, int vel) { sea_tap(i, vel, 90, 1); }
static void sea_top(int i)                       /* a shake: the rock there jolts, every crystal near is stressed, chips fly */
{
    int32_t x = 15 + iclamp(i, 0, 10) * 21;
    int s;
    for (s = 0; s < RP_N; s++) {
        int32_t d = iabs(15 + s * 14 - x);
        if (d < 40) { sea_add(s, 100 - d * 2); sea.c[s].glow = 255; }
    }
    sea.shake_t = 250; sea.shake_x = x;
    sea_throw(x, sea_gy(x) - 2, 2 + (int)(ri_k(RI_SEAM, 2) / 48), SEA_NONE, 255, 220);
    sfx(38, 0, 70, W_NOISE, 700, 0, 40);
}
static void sea_play(int on) { if (!on) sfx_ex(50, 0, 300, W_SAW, 1200, 2, 60, 0, 0, 65200); else snd_ui(72); }

/* a pointed blade from (bx, by) along angle a: length h, half width hw, its shoulders at sh/8 of the way up */
static void sea_blade(int32_t bx, int32_t by, int32_t a, int32_t h, int32_t hw, int32_t sh, int32_t fa, col_t c, col_t line)
{
    int32_t ux = isin(a), uy = -icos(a), xy[10], hs = h * sh / 8;
#define SX(al, ac) (bx + (((al) * ux - (ac) * uy) >> 14))
#define SY(al, ac) (by + (((al) * uy + (ac) * ux) >> 14))
    xy[0] = SX(0, -hw); xy[1] = SY(0, -hw); xy[2] = SX(hs, -hw); xy[3] = SY(hs, -hw); xy[4] = SX(h, 0); xy[5] = SY(h, 0);
    xy[6] = SX(hs, hw); xy[7] = SY(hs, hw); xy[8] = SX(0, hw); xy[9] = SY(0, hw);
    galpha(fa); gpoly(xy, 5, c); galpha(256); gpolyline(xy, 5, 0, line);
    if (h > 9) { galpha(120); gline(xy[4], xy[5], SX(hs, hw / 3), SY(hs, hw / 3), line); gline(SX(hs, hw / 3), SY(hs, hw / 3), SX(0, hw / 3), SY(0, hw / 3), line); galpha(256); }   /* a facet's edge */
}
/* crystal s, grown g of 256; still: stopped for planting (no glow, no tremble) */
static void sea_draw_one(int s, int32_t g, int still)
{
    const rinst_t *r = &ri[RI_SEAM];
    int w = sea_what(s), k;
    int32_t bx, by, a, ux, uy, h = sea_h(s, g), hw = sea_hw(s), hot = r->fl[s], sg, fa = 70 + hot * 186 / 256, xy[24], mx, my;
    col_t c = sea_col(w), line = hot > 120 ? th->text : c;
    if (!w) return;
    sea_geo(s, &bx, &by, &a);
    sg = w == SEA_STONE || still ? 0 : imin(256, sea.c[s].stress * 256 / sea_thr(s));
    if (sg > 150) bx += (tsin(rm.t, 700, s * 9000) * (sg - 110)) >> 21;   /* near breaking: it trembles */
    ux = isin(a); uy = -icos(a);
    mx = SX(h / 2, 0); my = SY(h / 2, 0);
    if (hot) { galpha(hot * 110 / 256); gdisc(mx, my, 5 + h / 5, c); galpha(256); }
    if (sea.c[s].glow && !still) { galpha(sea.c[s].glow / 2); gcirc(mx, my, hw + 3 + (255 - sea.c[s].glow) / 40, ri_col(RI_SEAM)); galpha(256); }
    switch (w) {
    case SEA_QUARTZ:
        if (h > 12) sea_blade(SX(0, hw + 1), SY(0, hw + 1), a + 3600, h * 2 / 5, 2, 5, fa, c, line);   /* a small one beside it */
        sea_blade(bx, by, a, h, hw, 6, fa, c, line);
        break;
    case SEA_AMETHYST:                           /* a cluster: three stubby points */
        sea_blade(SX(0, -2), SY(0, -2), a - 3300, h * 5 / 8, hw, 4, fa, c, line);
        sea_blade(SX(0, 2), SY(0, 2), a + 3000, h * 3 / 4, hw, 4, fa, c, line);
        sea_blade(bx, by, a, h, hw + 1, 4, fa, c, line);
        break;
    case SEA_SALT: {                             /* cubes, one on another */
        int32_t q = 5 + (r->p[s].size & 7) / 3, n = h / q;
        for (k = 0; k <= n; k++) {
            int32_t z = k < n ? q : h - n * q, off = k == 0 ? 0 : (k & 1) ? 2 : -2, x = SX(k * q, off) - q / 2, y = SY(k * q, off) - z;
            if (z < 2) continue;
            galpha(fa); grect(x, y, k < n ? q + 1 : z + 1, z + 1, c); galpha(256); gbox(x, y, k < n ? q + 1 : z + 1, z + 1, line);
            if (z >= 5) gp(x + 2, y + 2, th->text);
        }
        break; }
    case SEA_OPAL:                               /* an egg, colours moving in it */
        for (k = 0; k < 12; k++) {
            int32_t t = k * 5461, al = (h * (16384 - icos(t))) >> 15, ac = (hw * isin(t)) >> 14;
            xy[2 * k] = SX(al, ac); xy[2 * k + 1] = SY(al, ac);
        }
        galpha(fa); gpoly(xy, 12, c); galpha(256);
        if (h > 8) for (k = 0; k < 2; k++) {
            int32_t sw = tsin(rm.t, 16 + s, k * 30000 + s * 7000) * (hw / 2) >> 14;
            galpha(150); gdisc(SX(h * (1 + k) / 3, sw), SY(h * (1 + k) / 3, sw), imax(1, hw / 3), th->caps[(rm.t / 280u + (uint32_t)(s + k * 2)) % 5u]); galpha(256);
        }
        gpolyline(xy, 12, 1, line);
        break;
    default:                                     /* stone: a squat lump */
        xy[0] = SX(0, -hw); xy[1] = SY(0, -hw); xy[2] = SX(h * 5 / 8, -hw + 1); xy[3] = SY(h * 5 / 8, -hw + 1); xy[4] = SX(h, -hw / 3); xy[5] = SY(h, -hw / 3);
        xy[6] = SX(h * 7 / 8, hw / 2); xy[7] = SY(h * 7 / 8, hw / 2); xy[8] = SX(h / 2, hw); xy[9] = SY(h / 2, hw); xy[10] = SX(0, hw); xy[11] = SY(0, hw);
        gpoly(xy, 6, th->bg); galpha(60 + hot * 190 / 256); gpoly(xy, 6, c); galpha(256); gpolyline(xy, 6, 0, line);
        gline(xy[4], xy[5], SX(h / 3, 0), SY(h / 3, 0), th->mute);
        break;
    }
    if (sg > 20) {                               /* stress: a glow at its heart, then cracks */
        galpha(sg * 120 / 256); gdisc(mx, my, 1 + h / 12, th->caps[0]); galpha(256);
        if (sg > 128 && h > 10) { galpha(sg); gline(SX(h * 3 / 4, -hw / 2), SY(h * 3 / 4, -hw / 2), mx, my, th->text); gline(mx, my, SX(h / 4, hw / 2), SY(h / 4, hw / 2), th->text); galpha(256); }
    }
    if (sea.c[s].brk < 320 && !still) {          /* just broken: a flash, and a ring going out from where its tip was */
        int32_t hm = sea_hmax(s), tx = SX(hm * 2 / 3, 0), ty = SY(hm * 2 / 3, 0);
        if (sea.c[s].brk < 90) { galpha(220 - sea.c[s].brk * 2); gdisc(tx, ty, 6, th->text); galpha(256); }
        galpha(256 - sea.c[s].brk * 3 / 4); gcirc(tx, ty, 4 + sea.c[s].brk / 22, th->text); galpha(256);
    }
#undef SX
#undef SY
}
static void sea_draw(int edit)
{
    const rinst_t *r = &ri[RI_SEAM];
    int still = edit && !r->run, s, i;
    int32_t xy[42];
    col_t c = ri_col(RI_SEAM);
    /* the rock, and the seam along its top */
    xy[0] = 0; xy[1] = sea_gy(0);
    for (i = 0; i <= 16; i++) {
        int32_t x = 8 + i * 14, jig = sea.shake_t > 0 && iabs(x - sea.shake_x) < 44 ? (tsin(rm.t, 900, i * 20000) * sea.shake_t) >> 21 : 0;
        xy[2 + 2 * i] = x; xy[3 + 2 * i] = sea_gy(x) + (int32_t)(ihash((uint32_t)i * 31u + 7u) % 5u) - 2 + jig;
    }
    xy[36] = 239; xy[37] = sea_gy(239); xy[38] = 239; xy[39] = 199; xy[40] = 0; xy[41] = 199;
    gpoly(xy, 21, th->panel);
    galpha(70);
    for (i = 1; i <= 2; i++) gline(0, imin(199, sea_gy(0) + i * 11), 239, imin(199, sea_gy(239) + i * 11 + 3), th->mute);
    galpha(256);
    gpolyline(xy, 19, 0, th->mute);
    galpha(110); for (i = 0; i < 18; i++) gline(xy[2 * i], xy[2 * i + 1] + 2, xy[2 * i + 2], xy[2 * i + 3] + 2, c); galpha(256);
    if (edit) for (s = 0; s < RP_N; s++) if (!sea_what(s)) gline(13 + s * 14, sea_gy(15 + s * 14) - 2, 17 + s * 14, sea_gy(15 + s * 14) - 2, th->mute);   /* a bare place */
    for (s = 0; s < RP_N; s++) sea_draw_one(s, still ? 256 : sea_g(s), still);
    if (r->seq && !still) {                      /* the drip: a bead swells under the roof, then falls onto the crystal in time */
        uint32_t per = sea_drip_per(), ph = (rm.tq8 >> 8) % per, fw = per < 40u ? per : 40u;
        int32_t bx, by, a, h, tx, ty;
        col_t wc = th->caps[2];
        s = sea_drip_at();
        sea_geo(s, &bx, &by, &a); h = sea_h(s, sea_g(s));
        tx = bx + ((h * isin(a)) >> 14); ty = by - ((h * icos(a)) >> 14);
        xy[0] = tx - 4; xy[1] = 13; xy[2] = tx + 4; xy[3] = 13; xy[4] = tx; xy[5] = 19;
        gpoly(xy, 3, th->panel); gpolyline(xy, 3, 0, th->mute);
        if (!r->run) gdisc(tx, 21, 1, wc);
        else if (ph < per - fw) gdisc(tx, 21, 1 + (int32_t)(ph * 2u / (per - fw)), wc);
        else {
            int32_t f = (int32_t)((ph - (per - fw)) * 256u / fw), y = 21 + (((ty - 23) * f >> 8) * f >> 8);
            gline(tx, y - 4, tx, y, wc); gdisc(tx, y, 2, wc); gp(tx - 1, y - 1, th->text);
        }
    }
    if (sea.drip_t > 0 && !still) {              /* its splash */
        int32_t e = (180 - sea.drip_t) / 12;
        galpha(sea.drip_t * 256 / 180);
        garc(sea.drip_x, sea.drip_y, 3 + e, 2 + e / 2, 32768, 65536, th->caps[2], 0);
        gp(sea.drip_x - 3 - e, sea.drip_y - 2 - e / 2, th->text); gp(sea.drip_x + 3 + e, sea.drip_y - 2 - e / 2, th->text);
        galpha(256);
    }
    for (i = 0; i < SEA_NSH; i++) {              /* the shards: small tumbling slivers */
        int32_t x = sea.sh[i].x >> 8, y = sea.sh[i].y >> 8, len, dx, dy;
        col_t sc = sea_col(sea.sh[i].kind);
        if (!sea.sh[i].on || y < 13 || still) continue;
        if (sea.sh[i].on == 2) {                 /* landed: a spark going out */
            int32_t e = 1 + sea.sh[i].life / 4;
            galpha(256 - sea.sh[i].life * 20);
            gp(x - e, y, th->text); gp(x + e, y, th->text); gp(x, y - e, th->text); gp(x, y, sc);
            galpha(256);
            continue;
        }
        len = sea.sh[i].big == 2 ? 4 : sea.sh[i].big ? 2 : 0;
        dx = (len * icos(sea.sh[i].rot)) >> 14; dy = (len * isin(sea.sh[i].rot)) >> 14;
        if (!len) { gp(x, y, th->text); gp(x + 1, y, sc); }
        else if (sea.sh[i].big == 2) {
            xy[0] = x - dx; xy[1] = y - dy; xy[2] = x + dx; xy[3] = y + dy; xy[4] = x - dy / 2 - 1; xy[5] = y + dx / 2 + 1;
            galpha(170); gpoly(xy, 3, sc); galpha(256); gpolyline(xy, 3, 1, th->text);
        } else { gline(x - dx, y - dy, x + dx, y + dy, sc); gp(x + dx, y + dy, th->text); }
    }
    if (edit && still) {                         /* open: the parts numbered, those in hand marked */
        for (s = 0; s < RP_N; s++) {
            char n[3];
            int on = r->hand >> s & 1;
            int32_t bx = 15 + s * 14, by = sea_gy(bx);
            n[0] = s >= 9 ? '1' : (char)('1' + s); n[1] = s >= 9 ? (char)('0' + s - 9) : 0; n[2] = 0;
            gtextc(bx + 1, by + 6, n, on ? th->text : th->mute, 1);
            if (on) {
                int32_t top = by - imax(14, sea_hmax(s)) - 6;
                gline(bx - 7, top, bx - 7, by + 14, th->text); gline(bx + 7, top, bx + 7, by + 14, th->text);
                gline(bx - 7, top, bx - 4, top, th->text); gline(bx + 4, top, bx + 7, top, th->text);
                gline(bx - 7, by + 14, bx + 7, by + 14, th->text);
            }
        }
        if ((r->hand & (r->hand - 1)) == 0) {    /* one part in hand: what it is */
            char nb[6];
            s = 0;
            while (s < RP_N - 1 && !(r->hand >> s & 1u)) s++;
            gtextc(120, 20, SEA_KINDS[sea_what(s)], sea_col(sea_what(s)), 2);
            rm_note_name(nb, rm_note(RI_SEAM, r->p[s].note));
            gtextc(120, 38, nb, th->text, 1);
        }
    }
}

/* ---- its lesson: the seam itself, played by the lesson */
static void sl_reset(void)
{
    rinst_t *r = &ri[RI_SEAM];
    int i;
    ri_load_algo(RI_SEAM, 1);
    r->k[0] = 5; r->k[1] = 8; r->k[2] = 0; r->k[3] = 0; r->seq = 0; r->arp = 0; r->run = 1; r->sel = 7; r->preset = 0; r->oct = 0;
    for (i = 0; i < 4; i++) { r->lfo[i] = 0; r->env[i] = 4; r->sq[i] = SEA_SQDEF[i]; }
    rm.edit = 0; rm.menu = 0;
    sea_clear();
}
static void sl_say(const char *s) { rt.label = s; rt.label_t = 1000; }
static void sl_1(int32_t t, int32_t dt)
{
    if (RT_FIRST) sl_reset();
    rt.icon = RTI_K1;
    if (RT_AT(2600)) ri[RI_SEAM].k[0] = 6;
    if (RT_AT(4000)) ri[RI_SEAM].k[0] = 7;
}
static void sl_2(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_SEAM];
    if (RT_FIRST) { r->k[0] = 7; r->k[1] = 0; }
    rt.icon = t < 2600 ? RTI_K2 : t < 5400 ? RTI_LOW : RTI_K2;
    if (RT_AT(2800) || RT_AT(3300)) sea_key(2, 200);
    if (RT_AT(3900) || RT_AT(4400)) sea_key(8, 200);
    if (RT_AT(4800) || RT_AT(5200)) sea_key(11, 200);
    if (RT_AT(5600)) r->k[1] = 6;
}
static void sl_3(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_SEAM];
    if (RT_FIRST) { r->k[0] = 6; r->k[1] = 5; r->k[2] = 3; }
    rt.icon = t < 3800 ? RTI_K3 : RTI_SELECT;
    if (RT_AT(1800)) r->k[2] = 6;
    if (RT_AT(4000)) r->sel = 10;
    if (RT_AT(5000)) r->sel = 13;
}
static void sl_4(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_SEAM];
    if (RT_FIRST) { r->sel = 9; r->k[2] = 4; r->k[3] = 4; }
    rt.icon = t < 3000 ? RTI_K4 : RTI_PRESETS;
    if (RT_AT(1500)) r->k[3] = 7;
    if (RT_AT(3200)) { r->preset = 1; sl_say(SEA_PRE[1]); }
    if (RT_AT(4400)) { r->preset = 2; sl_say(SEA_PRE[2]); }
    if (RT_AT(5600)) { r->preset = 3; sl_say(SEA_PRE[3]); }
}
static void sl_5(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_SEAM];
    if (RT_FIRST) { r->preset = 0; r->k[0] = 3; r->k[1] = 0; r->k[2] = 3; r->k[3] = 2; r->seq = 1; r->sq[0] = 6; r->sq[1] = 2; r->sq[3] = 0; sea.drip_n = 0; }
    rt.icon = t < 3000 ? RTI_BTN + BTN_SEQ : t < 5000 ? RTI_K4 : RTI_K1;
    if (RT_AT(3200)) r->sq[3] = 2;
    if (RT_AT(5200)) r->sq[0] = 7;
}
static void sl_6(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_SEAM];
    int i;
    if (RT_FIRST) { r->seq = 0; rm.edit = 1; r->run = 0; r->hand = 1u << 2; }
    rt.icon = t < 1200 ? RTI_BTN + BTN_EDIT : t < 2000 ? RTI_LOW : t < 5200 ? RTI_K1 : t < 6300 ? RTI_K2 : t < 7300 ? RTI_K3 : RTI_K4;
    if (RT_AT(1300)) { r->hand = 1u << 5; ri_strike(RI_SEAM, 5, 150); }
    for (i = 1; i < SEA_NK; i++) if (RT_AT(1500 + i * 600)) { r->p[5].what = (uint8_t)i; r->p[5].size = 2; r->p[5].note = 3; ri_strike(RI_SEAM, 5, 150); }
    if (RT_AT(5300)) { r->p[5].what = SEA_QUARTZ; ri_strike(RI_SEAM, 5, 150); }
    if (RT_AT(5500) || RT_AT(5800) || RT_AT(6100)) { r->p[5].size++; ri_strike(RI_SEAM, 5, 150); }
    if (RT_AT(6500) || RT_AT(6800) || RT_AT(7100)) { r->p[5].note += 2; ri_strike(RI_SEAM, 5, 150); }
    if (RT_AT(7500) || RT_AT(7900)) { r->p[5].qual += 2; ri_strike(RI_SEAM, 5, 150); }
}
static void sl_7(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_SEAM];
    if (RT_FIRST) { rm.edit = 0; r->run = 1; r->k[0] = 5; r->k[1] = 2; r->k[2] = 5; r->k[3] = 3; r->sel = 8; }
    rt.icon = t < 2200 ? RTI_TOP : t < 3400 ? RTI_BTN + BTN_REC : t < 4600 ? RTI_BTN + BTN_SAVE : RTI_BTN + BTN_HOME;
    if (RT_AT(600)) sea_top(2);
    if (RT_AT(1300)) sea_top(7);
    if (RT_AT(1900)) sea_top(4);
}
static const rt_step_t SEA_LESSON[7] = {
    {6000, {"CRYSTALS GROW OUT OF THE ROCK. A FULL-GROWN ONE BREAKS, RINGS ITS NOTE AND GROWS AGAIN. KNOB 1: HOW FAST THEY GROW.",
            "KRISTALLE WACHSEN AUS DEM FELS. EIN AUSGEWACHSENER BRICHT, KLINGT MIT SEINEM TON UND WÄCHST NEU. KNOB 1: WIE SCHNELL SIE WACHSEN.",
            "KRYSTALLER GROR UD AF KLIPPEN. EN UDVOKSET KRYSTAL KNÆKKER, KLINGER MED SIN TONE OG GROR IGEN. KNOB 1: HVOR HURTIGT DE GROR.",
            "DES CRISTAUX POUSSENT DANS LA ROCHE. ARRIVÉ À SA TAILLE, UN CRISTAL CASSE, SONNE SA NOTE ET REPOUSSE. KNOB 1 : LEUR VITESSE.",
            "DE LA ROCA CRECEN CRISTALES. CUANDO UNO HA CRECIDO DEL TODO, SE ROMPE, SUENA SU NOTA Y VUELVE A CRECER. KNOB 1: LO RÁPIDO QUE CRECEN.",
            "KAYADAN KRİSTALLER BÜYÜR. TAM BÜYÜYEN KRİSTAL KIRILIR, NOTASINI ÇALAR VE YENİDEN BÜYÜR. KNOB 1: NE KADAR HIZLI BÜYÜDÜKLERİ.",
            "KRISTALLEN GROEIEN UIT DE ROTS. EEN VOLGROEIDE BREEKT, LAAT ZIJN NOOT KLINKEN EN GROEIT OPNIEUW. KNOB 1: HOE SNEL ZE GROEIEN."}, sl_1},
    {7000, {"EACH CRYSTAL IS ITS OWN CLOCK: A BIG ONE TAKES LONGER. KNOB 2 IS THE STRESS. AT NOTHING THEY WAIT, FULL-GROWN, UNTIL SOMETHING HITS THEM. THE BOTTOM ROW TAPS THEM.",
            "JEDER KRISTALL IST SEINE EIGENE UHR: EIN GROSSER BRAUCHT LÄNGER. KNOB 2 IST DIE SPANNUNG. BEI NULL WARTEN SIE AUSGEWACHSEN, BIS ETWAS SIE TRIFFT. DIE UNTERE REIHE KLOPFT SIE AN.",
            "HVER KRYSTAL ER SIT EGET UR: EN STOR TAGER LÆNGERE TID. KNOB 2 ER SPÆNDINGEN. VED NUL VENTER DE UDVOKSEDE, TIL NOGET RAMMER DEM. DEN NEDERSTE RÆKKE BANKER PÅ DEM.",
            "CHAQUE CRISTAL EST SA PROPRE HORLOGE : UN GRAND MET PLUS DE TEMPS. KNOB 2 EST LA TENSION. À ZÉRO, ILS ATTENDENT QU'ON LES TOUCHE. LA RANGÉE DU BAS LES FRAPPE.",
            "CADA CRISTAL ES SU PROPIO RELOJ: UNO GRANDE TARDA MÁS. KNOB 2 ES LA TENSIÓN. A CERO ESPERAN, YA CRECIDOS, A QUE ALGO LOS GOLPEE. LA FILA DE ABAJO LOS TOCA.",
            "HER KRİSTAL KENDİ SAATİDİR: BÜYÜK OLAN DAHA UZUN SÜRER. KNOB 2 GERİLİMDİR. SIFIRDA, BÜYÜMÜŞ HALDE BİR ŞEY ÇARPANA DEK BEKLERLER. ALT SIRA ONLARA VURUR.",
            "ELK KRISTAL IS ZIJN EIGEN KLOK: EEN GROOT DOET ER LANGER OVER. KNOB 2 IS DE SPANNING. OP NUL WACHTEN ZE VOLGROEID TOT IETS ZE RAAKT. DE ONDERSTE RIJ TIKT ZE AAN."}, sl_2},
    {6500, {"KNOB 3: A BREAKING CRYSTAL THROWS SHARDS. WHERE A SHARD LANDS, THAT CRYSTAL RINGS AND IS STRESSED: BREAKS SET OFF BREAKS. SELECT LEANS THE SEAM, AND THE SHARDS FALL THAT WAY.",
            "KNOB 3: EIN BRECHENDER KRISTALL WIRFT SPLITTER. WO EINER LANDET, KLINGT DER KRISTALL UND GERÄT UNTER SPANNUNG: BRUCH LÖST BRUCH AUS. SELECT NEIGT DIE ADER, DORTHIN FALLEN DIE SPLITTER.",
            "KNOB 3: EN KRYSTAL, DER KNÆKKER, KASTER SPLINTER. HVOR EN SPLINT LANDER, KLINGER KRYSTALLEN OG KOMMER I SPÆNDING: BRUD UDLØSER BRUD. SELECT HÆLDER ÅREN, OG DERHEN FALDER SPLINTERNE.",
            "KNOB 3 : UN CRISTAL QUI CASSE LANCE DES ÉCLATS. LÀ OÙ UN ÉCLAT TOMBE, LE CRISTAL SONNE ET SE TEND : UNE CASSE EN ENTRAÎNE D'AUTRES. SELECT PENCHE LE FILON, LES ÉCLATS TOMBENT DE CE CÔTÉ.",
            "KNOB 3: UN CRISTAL QUE SE ROMPE LANZA ESQUIRLAS. DONDE CAE UNA, ESE CRISTAL SUENA Y SE TENSA: UNA ROTURA PROVOCA OTRAS. SELECT INCLINA LA VETA, Y LAS ESQUIRLAS CAEN HACIA ESE LADO.",
            "KNOB 3: KIRILAN KRİSTAL KIYMIK SAÇAR. KIYMIĞIN DÜŞTÜĞÜ KRİSTAL ÇINLAR VE GERİLİR: KIRILMA KIRILMAYI BAŞLATIR. SELECT DAMARI EĞER, KIYMIKLAR O YANA DÜŞER.",
            "KNOB 3: EEN BREKEND KRISTAL GOOIT SCHERVEN. WAAR EEN SCHERF LANDT, KLINKT DAT KRISTAL EN KOMT HET ONDER SPANNING: BREUK LOKT BREUK UIT. SELECT KANTELT DE ADER, DIE KANT VALLEN DE SCHERVEN OP."}, sl_3},
    {7000, {"KNOB 4: A RINGING CRYSTAL SETS ITS NEIGHBOURS AND THE NOTES THAT GO WITH IT RINGING TOO. PRESETS CHANGE THE SHARDS: CLEAR, FROST, HEAVY, DUST.",
            "KNOB 4: EIN KLINGENDER KRISTALL BRINGT SEINE NACHBARN UND DIE VERWANDTEN TÖNE ZUM MITKLINGEN. PRESETS ÄNDERT DIE SPLITTER: CLEAR, FROST, HEAVY, DUST.",
            "KNOB 4: EN KRYSTAL, DER KLINGER, FÅR SINE NABOER OG DE BESLÆGTEDE TONER TIL AT KLINGE MED. PRESETS ÆNDRER SPLINTERNE: CLEAR, FROST, HEAVY, DUST.",
            "KNOB 4 : UN CRISTAL QUI SONNE FAIT SONNER SES VOISINS ET LES NOTES PARENTES. PRESETS CHANGE LES ÉCLATS : CLEAR, FROST, HEAVY, DUST.",
            "KNOB 4: UN CRISTAL QUE SUENA HACE SONAR A SUS VECINOS Y A LAS NOTAS AFINES. PRESETS CAMBIA LAS ESQUIRLAS: CLEAR, FROST, HEAVY, DUST.",
            "KNOB 4: ÇINLAYAN KRİSTAL KOMŞULARINI VE AKRABA NOTALARI DA ÇINLATIR. PRESETS KIYMIKLARI DEĞİŞTİRİR: CLEAR, FROST, HEAVY, DUST.",
            "KNOB 4: EEN KLINKEND KRISTAL LAAT ZIJN BUREN EN DE VERWANTE NOTEN MEEKLINKEN. PRESETS VERANDERT DE SCHERVEN: CLEAR, FROST, HEAVY, DUST."}, sl_4},
    {7500, {"SEQ STARTS THE DRIP: A DROP FALLS IN TIME AND TAPS THE CRYSTAL UNDER IT. HOLD SEQ AND TURN KNOB 1 TO 4: PACE, PLACE, FORCE, AND WALK, WHICH MOVES IT ALONG AFTER EVERY DROP.",
            "SEQ STARTET DEN TROPFEN: ER FÄLLT IM TAKT UND KLOPFT DEN KRISTALL DARUNTER AN. SEQ HALTEN UND KNOB 1 BIS 4 DREHEN: TEMPO, ORT, KRAFT UND WANDERN NACH JEDEM TROPFEN.",
            "SEQ STARTER DRYPPET: EN DRÅBE FALDER I TAKT OG BANKER PÅ KRYSTALLEN UNDER SIG. HOLD SEQ OG DREJ KNOB 1 TIL 4: TEMPO, STED, KRAFT OG VANDRING EFTER HVER DRÅBE.",
            "SEQ LANCE LA GOUTTE : ELLE TOMBE EN MESURE ET FRAPPE LE CRISTAL DU DESSOUS. TIENS SEQ ET TOURNE KNOB 1 À 4 : CADENCE, PLACE, FORCE, ET MARCHE, QUI LA DÉPLACE APRÈS CHAQUE GOUTTE.",
            "SEQ INICIA LA GOTA: CAE A COMPÁS Y TOCA EL CRISTAL QUE TIENE DEBAJO. MANTÉN SEQ Y GIRA KNOB 1 A 4: RITMO, LUGAR, FUERZA Y PASEO, QUE LA MUEVE TRAS CADA GOTA.",
            "SEQ DAMLAYI BAŞLATIR: DAMLA TEMPOYLA DÜŞER VE ALTINDAKİ KRİSTALE VURUR. SEQ'İ BASILI TUT, KNOB 1-4'Ü ÇEVİR: HIZ, YER, GÜÇ VE HER DAMLADAN SONRA YÜRÜYÜŞ.",
            "SEQ START DE DRUPPEL: DIE VALT IN DE MAAT EN TIKT HET KRISTAL ERONDER AAN. HOUD SEQ VAST EN DRAAI KNOB 1 TOT 4: TEMPO, PLEK, KRACHT EN WANDELEN NA ELKE DRUPPEL."}, sl_5},
    {8500, {"EDIT STOPS THE SEAM. HOLD A KEY OF THE BOTTOM ROW: THAT PART IS IN HAND. KNOB 1: WHAT GROWS THERE. KNOB 2: ITS SIZE, SO HOW LONG IT GROWS. KNOB 3: ITS NOTE. KNOB 4: HOW BRITTLE.",
            "EDIT HÄLT DIE ADER AN. HALTE EINE TASTE DER UNTEREN REIHE: DIESER TEIL IST IN DER HAND. KNOB 1: WAS DORT WÄCHST. KNOB 2: GRÖSSE, ALSO WIE LANGE ES WÄCHST. KNOB 3: TON. KNOB 4: WIE SPRÖDE.",
            "EDIT STANDSER ÅREN. HOLD EN TANGENT I DEN NEDERSTE RÆKKE: DEN DEL ER I HÅNDEN. KNOB 1: HVAD DER GROR. KNOB 2: STØRRELSE, ALTSÅ HVOR LÆNGE DEN GROR. KNOB 3: TONE. KNOB 4: HVOR SPRØD.",
            "EDIT ARRÊTE LE FILON. TIENS UNE TOUCHE DU BAS : CETTE PARTIE EST EN MAIN. KNOB 1 : CE QUI Y POUSSE. KNOB 2 : SA TAILLE, DONC LE TEMPS QU'IL MET. KNOB 3 : SA NOTE. KNOB 4 : SA FRAGILITÉ.",
            "EDIT DETIENE LA VETA. MANTÉN UNA TECLA DE ABAJO: ESA PARTE ESTÁ EN LA MANO. KNOB 1: LO QUE CRECE AHÍ. KNOB 2: SU TAMAÑO, O SEA CUÁNTO TARDA. KNOB 3: SU NOTA. KNOB 4: LO FRÁGIL QUE ES.",
            "EDIT DAMARI DURDURUR. ALT SIRADAN BİR TUŞU BASILI TUT: O PARÇA ELİNDE. KNOB 1: ORADA NE BÜYÜR. KNOB 2: BOYU, YANİ NE KADAR SÜREDE BÜYÜR. KNOB 3: NOTASI. KNOB 4: NE KADAR KIRILGAN.",
            "EDIT ZET DE ADER STIL. HOUD EEN TOETS VAN DE ONDERSTE RIJ VAST: DAT DEEL HEB JE IN DE HAND. KNOB 1: WAT ER GROEIT. KNOB 2: DE GROOTTE, DUS HOE LANG HET GROEIT. KNOB 3: DE NOOT. KNOB 4: HOE BROOS."}, sl_6},
    {6000, {"THE TOP ROW SHAKES THE ROCK WHERE YOU PRESS. REC RECORDS WHAT SOUNDS ONTO THE LOOP. SAVE KEEPS THE ROOM. HOME GOES BACK TO IT. NOW YOU!",
            "DIE OBERE REIHE RÜTTELT AM FELS, WO DU DRÜCKST. REC NIMMT AUF, WAS KLINGT. SAVE SICHERT DEN RAUM. HOME FÜHRT ZURÜCK. JETZT DU!",
            "DEN ØVERSTE RÆKKE RYSTER KLIPPEN, DER HVOR DU TRYKKER. REC OPTAGER DET, DER LYDER. SAVE GEMMER RUMMET. HOME GÅR TILBAGE. NU DIG!",
            "LA RANGÉE DU HAUT SECOUE LA ROCHE LÀ OÙ TU APPUIES. REC ENREGISTRE CE QUI SONNE. SAVE GARDE LA SALLE. HOME Y RETOURNE. À TOI !",
            "LA FILA DE ARRIBA SACUDE LA ROCA DONDE PULSAS. REC GRABA LO QUE SUENA. SAVE GUARDA LA SALA. HOME VUELVE A ELLA. ¡AHORA TÚ!",
            "ÜST SIRA BASTIĞIN YERDEKİ KAYAYI SARSAR. REC ÇALANI KAYDEDER. SAVE ODAYI SAKLAR. HOME ODAYA DÖNER. ŞİMDİ SEN!",
            "DE BOVENSTE RIJ SCHUDT DE ROTS WAAR JE DRUKT. REC NEEMT OP WAT KLINKT. SAVE BEWAART DE KAMER. HOME GAAT TERUG. NU JIJ!"}, sl_7},
};
static const rdesc_t SEAM_DESC = {
    "CRYSTAL SEAM", {"GROW", "STRESS", "SHARDS", "RING"}, {8, 8, 8, 8}, {4, 4, 4, 3}, {0, 0, 0, 0},
    "FALL", 14, 9,
    "SEQ: THE DRIP", {"PACE", "PLACE", "FORCE", "WALK"}, {7, 15, 8, 3}, {5, 4, 5, 0},
    SEA_KINDS, SEA_NK, SEA_SND, SEA_PRE, 4, SEA_ALGOS, 8,
    sea_init, sea_step, sea_draw, sea_key, sea_top, sea_layout, sea_play, SEA_LESSON, 7,
};
