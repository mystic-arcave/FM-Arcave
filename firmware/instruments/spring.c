/* SPDX-License-Identifier: GPL-3.0-only */
/* THE SPRING: an instrument for FM-ARCAVE's music room that is LOADED into a place of the room, not built into the
 * firmware (firmware/src/arcave/mi.h; docs/MACHINES.md). It is also the one to start a new instrument from.
 *
 * Warm water over sixteen vents in the floor of a pool. A vent that has something growing in it builds pressure and,
 * on a sixteenth, lets a bubble go. The bubble is silent: the sound comes when it meets the surface and pops, and it
 * is the sound of what grows in its vent.
 *
 *   PEBBLE  a plip                    REED   a whistle                 CLAY  a bloop of a bass note
 *   EMBER   a bright buzz             SHELL  a bell
 *
 *   KNOB 1 HEAT      how fast the pressure builds (small vents are quick, big ones slow: SIZE)
 *   KNOB 2 DEPTH     how deep the water is: the way up, one to nine sixteenths
 *   KNOB 3 CURRENT   a flow to the left or the right. Small bubbles are carried faster than big ones, and bubbles
 *                    that meet become one, which pops as one: two or three notes at once
 *   KNOB 4 FIZZ      little bubbles that go up with each one and get there first
 *   SELECT           lily pads on the water: a bubble under one waits, and on the beat the pad lets them all go
 *   PRESETS          the water: WATER, HONEY (slow and soft), SODA (quick), MUD (nothing meets)
 *   SEQ              the frog: on the beat it jumps in, and everything near it pops. Held: KNOB 1 PLACE,
 *                    KNOB 2 EVERY, KNOB 3 SPLASH, KNOB 4 CROAK
 *   bottom row       squeeze a vent: it sounds now, and again when its bubble comes up
 *   top row          a pebble thrown in at that place
 *
 * The rules of a loaded instrument, all kept here: no variables of its own (its world is the spg_t it is handed, its
 * tables are const), nothing of the firmware's by name (everything through a and c), no C library. */
#include "../src/arcave/mi.h"

#define SPG_NB 28                                 /* bubbles at once */
#define SPG_NR 6                                  /* rings on the water */
#define SPG_FLOOR 184
#define SPG_VX(s) (15 + (s) * 14)
enum { SPK_NONE, SPK_PEBBLE, SPK_REED, SPK_CLAY, SPK_EMBER, SPK_SHELL, SPK_N };
enum { SPV_PEBBLE, SPV_REED, SPV_CLAY, SPV_EMBER, SPV_SHELL, SPV_FIZZ, SPV_CROAK, SPV_SPLASH };   /* its voices */
enum { SPW_WATER, SPW_HONEY, SPW_SODA, SPW_MUD };

typedef struct {
    int32_t x;                                   /* Q8 px */
    uint32_t born, pop;                          /* the room's clock (ticks * 256) when it was let go, and when it is up */
    uint8_t on, r, n, held;                      /* 1 a bubble, 2 fizz; its radius; how many vents' bubbles it is; under a pad */
    uint8_t v[3], ord;                           /* those vents; (fizz) which of its bubble's it is */
} spg_bub_t;
typedef struct {
    spg_bub_t b[SPG_NB];
    uint16_t press[MI_NPART];                    /* each vent's pressure */
    uint8_t puff[MI_NPART];                      /* a vent that just let go, dying away */
    int16_t ring_x[SPG_NR]; uint16_t ring_t[SPG_NR];   /* a pop: a ring on the water, its age in ms (0: none) */
    int32_t frog_t;                              /* ms since the frog jumped (0: it sits) */
    uint8_t ring_i, inited;
} spg_t;

static const char *const SPG_KINDS[SPK_N] = {"NOTHING", "PEBBLE", "REED", "CLAY", "EMBER", "SHELL"};
static const uint8_t SPG_SND[SPK_N] = {MI_SILENT, SPV_PEBBLE, SPV_REED, SPV_CLAY, SPV_EMBER, SPV_SHELL};
static const char *const SPG_WATERS[4] = {"WATER", "HONEY", "SODA", "MUD"};
static const char *const SPG_ALGOS[7] = {"YOUR OWN", "FIRST POOL", "SLOW CLAY", "REED CHOIR", "PEBBLE RAIN", "EMBERS", "ALL OF IT"};
static const uint8_t SPG_PER[8] = {2, 3, 4, 6, 8, 12, 16, 24};   /* sixteenths between a vent's bubbles at HEAT 4, by its size */
static const uint16_t SPG_EVERY[4] = {96, 192, 384, 768};       /* the frog: ticks between jumps */

