/* SPDX-License-Identifier: GPL-3.0-only */
/* Mending the jukebox, in two parts, both after UMBRA OBSCURA:
 *   the lamp   four rings of glass, each with a crack. KNOB 1, 2, 3 and 4 turn a ring each, a notch a click, and each
 *              outer ring drags the one inside it the other way. Light gets out only where the cracks line up. Two
 *              neighbours in line straight up, down, left or right lock together, and so does each ring that joins
 *              them: a locked ring no longer turns, nor is it dragged (the numbers at the foot show which are locked).
 *   the dish   a flywheel. Slide a hand a long way along the keys (or turn a knob) to spin it: how fast the hand went
 *              is how fast it turns, and its speed is the tempo, four beats to the turn. Hold a speed for eight beats
 *              and it is yours. Then KNOB 1-4 are the song's effects (HOME tells what each does), and REC, and two
 *              bars of keys are your song. Once the power is back, PLAY at the jukebox does the dish again: a new song.
 * Between them, the first time: the lamp's light swells until it fills the screen, and the dish comes out of it under
 * two large cards (the jukebox is mended; make a song to celebrate). While the dish stands still a hand on a drawing of
 * the keys brushes along them, back and forth, and the FM-1's own keys light under it. */

/* ---------------------------------------------------------------- the lamp */
#define LP_RINGS 4                               /* one to a numbered knob */
static const uint8_t LP_N[LP_RINGS] = {8, 12, 16, 24}, LP_R[LP_RINGS] = {28, 46, 64, 82}, LP_MUL[LP_RINGS] = {6, 4, 3, 2};
static const int16_t LP_GW[LP_RINGS] = {3400, 2700, 2100, 1700};   /* a crack's half width, angle units */
/* help, for whoever is stuck: after a minute without the light (all visits counted), HOME breathes and a note says it
 * gives a hint (a page: how the rings drag each other, and the way through); after two, holding HOME three seconds
 * skips the rings: they turn into line by themselves, outside first */
#define LP_HINT_MS 60000
#define LP_SKIP_MS 120000
#define LP_SKIP_HOLD 3000
#define LP_GLIDE 900                             /* how long a ring takes to glide into line; the next starts after LP_STAGGER */
#define LP_STAGGER 260
#define LP_DONE_MS 3200                          /* the light back: this long, then the dish */
#define JK_BLOOM_IN 800                          /* its last moments: the light swells to fill the screen */
static struct {
    int32_t pos[LP_RINGS], hold, spin, flash[LP_RINGS], t, done_t, idle, home_t, skip_t, off[LP_RINGS];
    uint8_t done, hint, home_on, skip, lock;     /* lock: the rings locked in line (bit = ring), one run of neighbours */
    int32_t lock_t[LP_RINGS], nope[LP_RINGS];    /* since each locked (ms, for its flash); a locked knob turned (ms left) */
} lp;
static int32_t lp_stuck;                         /* time spent at the rings without the light, every visit (not saved) */

static void lamp_reset(void)
{
    uint32_t i;
    for (i = 0; i < sizeof lp; i++) ((uint8_t *)&lp)[i] = 0;
    lp.pos[0] = 3; lp.pos[1] = 7; lp.pos[2] = 5; lp.pos[3] = 9;
    sy_drone_want = 110;
}
/* how far ring i still has to glide while the rings are being skipped (angle units, 0 once there) */
static int32_t lamp_glide(int i)
{
    int32_t u;
    if (!lp.skip) return 0;
    u = iclamp((lp.skip_t - (LP_RINGS - 1 - i) * LP_STAGGER) * 256 / LP_GLIDE, 0, 256);
    return lp.off[i] * (256 - ease8(u)) / 256;
}
static int32_t lamp_ang(int i) { return lp.pos[i] * 65536 / LP_N[i] - 16384 + lp.spin + lamp_glide(i); }
static int lamp_eq(int i, int j) { return lp.pos[i] * LP_MUL[i] == lp.pos[j] * LP_MUL[j]; }
static void lamp_turn(int n, int32_t d)
{
    static const uint8_t DEG[LP_RINGS] = {0, 2, 4, 6};
    if (lp.done || lp.skip || lp.hint || !d) return;
    if (lp.lock >> n & 1u) {                     /* locked: it stays, and says so */
        if (lp.nope[n] < 200) sfx(43, 0, 120, W_SQUARE, 900, 1, 90);
        lp.nope[n] = 450; lp.idle = 0;
        return;
    }
    if (d > 1) { lamp_turn(n, d - 1); d = 1; }   /* a quick turn: every notch counts */
    if (d < -1) { lamp_turn(n, d + 1); d = -1; }
    lp.pos[n] = ((lp.pos[n] + d) % LP_N[n] + LP_N[n]) % LP_N[n];
    lp.flash[n] = 200;
    if (n > 0 && !(lp.lock >> (n - 1) & 1u)) { lp.pos[n - 1] = ((lp.pos[n - 1] - d) % LP_N[n - 1] + LP_N[n - 1]) % LP_N[n - 1]; lp.flash[n - 1] = 120; }
    sfx_ex(60 + SC_PEN[(DEG[n] + lp.pos[n] % 3) % 5] + 12 * ((DEG[n] + lp.pos[n] % 3) / 5), 0, 320, W_TRI, 3200, 3, 150, 0, (2 * n - 3) * 25, 0);
    lp.idle = 0;
}
/* rings in line lock: two neighbours whose cracks agree straight up, down, left or right (a quarter: every ring has a
 * notch there, so the rest can always follow), then each neighbour of the locked run that comes into line with it */
