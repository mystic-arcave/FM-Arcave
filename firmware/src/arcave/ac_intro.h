/* SPDX-License-Identifier: GPL-3.0-only */
/* FM ARCAVE's intro: the title, the wide shot, "press PLAY to wake up Rock", the roll down the course to the cave.
 * A port of the browser prototype (mystic-arcave-intro.html) to whole numbers: positions and speeds are in 1/256 px,
 * the land comes from a table (tools/gen_course.py), the physics steps 128 times a second. */
#include "arcave_course.h"

/* two courses: the opening roll is the short one; out of the Arcave's upper door it is the long one (twice the
 * length, the first build's), raced back. Everything below reads the one in use through these names */
static const course_t *co = &CO_SHORT;
#define CO_L (co->l)
#define CO_XE (co->xe)
#define CO_XSTOP (co->xstop)
#define CO_HN (co->hn)
#define CO_HT (co->ht)
#define CO_YEND_Q4 (co->yend_q4)
#define CO_SHROOM (co->shroom)
#define CO_NSHROOM (co->nshroom)
#define CO_PAD (co->pad)
#define CO_NPAD (co->npad)
#define CO_SKY (co->sky)
#define CO_NSKY (co->nsky)
#define CO_PROP (co->prop)
#define CO_NPROP (co->nprop)
#define CO_POOL (co->pool)
#define CO_NPOOL (co->npool)

enum { IP_TITLE, IP_WIDE, IP_WAKE, IP_PLAY, IP_OUT, IP_DONE };

typedef struct {
    int32_t x, y, vx, vy;                        /* Q8 px, px/s */
    int32_t th, om;                              /* the turn (angle units) and its speed (units/s) */
    int32_t coy;                                 /* steps left in which a jump still counts after leaving the ground */
    int32_t t_vn, t_vt, t_sl;                    /* the last landing */
    uint8_t g, jumped, touch, thud;
} rock_t;

static struct {
    uint8_t phase, turned, on_course, done, pad_jump, exact, on_pad, found, loop_on;
    int32_t t;                                   /* ms in this phase */
    rock_t r;
    int32_t eyes, camx, camy, camz, cam_y, c_off, fade, hint, cave_a, cave_t, course_t, near, clean, bonus, spores, rings;
    int32_t rocket, drag, acc, loop_a, loop_w;
    struct { int32_t x, y, t; uint8_t c, big, label, on; } burst;
    struct { int32_t x, y, t; } trail[20];
    uint32_t ntrail;
    uint8_t pad_used[CO_MAXPAD], sky_used[CO_MAXSKY];
    int16_t sky_sq[CO_MAXSKY];
    int32_t veil;                                /* the rock face that hides the hollow behind the start: 256 solid .. 0 gone */
    uint8_t race;                                /* the race back: 0 not, 1 out of the door (before the line), 2 on the clock */
    int32_t race_ms, race_show, step_cd;
} G;
/* the race back: out of the Arcave's upper door, over the start line, to where the game began. The pads and the
 * sky things are the mirrored set (CO_RPAD, CO_RSKY), a booster throws Rock up each of the four big steps, and
 * Rock pushes harder (it is uphill all the way). The best time is kept with the scores. */
#define RACE_X0 (CO_L - 96)                      /* where Rock comes out */
#define RACE_START (CO_L - 196)
#define RACE_END 74
static uint32_t race_best_cs, race_last_cs;      /* hundredths of a second; 0: never run */
static uint8_t race_new, race_save_due;
#define I_PAD (G.race ? CO_RPAD : CO_PAD)
#define I_NPAD (G.race ? CO_NRPAD : CO_NPAD)
#define I_SKY (G.race ? CO_RSKY : CO_SKY)
#define I_NSKY (G.race ? CO_NRSKY : CO_NSKY)
#define I_DIR (G.race ? -1 : 1)
static uint32_t in_time;                         /* ms since the intro began: what its little animations run on */

static int32_t land_y(int32_t x)
{
    int32_t i = (x >> 8) - CO_HX0, f = x & 255;
    if (i < 0) { i = 0; f = 0; }
    if (i > CO_HN - 2) { i = CO_HN - 2; f = 256; }
    return (CO_HT[i] * (256 - f) + CO_HT[i + 1] * f) >> 4;
}
static int32_t land_sl(int32_t x) { return iclamp(land_y(x + 128) - land_y(x - 128), -768, 768); }

#define GR Q8(520)
#define TQ 480
#define VK Q8(215)
#define JMIN 60
#define JMAX 300
#define CAMMAX Q8(CO_L + 50 - 240)

/* one step of 1/128 s. knob: -256..256; jump: -1 or the strength 0..256 */
static void rock_phys(rock_t *s, int32_t knob, int32_t jump)
{
    int32_t sl = land_sl(s->x), n = (int32_t)isqrt((uint32_t)(65536 + sl * sl)), tx = 65536 / n, ty = sl * 256 / n, gy;
    if (s->g) {
        int32_t vt = (s->vx * tx + s->vy * ty) >> 8, push = knob * (G.race ? TQ * 8 / 5 : TQ), lump, fr, a;
        if ((knob > 0 && vt > 0) || (knob < 0 && vt < 0)) push = (push * imax(0, 256 - iabs(vt) * 256 / (G.race ? Q8(300) : VK))) >> 8;
        lump = -513 * (rock_rad(s->th + 209) - rock_rad(s->th - 209));
        fr = -vt / 5 - (iabs(vt) > 512 ? (vt > 0 ? Q8(5) : -Q8(5)) : 0);
        a = push + lump + fr;
        s->vx += ((a * tx) >> 8) >> 7;
        s->vy += ((a * ty) >> 8) >> 7;
    }
    s->vy += GR >> 7;
    if (s->g) s->coy = 13; else if (s->coy > 0) s->coy--;
    if (jump >= 0 && s->coy > 0) {
        s->vy = imin(s->vy, 0) - Q8(JMIN) - (JMAX - JMIN) * jump;
        s->g = 0; s->coy = 0; s->jumped = 1;
    }
    s->x += s->vx >> 7; s->y += s->vy >> 7;
    if (s->x < Q8(CO_XMIN)) { s->x = Q8(CO_XMIN); s->vx = imax(0, s->vx); }
    if (s->x < Q8(CO_WALLX) + ROCK_RM && s->x > Q8(CO_WALLX) - ROCK_RM && s->y + ROCK_RM > Q8(CO_LEDGE + 4)) {
        s->x = Q8(CO_WALLX) + ROCK_RM; s->vx = imax(0, s->vx);
    }
    if (s->x > Q8(CO_XSTOP)) { s->x = Q8(CO_XSTOP); if (s->vx > 0) s->vx = -s->vx / 5; }
    gy = land_y(s->x) - rock_rad(s->th);
    if (s->y >= gy) {
        int32_t vn, vt;
        sl = land_sl(s->x); n = (int32_t)isqrt((uint32_t)(65536 + sl * sl)); tx = 65536 / n; ty = sl * 256 / n;
        vn = (-s->vx * ty + s->vy * tx) >> 8; vt = (s->vx * tx + s->vy * ty) >> 8;
        s->y = gy;
        if (vn > 0) {
            int32_t out = vn > Q8(95) ? -vn / 5 : 0;
            if (!s->g) { s->touch = 1; s->t_vn = vn; s->t_vt = vt; s->t_sl = sl; if (vn > Q8(60)) s->thud = 1; }
            s->vx = (vt * tx - out * ty) >> 8; s->vy = (vt * ty + out * tx) >> 8;
            s->g = out == 0;
        } else s->g = 1;
    } else s->g = (gy - s->y) < 154;
    if (s->g) s->om = (((s->vx * tx + s->vy * ty) >> 8) * 1372) >> 8;
    else s->om -= s->om / 512;
    s->th += s->om >> 7;
}

