/* SPDX-License-Identifier: GPL-3.0-only */
/* Mending the jukebox, in two parts, both after UMBRA OBSCURA:
 *   the lamp   four rings of glass, each with a crack. KNOB 1, 2, 3 and 4 turn a ring each, a notch a click, and each
 *              outer ring drags the one inside it the other way. Light gets out only where the cracks line up.
 *   the dish   a flywheel. Slide a hand a long way along the keys (or turn a knob) to spin it: how fast the hand went
 *              is how fast it turns, and its speed is the tempo, four beats to the turn. Hold a speed for eight beats
 *              and it is yours. Then KNOB 1-4 are the song's effects (HOME tells what each does), and REC, and two
 *              bars of keys are your song. Once the power is back, PLAY at the jukebox does the dish again: a new song. */

/* ---------------------------------------------------------------- the lamp */
#define LP_RINGS 4                               /* one to a numbered knob */
static const uint8_t LP_N[LP_RINGS] = {8, 12, 16, 24}, LP_R[LP_RINGS] = {28, 46, 64, 82}, LP_MUL[LP_RINGS] = {6, 4, 3, 2};
static const int16_t LP_GW[LP_RINGS] = {3400, 2700, 2100, 1700};   /* a crack's half width, angle units */
static struct { int32_t pos[LP_RINGS], hold, spin, flash[LP_RINGS], t, done_t, idle; uint8_t done; } lp;

static void lamp_reset(void)
{
    lp.pos[0] = 3; lp.pos[1] = 7; lp.pos[2] = 5; lp.pos[3] = 9;
    lp.hold = 0; lp.spin = 0; lp.t = 0; lp.done = 0; lp.done_t = 0; lp.idle = 0;
    lp.flash[0] = lp.flash[1] = lp.flash[2] = lp.flash[3] = 0;
    sy_drone_want = 110;
}
static int32_t lamp_ang(int i) { return lp.pos[i] * 65536 / LP_N[i] - 16384 + lp.spin; }
static int lamp_eq(int i, int j) { return lp.pos[i] * LP_MUL[i] == lp.pos[j] * LP_MUL[j]; }
static void lamp_turn(int n, int32_t d)
{
    static const uint8_t DEG[LP_RINGS] = {0, 2, 4, 6};
    if (lp.done || !d) return;
    if (d > 1) { lamp_turn(n, d - 1); d = 1; }   /* a quick turn: every notch counts */
    if (d < -1) { lamp_turn(n, d + 1); d = -1; }
    lp.pos[n] = ((lp.pos[n] + d) % LP_N[n] + LP_N[n]) % LP_N[n];
    lp.flash[n] = 200;
    if (n > 0) { lp.pos[n - 1] = ((lp.pos[n - 1] - d) % LP_N[n - 1] + LP_N[n - 1]) % LP_N[n - 1]; lp.flash[n - 1] = 120; }
    sfx_ex(60 + SC_PEN[(DEG[n] + lp.pos[n] % 3) % 5] + 12 * ((DEG[n] + lp.pos[n] % 3) / 5), 0, 320, W_TRI, 3200, 3, 150, 0, (2 * n - 3) * 25, 0);
    lp.idle = 0;
}
/* 0: still at it; 1: the light is back; -1: HOME, back to the cave */
static int lamp_tick(int32_t dt)
{
    int i;
    lp.t += dt; lp.idle += dt;
    if (BDOWN(BTN_HOME) && !lp.done) return -1;
    lamp_turn(0, in.enc[ENC_KNOB1]); lamp_turn(1, in.enc[ENC_KNOB2]); lamp_turn(2, in.enc[ENC_KNOB3]); lamp_turn(3, in.enc[ENC_KNOB4]);
    for (i = 0; i < LP_RINGS; i++) lp.flash[i] = imax(0, lp.flash[i] - dt);
    lp.spin += (lp.done ? 9000 : 420) * dt / 1000;
    if (!lp.done) {
        lp.hold = (lamp_eq(0, 1) && lamp_eq(1, 2) && lamp_eq(2, 3)) ? lp.hold + dt : 0;
        if (lp.hold > 700) {
            static const uint8_t ARP[8] = {60, 64, 67, 72, 76, 79, 84, 88};
            lp.done = 1;
            for (i = 0; i < 8; i++) sfx(ARP[i], (uint32_t)i * 90u, 2400, W_TRI, 3000, 5, 200);
            sfx(36, 0, 3000, W_SINE, 5000, 30, 0);
        }
    } else {
        lp.done_t += dt;
        led_key_want = ((1u << AC_NKEY) - 1u) & (0x1111111u << ((lp.done_t / 90) & 3));
        if (lp.done_t > 3200) return 1;
    }
    return 0;
}
static void lamp_draw(void)
{
    int32_t cx = 120, cy = 116, i, k, agree = (lamp_eq(0, 1) ? 1 : 0) + (lamp_eq(1, 2) ? 1 : 0) + (lamp_eq(2, 3) ? 1 : 0);
    uint32_t tm = (uint32_t)lp.t;
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
        col_t c = lp.flash[i] > 0 ? th->pad : th->caps[i + 1];
        garc(cx, cy, LP_R[i], LP_R[i], a + LP_GW[i], a + 65536 - LP_GW[i], c, lp.flash[i] > 0 || lp.done);
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
    } else {                                     /* the words on cleared strips, top and foot: the rings run under them */
        int32_t a = ts(1, 2), at = ts(1, 3), h = gblock_h(232, TR(T_LAMP1), at), y;
        if (lp.t < 9000 || a == 1) { grect(0, 0, 240, h + 4, th->bg); gblock(4, 2, 232, TR(T_LAMP1), th->text, at, 1); }
        else h = 0;                              /* (larger text: it steps aside once read, the rings need the room) */
        if (agree >= 1 && agree < 3) gfit(120, h + 4 + 2 * a, 232, TR(T_LAMP_TWO), th->caps[3], a);
        y = gfoot(238 - TLH(a), TR(lp.idle > 20000 ? T_LAMP_HINT : T_LAMP2), lp.idle > 20000 ? th->caps[0] : th->mute, th->bg, a);
        grect(0, 238 - TLH(a), 240, TLH(a) + 2, th->bg);
        gfit(120, 238 - TLH(a) + 2 * a, 232, TR(T_KNOBS123), th->pad, a);
        (void)y;
    }
}