static void lamp_lock(void)
{
    int i, j;
    for (i = 0; i < LP_RINGS - 1; i++) {
        int in_ = lp.lock >> i & 1u, out = lp.lock >> (i + 1) & 1u;
        if ((in_ && out) || !lamp_eq(i, i + 1) || (lp.pos[i] * LP_MUL[i]) % 12) continue;
        if (!lp.lock || in_ || out) {
            uint8_t was = lp.lock;
            lp.lock |= (uint8_t)(3u << i);
            for (j = 0; j < LP_RINGS; j++)
                if ((lp.lock & ~was) >> j & 1u) { lp.lock_t[j] = 0; sfx(67 + SC_PEN[j % 5], (uint32_t)j * 40u, 600, W_TRI, 3000, 3, 170); }
            sfx(96, 0, 80, W_NOISE, 900, 1, 90);
        }
    }
}
static void lamp_skip(void)                      /* every crack to the top, from wherever it is: the shorter way round */
{
    int i;
    for (i = 0; i < LP_RINGS; i++) { lp.off[i] = (int32_t)(int16_t)(uint16_t)(lp.pos[i] * 65536 / LP_N[i]); lp.pos[i] = 0; }
    lp.skip = 1; lp.skip_t = 0; lp.hint = 0;
    sfx(55, 0, 900, W_TRI, 3000, 8, 160); sfx(62, 120, 900, W_TRI, 3000, 8, 160);
}
/* 0: still at it; 1: the light is back; -1: back to the cave (HOME before the hint is offered, REC from the hint) */
static int lamp_tick(int32_t dt)
{
    int i;
    lp.t += dt; lp.idle += dt;
    if (!lp.done) lp_stuck += dt;
    /* HOME: acted on when it is let go (a hold may be on its way to a skip) */
    if (BDOWN(BTN_HOME)) { lp.home_on = 1; lp.home_t = 0; }
    if (lp.home_on && BHELD(BTN_HOME)) {
        lp.home_t += dt;
        if (lp_stuck >= LP_SKIP_MS && lp.home_t >= LP_SKIP_HOLD && !lp.done && !lp.skip) { lp.home_on = 0; lamp_skip(); }
    } else if (lp.home_on) {
        lp.home_on = 0;
        if (lp.home_t < 700 && !lp.done && !lp.skip) {
            if (lp.hint) { lp.hint = 0; snd_ui(67); }
            else if (lp_stuck >= LP_HINT_MS) { lp.hint = 1; lp.t = 0; snd_ui(76); }
            else return -1;
        }
    }
    if (lp.hint && BDOWN(BTN_REC)) return -1;
    lamp_turn(0, in.enc[ENC_KNOB1]); lamp_turn(1, in.enc[ENC_KNOB2]); lamp_turn(2, in.enc[ENC_KNOB3]); lamp_turn(3, in.enc[ENC_KNOB4]);
    if (!lp.skip && !lp.done) lamp_lock();
    for (i = 0; i < LP_RINGS; i++) { lp.flash[i] = imax(0, lp.flash[i] - dt); lp.nope[i] = imax(0, lp.nope[i] - dt); lp.lock_t[i] += dt; }
    lp.spin += (lp.done ? 9000 : 420) * dt / 1000;
    if (lp.skip && !lp.done) {
        int32_t was = lp.skip_t;
        lp.skip_t += dt;
        for (i = 0; i < LP_RINGS; i++) {         /* each ring clicks home as it arrives */
            int32_t at = (LP_RINGS - 1 - i) * LP_STAGGER + LP_GLIDE;
            if (was < at && lp.skip_t >= at) { lp.flash[i] = 200; sfx(72 + SC_PEN[(3 - i) % 5], 0, 500, W_TRI, 3000, 3, 180); }
        }
    }
    if (!lp.done) {
        int ready = !lp.skip || lp.skip_t >= (LP_RINGS - 1) * LP_STAGGER + LP_GLIDE;
        lp.hold = ready && (lamp_eq(0, 1) && lamp_eq(1, 2) && lamp_eq(2, 3)) ? lp.hold + dt : 0;
        if (lp.hold > 700) {
            static const uint8_t ARP[8] = {60, 64, 67, 72, 76, 79, 84, 88};
            lp.done = 1; lp.hint = 0;
            for (i = 0; i < 8; i++) sfx(ARP[i], (uint32_t)i * 90u, 2400, W_TRI, 3000, 5, 200);
            sfx(36, 0, 3000, W_SINE, 5000, 30, 0);
        }
        /* HOME breathes while it has something to give (the hint; on the hint page, the way back to the rings): lit
         * while it is held toward a skip */
        if (lp_stuck >= LP_HINT_MS && !lp.skip) {
            if (lp.home_on && lp_stuck >= LP_SKIP_MS) led_btn_want |= 1u << BTN_HOME;
            else led_breathe_want |= 1u << BTN_HOME;
        }
    } else {
        if (lp.done_t < LP_DONE_MS - JK_BLOOM_IN && lp.done_t + dt >= LP_DONE_MS - JK_BLOOM_IN) {   /* the swell */
            sfx(48, 0, 1400, W_SINE, 3600, 600, 120); sfx(55, 0, 1400, W_TRI, 2400, 600, 160);
        }
        lp.done_t += dt;
        led_key_want = ((1u << AC_NKEY) - 1u) & (0x1111111u << ((lp.done_t / 90) & 3));
        if (lp.done_t > LP_DONE_MS) return 1;
    }
    return 0;
}

/* the hint's picture: four small rings, put into line one at a time from the outside in (each dragging the one inside
 * it the other way), then the light out through them; again and again. Returns which line goes with it: 0 while the
 * outer ring is put straight, 1 after */
#define LP_HINT_LOOP 11000
static int lamp_hint_rings(int32_t cx, int32_t cy, uint32_t tm)
{
    static const int8_t P0[LP_RINGS] = {3, 7, 5, 9}, MOVE[LP_RINGS] = {-3, -4, 4, 3};   /* MOVE[s]: ring 3 - s, notches */
    static const uint8_t R[LP_RINGS] = {9, 15, 21, 27};
    static const int16_t AT[LP_RINGS + 1] = {1800, 4400, 6000, 7600, 9200};  /* when each step turns (900 ms), the light */
    int32_t ph = (int32_t)(tm % LP_HINT_LOOP), posq[LP_RINGS], s, i, k, cur = -1;
    for (i = 0; i < LP_RINGS; i++) posq[i] = P0[i] * 256;
    for (s = 0; s < LP_RINGS; s++) {
        int32_t r = LP_RINGS - 1 - s, f = ease8(iclamp((ph - AT[s]) * 256 / 900, 0, 256));
        posq[r] += MOVE[s] * f;
        if (r > 0) posq[r - 1] -= MOVE[s] * f;
        if (ph >= (s ? AT[s] - 500 : 0) && ph < AT[s + 1] - 500) cur = r;
    }
    if (ph >= AT[LP_RINGS]) {                    /* lined up: the light gets out */
        int32_t a0 = posq[3] * 65536 / 256 / LP_N[3] - 16384;
        cur = -1;
        for (k = -3; k <= 3; k++) {
            int32_t a = a0 + k * 500;
            galpha(150 + (tsin(tm, 30, k * 9000) >> 8));
            gline(cx + ((4 * icos(a)) >> 14), cy + ((4 * isin(a)) >> 14), cx + ((44 * icos(a)) >> 14), cy + ((44 * isin(a)) >> 14), th->caps[(k + 3 + (int32_t)(tm / 150u)) % 5]);
        }
        galpha(256);
    }
    for (i = 0; i < LP_RINGS; i++) {
        int32_t a = posq[i] * 65536 / 256 / LP_N[i] - 16384, gw = LP_GW[i] * 3 / 2;
        garc(cx, cy, R[i], R[i], a + gw, a + 65536 - gw, i == cur ? th->pad : th->caps[i + 1], i == cur);
    }
    galpha(90 + (tsin(tm, 30, 0) >> 9)); gdisc(cx, cy, 4, th->pad); galpha(256); gdisc(cx, cy, 2, th->pad);
    if (cur >= 0) {                              /* which knob is turning */
        char b[10] = "KNOB 0";
        b[5] = (char)('1' + cur);
        gtext(cx + 39, cy + 2, b, th->caps[cur + 1], ts(1, 2));
    }
    return ph >= AT[1] - 500;
}
static void lamp_hint_draw(void)
{
    int32_t sh = ts(2, 4), sf = ts(1, 2), a = ts(1, 2), y, top, room, h1, h2, foot, line;
    uint32_t tm = (uint32_t)lp.t;
    gclear(th->bg);
    /* the picture top left, the word and the knob turning beside it; under them how the rings drag, and the step the
     * picture is at */
    line = lamp_hint_rings(37, 37, tm);
    while (sh > 2 && gtextw(TR(T_HINT), sh) > 160) sh--;
    gtext(76, 10, TR(T_HINT), th->caps[0], sh);
    while (sf > 1 && (gtextw(TR(T_LAMP_TIP_BACK), sf) > 232 || gtextw(TR(T_REC_BACK), sf) > 232)) sf--;
    top = 72;
    foot = 238 - 2 * TLH(sf);
    room = foot - 2 - top;
    for (;;) {                                   /* the size asked for, smaller only if they would not fit */
        h1 = gblock_h(228, TR(T_LAMP_TIP1), a);
        h2 = imax(gblock_h(228, TR(T_LAMP_TIP2), a), gblock_h(228, TR(T_LAMP_TIP3), a));
        if (h1 + 6 + h2 <= room || a == 1) break;
        a--;
    }
    y = top + imax(0, (room - h1 - 6 - h2) / 2);
    y += gblock(6, y, 228, TR(T_LAMP_TIP1), th->text, a, 0) + 6;
    gblock(6, y, 228, TR(line ? T_LAMP_TIP3 : T_LAMP_TIP2), th->caps[line ? 2 : 4], a, 0);
    gfit(120, foot + 2 * sf, 232, TR(T_LAMP_TIP_BACK), th->pad, sf);
    gfit(120, foot + TLH(sf) + 2 * sf, 232, TR(T_REC_BACK), th->mute, sf);
}

/* the foot: "KNOB 1 2 3 4", each number in its ring's colour; a locked ring's number boxed, with a padlock over it.
 * y: the capitals' top (as gtext) */