/* a pad's jump: the strength (of 19) that lands best on the next downward slope */
static int32_t pad_plan(int32_t px, int32_t far, int32_t speed, int32_t d)   /* d: 1 rightward, -1 leftward (mirrored) */
{
    int32_t sl = d * land_sl(px), n = (int32_t)isqrt((uint32_t)(65536 + sl * sl)), tx = 65536 / n, ty = sl * 256 / n;
    int32_t y0 = land_y(px) - ROCK_RM, k, best = 0, best_sc = -0x7FFFFFFF;
    for (k = 0; k < 19; k++) {
        int32_t x = px, y = y0, vx = (speed * tx) >> 8, vy = imin((speed * ty) >> 8, 0) - Q8(JMIN) - Q8(JMAX - JMIN) * k / 18, t, s2, n2, vt, vn, sc;
        for (t = 0; t < 210; t++) {
            vy += (GR * 273) >> 14; x += d * ((vx * 273) >> 14); y += (vy * 273) >> 14;
            if (t > 4 && y >= land_y(x) - ROCK_RM) break;
        }
        s2 = d * land_sl(x); n2 = (int32_t)isqrt((uint32_t)(65536 + s2 * s2));
        vt = (vx + ((vy * s2) >> 8)) * 256 / n2; vn = (-((vx * s2) >> 8) + vy) * 256 / n2;
        sc = ((s2 > 26 && vn < vt / 2) ? (d * (x - far) > 0 ? Q8(2000) : Q8(1000)) : 0) + vt - vn * 6 / 10;
        if (sc > best_sc) { best_sc = sc; best = k; }
    }
    return best;
}

static void intro_setcam(int32_t z, int32_t sx, int32_t sy)   /* the rock at screen (sx, sy), zoom z (Q8) */
{
    G.camz = z; G.camx = G.r.x - (sx << 16) / z; G.camy = G.r.y - (sy << 16) / z;
}
static int32_t exp2_q8(int32_t l)                /* 2^(l/256) -> Q8 */
{
    int32_t i = l >> 8, f = l & 255, m = 256 + ((f * (168 + ((88 * f) >> 8))) >> 8);
    return i >= 0 ? m << i : m >> -i;
}
/* between two camera keys: zoom by ratio, the rock's place straight */
static void intro_mixcam(int32_t la, int32_t sxa, int32_t sya, int32_t lb, int32_t sxb, int32_t syb, int32_t t)
{
    t = ease8(t);
    intro_setcam(exp2_q8(la + (lb - la) * t / 256), sxa + (sxb - sxa) * t / 256, sya + (syb - sya) * t / 256);
}
#define CAM_WIDE -256, 26, 78                    /* log2(zoom) Q8, where the rock sits */
#define CAM_ZOOM 594, 120, 142
#define CAM_PLAY 0, 84, 130

static void intro_reset(void)
{
    uint32_t i;
    for (i = 0; i < sizeof G; i++) ((uint8_t *)&G)[i] = 0;
    co = &CO_SHORT; G.veil = 256;
    G.r.x = Q8(60); G.r.y = land_y(Q8(60)) - rock_rad(0); G.r.g = 1;
    G.phase = IP_TITLE; G.c_off = Q8(84);
    in_time = 0;
    intro_setcam(128, 26, 78);
    snd_jingle();
}
static void intro_skip_to_end(void)              /* (tests) the last stretch before the cave */
{
    G.phase = IP_PLAY; G.t = 0; G.eyes = 256; G.turned = 1;
    G.r.x = Q8(CO_L - 700); G.r.y = land_y(G.r.x) - rock_rad(0); G.cam_y = G.r.y - Q8(130);
}

static void intro_race_start(void)               /* out of the Arcave's upper door: at the cave mouth, facing back */
{
    uint32_t i;
    for (i = 0; i < sizeof G; i++) ((uint8_t *)&G)[i] = 0;
    co = &CO_LONG; G.veil = 256;
    G.phase = IP_PLAY; G.eyes = 256; G.turned = 1; G.race = 1; G.fade = 256; G.c_off = Q8(156);
    G.r.x = Q8(RACE_X0); G.r.y = land_y(G.r.x) - rock_rad(0); G.r.g = 1; G.cam_y = G.r.y - Q8(130);
    G.camz = 256; G.camx = iclamp(G.r.x - G.c_off, Q8(CO_CAMMIN), CAMMAX); G.camy = G.cam_y;
    race_new = 0;
}

static void intro_burst(int32_t x, int32_t y, int32_t c, int big, int label)
{
    G.burst.x = x; G.burst.y = y; G.burst.t = 0; G.burst.c = (uint8_t)c; G.burst.big = (uint8_t)big; G.burst.label = (uint8_t)label; G.burst.on = 1;
}

static void intro_sky_hits(int32_t dt)
{
    rock_t *r = &G.r;
    uint32_t i;
    int32_t d = I_DIR;
    for (i = 0; i < (uint32_t)I_NSKY; i++) {
        const co_sky_t *o = &I_SKY[i];
        int32_t dx = (r->x >> 8) - o->x, dy;
        if (G.sky_sq[i] > 0) G.sky_sq[i] = (int16_t)imax(0, G.sky_sq[i] - dt);
        if (dx < -40 || dx > 40) continue;
        dy = (r->y >> 8) - o->y;
        if (o->t == 0) {
            if (!G.sky_used[i] && dx * dx + dy * dy < 144) {
                G.sky_used[i] = 1; G.spores++; r->vy -= Q8(25);
                sfx(INTRO_PENT[(o->c + G.spores) % 9], 0, 600, W_SINE, 2950, 6, 130);
            }
        } else if (o->t == 1) {
            if (iabs(dx) < 17 && dy > -20 && dy < 6 && r->vy > -Q8(60) && G.sky_sq[i] <= 0) {
                static const uint8_t JN[4] = {0, 4, 7, 12};
                r->vy = -Q8(335); r->vx = d * (imax(d * r->vx, Q8(150)) + Q8(15)); r->g = 0; G.sky_sq[i] = 250;
                intro_burst(o->x << 8, o->y << 8, o->c, 0, 0);
                sfx(72 + JN[o->c % 4], 0, 600, W_SINE, 2950, 6, 130);
            }
        } else if (!G.sky_used[i] && dx * dx + dy * dy < 400) {
            G.sky_used[i] = 1; r->vx = d * (imax(d * r->vx, 0) + Q8(170)); r->vy = imin(r->vy, -Q8(150)); G.rings++;
            intro_burst(r->x, r->y, o->c, 1, T_SKYHIGH);
            snd_chime(1);
        }
    }
}

#define CTRL_X 96                                /* the controls box: its middle, in the course's pixels */
static int intro_want_play(void) { return (G.phase == IP_WIDE && G.t > 1000) || (G.phase == IP_PLAY && G.cave_a > 128); }

static void intro_press_play(void)
{
    if (G.phase == IP_TITLE) { if (G.t < 5600) G.t = 5600; }
    else if (G.phase == IP_WIDE) { G.phase = IP_WAKE; G.t = 0; }
    else if (G.phase == IP_PLAY && G.cave_a > 128) { G.phase = IP_OUT; G.t = 0; }
}