static int32_t spg_abs(int32_t v) { return v < 0 ? -v : v; }
static int32_t spg_clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static int32_t spg_surf(const mi_ctx_t *c) { return 150 - c->k[1] * 100 / 128; }   /* the water's top, px */
static mm_col_t spg_col(const mi_ctx_t *c, int kind)
{
    return kind == SPK_PEBBLE ? MM_RGB(176, 180, 200) : kind == SPK_REED ? MM_RGB(110, 220, 90) : kind == SPK_CLAY ? c->col[MMC_CAP0]
         : kind == SPK_EMBER ? MM_RGB(255, 140, 60) : kind == SPK_SHELL ? c->col[MMC_CAP3] : c->col[MMC_MUTE];
}
/* where a bubble is on its way up, px */
static int32_t spg_y(const mi_ctx_t *c, const spg_bub_t *b)
{
    int32_t span = (int32_t)(b->pop - b->born), go = (int32_t)(c->tq8 - b->born), top = spg_surf(c);
    if (b->held || go >= span) return top + b->r;
    if (go < 0) go = 0;
    return SPG_FLOOR - 4 - ((SPG_FLOOR - 4 - top - b->r) * (go >> 4)) / ((span >> 4) > 0 ? (span >> 4) : 1);
}
static int spg_pad_over(const mi_ctx_t *c, int32_t x)   /* a lily pad floats over x (px) */
{
    int n = c->r->sel, k;
    for (k = 0; k < n; k++) if (spg_abs(x - 240 * (2 * k + 1) / (2 * n)) <= 22) return 1;
    return 0;
}
static void spg_ring(spg_t *w, int32_t x)
{
    w->ring_x[w->ring_i % SPG_NR] = (int16_t)x; w->ring_t[w->ring_i % SPG_NR] = 1; w->ring_i++;
}
static spg_bub_t *spg_free(spg_t *w)
{
    int i;
    for (i = 0; i < SPG_NB; i++) if (!w->b[i].on) return &w->b[i];
    return 0;
}
/* vent s lets a bubble go at `born` (and the fizz that goes with it) */
static void spg_emit(spg_t *w, mi_ctx_t *c, int s, uint32_t born)
{
    const mi_inst_t *r = c->r;
    uint32_t rise = 384u * (uint32_t)(16 + c->k[1]);   /* 24 ticks * 256 for each sixteenth of depth */
    int i, nf = (c->k[3] + 8) / 43 + (r->preset == SPW_SODA);
    spg_bub_t *b = spg_free(w);
    if (r->preset == SPW_HONEY) rise *= 2u; else if (r->preset == SPW_SODA) rise /= 2u;
    if (!b) return;
    b->on = 1; b->x = SPG_VX(s) << 8; b->born = born; b->pop = born + rise; b->held = 0;
    b->r = (uint8_t)(3 + r->p[s].size / 2 + (r->preset == SPW_HONEY ? 2 : 0)); b->n = 1; b->v[0] = (uint8_t)s;
    w->puff[s] = 255;
    for (i = 0; i < nf && i < 3; i++) {
        spg_bub_t *f = spg_free(w);
        if (!f) return;
        f->on = 2; f->x = (SPG_VX(s) + i * 5 - 4) << 8; f->born = born; f->pop = born + rise * (uint32_t)(i + 1) / (uint32_t)(nf + 1);
        f->held = 0; f->r = 1; f->n = 1; f->v[0] = (uint8_t)s; f->ord = (uint8_t)i;
    }
}
/* a bubble meets the surface (or the frog, or a pebble): the vents it came from are struck */
static void spg_pop(spg_t *w, mi_ctx_t *c, const mi_api_t *a, spg_bub_t *b, int32_t vel)
{
    const mi_inst_t *r = c->r;
    int k;
    if (b->on == 2) a->hit(SPV_FIZZ, r->p[b->v[0]].note + 2 * (b->ord + 1), 70 - b->ord * 12, 0, r->p[b->v[0]].qual);
    else {
        vel += b->r * 12 + (r->preset == SPW_HONEY ? -30 : r->preset == SPW_MUD ? 20 : 0);
        for (k = 0; k < b->n; k++) a->strike(b->v[k], spg_clamp(vel, 60, 255));
        spg_ring(w, b->x >> 8);
    }
    b->on = 0;
}
static void spg_burst(spg_t *w, mi_ctx_t *c, const mi_api_t *a, int32_t x, int32_t reach, int32_t vel)   /* everything within reach of x pops now */
{
    int i;
    for (i = 0; i < SPG_NB; i++) if (w->b[i].on && spg_abs((w->b[i].x >> 8) - x) <= reach) spg_pop(w, c, a, &w->b[i], vel);
}