static void lamp_knobs(int32_t y, int32_t a, uint32_t tm)
{
    static const char *const NUM[LP_RINGS] = {"1", "2", "3", "4"};
    int32_t gap = 9 * a, wk = gtextw("KNOB", a), wn = gtextw("4", a), w = wk + LP_RINGS * (gap + wn), x = 120 - w / 2, i;
    gtext(x, y, "KNOB", th->pad, a);
    x += wk;
    for (i = 0; i < LP_RINGS; i++) {
        int locked = lp.lock >> i & 1u;
        int32_t nx = x + gap + i * (gap + wn), bx = nx - a - 1, by = y - a - 1, bw = wn + 2 * a + 2, bh = 7 * a + 2;
        col_t c = th->caps[i + 1];
        if (locked) {
            int fl = lp.lock_t[i] < 600 && (lp.lock_t[i] / 100) & 1;
            grect(bx, by, bw, bh, fl ? th->pad : c);
            /* the padlock: a shackle over the box */
            garc(nx + wn / 2, by, 2 + a, 2 + a, 32768, 65536, fl ? th->pad : c, 1);
            gtext(nx, y, NUM[i], th->bg, a);
        } else {
            if (lp.nope[i] > 0 && (tm / 90u) & 1u) c = th->caps[0];
            gtext(nx, y, NUM[i], c, a);
        }
    }
}
/* the light filling the screen, f 0..256: rays and soft discs of it growing from (cx, cy), then everything gone to it.
 * The lamp ends in it and the dish begins in it, so the one becomes the other */
static void jk_bloom(int32_t cx, int32_t cy, int32_t f, uint32_t tm)
{
    int32_t e = ease8(f), r = 6 + 172 * e / 256, i;
    if (f <= 0) return;
    galpha(e);
    for (i = 0; i < 16; i++) {                   /* rays in the five colours, turning */
        int32_t a = i * 4096 + (int32_t)(tm * 24u), r0 = r / 3, r1 = r + 24 + (i & 1) * 18;
        gline2(cx + ((r0 * icos(a)) >> 14), cy + ((r0 * isin(a)) >> 14), cx + ((r1 * icos(a)) >> 14), cy + ((r1 * isin(a)) >> 14), th->caps[i % 5]);
    }
    galpha(e / 3); gdisc(cx, cy, r, th->text);
    galpha(e / 2); gdisc(cx, cy, r * 3 / 4, th->text);
    galpha(e); gdisc(cx, cy, r / 2, th->text);
    galpha(256);
    if (f > 150) gveil(th->text, (uint32_t)iclamp((f - 150) * 32 / 106, 0, 32));
}
static void lamp_draw(void)
{
    int32_t cx = 120, cy = 116, i, k, agree = (lamp_eq(0, 1) ? 1 : 0) + (lamp_eq(1, 2) ? 1 : 0) + (lamp_eq(2, 3) ? 1 : 0);
    uint32_t tm = (uint32_t)lp.t;
    if (lp.hint) { lamp_hint_draw(); return; }
    if (lp.skip && lp.skip_t < (LP_RINGS - 1) * LP_STAGGER + LP_GLIDE) agree = 0;
    gclear(th->bg);
    for (k = 0; k < 72; k++) {                   /* the light: each ray goes as far as the first glass that stops it */
        int32_t a = k * 65536 / 72, len = 120, out = 1;
        for (i = 0; i < LP_RINGS; i++) {
            int32_t d = (int32_t)(int16_t)(uint16_t)(a - lamp_ang(i));
            if (iabs(d) > LP_GW[i]) { len = LP_R[i] - 1; out = 0; break; }
        }
        galpha(out ? 230 : 46 + (i * 20));
        gline(cx + ((7 * icos(a)) >> 14), cy + ((7 * isin(a)) >> 14), cx + ((len * icos(a)) >> 14), cy + ((len * isin(a)) >> 14),
              out ? th->caps[(k + (int32_t)(tm / 120u)) % 5] : th->pad);
    }
    galpha(256);
    for (i = 0; i < LP_RINGS; i++) {
        int32_t a = lamp_ang(i);
        int locked = lp.lock >> i & 1u;
        col_t c = lp.flash[i] > 0 || (locked && lp.lock_t[i] < 600 && (lp.lock_t[i] / 100) & 1) || lp.nope[i] > 0 ? th->pad : th->caps[i + 1];
        garc(cx, cy, LP_R[i], LP_R[i], a + LP_GW[i], a + 65536 - LP_GW[i], c, lp.flash[i] > 0 || lp.done || locked);
        if (locked && !lp.done) garc(cx, cy, LP_R[i] - 2, LP_R[i] - 2, a + LP_GW[i], a + 65536 - LP_GW[i], c, 0);
        for (k = 0; k < LP_N[i]; k++) {          /* the notches */
            int32_t t = lp.spin - 16384 + k * 65536 / LP_N[i];
            gline(cx + (((LP_R[i] + 2) * icos(t)) >> 14), cy + (((LP_R[i] + 2) * isin(t)) >> 14), cx + (((LP_R[i] + 5) * icos(t)) >> 14), cy + (((LP_R[i] + 5) * isin(t)) >> 14), th->mute);
        }
        for (k = -1; k <= 1; k += 2)             /* the crack's two edges */
            gdisc(cx + ((LP_R[i] * icos(a + k * LP_GW[i])) >> 14), cy + ((LP_R[i] * isin(a + k * LP_GW[i])) >> 14), 1, th->pad);
    }
    galpha(90 + (tsin(tm, 30, 0) >> 9)); gdisc(cx, cy, 10, th->pad); galpha(256); gdisc(cx, cy, 5, th->pad);
    if (lp.done) {
        grect(0, 0, 240, 8 + TLH(ts(2, 4)), th->bg);
        gfit(120, 6 + 2 * ts(2, 4), 232, TR(T_LAMP_DONE), th->text, ts(2, 4));
        jk_bloom(cx, cy, (lp.done_t - (LP_DONE_MS - JK_BLOOM_IN)) * 256 / JK_BLOOM_IN, tm);   /* and on into the dish */
        return;
    }
    {   /* the words on cleared strips, top and foot: the rings run under them */
        int32_t a = ts(1, 2), at = ts(1, 3), h = gblock_h(232, TR(T_LAMP1), at), fy;
        if (lp.t < 9000 || a == 1) { grect(0, 0, 240, h + 4, th->bg); gblock(4, 2, 232, TR(T_LAMP1), th->text, at, 1); }
        else h = 0;                              /* (larger text: it steps aside once read, the rings need the room) */
        if (agree >= 1 && agree < 3 && !lp.lock) gfit(120, h + 4 + 2 * a, 232, TR(T_LAMP_TWO), th->caps[3], a);
        fy = gfoot(238 - TLH(a), TR(lp.idle > 20000 ? T_LAMP_HINT : T_LAMP2), lp.idle > 20000 ? th->caps[0] : th->mute, th->bg, a);
        grect(0, 238 - TLH(a), 240, TLH(a) + 2, th->bg);
        lamp_knobs(238 - TLH(a) + 2 * a, a, tm);
        if (lp_stuck >= LP_HINT_MS && !lp.skip) {   /* the note: a panel over the foot of the rings, popping up when it first comes */
            int skip = lp_stuck >= LP_SKIP_MS, id = skip ? T_LAMP_SKIP_POP : T_LAMP_HINT_POP;
            int32_t since = lp_stuck - (skip ? LP_SKIP_MS : LP_HINT_MS), pop = imin(256, since * 256 / 220);
            int32_t sc = ts(1, 3), w = 216, bh, bar = skip && lp.home_on ? 12 : 0, y;
            while (sc > 1 && gblock_h(w - 16, TR(id), sc) > 3 * TLH(sc)) sc--;
            bh = gblock_h(w - 16, TR(id), sc) + 12 + bar;
            y = fy - 6 - bh + (256 - ease8(pop)) * 20 / 256;
            galpha(pop);
            grect(120 - w / 2, y, w, bh, th->panel);
            gbox(120 - w / 2, y, w, bh, (ipulse(tm, 1200) > 128 || bar) ? th->caps[0] : th->pad);
            gblock(120 - w / 2 + 8, y + 6, w - 16, TR(id), th->text, sc, 1);
            if (bar) {                           /* holding: a bar fills toward the skip */
                gbox(120 - w / 2 + 12, y + bh - 12, w - 24, 7, th->mute);
                grect(120 - w / 2 + 14, y + bh - 10, (w - 28) * imin(lp.home_t, LP_SKIP_HOLD) / LP_SKIP_HOLD, 3, th->caps[0]);
            }
            galpha(256);
        }
    }
}