/* ---------------------------------------------------------------- the dish */
enum { DS_STIR, DS_LOCKED, DS_WAIT, DS_COUNT, DS_REC, DS_REVIEW };
static struct {
    int32_t w, a, still, off, ema, bpm, t, msg_t, stage_t, flung, lastk, last16, last4, steady;
    uint32_t start_step;
    int32_t sdir, slen, sms, gap;                /* a slide along the keys: which way, how many keys, how long; since the last key */
    int32_t fx_t;                                /* an effect's knob was just turned: its name shows */
    uint8_t stage, msg, n, few, fx_k;
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
    ds.lastk = -1; ds.last16 = -1; ds.last4 = -1;
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
            used = gfit(120, top, 232, TR(T_DISH1), th->text, st);
            if (ds.steady == 0 && ds.msg_t <= 0) gblock(4, top + 5 * used + 4, 232, TR(T_DISH2), th->mute, a, 1);
            if (ds.steady > 0) {
                char num[2] = {(char)('0' + ds.steady), 0};
                gtextc(120, cy - 12, num, th->text, 5);
            }
            if (ds.bpm > 20) {
                char b[12]; int32_t v = ds.bpm, n = 0;
                if (v >= 100) b[n++] = (char)('0' + v / 100);
                b[n++] = (char)('0' + v / 10 % 10); b[n++] = (char)('0' + v % 10); b[n++] = ' '; b[n++] = 'B'; b[n++] = 'P'; b[n++] = 'M'; b[n] = 0;
                gfit(120, 238 - TLH(a) + 2 * a, 232, b, (ds.bpm > 55 && ds.bpm < 165) ? th->pad : th->mute, a);
            }
            if (ds.msg_t > 0) gfoot(238 - TLH(a) - 2, TR(ds.msg), th->caps[0], th->bg, ts(2, 4));
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