static void spg_init(void *wv, mi_ctx_t *c, const mi_api_t *a)
{
    spg_t *w = (spg_t *)wv;
    int s;
    (void)a;
    for (s = 0; s < MI_NPART; s++) {             /* each vent starts part of the way to its next bubble, by where it is */
        uint32_t thr = (uint32_t)SPG_PER[c->r->p[s].size & 7u] * 64u;
        if (!w->inited || w->press[s] >= thr) w->press[s] = (uint16_t)(thr * (uint32_t)((s * 5) % 16) / 16u);
    }
    w->inited = 1;
}
static void spg_step(void *wv, mi_ctx_t *c, const mi_api_t *a)   /* 10 ms */
{
    spg_t *w = (spg_t *)wv;
    const mi_inst_t *r = c->r;
    int32_t ys[SPG_NB], cur = (c->k[2] - 64) * 3;
    uint32_t idx;
    int i, j, s;
    for (s = 0; s < MI_NPART; s++) if (w->puff[s]) w->puff[s] = (uint8_t)(w->puff[s] > 16 ? w->puff[s] - 16 : 0);
    for (i = 0; i < SPG_NR; i++) if (w->ring_t[i]) w->ring_t[i] = (uint16_t)(w->ring_t[i] >= 400 ? 0 : w->ring_t[i] + 10);
    if (w->frog_t) { w->frog_t += 10; if (w->frog_t > 600) w->frog_t = 0; }
    if (c->edit) return;                         /* opened: the water stands still */
    if (r->preset == SPW_MUD) cur /= 3;
    /* the vents: pressure on every sixteenth, a bubble when it is up */
    if (a->cross(MI_TICKS_STEP, 0, &idx) && r->run) {
        uint32_t born = ((idx - 64u) * MI_TICKS_STEP) << 8;
        for (s = 0; s < MI_NPART; s++) {
            uint32_t thr = (uint32_t)SPG_PER[r->p[s].size & 7u] * 64u, p;
            if (!r->p[s].what) continue;
            p = w->press[s] + (uint32_t)c->k[0];
            if (p >= thr) { p -= thr; if (p >= thr) p = 0; spg_emit(w, c, s, born); }
            w->press[s] = (uint16_t)p;
        }
    }
    /* the current: small bubbles are carried faster than big ones */
    for (i = 0; i < SPG_NB; i++) {
        spg_bub_t *b = &w->b[i];
        if (!b->on) continue;
        if (!b->held) b->x += (b->on == 2 ? cur * 3 / 2 : cur * (9 - (b->r > 9 ? 9 : b->r)) / 6) + (a->sin((int32_t)(c->ms * 90u) + i * 9000) >> 9);
        b->x = spg_clamp(b->x, 8 << 8, 232 << 8);
        ys[i] = spg_y(c, b);
    }
    /* two that meet become one: it keeps the earlier way up, and all their vents */
    if (r->preset != SPW_MUD)
        for (i = 0; i < SPG_NB; i++)
            for (j = i + 1; j < SPG_NB; j++) {
                spg_bub_t *p = &w->b[i], *q = &w->b[j], *t;
                int32_t d = p->r + q->r, k;
                if (p->on != 1 || q->on != 1 || p->held || q->held) continue;
                if (spg_abs((p->x >> 8) - (q->x >> 8)) >= d || spg_abs(ys[i] - ys[j]) >= d) continue;
                if ((int32_t)(q->pop - p->pop) < 0) { t = p; p = q; q = t; }
                for (k = 0; k < q->n && p->n < 3; k++) p->v[p->n++] = q->v[k];
                p->r = (uint8_t)(p->r + 2 > 9 ? 9 : p->r + 2);
                q->on = 0;
            }
    /* the surface: a pop, or a wait under a lily pad until the beat */
    j = a->cross(96u, 0, 0);
    for (i = 0; i < SPG_NB; i++) {
        spg_bub_t *b = &w->b[i];
        if (!b->on) continue;
        if (b->held) { if (j) spg_pop(w, c, a, b, 130); continue; }
        if ((int32_t)(c->tq8 - b->pop) < 0) continue;
        if (b->on == 1 && !j && spg_pad_over(c, b->x >> 8)) { b->held = 1; continue; }
        spg_pop(w, c, a, b, 110);
    }
    /* the frog */
    if (r->seq && a->cross(SPG_EVERY[r->sq[1] & 3u], 0, 0)) {
        int at = r->sq[0] & 15;
        w->frog_t = 1;
        spg_burst(w, c, a, SPG_VX(at), 10 + r->sq[2] * 12, 150);
        spg_ring(w, SPG_VX(at));
        a->hit(SPV_SPLASH, 0, 50 + r->sq[2] * 14, r->sq[2], 0);
        if (r->sq[3]) a->hit(SPV_CROAK, r->p[at].note, 70 + r->sq[3] * 22, 4, r->p[at].qual);
    }
}
static void spg_key(void *wv, mi_ctx_t *c, const mi_api_t *a, int i, int vel)   /* a vent squeezed: now, and a bubble for later */
{
    spg_t *w = (spg_t *)wv;
    a->strike(i, vel);
    if (c->r->p[i].what) spg_emit(w, c, i, c->tq8); else w->puff[i] = 255;
}
static void spg_top(void *wv, mi_ctx_t *c, const mi_api_t *a, int i)   /* a pebble thrown in */
{
    spg_t *w = (spg_t *)wv;
    int32_t x = 20 + i * 20;
    spg_burst(w, c, a, x, 26, 140);
    spg_ring(w, x);
    a->hit(SPV_SPLASH, 0, 90, 1, 0);
}
static void spg_play(void *wv, mi_ctx_t *c, const mi_api_t *a, int on) { (void)wv; (void)c; a->ui(on ? 72 : 60); }
static void spg_layout(void *wv, mi_ctx_t *c, const mi_api_t *a, int n)
{
    mi_part_t *p = c->r->p;
    int s;
    (void)wv; (void)a;
    for (s = 0; s < MI_NPART; s++) { p[s].what = SPK_NONE; p[s].size = 3; p[s].note = (uint8_t)(s % 5); p[s].qual = 3; }
#define SPG_P(s_, w_, z_, n_, q_) (p[s_].what = (w_), p[s_].size = (z_), p[s_].note = (n_), p[s_].qual = (q_))
    switch (n) {
    case 1: SPG_P(0, SPK_CLAY, 4, 0, 3); SPG_P(8, SPK_CLAY, 4, 3, 4); SPG_P(4, SPK_REED, 5, 4, 3); SPG_P(11, SPK_REED, 6, 6, 5);
            SPG_P(2, SPK_PEBBLE, 1, 7, 3); SPG_P(6, SPK_PEBBLE, 2, 9, 2); SPG_P(13, SPK_PEBBLE, 1, 8, 4); SPG_P(15, SPK_SHELL, 7, 11, 3); break;
    case 2: for (s = 0; s < MI_NPART; s += 4) SPG_P(s, SPK_CLAY, 4 + s / 4, (s / 4 * 2) % 5, 3 + s / 4);
            SPG_P(2, SPK_PEBBLE, 2, 9, 3); SPG_P(10, SPK_PEBBLE, 2, 7, 3); break;
    case 3: for (s = 0; s < MI_NPART; s += 2) SPG_P(s, SPK_REED, 3 + (s / 2) % 4, (s / 2 * 2) % 9, s % 8); break;
    case 4: for (s = 0; s < MI_NPART; s++) if (s % 4 != 3) SPG_P(s, SPK_PEBBLE, (s * 3) % 4, 5 + (s * 2) % 8, s % 8); break;
    case 5: for (s = 0; s < MI_NPART; s += 3) SPG_P(s, SPK_EMBER, 2 + s % 4, (s * 3) % 7, 2 + s % 5);
            SPG_P(7, SPK_SHELL, 6, 9, 3); SPG_P(14, SPK_SHELL, 7, 11, 5); break;
    default: for (s = 0; s < MI_NPART; s++) SPG_P(s, 1 + s % 5, (s * 3) % 8, (s * 2) % 10, (s * 5) % 8); break;
    }
#undef SPG_P
}
/* its voices. The firmware puts the instrument's ENV and its strip of the mixer on every oscillator */
static void spg_voice(void *wv, mi_ctx_t *c, const mi_api_t *a, int voice, int32_t deg, int32_t vel, int32_t size, int32_t qual)
{
    int32_t n = a->note(deg);
    (void)wv; (void)c;
    switch (voice) {
    case SPV_PEBBLE:                             /* a plip: a high sine that falls into its note, and a tick */
        a->glide(12 + qual, 30); a->vo(n + 36, 0, 50 + size * 10, MM_SINE, 2600, 0, 0, 0);
        a->flt(2, 100 + qual * 2, 80, 0, 30); a->vo(60, 0, 14, MI_HAT, 900, 0, 0, 0);
        break;
    case SPV_REED:                               /* a whistle, with breath on it for the better ones */
        a->vo(n + 24, 0, 220 + size * 60, MM_TRI, 2400, 12, 0, 0);
        if (qual >= 4) a->vo(n + 36, 0, 160 + size * 40, MM_SINE, 500 + qual * 80, 20, 5, 15);
        break;
    case SPV_CLAY:                               /* a bloop: a low sine that rises into its note */
        a->glide(-(5 + qual), 50 + size * 8); a->vo(n - (size >= 5 ? 12 : 0), 0, 180 + size * 50, MM_SINE, 5600, 0, 0, 0);
        a->vo(n + 12, 0, 60, MM_TRI, 900, 0, 0, 0);
        break;
    case SPV_EMBER:                              /* a square through a narrow filter that closes */
        a->flt(2, n + 36 + qual * 2, 140, 20, 120); a->vo(n + 12, 0, 160 + size * 40, MM_SQUARE, 3000, 0, 0, 0);
        a->vo(n + 31, 0, 90, MM_SINE, 700, 0, 0, -20);
        break;
    case SPV_SHELL:                              /* a bell */
        a->vo(n + 24, 0, 700 + size * 200, MM_SINE, 2000, 1, 0, 0);
        a->vo(n + 31, 0, 500 + size * 120, MM_SINE, 900, 1, 6, 25);
        a->vo(n + 43, 0, 200, MM_SINE, 300 + qual * 60, 1, 0, -25);
        break;
    case SPV_FIZZ:
        a->vo(n + 48, 0, 30, MM_SINE, 700, 0, 0, (int32_t)(a->hash((uint32_t)(deg * 7 + vel)) % 100u) - 50);
        break;
    case SPV_CROAK:                              /* two short saws under a low-pass: rib-bit */
        a->flt(1, 50 + qual * 3, 200, 30, 140); a->glide(-7, 60); a->vo(n - 12, 0, 150, MM_SAW, 4200, 2, 0, 0);
        a->flt(1, 54 + qual * 3, 200, 30, 100); a->vo(n - 12, 80, 110, MM_SAW, 3000, 2, 0, 0);
        break;
    default:                                     /* SPV_SPLASH */
        a->flt(2, 96, 40, 0, 120); a->vo(60, 0, 120 + size * 20, MI_HAT, 2400, 0, 0, 0);
        a->glide(-12, 80); a->vo(48, 0, 100, MM_SINE, 2000, 0, 0, 0);
        break;
    }
}