/* ---------------------------------------------------------------- the dish */
enum { DS_STIR, DS_LOCKED, DS_WAIT, DS_COUNT, DS_REC, DS_REVIEW };
static struct {
    int32_t w, a, still, off, ema, bpm, t, msg_t, stage_t, flung, lastk, last16, last4, steady;
    uint32_t start_step;
    int32_t sdir, slen, sms, gap;                /* a slide along the keys: which way, how many keys, how long; since the last key */
    int32_t fx_t;                                /* an effect's knob was just turned: its name shows */
    int32_t intro_t, demo_t, stir_t;             /* the welcome's time; the brushing hand's; since the stirring began */
    int32_t tip_a;                               /* the words under the heading: how much they show (0..256) */
    int16_t kage[AC_NKEY];                       /* ms since the drawn hand last went over each key */
    uint8_t stage, msg, n, few, fx_k, intro;     /* intro: the welcome is on (the first time, after the lamp) */
    uint8_t ev[TAKE_MAX][2];
} ds;
static struct { uint8_t again, n, bpm, fx[4], ev[TAKE_MAX][2]; } ds_old;   /* the song there was, should this one not be kept */
#define DS_SLIDE 8                               /* keys a hand must cross before its speed is believed */

static void dish_reset(int again)
{
    uint32_t i;
    ds_old.again = (uint8_t)again; ds_old.n = take_n; ds_old.bpm = take_bpm;
    for (i = 0; i < 4u; i++) ds_old.fx[i] = take_fx[i];
    for (i = 0; i < TAKE_MAX; i++) { ds_old.ev[i][0] = take_ev[i][0]; ds_old.ev[i][1] = take_ev[i][1]; }
    for (i = 0; i < sizeof ds; i++) ((uint8_t *)&ds)[i] = 0;
    ds.lastk = -1; ds.last16 = 0; ds.last4 = 0;  /* (at rest on its first beat: no "faster" before a hand has touched it) */
    ds.intro = (uint8_t)!again; ds.off = 5000;   /* (no hand on the keys yet: the drawn one shows at once) */
    ds.tip_a = 256;
    take_n = 0;
    mus_set(MUS_OFF, 0);
    sy_drone_want = 60;
}
static void dish_abort(void)                     /* left without keeping a song: the one before, if there was one, stays */
{
    uint32_t i;
    mus_set(MUS_OFF, 0);
    take_n = 0;
    if (!ds_old.again) return;
    take_bpm = ds_old.bpm;
    for (i = 0; i < 4u; i++) take_fx[i] = ds_old.fx[i];
    for (i = 0; i < TAKE_MAX; i++) { take_ev[i][0] = ds_old.ev[i][0]; take_ev[i][1] = ds_old.ev[i][1]; }
    take_n = ds_old.n;
}
static void dish_msg(int id) { ds.msg = (uint8_t)id; ds.msg_t = 1800; }

/* ---- the welcome: out of the lamp's light, two large cards over the dish, then the dish is yours. PLAY or a key skips */
#define JI_BLOOM 700                             /* the light draining away into the dish */
#define JI_C1 250                                /* "the jukebox is repaired" comes in ... */
#define JI_C1_OUT 2900                           /* ... and goes up and away */
#define JI_C2 3150                               /* "make a song to celebrate" */
#define JI_C2_OUT 5900
#define JI_END 6300
/* ---- the brushing hand: while the dish stands still and no hand is on the keys, a drawn hand brushes along a drawing
 * of the keys, left to right only: it comes up onto the keys at the left, brushes across, lifts away off the right
 * end, and comes again from the left. The keys it passes light (here and on the FM-1) */
#define JD_WAIT 900                              /* the keys left alone this long: the hand comes */
#define JD_IN 300                                /* one stroke: up onto the keys at the left ... */
#define JD_SWEEP 1500                            /* ... across them to the right (ends at this ms) ... */
#define JD_OUT 1850                              /* ... lifted away off the keys ... */
#define JD_PERIOD 2400                           /* ... and gone a moment, then the next */
#define JD_Y 206                                 /* the drawn keys: the top row's top */
static int32_t jk_demo_top(void) { return JD_Y - 4 - gblock_h(232, TR(T_JK_BRUSH), ts(2, 2)); }   /* the words over them */
static const col_t JK_ZONE[4] = {AC_RGB(255, 214, 40), AC_RGB(255, 255, 255), AC_RGB(40, 224, 96), AC_RGB(48, 112, 255)};   /* (as CRYSTAL ROAD's keys) */
static void tu_icon(const int8_t (*pc)[6], int32_t n, const int8_t (*ln)[4], int32_t m, int32_t ox, int32_t oy, int32_t s, int32_t a, int32_t al);   /* (ac_tutor.h) */
/* an open hand flat on the keys, the fingers up, round the middle fingertip: palm, four fingers, thumb; the lines part
 * the fingers */
static const int8_t JK_HAND[6][6] = {{0, 33, 12, 0, 46, 14}, {-9, 21, 4, -10, 4, 4}, {-2, 21, 4, -2, 0, 4}, {5, 21, 4, 6, 3, 4},
                                     {12, 24, 3, 13, 10, 3}, {-13, 36, 4, -22, 22, 4}};
static const int8_t JK_HAND_L[3][4] = {{-6, 4, -6, 19}, {2, 3, 2, 19}, {9, 8, 9, 21}};
static int32_t jk_key_x(int k)                   /* as CRYSTAL ROAD lays the keys out: the bottom row 15 px apart, the top between */
{
    int i, w = 0;
    for (i = 0; i < k; i++) if (!KEY_IS_SHARP[i]) w++;
    return KEY_IS_SHARP[k] ? w * 15 : w * 15 + 7;
}
/* where the hand is in its stroke at t: x (the middle fingertip), off (0 on the keys .. 256 gone below the screen) and
 * v, how fast it goes (Q14 of its fastest; always rightward) */
static void jk_demo_pose(int32_t t, int32_t *x, int32_t *off, int32_t *v)
{
    int32_t u;
    t %= JD_PERIOD;
    *v = 0;
    if (t < JD_IN) { *x = 4; *off = 256 - ease8(t * 256 / JD_IN); return; }
    if (t < JD_SWEEP) {
        u = (t - JD_IN) * 256 / (JD_SWEEP - JD_IN);
        *x = 4 + 232 * ease8(u) / 256; *off = 0; *v = isin(u * 128);   /* (slow at both ends, fastest in the middle) */
        return;
    }
    *x = 236;
    *off = t < JD_OUT ? ease8((t - JD_SWEEP) * 256 / (JD_OUT - JD_SWEEP)) : 256;
}
static void jk_demo_tick(int32_t dt)
{
    int k;
    int32_t hx, off, v;
    if (!ds.demo_t) for (k = 0; k < AC_NKEY; k++) ds.kage[k] = 2000;
    ds.demo_t += dt;
    jk_demo_pose(ds.demo_t, &hx, &off, &v);
    for (k = 0; k < AC_NKEY; k++) {
        if (off < 40 && iabs(jk_key_x(k) - hx) <= 12) ds.kage[k] = 0;   /* (under the fingers, while they touch) */
        else ds.kage[k] = (int16_t)imin(ds.kage[k] + dt, 2000);
        if (ds.kage[k] < 260) led_key_dim_want |= 1u << k;
    }
}
static void jk_demo_draw(uint32_t tm)
{
    int32_t hx, off, v, k, y0 = JD_Y, top = jk_demo_top();
    jk_demo_pose(ds.demo_t, &hx, &off, &v);
    grect(0, top - 2, 240, 240 - top + 2, th->bg);
    gblock(4, top, 232, TR(T_JK_BRUSH), ipulse(tm, 1600) > 128 ? th->pad : th->text, ts(2, 2), 1);
    for (k = 0; k < AC_NKEY; k++) {              /* the keys: lit as the hand goes over them, fading after */
        int32_t x = jk_key_x(k), sh = KEY_IS_SHARP[k], bx = sh ? x - 4 : x - 6, by = sh ? y0 : y0 + 11, w = sh ? 9 : 13, h = sh ? 9 : 14;
        int32_t age = ds.kage[k];
        col_t c = JK_ZONE[KEY_ZONE[k]];
        if (age < 120) grect(bx, by + 1, w, h, th->text);
        else if (age < 900) { galpha(256 - (age - 120) * 200 / 780); grect(bx, by, w, h, c); galpha(256); }
        else { galpha(110); gbox(bx, by, w, h, th->mute); galpha(256); }
    }
    for (k = 0; k < 3; k++) {                    /* the hand's wake */
        int32_t len = v * (16 + 6 * k) >> 14, x0 = hx - (15 + 2 * k), y = y0 + 2 + 9 * k;
        if (len < 3) continue;
        galpha(150 - 30 * k); gline(x0, y, x0 - len, y, th->text); galpha(256);
    }
    /* the hand: up from under the screen at the left, leaning back as it goes (the fingers trailing), down and away at
     * the right */
    if (off < 256) tu_icon(JK_HAND, 6, JK_HAND_L, 3, hx, y0 + 6 + off * 60 / 256, 1, -(v * 2800 >> 14), 256);
}

