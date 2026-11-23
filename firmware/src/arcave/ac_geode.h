/* SPDX-License-Identifier: GPL-3.0-only */
/* THE GEODE (ac_room.h has the controls every instrument shares). A round shell that turns; stones lie loose inside.
 * The shell is silent: a sound comes only when a stone, or the plectrum, meets a part of its inside. The inside is
 * 16 parts, the bottom row of keys bent into a ring with its two ends at the bottom:
 *
 *   BARE ROCK  a click                                   CRYSTAL   a clear ring; hit often enough it breaks, and regrows
 *   MUSHROOM   throws the stone back: an acid bass note  MOSS      sticky and quiet: tuned noise; it carries a stone up
 *   OOZE       the stone sinks in: a chord out of the scale
 *
 *   KNOB 1 SPIN     still, or a turn in 4 bars, 2 bars, a bar, half a bar, either way; it is tied to the clock
 *   KNOB 2 WEIGHT   how hard the stones fall        KNOB 3 GRIP   how much the shell drags them along
 *   KNOB 4 STONES   how many                        SELECT TILT   which way is down
 *   PRESETS         the stones: PEBBLE, BOULDER, SEEDS, DROP
 *   SEQ             the plectrum: it stays where it is and plucks each part that passes, so it keeps time.
 *                   Held: KNOB 1 PLACE, KNOB 2 COUNT, KNOB 3 FORCE, KNOB 4 REACH (a short one only touches tall things)
 *   bottom row      knock a part by hand            top row       flick the stones toward that side */
