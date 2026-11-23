/* SPDX-License-Identifier: GPL-3.0-only */
/* HOME on the course: the controls, shown and then done. A lesson of five steps, each a little scene with Rock above a
 * line drawing of the FM-1 and the two hands that hold it; each is shown once (WATCH), then waits for the player to do
 * it on the real FM-1 (YOUR TURN: the scene answers the real knobs and keys) and is ticked off:
 *   roll        the left hand turns PRESETS: Rock rolls                         yours: any knob but ALGORITHM, 12 clicks
 *   hop         the right thumb, holding the bottom right corner, on a bottom key: a hop     yours: two bottom keys
 *   big hop     the same thumb on a top key: a big hop                                       yours: two top keys
 *   the jump    rolling over the white keys in a hill, a top key: Rock is thrown from the crest, lands on a glowing
 *               bounce mushroom and is thrown again: BONUS        yours: roll up onto the white keys, then a top key
 *   or          the other grip, last: the right hand on KNOB 4, the left thumb on the keys   (only shown: on at its end,
 *               or at a key, knob or button)
 * The device and the hands are lit lines on the dark, as the Hat and Rock are. The knob hand pinches its knob and
 * twists it, lets go and twists again; the thumb waits just off its key, comes down on it (a ring goes out from under
 * it, three small strokes stand over the key) and lifts. At your turn the hands go out to the sides: the knob to use
 * glows, the keys to use are lit (on the drawing and on the FM-1), and the key pressed goes down on the drawing.
 * The words are the controls sheet's own ("BIG HOP = TOP KEYS": the word large over the scene, the rest
 * under the scene, over the device), so every language has them. The first step has a line of its own under its word:
 * TURN KNOB TO ROLL. The lesson's own engine (WATCH, YOUR TURN, DONE, the marks, ALL DONE) is below, shared with the
 * machines' (ac_mtutor.h). */

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
/* Every tutorial (this one and each machine's, ac_mtutor.h) is a lesson of steps, and each step comes in three parts:
 *   WATCH      the step's scene plays once, the hands on the drawn FM-1 doing what it asks
 *   YOUR TURN  the hands go; the knob or the keys to use glow, on the drawing and on the FM-1 itself, and the scene
 *              answers the real knobs and keys. It waits as long as it takes; left alone a while, it shows the step again
 *   DONE       a tick over the scene and on the step's mark, a chime; then the next step
 * A step that only tells (info) is its WATCH alone: it ticks itself off as its demo ends, and a key, a knob or a button
 * goes on at once. After the last, a card: all done. PLAY skips a step, HOME returns. A step's mark fills as its demo plays, then as it
 * is done (some ask for a few turns or presses). Once a demo has played out, pressing or turning cuts its replays short. */
enum { TP_WATCH, TP_TRY, TP_DONE, TP_END };
#define TP_DONE_MS 1200
#define TP_IDLE_MS 9000                          /* left alone this long at your turn: the step is shown again */
#define TP_SWAP_MS 800                           /* "YOUR TURN" over the scene as the hands go */
#define TP_LONG 100000
static struct {
    int32_t step, phase, pt, prev, idle, idle_max, seen, started;   /* idle_max: how long left alone before the demo again */
    int32_t prog, goal, meter;                   /* how much of the step is done, of how much; meter: the same, 0..256 */
    int32_t ev, evk, kev;                        /* ms since a press that the step answered, its key; ms since a knob turned */
    int32_t v[8];                                /* the step's own state */
    uint32_t done;                               /* the steps ticked off, a bit each */
} tp;
static void tp_go(int32_t step)
{
    tp.step = step; tp.phase = TP_WATCH; tp.pt = 0; tp.prev = -1; tp.idle = 0; tp.idle_max = TP_IDLE_MS; tp.seen = 0; tp.started = 0;
    tp.prog = 0; tp.goal = 1; tp.meter = 0; tp.ev = tp.kev = TP_LONG; tp.evk = -1;
}
static void tp_begin(void) { int k; tp.done = 0; for (k = 0; k < 8; k++) tp.v[k] = 0; tp_go(0); }
static void tp_after(int32_t n) { if (tp.step + 1 >= n) { tp.phase = TP_END; tp.pt = 0; } else tp_go(tp.step + 1); }
/* a frame of a lesson of n steps, whose step's demo lasts watch_ms: start() sets a step's own state the first time its
 * turn comes, live() answers the knobs and keys (in.* only at YOUR TURN; it goes on animating while DONE shows) */