/* returns 1 once the screen has gone black at the cave */
static int intro_tick(int32_t dt)
{
    rock_t *r = &G.r;
    G.t += dt; in_time += (uint32_t)dt;
    if (BDOWN(BTN_PLAY)) intro_press_play();
    if (in.kdown && G.phase != IP_TITLE) snd_key(in.hikey);
    if (G.phase == IP_TITLE) {
        G.fade = G.t > 5600 ? imin(256, (G.t - 5600) * 256 / 600) : 0;
        if (G.t > 6200) { G.phase = IP_WIDE; G.t = -800; }
    } else if (G.phase == IP_WIDE) {
        G.fade = G.t < 0 ? -G.t * 256 / 800 : 0;
        intro_setcam(128, 26, 78);
    } else if (G.phase == IP_WAKE) {
        int32_t t = G.t;
        if (t < 500) intro_mixcam(CAM_WIDE, CAM_ZOOM, t * 256 / 500);
        else if (t < 2700) intro_setcam(1280, 120, 142);
        else intro_mixcam(CAM_ZOOM, CAM_PLAY, (t - 2700) * 256 / 800);
        G.eyes = t < 1000 ? 0 : t < 1250 ? (t - 1000) * 256 / 250 : (t > 1850 && t < 2070) ? iabs(t - 1960) * 256 / 110 : 256;
        if (t >= 1000 && t - dt < 1000) { sfx(79, 0, 500, W_SINE, 2600, 8, 130); sfx(84, 110, 700, W_SINE, 2600, 8, 130); }
        if (t >= 3500) { G.phase = IP_PLAY; G.t = 0; G.eyes = 256; G.cam_y = r->y - Q8(130); }
    } else if (G.phase == IP_PLAY || G.phase == IP_OUT) {
        int live = G.phase == IP_PLAY, lock = 0, first = 1;
        int32_t knob, jump = -1, pad = -1, ahead = Q8(999), win, i, bt, d = I_DIR;
        const co_pad_t *pads = I_PAD;
        if (live && G.fade > 0) G.fade = imax(0, G.fade - dt * 256 / 900);   /* (out of the door: in from black) */
        if (live && in.any_knob) G.drag = iclamp(G.drag + in.any_knob * 115, -384, 384);
        knob = live ? iclamp(G.drag, -256, 256) : 0;
        if (iabs(knob) > 13) G.turned = 1;
        G.drag -= G.drag * imin(dt, 100) / 250;
        if (live) jump = in.hop;
        r->jumped = 0; r->touch = 0; r->thud = 0;
        if (live && d > 0) {
            for (i = 0; i < CO_NPAD; i++)
                if (Q8(pads[i].x) > r->x - Q8(28) - iabs(r->vx) * 12 / 100) { pad = i; break; }
        } else if (live) {                       /* the race back: the next pad is to the left */
            for (i = CO_NRPAD - 1; i >= 0; i--)
                if (Q8(pads[i].x) < r->x + Q8(28) + iabs(r->vx) * 12 / 100) { pad = i; break; }
        }
        if (pad >= 0) ahead = d * (Q8(pads[pad].x) - r->x);
        win = Q8(20) + iabs(r->vx) * 12 / 100;
        G.on_pad = pad >= 0 && !G.pad_used[pad] && iabs(ahead) < win && d * r->vx > Q8(30) && (r->g || r->coy > 0);
        G.near = (pad >= 0 && !G.pad_used[pad] && ahead < Q8(170) && d * r->vx > Q8(30)) ? 256 - imax(0, ahead - win) / 170 : 0;
        if (jump >= 0 && G.on_pad && r->coy > 0) {   /* any key on the pad: the jump is set for the next downward slope */
            int32_t k = pad_plan(r->x, Q8(pads[pad].far), (int32_t)isqrt((uint32_t)((r->vx >> 4) * (r->vx >> 4) + (r->vy >> 4) * (r->vy >> 4))) << 4, d);
            G.exact = (jump == HOP_BIG) == (k >= 13);   /* the row that suits this pad: a bonus on landing */
            jump = k * 256 / 18; lock = 1; G.pad_used[pad] = 1;
        }
        if (live && !G.loop_on && r->x < Q8(CO_LOOPX + 6) && r->y > land_y(Q8(CO_LOOPX)) - Q8(26)) {
            G.loop_on = 1; G.loop_a = 0; G.loop_w = imax(Q8(4), iabs(r->vx) * 10 / 244);   /* rad/s Q8 */
        }
        if (G.loop_on) {                         /* the loop behind the start: three half turns, then away */
            int32_t R = Q8(CO_LOOPR) - ROCK_RM, cy = land_y(Q8(CO_LOOPX)) - Q8(CO_LOOPR + 1), a16;
            G.loop_w = imin(Q8(16), G.loop_w + dt * 9 * 256 / 1000);
            G.loop_a += G.loop_w * dt / 1000;    /* rad Q8 */
            a16 = (G.loop_a * 10430) >> 8;
            r->x = Q8(CO_LOOPX) - ((R * isin(a16)) >> 14); r->y = cy + ((R * icos(a16)) >> 14);
            r->th -= ((G.loop_w * dt / 1000) * 10430 * 5 / 2) >> 8; r->vx = 0; r->vy = 0;
            if (G.loop_a >= 2413) {              /* 3 pi */
                r->x = Q8(CO_LOOPX); r->y = cy - R; r->vx = Q8(600); r->vy = -Q8(430); r->g = 0; r->coy = 0; r->om = 18 * 10430;
                G.loop_on = 0; G.rocket = 2300; snd_whoosh(); intro_burst(r->x, r->y, 1, 1, T_WHEEE);
            }
        } else {
            G.acc += dt * 128;
            while (G.acc >= 1000) { G.acc -= 1000; rock_phys(r, knob, first ? jump : -1); first = 0; }
        }
        if (G.step_cd > 0) G.step_cd -= dt;
        if (live && G.race && G.step_cd <= 0 && (r->g || r->coy > 0) && r->vx < -Q8(20))
            for (i = 0; i < CO_NSTEP; i++)       /* a booster at the foot of a big step throws Rock up it */
                if (iabs((r->x >> 8) - CO_STEP[i].x) < 13) {
                    r->vx = -Q8(CO_STEP[i].vx); r->vy = -Q8(CO_STEP[i].vy); r->g = 0; r->coy = 0; G.step_cd = 1500; G.rocket = 900;
                    snd_whoosh(); sfx_ex(50, 0, 500, W_SAW, 2600, 3, 200, 0, 0, 65750); intro_burst(r->x, r->y, i, 1, T_WHEEE);
                    break;
                }
        if (live) intro_sky_hits(dt);
        if (live && G.race == 1 && r->x < Q8(RACE_START)) {   /* over the line: the clock runs */
            G.race = 2; G.race_ms = 0;
            sfx(72, 0, 200, W_SQUARE, 1800, 2, 100); sfx(79, 120, 200, W_SQUARE, 1800, 2, 100); sfx(84, 240, 500, W_SQUARE, 1800, 2, 100);
        }
        if (live && G.race == 2) {
            G.race_ms += dt;
            if (r->x < Q8(RACE_END)) {           /* home: the time, and the best one kept */
                uint32_t k2;
                race_last_cs = (uint32_t)G.race_ms / 10u; if (!race_last_cs) race_last_cs = 1;
                race_new = !race_best_cs || race_last_cs < race_best_cs;
                if (race_new) { race_best_cs = race_last_cs; race_save_due = 1; }
                G.race = 0; G.race_show = 7000;   /* the course is the game's own again */
                for (k2 = 0; k2 < sizeof G.pad_used; k2++) G.pad_used[k2] = 0;
                for (k2 = 0; k2 < sizeof G.sky_used; k2++) { G.sky_used[k2] = 0; G.sky_sq[k2] = 0; }
                snd_jingle(); intro_burst(r->x, r->y, 2, 1, T_FINISH);
            }
        }
        if (G.race_show > 0) G.race_show -= dt;
        /* the hollow behind the start is hidden by a rock face until Rock is up on the ledge and rolls into it */
        G.veil = iclamp(G.veil + (r->x < Q8(CO_WALLX - 16) ? -dt : dt) * 256 / 450, 0, 256);
        if (!G.found && r->x < Q8(CO_CARDX + 110)) { G.found = 1; snd_jingle(); }
        if (G.rocket > 0) {
            G.rocket -= dt;
            if (G.ntrail < 20u) { G.trail[G.ntrail].x = r->x; G.trail[G.ntrail].y = r->y; G.trail[G.ntrail].t = 0; G.ntrail++; }
        }
        for (i = 0; i < (int32_t)G.ntrail; i++) G.trail[i].t += dt;
        while (G.ntrail && G.trail[0].t > 500) { for (i = 1; i < (int32_t)G.ntrail; i++) G.trail[i - 1] = G.trail[i]; G.ntrail--; }
        if (r->jumped) {
            G.pad_jump = (uint8_t)lock;
            if (lock) { G.clean++; intro_burst(r->x, r->y, G.clean, 0, 0); snd_chime(0); }
        }
        if (r->touch) {
            if (G.pad_jump && d * r->t_vt > 0) {
                int32_t sl = r->t_sl, n = (int32_t)isqrt((uint32_t)(65536 + sl * sl)), b = d * (G.exact ? Q8(150) : Q8(55));
                r->vx += b * 256 / n; r->vy += ((b * sl) >> 8) * 256 / n;
                if (G.exact) { G.bonus++; intro_burst(r->x, r->y, G.bonus, 1, T_BONUS); snd_chime(1); }
            }
            G.pad_jump = 0;
        }
        if (r->thud) sfx_ex(40, 0, 160, W_SINE, 2600, 2, 30, 0, 0, 64700);
        sy_rumble_want = (live && r->g) ? imin(40, iabs(r->vx) >> 11) : 0;
        sy_rumble_pitch = 90 + (iabs(r->vx) >> 9);
        if (G.burst.on) { G.burst.t += dt; if (G.burst.t > 600) G.burst.on = 0; }
        bt = (G.t + (int32_t)(ihash((uint32_t)(G.t / 3700)) * 2000u >> 16)) % 3700;
        G.eyes = bt < 160 ? iabs(bt - 80) * 256 / 80 : 256;
        G.c_off += (((G.race ? r->vx < Q8(25) : r->vx < -Q8(25)) ? (G.race ? Q8(156) : Q8(150)) : Q8(84)) - G.c_off) * imin(400, dt * 5 / 2) / 400;
        G.cam_y += (r->y - Q8(130) - G.cam_y) * imin(200, dt) / 200;
        G.camz = 256; G.camx = iclamp(r->x - G.c_off, Q8(CO_CAMMIN), CAMMAX); G.camy = G.cam_y;
        if (live) {
            if (!G.turned && G.t > 5000) G.hint = imin(256, G.hint + dt * 384 / 1000); else G.hint = imax(0, G.hint - dt * 512 / 1000);
            if (r->x > Q8(CO_XS) && !G.done && !G.race) G.on_course = 1;
            if (r->x > Q8(CO_L - 64)) { G.done = 1; G.on_course = 0; G.cave_t += dt; G.cave_a = imin(256, G.cave_a + dt * 384 / 1000); }
            else { G.cave_a = imax(0, G.cave_a - dt * 512 / 1000); if (!G.cave_a) G.cave_t = 0; }
            if (G.on_course) G.course_t += dt;
        } else {
            G.fade = imin(256, G.t * 256 / 1400);
            sy_music_want = imax(0, 256 - G.t * 256 / 1400);
            if (G.t > 1600) { G.phase = IP_DONE; sy_rumble_want = 0; return 1; }
        }
    }
    /* the lights: PLAY breathes when it is wanted; near a pad every key calls */
    if (intro_want_play()) led_play_breathe = 1;
    /* the controls box stands by the start: while it is on the screen HOME breathes (arcave.c opens the sheet) */
    if (G.phase == IP_PLAY && !G.race && G.race_show <= 0 && (G.camx >> 8) < CTRL_X + 118 && (G.camx >> 8) + AC_W > CTRL_X - 118) led_breathe_want |= 1u << BTN_HOME;
    if (G.phase == IP_PLAY && G.near > 0) {
        if (G.on_pad || tsin(in_time, 120, 0) > 0) led_key_want = (1u << AC_NKEY) - 1u;
        else led_key_dim_want = (1u << AC_NKEY) - 1u;
    }
    led_key_want |= in.keys;
    return 0;
}