/* one line of the large words, centred on 120, its shadow under it. The word with JUKE in it (JUKEBOX, JUKEBOXEN,
 * JUKE-BOX: every language has one) in the five colours turning through it, its letters hopping one after another */
static void jk_line(const char *s, int32_t y, col_t c, int32_t a, uint32_t tm)
{
    char part[80];
    int32_t n = 0, ws = -1, we, i, pass, sh = imax(1, a / 2), x;
    while (s[n] && n < 79) n++;
    for (i = 0; i + 3 < n && ws < 0; i++) if (s[i] == 'J' && s[i + 1] == 'U' && s[i + 2] == 'K' && s[i + 3] == 'E') ws = i;
    if (ws < 0) { gtextc(120 + sh, y + sh, s, th->bg, a); gtextc(120, y, s, c, a); return; }
    for (we = ws; we < n && s[we] != ' '; we++) ;
    for (pass = 0; pass < 2; pass++) {           /* the shadow, then the words */
        int32_t o = pass ? 0 : sh;
        x = 120 - gtextw(s, a) / 2 + o;
        for (i = 0; i < ws; i++) part[i] = s[i];
        part[ws] = 0;
        x = gtext(x, y + o, part, pass ? c : th->bg, a);
        for (i = ws; i < we; i++) {              /* the word, a letter at a time */
            int32_t hop = isin((int32_t)((tm % 1100u) * 65536u / 1100u) - (i - ws) * 6500), k = (i - ws + (int32_t)(tm / 140u)) % 5;
            hop = hop > 0 ? (hop * (a + 2)) >> 14 : 0;
            part[0] = s[i]; part[1] = 0;
            x = gtext(x, y + o - hop, part, pass ? th->caps[k] : th->bg, a);
        }
        for (i = we; i < n; i++) part[i - we] = s[i];
        part[n - we] = 0;
        gtext(x, y + o, part, pass ? c : th->bg, a);
    }
}
/* a sentence large, in lines, centred on y: each line drops in (a little past, then back) from t_in, one after the
 * other, and goes up and away from t_out. col: -1 all th->text, else each line its own colour from th->caps */
static void jk_big(const char *s, int32_t y, int32_t t, int32_t t_in, int32_t t_out, int col, uint32_t tm)
{
    const char *ls[4]; int32_t ln[4], n = 0, a, i, w = 226, h;
    char buf[80];
    for (a = 6; a >= 2; a--) {                   /* the largest that fits in three lines, no word broken */
        const char *p = s;
        n = 0;
        while (*p && n <= 3) {
            const char *st, *q, *end = 0;
            while (*p == ' ') p++;
            st = q = p;
            for (;;) {
                const char *e = q;
                int32_t m;
                while (*e && *e != ' ') e++;
                m = imin((int32_t)(e - st), 79);
                for (i = 0; i < m; i++) buf[i] = st[i];
                buf[m] = 0;
                if (gtextw(buf, a) > w) break;
                end = e;
                if (!*e) break;
                q = e + 1;
            }
            if (!end) { n = 9; break; }          /* a word alone is too wide at a */
            if (n < 4) { ls[n] = st; ln[n] = (int32_t)(end - st); }
            n++;
            p = end;
        }
        if (n <= 3 && n * TLH(a) <= 132) break;
    }
    if (a < 2) a = 2;
    h = n * TLH(a);
    for (i = 0; i < n && i < 4; i++) {
        int32_t u = iclamp((t - t_in - i * 110) * 256 / 420, 0, 256), vv = u - 256, pos, o, yy, al;
        int32_t m = imin(ln[i], 79), j;
        col_t c = col < 0 ? th->text : th->caps[(col + i) % 5];
        if (!u) continue;
        pos = 256 + ((691 * vv / 256) * vv / 256) * vv / 256 + 435 * vv / 256 * vv / 256;   /* past, then back */
        o = ease8((t - t_out - i * 70) * 256 / 360);
        al = imin(256, u * 2) * (256 - o) / 256;
        if (al <= 0) continue;
        for (j = 0; j < m; j++) buf[j] = ls[i][j];
        buf[m] = 0;
        yy = y - h / 2 + i * TLH(a) + a - (256 - pos) * 30 / 256 - o * 44 / 256;
        if (col >= 0) yy += tsin(tm, 12, i * 20000) >> 13;   /* (the party's lines bob) */
        galpha(al);
        jk_line(buf, yy, c, a, tm);              /* (with a shadow: the dish is under the words) */
        galpha(256);
    }
}
static void jk_star(int32_t x, int32_t y, int32_t r, col_t c)   /* a four-pointed twinkle */
{
    gline(x - r, y, x + r, y, c); gline(x, y - r, x, y + r, c);
    if (r > 2) { gline(x - r / 2, y - r / 2, x + r / 2, y + r / 2, c); gline(x - r / 2, y + r / 2, x + r / 2, y - r / 2, c); }
}
static void jk_note(int32_t x, int32_t y, col_t c)   /* a quaver */
{
    gellf(x, y, 3, 2, c, 0); grect(x + 2, y - 9, 1, 9, c); gline(x + 3, y - 9, x + 6, y - 5, c);
}
/* Rock pops up from under the bottom right corner while the first card stands, grins (with his eyes: he has no mouth),
 * and drops away again */
