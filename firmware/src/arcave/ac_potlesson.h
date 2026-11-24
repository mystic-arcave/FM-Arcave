/* SPDX-License-Identifier: GPL-3.0-only */
/* POTION CHOIR's lesson (Ben, 2026-11-24: an animated opening that ends with how it is played). On the story's card,
 * after the opening (po_intro, ac_duoplace.h), for two; for three the card's own words, as before. The way the
 * machines' lessons go (ac_mtutor.h, ac_tutor.h): the scene at the top, a line under it, the drawn FM-1 with the hands
 * doing it; each step WATCH, then YOUR TURN (the real knob and keys, the scene answering), then a tick; a step that
 * only tells ticks itself off, and a key goes on. PLAY skips a step, REC leaves the card as ever. Each player goes
 * through it on their own FM-1, at their own pace; then ALL DONE, and the card's own page: ready, and the game counts in.
 *   1 ONE OF YOU SEES WHO NEEDS IT     your screen and your friend's, half size: on yours one is marked; on theirs, ?
 *   2 SAY WHAT IT LOOKS LIKE            you, in a bubble its shape, its eyes, its colour, what is on its head, its mouth
 *   3 TURN A KNOB TO MOVE THE POTION    your turn to pour: a knob moves the potion along your rocks (twice)
 *   4 PRESS ANY KEY TO POUR             over the one that needs it: it sings (a wrong one: X, and back over it)
 *   5 THEN YOU SWAP                     the mark goes to your friend's screen, the potion comes to yours
 *   6 MORE ROCKS EACH ROUND             six, eight, then twelve
 * The scenes are the game's own screen (du_potion) with a choir of the lesson's own (PL_SEED); what of du they change
 * is put back after each picture. */

#define PL_N 6
static const uint16_t PL_MS[PL_N] = {4600, 5200, 3600, 3200, 5000, 6000};
#define PL_INFO (1u << 0 | 1u << 1 | 1u << 4 | 1u << 5)
#define PL_SY 36                                 /* the game's screen from this row, in the half-size pictures */
#define PL_END_MS 1800                           /* ALL DONE, this long, then the card */
static col_t pl_shot[119 * TU_SCENE_H] AC_POOL;  /* your friend's screen, half size, while yours is drawn */
static int pl_me, pl_fr, pl_need;                /* you, your friend (seats), the rock that needs it (third along yours) */
static int32_t pl_endt;
#define PV_POS tp.v[0]                           /* your turn: the potion over rock PV_POS of your order */
#define PV_POUR tp.v[1]                          /* ms since it was poured (-1: not yet), and whether rightly */
#define PV_OK tp.v[2]

static int pl_r(int giver) { return du_giver(0) == giver ? 0 : 1; }   /* the first pour that is giver's */
/* the game's screen as seat sees it in pour r: the potion over rock pos of the pourer's order, poured ms ago (-1 not) */
static void pl_view(int seat, int r, int pos, int32_t poured, int32_t now)
{
    int g = du_giver(r);
    du_pot_round(PL_SEED, 0);
    pl_need = du.pperm[pl_me][2];
    du.seat = (uint8_t)seat; du.pr = (uint8_t)r; du.pstart = 0; du.pend = 0; du.n = 0;
    du_n[r].lane = (uint8_t)pl_need; du_n[r].st[0] = 0; du_n[r].pair = 0; du_n[r].pitch = -1;
    du.dial[g] = du.pperm[g][((pos % du.pn) + du.pn) % du.pn];
    if (poured >= 0) {
        int c = du.dial[g], ok = c == pl_need;
        du.pend = now + 400; du_n[r].st[0] = (uint8_t)(ok ? 1 : 3); du_n[r].pitch = (int8_t)c; du_n[r].pair = 1;
        if (ok) { du_ch[c].awake = 1; du.csing = (uint8_t)c; du.csing_t = now - poured / 6; }
    }
    du_bg(fm1_ms);
    du_potion(now);
}
/* the screen as drawn, rows PL_SY.., half size (the brightest of each two by two) to d, a row every stride. In place
   it is safe: every row read is below the one written */