/* ---------------------------------------------------------------- drawing */
static void race_str(char *b, uint32_t cs)       /* hundredths of a second -> M:SS.CC */
{
    uint32_t m = cs / 6000u, n = 0;
    if (m >= 10u) b[n++] = (char)('0' + m / 10u % 10u);
    b[n++] = (char)('0' + m % 10u); b[n++] = ':';
    b[n++] = (char)('0' + cs / 1000u % 6u); b[n++] = (char)('0' + cs / 100u % 10u); b[n++] = '.';
    b[n++] = (char)('0' + cs / 10u % 10u); b[n++] = (char)('0' + cs % 10u); b[n] = 0;
}
/* a line across the course: a post of squares, its word on a flag */
static void race_post(int32_t px, int32_t py, const char *s, col_t c)
{
    int32_t k, a = ts(1, 2), w = gtextw(s, a) + 8, h = 5 * a + 8;
    for (k = 0; k < 9; k++) grect(px - 2 + (k & 1) * 2, py - 4 - k * 4, 2, 4, th->text);
    for (k = 0; k < 9; k++) grect(px - (k & 1) * 2, py - 4 - k * 4, 2, 4, th->ground);
    grect(px - 1, py - 42 - h, w, h, th->ground); gbox(px - 1, py - 42 - h, w, h, c);   /* the flag flies to the right of its post */
    gtext(px + 3, py - 38 - h, s, c, a);
}
static void T8(const char *s, int32_t x, int32_t y, col_t c, int32_t a) { galpha(a); gtextc(x, y - 2, s, c, 1); galpha(256); }
/* small print as a centred block with its top at y, in the size the settings ask for (two lines at most twice the
 * first build's: the intro's pictures need the room); its height */
static int32_t TB(const char *s, int32_t cap, int32_t y, col_t c, int32_t a)
{
    int32_t h;
    galpha(a);
    h = gblock(4, y, 232, s, c, ts(1, cap), 1);
    galpha(256);
    return h;
}

static int32_t far_f1(int32_t u) { return (22 * isin(u * 149) + 9 * isin(u * 453 + 10430)) >> 14; }
static int32_t far_f2(int32_t u) { return (28 * isin(u * 110 + 20860) + 8 * isin(u * 336)) >> 14; }
static void far_layer(int32_t tlx, int32_t tly, int32_t par256, int32_t base, int which, col_t col, int fill)
{
    int32_t sx, py = 0;
    for (sx = 0; sx < AC_W; sx++) {
        int32_t u = sx + tlx * par256 / 256, y = base - tly * par256 * 12 / 25600 + (which ? far_f2(u) : far_f1(u));
        if (fill) { int32_t j; for (j = imax(y, 0); j < AC_H; j++) fb_px[j * AC_W + sx] = col; }
        else if (sx == 0) gp(sx, y, col);
        else { int32_t a = imin(py, y), b = imax(py, y), j; for (j = a + (b > a && y > py); j <= b - (b > a && y < py); j++) gp(sx, j, col); }
        py = y;
    }
}

/* the tip at the foot of the title: "change language" between OCT- and OCT+ until one of them is pressed, the
 * language's name after. arcave.c draws it over the title and for three seconds past it. a: 0..256 */
