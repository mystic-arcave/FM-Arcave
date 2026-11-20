/* SPDX-License-Identifier: GPL-3.0-only */
/* HOME in a machine: how it is played, shown the way the course's controls are (ac_tutor.h). A loop of short steps,
 * each a little scene of the machine over a line drawing of the FM-1, the hands doing what the step says, and on the
 * FM-1's own screen the same scene, very small. The words are the machine's own help lines, one to a step, so every
 * language has them.
 *   CRYSTAL ROAD  a crystal at the line: a key of its lit group (twice, a line of words each); the far side of the
 *                 keyboard, the far side of the road (twice); a crystal missed; KNOB 1: the difficulty, ALGO: the song
 *   JUICE WORKS   keys under a tap: a glass; KNOB 1: pour; to the ticket's line; the same keys: serve; hold and press
 *                 under another tap: the glass moves
 *   NIGHT BLOOM   KNOB 1 turns the petal to a spore; each spore on its colour; SELECT and the second ring; ALGORITHM
 *                 and the third (an arrow to each knob as it joins, as in the game)
 * Each step is shown, then waits for the player to do it (ac_tutor.h: WATCH, YOUR TURN, DONE), or is only shown (info):
 *   CRYSTAL ROAD  on its own song, on the beat: a low crystal; a high one then a low one; the miss (info); KNOB 1
 *                 through the modes and ALGO to another song
 *   JUICE WORKS   a key under tap 1: a glass; KNOB 1 pours it to the ticket's line, then closes; the same keys serve;
 *                 hold one and press under tap 2: it moves
 *   NIGHT BLOOM   KNOB 1 steers the petal onto its spore; each spore on its colour (info); SELECT's ring; ALGORITHM's
 * HOME again returns; REC goes back to the Arcave. */

#define MT_MAXS 4
/* each machine's steps: how long each demo lasts (CRYSTAL ROAD's first three: as long as the song takes), its words (a
 * text and its line; line 9: all its lines) and, taking turns with them, a second (text2, 0: none; CRYSTAL ROAD's
 * change with the song's bars, the others every few seconds); info: the steps only shown, a bit each */