#define JR_IN (JI_C1 + 900)                      /* (after the card's lines have landed) */
#define JR_GRIN (JR_IN + 380)
#define JR_OUT (JI_C1_OUT - 420)
#define JR_Z 1100                                /* his size, Q8: about four times the cave's Rock */
static void jk_rock_pt(int32_t x, int32_t y, int32_t c, int32_t sn, int32_t ux, int32_t uy, int32_t *px, int32_t *py)
{   /* a point on Rock (tenths of his units, as rock_draw counts them) turned by the angle whose cos and sin are c, sn */
    *px = x + ((((ux * c - uy * sn) / 10) >> 6) * JR_Z >> 16);
    *py = y + ((((ux * sn + uy * c) / 10) >> 6) * JR_Z >> 16);
}
static void jk_rock(int32_t t, uint32_t tm)
{
    int32_t u, pos, x, y, ang, c, sn, g, e, ex, ey;
    if (t < JR_IN || t >= JR_OUT + 300) return;
    if (t < JR_GRIN) {                           /* up, a little past, back */
        int32_t vv;
        u = iclamp((t - JR_IN) * 256 / (JR_GRIN - JR_IN), 0, 256); vv = u - 256;
        pos = 256 + ((691 * vv / 256) * vv / 256) * vv / 256 + 435 * vv / 256 * vv / 256;
    } else if (t < JR_OUT) pos = 256;
    else pos = 256 - ease8((t - JR_OUT) * 256 / 300);
    x = 194 + (256 - pos) * 60 / 256; y = 204 + (256 - pos) * 90 / 256;
    ang = -5200 + (tsin(tm, 18, 0) >> 6);         /* leaning in from the corner, rocking a little */
    if (t >= JR_GRIN && t < JR_OUT) y += (isin((int32_t)((tm % 520u) * 65536u / 520u)) * 2) >> 14;   /* (chuckling) */
    g = t < JR_GRIN ? 0 : ease8((t - JR_GRIN) * 256 / 220);   /* the grin, coming */
    prock_draw(x, y, JR_Z, ang, g > 128 ? 0 : 256);
    c = icos(ang); sn = isin(ang);
    if (g > 128)                                 /* eyes shut happy: two arches */
        for (e = -1; e <= 1; e += 2) {
            jk_rock_pt(x, y, c, sn, e * 34, -8, &ex, &ey);
            garc(ex, ey, 9, 7, 32768 + 2500, 65536 - 2500, th->eye, 1);   /* (in the eyes' own colour: the body is dark) */
            garc(ex, ey + 1, 9, 7, 32768 + 2500, 65536 - 2500, th->eye, 1);
            garc(ex, ey + 2, 9, 7, 32768 + 4000, 65536 - 4000, th->eye, 1);
        }
    /* no mouth (Rock has none): the grin is in the eyes, shut happy, and a blush on each cheek */
    if (g > 128)
        for (e = -1; e <= 1; e += 2) {
            int32_t j;
            jk_rock_pt(x, y, c, sn, e * 44, 22, &ex, &ey);
            galpha((g - 128) * 2);
            for (j = -1; j <= 1; j++) gline(ex + 3 * j - 1, ey + 2, ex + 3 * j + 1, ey - 2, th->caps[0]);   /* a blush: three little strokes */
            galpha(256);
        }
    if (g > 200 && t - JR_GRIN < 900) {          /* a twinkle beside him as the grin comes */
        int32_t gl = t - JR_GRIN - 180, r;
        jk_rock_pt(x, y, c, sn, -96, -70, &ex, &ey);
        r = gl < 0 ? 0 : 6 - iabs(gl - 300) / 50;
        if (r > 0) { jk_star(ex, ey, r, th->text); gdisc(ex, ey, 1, th->text); }
    }
}
static void jk_intro_tick(int32_t dt)
{
    int32_t was = ds.intro_t, t, k;
    ds.intro_t += dt;
    t = ds.intro_t;
    if (was < JI_C1 && t >= JI_C1) {             /* mended: a chord, strummed */
        static const uint8_t CH[5] = {48, 60, 64, 67, 72};
        for (k = 0; k < 5; k++) sfx(CH[k], (uint32_t)k * 45u, 2200, W_TRI, 3000, 6, 200);
    }
    if (was < JI_C2 && t >= JI_C2) {             /* the party: a run up, and a bright one on top */
        static const uint8_t RUN[7] = {67, 69, 72, 74, 76, 79, 84};
        for (k = 0; k < 7; k++) sfx(RUN[k], (uint32_t)k * 65u, 700, W_TRI, 2600, 4, 180);
        sfx(88, 7u * 65u, 1600, W_SINE, 2000, 8, 220);
    }
    if (was < JR_IN && t >= JR_IN) { sfx(55, 0, 90, W_SINE, 4200, 1, 0); sfx(67, 70, 140, W_TRI, 3000, 2, 60); }   /* Rock: pop! */
    if (was < JR_GRIN && t >= JR_GRIN) { sfx(79, 0, 160, W_TRI, 2200, 2, 120); sfx(84, 110, 160, W_TRI, 2200, 2, 120); sfx(103, 300, 700, W_SINE, 1500, 2, 220); }   /* a chuckle, the twinkle */
    if (was < JR_OUT && t >= JR_OUT) sfx(60, 0, 120, W_SINE, 3200, 2, 0);   /* and away */
    led_play_breathe = 1;                        /* (PLAY skips) */
    if (t < JI_C2) led_key_want = ((1u << AC_NKEY) - 1u) & (0x1111111u << ((t / 90) & 3));   /* the lamp's chase, carried on */
    else for (k = 0; k < AC_NKEY; k++) if (ihash((uint32_t)k * 31u + (uint32_t)t / 110u) < 9000u) led_key_want |= 1u << k;   /* twinkling */
}
static void jk_intro_draw(uint32_t tm)
{
    int32_t t = ds.intro_t, cx = 120, cy = 122, k, dim = iclamp(t * 256 / JI_BLOOM, 0, 256) * iclamp((JI_END - t) * 256 / 400, 0, 256) / 256;
    gveil(th->bg, (uint32_t)(dim * 18 / 256));   /* the dish, quieter under the words */
    if (t >= JI_C1 && t < JI_C2) for (k = 0; k < 14; k++) {   /* twinkles round the first card */
        int32_t x = 12 + (int32_t)(ihash((uint32_t)k + 70u) * 216u >> 16), y = 30 + (int32_t)(ihash((uint32_t)k + 170u) * 170u >> 16);
        int32_t p = ipulse(tm + ihash((uint32_t)k) , 900 + (uint32_t)k * 37u);
        if (p < 120) continue;
        galpha(p); jk_star(x, y, 1 + p / 80, th->caps[k % 5]); galpha(256);
    }
    if (t >= JI_C2 - 200) for (k = 0; k < 10; k++) {   /* notes rising off the dish for the second */
        int32_t life = 1700, u = (t - JI_C2 + 200 + k * 230) % life, x = 26 + (int32_t)(ihash((uint32_t)k + 300u) * 188u >> 16);
        x += tsin(tm, 8, k * 7000) >> 12;
        galpha(imin(256, (life - u) * 256 / 500)); jk_note(x, cy + 40 - u * 140 / life, th->caps[k % 5]); galpha(256);
    }
    jk_big(TR(T_JK_FIXED), cy - 22, t, JI_C1, JI_C1_OUT, -1, tm);
    jk_rock(t, tm);
    jk_big(TR(T_JK_PARTY), cy - 22, t, JI_C2, JI_C2_OUT, 1, tm);
    jk_bloom(cx, cy, 256 - t * 256 / JI_BLOOM, tm);   /* the lamp's light, going back into the dish */
}

/* 0: still at it; 1: the song is kept (the power comes back); -1: HOME while stirring: away; 2: HOME after that: the
 * effects' page (arcave.c opens it; REC from there leaves) */