static uint8_t lang_touched;
static void lang_tip_draw(int32_t a)
{
    const char *s = TR(lang_touched ? T_LANG : T_CHANGE_LANG);
    int32_t wd = gtextw(s, 1), sc = ts(1, 2);
    if (a <= 0) return;
    if (sc == 1) {                               /* small: all on one line, the words between the two buttons */
        T8(s, 121, 222, th->text, a);
        T8("\xE2\x86\x90 OCT-", 121 - wd / 2 - 26, 222, th->mute, a);
        T8("OCT+ \xE2\x86\x92", 121 + wd / 2 + 26, 222, th->mute, a);
        return;
    }
    galpha(a);                                   /* larger: the words, and under them the two buttons left and right */
    gfit(120, 206, 232, s, th->text, sc);
    gtext(8, 225, "\xE2\x86\x90 OCT-", th->mute, sc);
    gtextr(232, 225, "OCT+ \xE2\x86\x92", th->mute, sc);
    galpha(256);
}

static void intro_draw_title(void)
{
    int32_t tt = G.t, d = smooth8(0, 1150, tt), rise = (256 - d) * 12 / 256, open, lk = 0, i, w = 0, x0, sweep;
    int32_t up = ts(1, 2) > 1 ? 26 : 0;          /* larger text under the name: the hat and the name move up for it */
    const char *s = "FM ARCAVE";
    char one[2] = {0, 0};
    int32_t xs[16], n = 0;
    if (th->dark) { sky_draw(); gfade(256 - 179 * smooth8(0, 1500, tt) / 256); }
    else gclear(th->bg);
    open = tt < 1300 ? 0 : tt < 1600 ? (tt - 1300) * 256 / 300 : (tt > 2700 && tt < 2920) ? iabs(tt - 2810) * 256 / 110 : 256;
    if (tt > 1750 && tt < 2450) lk = isin((tt - 1750) * 65536 / 700) >> 6;
    if (tt > 2810 && tt < 3700) {
        int32_t a = (tt - 2810) * 256 / 900;
        for (i = 0; i < 3; i++) { galpha((256 - a) * (256 - i * 77) / 256); gcirc(120, 104 - up, 8 + i * 9 + a * 90 / 256, th->caps[i]); }
        galpha(256);
    }
    hat_draw(120, 108 + rise - up, 256, th->hat, th->hat_eye, th->hat_pupil, open, lk, th->dark, th->hat_rim, d);
    /* the name drops in letter by letter, in the colours of the caps, then settles to white with a sweep */
    for (i = 0; s[i]; i++) { one[0] = s[i]; xs[n++] = w; w += gtextw(one, 3) + 3 + (s[i] == ' ' ? 3 : 0); }
    x0 = 121 - (w - 3) / 2;
    sweep = (tt - 3300) * 16 / 1000;
    for (i = 0; s[i]; i++) {
        int32_t a = (tt - 1500 - i * 110) * 256 / 280, e, y;
        if (a <= 0) continue;
        e = imin(256, a); y = 152 - up - (256 - e) * (256 - e) * 10 / 65536;
        one[0] = s[i];
        galpha(imin(256, a * 2));
        gtext(x0 + xs[i], y, one, (a < 333 || iabs(sweep - i) < 2) ? th->caps[i % 5] : th->text, 3);
    }
    galpha(256);
    TB(TR(T_TAGLINE), 2, up ? 150 : 174, th->dark ? HEXC(0x7fe6d2) : HEXC(0xf2b45a), smooth8(2900, 3600, tt));
    gfade(G.fade);
}