typedef struct { uint8_t n, info; uint16_t ms[MT_MAXS]; int16_t text[MT_MAXS]; uint8_t line[MT_MAXS]; int16_t text2[MT_MAXS]; uint8_t line2[MT_MAXS]; } mt_def_t;
static const mt_def_t MT_DEF[3] = {
    {4, 1u << 2, {0, 0, 0, 4200}, {T_ROAD_1, T_ROAD_2, T_ROAD_3, T_ROAD_4}, {0, 0, 0, 9}, {T_ROAD_1, T_ROAD_2, 0, 0}, {1, 1, 0, 0}},
    {4, 0, {4800, 7200, 4800, 7600}, {T_TABLE_2, T_TABLE_1, T_TABLE_2, T_TABLE_2}, {0, 0, 1, 2}, {0, T_TABLE_3, 0, 0}, {0, 0, 0, 0}},
    {4, 1u << 1, {3600, 3600, 3800, 3800}, {T_BLOOM_1, T_BLOOM_2, T_BLOOM_1, T_BLOOM_3}, {0, 0, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
};
static const uint8_t MJ_OLD[4] = {0, 1, 3, 4};   /* JUICE WORKS: each step's scene (the second pours to the ticket's line) */
/* each step's demo lasts ms; how many times slower than its scene's own timings it plays (JUICE WORKS has the most to read) */
static const uint8_t MT_SLOW[3] = {1, 2, 1};
static const uint8_t TU_KN[8][2] = {{12, 13}, {27, 13}, {12, 29}, {27, 29}, {85, 14}, {102, 14}, {120, 14}, {137, 14}};
enum { MK_MASTER, MK_SELECT, MK_PRESETS, MK_ALGO, MK_K1, MK_K2, MK_K3, MK_K4 };   /* the knobs as drawn */
static int mt_lv;                                /* the scene being drawn is the player's turn (the scenes read tp.v) */
static uint32_t mt_hl;                           /* at your turn: the knobs to use glow, a bit each (MK_...) */
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
        if (mt_hl >> k & 1u) {                   /* your turn: a knob to use, glowing */
            galpha(110 + ipulse(tm, 900) * 146 / 256); gcirc(x0 + TU_KN[k][0], y0 + TU_KN[k][1], 7, c); gcirc(x0 + TU_KN[k][0], y0 + TU_KN[k][1], 8, c);
        }
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

/* CRYSTAL ROAD, small: the road from a horizon at y 6 to the line at y 64, PEBBLE's four lanes (or KNOB 1's choice) */
#define MT_RY0 6
#define MT_RY1 64
static void mt_rproj(int32_t u, int32_t z, int32_t *x, int32_t *y, int32_t *s)
{
    int32_t p;
    z = iclamp(z, 0, 330);
    p = (z * z / 256 * 3 + z) / 4;
    *y = MT_RY0 + (MT_RY1 - MT_RY0) * p / 256;
    *x = 120 + u * (14 + 96 * p / 256) / 256;
    *s = 30 + 150 * p / 256;
}
static int mt_bit(int k)                         /* a key of the 27 -> its bit as mt_device draws it */
{
    int i, n = 0;
    for (i = 0; i < k; i++) if (KEY_IS_SHARP[i] == KEY_IS_SHARP[k]) n++;
    return KEY_IS_SHARP[k] ? 16 + n : n;
}
static uint32_t mt_lane_bits(int lane, int L)
{
    uint32_t m = 0; int k;
    for (k = 0; k < AC_NKEY; k++) if (gh_key_lane(k, L) == lane) m |= 1u << mt_bit(k);
    return m;
}
static int32_t mt_step_ms;                       /* how long the step being drawn lasts */
/* CRYSTAL ROAD's lesson. Its first three steps run on the lesson's own song (FIRST LIGHT, ac_music.h): a bar of
 * count-in, then two bars round and round, the crystals coming down the road four beats ahead of their notes and
 * judged as in the game, PEBBLE's way (a key of the note's lane within GH_WIN of its beat; PERFECT within GH_PERFECT):
 *   A  one low note, on the second bar's one                       (the words: WHEN A CRYSTAL... / PRESS A KEY..., a bar each)
 *   B  a high note on the first bar's three, then the low one      (THE ROAD IS THE KEYBOARD / LOW NOTES LEFT...)
 *   C  only shown: the low note let go by, sour, a hole in the song
 * The demo plays its notes itself, on the beat, the thumbs pressing; at your turn a missed note is sour and the song
 * goes round again until the step's notes are caught in one pass (B: the high one, then the low one). Every step
 * starts the song from a fresh count-in. The last step (KNOB 1, ALGO) has the song going on under it. */
#define LES_LOOP 768                             /* two bars, in ticks (96 a beat) */
#define LES_WIN 40                               /* (GH_WIN[GH_PEBBLE]) */
static struct {
    int32_t n, nt[2], nd[2];                     /* the step's notes in a pass: their ticks in it, their ladder steps */
    int32_t next;                                /* the next note to judge, counted from the song's start (pass * n + j) */
    uint8_t st[8];                               /* each judged note, by its number & 7: 1 PERFECT, 2 GOOD, 3 missed */
    int32_t judge_k, judge_t;                    /* the last judged note, and when (ticks) */
    int32_t rx0, rx1, rt;                        /* Rock's leap: from, to, when (ticks) */
    int auto_, ph, step;                         /* the demo: 1 catches its notes, 2 lets them go; the phase and step it began for */
    uint8_t prev_mode, prev_track, on;           /* the music before the lesson, given back after it */
} les;
static int32_t mt_rx = 120;                      /* where Rock stands on the line, from step to step */
static int32_t mt_lane_x(int lane) { return 120 + ((2 * lane + 1) * 512 / 8 - 256) * 110 / 256; }
static int mt_road_scene(int32_t step) { return (int)step; }   /* (A, B, C, the modes) */
static int32_t les_now(void) { return (int32_t)((tr_now_q8() >> 8) - les_step0 * 24u); }
static int32_t les_tick(int32_t k) { int32_t n = imax(1, les.n); return k / n * LES_LOOP + les.nt[k % n]; }
static int les_lane(int32_t k) { return (int)(les.nd[k % imax(1, les.n)] * 4 / 16); }
static int32_t les_ms(int32_t ticks) { return ticks * 60000 / ((int32_t)(tr_bpm_q8 >> 8) * 96); }
static void les_begin(int sc, int auto_);
static void les_open(void)                       /* HOME in CRYSTAL ROAD: the lesson's song in place of the tune */
{
    les.prev_mode = mus_mode; les.prev_track = mus_track; les.on = 1; les.ph = -1; les.step = -1; mt_rx = 120;
    mus_set(MUS_LESSON, 0);
    les_begin(0, 1);                             /* (the first step's song at once: the page is drawn before its first tick) */
}
static void les_close(void)
{
    if (!les.on) return;
    les.on = 0; les_mask = 0; les_auto = 0;
    mus_set(les.prev_mode, les.prev_track);
}
/* the song again from a fresh count-in, for step sc: auto 1 the demo catching its notes, 2 missing them, 0 your turn */
static void les_begin(int sc, int auto_)
{
    les.n = sc == 1 ? 2 : 1;
    if (sc == 1) { les.nt[0] = 192; les.nd[0] = 13; les.nt[1] = 384; les.nd[1] = 2; }
    else { les.nt[0] = 384; les.nd[0] = 2; }
    les_mask = sc == 1 ? (1u << 8 | 1u << 16) : 1u << 16;
    les_auto = (uint8_t)(auto_ == 1);
    les_step0 = ((tr_now_q8() >> 8) / 96u + 1u) * 4u + 16u;   /* (from the next beat: four beats of count-in) */
    les.auto_ = auto_; les.next = 0; les.judge_k = -1; les.judge_t = -100000; les.rx0 = les.rx1 = mt_rx; les.rt = -100000;
    tp.prog = 0;
#ifdef AC_TEST_SCENE                             /* (tests) when the song's notes land, to press keys by */
    {
        int32_t j, k;
        printf("%6u ms  lesson step %d (%s): notes at", fm1_ms, sc, auto_ == 1 ? "demo" : auto_ == 2 ? "demo, missed" : "your turn");
        for (k = 0; k < 2; k++) for (j = 0; j < les.n; j++) printf(" %d", (int32_t)fm1_ms + les_ms(les_tick(k * les.n + j) - les_now()));
        printf(" ms\n");
    }
#endif
}
static void les_rock(int lane, int32_t now)      /* Rock leaps to a lane */
{
    int32_t f = ease8(iclamp((now - les.rt) * 256 / 16, 0, 256));
    les.rx0 = les.rx0 + (les.rx1 - les.rx0) * f / 256; les.rx1 = mt_lane_x(lane); les.rt = now; mt_rx = les.rx1;
}
static void les_sour(int32_t k) { int32_t p = les_pitch(les.nd[k % imax(1, les.n)]); sfx(p + 1, 0, 260, W_SAW, 2400, 2, 20); sfx(p - 6, 0, 260, W_SQUARE, 1300, 2, 20); }
/* a frame of the song's notes: the demo's own catches and misses, or (your turn) the player's keys */
static void les_judge(int32_t dt, int yours)
{
    int32_t now = les_now(), nowj = now - (dt * (int32_t)(tr_bpm_q8 >> 8) * 96 / 60000) / 2, k;
    if (les.auto_ == 1)
        for (; les_tick(les.next) <= now; les.next++) {   /* (the tune itself plays the note, on its step) */
            les.st[les.next & 7] = 1; les.judge_k = les.next; les.judge_t = now; les_rock(les_lane(les.next), now);
            sfx(84 + SC_PEN[les.next % 5], 0, 180, W_SINE, 900, 1, 40);
        }
    else if (les.auto_ == 2)
        for (; les_tick(les.next) + LES_WIN < now; les.next++) { les.st[les.next & 7] = 3; les.judge_k = les.next; les.judge_t = now; les_sour(les.next); }
    if (les.auto_ || !yours) {                   /* (DONE: what comes down goes by) */
        if (!les.auto_) while (les_tick(les.next) + LES_WIN < now) { les.st[les.next & 7] = 0; les.next++; }
        return;
    }
    for (k = 0; k < AC_NKEY; k++) {              /* a key: the note on its way, if it is of the key's lane and in reach */
        int lane;
        int32_t d;
        if (!(in.kdown >> k & 1u)) continue;
        lane = gh_key_lane((int)k, 4);
        les_rock(lane, now); tp.ev = 0; tp.evk = k;
        d = les_tick(les.next) - nowj;
        if (les_lane(les.next) == lane && iabs(d) <= LES_WIN) {
            int j = (int)(les.next % les.n), perf = iabs(d) <= GH_PERFECT;
            les.st[les.next & 7] = (uint8_t)(perf ? 1 : 2); les.judge_k = les.next; les.judge_t = now;
            mus_mel_voice(les_pitch(les.nd[j]), 0, 256, (uint32_t)les_ms(imax(0, d)));   /* (early: it waits for the beat) */
            if (perf) sfx(84 + SC_PEN[j + 2], (uint32_t)les_ms(imax(0, d)), 180, W_SINE, 900, 1, 40);
            if (j == 0) tp.prog = 1; else if (tp.prog == j) tp.prog = j + 1;   /* (in order, in one pass) */
#ifdef AC_TEST_SCENE
            printf("%6u ms  lesson: caught note %d, %s (%d ticks off)\n", fm1_ms, les.next, perf ? "PERFECT" : "GOOD", d);
#endif
            les.next++;
        } else sfx(key_play(k) + 12, 0, 400, W_TRI, 1500, 6, 120);   /* nothing of its lane in reach: its own note, quietly */
    }
    for (; les_tick(les.next) + LES_WIN < nowj; les.next++) {   /* gone by untouched: sour, and the pass starts over */
        les.st[les.next & 7] = 3; les.judge_k = les.next; les.judge_t = now; les_sour(les.next); tp.prog = 0;
#ifdef AC_TEST_SCENE
        printf("%6u ms  lesson: missed note %d, the song goes round again\n", fm1_ms, les.next);
#endif
    }
}
/* the last step's own state: KNOB 1's mode, the modes seen (a bit each), the song ALGO picks, ALGO turned */
#define MV_MODE tp.v[4]
#define MV_SEEN tp.v[5]
#define MV_SONG tp.v[6]
#define MV_ALGO tp.v[7]
static void mt_road_start(void)
{
    MV_MODE = 0; MV_SEEN = 1; MV_SONG = (int32_t)gh.track; MV_ALGO = 0;
    tp.goal = tp.step == 3 ? 3 : tp.step == 1 ? 2 : 1;
}
static void mt_road_live(int32_t dt)
{
    if (tp.step < 3) { les_judge(dt, tp.phase == TP_TRY); return; }
    if (tp.phase != TP_TRY) return;
    if (in.enc[ENC_KNOB1]) {                     /* KNOB 1 through the modes, ALGO to another song */
        int32_t m = iclamp(MV_MODE + in.enc[ENC_KNOB1], 0, 2);
        tp.kev = 0;
        if (m != MV_MODE) { MV_MODE = m; MV_SEEN |= 1 << m; snd_ui(79); }
    }
    if (in.enc[ENC_ALGORITHM]) { MV_SONG = (int32_t)gh_song_step((uint32_t)MV_SONG, in.enc[ENC_ALGORITHM]); MV_ALGO = 1; snd_ui(84); }
    tp.prog = ((MV_SEEN >> 1) & 1) + ((MV_SEEN >> 2) & 1) + MV_ALGO;
}
/* the road under every step: L lanes, the beats coming down it to land on the line on the song's beat, mushrooms by it */
static void mt_road_bg(int32_t L, int32_t now, int32_t beat)
{
    int32_t i, x, y, s, x2, y2, s2, ph = ((now % 96) + 96) % 96;
    for (i = 0; i < 5; i++) {                    /* the beats coming down */
        int32_t z = 256 - (i * 96 - ph) * 256 / 384;
        if (z < 0 || z > 256) continue;
        mt_rproj(-256, z, &x, &y, &s); mt_rproj(256, z, &x2, &y2, &s2);
        galpha(30 + z / 3); ghl(x, x2 + 1, y, th->bands[i & 3]);
    }
    for (i = 0; i <= L; i++) {                   /* the lanes */
        int32_t u_ = (i * 512 / L) - 256;
        mt_rproj(u_, 0, &x, &y, &s); mt_rproj(u_, 256, &x2, &y2, &s2);
        galpha(i == 0 || i == L ? 230 : !(i % (L / 4)) ? 90 : 40);
        gline(x, y, x2, y2, i == 0 || i == L ? th->rim : th->bands[1]);
    }
    galpha(256);
    for (i = 0; i < 4; i++) {                    /* mushrooms by the road, a beat apart */
        int32_t z = 256 - (i * 96 + 48 - ph) * 256 / 384;
        if (z < 0 || z > 256) continue;
        mt_rproj((i & 1) ? 330 : -330, z, &x, &y, &s);
        galpha(70 + z / 2); mush_draw(x, y, s * 200 / 256, th->caps[i % 5], th->stem, beat);
    }
    galpha(256);
}
static void mt_road_rock(int32_t x0, int32_t x1, int32_t f, uint32_t tm)
{
    int32_t hop = 12 * f * (256 - f) / 16384;
    rock_draw(x0 + (x1 - x0) * f / 256, MT_RY1 - 6 - hop, 180, (int32_t)tm * 40, (tm % 3100u) < 150u ? 40 : 256, th->rock, th->rock_rim, th->dark, th->moss, th->eye, th->pupil);
}
/* steps A, B, C: the lesson's song on the road */
static int mt_road_song(uint32_t tm, mt_act_t *act, uint32_t *lit)
{
    static const int8_t KEY[4] = {4, 9, 16, 23}, SIDE[4] = {-1, -1, 1, 1};   /* (the key the thumb presses: a white one of the lane) */
    int32_t now = les_now(), ph = ((now % 96) + 96) % 96, beat = 256 - imin(256, ph * 256 / 40), k, x, y, s, n = 0, j;
    mt_road_bg(4, now, now < -384 ? 0 : beat);
    for (k = imax(0, les.next - 2); tp.phase != TP_DONE && les_tick(k) - now <= 384; k++) {   /* the notes coming down */
        int32_t dz = les_tick(k) - now, st = k < les.next ? les.st[k & 7] : 0, lane = les_lane(k), lu = ((2 * lane + 1) * 512 / 8) - 256;
        col_t c = GH_ZONE[lane];
        if (st == 0 && dz >= -LES_WIN) {         /* on its way (or waiting for its key, just past the line) */
            mt_rproj(lu, 256 - dz * 256 / 384, &x, &y, &s);
            gh_crystal(x, y, s, c, beat, 1, 0);
            if (dz <= 96) *lit |= mt_lane_bits(lane, 4);   /* its keys light a beat ahead */
        } else if (st == 3 && now - les.judge_t < 120 && k == les.judge_k) {   /* missed: gone dark */
            mt_rproj(lu, 256 + (now - les.judge_t) * 64 / 120, &x, &y, &s);
            galpha(256 - (now - les.judge_t) * 256 / 120); gh_crystal(x, y, s, c, 0, 1, 1); galpha(256);
        }
    }
    if (les.judge_k >= 0 && now - les.judge_t < 60 && les.st[les.judge_k & 7] != 3) {   /* caught: shards, a ring, PERFECT */
        int32_t q = (now - les.judge_t) * 34 / 60, lane = les_lane(les.judge_k);
        col_t c = GH_ZONE[lane];
        x = mt_lane_x(lane);
        galpha(256 - q * 7); gcirc(x, MT_RY1 - 6, 5 + q, c);
        for (j = 0; j < 7; j++) {
            int32_t a = -28000 + j * 9300, d0 = 3 + q, d1 = 7 + q * 2;
            gline2(x + ((d0 * icos(a)) >> 14), MT_RY1 - 8 + ((d0 * isin(a)) >> 14), x + ((d1 * icos(a)) >> 14), MT_RY1 - 8 + ((d1 * isin(a)) >> 14), j & 1 ? th->text : c);
        }
        galpha(256 - (now - les.judge_t) * 256 / 60);
        gfit(x, MT_RY1 - 30 - (now - les.judge_t) / 4, 100, TR(les.st[les.judge_k & 7] == 1 ? T_PERFECT : T_GOOD), les.st[les.judge_k & 7] == 1 ? th->text : th->pad, 1);
        galpha(256);
    }
    if (les.judge_k >= 0 && les.st[les.judge_k & 7] == 3 && now - les.judge_t < 150) {   /* a hole where the note was */
        galpha(200 - (now - les.judge_t) * 200 / 150); gfit(120, 30, 200, "\xE2\x99\xAA ?", th->mute, 2); galpha(256);
    }
    if (now < 0 && now >= -384) {                /* the count-in: 1 2 3 4, large, on its beats */
        char cnt[2] = {(char)('1' + (now + 384) / 96), 0};
        galpha(beat); gfit(120, 22, 60, cnt, th->text, 3); galpha(256);
    }
    galpha(150 + beat * 106 / 256); grect(4, MT_RY1, 232, 2, th->rim); galpha(256);
    mt_road_rock(les.rx0, les.rx1, ease8(iclamp((now - les.rt) * 256 / 16, 0, 256)), tm);
    if (les.auto_ && tp.phase == TP_WATCH)       /* the demo: a thumb on each note's key, pressing it on the beat (or not) */
        for (j = 0; j < les.n; j++) {
            int lane = (int)(les.nd[j] * 4 / 16);
            int32_t p = (now - les.nt[j] + LES_LOOP / 2) / LES_LOOP, age = les_ms(now - (p * LES_LOOP + les.nt[j]));
            if (now - les.nt[j] + LES_LOOP / 2 < 0) age = -1000;
            act[n].kind = MTA_THUMB; act[n].a = (uint8_t)mt_bit(KEY[lane]); act[n].side = SIDE[lane];
            act[n].f = les.auto_ == 1 ? tu_press(age) : 0; act[n].sl = 0; n++;
            if (les.auto_ == 1 && age >= 0 && age < 420) mt_age[mt_bit(KEY[lane])] = age;
        }
    return (int)n;
}
/* the last step: KNOB 1 turns from PEBBLE to ROCK to CRYSTAL, ALGO to another song (the demo, or the player's) */
static int mt_road(int32_t step, int32_t u, uint32_t tm, mt_act_t *act, uint32_t *lit, int *blit, int *bdown)
{
    int32_t now = les_now(), beat = 256 - imin(256, (((now % 96) + 96) % 96) * 256 / 40), n = 0, k;
    int32_t L = mt_lv ? 4 << MV_MODE : u < 1100 ? 4 : u < 2200 ? 8 : 16;
    int sel = L == 4 ? 0 : L == 8 ? 1 : 2;
    (void)blit; (void)bdown;
    if (step < 3) return mt_road_song(tm, act, lit);
    mt_road_bg(L, now, beat);
    galpha(150 + beat * 106 / 256); grect(4, MT_RY1, 232, 2, th->rim); galpha(256);
    mt_road_rock(120, 120, 256, tm);
    for (k = 0; k < 3; k++) {
        int32_t mx = 44 + k * 76;
        if (k == sel) { galpha(220); grect(mx - 32, 4, 64, 15, th->panel); galpha(256); gbox(mx - 32, 4, 64, 15, th->caps[0]); }
        gfit(mx, 8, 60, GH_MODE[k], k == sel ? th->text : th->mute, 1);
    }
    {   /* and ALGO, to another song: its name under the modes */
        uint32_t song = mt_lv ? (uint32_t)MV_SONG : u < 3000 ? gh.track : gh_song_step(gh.track, 1);
        galpha(mt_lv || u >= 2300 ? 256 : 110); gfit(120, 24, 220, gh_song_name(song), gh_song_col(song), 1); galpha(256);
    }
    if (!mt_lv) {
        act[n].kind = MTA_PINCH; act[n].a = MK_K1; act[n].side = 1; act[n].f = (u > 700 && u < 1150) || (u > 1800 && u < 2250);
        act[n].sl = imax(mt_in(u), mt_out(u, mt_step_ms)); n++;
        act[n].kind = MTA_PINCH; act[n].a = MK_ALGO; act[n].side = -1; act[n].f = u > 2700 && u < 3150;
        act[n].sl = imax(mt_in(u - 2300), mt_out(u, mt_step_ms)); n++;
    }
    *lit = 0;
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
/* JUICE WORKS at your turn */
#define MJ_HAS tp.v[0]                           /* a glass under tap 1, and ms since it came */
#define MJ_POP tp.v[1]
#define MJ_FILL tp.v[2]                          /* what is in it (Q8 parts), tap 1's valve (0..3) */
#define MJ_VALVE tp.v[3]
#define MJ_SERVE tp.v[4]                         /* ms since it was served, or moved (-1: not yet) */
#define MJ_MOVE tp.v[5]
#define MJ_TOL (Q8(1) / 3)                       /* the ticket's line: a little over or under is fine */
static uint32_t mt_zone_keys(int z) { uint32_t m = 0; int k; for (k = 0; k < AC_NKEY; k++) if (jw_zone(k) == z) m |= 1u << k; return m; }
static uint32_t mt_zone_bits(int z) { uint32_t m = 0; int k; for (k = 0; k < AC_NKEY; k++) if (jw_zone(k) == z) m |= 1u << mt_bit(k); return m; }
static void mt_juice_start(void)
{
    int32_t s = MJ_OLD[tp.step];                 /* (the steps by their scenes: 0 glass, 1 pour, 3 serve, 4 move) */
    MJ_HAS = s > 0; MJ_POP = 1000; MJ_FILL = s == 3 ? Q8(3) : 0; MJ_VALVE = 0; MJ_SERVE = MJ_MOVE = -1;
    tp.goal = s == 1 ? 256 : 1;                  /* (pouring: the mark fills with the glass) */
}
static void mt_juice_live(int32_t dt)
{
    int32_t s = MJ_OLD[tp.step], k;
    uint32_t z0 = mt_zone_keys(0), z1 = mt_zone_keys(1);
    MJ_POP += dt;
    if (MJ_SERVE >= 0) MJ_SERVE += dt;
    if (MJ_MOVE >= 0) MJ_MOVE += dt;
    MJ_FILL = imin(Q8(JW_CAP), MJ_FILL + MJ_VALVE * dt * 256 / 2100);   /* the tap pours (a part in 0.7 s, wide open) */
    if (tp.phase != TP_TRY) return;
    if (in.kdown) {
        for (k = AC_NKEY - 1; k > 0 && !(in.kdown >> k & 1u); k--) ;
        tp.ev = 0; tp.evk = k;
        sfx(key_play(k) + 12, 0, 300, W_TRI, 2200, 4, 120);
    }
    if (in.enc[ENC_KNOB1] && s == 1) {   /* KNOB 1: tap 1 opens and closes */
        int32_t v = iclamp(MJ_VALVE + in.enc[ENC_KNOB1], 0, 3);
        tp.kev = 0;
        if (v != MJ_VALVE) { MJ_VALVE = v; sfx(48 + v * 5, 0, 90, W_SQUARE, 1300, 1, 60); }
    }
    switch (s) {
    case 0:                                      /* a key under tap 1: a glass */
        if (in.kdown & z0) { MJ_HAS = 1; MJ_POP = 0; tp.prog = 1; sfx(90, 0, 160, W_SINE, 1800, 1, 160); }
        break;
    case 1:                                      /* KNOB 1 pours, to the ticket's line, and closes the tap */
        if (MJ_VALVE == 0 && MJ_FILL > Q8(3) + MJ_TOL) {   /* too much: a new glass (sour) */
            sfx(41, 0, 260, W_SAW, 2400, 2, 20); MJ_FILL = 0; MJ_POP = 0;
        }
        tp.prog = MJ_VALVE == 0 && iabs(MJ_FILL - Q8(3)) <= MJ_TOL ? 256 : imin(255, MJ_FILL * 255 / Q8(3));
        break;
    case 3:                                      /* the same keys again: served */
        if ((in.kdown & z0) && MJ_SERVE < 0) { MJ_SERVE = 0; tp.prog = 1; snd_chime(1); }
        break;
    default:                                     /* hold a key under tap 1 and press under tap 2: it moves */
        if ((in.kdown & z1) && (in.keys & z0) && MJ_MOVE < 0) { MJ_MOVE = 0; tp.prog = 1; sfx(90, 0, 160, W_SINE, 1800, 1, 160); }
    }
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
    if (mt_lv) {                                 /* your turn: the player's glass */
        int32_t q;
        has = MJ_HAS; valve = MJ_VALVE; g.n = MJ_FILL > 0; g.c[0] = 0; g.v[0] = MJ_FILL; gy = floor; galp = 256; gx = BX[0];
        if (step == 3 && MJ_SERVE >= 0) { q = imin(500, MJ_SERVE); gy = floor - q * 40 / 500; galp = 256 - q * 256 / 500; }
        if (step == 4 && MJ_MOVE >= 0) gx = BX[0] + (BX[1] - BX[0]) * mt_ramp(MJ_MOVE, 0, 400) / 256;
        u = step == 3 ? (MJ_SERVE >= 0 ? 700 + MJ_SERVE : 0) : 1000;   /* (the ticket, GOOD: as the demo's, from the serve) */
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
        int32_t pop = mt_lv ? mt_ramp(MJ_POP, 0, 200) : step == 0 ? mt_ramp(u, 700, 900) : step == 4 ? mt_ramp(u, 200, 400) : 256;
        galpha(galp * pop / 256); jw_glass_draw(gx, gy, 8, &g, step == 2 ? th->pad : th->text); galpha(256);
    }
    if (step < 3 || (step == 3 && u < 1200)) {   /* the ticket: three parts of the yellow */
        int32_t tx = 214, hl = (step == 1 || step == 2) && ipulse(tm, 700) > 128;
        galpha(step == 3 ? 256 - imin(256, imax(0, u - 700) * 256 / 500) : 256);
        gline(tx, 3, tx, 10, th->mute);
        grect(tx - 11, 10, 23, 58, th->panel); gbox(tx - 11, 10, 23, 58, hl ? th->pad : th->text);
        jw_glass_draw(tx, 62, 8, &tk, th->text);   /* (the same glass as the one being filled) */
        galpha(256);
        if (step == 1 || step == 2) {            /* the glass and the ticket: the same line */
            int32_t ly = floor - 1 - 3 * JW_UPX, ty = 61 - 3 * JW_UPX, x;
            for (x = gx + 11; x < tx - 13; x += 4) gp(x, ly + (ty - ly) * (x - gx - 11) / (tx - 24 - gx), th->pad);
        }
    }
    if (step == 3 && u >= 700 && u < 1700) { galpha(256 - imax(0, u - 1300) * 256 / 400); gfit(120, 44 - (u - 700) / 60, 120, TR(T_GOOD), th->pad, 2); galpha(256); }
    if (mt_lv) {                                 /* your turn: no hands; the keys under tap 1 lit (and tap 2's while one is held) */
        *lit = mt_zone_bits(0) | (step == 4 && (in.keys & mt_zone_keys(0)) ? mt_zone_bits(1) : 0u);
        mt_hl = step == 1 ? 1u << MK_K1 : 0u;
        return 0;
    }
    /* the hands: the left thumb under tap 1 (keys 0-3); the right hand on KNOB 1; at the end the right thumb under tap 2 */
    act[n].kind = MTA_THUMB; act[n].a = 2; act[n].side = -1;
    act[n].f = step == 0 ? tu_press(u - 700) : step == 3 ? tu_press(u - 700) : step == 4 ? (u < 600 ? tu_press(u - 600) : u < 2600 ? 256 : tu_press(u - 2600 + 150)) : 0;
    act[n].sl = step == 0 ? mt_in(u) : step == 4 ? mt_out(u, MT_DEF[1].ms[3] / MT_SLOW[1]) : 0; n++;
    if (step == 0) mt_press(2, u, 700);
    if (step == 3) mt_press(2, u, 700);
    if (step == 4 && u >= 600 && u < 2600) mt_age[2] = 100;
    if (step >= 1 && step <= 3) {
        act[n].kind = MTA_PINCH; act[n].a = MK_K1; act[n].side = 1; act[n].f = step == 1 && ((u > 300 && u < 650) || (u > 2700 && u < 3050));
        act[n].sl = step == 1 ? mt_in(u) : step == 3 ? mt_out(u, MT_DEF[1].ms[2] / MT_SLOW[1]) : 0; n++;
    }
    if (step == 4) {
        act[n].kind = MTA_THUMB; act[n].a = 5; act[n].side = 1; act[n].f = tu_press(u - 1500); act[n].sl = imax(mt_in(u - 700), mt_out(u, MT_DEF[1].ms[3] / MT_SLOW[1])); n++;
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
/* NIGHT BLOOM at your turn */
#define MB_A(k) tp.v[k]                          /* the petals' angles (65536 a turn), the first three rings */
#define MB_I tp.v[3]                             /* which of the step's spores is coming, how long it has been coming */
#define MB_T tp.v[4]
#define MB_HIT tp.v[5]                           /* ms since it was caught (-1: not yet) */
#define MB_BLOOM tp.v[6]                         /* the flowers in the garden */
#define MB_IN 2200                               /* a spore takes this long to come in to its ring; there it waits */
static const int16_t MB_R[3] = {14, 25, 36};
static const int32_t MB_SP[4][2] = {{8000, 8000}, {30000, 45536}, {20000, 20000}, {40000, 40000}};   /* each step's spores */
static int mb_ring(int32_t step) { return step < 2 ? 0 : (int)step - 1; }
static void mt_bloom_start(void)
{
    static const int8_t BLOOMED[4] = {0, 1, 3, 4};
    int32_t s = tp.step;
    if (s == 0) MB_A(0) = -16384;
    if (s == 2) MB_A(1) = -16384;
    if (s == 3) MB_A(2) = -16384;
    MB_I = 0; MB_T = 0; MB_HIT = -1; MB_BLOOM = BLOOMED[s];
    tp.goal = s == 1 ? 2 : 1;
}
static void mt_bloom_live(int32_t dt)
{
    int32_t s = tp.step, r = mb_ring(s), k;
    if (MB_HIT >= 0) {
        MB_HIT += dt;
        if (MB_HIT >= 500 && tp.phase == TP_TRY && MB_I + 1 < tp.goal) { MB_I++; MB_T = 0; MB_HIT = -1; }   /* the next spore */
        return;
    }
    MB_T += dt;
    if (tp.phase != TP_TRY) return;
    for (k = 0; k <= r; k++)                     /* each ring's knob turns its petal, a twenty-fourth of a turn a click */
        if (in.enc[NB_ENC[k]]) { MB_A(k) += in.enc[NB_ENC[k]] * 2731; tp.kev = 0; }
    if (MB_T >= MB_IN * 2 / 3 && iabs((int16_t)(MB_A(r) - MB_SP[s][MB_I])) < NB_ARC) {   /* on its petal: caught */
        MB_HIT = 0; MB_BLOOM++; tp.prog++;
        sfx(72 + SC_PEN[MB_BLOOM % 5], 0, 600, W_SINE, 3000, 4, 170);
    }
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
    if (mt_lv) { a1 = MB_A(0); a2 = MB_A(1); a3 = MB_A(2); w2 = step >= 2 ? 256 : 0; w3 = step == 3 ? 256 : 0; u = 100000; }   /* (none of the demo's spores) */
    galpha(140); for (k = 0; k < 18; k++) gp((int32_t)(ihash((uint32_t)k * 7u + 1u) % 240u), (int32_t)(ihash((uint32_t)k * 3u + 9u) % 76u), th->mute); galpha(256);
    /* the garden: a flower for each spore caught so far in the loop */
    bloomed = mt_lv ? MB_BLOOM : step == 0 ? (u >= 2600) : step == 1 ? 1 + (u >= 1300) + (u >= 3000) : step == 2 ? 3 + (u >= 3000) : 4 + (u >= 3000);
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
    if (mt_lv) {                                 /* your turn: the player's petals, and the spore coming or waiting */
        int r = mb_ring(step);
        int32_t ang = MB_SP[step][MB_I], rr = MB_T < MB_IN ? mt_in_r(MB_T, 0, MB_IN, MB_R[r] + 5) : MB_R[r] + 5 + (tsin(tm, 50, 0) >> 13);
        a1 = MB_A(0); a2 = MB_A(1); a3 = MB_A(2);
        w2 = step >= 2 ? 256 : 0; w3 = step == 3 ? 256 : 0; bloomed = MB_BLOOM;
        if (MB_HIT < 0) mt_spore(cx, cy, rr, ang, NB_COL[r]);
        else if (MB_HIT < 250) { if (r == 0) hit1 = 1; else if (r == 1) hit2 = 1; else hit3 = 1; }
    }
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
        int32_t ta = mt_lv ? (step == 1 ? 0 : 256) : step == 0 ? imin(256, imax(0, 1800 - u) * 256 / 300) : (step == 2 || step == 3) ? imin(mt_ramp(u, 0, 300), imax(0, 2600 - u) * 256 / 300) : 0;
        if (step == 0) nb_tag(228, 10, -8192, 192, 18, 0, ta, tm, 1);
        if (step == 2) nb_tag(12, 10, -24576, 36, 22, 1, ta, tm, 1);
        if (step == 3) nb_tag(12, 70, 24576, 40, 50, 2, ta, tm, 1);
    }
    *lit = 0;
    if (mt_lv) { mt_hl = step < 2 ? 1u << MK_K1 : step == 2 ? 1u << MK_SELECT : 1u << MK_ALGO; return 0; }
    /* the hands: the right hand on KNOB 1 throughout; the left on SELECT for the second ring, ALGORITHM for the third */
    act[n].kind = MTA_PINCH; act[n].a = MK_K1; act[n].side = 1;
    act[n].f = (step == 0 && u > 500 && u < 2200) || (step == 1 && ((u > 300 && u < 1100) || (u > 1700 && u < 2700)));
    act[n].sl = step == 0 ? mt_in(u) : step == 3 ? mt_out(u, MT_DEF[2].ms[3]) : 0; n++;
    if (step == 2) { act[n].kind = MTA_PINCH; act[n].a = MK_SELECT; act[n].side = -1; act[n].f = u > 800 && u < 2400; act[n].sl = imax(mt_in(u - 300), mt_out(u, MT_DEF[2].ms[2])); n++; }
    if (step == 3) { act[n].kind = MTA_PINCH; act[n].a = MK_ALGO; act[n].side = -1; act[n].f = u > 800 && u < 2400; act[n].sl = imax(mt_in(u - 300), mt_out(u, MT_DEF[2].ms[3])); n++; }
    *lit = 0;
    return (int)n;
}

/* ---------------------------------------------------------------- the lesson: sounds, lights, the page */
/* how long a step's demo lasts: CRYSTAL ROAD's first three, through the count-in and the step's notes */
static int32_t mt_watch_ms(int w)
{
    if (w == 0 && tp.step < 3) {
        if (les.step != tp.step || les.ph != TP_WATCH) return tp.pt + 1000;   /* (the song starts this frame) */
        return tp.pt + les_ms(imax(0, les.nt[les.n - 1] + 144 - les_now()));   /* (a beat and a half after its last note) */
    }
    return MT_DEF[w].ms[tp.step];
}
static void mtutor_open(void) { if (mc.which == 0) les_open(); }
static void mtutor_close(void) { les_close(); }
static void mtutor_tick(int32_t dt)
{
    static void (*const START[3])(void) = {mt_road_start, mt_juice_start, mt_bloom_start};
    static void (*const LIVE[3])(int32_t) = {mt_road_live, mt_juice_live, mt_bloom_live};
    /* the demos' moments that sound: {step, ms, what} - 0 a low key, 1 a high key, 6 a knob's click, 3 a catch, 4 a
     * glass, 5 served */
    static const int16_t EV[3][8][3] = {
        {{3, 1100, 6}, {3, 2200, 6}, {3, 3000, 6}, {-1, 0, 0}},   /* (the first three: the song plays them) */
        {{0, 700, 4}, {2, 700, 5}, {3, 200, 4}, {3, 1500, 0}, {-1, 0, 0}},
        {{0, 2600, 3}, {1, 1300, 3}, {1, 3000, 3}, {2, 3000, 3}, {3, 3000, 3}, {-1, 0, 0}},
    };
    int w = mc.which, k;
    tp.idle_max = w == 0 && tp.step < 2 ? 0x7FFFFFFF : TP_IDLE_MS;   /* (CRYSTAL ROAD's song goes round until the notes are caught) */
    tp_run(dt, MT_DEF[w].n, mt_watch_ms(w), (int)(MT_DEF[w].info >> tp.step & 1u), START[w], LIVE[w]);
    led_breathe_want |= 1u << BTN_REC;
    if (w == 0 && (les.ph != tp.phase || les.step != tp.step)) {   /* CRYSTAL ROAD: each demo and each turn, the song afresh */
        les.ph = tp.phase; les.step = tp.step;
        if (tp.step < 3 && tp.phase == TP_WATCH) les_begin(tp.step, tp.step == 2 ? 2 : 1);
        else if (tp.step < 3 && tp.phase == TP_TRY) { les_begin(tp.step, 0); tp.goal = les.n; }
        else { les_mask = 0; les_auto = 0; les.auto_ = 0; }   /* (done, or the last step: the melody whole, softly) */
    }
    if (w == 0 && tp.step < 3 && tp.phase == TP_WATCH) les_judge(dt, 0);   /* (the demo's own catches) */
    if (tp.phase != TP_WATCH) return;
    for (k = 0; k < 8 && EV[w][k][0] >= 0; k++) {
        if (EV[w][k][0] != tp.step || !TP_AT(EV[w][k][1] * MT_SLOW[w])) continue;
        switch (EV[w][k][2]) {
            case 0: sfx(key_play(4) + 12, 0, 500, W_TRI, 2800, 5, 180); break;
            case 1: sfx(key_play(23) + 12, 0, 500, W_TRI, 2800, 5, 180); break;
            case 6: snd_ui(79); break;
            case 3: sfx(72 + SC_PEN[k % 5], 0, 600, W_SINE, 3000, 4, 170); break;
            case 4: sfx(90, 0, 160, W_SINE, 1800, 1, 160); break;
            default: snd_chime(1); break;
        }
    }
}
static void mtutor_draw(void)
{
    int w = mc.which, live = tp_live();
    int32_t step = tp.step, u, k, n = 0, a;
    uint32_t tm = (uint32_t)sh.t, lit = 0, rowmask = 0;
    int blit = -1, bdown = -1;
    mt_act_t act[4];
    col_t c = th->caps[w * 2 % 5];
    if (tp.phase == TP_END) { tp_end_card(MT_DEF[w].n, c, tm); tp_foot(3, tm); return; }
    u = MT_DEF[w].ms[step] ? tp.pt % MT_DEF[w].ms[step] : tp.pt;
    for (k = 0; k < 27; k++) mt_age[k] = -1;
    mt_lv = live; mt_hl = 0;
    grect(0, 0, AC_W, AC_H, th->bg);
    gclip_y1 = TU_SCENE_H;
    mt_step_ms = MT_DEF[w].ms[step];
    if (w == 0) {
        if (live && step == 3) { u = 1000; mt_hl = 1u << MK_K1 | 1u << MK_ALGO; }
        n = mt_road(mt_road_scene(step), u, tm, act, &lit, &blit, &bdown);
    } else if (w == 1) n = mt_juice(MJ_OLD[step], u / MT_SLOW[1], tm, act, &lit, &blit, &bdown);
    else n = mt_bloom(step, u, tm, act, &lit, &blit, &bdown);
    if (w == 0 && step < 3) { if (!live) u = 1000; }   /* (the song's own count-in: no fade) */
    if (live) {                                  /* your turn: no hands; the key the player pressed goes down */
        n = 0;
        if (tp.evk >= 0 && tp.ev < 420) mt_age[mt_bit((int)tp.evk)] = tp.ev;
    }
    gclip_y1 = AC_H;
    mt_mini(120);                                /* (before the overlay: the FM-1's own screen shows the game alone) */
    gclip_y1 = TU_SCENE_H; tp_overlay(40, c); gclip_y1 = AC_H;
    if (!live && u < 250) { galpha(256 - u * 256 / 250); grect(0, 0, AC_W, TU_SCENE_H, th->bg); galpha(256); }   /* each demo fades in */
    /* under the scene, a mark for each step (the machine is the one just left: the scene is all its own) */
    tp_pips(MT_DEF[w].n, TU_SCENE_H + 1, mt_watch_ms(w), c, tm);
    {   /* the step's line, under the scene, over the device; a second taking turns with it (CRYSTAL ROAD: a bar each) */
        char buf[120];
        int alt = MT_DEF[w].text2[step] && (w == 0 && step < 3 ? les_now() >= 0 && (les_now() / 384) & 1 : (tp.pt / 3500) & 1);
        const char *s = TR(alt ? MT_DEF[w].text2[step] : MT_DEF[w].text[step]);
        int32_t m = 0, ln = alt ? MT_DEF[w].line2[step] : MT_DEF[w].line[step], h;
        while (ln > 0 && ln != 9 && *s) { if (*s == '\n') ln--; s++; }
        while (*s && (*s != '\n' || ln == 9) && m < 118) buf[m++] = *s++;
        buf[m] = 0;
        a = ts(1, 2);
        if (gblock_h(232, buf, a) > 3 * TLH(a)) {   /* too long for three lines: its two sentences, one after the other */
            int32_t j;
            for (j = 0; buf[j] && !(buf[j] == '.' && buf[j + 1] == ' '); j++) ;
            if (buf[j]) {
                if (live ? !((tp.pt / 3500) & 1) : u < MT_DEF[w].ms[step] / 2) buf[j + 1] = 0;
                else { int32_t q = 0; j += 2; while (buf[j]) buf[q++] = buf[j++]; buf[q] = 0; }
            }
        }
        while (a > 1 && gblock_h(232, buf, a) > 3 * TLH(a)) a--;
        h = gblock_h(232, buf, a);
        galpha(live || MT_DEF[w].text2[step] ? 256 : imin(256, u * 256 / 250));
        gblock(4, TU_SCENE_H + 5 + imax(0, (3 * TLH(ts(1, 2)) - h) / 2), 232, buf, th->text, a, 1);
        galpha(256);
    }
    mt_device(lit, mt_keyage, blit, bdown, act, (int)n, c, tm);
    /* the lights on the FM-1 itself: the keys the step is about, and any going down */
    for (k = 0; k < 27; k++) {
        int top = k >= 16, idx = top ? k - 16 : k;
        if (top && idx > 10) continue;
        if (lit >> k & 1u) rowmask |= 1u << mt_chrom(top, idx);
        if (!live && mt_age[k] >= 0 && mt_age[k] < 220) led_key_want |= 1u << mt_chrom(top, idx);
    }
    led_key_dim_want |= rowmask;
    if (live) led_key_want |= in.keys;
    if (bdown >= 0) led_btn_want |= 1u << (bdown + 2); else if (blit >= 0) led_btn_want |= 1u << (blit + 2);
    tp_foot(3, tm);
    mt_lv = 0; mt_hl = 0;
}