static void tp_run(int32_t dt, int32_t n, int32_t watch_ms, int info, void (*start)(void), void (*live)(int32_t))
{
    int act = in.kdown || in.any_knob;
    tp.prev = tp.pt; tp.pt += dt;
    tp.ev = imin(TP_LONG, tp.ev + dt); tp.kev = imin(TP_LONG, tp.kev + dt);
    if (tp.phase == TP_END) return;
    if (BDOWN(BTN_PLAY)) { if (info) tp.done |= 1u << tp.step; snd_ui(79); tp_after(n); return; }   /* (OCT is the language, everywhere) */
    if (tp.phase == TP_WATCH && info) {          /* only to watch: on at its end, or at once on a key, knob or button */
        uint32_t b = in.bdown & ~(1u << BTN_HOME | 1u << BTN_REC | 1u << BTN_OCT_UP | 1u << BTN_OCT_DOWN);
        if (tp.pt >= watch_ms || act || b) { tp.done |= 1u << tp.step; snd_ui(79); tp_after(n); }
        return;
    }
    if (tp.phase == TP_WATCH) {
        if (tp.pt < watch_ms && !(act && tp.seen)) return;
        tp.seen = 1; tp.phase = TP_TRY; tp.pt = 0; tp.prev = -1; tp.idle = 0;
        if (!tp.started) { tp.started = 1; tp.prog = 0; tp.meter = 0; tp.ev = tp.kev = TP_LONG; tp.evk = -1; start(); }
        snd_ui(84);
    }
    if (tp.phase == TP_TRY) {
        tp.idle = act ? 0 : tp.idle + dt;
        live(dt);
        tp.meter = iclamp(tp.prog * 256 / imax(1, tp.goal), 0, 256);
        if (tp.prog >= tp.goal) { tp.phase = TP_DONE; tp.pt = 0; tp.done |= 1u << tp.step; snd_chime(1); }
        else if (tp.idle >= tp.idle_max) { tp.phase = TP_WATCH; tp.pt = 0; tp.prev = -1; }
        return;
    }
    live(dt);
    if (tp.pt >= TP_DONE_MS) { tp_after(n); if (tp.phase == TP_END) snd_chime(1); }
}
#define TP_AT(ms) (tp.prev < (ms) && tp.pt >= (ms))   /* the demo has just passed ms */
static int tp_live(void) { return tp.phase == TP_TRY || tp.phase == TP_DONE; }
/* a tick, s across, a little bold */
static void tp_check(int32_t cx, int32_t cy, int32_t s, col_t c)
{
    gline2(cx - s / 2, cy, cx - s / 6, cy + s / 3, c); gline2(cx - s / 6, cy + s / 3, cx + s / 2, cy - s / 3, c);
    gline(cx - s / 2, cy + 1, cx - s / 6, cy + s / 3 + 1, c); gline(cx - s / 6, cy + s / 3 + 1, cx + s / 2, cy - s / 3 + 1, c);
}
/* the marks under a lesson's scene, one a step: ticked when done; the step on fills (white as its demo plays, in the
 * step's colour as it is done); to their left what the step is at: WATCH, or YOUR TURN, breathing */