/* ---- its picture */
static void spg_vent(mi_ctx_t *c, const mi_api_t *a, int32_t x, int32_t fy, const mi_part_t *p, int32_t hot)
{
    mm_col_t col = hot > 120 ? c->col[MMC_TEXT] : spg_col(c, p->what);
    int32_t xy[6];
    switch (p->what) {
    case SPK_PEBBLE: a->disc(x, fy - 2, 2 + p->size / 3, col); break;
    case SPK_REED: {
        int32_t h = 10 + p->size * 3, sw = a->sin((int32_t)(c->ms * 30u) + x * 700) >> 13;
        a->line(x, fy, x + sw, fy - h, col); a->disc(x + sw, fy - h, 1 + (p->qual >= 4), col);
        break; }
    case SPK_CLAY: a->disc(x, fy, 4 + p->size / 2, col); a->rect(x - 9, fy + 1, 19, 9, c->col[MMC_BG]); break;
    case SPK_EMBER:
        xy[0] = x - 3; xy[1] = fy; xy[2] = x; xy[3] = fy - 7 - p->size; xy[4] = x + 3; xy[5] = fy;
        a->alpha(150 + (a->sin((int32_t)(c->ms * 120u) + x * 900) >> 8) + (hot > 105 ? 105 : hot)); a->poly(xy, 3, col); a->alpha(256);
        break;
    case SPK_SHELL: a->arc(x, fy - 4, 4 + p->size / 3, 4 + p->size / 3, 32768, 65536 + 16384, col, 1); a->disc(x, fy - 4, 1, col); break;
    default: a->line(x - 2, fy, x + 2, fy, c->col[MMC_MUTE]); break;
    }
}
static void spg_draw(void *wv, mi_ctx_t *c, const mi_api_t *a)
{
    const spg_t *w = (const spg_t *)wv;
    const mi_inst_t *r = c->r;
    int32_t top = spg_surf(c), x, i, s, n = r->sel;
    mm_col_t wet = c->col[MMC_CAP4], own = c->col[MIC_OWN], leaf = MM_RGB(110, 220, 90);
    a->alpha(34); a->rect(0, top, 240, SPG_FLOOR - top, wet); a->alpha(256);
    a->rect(0, SPG_FLOOR, 240, 1, c->col[MMC_RIM]);
    for (x = 0; x < 240; x += 8)                 /* the surface, never quite still */
        a->line(x, top + (a->sin(x * 900 + (int32_t)(c->ms * 40u)) >> 13), x + 8, top + (a->sin((x + 8) * 900 + (int32_t)(c->ms * 40u)) >> 13), wet);
    for (i = 0; i < SPG_NR; i++)
        if (w->ring_t[i]) { a->alpha(256 - w->ring_t[i] * 256 / 400); a->arc(w->ring_x[i], top, 3 + w->ring_t[i] / 16, 1 + w->ring_t[i] / 90, 0, 65536, c->col[MMC_TEXT], 0); a->alpha(256); }
    for (s = 0; s < MI_NPART; s++) {
        const mi_part_t *p = &r->p[s];
        int32_t vx = SPG_VX(s), hot = r->fl[s];
        if (hot && p->what) { a->alpha(hot * 150 / 256); a->disc(vx, SPG_FLOOR - 6, 9, spg_col(c, p->what)); a->alpha(256); }
        spg_vent(c, a, vx, SPG_FLOOR, p, hot);
        if (w->puff[s]) { a->alpha(w->puff[s]); a->arc(vx, SPG_FLOOR - 6, 3, 3, 0, 65536, c->col[MMC_TEXT], 0); a->alpha(256); }
        if (c->edit) {
            char nb[3];
            int on = r->hand >> s & 1;
            nb[0] = s >= 9 ? '1' : (char)('1' + s); nb[1] = s >= 9 ? (char)('0' + s - 9) : 0; nb[2] = 0;
            a->text(vx - a->textw(nb, 1) / 2, SPG_FLOOR + 5, nb, on ? c->col[MMC_TEXT] : c->col[MMC_MUTE], 1);
            if (on) a->box(vx - 7, top - 4, 15, SPG_FLOOR - top + 4, c->col[MMC_TEXT]);
        }
    }
    for (i = 0; i < SPG_NB; i++) {
        const spg_bub_t *b = &w->b[i];
        int32_t bx = b->x >> 8, by;
        if (!b->on) continue;
        by = spg_y(c, b);
        if (b->on == 2) { a->disc(bx, by, 1, c->col[MMC_TEXT]); continue; }
        a->alpha(60); a->disc(bx, by, b->r, spg_col(c, r->p[b->v[0]].what)); a->alpha(256);
        a->arc(bx, by, b->r, b->r, 0, 65536, b->held ? leaf : c->col[MMC_TEXT], 0);
        if (b->n > 1) a->arc(bx, by, b->r - 2, b->r - 2, 0, 65536, spg_col(c, r->p[b->v[1]].what), 0);
        a->rect(bx - b->r / 2, by - b->r / 2, 1, 1, c->col[MMC_TEXT]);
    }
    for (i = 0; i < n; i++) {                    /* the lily pads */
        int32_t px = 240 * (2 * i + 1) / (2 * n);
        a->rect(px - 20, top - 1, 40, 3, leaf); a->disc(px - 20, top, 1, leaf); a->disc(px + 20, top, 1, leaf);
        a->line(px, top - 1, px + 6, top - 1, c->col[MMC_BG]);
    }
    if (r->seq) {                                /* the frog, on its leaf; just jumped: under, and coming back up */
        int32_t fx = SPG_VX(r->sq[0] & 15), fy = top - 5, t = w->frog_t;
        if (t) fy = top + 8 - (t > 300 ? (t - 300) * 13 / 300 : 0);
        a->rect(fx - 8, top - 1, 16, 2, leaf);
        a->disc(fx, fy, 5, MM_RGB(70, 190, 80));
        a->disc(fx - 3, fy - 4, 2, c->col[MMC_TEXT]); a->disc(fx + 3, fy - 4, 2, c->col[MMC_TEXT]);
        a->rect(fx - 3, fy - 4, 1, 1, c->col[MMC_BG]); a->rect(fx + 3, fy - 4, 1, 1, c->col[MMC_BG]);
    }
    if (c->edit && r->hand && (r->hand & (r->hand - 1)) == 0) {   /* one vent in hand: what it is, and its note */
        char nb[6];
        for (s = 0; s < MI_NPART - 1 && !(r->hand >> s & 1); s++) ;
        a->fit(120, 22, 200, SPG_KINDS[r->p[s].what % SPK_N], spg_col(c, r->p[s].what), 2);
        a->note_name(nb, a->note(r->p[s].note));
        a->fit(120, 42, 200, nb, c->col[MMC_TEXT], 1);
    } else if (!c->edit) {
        if (c->k[0] < 8) a->text(4, 16, "COLD", c->col[MMC_MUTE], 1);
        else if (r->preset) a->text(4, 16, SPG_WATERS[r->preset & 3u], c->col[MMC_MUTE], 1);
    }
    (void)own;
}
/* as it stands in the room: a basin on the floor, its bubbles going up, a glint where one just popped */
static void spg_small(void *wv, mi_ctx_t *c, const mi_api_t *a)
{
    const spg_t *w = (const spg_t *)wv;
    const mi_inst_t *r = c->r;
    int32_t x = c->x, y = c->y, i, s;
    mm_col_t own = c->col[MIC_OWN];
    a->arc(x, y - 3, 28, 6, 0, 65536, c->col[MMC_RIM], 1);
    a->alpha(60); a->rect(x - 24, y - 46, 48, 42, c->col[MMC_CAP4]); a->alpha(256);
    a->line(x - 24, y - 46, x - 28, y - 3, c->col[MMC_RIM]); a->line(x + 24, y - 46, x + 28, y - 3, c->col[MMC_RIM]);
    a->line(x - 24, y - 46, x + 24, y - 46, own);
    for (i = 0; i < SPG_NB; i++) {
        const spg_bub_t *b = &w->b[i];
        int32_t span, go;
        if (!b->on) continue;
        span = (int32_t)(b->pop - b->born) >> 4; go = (int32_t)(c->tq8 - b->born) >> 4;
        if (b->held || go > span) go = span;
        if (go < 0) go = 0;
        a->disc(x + ((b->x >> 8) - 120) * 22 / 112, y - 6 - 38 * go / (span > 0 ? span : 1), b->on == 2 ? 0 : 1 + (b->r > 5), own);
    }
    for (s = 0; s < MI_NPART; s++)
        if (r->fl[s] > 100) a->arc(x + (SPG_VX(s) - 120) * 22 / 112, y - 46, 2 + (255 - r->fl[s]) / 40, 1, 0, 65536, c->col[MMC_TEXT], 0);
}