static int dish_tick(int32_t dt)
{
    int32_t k;
    ds.t += dt; ds.stage_t += dt;
    if (ds.msg_t > 0) ds.msg_t -= dt;
    if (ds.fx_t > 0) ds.fx_t -= dt;
    if (BDOWN(BTN_HOME)) {
        if (ds.stage != DS_STIR) return 2;
        dish_abort(); return -1;
    }
    if (ds.intro) {                              /* the welcome: PLAY, or a key (which then counts), and the dish is yours */
        jk_intro_tick(dt);
        if (ds.intro_t < JI_END && !BDOWN(BTN_PLAY) && !(in.kdown && ds.intro_t > 400)) return 0;
        ds.intro = 0;
    }
    if (ds.stage == DS_STIR) {
        int32_t s16, s4, hand = -1;
        for (k = AC_NKEY - 1; k >= 0; k--) if (in.keys >> k & 1u) { hand = k; break; }
        if (in.hikey >= 0) hand = in.hikey;
        ds.gap += dt;
        if (hand >= 0) {
            if (ds.lastk >= 0 && hand != ds.lastk) {
                int32_t dk = hand - ds.lastk, dir = dk > 0 ? 1 : -1;
                if (dir != ds.sdir || ds.gap > 260) { ds.sdir = dir; ds.slen = 0; ds.sms = 0; }   /* a new slide starts here */
                else { ds.slen += iabs(dk); ds.sms += ds.gap; }
                ds.gap = 0; ds.still = 0;
                if (ds.slen >= DS_SLIDE && ds.sms > 0)   /* far enough to tell how fast the hand goes: the dish takes that speed */
                    ds.w += (dir * imin(1331, ds.slen * 20000 / ds.sms) - ds.w) / 2;
            } else ds.still += dt;
            ds.lastk = hand; ds.off = 0;
        } else { ds.off += dt; if (ds.off > 400) { ds.lastk = -1; ds.sdir = 0; } ds.still = 0; }
        if (in.any_knob) ds.w = iclamp(ds.w + in.any_knob * 60, -1331, 1331);
        ds.w -= ds.w * dt * (ds.still > 300 ? 11 : 1) / (ds.still > 300 ? 10000 : 50000);   /* a resting finger brakes it */
        ds.a += ds.w * dt / 1000;                /* rad Q8 */
        ds.bpm = iabs(ds.w) * 382 / 2560;        /* |w| / tau * 4 * 60 */
        s16 = iabs(ds.a) * 16 / 1608; s4 = iabs(ds.a) * 4 / 1608;
        if (s16 != ds.last16) {                  /* the dish is the clock: every sixteenth it passes sounds */
            static const char BASS[] = "o..o..o.o..o..q.";
            ds.last16 = s16;
            if (!(s16 & 1)) sfx(0, 0, 40, W_HAT, 900, 1, 0);
            if (BASS[s16 & 15] == 'o') sfx(36, 0, 380, W_SINE, 5000, 5, 0);
            else if (BASS[s16 & 15] == 'q') sfx(43, 0, 280, W_SINE, 4000, 5, 0);
        }
        if (s4 != ds.last4) {
            ds.last4 = s4;
            if (ds.bpm > 55 && ds.bpm < 165 && iabs(ds.bpm - ds.ema) < 7) {
                ds.steady++;
                sfx(72 + SC_PEN[ds.steady % 5], 0, 400, W_TRI, 2600, 4, 150);
            } else {
                if (ds.steady > 1 || ds.msg_t <= 0) dish_msg(ds.bpm >= 165 ? T_DISH_FAST : ds.bpm <= 55 ? T_DISH_SLOW : T_DISH_HOLD);
                ds.steady = 0;
            }
            ds.ema = (ds.ema + ds.bpm) / 2;
            if (ds.steady >= 8) {
                ds.stage = DS_LOCKED; ds.stage_t = 0;
                take_bpm = (uint8_t)iclamp(ds.ema, 56, 164);
                sfx(48, 0, 2600, W_TRI, 3000, 10, 160); sfx(55, 60, 2600, W_TRI, 3000, 10, 160); sfx(64, 120, 2600, W_TRI, 3000, 10, 160); sfx(72, 180, 2600, W_TRI, 3000, 10, 160);
                mus_set(MUS_JUKE, 0);
                sy_drone_want = 0;
            }
        }
        if (ds.bpm > 172 && ds.t - ds.flung > 3000) { ds.flung = ds.t; ds.w = ds.w * 55 / 100; ds.steady = 0; snd_whoosh(); dish_msg(T_DISH_FAST); }
        if (hand >= 0) led_key_want |= 1u << hand;
        led_key_dim_want |= 1u << ((iabs(ds.a) * AC_NKEY / 1608) % AC_NKEY);
        ds.stir_t += dt;
        if (ds.bpm <= 20 && ds.off > JD_WAIT) jk_demo_tick(dt); else ds.demo_t = 0;
        {   /* the words under the heading: there while nothing moves; a hand on the keys fades them (and back once it lets
             * the dish stand again) */
            int32_t want = ds.bpm <= 20 && ds.off > 250 && ds.msg_t <= 0 ? 256 : 0;
            ds.tip_a = want > ds.tip_a ? imin(want, ds.tip_a + dt * 256 / 300) : imax(want, ds.tip_a - dt * 256 / 220);
        }
    } else {
        uint32_t now = tr_now_q8(), step = (now >> 8) / TICKS_STEP;
        ds.a = (int32_t)(((now >> 4) % (TICKS_BAR * 16u)) * 1608u / (TICKS_BAR * 16u));   /* the dish turns with the clock now: a bar a turn */
        if (ds.stage == DS_LOCKED && ds.stage_t > 2200) { ds.stage = DS_WAIT; ds.stage_t = 0; }
        if ((ds.stage == DS_WAIT || ds.stage == DS_REVIEW) && BDOWN(BTN_REC)) {
            take_n = 0; ds.n = 0; ds.few = 0;
            ds.start_step = ((step + 16u + 31u) / 32u) * 32u;
            ds.stage = DS_COUNT; ds.stage_t = 0;
            snd_ui(84);
        } else if (ds.stage == DS_COUNT) {
            if (step >= ds.start_step) { ds.stage = DS_REC; ds.stage_t = 0; }
            else {
                static uint32_t last_beat;
                uint32_t beat = (now >> 8) / TICKS_BEAT;
                if (beat != last_beat && ds.start_step - step <= 16u) sfx(84, 0, 80, W_SQUARE, 1800, 1, 0);
                last_beat = beat;
            }
        }
        if (ds.stage == DS_REC) {
            if (step >= ds.start_step + 32u) {
                if (ds.n < 3u) { ds.stage = DS_WAIT; ds.few = 1; }
                else { for (k = 0; k < ds.n; k++) { take_ev[k][0] = ds.ev[k][0]; take_ev[k][1] = ds.ev[k][1]; } take_n = ds.n; ds.stage = DS_REVIEW; }
                ds.stage_t = 0;
            } else
                for (k = 0; k < AC_NKEY; k++)
                    if ((in.kdown >> k & 1u) && ds.n < TAKE_MAX) {
                        uint32_t s = (((now >> 8) + TICKS_STEP / 2u) / TICKS_STEP - ds.start_step) % 32u;
                        ds.ev[ds.n][0] = (uint8_t)s; ds.ev[ds.n][1] = (uint8_t)k; ds.n++;
                    }
            led_btn_want |= 1u << BTN_REC;
        }
        for (k = 0; k < 4; k++) {                /* the four knobs: the song's effects */
            int32_t d = in.enc[ENC_KNOB1 + k], v = iclamp((int32_t)take_fx[k] + d, 0, TAKE_FX_MAX);
            if (!d) continue;
            ds.fx_k = (uint8_t)k; ds.fx_t = 1600;
            if (v != take_fx[k]) { take_fx[k] = (uint8_t)v; take_voice(72 + SC_PEN[v % 5] + 12 * (v / 5), 0); }   /* and how it sounds now */
        }
        for (k = 0; k < AC_NKEY; k++) if (in.kdown >> k & 1u) take_voice(key_note(k) + 12, 0);
        if (ds.stage == DS_WAIT && ipulse((uint32_t)ds.t, 900) > 128) led_btn_want |= 1u << BTN_REC;
        if (ds.stage == DS_REVIEW) {
            led_play_breathe = 1;
            if (BDOWN(BTN_PLAY)) return 1;
        }
        led_btn_want |= (1u << BTN_FX) << ((step / 4u) % 4u);   /* four buttons count the bar */
        led_key_want |= in.keys;
    }
    return 0;
}
static void dish_draw(void)
{
    int32_t cx = 120, cy = 122, i, a16 = (ds.a * 10430) >> 8, k;
    uint32_t tm = (uint32_t)ds.t;
    gclear(th->bg);
    /* the dish: light on a tilted plate, dust riding it */
    for (i = 4; i >= 0; i--) {
        galpha(40 + i * 14);
        gellf(cx, cy, 30 + i * 13, (30 + i * 13) * 55 / 100, th->bands[(i + (int32_t)(tm / 700u)) % 5], 0);
    }
    galpha(256);
    garc(cx, cy, 84, 46, 0, 65536, th->rim, 0);
    for (i = 0; i < 70; i++) {
        int32_t rr = 20 + (int32_t)(ihash((uint32_t)i + 40u) * 62u >> 16), a = a16 * (90 - rr / 2) / 60 + (int32_t)ihash((uint32_t)i);
        galpha(120 + (int32_t)(ihash((uint32_t)i + 9u) >> 9));
        gp(cx + ((rr * icos(a)) >> 14), cy + ((rr * 55 / 100 * isin(a)) >> 14), th->caps[i % 5]);
    }
    galpha(256);
    gellf(cx, cy, 13, 8, th->bg, 0); garc(cx, cy, 13, 8, 0, 65536, th->pad, 0);
    for (i = 0; i < 16; i++) {                   /* sixteen marks round it, a bright one every beat */
        int32_t a = i * 4096 - 16384, x = cx + ((98 * icos(a)) >> 14), y = cy + ((54 * isin(a)) >> 14);
        if (i & 3) gp(x, y, th->mute); else grect(x - 1, y - 1, 3, 3, th->text);
    }
    {   /* where the dish is in its turn */
        int32_t a = a16 - 16384, x = cx + ((98 * icos(a)) >> 14), y = cy + ((54 * isin(a)) >> 14);
        galpha(90); gdisc(x, y, 7, th->pad); galpha(256); gdisc(x, y, 3, th->pad);
    }
    if (ds.stage >= DS_REC || ds.stage == DS_WAIT) { /* the notes played, as stars: when by the angle, how high by the distance */
        int32_t n = ds.stage == DS_REVIEW ? take_n : ds.n;
        uint32_t s32 = ((tr_now_q8() >> 8) / TICKS_STEP) % 32u;
        for (k = 0; k < n; k++) {
            int32_t st = ds.ev[k][0], a = st * 2048 * 2 - 16384, rr = 24 + ds.ev[k][1] * 2;
            int32_t x = cx + ((rr * icos(a)) >> 14), y = cy + ((rr * 55 / 100 * isin(a)) >> 14);
            int hit = (uint32_t)st == s32 && ds.stage == DS_REVIEW;
            galpha(80); gdisc(x, y, hit ? 6 : 3, th->caps[ds.ev[k][1] % 5]); galpha(256);
            grect(x - 1, y - 1, hit ? 3 : 2, hit ? 3 : 2, th->text);
        }
    }
    if (ds.intro) { jk_intro_draw(tm); return; }
    if (ds.stage != DS_STIR) {                   /* the four effects: a column of pips each, beside the dish */
        for (k = 0; k < 4; k++) {
            int32_t x = k < 2 ? 3 : 227, y0 = (k & 1) ? 128 : 76, j;
            char nb[2] = {(char)('1' + k), 0};
            col_t c = th->caps[(k + 1) % 5];
            gtext(x + 2, y0, nb, ds.fx_t > 0 && ds.fx_k == k ? th->text : c, 1);
            for (j = 0; j < TAKE_FX_MAX; j++) {
                galpha(j < take_fx[k] ? 256 : 70);
                grect(x, y0 + 9 + (TAKE_FX_MAX - 1 - j) * 5, 10, 4, j < take_fx[k] ? c : th->mute);
            }
            galpha(256);
        }
        if (ds.fx_t > 0) {                       /* the one just turned: its name, and how much */
            char b[40]; const char *p = TR(T_FX_NAME1 + ds.fx_k); int32_t n = 0;
            while (*p && n < 34) b[n++] = *p++;
            b[n++] = ' '; b[n++] = (char)('0' + take_fx[ds.fx_k]); b[n] = 0;
            galpha(imin(256, ds.fx_t));
            grect(30, cy - 6 - 3 * ts(2, 3), 180, 5 * ts(2, 3) + 12, th->bg);
            gfit(120, cy - 3 * ts(2, 3), 176, b, th->caps[(ds.fx_k + 1) % 5], ts(2, 3));
            galpha(256);
        }
    }
    {   /* the words: a heading at the top, what to do at the foot */
        int32_t st = ts(2, 4), a = ts(1, 3), top = 6 + 2 * st, used, hint = imax(1, ts(1, 2) - 1), fy;
        if (ds.stage == DS_STIR) {
            int32_t in_ = imin(256, ds.stir_t * 256 / 400);   /* (out of the welcome: the words come up) */
            galpha(in_);
            used = gfit(120, top, 232, TR(T_DISH1), th->text, st);
            galpha(256);
            if (ds.tip_a > 0) {                  /* on a dark glass, so the dish's turning dot never runs through a letter */
                int32_t ty = top + 5 * used + 4, th_ = gblock_h(232, TR(T_DISH2), a), bw = th_ > TLH(a) ? 236 : gtextw(TR(T_DISH2), a) + 14;
                int32_t al = in_ * ds.tip_a / 256;
                galpha(al * 228 / 256);                 /* (dark, the dish just showing through) */
                grect(120 - bw / 2 + 1, ty - 2, bw - 2, th_ + 3, th->bg); grect(120 - bw / 2, ty - 1, bw, th_ + 1, th->bg);
                galpha(al);
                gblock(4, ty, 232, TR(T_DISH2), th->mute, a, 1);
                galpha(256);
            }
            if (ds.bpm <= 20) {
                fy = 238;
                if (ds.demo_t > 0) { jk_demo_draw(tm); fy = jk_demo_top() - 2; }
            } else {                             /* spinning: its BPM large on the dish, eight pips for the steady beats, and
                                                    at the foot: let go and it keeps that speed */
                char b[4]; int32_t v = imin(ds.bpm, 999), n = 0, ok = ds.bpm > 55 && ds.bpm < 165;
                if (v >= 100) b[n++] = (char)('0' + v / 100);
                b[n++] = (char)('0' + v / 10 % 10); b[n++] = (char)('0' + v % 10); b[n] = 0;
                galpha(200); gellf(cx, cy + 4, 64, 40, th->bg, 0); galpha(256);
                gtextc(120, cy - 22, b, ok ? th->text : th->mute, 6);
                gtextc(120, cy + 13, "BPM", ok ? th->pad : th->mute, 2);
                for (k = 0; k < 8; k++) {
                    int32_t x = 120 - 49 + k * 14, on = k < ds.steady;
                    if (on) gdisc(x, cy + 32, 4, th->pad); else gcirc(x, cy + 32, 4, th->mute);
                }
                fy = gfoot(238, TR(T_DISH_LET), th->pad, th->bg, a);
            }
            if (ds.msg_t > 0) gfoot(fy - 2, TR(ds.msg), th->caps[0], th->bg, ts(2, 4));
        } else if (ds.stage == DS_LOCKED) {
            gblock(4, 6, 232, TR(T_DISH_LOCK), th->text, st, 1);
        } else if (ds.stage == DS_WAIT) {
            gfit(120, top, 232, TR(T_TAKE1), th->text, st);
            fy = gfoot(238, TR(ds.few ? T_TAKE_FEW : T_TAKE_REC), ds.few ? th->caps[0] : th->pad, th->bg, a);
            gfoot(fy - 2, TR(T_FX_HINT), th->mute, th->bg, hint);
        } else if (ds.stage == DS_COUNT) {
            uint32_t step = (tr_now_q8() >> 8) / TICKS_STEP;
            gfit(120, top, 232, TR(T_TAKE_READY), th->text, st);
            if (ds.start_step > step && ds.start_step - step <= 16u) {
                char num[2] = {(char)('0' + (ds.start_step - step + 3u) / 4u), 0};
                gtextc(120, cy - 12, num, th->pad, 5);
            }
        } else if (ds.stage == DS_REC) {
            used = gfit(128, top, 208, TR(T_TAKE_RECING), HEXC(0xff453a), st);
            gdisc(128 - gtextw(TR(T_TAKE_RECING), used) / 2 - 4 - 3 * used, top + 5 * used / 2, 2 * used, HEXC(0xff453a));
        } else {
            gfit(120, top, 232, TR(T_YOURSONG), th->text, st);
            fy = gfoot(238, TR(T_TAKE_KEEP), th->pad, th->bg, a);
            gfoot(fy - 2, TR(T_FX_HINT), th->mute, th->bg, hint);
        }
    }
}