static void tp_pips(int32_t n, int32_t y, int32_t watch_ms, col_t c, uint32_t tm)
{
    int32_t k, x0 = 120 - n * 8;
    for (k = 0; k < n; k++) {
        int32_t x = x0 + k * 16;
        if (tp.done >> k & 1u) { tp_check(x + 7, y, 9, th->pad); continue; }
        grect(x, y - 1, 14, 3, th->mute);
        if (k == tp.step && tp.phase != TP_END) {
            int32_t f = tp.phase == TP_WATCH ? tp.pt * 256 / imax(1, watch_ms) : tp.meter;
            grect(x, y - 1, 14 * iclamp(f, 0, 256) / 256, 3, tp.phase == TP_WATCH ? th->text : c);
        }
    }
    if (tp.phase == TP_WATCH || tp.phase == TP_TRY) {
        galpha(tp.phase == TP_TRY ? 150 + ipulse(tm, 1000) * 106 / 256 : 190);
        gtextr(x0 - 6, y - 2, TR(tp.phase == TP_WATCH ? T_TU_WATCH : T_TU_TURN), tp.phase == TP_WATCH ? th->text : c, 1);
        galpha(256);
    }
}
/* over the scene (its middle at cy): YOUR TURN as the hands go; the tick when a step is done */
static void tp_overlay(int32_t cy, col_t c)
{
    if (tp.phase == TP_TRY && tp.pt < TP_SWAP_MS && tp.seen == 1) {
        int32_t a = tp.pt < TP_SWAP_MS - 250 ? imin(256, tp.pt * 256 / 120) : (TP_SWAP_MS - tp.pt) * 256 / 250, sc = ts(2, 3);
        galpha(a * 3 / 4); grect(0, cy - 3 * sc - 3, AC_W, 5 * sc + 6, th->bg);
        galpha(a); gfit(120, cy - 3 * sc + 1, 228, TR(T_TU_TURN), c, sc); galpha(256);
    }
    if (tp.phase == TP_DONE) {
        int32_t a = imin(256, tp.pt * 256 / 150), r = 8 + 6 * ease8(imin(256, tp.pt * 256 / 260)) / 256;
        if (tp.pt > TP_DONE_MS - 250) a = imax(0, (TP_DONE_MS - tp.pt) * 256 / 250);
        galpha(a * 3 / 4); gdisc(120, cy, r + 5, th->bg);
        galpha(a); gcirc(120, cy, r + 5, th->pad); gcirc(120, cy, r + 4, th->pad);
        tp_check(120, cy, r * 3 / 2, th->pad);
        galpha(256);
    }
}
/* the end of a lesson: every step ticked, Rock bouncing for joy, ALL DONE */
static void tp_end_card(int32_t n, col_t c, uint32_t tm)
{
    int32_t k, sc = ts(2, 4), b = (int32_t)(tm % 700u), hop = 4 * 18 * b / 700 * (700 - b) / 700;
    grect(0, 0, AC_W, AC_H, th->bg);
    for (k = 0; k < 22; k++) gp((int32_t)(ihash((uint32_t)k * 7u + 3u) % 240u), (int32_t)(ihash((uint32_t)k * 13u + 5u) % 200u), th->mute);
    for (k = 0; k < 6; k++) {                    /* rings going out, in the caps' colours */
        int32_t q = (int32_t)((tm + (uint32_t)k * 400u) % 2400u) * 256 / 2400;
        galpha(256 - q); gcirc(120, 70, 20 + q * 80 / 256, th->caps[k % 5]);
    }
    galpha(256);
    ghl(70, 170, 92, th->rim);
    prock_draw(120, 80 - hop, 300, (int32_t)tm * 30, (tm % 3100u) < 150u ? 30 : 256);
    gfit(120, 112, 228, TR(T_TU_ALL), c, sc);
    for (k = 0; k < n; k++) tp_check(120 - n * 8 + k * 16 + 7, 120 + 5 * sc + 10, 9, th->pad);
}
static int32_t tu_tap(int32_t step, int32_t u, int32_t *row)
{
    const tu_step_t *s = &TU_STEP[step];
    int k = s->tap[1] && u >= (int32_t)s->tap[1] - TU_REACH ? 1 : 0;
    *row = s->row[k];
    return s->tap[k] ? u - (int32_t)s->tap[k] : -1000;
}
/* the course's lesson at your turn: what the scene shows is the player's own doing */
#define TV_ROLL tp.v[0]                          /* how far Rock has rolled, in the demo's ms of rolling (46 px a second) */
#define TV_SHOWN tp.v[1]                         /* the same, eased toward it */
#define TV_JX tp.v[2]                            /* the jump: Rock along the hill (px * 16), and where it was thrown from */
#define TV_JX0 tp.v[3]
#define TV_FLY tp.v[4]                           /* ms since the key that threw it (-1: not yet) */
#define TU_FLY_UP 300                            /* the jump: thrown, Rock goes on up to the crest, then flies as in the demo */
static int tu_live;                              /* the scene being drawn is the player's: Rock where they put him */
static int32_t tu_live_s, tu_live_hop, tu_jrx = -1, tu_want_knob;
static void tu_start(void)
{
    const tu_step_t *s = &TU_STEP[tp.step];
    TV_SHOWN = TV_ROLL; TV_JX = 10 * 16; TV_FLY = -1;
    tp.goal = s->act == TA_ROLL ? 12 : s->act == TA_HOP ? 2 : 1;
}
static void tu_live_tick(int32_t dt)
{
    const tu_step_t *s = &TU_STEP[tp.step];
    int32_t d = in.any_knob - in.enc[ENC_ALGORITHM], k, was;   /* every knob but ALGORITHM rolls, as on the course */
    if (tp.phase == TP_TRY) {
        if (d) { tp.kev = 0; TV_ROLL += d * 160; }
        if (in.kdown) {
            for (k = AC_NKEY - 1; k > 0 && !(in.kdown >> k & 1u); k--) ;
            tp.ev = 0; tp.evk = k;
            sfx(key_note(k), 0, 600, W_TRI, 2800, 6, 200);
        }
        switch (s->act) {
        case TA_ROLL: tp.prog = imin(tp.goal, tp.prog + iabs(d)); break;
        case TA_HOP: if (in.kdown && KEY_IS_SHARP[tp.evk] == (s->row[0] == 2)) tp.prog++; break;   /* the row it teaches */
        default:                                 /* the jump: roll up the hill; a top key on the white keys throws */
            if (TV_FLY < 0) {
                TV_JX = iclamp(TV_JX + d * 80, 10 * 16, TU_CREST * 16);
                if (in.kdown && KEY_IS_SHARP[tp.evk] && TV_JX >= 80 * 16) { TV_FLY = 0; TV_JX0 = TV_JX; }
            }
        }
    }
    TV_SHOWN += (TV_ROLL - TV_SHOWN) * imin(dt, 100) / 150;
    if (s->act == TA_JUMP && TV_FLY >= 0) {
        was = TV_FLY; TV_FLY += dt;
#define TU_PAST(ms) (was < (ms) && TV_FLY >= (ms))
        if (TU_PAST(TU_FLY_UP)) snd_whoosh();
        if (TU_PAST(TU_FLY_UP + TU_T_LAND - TU_T_OFF)) { sfx(67, 0, 800, W_SINE, 3400, 4, 220); sfx(79, 60, 600, W_SINE, 1800, 4, 220); snd_chime(1); }
#undef TU_PAST
        if (TV_FLY >= TU_FLY_UP + TU_T_OUT - TU_T_OFF + 500) tp.prog = 1;   /* (once BONUS is up) */
    }
}
/* the row a step asks for: 1 the bottom keys, 2 the top, 3 both, 0 none */
static int32_t tu_want_row(const tu_step_t *s) { return s->act == TA_HOP ? s->row[0] : s->act == TA_JUMP ? 2 : s->act == TA_SWAP ? 3 : 0; }
static void tutor_tick(int32_t dt)
{
    const tu_step_t *s;
    int32_t j, row, age, k;
    uint32_t rowmask = 0;
    tp_run(dt, TU_N, TU_STEP[tp.step].ms, TU_STEP[tp.step].act == TA_SWAP, tu_start, tu_live_tick);   /* (the other grip: only shown) */
    if (tp.phase == TP_END) return;
    s = &TU_STEP[tp.step];
    if (tp.phase == TP_WATCH) {                  /* the demo's sounds: a press is the key's own note, as on the course */
        for (j = 0; j < 2; j++)
            if (s->tap[j] && TP_AT((int32_t)s->tap[j])) sfx(key_note(s->row[j] == 2 ? 22 : 23), 0, 600, W_TRI, 2800, 6, 200);
        if (s->act == TA_JUMP) {
            if (TP_AT(TU_T_OFF)) snd_whoosh();
            if (TP_AT(TU_T_LAND)) { sfx(67, 0, 800, W_SINE, 3400, 4, 220); sfx(79, 60, 600, W_SINE, 1800, 4, 220); snd_chime(1); }
        }
        /* on the FM-1 itself: the row being shown glows, and lights as the key goes down */
        age = tu_tap(tp.step, tp.pt % s->ms, &row);
        if (row) for (k = 0; k < AC_NKEY; k++) if (KEY_IS_SHARP[k] == (row == 2)) rowmask |= 1u << k;
        led_key_dim_want |= rowmask;
        if (age >= 0 && age < 220) led_key_want |= rowmask;
        return;
    }
    /* your turn: the keys to use glow on the FM-1, and light as they are pressed */
    row = tu_want_row(s);
    for (k = 0; k < AC_NKEY; k++) if (row >> KEY_IS_SHARP[k] & 1) rowmask |= 1u << k;
    led_key_dim_want |= rowmask;
    led_key_want |= in.keys;
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
    /* the keys: sixteen below, eleven above in threes and twos; the row the thumb is over is lit (at your turn: the row
     * or rows to use, and the key the player pressed goes down, whichever it is) */
    {
        int32_t pw = hw, ps = hs, aw = row == 1 ? age : -1, as = row == 2 ? age : -1;
        if (tu_live && tp.evk >= 0) {
            int32_t j, nw = 0, ns = 0;
            for (j = 0; j < tp.evk; j++) { if (KEY_IS_SHARP[j]) ns++; else nw++; }
            if (KEY_IS_SHARP[tp.evk]) { ps = TU_SHARP_AFTER[imin(ns, 10)]; as = tp.ev; aw = -1; } else { pw = nw; aw = tp.ev; as = -1; }
        }
        for (k = 0; k < 16; k++) tu_key(x0 + 11 + k * 8, y0 + 69, row & 1, k == pw ? aw : -1, c);
        for (k = 0; k < 11; k++) tu_key(x0 + 15 + TU_SHARP_AFTER[k] * 8, y0 + 54, (row & 2) != 0, TU_SHARP_AFTER[k] == ps ? as : -1, c);
    }
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
    if (tu_live && tu_want_knob) {               /* your turn: the knob to use, glowing */
        galpha(110 + ipulse(tm, 900) * 146 / 256); gcirc(kx, ky, 7, c); gcirc(kx, ky, 8, c); galpha(256);
    }
    /* the hand at the corner: its thumb waits just off the key, comes down on it and lifts */
    kp[0][0] = x0 + 11 + hw * 8 + 3; kp[0][1] = y0 + 69 + 6;
    kp[1][0] = x0 + 15 + hs * 8 + 3; kp[1][1] = y0 + 54 + 6;
    r = row == 2 ? 1 : 0;
    px = kp[1 - r][0] + (kp[r][0] - kp[1 - r][0]) * rf / 256; py = kp[1 - r][1] + (kp[r][1] - kp[1 - r][1]) * rf / 256;
    f = tu_press(age);
    tx = px + s * (1 + 7 * (256 - f) / 256); ty = py + 4 * (256 - f) / 256;
    tu_icon(TU_TAP, 2, TU_TAP_L, 3, tx + s * tslide, ty, s, 0, 256);
    if (!tu_live && age >= 0 && age < 420) {     /* the press: a ring out from under the thumb, three strokes over the key */
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
    prock_draw(x, y, 256, ang, (tm % 3100u) < 150u ? 30 : 256);
}
/* a roll, hop or other-grip step: how far Rock has rolled (s, in the scene's pixels: only the steps with a knob turning
 * move it) and how high it is hopping (a hop with each press: small from the bottom row, big from the top) */
static void tu_roll_state(int32_t step, int32_t u, int32_t loops, int32_t *s, int32_t *hop)
{
    const tu_step_t *st = &TU_STEP[step];
    int32_t k, roll = loops * 6200 + (step > 0 ? 2400 : 0) + (st->act == TA_ROLL || st->act == TA_SWAP ? u : 0);
    if (tu_live) { *s = tu_live_s; *hop = tu_live_hop; return; }
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
    if (tu_jrx >= 0 && u < TU_T_OFF) { *rx = tu_jrx; *ry = tu_hill(*rx) - 9 - tu_live_hop; return; }   /* (the player's) */
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
        if (u < TU_T_OFF) { int32_t rxn, ryn; tu_jump_pos(u, &rxn, &ryn); if (iabs(rxn - (86 + key * 5)) < 5 && ryn >= tu_hill(rxn) - 10) sink = 2; }
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

/* the foot, taking turns: HOME back, PLAY skips a step (and in a machine, REC back to the Arcave) */
static void tp_foot(int n, uint32_t tm)
{
    static const int ID[3] = {T_HELP_RETURN, T_TU_SKIP, T_REC_BACK};
    uint32_t f = (tm / 3000u) % (uint32_t)n;
    if (tp.phase == TP_END) {                    /* all done: only the way back, large, under the ticks */
        int32_t a = ts(2, 3);
        galpha(170 + ipulse(tm, 1400) * 86 / 256);
        gfit(120, 232 - 5 * a, 232, TR(T_HELP_RETURN), th->pad, a);
        galpha(256);
        return;
    }
    galpha(150 + ipulse(tm, 1400) * 106 / 256);
    gfit(120, 231, 232, TR(ID[f]), f == 0u ? th->pad : f == 1u ? th->caps[3] : th->mute, 1);
    galpha(256);
}
static void tutor_draw(void)
{
    static const int IDS[3] = {T_HELP_ROLL, T_HELP_LOW, T_HELP_HIGH};
    int32_t step, u, loops = 0, k, n, sc, a, fade = 0, id, row, age, live = tp_live();
    uint32_t tm = (uint32_t)sh.t;
    const tu_step_t *st;
    const char *s, *rest;
    char word[40];
    col_t c;
    if (tp.phase == TP_END) { tp_end_card(TU_N, th->caps[0], tm); tp_foot(2, tm); return; }
    step = tp.step; st = &TU_STEP[step];
    u = tp.pt % st->ms;
    tu_live = live; tu_jrx = -1; tu_live_hop = 0; tu_want_knob = st->act != TA_HOP;
    if (live) {                                  /* your turn: Rock where the player has put him */
        if (tp.evk >= 0) {                       /* a hop from the last key pressed: small from the bottom row, big from the top */
            int32_t big = KEY_IS_SHARP[tp.evk], len = big ? 640 : 480, h = big ? 26 : 12;
            if (tp.ev < len) tu_live_hop = 4 * h * tp.ev / len * (len - tp.ev) / len;
        }
        tu_live_s = TV_SHOWN * 46 / 1000;
        if (st->act == TA_JUMP) {
            u = 1000;
            if (TV_FLY < 0) tu_jrx = TV_JX / 16;
            else if (TV_FLY < TU_FLY_UP) { tu_jrx = (TV_JX0 + (TU_CREST * 16 - TV_JX0) * TV_FLY / TU_FLY_UP) / 16; tu_live_hop = 0; }
            else u = TU_T_OFF + TV_FLY - TU_FLY_UP;
        }
        age = tp.ev; row = tu_want_row(st);
    } else age = tu_tap(step, u, &row);
    c = st->act == TA_ROLL ? th->caps[0] : st->act == TA_JUMP ? th->pad : st->act == TA_SWAP ? th->caps[3] : th->caps[row == 2 ? 4 : 2];
    grect(0, 0, AC_W, AC_H, th->bg);
    for (k = 0; k < 22; k++) gp((int32_t)(ihash((uint32_t)k * 7u + 3u) % 240u), (int32_t)(ihash((uint32_t)k * 13u + 5u) % 60u), th->mute);
    gclip_y1 = TU_SCENE_H;
    if (st->act == TA_JUMP) tu_scene_jump(u, tm); else tu_scene_roll(step, u, loops, tm);
    /* the demo's jump is another place: it fades in, and out again before your turn; so does the other grip's start */
    if (!live) {
        if (st->act == TA_JUMP) fade = u < 250 ? 256 - u * 256 / 250 : u > st->ms - 300 ? 256 - (st->ms - u) * 256 / 300 : 0;
        else if (st->act == TA_SWAP && u < 250) fade = 256 - u * 256 / 250;
    }
    if (fade > 0) { galpha(fade); grect(0, 0, AC_W, TU_SCENE_H, th->bg); galpha(256); }
    gclip_y1 = AC_H;
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
        s = TR(IDS[st->act == TA_ROLL ? 0 : st->row[0]]);
        for (n = 0; s[n] && !(s[n] == ' ' && s[n + 1] == '=') && n < 38; n++) word[n] = s[n];
        word[n] = 0;
        rest = s[n] ? s + n + 3 : s + n;
        if (st->act == TA_ROLL) rest = TR(T_HELP_TURN);   /* the first step says what to do, not which knobs */
        galpha(live ? 256 : imin(256, u * 256 / 200));
        gtextc(120, 4 + 2 * sc, word, c, sc);
        galpha(256);
    }
    tp_overlay(42, c);
    /* where in the lesson it is: a mark for each step under the scene */
    tp_pips(TU_N, TU_SCENE_H + 1, st->ms, c, tm);
    {   /* the demo: the thumb moves to the top row as a step changes to it, and in the last step before its second press;
         * the hands go out to the sides and come back as the grip changes; the knob hand comes in first, alone.
         * Your turn: both hands go out to the sides (they are the player's now) */
        int32_t rf = 256, sl = 0, st_ = 0, e;
        if (live) {
            e = tp.phase == TP_TRY ? imin(300, tp.pt) : 300;
            sl = st_ = 2 * TU_SLIDE * e / 300 * e / 300;
            row = tu_want_row(st);
            if (st->act == TA_ROLL) st_ = 2 * TU_SLIDE;
        } else {
            if (step == 2) rf = imin(256, u * 256 / 300);
            if (st->tap[1] && st->row[1] != st->row[0] && u >= (int32_t)st->tap[1] - TU_REACH) rf = imin(256, (u - (int32_t)st->tap[1] + TU_REACH) * 256 / 300);
            if ((step == 0 || step == 4) && u < 350) { e = 350 - u; sl = TU_SLIDE * e / 350 * e / 350; }
            if ((step == 3 || step == 4) && u > st->ms - 300) { e = u - (st->ms - 300); sl = TU_SLIDE * e / 300 * e / 300; }
            /* the hand at the keys: away while the first step only turns the knob, in as the second begins */
            st_ = step == 0 ? 3 * TU_SLIDE / 2 : step == 1 && u < 300 ? TU_SLIDE * (300 - u) / 300 * (300 - u) / 300 : sl;
        }
        tu_device(st->grip, live ? tp.kev < 250 : st->act == TA_ROLL || st->act == TA_SWAP || (st->act == TA_JUMP && u < TU_T_OFF),
                  row, rf, age, sl, st_, c, tm);
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
    tp_foot(2, tm);
    tu_live = 0; tu_jrx = -1;
}