#define GE_CX 120
#define GE_CY 106
#define GE_R 80
#define GE_NS 8
static const uint8_t GE_DESC_SQDEF[4] = {8, 0, 5, 7};
enum { MT_BARE, MT_CRYSTAL, MT_MUSH, MT_MOSS, MT_OOZE, MT_N };
static const char *const GE_KINDS[MT_N] = {"BARE ROCK", "CRYSTAL", "MUSHROOM", "MOSS", "OOZE"};
static const uint8_t GE_SND[MT_N] = {SK_CLICK, SK_CRYSTAL, SK_ACID, SK_MOSS, SK_OOZE};
static const char *const GE_STONES[4] = {"PEBBLE", "BOULDER", "SEEDS", "DROP"};
static const char *const GE_ALGOS[8] = {"YOUR OWN", "FIRST SHELL", "BELL RING", "ACID WHEEL", "MOSS BED", "OOZE CHOIR", "HALF AND HALF", "ALL OF IT"};
static struct {
    int32_t theta, omega;                        /* the shell's turn (65536 a turn) and its speed a step */
    int32_t grow[RP_N];                          /* a crystal's length, of 256 (it regrows after breaking) */
    uint8_t hits[RP_N];
    int32_t brk[RP_N];                           /* ms since it broke (a ring going out) */
    struct { int32_t x, y, vx, vy, cool; int8_t last; } st[GE_NS];   /* Q8 px from the middle */
    int8_t pl_last[4];                           /* the part under each plectrum */
    int32_t pl_t[4];                             /* each one's twang, dying away */
    uint8_t inited;
} ge;
static col_t ge_col(int m) { return m == MT_CRYSTAL ? th->caps[3] : m == MT_MUSH ? th->caps[0] : m == MT_MOSS ? AC_RGB(110, 220, 90) : m == MT_OOZE ? th->caps[1] : th->mute; }
static int32_t ge_slot_a(int s) { return ge.theta + 16384 + s * 4096 + 2048; }   /* the middle of part s */
static int32_t ge_h(int s)                       /* how far part s stands in from the shell (px) */
{
    const rpart_t *p = &ri[RI_GEODE].p[s];
    return p->what == MT_CRYSTAL ? (6 + p->size * 2) * ge.grow[s] / 256 : p->what == MT_MUSH ? 8 + p->size : p->what == MT_MOSS ? 3 + p->size / 2 : p->what == MT_OOZE ? 4 + p->size / 2 : 0;
}
static int ge_nstones(void) { const rinst_t *r = &ri[RI_GEODE]; return imin(GE_NS, (ri_k(RI_GEODE, 3) + 8) / 16 * (r->preset == 2 ? 2 : 1)); }
static int32_t ge_srad(void) { static const uint8_t R[4] = {4, 7, 2, 3}; return R[ri[RI_GEODE].preset & 3u]; }
static void ge_init(void)
{
    int i;
    for (i = 0; i < RP_N; i++) { if (!ge.inited || ge.grow[i] <= 0) ge.grow[i] = 256; ge.hits[i] = 0; ge.brk[i] = 9999; }
    if (!ge.inited) {
        for (i = 0; i < GE_NS; i++) { ge.st[i].x = Q8(i * 9 - 30); ge.st[i].y = Q8(40 - (i & 1) * 14); ge.st[i].vx = ge.st[i].vy = 0; ge.st[i].last = -1; ge.st[i].cool = 999; }
        for (i = 0; i < 4; i++) ge.pl_last[i] = -1;
        ge.inited = 1;
    }
}
static void ge_layout(int n)
{
    rpart_t *p = ri[RI_GEODE].p;
    int s;
    for (s = 0; s < RP_N; s++) { p[s].what = MT_BARE; p[s].size = 3; p[s].note = (uint8_t)(s % 5); p[s].qual = 3; }
#define GP(s_, w_, z_, n_, q_) (p[s_].what = (w_), p[s_].size = (z_), p[s_].note = (n_), p[s_].qual = (q_))
    switch (n) {
    case 1: GP(2, MT_CRYSTAL, 2, 4, 3); GP(10, MT_CRYSTAL, 5, 2, 3); GP(6, MT_MUSH, 3, 0, 4); GP(13, MT_MUSH, 5, 3, 5); GP(8, MT_MOSS, 3, 7, 4); GP(4, MT_OOZE, 3, 0, 1); break;
    case 2: for (s = 0; s < RP_N; s += 2) GP(s, MT_CRYSTAL, (s / 2) % 4 + 1, (s / 2 * 2) % 9, 2 + s % 4); break;
    case 3: for (s = 0; s < RP_N; s++) if (s % 4 != 3) GP(s, MT_MUSH, 2 + s % 3, (s * 3) % 7, 3 + s % 4); break;
    case 4: for (s = 0; s < RP_N; s++) if (s % 3 != 1) GP(s, MT_MOSS, 1 + (s * 5) % 6, 5 + (s * 2) % 8, s % 8); break;
    case 5: for (s = 0; s < RP_N; s += 4) { GP(s, MT_OOZE, 4, s / 4 * 2 % 7, 1 + s / 4); GP(s + 2, MT_MOSS, 2, 9, 2); } break;
    case 6: for (s = 0; s < 8; s++) { GP(s, MT_MUSH, 3, s % 5, 4); if (s & 1) GP(s + 8, MT_CRYSTAL, 3, 4 + s % 5, 3); } break;
    default: for (s = 0; s < RP_N; s++) GP(s, 1 + s % 4, 1 + (s * 3) % 6, (s * 2) % 10, (s * 5) % 8); break;
    }
#undef GP
}
static void ge_part_hit(int s, int32_t vel)      /* part s is struck, by a stone, the plectrum or a key */
{
    rinst_t *r = &ri[RI_GEODE];
    const rpart_t *p = &r->p[s];
    if (p->what == MT_CRYSTAL) {
        if (ge.grow[s] < 120) return;            /* still a stump: nothing to ring */
        if (++ge.hits[s] >= 2 + p->qual) {       /* it breaks, and grows back */
            ge.hits[s] = 0; ge.grow[s] = 30; ge.brk[s] = 0; r->fl[s] = 255;
            rm_hit(RI_GEODE, SK_CRACK, p->note, vel, p->size, p->qual);
            return;
        }
    }
    ri_strike(RI_GEODE, s, p->what == MT_BARE ? vel / 2 : vel);
}
static void ge_step(void)                        /* 10 ms */
{
    static const uint16_t MULT[5] = {0, 64, 128, 256, 512};   /* turns a bar, Q8 */
    static const uint8_t SREST[4] = {256 / 4, 150 / 4, 310 / 4, 0}, SVEL[4] = {0, 60, 0, 0};
    rinst_t *r = &ri[RI_GEODE];
    int i, j, s, ns = ge_nstones(), edit = rm.edit && rm.focus == RI_GEODE;
    int32_t kv = ri_k(RI_GEODE, 0) - 64, ak = iabs(kv), mult, want, vw, grav = 12 + ri_k(RI_GEODE, 1) * 6 / 16, grip = 2 + ri_k(RI_GEODE, 2) / 16;
    int32_t dtq = (int32_t)(rm.tq8 - rm.ptq8), ga = r->sel * 4096, gx = -isin(ga), gy = icos(ga), sr = ge_srad();
    mult = MULT[ak / 16] + (MULT[imin(4, ak / 16 + 1)] - MULT[ak / 16]) * (ak % 16) / 16;
    want = r->run ? (dtq * 256 / 384 * mult >> 8) * (kv < 0 ? -1 : 1) : 0;
    if (edit && !r->run) {                       /* stopped for planting: it settles with part 1 at the bottom left */
        ge.omega -= ge.omega / 5 + (ge.omega > 0 ? 1 : ge.omega < 0 ? -1 : 0);
        if (iabs(ge.omega) < 14) { int32_t t16 = (int16_t)(ge.theta & 65535); ge.omega = 0; ge.theta = t16 - t16 / 5; if (iabs(ge.theta) < 12) ge.theta = 0; }
    } else if (!r->run) ge.omega = ge.omega * 92 / 100;   /* PLAY: the brake */
    else {
        ge.omega += (want - ge.omega) / 20 + (want > ge.omega ? 1 : want < ge.omega ? -1 : 0);
        if (iabs(want - ge.omega) <= 2) ge.omega = want;
        if (ge.omega == want && want && !(ak % 16) && !(r->lfo_on && r->lfo[0])) {   /* at speed: held to the clock, so a part goes by on every step of the bar */
            uint32_t cyc = (rm.tq8 % (384u * 256u * 4u)) * (uint32_t)mult / 384u;
            ge.theta += (int16_t)((kv < 0 ? 0u - cyc : cyc) - (uint32_t)ge.theta) / 6;
        }
    }
    ge.theta += ge.omega;
    vw = (ge.omega * GE_R * 201) >> 13;          /* how fast the shell's wall goes by (Q8 px a step) */
    for (s = 0; s < RP_N; s++) { if (ge.brk[s] < 9000) ge.brk[s] += 10; if (ge.grow[s] < 256) ge.grow[s] += 1; }
    for (i = 0; i < 4; i++) if (ge.pl_t[i] > 0) ge.pl_t[i] -= 10;
    if (r->seq && !edit) {                       /* the plectrum: fixed in the room, it plucks what passes */
        int np = 1 + r->sq[1];
        for (i = 0; i < np; i++) {
            int32_t a = 16384 + r->sq[0] * 4096 + i * 65536 / np, gap = (8 - r->sq[3]) * 3;
            s = (int)(((uint32_t)(a - ge.theta - 16384) & 65535u) >> 12);
            if (s != ge.pl_last[i]) {
                if (ge.pl_last[i] >= 0 && (r->sq[3] >= 8 || ge_h(s) > gap)) { ge_part_hit(s, 90 + r->sq[2] * 20); ge.pl_t[i] = 160; }
                ge.pl_last[i] = (int8_t)s;
            }
        }
    }
    for (i = 0; i < ns; i++) {
        int32_t d, best = -0x7FFFFFFF, rsq, nx, ny, vn, vt, k, rest, x, y, mat;
        ge.st[i].cool += 10;
        ge.st[i].vx += (gx * grav) >> 14; ge.st[i].vy += (gy * grav) >> 14;
        ge.st[i].x += ge.st[i].vx; ge.st[i].y += ge.st[i].vy;
        x = ge.st[i].x; y = ge.st[i].y;
        d = (int32_t)isqrt((uint32_t)((x >> 2) * (x >> 2) + (y >> 2) * (y >> 2))) << 2;
        if (d < 64) continue;
        s = 0;
        for (j = 0; j < RP_N; j++) {
            int32_t a = ge_slot_a(j), dot = (x >> 3) * (icos(a) >> 3) + (y >> 3) * (isin(a) >> 3);
            if (dot > best) { best = dot; s = j; }
        }
        rsq = Q8(GE_R - sr - ge_h(s));
        if (d < rsq - Q8(3)) { ge.st[i].last = -1; continue; }
        if (d < rsq) continue;
        mat = r->p[s].what;
        nx = x * 256 / d; ny = y * 256 / d;
        vn = (ge.st[i].vx * nx + ge.st[i].vy * ny) >> 8;
        ge.st[i].x = nx * rsq >> 8; ge.st[i].y = ny * rsq >> 8;
        rest = mat == MT_MUSH ? 256 : mat == MT_CRYSTAL ? 110 : mat == MT_MOSS || mat == MT_OOZE ? 0 : 70;
        rest = rest * SREST[r->preset & 3u] / 64;
        k = (mat == MT_MOSS ? 40 + r->p[s].qual * 8 : mat == MT_OOZE ? 26 : mat == MT_MUSH ? 6 : 9) * grip / 4;
        if ((s != ge.st[i].last || vn > 260) && ge.st[i].cool > 70) {   /* the stone has met part s */
            ge_part_hit(s, iclamp(vn / 3 + 50 + SVEL[r->preset & 3u], 50, 255));
            ge.st[i].cool = 0;
            if (mat == MT_MUSH) vn = imax(vn, 380 + r->p[s].size * 50);
            if (mat == MT_OOZE) { ge.st[i].vx /= 3; ge.st[i].vy /= 3; }   /* it sinks in */
        }
        ge.st[i].last = (int8_t)s;
        if (vn > 0) { ge.st[i].vx -= nx * (vn * (256 + rest) >> 8) >> 8; ge.st[i].vy -= ny * (vn * (256 + rest) >> 8) >> 8; }
        vt = (ge.st[i].vx * -ny + ge.st[i].vy * nx) >> 8;
        k = imin(k, 200);
        ge.st[i].vx += -ny * ((vw - vt) * k >> 8) >> 8; ge.st[i].vy += nx * ((vw - vt) * k >> 8) >> 8;
    }
    for (i = 0; i < ns; i++)
        for (j = i + 1; j < ns; j++) {
            int32_t dx = ge.st[j].x - ge.st[i].x, dy = ge.st[j].y - ge.st[i].y, d = (int32_t)isqrt((uint32_t)(dx * dx + dy * dy)), e, rv, m = Q8(2 * sr + 1);
            if (d >= m || d < 1) continue;
            e = (m - d) / 2;
            ge.st[i].x -= dx * e / d; ge.st[i].y -= dy * e / d; ge.st[j].x += dx * e / d; ge.st[j].y += dy * e / d;
            rv = ((ge.st[j].vx - ge.st[i].vx) * dx + (ge.st[j].vy - ge.st[i].vy) * dy) / d;
            if (rv < 0) { ge.st[i].vx += dx * rv / d * 8 / 10; ge.st[i].vy += dy * rv / d * 8 / 10; ge.st[j].vx -= dx * rv / d * 8 / 10; ge.st[j].vy -= dy * rv / d * 8 / 10; }
        }
}
static void ge_key(int i, int vel) { ge_part_hit(i, vel); }
static void ge_top(int i)                        /* a flick: every stone thrown toward that side of the shell */
{
    int32_t a = 32768 + i * 3277, k;
    for (k = 0; k < GE_NS; k++) { ge.st[k].vx += (icos(a) * 520) >> 14; ge.st[k].vy += (isin(a) * 520) >> 14 ; ge.st[k].last = -1; }
    sfx(96, 0, 40, W_NOISE, 500, 0, 40);
}
static void ge_play(int on) { if (!on) sfx_ex(50, 0, 300, W_SAW, 1200, 2, 60, 0, 0, 65200); else snd_ui(72); }