static void pl_half(col_t *d, int32_t stride)
{
    int32_t x, y, a, b;
    for (y = 0; y < TU_SCENE_H; y++)
        for (x = 0; x < 119; x++) {
            col_t best = 0; int32_t bl = -1;
            for (b = 0; b < 2; b++)
                for (a = 0; a < 2; a++) {
                    col_t v = fb_px[(PL_SY + y * 2 + b) * AC_W + x * 2 + a];
                    int32_t l = ((v >> 11) & 31) * 2 + ((v >> 5) & 63) + (v & 31) * 2;
                    if (l > bl) { bl = l; best = v; }
                }
            d[y * stride + x] = best;
        }
}
/* your screen and your friend's side by side, each named in a tab of its colour; hot: the side to look at (-1 none) */
static void pl_two(int r, int32_t now, int hot, uint32_t tm)
{
    int s;
    int32_t y;
    pl_view(pl_fr, r, 0, -1, now); pl_half(pl_shot, 119);
    pl_view(pl_me, r, 0, -1, now); pl_half(fb_px, AC_W);
    for (y = 0; y < TU_SCENE_H; y++) {
        int32_t x;
        for (x = 0; x < 119; x++) fb_px[y * AC_W + 121 + x] = pl_shot[y * 119 + x];
        fb_px[y * AC_W + 119] = fb_px[y * AC_W + 120] = th->bg;
    }
    for (s = 0; s < 2; s++) {
        int32_t x = s * 121, w;
        const char *nm = TR(s ? T_DU_FRIEND : T_DU_YOU);
        col_t c = DU_COL[s ? pl_fr : pl_me];
        galpha(hot == s ? 150 + ipulse(tm, 900) * 106 / 256 : 110); gbox(x, 0, 119, TU_SCENE_H, c); galpha(256);
        w = gtextw(nm, 1) + 8;
        grect(x + 60 - w / 2, 0, w, 10, c); gtextc(x + 60, 2, nm, th->bg, 1);
    }
}
/* the step's scene at u (the demo's ms; at your turn the player's), in rows 0..TU_SCENE_H; its hands and lit keys */
static int pl_scene(int32_t step, int32_t u, int live, uint32_t tm, mt_act_t *act, uint32_t *lit)
{
    int n = 0;
    int32_t ms = PL_MS[step];
    if (step == 0) {                             /* only yours marks who needs it; on your friend's, the potion and a ? */
        pl_two(pl_r(pl_fr), u / 6, u < 2300 ? 0 : 1, tm);
        if (u >= 2300) { galpha(170 + ipulse(tm, 900) * 86 / 256); gtextc(181, 30, "?", th->text, 3); galpha(256); }
        return 0;
    }
    if (step == 1) {                             /* you say it: its shape, its eyes, its colour, its head, its mouth */
        static const uint8_t ORD[5] = {DC_SHAPE, DC_EYES, DC_COL, DC_TOP, DC_MOUTH};
        int32_t k, show = 0, f = dw_ramp(u, 3600, 4400), xy[6];
        du_pot_round(PL_SEED, 0);
        pl_need = du.pperm[pl_me][2];
        du_bg(fm1_ms);
        for (k = 0; k < 5; k++) if (u >= 500 + k * 600) show |= ORD[k];
        dw_rock(24, 60, pl_me, 300, 0, u < 3500 ? dw_hop(u % 600, 0, 300) : 0, tm);
        xy[0] = 40; xy[1] = 58; xy[2] = 58; xy[3] = 50; xy[4] = 56; xy[5] = 64; gpoly(xy, 3, th->text);
        gellf(98, 38, 46, 34, th->text, 0); gellf(98, 38, 44, 32, th->bg, 0);
        du_char_m(98, 44, &du_ch[pl_need], show);
        galpha(f); gtext(150, 30, "\xE2\x86\x92", th->text, 2); galpha(256);
        dw_rock(208, 60, pl_fr, 300, 0, dw_hop(u, 4300, 300), tm);
        galpha(f); du_bottle(208, 34 + (tsin(tm, 60, 0) >> 12), DU_COL[pl_fr]); galpha(256);
        return 0;
    }
    if (step == 2 || step == 3) {                /* your screen as the one who pours, its own size: the row the potion is on */
        int32_t pos = step == 3 ? 2 : u >= 2300 ? 2 : u >= 1300 ? 1 : 0, poured = step == 3 && u >= 1400 ? u - 1400 : -1, sy, y;
        if (live) { pos = PV_POS; poured = PV_POUR; }
        pl_view(pl_me, pl_r(pl_me), (int)pos, poured, (live ? 1000 : u) / 6);
        sy = ((pos % 6) + 6) % 6 < 3 ? 34 : 102;
        for (y = 0; y < TU_SCENE_H * AC_W; y++) fb_px[y] = fb_px[sy * AC_W + y];   /* (up: each read below the write) */
        if (live) {
            if (step == 2) mt_hl = 1u << MK_K1;
            else *lit = 0xFFFFu | 0x7FFu << 16;
            return 0;
        }
        if (step == 2) {                         /* the right hand on KNOB 1, turning it twice */
            act[n].kind = MTA_PINCH; act[n].a = MK_K1; act[n].side = 1; act[n].f = (u > 1000 && u < 1350) || (u > 2000 && u < 2350);
            act[n].sl = imax(mt_in(u), mt_out(u, ms)); n++;
        } else {                                 /* a thumb on a key: poured */
            act[n].kind = MTA_THUMB; act[n].a = 9; act[n].side = 1; act[n].f = tu_press(u - 1400);
            act[n].sl = imax(mt_in(u), mt_out(u, ms)); n++;
            mt_press(9, u, 1400);
            *lit = 0xFFFFu | 0x7FFu << 16;
        }
        return n;
    }
    if (step == 4) {                             /* you swap: the potion comes to you, the mark goes to your friend */
        pl_two(u >= 2400 ? pl_r(pl_me) : pl_r(pl_fr), u / 6, -1, tm);
        if (u >= 1500 && u < 2700) {
            int32_t a = 256 - dw_ramp(u, 2400, 2700);
            galpha(a); gdisc(120, 40, 13, th->bg); gcirc(120, 40, 13, th->text);
            gtextc(120, 34, "\xE2\x86\x94", th->text, 2); galpha(256);
        }
        return 0;
    }
    {                                            /* three rounds, each of new rocks: six, then eight, then twelve */
        static const char *const NUM[3] = {"6", "8", "12"};
        int32_t ro = u < 2000 ? 0 : u < 4000 ? 1 : 2, k;
        du_pot_round(PL_SEED, (int)ro);
        du.seat = (uint8_t)pl_me; du.pr = (uint8_t)(ro * 2); du.pend = 0; du.pstart = 384; du.n = 6;
        for (k = 0; k < 6; k++) { du_n[k].st[0] = (uint8_t)(k < ro * 2 ? 1 : 0); du_n[k].lane = du.pperm[0][0]; }
        du.dial[du_giver((int)ro * 2)] = du.pperm[du_giver((int)ro * 2)][1];
        du_bg(fm1_ms);
        du_potion((u - ro * 2000) / 3);
        pl_half(&fb_px[64], AC_W);
        grect(0, 0, 64, TU_SCENE_H, th->bg); grect(183, 0, AC_W - 183, TU_SCENE_H, th->bg);
        galpha(110); gbox(63, 0, 121, TU_SCENE_H, th->rim); galpha(256);
        gtextc(32, 24, NUM[ro], th->caps[2], 4);
        for (k = 0; k < 3; k++) { if (k <= ro) gdisc(211 + (k - 1) * 12, 40, 4, th->caps[2]); else gcirc(211 + (k - 1) * 12, 40, 4, th->mute); }
        return 0;
    }
}