/* ---- its lesson: the spring itself, played by the lesson. A step's first frame sets the scene (the firmware also
 * asks for that alone, with c->t 0, for the steps before a topic out of the middle) */
#define SPG_AT(ms_) (c->t >= (ms_) && c->t - c->dt < (ms_))
static void spg_lesson(void *wv, mi_ctx_t *c, const mi_api_t *a)
{
    mi_inst_t *r = c->r;
    int first = c->t - c->dt <= 0, i;
    switch (c->step) {
    case 0:
        if (first) {
            a->layout(1);
            r->k[0] = 2; r->k[1] = 3; r->k[2] = 4; r->k[3] = 0; r->seq = 0; r->arp = 0; r->run = 1; r->sel = 0; r->preset = 0; r->oct = 0;
            for (i = 0; i < 4; i++) { r->lfo[i] = 0; r->env[i] = 4; }
            r->sq[0] = 4; r->sq[1] = 2; r->sq[2] = 4; r->sq[3] = 5;
            a->open(0, 0, 0);
        }
        a->icon(MII_K1);
        if (SPG_AT(1500)) r->k[0] = 4;
        if (SPG_AT(3200)) r->k[0] = 6;
        break;
    case 1:
        if (first) r->k[0] = 4;
        a->icon(MII_K2);
        if (SPG_AT(1200)) r->k[1] = 6;
        if (SPG_AT(4000)) r->k[1] = 1;
        break;
    case 2:
        if (first) r->k[1] = 3;
        a->icon(c->t < 3800 ? MII_K3 : MII_K4);
        if (SPG_AT(800)) r->k[2] = 8;
        if (SPG_AT(3800)) { r->k[2] = 4; r->k[3] = 8; }
        break;
    case 3:
        if (first) { r->k[2] = 4; r->k[3] = 0; }
        a->icon(MII_SELECT);
        if (SPG_AT(800)) r->sel = 1;
        if (SPG_AT(2600)) r->sel = 3;
        break;
    case 4:
        if (first) { r->sel = 0; r->seq = 1; r->sq[1] = 1; }
        a->icon(c->t < 3000 ? MII_BTN + MMB_SEQ : c->t < 5000 ? MII_K1 : MII_K3);
        if (SPG_AT(3400)) r->sq[0] = 11;
        if (SPG_AT(5200)) r->sq[2] = 8;
        break;
    default:
        if (first) { r->seq = 0; r->sq[0] = 4; r->sq[2] = 4; }
        a->icon(c->t < 2800 ? MII_LOW : c->t < 4600 ? MII_TOP : MII_BTN + MMB_EDIT);
        if (SPG_AT(700)) spg_key(wv, c, a, 0, 200);
        if (SPG_AT(1300)) spg_key(wv, c, a, 4, 200);
        if (SPG_AT(1900)) spg_key(wv, c, a, 15, 200);
        if (SPG_AT(3300)) spg_top(wv, c, a, 5);
        if (SPG_AT(4800)) { a->open(1, 0, 0); r->hand = 1u << 9; }
        if (SPG_AT(5600)) { r->p[9].what = SPK_SHELL; r->p[9].size = 5; r->p[9].note = 6; a->changed(); a->strike(9, 150); a->label(SPG_KINDS[SPK_SHELL]); }
        break;
    }
}
#undef SPG_AT
static const mi_step_t SPG_STEPS[6] = {
    {6500, {"WARM WATER OVER SIXTEEN VENTS. KNOB 1 IS THE HEAT: EACH VENT LETS A BUBBLE GO WHEN ITS PRESSURE IS UP. SMALL VENTS ARE QUICK, BIG ONES SLOW.",
     "WARMES WASSER ÜBER SECHZEHN QUELLEN. KNOPF 1 IST DIE HITZE: JEDE QUELLE LÄSST EINE BLASE LOS, WENN IHR DRUCK REICHT. KLEINE SIND SCHNELL, GROSSE LANGSAM.",
     "VARMT VAND OVER SEKSTEN KILDER. KNAP 1 ER VARMEN: HVER KILDE SLIPPER EN BOBLE, NÅR TRYKKET ER OPPE. SMÅ KILDER ER HURTIGE, STORE ER LANGSOMME.",
     "DE L'EAU CHAUDE SUR SEIZE SOURCES. LE BOUTON 1 EST LA CHALEUR : CHAQUE SOURCE LÂCHE UNE BULLE QUAND SA PRESSION SUFFIT. LES PETITES SONT VIVES, LES GRANDES LENTES.",
     "AGUA TIBIA SOBRE DIECISÉIS FUENTES. EL MANDO 1 ES EL CALOR: CADA FUENTE SUELTA UNA BURBUJA CUANDO TIENE PRESIÓN. LAS PEQUEÑAS SON RÁPIDAS, LAS GRANDES LENTAS.",
     "ON ALTI KAYNAĞIN ÜSTÜNDE ILIK SU. DÜĞME 1 ISIDIR: BASINCI YETİNCE HER KAYNAK BİR BALONCUK BIRAKIR. KÜÇÜKLER HIZLI, BÜYÜKLER YAVAŞTIR.",
     "WARM WATER BOVEN ZESTIEN BRONNEN. KNOP 1 IS DE WARMTE: ELKE BRON LAAT EEN BEL LOS ALS DE DRUK HOOG GENOEG IS. KLEINE ZIJN SNEL, GROTE TRAAG."}, 0},
    {6500, {"A BUBBLE IS SILENT UNTIL IT MEETS THE SURFACE: THERE IT POPS, AND YOU HEAR ITS VENT. KNOB 2 IS THE DEPTH: DEEPER WATER, A LONGER WAY UP.",
     "EINE BLASE IST STUMM, BIS SIE DIE OBERFLÄCHE TRIFFT: DORT PLATZT SIE, UND DU HÖRST IHRE QUELLE. KNOPF 2 IST DIE TIEFE: TIEFERES WASSER, EIN LÄNGERER WEG.",
     "EN BOBLE ER TAVS, INDTIL DEN MØDER OVERFLADEN: DER BRISTER DEN, OG DU HØRER DENS KILDE. KNAP 2 ER DYBDEN: DYBERE VAND, EN LÆNGERE VEJ OP.",
     "UNE BULLE EST MUETTE JUSQU'À LA SURFACE : ELLE Y ÉCLATE, ET TU ENTENDS SA SOURCE. LE BOUTON 2 EST LA PROFONDEUR : PLUS D'EAU, PLUS DE CHEMIN.",
     "UNA BURBUJA NO SUENA HASTA LLEGAR A LA SUPERFICIE: AHÍ REVIENTA Y OYES SU FUENTE. EL MANDO 2 ES LA PROFUNDIDAD: MÁS AGUA, MÁS CAMINO.",
     "BALONCUK YÜZEYE VARANA DEK SESSİZDİR: ORADA PATLAR VE KAYNAĞINI DUYARSIN. DÜĞME 2 DERİNLİKTİR: SU DERİNSE YOL UZUNDUR.",
     "EEN BEL IS STIL TOT ZE HET OPPERVLAK RAAKT: DAAR KNAPT ZE, EN JE HOORT HAAR BRON. KNOP 2 IS DE DIEPTE: DIEPER WATER, EEN LANGERE WEG OMHOOG."}, 0},
    {7000, {"KNOB 3 IS THE CURRENT. BUBBLES THAT MEET BECOME ONE AND POP AS ONE: TWO NOTES AT ONCE. KNOB 4 IS THE FIZZ: LITTLE BUBBLES THAT GET THERE FIRST.",
     "KNOPF 3 IST DIE STRÖMUNG. BLASEN, DIE SICH TREFFEN, WERDEN EINE UND PLATZEN ALS EINE: ZWEI TÖNE ZUGLEICH. KNOPF 4 IST DAS PERLEN: KLEINE BLASEN, DIE ZUERST OBEN SIND.",
     "KNAP 3 ER STRØMMEN. BOBLER, DER MØDES, BLIVER TIL ÉN OG BRISTER SOM ÉN: TO TONER PÅ ÉN GANG. KNAP 4 ER BRUSET: SMÅ BOBLER, DER NÅR OP FØRST.",
     "LE BOUTON 3 EST LE COURANT. LES BULLES QUI SE RENCONTRENT N'EN FONT QU'UNE ET ÉCLATENT ENSEMBLE : DEUX NOTES À LA FOIS. LE BOUTON 4 EST LE PÉTILLEMENT : DE PETITES BULLES QUI ARRIVENT D'ABORD.",
     "EL MANDO 3 ES LA CORRIENTE. LAS BURBUJAS QUE SE ENCUENTRAN SE HACEN UNA Y REVIENTAN COMO UNA: DOS NOTAS A LA VEZ. EL MANDO 4 ES EL BURBUJEO: BURBUJITAS QUE LLEGAN ANTES.",
     "DÜĞME 3 AKINTIDIR. KARŞILAŞAN BALONCUKLAR BİR OLUR VE BİRLİKTE PATLAR: AYNI ANDA İKİ NOTA. DÜĞME 4 KÖPÜKTÜR: ÖNCE VARAN KÜÇÜK BALONCUKLAR.",
     "KNOP 3 IS DE STROMING. BELLEN DIE ELKAAR RAKEN WORDEN ÉÉN EN KNAPPEN ALS ÉÉN: TWEE NOTEN TEGELIJK. KNOP 4 IS HET BRUISEN: KLEINE BELLETJES DIE ER EERDER ZIJN."}, 0},
    {6000, {"SELECT FLOATS LILY PADS. A BUBBLE UNDER A PAD WAITS, AND ON THE BEAT THE PAD LETS THEM ALL GO TOGETHER.",
     "SELECT LEGT SEEROSENBLÄTTER AUFS WASSER. EINE BLASE UNTER EINEM BLATT WARTET, UND AUF DEN SCHLAG LÄSST DAS BLATT ALLE ZUSAMMEN LOS.",
     "SELECT LÆGGER ÅKANDEBLADE PÅ VANDET. EN BOBLE UNDER ET BLAD VENTER, OG PÅ SLAGET SLIPPER BLADET DEM ALLE PÅ ÉN GANG.",
     "SELECT POSE DES NÉNUPHARS. UNE BULLE SOUS UNE FEUILLE ATTEND, ET SUR LE TEMPS LA FEUILLE LES LÂCHE TOUTES ENSEMBLE.",
     "SELECT PONE NENÚFARES. UNA BURBUJA BAJO UNA HOJA ESPERA, Y AL COMPÁS LA HOJA LAS SUELTA TODAS JUNTAS.",
     "SELECT NİLÜFER YAPRAKLARI KOYAR. YAPRAĞIN ALTINDAKİ BALONCUK BEKLER, VURUŞTA YAPRAK HEPSİNİ BİRDEN BIRAKIR.",
     "SELECT LEGT LELIEBLADEN OP HET WATER. EEN BEL ONDER EEN BLAD WACHT, EN OP DE TEL LAAT HET BLAD ZE ALLEMAAL TEGELIJK LOS."}, 0},
    {7500, {"SEQ WAKES THE FROG. IT JUMPS IN ON THE BEAT AND EVERYTHING NEAR IT POPS. HOLD SEQ AND TURN KNOB 1 TO 4: ITS PLACE, HOW OFTEN, ITS SPLASH, ITS CROAK.",
     "SEQ WECKT DEN FROSCH. ER SPRINGT AUF DEN SCHLAG HINEIN, UND ALLES IN SEINER NÄHE PLATZT. HALTE SEQ UND DREH KNOPF 1 BIS 4: SEIN PLATZ, WIE OFT, SEIN PLATSCHER, SEIN QUAKEN.",
     "SEQ VÆKKER FRØEN. DEN HOPPER I PÅ SLAGET, OG ALT I NÆRHEDEN BRISTER. HOLD SEQ OG DREJ KNAP 1 TIL 4: DENS PLADS, HVOR TIT, DENS PLASK, DENS KVÆK.",
     "SEQ RÉVEILLE LA GRENOUILLE. ELLE PLONGE SUR LE TEMPS ET TOUT CE QUI EST AUTOUR ÉCLATE. TIENS SEQ ET TOURNE LES BOUTONS 1 À 4 : SA PLACE, SA CADENCE, SON PLOUF, SON COASSEMENT.",
     "SEQ DESPIERTA A LA RANA. SALTA AL AGUA AL COMPÁS Y TODO LO QUE TIENE CERCA REVIENTA. MANTÉN SEQ Y GIRA LOS MANDOS 1 A 4: SU SITIO, CADA CUÁNTO, SU CHAPUZÓN, SU CROAR.",
     "SEQ KURBAĞAYI UYANDIRIR. VURUŞTA SUYA ATLAR VE YAKININDAKİ HER ŞEY PATLAR. SEQ'İ BASILI TUT VE DÜĞME 1-4'Ü ÇEVİR: YERİ, SIKLIĞI, SIÇRATMASI, VIRAKLAMASI.",
     "SEQ MAAKT DE KIKKER WAKKER. HIJ SPRINGT OP DE TEL IN HET WATER EN ALLES IN DE BUURT KNAPT. HOUD SEQ VAST EN DRAAI KNOP 1 TOT 4: ZIJN PLEK, HOE VAAK, ZIJN PLONS, ZIJN GEKWAAK."}, 0},
    {7200, {"THE BOTTOM ROW SQUEEZES A VENT: YOU HEAR IT NOW, AND AGAIN WHEN ITS BUBBLE COMES UP. THE TOP ROW THROWS A PEBBLE IN. EDIT PLANTS THE VENTS. NOW YOU!",
     "DIE UNTERE REIHE DRÜCKT EINE QUELLE: DU HÖRST SIE JETZT, UND NOCH EINMAL, WENN IHRE BLASE OBEN IST. DIE OBERE REIHE WIRFT EINEN KIESEL HINEIN. EDIT PFLANZT DIE QUELLEN. JETZT DU!",
     "NEDERSTE RÆKKE KLEMMER EN KILDE: DU HØRER DEN NU, OG IGEN NÅR DENS BOBLE KOMMER OP. ØVERSTE RÆKKE KASTER EN STEN I. EDIT PLANTER KILDERNE. NU ER DET DIG!",
     "LA RANGÉE DU BAS PRESSE UNE SOURCE : TU L'ENTENDS TOUT DE SUITE, PUIS ENCORE QUAND SA BULLE ARRIVE. LA RANGÉE DU HAUT JETTE UN CAILLOU. EDIT PLANTE LES SOURCES. À TOI !",
     "LA FILA DE ABAJO APRIETA UNA FUENTE: LA OYES AHORA, Y OTRA VEZ CUANDO SUBE SU BURBUJA. LA FILA DE ARRIBA TIRA UNA PIEDRECITA. EDIT PLANTA LAS FUENTES. ¡TE TOCA!",
     "ALT SIRA BİR KAYNAĞI SIKAR: ŞİMDİ DUYARSIN, BALONCUĞU YUKARI VARINCA BİR DAHA. ÜST SIRA SUYA ÇAKIL ATAR. EDIT KAYNAKLARI DİKER. ŞİMDİ SEN!",
     "DE ONDERSTE RIJ KNIJPT IN EEN BRON: JE HOORT HAAR NU, EN NOG EENS ALS HAAR BEL BOVENKOMT. DE BOVENSTE RIJ GOOIT EEN STEENTJE. EDIT PLANT DE BRONNEN. NU JIJ!"}, 0},
};
static const mi_topic_t SPG_TOPICS[6] = {{"VENTS AND HEAT", 0, 1}, {"DEPTH", 1, 1}, {"CURRENT AND FIZZ", 2, 1}, {"LILY PADS", 3, 1}, {"SEQ: THE FROG", 4, 1}, {"THE KEYS", 5, 1}};

MI_INSTRUMENT(spring) = {
    MI_MAGIC, MI_API, sizeof(spg_t), 16, 1, 0, 0,   /* its number (16 is THE SPRING), its version */
    "THE SPRING", "SPRING", "SP",
    {"HEAT", "DEPTH", "CURRENT", "FIZZ"}, {8, 8, 8, 8}, {4, 3, 4, 0}, {0, 0, 1, 0},
    "LILY PADS", 4, 0, SPK_N, 4,
    "SEQ: THE FROG", {"PLACE", "EVERY", "SPLASH", "CROAK"}, {15, 3, 8, 8}, {4, 2, 4, 5},
    SPG_KINDS, SPG_SND, SPG_WATERS, SPG_ALGOS, 7, 6, 6, 0, SPG_STEPS, SPG_TOPICS,
    spg_init, spg_step, spg_draw, spg_small, spg_key, spg_top, spg_layout, spg_play, spg_voice, spg_lesson,
};