static void intro_draw(void)
{
    int32_t z = G.camz, tlx = G.camx >> 8, tly = G.camy >> 8, sx, i, k, x0, x1, ca = 0, prog;
    static int16_t gy[AC_W];
    rock_t *r = &G.r;
    uint32_t tm = in_time;
#define WX(x) ((((x) - G.camx) * z) >> 16)
#define WY(y) ((((y) - G.camy) * z) >> 16)
#define ZS(v) (((v) * z) >> 8)
    if (G.phase == IP_TITLE) { intro_draw_title(); return; }
    sky_draw();
    if (G.phase == IP_PLAY || G.phase == IP_OUT) ca = imax(0, (land_y((tlx + 84) << 8) >> 8) - 130 - tly);
    if (ca > 25) {                               /* the higher the camera climbs, the more of the upper sky comes down */
        for (k = 0; k < 3; k++) {
            int32_t py = 0;
            galpha(imin(192, (ca - 25) * 256 / 90) * (256 - k * 51) / 256);
            for (sx = 0; sx < AC_W; sx += 2) {
                int32_t y = -34 + ca * 55 / 100 + k * 11 + ((8 * (isin(sx * 274 + (int32_t)(tm * 6u) + k * 17731 + tlx * 42) ) + 4 * isin(sx * 802 - (int32_t)(tm * 10u) + k * 10430)) >> 14);
                if (sx) gline2(sx - 2, py, sx, y, th->caps[(k * 2) % 5]);
                py = y;
            }
        }
        galpha(256);
    }
    if (ca > 100) {                              /* an eye in the sky */
        int32_t ex = 174, ey = -62 + ca * 42 / 100, blink = ((tm + 1000u) % 5000u) < 160u, op = imin(256, (ca - 100) * 256 / 80) * (blink ? 38 : 256) / 256;
        gcirc(ex, ey, 24, th->pad);
        for (k = 0; k < 12; k++) {
            int32_t a = (int32_t)(tm * 3u) + k * 5461, l = 32 + (tsin(tm, 20, k * 10430) >> 13);
            gline(ex + ((28 * icos(a)) >> 14), ey + ((28 * isin(a)) >> 14), ex + ((l * icos(a)) >> 14), ey + ((l * isin(a)) >> 14), th->pad);
        }
        hat_draw(ex, ey + 3, 256, 0, th->pad, th->bg, op, -128, 2, 0, 0);
    }
    if (ca > 180)
        for (k = 0; k < 3; k++) {
            int32_t q = (int32_t)((tm * 45u / 100u + (uint32_t)k * 370u) % 1000u), x = 260 - q * 320 / 1000 + k * 30, y = -50 + ca * 30 / 100 + k * 16 + q * 70 / 1000;
            galpha(isin(q * 32768 / 1000) >> 6); gline(x, y, x + 16, y - 6, th->text);
        }
    galpha(256);
    far_layer(tlx, tly, 46, 150, 0, th->far1, th->fill);
    /* giant far mushrooms, thicker toward the cave */
    for (k = (tlx * 107 / 256 - 40) / 84 - 1; k <= (tlx * 107 / 256 + 280) / 84; k++) {
        int32_t wx = k * 84 * 256 / 107, u, bx, by, s;
        uint32_t need;
        if (k < 3) continue;
        need = (uint32_t)imax(0, (wx * 100 / CO_L - 18) * 819);
        if (ihash((uint32_t)k + 1300u) > need) continue;
        u = k * 84 + (int32_t)(ihash((uint32_t)k) * 40u >> 16);
        bx = u - tlx * 107 / 256; by = 178 - tly * 107 * 12 / 25600 + far_f2(u);
        s = 230 + (int32_t)(ihash((uint32_t)k + 60u) * 333u >> 16);
        galpha(th->dark ? 128 : 192);
        mush_draw(bx, by + 6, s, th->caps[ihash((uint32_t)k + 9u) * 5u >> 16], th->far_stem, 102 + (tsin(tm, 13, k * 10430) * 102 >> 14));
        galpha(256);
    }
    {   /* the dome, far off, near the end */
        int32_t u = (CAMMAX >> 8) * 107 / 256 + 112, dx = u - tlx * 107 / 256, by = 178 - tly * 107 * 12 / 25600 + far_f2(u) + 4;
        if (dx > -60 && dx < 300) {
            static const uint8_t DR[3][2] = {{42, 8}, {29, 6}, {15, 4}};
            galpha(179);
            for (i = 0; i < 3; i++) {
                int32_t px = 0, py = 0, m = DR[i][1];
                for (k = 0; k <= m; k++) {
                    int32_t a = 32768 + k * 32768 / m, x = dx + ((DR[i][0] * icos(a)) >> 14), y = by + ((DR[i][0] * isin(a)) >> 14);
                    if (k) gline(px, py, x, y, th->pad);
                    if (i < 2) {
                        int32_t m2 = DR[i + 1][1], k2 = imin(m2, (k * m2 + m / 2) / m), a2 = 32768 + k2 * 32768 / m2;
                        gline(x, y, dx + ((DR[i + 1][0] * icos(a2)) >> 14), by + ((DR[i + 1][0] * isin(a2)) >> 14), th->pad);
                    }
                    px = x; py = y;
                }
            }
            galpha(64 + (tsin(tm, 20, 0) * 38 >> 14));
            gellf(dx, by, 13, 13, th->caps[(tm / 1000u) % 5u], 1);
            galpha(256);
        }
    }
    far_layer(tlx, tly, 107, 178, 1, th->far2, th->fill);
    /* the ground: filled, then its coloured seams, then the rim */
    for (sx = 0; sx < AC_W; sx++) {
        int32_t y = WY(land_y(G.camx + (sx << 16) / z)), j;
        gy[sx] = (int16_t)iclamp(y, -2000, 2000);
        for (j = imax(y, 0); j < AC_H; j++) fb_px[j * AC_W + sx] = th->ground;
    }
    prog = imax(0, (tlx + (120 << 8) / z) * 5 / CO_L);
    for (k = 4; k >= 0; k--) {
        static const uint8_t BD[5] = {3, 7, 12, 18, 26}, BA[5] = {218, 166, 128, 92, 61};
        int32_t off = BD[k] * imax(154, z) / 256, on = ZS(7 + k * 5), per = on + ZS(4 + k * 4), sh = ((tlx * z) >> 8) + k * 13;
        col_t c = th->bands[(k + (th->dark ? prog : 0)) % 5];
        if (per < 2) per = 2;
        galpha(BA[k]);
        for (sx = 0; sx < AC_W; sx++)
            if (((sx + sh) % per + per) % per < on) {
                int32_t a = gy[sx] + off, b = sx ? gy[sx - 1] + off : a, j;
                for (j = imin(a, b); j <= imax(a, b); j++) gp(sx, j, c);
            }
    }
    galpha(256);
    for (sx = 0; sx < AC_W; sx++) {
        int32_t a = gy[sx], b = sx ? gy[sx - 1] : a, j;
        for (j = imin(a, b) + (a != b && a > b); j <= imax(a, b) - (a != b && a < b); j++) gp(sx, j, th->rim);
    }
    x0 = tlx - 30; x1 = tlx + (240 << 8) / z + 30;
    for (i = 0; i < CO_NSHROOM; i++) {
        const co_shroom_t *m = &CO_SHROOM[i];
        if (m->x < x0) continue;
        if (m->x > x1) break;
        mush_draw(WX(m->x << 8), WY(land_y(m->x << 8)) + 1, ZS(m->s), th->caps[m->c], th->stem, 128 + (tsin(tm, 20, m->ph << 8) >> 7));
    }
    for (i = 0; i < CO_NPOOL; i++) {             /* pools of light in the late valleys */
        const co_pool_t *o = &CO_POOL[i];
        int32_t lv, wx;
        if (o->b < x0 || o->a > x1) continue;
        lv = WY(o->lv << 4);
        galpha(154);
        for (sx = imax(0, WX(o->a << 8)); sx <= imin(AC_W - 1, WX(o->b << 8)); sx++) {
            int32_t j;
            for (j = lv; j < gy[sx]; j++) gp(sx, j, th->caps[(o->c + tm * 7u / 10000u) % 5u]);
        }
        galpha(256);
        for (k = 0; k < 2; k++) {
            wx = o->a + (o->b - o->a) * (int32_t)((tm * 15u / 100u + (uint32_t)k * 500u) % 1000u) / 1000;
            grect(WX(wx << 8), lv, imax(1, ZS(3)), 1, th->pad);
        }
    }
    for (i = 0; i < CO_NPROP; i++) {
        const co_prop_t *o = &CO_PROP[i];
        int32_t px, py, near;
        if (o->x < x0 || o->x > x1) continue;
        px = WX(o->x << 8); py = WY(land_y(o->x << 8));
        near = iabs((r->x >> 8) - o->x) < 70 && G.phase == IP_PLAY;
        if (o->t == 0) {                         /* a sleeping rock friend: it looks up as you pass */
            int32_t s = (int32_t)o->n, e = near ? (tsin(tm, 30, o->x * 10430) > 15700 ? 26 : 256) : 0;
            rock_draw(px, py - ((66 * s / 10) * z >> 16), (z * s) >> 8, (land_sl(o->x << 8) * 6 / 10 * 10430) >> 8, e,
                      th->rock, th->rock_rim, th->dark, th->caps[o->c], th->eye, th->pupil);
        } else if (o->t == 1) {
            for (k = 0; k < (int32_t)o->n; k++) {
                int32_t h = ZS(8 + (int32_t)(ihash((uint32_t)(o->x * 7 + k)) * 16u >> 16)), w = ZS(3), bx = px + ZS(k * 7 - 7), xy[6];
                xy[0] = bx - w; xy[1] = py + 1; xy[2] = bx + ZS((int32_t)(ihash((uint32_t)(o->x * 7 + k + 3)) * 4u >> 16) - 2); xy[3] = py - h; xy[4] = bx + w; xy[5] = py + 1;
                gpoly(xy, 3, th->dark ? 0 : th->bands[k % 3]);
                gpolyline(xy, 3, 1, th->caps[(o->c + k) % 5]);
            }
        } else if (o->t == 2) {
            galpha(near ? 256 : 141);
            garc(px, py - ZS(21), ZS(9), ZS(22), 0, 65536, th->caps[o->c], near);
            galpha(256);
        } else if (o->t == 3) {
            gline(px, py, px, py - ZS(18), th->stem); gline(px, py - ZS(18), px + ZS(5), py - ZS(18), th->stem);
            galpha(64); gdisc(px + ZS(5), py - ZS(14), ZS(7), th->caps[o->c]); galpha(256);
            grect(px + ZS(4), py - ZS(16), imax(1, ZS(2)), imax(1, ZS(3)), th->caps[o->c]);
        } else if (z == 256) {
            const char *s = TR(T_SIGN1 + o->n);
            int32_t a = ts(1, 2), w = gtextw(s, a) + 8, h = 5 * a + 8;
            gline(px, py, px, py - 22, th->stem);
            grect(px - w / 2, py - 21 - h, w, h, th->ground); gbox(px - w / 2, py - 21 - h, w, h, th->pad);
            gtextc(px + 1, py - 17 - h, s, th->text, a);
        }
    }
    if (x0 < -250) {                             /* the hollow behind the start: the Hat, the old name, the loop */
        int32_t px = WX(Q8(CO_CARDX)), py = WY(land_y(Q8(CO_CARDX))), lx = WX(Q8(CO_LOOPX)), ly = WY(land_y(Q8(CO_LOOPX)) - Q8(CO_LOOPR + 1));
        int32_t bl = (tm % 4300u) < 180u ? iabs((int32_t)(tm % 4300u) - 90) * 256 / 90 : 256;
        hat_draw(px, py - ZS(12), (230 * z) >> 8, th->dark ? HEXC(0x0c1216) : th->hat, th->hat_eye, th->hat_pupil, bl,
                 iclamp(((r->x >> 8) - CO_CARDX) * 256 / 80, -256, 256), th->dark, th->hat_rim, 256);
        if (z == 256) {
            int32_t big = ts(1, 2) > 1;
            gtextc(px, py - (big ? 128 : 103), "THE MYSTIC ARCAVE", th->text, 2);
            TB(TR(T_TAGLINE), 2, py - (big ? 114 : 90), th->caps[0], 256);
        }
        garc(lx, ly, ZS(CO_LOOPR), ZS(CO_LOOPR), 0, 65536, th->pad, 1);
        for (k = 0; k < 5; k++) {
            int32_t a = -(int32_t)(tm * 25u) + k * 13107;
            garc(lx, ly, ZS(CO_LOOPR + 4), ZS(CO_LOOPR + 4), a, a + 7300, th->caps[k], 1);
        }
    }
    if (G.veil > 0 && x0 < CO_WALLX) {            /* a rock face over the hollow: the ledge looks like a shelf in a cliff */
        static const int16_t VF[6][2] = {{-20, 3}, {-26, -38}, {-21, -86}, {-33, -150}, {-27, -236}, {-44, -420}};
        int32_t xy[18];
        for (i = 0; i < 6; i++) { xy[2 * i] = WX(Q8(CO_WALLX + VF[i][0])); xy[2 * i + 1] = WY(Q8(CO_LEDGE + VF[i][1])); }
        xy[12] = WX(Q8(CO_WALLX - 1100)); xy[13] = WY(Q8(CO_LEDGE - 420));
        xy[14] = WX(Q8(CO_WALLX - 1100)); xy[15] = WY(Q8(CO_LEDGE + 500));
        xy[16] = xy[0]; xy[17] = WY(Q8(CO_LEDGE + 500));
        galpha(G.veil);
        gpoly(xy, 9, th->mount); gpolyline(xy, 6, 0, th->rim);
        galpha(256);
    }
    if (z == 256 && (G.race || G.race_show > 0)) {   /* the two lines of the race back */
        if (x0 < RACE_START + 40 && x1 > RACE_START - 40) race_post(WX(Q8(RACE_START)), WY(land_y(Q8(RACE_START))), TR(T_RACE_START), th->pad);
        if (x0 < RACE_END + 40 && x1 > RACE_END - 40) race_post(WX(Q8(RACE_END)), WY(land_y(Q8(RACE_END))), TR(T_FINISH), th->caps[3]);
    }
    if (G.race)
        for (i = 0; i < CO_NSTEP; i++) {         /* the boosters: chevrons running up and to the left */
            int32_t j;
            if (CO_STEP[i].x < x0 || CO_STEP[i].x > x1) continue;
            for (j = 0; j < 3; j++) {
                int32_t bx = CO_STEP[i].x + 9 - j * 9, by = WY(land_y(Q8(bx))) - 4;
                galpha(G.step_cd > 0 ? 256 : 90 + ((((int32_t)(tm / 120u) + 3 - j) % 3) == 0 ? 166 : 0));
                gline2(WX(Q8(bx)) + 3, by - 4, WX(Q8(bx)), by, th->pad); gline2(WX(Q8(bx)), by, WX(Q8(bx)) + 3, by + 3, th->pad);
            }
            galpha(256);
        }
    if ((G.phase == IP_PLAY || G.phase == IP_OUT) && !G.race && G.race_show <= 0 && z == 256 && x0 < CTRL_X + 118 && x1 > CTRL_X - 118) {
        /* "for controls press the HOME button", HOME drawn as the button is; it stands in the land, so rolling on leaves it behind */
        /* its words wrap at the size the settings ask for, in every language: only a word too wide for the screen
           makes the whole box smaller */
        const char *s = TR(T_CTRL_TIP);
        char wd[10][20];
        int32_t ww[10], ln[10], lw[6], nw = 0, nl = 0, n = 0, a = ts(1, 2), w = 0, h, bx, by, k, sp, bw, lx = 0;
        for (; ; s++) {
            if (*s == ' ' || *s == '|' || !*s) { if (n) { wd[nw][n] = 0; nw++; n = 0; } if (!*s || nw == 10) break; }
            else if (n < 18) wd[nw][n++] = *s;
        }
        for (;; a--) {
            sp = gtextw(" ", a); bw = gtextw("HOME", a) + 6 * a; nl = 0; w = 0;
            for (k = 0; k < nw; k++) {
                const char *q = wd[k]; char *at = 0; int32_t j;
                for (j = 0; q[j]; j++) if (q[j] == '@') at = &wd[k][j];
                if (at) { *at = 0; ww[k] = gtextw(wd[k], a) + bw + gtextw(at + 1, a) + (at[1] ? a : 0) + (wd[k][0] ? a : 0); *at = '@'; }
                else ww[k] = gtextw(q, a);
                if (k == 0 || lw[nl - 1] + sp + ww[k] > 216) { if (nl == 6) break; lw[nl++] = ww[k]; } else lw[nl - 1] += sp + ww[k];
                ln[k] = nl - 1;
                w = imax(w, lw[nl - 1]);
            }
            if ((w <= 216 && k == nw) || a == 1) break;
        }
        w += 12; h = nl * (TLH(a) + 2 * a) + 6;
        bx = WX(Q8(CTRL_X)) - w / 2; by = WY(land_y(Q8(CTRL_X))) - 58 - h;
        grect(bx, by, w, h, th->panel); gbox(bx, by, w, h, th->rim);
        for (k = 0, n = -1; k < nw; k++) {
            int32_t ty = by + 4 + 2 * a + ln[k] * (TLH(a) + 2 * a), j;
            char *at = 0;
            if (ln[k] != n) { n = ln[k]; lx = bx + (w - lw[n]) / 2; }
            for (j = 0; wd[k][j]; j++) if (wd[k][j] == '@') at = &wd[k][j];
            if (!at) gtext(lx, ty, wd[k], th->text, a);
            else {                               /* HOME, drawn as the button is */
                int32_t x = lx;
                *at = 0;
                if (wd[k][0]) { gtext(x, ty, wd[k], th->text, a); x += gtextw(wd[k], a) + a; }
                grect(x, ty - 2 * a, bw, 9 * a, th->bg); gbox(x, ty - 2 * a, bw, 9 * a, th->pad);
                gtext(x + 3 * a, ty, "HOME", th->pad, a); x += bw + a;
                gtext(x, ty, at + 1, th->text, a);
                *at = '@';
            }
            lx += ww[k] + sp;
        }
    }
    for (i = 0; i < I_NPAD; i++) {               /* launch pads: five white piano keys set into the slope. Nothing floats
                                                    over them (a floating key read as something to collect); the key under
                                                    Rock goes down, and the next pad's keys go down one after another */
        const co_pad_t *p = &I_PAD[i];
        int32_t d, dir = I_DIR, rx = r->x >> 8;
        int next;
        if (p->x < x0 || p->x > x1) continue;
        next = !G.pad_used[i] && G.phase == IP_PLAY && G.near > 0 && dir * (p->x - rx) > -40 && dir * (p->x - rx) < 200;
        galpha(G.pad_used[i] ? 77 : 256);
        for (d = -17; d <= 7; d++) {
            int32_t key = (d + 17) / 5, wx = p->x + dir * d, sink = 0;
            if ((d + 17) % 5 == 4) continue;     /* the gap between two keys */
            if (r->g && iabs(rx - (p->x + dir * (key * 5 - 15))) < 5) sink = 2;
            else if (next && (int32_t)(tm / 110u) % 8 == key) sink = 1;
            grect(WX(wx << 8), WY(land_y(wx << 8)) - ZS(2) + ZS(sink), imax(1, ZS(1)), imax(2, ZS(7)), th->pad);
        }
        galpha(256);
    }
    if (G.burst.on) {
        int32_t a = G.burst.t * 256 / 600, bx = WX(G.burst.x), by = WY(G.burst.y);
        for (i = 0; i < 3; i++) {
            galpha(imax(0, 256 - a) * (256 - i * 64) / 256);
            gcirc(bx, by, ZS(6 + i * 7 + a * (G.burst.big ? 52 : 26) / 256), th->caps[(i + G.burst.c) % 5]);
        }
        if (G.burst.big) { galpha(imax(0, 256 - a)); gfit(bx, by - 16 - 5 * ts(1, 3) - a * 10 / 256, 232, TR(G.burst.label ? G.burst.label : T_BONUS), th->pad, ts(1, 3)); }
        galpha(256);
    }
    if (x1 > CO_L - 20) {                        /* the mountain and the cave */
        static const int16_t MT[9][2] = {{-2, 3}, {5, -42}, {1, -92}, {13, -150}, {7, -232}, {24, -420}, {320, -420}, {320, 320}, {-2, 320}};
        static const uint8_t ARC[4][3] = {{20, 34, 230}, {15, 27, 179}, {10, 20, 128}, {5, 12, 90}};
        static const int16_t MM[4][3] = {{2, -30, 51}, {4, -52, 41}, {46, -8, 77}, {44, -38, 46}};
        int32_t xy[18], yend = CO_YEND_Q4 << 4, cx = WX(Q8(CO_L + 24)), cy = WY(yend + Q8(1));
        for (i = 0; i < 9; i++) { xy[2 * i] = WX(Q8(CO_L + MT[i][0])); xy[2 * i + 1] = WY(yend + Q8(MT[i][1])); }
        gpoly(xy, 9, th->mount); gpolyline(xy, 6, 0, th->rim);
        for (i = 0; i < 4; i++) {
            galpha(ARC[i][2]);
            garc(cx, cy, ZS(ARC[i][0]), ZS(ARC[i][1]), 32768, 65536, th->caps[(i + tm * 12u / 10000u) % 5u], 1);
        }
        galpha(256);
        for (i = 0; i < 4; i++)
            mush_draw(WX(Q8(CO_L + MM[i][0])), WY(yend + Q8(MM[i][1])), ZS(MM[i][2]), th->caps[(i + 1) % 5], th->stem, 128 + (tsin(tm, 20, i * 10430) >> 7));
    }
    for (i = 0; i < I_NSKY; i++) {
        const co_sky_t *o = &I_SKY[i];
        int32_t px, py;
        if (o->x < x0) continue;
        if (o->x > x1) break;
        if (G.sky_used[i] && o->t != 1) continue;
        px = WX(o->x << 8); py = WY(o->y << 8);
        if (py < -30 || py > 270) continue;
        if (o->t == 0) {
            galpha(64); gdisc(px, py, ZS(4) + (tsin(tm, 50, o->x * 10430) > 0), th->caps[o->c]); galpha(256);
            grect(px - 1, py - 1, 2, 2, th->caps[o->c]);
        } else if (o->t == 1) {                  /* a jelly cap: squashes as it throws you */
            int32_t q = G.sky_sq[i] > 0 ? G.sky_sq[i] * 256 / 250 : 0, rx = ZS(15) * (256 + q * 3 / 10) / 256, ry = ZS(10) * (256 - q * 4 / 10) / 256;
            py += ZS(3) * q / 256;
            galpha(51); gdisc(px, py, ZS(22), th->caps[o->c]); galpha(256);
            gellf(px, py, rx, ry, th->caps[o->c], 1);
            for (k = -2; k <= 2; k++) {
                int32_t tx = px + rx * k / 3, sw = tsin(tm, 30, k * 10430) * 3 >> 14;
                gline(tx, py, tx + ZS(sw), py + ZS(7) * ry / imax(1, ZS(10)), th->caps[o->c]);
                gline(tx + ZS(sw), py + ZS(7) * ry / imax(1, ZS(10)), tx, py + ZS(12 + (tsin(tm, 20, k * 10430) >> 13)) * ry / imax(1, ZS(10)), th->caps[o->c]);
            }
            grect(px - ZS(7), py - ZS(6), imax(1, ZS(2)), imax(1, ZS(2)), th->stem); grect(px + ZS(3), py - ZS(8), imax(1, ZS(2)), imax(1, ZS(2)), th->stem);
        } else {
            garc(px, py, ZS(14), ZS(14), 0, 65536, th->pad, 1);
            for (k = 0; k < 8; k++) {
                int32_t a = (int32_t)(tm * 16u) + k * 8192;
                grect(px + ((ZS(20) * icos(a)) >> 14) - 1, py + ((ZS(20) * isin(a)) >> 14) - 1, 2, 2, th->caps[k % 5]);
            }
        }
    }
    for (i = 0; i < (int32_t)G.ntrail; i++) {
        galpha(imax(0, 256 - G.trail[i].t * 256 / 500));
        grect(WX(G.trail[i].x) - 2, WY(G.trail[i].y) - 2, 4, 4, th->caps[iabs((G.trail[i].x >> 8) / 9) % 5]);
    }
    galpha(256);
    rock_draw(WX(r->x), WY(r->y), z, r->th, G.eyes, th->rock, th->rock_rim, th->dark, th->moss, th->eye, th->pupil);
    {   /* words in the sky */
        int32_t pulse = 97 + 159 * ipulse(tm, 2600) / 256;
        if (G.phase == IP_WIDE && G.t > 1000) {
            int32_t a = imin(256, (G.t - 1000) * 256 / 800) * pulse / 256;
            /* the first time the PLAY button is shown: nice and large, the words making room for it */
            {
                const char *q = TR(T_WAKE1);
                int in1 = 0;                     /* (which line the button is in: the second, in Turkish) */
                for (; *q; q++) if ((uint8_t)*q == 0xEEu) in1 = 1;
                if (ts(1, 2) == 1) { play_big = 1; T8(TR(T_WAKE1), 156, 32, th->text, a); T8(TR(T_WAKE2), 156, 50, th->text, a); }
                else { int32_t h; play_big = 2; h = TB(TR(T_WAKE1), 2, in1 ? 16 : 8, th->text, a); TB(TR(T_WAKE2), 2, (in1 ? 26 : 18) + h, th->text, a); }
                play_big = 0;
            }
        }
        if (G.phase == IP_PLAY && G.race == 1 && G.cave_a <= 0)
            TB(TR(T_RACE_GO), 3, 20, th->text, pulse);
        if (G.phase == IP_PLAY && G.race == 2) {  /* the clock, top middle */
            char b[12];
            int32_t a = ts(2, 3), w;
            race_str(b, (uint32_t)G.race_ms / 10u); b[6] = 0;   /* (tenths while it runs) */
            w = gtextw("0:00.0", a) + 12;
            grect(120 - w / 2, 4, w, 5 * a + 8, th->panel); gbox(120 - w / 2, 4, w, 5 * a + 8, th->pad);
            gtext(120 - w / 2 + 6, 8, b, th->text, a);
        }
        if (G.phase == IP_PLAY && G.race_show > 0) {   /* home: the time */
            char b[12];
            int32_t a = ts(2, 4), al = imin(256, G.race_show * 256 / 600), h;
            race_str(b, race_last_cs);
            galpha(al);
            h = TB(TR(T_RACE_TIME), 3, 18, th->mute, al);
            galpha(al); gtextc(120, 22 + h, b, th->text, a);
            if (race_new) gfit(120, 22 + h + 5 * a + 8 + 2 * ts(1, 3), 232, TR(T_NEWBEST), th->caps[3], ts(1, 3));
            galpha(256);
        }
        if (G.phase == IP_PLAY) {
            if (G.cave_a > 0) {
                int32_t h = TB(TR(T_ENTER), 3, 24, th->text, G.cave_a * pulse / 256);
                if (G.cave_t > 600) TB(TR(T_PRESSPLAY), 3, 26 + h, th->text, G.cave_a * pulse / 256);
            }
        }
    }
    gfade(G.fade);
#undef WX
#undef WY
#undef ZS
}