/* ---------------------------------------------------------------- the lesson: its turns, its sounds, its page */
static void pl_sing(void)
{
    sfx(72, 0, 260, W_SINE, 1600, 10, 180); sfx(76, 200, 260, W_SINE, 1600, 10, 180);
    sfx(79, 400, 260, W_SINE, 1600, 10, 180); sfx(84, 600, 600, W_SINE, 1600, 10, 180);
}
static void pl_start(void) { PV_POS = tp.step == 3 ? 2 : 0; PV_POUR = -1; PV_OK = 0; tp.goal = tp.step == 2 ? 2 : 1; }
static void pl_live(int32_t dt)
{
    if (PV_POUR >= 0) {
        PV_POUR += dt;
        if (!PV_OK && PV_POUR > 1100) { PV_POUR = -1; PV_POS = 2; }   /* (a wrong one: the potion back over it) */
    }
    if (tp.phase != TP_TRY) return;
    if (in.any_knob && PV_POUR < 0) {            /* a knob: along your rocks */
        PV_POS = (PV_POS + (in.any_knob > 0 ? 1 : 5)) % 6; tp.kev = 0; snd_ui(84);
        if (tp.step == 2) tp.prog++;
    }
    if (in.kdown && tp.step == 3 && PV_POUR < 0) {   /* a key: poured, over the one that needs it or not */
        int k;
        for (k = AC_NKEY - 1; k > 0 && !(in.kdown >> k & 1u); k--) ;
        tp.ev = 0; tp.evk = k;
        PV_POUR = 0; PV_OK = PV_POS == 2;
        sfx(62, 0, 300, W_NOISE, 900, 20, 40);
        if (PV_OK) { pl_sing(); tp.prog = 1; }
        else { sfx(61, 120, 260, W_SAW, 2400, 2, 20); sfx(54, 120, 260, W_SQUARE, 1300, 2, 20); }
    }
}
static void pl_begin(void) { tp_take = 0; tp_begin(); pl_endt = 0; }
/* a frame of it; 1: done (ALL DONE shown a while, or PLAY there) */
static int pl_tick(int32_t dt)
{
    int k;
    if (tp.phase == TP_END) { pl_endt += dt; return pl_endt >= PL_END_MS || BDOWN(BTN_PLAY); }
    tp_take = 0;
#ifdef DINGHY_HOST
    {   /* (tests) each step as it is ticked off, and the end */
        uint32_t was = tp.done;
        int32_t st = tp.step;
        tp_run(dt, PL_N, PL_MS[tp.step], (int)(PL_INFO >> tp.step & 1u), pl_start, pl_live);
        if (tp.done != was) printf("%6u ms  POTION CHOIR's lesson: step %d of %d done%s\n", fm1_ms, (int)st + 1, PL_N, tp.seen ? " (at your turn)" : "");
        if (tp.phase == TP_END && tp.pt == 0) printf("%6u ms  POTION CHOIR's lesson: all done\n", fm1_ms);
    }
#else
    tp_run(dt, PL_N, PL_MS[tp.step], (int)(PL_INFO >> tp.step & 1u), pl_start, pl_live);
#endif
    if (tp.phase != TP_WATCH) return 0;
    switch (tp.step) {                           /* the demos' sounds */
    case 1: for (k = 0; k < 5; k++) if (TP_AT(500 + k * 600)) sfx(76 + k * 2, 0, 120, W_TRI, 1500, 3, 60); break;
    case 2: if (TP_AT(1300) || TP_AT(2300)) snd_ui(84); break;
    case 3: if (TP_AT(1400)) { sfx(62, 0, 300, W_NOISE, 900, 20, 40); pl_sing(); } break;
    case 4: if (TP_AT(2400)) sfx(67, 0, 260, W_SINE, 900, 40, 120); break;
    case 5: if (TP_AT(2000) || TP_AT(4000)) snd_ui(79); break;
    default: break;
    }
    return 0;
}
static void pl_draw(uint32_t tm)
{
    __typeof__(du) keep = du;
    int live = tp_live(), n = 0;
    int32_t step = tp.step, u = tp.pt % PL_MS[iclamp(tp.step, 0, PL_N - 1)], k, a, h;
    uint32_t lit = 0, rowmask = 0;
    mt_act_t act[2];
    col_t c = th->caps[2];
    const char *s;
    if (tp.phase == TP_END) { tp_end_card(PL_N, c, tm); return; }
    for (k = 0; k < 27; k++) mt_age[k] = -1;
    mt_lv = live; mt_hl = 0;
    pl_me = du.seat & 1; pl_fr = !pl_me; du.np = 2;
    n = pl_scene(step, u, live, tm, act, &lit);
    du = keep;
    gclip_y1 = AC_H;
    grect(0, TU_SCENE_H, AC_W, AC_H - TU_SCENE_H, th->bg);
    if (live) {                                  /* your turn: no hands; the key the player pressed goes down */
        n = 0;
        if (tp.evk >= 0 && tp.ev < 420) mt_age[mt_bit((int)tp.evk)] = tp.ev;
    }
    mt_mini(120);
    gclip_y1 = TU_SCENE_H; tp_overlay(40, c); gclip_y1 = AC_H;
    if (!live && u < 250) { galpha(256 - u * 256 / 250); grect(0, 0, AC_W, TU_SCENE_H, th->bg); galpha(256); }
    tp_pips(PL_N, TU_SCENE_H + 1, PL_MS[step], c, tm);
    s = TR(T_PL_1 + step);                       /* the step's line, large, under the scene */
    a = ts(2, 3);
    while (a > 1 && gblock_h(232, s, a) > 3 * TLH(ts(1, 2))) a--;
    h = gblock_h(232, s, a);
    galpha(live ? 256 : imin(256, u * 256 / 250));
    gblock(4, TU_SCENE_H + 5 + imax(0, (3 * TLH(ts(1, 2)) - h) / 2), 232, s, th->text, a, 1);
    galpha(256);
    mt_device(lit, mt_keyage, -1, -1, act, n, c, tm);
    for (k = 0; k < 27; k++) {                   /* the lights on the FM-1 itself: the keys the step is about, any going down */
        int top = k >= 16, idx = top ? k - 16 : k;
        if (top && idx > 10) continue;
        if (lit >> k & 1u) rowmask |= 1u << mt_chrom(top, idx);
        if (!live && mt_age[k] >= 0 && mt_age[k] < 220) led_key_want |= 1u << mt_chrom(top, idx);
    }
    led_key_dim_want |= rowmask;
    galpha(150 + ipulse(tm, 1400) * 106 / 256);
    gfit(120, 231, 232, TR(T_TU_SKIP), th->caps[3], 1);
    galpha(256);
    mt_lv = 0; mt_hl = 0;
}