/* the shell at (cx, cy), radius R; edit: stopped, its parts numbered */
static void ge_draw_at(int32_t cx, int32_t cy, int32_t R, int edit)
{
    const rinst_t *r = &ri[RI_GEODE];
    int s, i, ns = ge_nstones(), big = R > 40;
    int32_t sr = imax(1, ge_srad() * R / GE_R);
#define PX(rad, a) (cx + (((rad) * icos(a)) >> 14))
#define PY(rad, a) (cy + (((rad) * isin(a)) >> 14))
    galpha(22); gdisc(cx, cy, R + (big ? 5 : 2), th->rim); galpha(256);
    gdisc(cx, cy, R, th->bg);
    garc(cx, cy, R, R, 0, 65536, th->rim, big);
    if (big && r->sel) {                         /* tilted: an arrow for which way is down */
        int32_t a = 16384 + r->sel * 4096;
        gline(PX(R + 9, a), PY(R + 9, a), PX(R + 16, a), PY(R + 16, a), th->pad); gdisc(PX(R + 16, a), PY(R + 16, a), 2, th->pad);
    }
    for (s = 0; s < RP_N; s++) {
        const rpart_t *p = &r->p[s];
        int32_t a = ge_slot_a(s), a0 = a - 2048, hot = r->fl[s], h = ge_h(s) * R / GE_R, j;
        col_t c = ge_col(p->what);
        if (big) {
            galpha(s == 0 ? 256 : 110);          /* the joins between parts, on the outside; a longer one where part 1 begins */
            gline(PX(R + 1, a0), PY(R + 1, a0), PX(R + (s == 0 ? 8 : 4), a0), PY(R + (s == 0 ? 8 : 4), a0), s == 0 ? th->text : th->mute);
            galpha(256);
        }
        if (hot) { galpha(hot * 150 / 256); gdisc(PX(R - 6 * R / GE_R, a), PY(R - 6 * R / GE_R, a), imax(3, 12 * R / GE_R), c); galpha(256); }
        if (p->what == MT_BARE) { if (hot > 60 && big) gline(PX(R - 1, a - 900), PY(R - 1, a - 900), PX(R - 1, a + 900), PY(R - 1, a + 900), th->text); }
        else if (p->what == MT_CRYSTAL) {
            for (j = -1; j <= 1; j += 2) {
                int32_t aj = a + j * 900, l = j < 0 ? h : h * 2 / 3, w = imax(1, (3 + p->qual / 3) * R / GE_R), xy[6], px = PX(R, aj), py = PY(R, aj), hx = icos(aj), hy = isin(aj);
                xy[0] = px + ((w * hy) >> 14); xy[1] = py - ((w * hx) >> 14); xy[2] = px - ((l * hx) >> 14); xy[3] = py - ((l * hy) >> 14); xy[4] = px - ((w * hy) >> 14); xy[5] = py + ((w * hx) >> 14);
                galpha(70 + hot * 186 / 256); gpoly(xy, 3, c); galpha(256); gpolyline(xy, 3, 0, hot > 120 ? th->text : c);
            }
            if (ge.brk[s] < 300) gcirc(PX(R - 10, a), PY(R - 10, a), 4 + ge.brk[s] / 20, th->text);
        } else if (p->what == MT_MUSH) {
            int32_t hh = h - 2 - hot / 90, rr = imax(2, (5 + p->size * 3 / 4) * R / GE_R), mx = PX(R - hh, a), my = PY(R - hh, a), ux = icos(a), uy = isin(a);
            gline2(PX(R, a), PY(R, a), mx, my, th->stem);
            gdisc(mx, my, rr, c); gdisc(mx + ((rr * 2 / 3 * ux) >> 14), my + ((rr * 2 / 3 * uy) >> 14), rr - 1, th->bg);
            if (rr > 3) gdisc(mx - ((2 * ux) >> 14), my - ((2 * uy) >> 14), rr - 2, hot > 120 ? th->text : c);
            if (big && p->qual >= 4) gp(mx - ((rr * ux) >> 14), my - ((rr * uy) >> 14), th->text);   /* a squelchy one glistens */
        } else if (p->what == MT_MOSS) {
            for (j = -3; j <= 3; j++) {
                int32_t aj = a + j * 560, l = (h * (3 + ((j + 9) % 3))) / 4 + 1;
                gline(PX(R, aj), PY(R, aj), PX(R - l, aj), PY(R - l, aj), hot > 120 ? th->text : c);
                if (big) gdisc(PX(R - 1, aj), PY(R - 1, aj), 1, c);
            }
        } else {                                 /* ooze: slow drops of it, swelling */
            for (j = -2; j <= 2; j++) {
                int32_t aj = a + j * 760, w = (tsin(rm.t, 18, s * 9000 + j * 12000) >> 12), l = imax(1, (h * (3 + (j & 1)) / 4 + w) * 1);
                galpha(150 + hot * 100 / 256); gdisc(PX(R - l / 2, aj), PY(R - l / 2, aj), imax(1, l / 2 + 1), c); galpha(256);
            }
            if (big) { galpha(60 + hot * 190 / 256); garc(cx, cy, R - h - 2, R - h - 2, a - 1800, a + 1800, th->text, 0); galpha(256); }
        }
        if (edit) {
            char n[3];
            int on = r->hand >> s & 1;
            n[0] = s >= 9 ? '1' : (char)('1' + s); n[1] = s >= 9 ? (char)('0' + s - 9) : 0; n[2] = 0;
            gtextc(PX(R + 9, a), PY(R + 9, a) - 3, n, on ? th->text : th->mute, 1);
            if (on) {
                int32_t a1 = a + 2048;
                gline(PX(R / 2, a0), PY(R / 2, a0), PX(R, a0), PY(R, a0), th->text); gline(PX(R / 2, a1), PY(R / 2, a1), PX(R, a1), PY(R, a1), th->text);
                garc(cx, cy, R + 2, R + 2, a0, a1, th->text, 1);
            }
        }
    }
    if (r->seq && big) {                         /* the plectra: fixed arms reaching in from outside the shell */
        int np = 1 + r->sq[1];
        for (i = 0; i < np; i++) {
            int32_t a = 16384 + r->sq[0] * 4096 + i * 65536 / np, tip = R - 1 - (8 - r->sq[3]) * 3, tw = ge.pl_t[i] > 0 ? (tsin(rm.t, 500, 0) * ge.pl_t[i] / 160) >> 11 : 0, xy[6];
            xy[0] = PX(R + 12, a - 500); xy[1] = PY(R + 12, a - 500); xy[2] = PX(R + 12, a + 500); xy[3] = PY(R + 12, a + 500); xy[4] = PX(tip, a + tw * 40); xy[5] = PY(tip, a + tw * 40);
            gpoly(xy, 3, ge.pl_t[i] > 0 ? th->text : th->pad); gpolyline(xy, 3, 1, th->text);
            gdisc(PX(R + 13, a), PY(R + 13, a), 3, th->pad);
        }
    }
    for (i = 0; i < ns; i++) {
        int32_t x = cx + ((ge.st[i].x >> 8) * R / GE_R), y = cy + ((ge.st[i].y >> 8) * R / GE_R), hot = ge.st[i].cool < 150 ? 256 - ge.st[i].cool * 256 / 150 : 0;
        gdisc(x, y, sr, hot > 100 ? th->text : th->rock);
        if (big) { gcirc(x, y, sr, th->text); if (r->preset == 3) gp(x - 1, y - 1, th->bg); }
        if (hot && big) { galpha(hot); gcirc(x, y, sr + 2 + (256 - hot) / 30, th->text); galpha(256); }
    }
#undef PX
#undef PY
}
static void ge_draw(int edit)
{
    static const char *const RATE[5] = {"STILL", "A TURN IN 4 BARS", "A TURN IN 2 BARS", "A TURN A BAR", "2 TURNS A BAR"};
    const rinst_t *r = &ri[RI_GEODE];
    ge_draw_at(GE_CX, GE_CY, GE_R, edit && ge.omega == 0 && ge.theta == 0);
    if (edit && (r->hand & (r->hand - 1)) == 0 && ge.omega == 0) {   /* one part in hand: what it is, in the middle */
        int s = 0;
        char nb[6];
        while (s < RP_N - 1 && !(r->hand >> s & 1u)) s++;
        gtextc(GE_CX, GE_CY - 12, GE_KINDS[r->p[s].what], ge_col(r->p[s].what), 2);
        rm_note_name(nb, rm_note(RI_GEODE, r->p[s].note));
        gtextc(GE_CX, GE_CY + 8, nb, th->text, 1);
    } else if (!edit) {
        int32_t ak = iabs(r->k[0] - 4);
        gtext(4, 16, RATE[ak], th->mute, 1);
        if (r->k[0] < 4) gtext(4, 25, "BACKWARDS", th->mute, 1);
    }
}

/* ---- its lesson: the geode itself, played by the lesson */
static void gl_reset(void)
{
    rinst_t *r = &ri[RI_GEODE];
    int i;
    ri_load_algo(RI_GEODE, 1);
    r->k[0] = 4; r->k[1] = 4; r->k[2] = 4; r->k[3] = 1; r->seq = 0; r->arp = 0; r->run = 1; r->sel = 0; r->preset = 0; r->oct = 0;
    for (i = 0; i < 4; i++) { r->lfo[i] = 0; r->env[i] = 4; r->sq[i] = GE_DESC_SQDEF[i]; }
    rm.edit = 0; rm.menu = 0;
}
static void gl_say(const char *s) { rt.label = s; rt.label_t = 1000; }
static void gl_1(int32_t t, int32_t dt)
{
    if (RT_FIRST) gl_reset();
    rt.icon = RTI_K1;
    if (RT_AT(900)) ri[RI_GEODE].k[0] = 5;
    if (RT_AT(2400)) ri[RI_GEODE].k[0] = 6;
}
static void gl_2(int32_t t, int32_t dt) { if (RT_FIRST) ri[RI_GEODE].k[0] = 7; rt.icon = RTI_NONE; }
static void gl_3(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_GEODE];
    int i;
    if (RT_FIRST) { r->k[0] = 4; r->k[3] = 0; for (i = 0; i < 5; i++) { r->p[5 + i].what = (uint8_t)i; r->p[5 + i].size = 3; r->p[5 + i].note = (uint8_t)(i * 2); r->p[5 + i].qual = 3; } ge_init(); }
    rt.icon = RTI_LOW;
    for (i = 0; i < 5; i++) if (RT_AT(900 + i * 1150)) { ge_part_hit(5 + i, 200); gl_say(GE_KINDS[i]); }
}
static void gl_4(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_GEODE];
    if (RT_FIRST) { rm.edit = 1; r->run = 0; r->hand = 1u << 2; }
    rt.icon = t < 1400 ? RTI_BTN + BTN_EDIT : t < 2200 ? RTI_LOW : t < 3200 ? RTI_K1 : t < 4700 ? RTI_K2 : t < 6200 ? RTI_K3 : RTI_K4;
    if (RT_AT(1500)) { r->hand = 1u << 12; ri_strike(RI_GEODE, 12, 150); }
    if (RT_AT(2400)) { r->p[12].what = MT_CRYSTAL; r->p[12].size = 1; r->p[12].note = 2; ge_init(); ri_strike(RI_GEODE, 12, 150); }
    if (RT_AT(3400) || RT_AT(3800) || RT_AT(4200)) { r->p[12].size += 2; ri_strike(RI_GEODE, 12, 150); }
    if (RT_AT(4900) || RT_AT(5300) || RT_AT(5700)) { r->p[12].note += 2; ri_strike(RI_GEODE, 12, 150); }
    if (RT_AT(6400) || RT_AT(6900)) { r->p[12].qual += 2; ri_strike(RI_GEODE, 12, 150); }
}
static void gl_5(int32_t t, int32_t dt)
{
    if (RT_FIRST) { rm.edit = 1; rm.menu = 1; rm.mrow = 0; rm.sc_mask = 1u << MT_CRYSTAL | 1u << MT_MOSS | 1u << MT_OOZE; }
    rt.icon = t < 1200 ? RTI_BTN + BTN_ARP : t < 2800 ? RTI_BTN + BTN_PLAY : t < 3600 ? RTI_TOP : RTI_BTN + BTN_PLAY;
    if (RT_AT(1500) || RT_AT(4000)) ri_scatter(RI_GEODE);
    if (RT_AT(3000)) rm.sc_mask ^= 1u << MT_MUSH;
    if (RT_AT(5300)) { rm.menu = 0; rm.sc_mask = 0xFE; }
}
static void gl_6(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_GEODE];
    if (RT_FIRST) { rm.menu = 0; rm.edit = 0; r->run = 1; r->k[0] = 7; r->k[3] = 0; r->seq = 1; }
    rt.icon = t < 3200 ? RTI_BTN + BTN_SEQ : RTI_K2;
    if (RT_AT(3600)) r->sq[1] = 1;
}
static void gl_7(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[RI_GEODE];
    if (RT_FIRST) { r->seq = 0; r->sq[1] = 0; r->k[3] = 2; }
    rt.icon = t < 1400 ? RTI_K4 : t < 2800 ? RTI_K2 : t < 4200 ? RTI_K3 : RTI_TOP;
    if (RT_AT(1500)) r->k[1] = 7;
    if (RT_AT(2900)) r->k[2] = 7;
    if (RT_AT(4400)) ge_top(3);
    if (RT_AT(5200)) ge_top(8);
}
static void gl_8(int32_t t, int32_t dt) { rt.icon = t < 1800 ? RTI_BTN + BTN_REC : t < 3400 ? RTI_BTN + BTN_SAVE : RTI_BTN + BTN_HOME; (void)dt; }
static const rt_step_t GE_LESSON[8] = {
    {5000, {"KNOB 1 SETS THE SHELL TURNING, AND IT KEEPS TURNING. THE STONE INSIDE IS LOOSE.",
     "KNOPF 1 SETZT DIE SCHALE IN DREHUNG, UND SIE DREHT WEITER. DER STEIN DARIN LIEGT LOSE.",
     "KNAP 1 SÆTTER SKALLEN I GANG, OG DEN BLIVER VED MED AT DREJE. STENEN INDENI LIGGER LØST.",
     "LE BOUTON 1 FAIT TOURNER LA COQUE, ET ELLE CONTINUE DE TOURNER. LA PIERRE À L'INTÉRIEUR EST LIBRE.",
     "EL MANDO 1 HACE GIRAR LA CÁSCARA, Y SIGUE GIRANDO. LA PIEDRA DE DENTRO ESTÁ SUELTA.",
     "DÜĞME 1 KABUĞU DÖNDÜRÜR VE KABUK DÖNMEYE DEVAM EDER. İÇİNDEKİ TAŞ SERBESTTİR.",
     "KNOP 1 LAAT DE SCHAAL DRAAIEN, EN ZE BLIJFT DRAAIEN. DE STEEN BINNENIN LIGT LOS."}, gl_1},
    {5500, {"THE SHELL ITSELF IS SILENT. A SOUND COMES ONLY WHEN THE STONE MEETS SOMETHING THAT GROWS INSIDE.",
     "DIE SCHALE SELBST IST STUMM. EIN KLANG ENTSTEHT NUR, WENN DER STEIN AUF ETWAS TRIFFT, DAS INNEN WÄCHST.",
     "SELVE SKALLEN ER TAVS. DER KOMMER KUN EN LYD, NÅR STENEN RAMMER NOGET, DER GROR INDENI.",
     "LA COQUE ELLE-MÊME EST MUETTE. UN SON NAÎT SEULEMENT QUAND LA PIERRE TOUCHE CE QUI POUSSE À L'INTÉRIEUR.",
     "LA CÁSCARA EN SÍ NO SUENA. SOLO HAY SONIDO CUANDO LA PIEDRA TOCA ALGO QUE CRECE DENTRO.",
     "KABUĞUN KENDİSİ SESSİZDİR. SES ANCAK TAŞ İÇERİDE BÜYÜYEN BİR ŞEYE ÇARPINCA ÇIKAR.",
     "DE SCHAAL ZELF IS STIL. ER KLINKT ALLEEN IETS ALS DE STEEN IETS RAAKT DAT BINNENIN GROEIT."}, gl_2},
    {7000, {"FIVE THINGS LINE THE SHELL. BARE ROCK CLICKS. CRYSTAL RINGS. MUSHROOM IS AN ACID BASS. MOSS IS NOISE. OOZE SINGS A CHORD. THE BOTTOM ROW KNOCKS THEM BY HAND.",
     "FÜNF DINGE SITZEN IN DER SCHALE. FELS KLICKT. KRISTALL KLINGT. PILZ IST EIN ACID-BASS. MOOS RAUSCHT. SCHLEIM SINGT EINEN AKKORD. DIE UNTERE TASTENREIHE KLOPFT SIE VON HAND AN.",
     "FEM TING SIDDER I SKALLEN. KLIPPE KLIKKER. KRYSTAL KLINGER. SVAMP ER EN ACID-BAS. MOS ER STØJ. SLIM SYNGER EN AKKORD. DEN NEDERSTE RÆKKE BANKER PÅ DEM MED HÅNDEN.",
     "CINQ CHOSES TAPISSENT LA COQUE. LA ROCHE CLIQUE. LE CRISTAL TINTE. LE CHAMPIGNON EST UNE BASSE ACID. LA MOUSSE EST DU BRUIT. LA VASE CHANTE UN ACCORD. LA RANGÉE DU BAS LES FRAPPE À LA MAIN.",
     "CINCO COSAS CUBREN LA CÁSCARA. LA ROCA HACE CLIC. EL CRISTAL SUENA. LA SETA ES UN BAJO ACID. EL MUSGO ES RUIDO. EL LIMO CANTA UN ACORDE. LA FILA DE ABAJO LAS GOLPEA A MANO.",
     "KABUKTA BEŞ ŞEY VAR. KAYA TIKLAR. KRİSTAL ÇINLAR. MANTAR ACID BASTIR. YOSUN GÜRÜLTÜDÜR. BALÇIK AKOR SÖYLER. ALT SIRA ONLARA ELLE VURUR.",
     "VIJF DINGEN BEKLEDEN DE SCHAAL. ROTS KLIKT. KRISTAL KLINKT. PADDENSTOEL IS EEN ACID-BAS. MOS IS RUIS. SLIJM ZINGT EEN AKKOORD. DE ONDERSTE RIJ TIKT ZE MET DE HAND AAN."}, gl_3},
    {8000, {"EDIT STOPS THE SHELL. HOLD A KEY OF THE BOTTOM ROW: THAT PART IS IN HAND. KNOB 1: WHAT GROWS THERE. KNOB 2: ITS SIZE. KNOB 3: ITS NOTE. KNOB 4: ITS QUALITY.",
     "EDIT HÄLT DIE SCHALE AN. HALTE EINE TASTE DER UNTEREN REIHE: DIESES STÜCK HAST DU IN DER HAND. KNOPF 1: WAS DORT WÄCHST. KNOPF 2: DIE GRÖSSE. KNOPF 3: DER TON. KNOPF 4: DIE ART.",
     "EDIT STANDSER SKALLEN. HOLD EN TANGENT I NEDERSTE RÆKKE: DEN DEL HAR DU I HÅNDEN. KNAP 1: HVAD DER GROR. KNAP 2: STØRRELSEN. KNAP 3: TONEN. KNAP 4: KARAKTEREN.",
     "EDIT ARRÊTE LA COQUE. TIENS UNE TOUCHE DE LA RANGÉE DU BAS : CETTE PARTIE EST EN MAIN. BOUTON 1 : CE QUI Y POUSSE. BOUTON 2 : SA TAILLE. BOUTON 3 : SA NOTE. BOUTON 4 : SON CARACTÈRE.",
     "EDIT PARA LA CÁSCARA. MANTÉN UNA TECLA DE LA FILA DE ABAJO: ESA PARTE ESTÁ EN TU MANO. MANDO 1: QUÉ CRECE AHÍ. MANDO 2: SU TAMAÑO. MANDO 3: SU NOTA. MANDO 4: SU CARÁCTER.",
     "EDIT KABUĞU DURDURUR. ALT SIRADAN BİR TUŞU BASILI TUT: O PARÇA ELİNDE. DÜĞME 1: ORADA NE BÜYÜR. DÜĞME 2: BOYU. DÜĞME 3: NOTASI. DÜĞME 4: NİTELİĞİ.",
     "EDIT ZET DE SCHAAL STIL. HOUD EEN TOETS VAN DE ONDERSTE RIJ VAST: DAT DEEL HEB JE IN DE HAND. KNOP 1: WAT ER GROEIT. KNOP 2: DE GROOTTE. KNOP 3: DE NOOT. KNOP 4: HET KARAKTER."}, gl_4},
    {6200, {"IN EDIT, ARP OPENS SCATTER: A NEW SHELL AT RANDOM, MADE ONLY OF THE KINDS YOU PICK WITH THE TOP ROW. " PB " ROLLS AGAIN.",
     "IN EDIT ÖFFNET ARP DAS STREUEN: EINE NEUE ZUFÄLLIGE SCHALE, NUR AUS DEN ARTEN, DIE DU MIT DER OBEREN REIHE WÄHLST. " PB " WÜRFELT NEU.",
     "I EDIT ÅBNER ARP SPREDNING: EN NY TILFÆLDIG SKAL, KUN AF DE SLAGS DU VÆLGER MED ØVERSTE RÆKKE. " PB " KASTER IGEN.",
     "DANS EDIT, ARP OUVRE LE SEMIS : UNE NOUVELLE COQUE AU HASARD, FAITE SEULEMENT DES SORTES CHOISIES AVEC LA RANGÉE DU HAUT. " PB " RELANCE.",
     "EN EDIT, ARP ABRE LA SIEMBRA: UNA CÁSCARA NUEVA AL AZAR, SOLO CON LAS CLASES QUE ELIJAS CON LA FILA DE ARRIBA. " PB " VUELVE A TIRAR.",
     "EDIT İÇİNDE ARP SAÇMAYI AÇAR: YALNIZCA ÜST SIRAYLA SEÇTİĞİN TÜRLERDEN, RASTGELE YENİ BİR KABUK. " PB " YENİDEN ATAR.",
     "IN EDIT OPENT ARP HET STROOIEN: EEN NIEUWE SCHAAL OP GOED GELUK, ALLEEN VAN DE SOORTEN DIE JE MET DE BOVENSTE RIJ KIEST. " PB " GOOIT OPNIEUW."}, gl_5},
    {7000, {"SEQ SWINGS IN THE PLECTRUM. IT STAYS IN ONE PLACE AND PLUCKS EVERY PART THAT PASSES, SO IT KEEPS TIME. HOLD SEQ AND TURN KNOB 1 TO 4: MOVE IT, ADD MORE, SET ITS FORCE AND REACH.",
     "SEQ SCHWENKT DAS PLEKTRUM HEREIN. ES BLEIBT AN EINER STELLE UND ZUPFT JEDES STÜCK, DAS VORBEIKOMMT: SO HÄLT ES DEN TAKT. HALTE SEQ UND DREH KNOPF 1 BIS 4: VERSCHIEBEN, MEHR DAVON, KRAFT UND REICHWEITE.",
     "SEQ SVINGER PLEKTERET IND. DET BLIVER PÅ ÉT STED OG KNIPSER HVER DEL, DER KOMMER FORBI, SÅ DET HOLDER TAKTEN. HOLD SEQ OG DREJ KNAP 1 TIL 4: FLYT DET, FLERE AF DEM, KRAFT OG RÆKKEVIDDE.",
     "SEQ FAIT ENTRER LE MÉDIATOR. IL RESTE AU MÊME ENDROIT ET PINCE CHAQUE PARTIE QUI PASSE : IL GARDE LE TEMPO. TIENS SEQ ET TOURNE LES BOUTONS 1 À 4 : LE DÉPLACER, EN AJOUTER, SA FORCE ET SA PORTÉE.",
     "SEQ METE LA PÚA. SE QUEDA EN UN SITIO Y PULSA CADA PARTE QUE PASA: ASÍ LLEVA EL COMPÁS. MANTÉN SEQ Y GIRA LOS MANDOS 1 A 4: MOVERLA, AÑADIR MÁS, SU FUERZA Y SU ALCANCE.",
     "SEQ PENAYI İÇERİ SOKAR. PENA YERİNDE DURUR VE GEÇEN HER PARÇAYI ÇEKER, BÖYLECE TEMPOYU TUTAR. SEQ'İ BASILI TUT VE DÜĞME 1-4'Ü ÇEVİR: YERİ, SAYISI, GÜCÜ VE ERİMİ.",
     "SEQ ZWAAIT HET PLECTRUM NAAR BINNEN. HET BLIJFT OP ÉÉN PLEK EN TOKKELT ELK DEEL DAT LANGSKOMT, DUS HET HOUDT DE MAAT. HOUD SEQ VAST EN DRAAI KNOP 1 TOT 4: VERPLAATSEN, MEER ERVAN, KRACHT EN BEREIK."}, gl_6},
    {6500, {"KNOB 4: MORE STONES. KNOB 2: THEIR WEIGHT. KNOB 3: THE SHELL'S GRIP. SELECT TILTS IT ALL. THE TOP ROW FLICKS THE STONES.",
     "KNOPF 4: MEHR STEINE. KNOPF 2: IHR GEWICHT. KNOPF 3: DER GRIFF DER SCHALE. SELECT KIPPT ALLES. DIE OBERE REIHE SCHNIPPT DIE STEINE.",
     "KNAP 4: FLERE STEN. KNAP 2: DERES VÆGT. KNAP 3: SKALLENS GREB. SELECT VIPPER DET HELE. ØVERSTE RÆKKE KNIPSER TIL STENENE.",
     "BOUTON 4 : PLUS DE PIERRES. BOUTON 2 : LEUR POIDS. BOUTON 3 : L'ADHÉRENCE DE LA COQUE. SELECT INCLINE LE TOUT. LA RANGÉE DU HAUT DONNE UNE PICHENETTE AUX PIERRES.",
     "MANDO 4: MÁS PIEDRAS. MANDO 2: SU PESO. MANDO 3: EL AGARRE DE LA CÁSCARA. SELECT LO INCLINA TODO. LA FILA DE ARRIBA DA UN TOQUE A LAS PIEDRAS.",
     "DÜĞME 4: DAHA ÇOK TAŞ. DÜĞME 2: AĞIRLIKLARI. DÜĞME 3: KABUĞUN TUTUŞU. SELECT HEPSİNİ EĞER. ÜST SIRA TAŞLARA FİSKE VURUR.",
     "KNOP 4: MEER STENEN. KNOP 2: HUN GEWICHT. KNOP 3: DE GRIP VAN DE SCHAAL. SELECT KANTELT ALLES. DE BOVENSTE RIJ TIKT DE STENEN WEG."}, gl_7},
    {5500, {"REC RECORDS WHAT SOUNDS ONTO THE LOOP. SAVE KEEPS THE ROOM. HOME GOES BACK TO IT. HOLD LFO, ENV OR FX AND TURN A KNOB FOR MORE. NOW YOU!",
     "REC NIMMT AUF, WAS KLINGT, IN DIE SCHLEIFE. SAVE BEWAHRT DEN RAUM. HOME FÜHRT ZURÜCK. HALTE LFO, ENV ODER FX UND DREH EINEN KNOPF FÜR MEHR. JETZT DU!",
     "REC OPTAGER DET, DER LYDER, I LOOPET. SAVE GEMMER RUMMET. HOME GÅR TILBAGE TIL DET. HOLD LFO, ENV ELLER FX OG DREJ EN KNAP FOR MERE. NU ER DET DIG!",
     "REC ENREGISTRE CE QUI SONNE DANS LA BOUCLE. SAVE GARDE LA SALLE. HOME Y RETOURNE. TIENS LFO, ENV OU FX ET TOURNE UN BOUTON POUR PLUS. À TOI !",
     "REC GRABA LO QUE SUENA EN EL BUCLE. SAVE GUARDA LA SALA. HOME VUELVE A ELLA. MANTÉN LFO, ENV O FX Y GIRA UN MANDO PARA MÁS. ¡TE TOCA!",
     "REC ÇALANI DÖNGÜYE KAYDEDER. SAVE ODAYI SAKLAR. HOME ODAYA DÖNER. DAHA FAZLASI İÇİN LFO, ENV YA DA FX'İ BASILI TUTUP BİR DÜĞME ÇEVİR. ŞİMDİ SEN!",
     "REC NEEMT OP WAT KLINKT, OP DE LOOP. SAVE BEWAART DE KAMER. HOME GAAT ERNAAR TERUG. HOUD LFO, ENV OF FX VAST EN DRAAI EEN KNOP VOOR MEER. NU JIJ!"}, gl_8},
};
static const rdesc_t GE_DESC = {
    "THE GEODE", {"SPIN", "WEIGHT", "GRIP", "STONES"}, {8, 8, 8, 4}, {7, 4, 4, 1}, {1, 0, 0, 0},
    "TILT", 15, 0,
    "SEQ: THE PLECTRUM", {"PLACE", "COUNT", "FORCE", "REACH"}, {15, 3, 8, 8}, {8, 0, 5, 7},
    GE_KINDS, MT_N, GE_SND, GE_STONES, 4, GE_ALGOS, 8,
    ge_init, ge_step, ge_draw, ge_key, ge_top, ge_layout, ge_play, GE_LESSON, 8,
};
